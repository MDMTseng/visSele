// Upgrade defs to the self-contained format, in place, verifiably.
//
//   node upgrade_defs.mjs <dir|def.hydef> [more...] [--ws ws://127.0.0.1:4090]
//                         [--dry-run] [--force]
//
// A def used to store its coarse feature levels and then go to disk for the
// picture ROI refine needs. It never stored the refine points at all, so it
// located coarsely with a high score and a report that read as normal. The core
// no longer loads that format: there is one way to load a trained localiser,
// and a second route to the same object is what let the two disagree in silence.
//
// This is the migration the operator would otherwise do by opening every def in
// the SBM studio and pressing 生成特徵點. It goes through THE SAME core request
// that button sends (SF with regenerate), so an upgraded def is not merely
// equivalent to one made that way -- it is one.
//
// WHY IT IS SAFE TO RE-EXTRACT, AND WHY IT IS CHECKED ANYWAY.
//
// Extraction is deterministic given the same image, region and parameters, and
// that was measured rather than assumed: on this bench the extraction path
// reproduced a def's stored cache bit for bit. So the coarse features should
// come back identical, and the only NEW thing is the ROI windows that were
// missing. Should is not is: this compares the returned levels against the ones
// already in the def and REFUSES any def where they differ, rather than
// silently handing back a recipe that measures something else. A def that fails
// that check is not broken -- it is one whose extraction no longer reproduces,
// and that is worth knowing before it runs parts, not after.
//
// Needs a core running with a websocket (any core; it is not asked to inspect
// anything) and, beside each def, the reference image it was built from.
import fs from 'node:fs';
import path from 'node:path';
import WebSocket from 'ws';
import JSum from 'jsum';

const args = process.argv.slice(2);
const targets = [];
// TWO SWITCHES, because they are two decisions and one flag for both is how a
// def whose features no longer reproduce got rewritten anyway: --force was
// reached for to re-do already-converted defs after a format change, and it
// silently took the safety check with it.
let URL = 'ws://127.0.0.1:4090', dry = false, redo = false, acceptChanged = false;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--ws') URL = args[++i];
  else if (args[i] === '--dry-run') dry = true;
  else if (args[i] === '--redo') redo = true;            // re-convert one already converted
  else if (args[i] === '--accept-changed-features') acceptChanged = true;
  else if (args[i] === '--force') {
    console.error('--force is gone: say --redo to re-convert an already converted def, '
      + 'or --accept-changed-features to overrule the reproducibility check. '
      + 'They used to be the same flag, and that rewrote a def it should have refused.');
    process.exit(2);
  } else targets.push(args[i]);
}
if (targets.length === 0) {
  console.error('usage: node upgrade_defs.mjs <dir|def.hydef> [...] [--ws URL] [--dry-run] [--force]');
  process.exit(2);
}

const defs = [];
for (const t of targets) {
  const st = fs.statSync(t);
  // Braces are load-bearing. Without them the `else` binds to the INNER `if`,
  // so every file in the directory that is NOT a .hydef pushes the directory
  // itself onto the list -- which then fails to parse as a def. Exactly the
  // shape of the dangling `if` this whole change came from.
  if (st.isDirectory()) {
    for (const f of fs.readdirSync(t)) {
      if (f.endsWith('.hydef')) defs.push(path.join(t, f));
    }
  } else {
    defs.push(t);
  }
}
if (defs.length === 0) { console.error('no .hydef found'); process.exit(2); }

// --- BPG framing (same as make_sbm_fixture_def.mjs) -------------------------
const BPG_HDR = 9;
const enc = new TextEncoder();
function frame(type, prop, pgID, obj) {
  const body = enc.encode(obj == null ? '' : JSON.stringify(obj));
  const buf = new Uint8Array(BPG_HDR + body.length + 1);
  buf[0] = type.charCodeAt(0); buf[1] = type.charCodeAt(1); buf[2] = prop;
  new DataView(buf.buffer).setUint16(3, pgID, false);
  new DataView(buf.buffer).setUint32(5, body.length + 1, false);
  buf.set(body, BPG_HDR);
  return buf;
}

const ws = new WebSocket(URL);
ws.binaryType = 'arraybuffer';
let pg = 1;
const bail = (why, code = 1) => { console.error(why); try { ws.close(); } catch {} process.exit(code); };
ws.on('error', (e) => bail('ws: ' + e.message));

// The digest the editor will recompute on load. Must match
// InspectionEditorLogic.rootDefInfoLoading exactly.
function defSha1(featureSet) {
  const clone = JSON.parse(JSON.stringify(featureSet));
  clone.forEach((feature) => {
    Object.keys(feature)
      .filter((k) => k.startsWith('__'))
      .forEach((k) => { delete feature[k]; });
  });
  return JSum.digest(clone, 'sha1', 'hex');
}

// The SBM entry inside one featureSet, or null when this def has no shape
// localiser at all (every def in the shared folder is still sig360 today).
function sbmOf(fsEntry) {
  const ihs = fsEntry && fsEntry.inherentfeatures;
  if (!Array.isArray(ihs)) return null;
  return ihs.find((e) => e && (e.name === '@__SBM_INFO__' || e.shape_cache || e.__shape_cache)) || null;
}

let idx = 0, pending = null;
const report = { done: [], skipped: [], failed: [] };

function next() {
  if (idx >= defs.length) return finish();
  const p = defs[idx++];
  let def;
  try { def = JSON.parse(fs.readFileSync(p, 'utf8')); }
  catch (e) { report.failed.push([p, 'not JSON: ' + e.message]); return next(); }

  const fs0 = (def.featureSet || [])[0];
  const sbm = sbmOf(fs0);
  const cache = sbm && (sbm.shape_cache || sbm.__shape_cache);
  if (!fs0 || !sbm || !cache) { report.skipped.push([p, 'no shape localiser']); return next(); }
  if (cache.roi && !redo) { report.skipped.push([p, 'already self-contained']); return next(); }

  const png = p.replace(/\.hydef$/, '.png');
  if (!fs.existsSync(png)) {
    // The picture is needed ONCE, here, to cut the windows out of. After this
    // the def stops needing it -- which is the point of the exercise.
    report.failed.push([p, 'reference image missing: ' + path.basename(png)]);
    return next();
  }

  const sent = JSON.parse(JSON.stringify(def));
  sent.featureSet[0]._ref_image_path = path.resolve(png).replace(/\\/g, '/');
  pending = { p, def, cache, png };
  process.stdout.write(`  ${path.basename(p)} ... `);
  ws.send(frame('SF', 0, pg++, { definfo: sent, regenerate: true }));
}

function finish() {
  console.log('');
  for (const [p, why] of report.skipped) console.log(`  skip    ${path.basename(p)} -- ${why}`);
  for (const [p, why] of report.failed)  console.log(`  FAIL    ${path.basename(p)} -- ${why}`);
  console.log(`\n${report.done.length} upgraded, ${report.skipped.length} skipped, ${report.failed.length} failed`);
  ws.close();
  process.exit(report.failed.length ? 1 : 0);
}

setTimeout(() => bail('timed out waiting for the core'), 60000 + defs.length * 30000);

ws.on('open', () => setTimeout(next, 400));

ws.on('message', (d) => {
  if (!(d instanceof ArrayBuffer)) d = d.buffer.slice(d.byteOffset, d.byteOffset + d.byteLength);
  const b = new Uint8Array(d);
  const type = String.fromCharCode(b[0], b[1]);
  if (type === 'HR') { ws.send(frame('HR', 0, pg++, { a: ['d'] })); return; }
  if (type !== 'SF' || !pending) return;
  const text = new TextDecoder().decode(b.subarray(BPG_HDR)).replace(/\0+$/, '');
  const cur = pending; pending = null;

  let data;
  try { data = JSON.parse(text); }
  catch (e) { console.log('FAIL'); report.failed.push([cur.p, 'SF reply was not JSON']); return next(); }

  const fresh = data.shape_cache;
  if (!fresh) {
    console.log('FAIL');
    report.failed.push([cur.p, 'the core extracted nothing -- check the include region and the image']);
    return next();
  }
  if (!fresh.roi) {
    console.log('FAIL');
    report.failed.push([cur.p, 'the core produced a cache with no ROI windows (old core?)']);
    return next();
  }

  // A def whose stored hash does not match its own contents is already in a
  // state the editor refuses. Re-stamping it here would make it open again
  // while hiding whatever caused the mismatch, so leave it for a person.
  if (cur.def.featureSet_sha1 !== undefined
      && defSha1(cur.def.featureSet) !== cur.def.featureSet_sha1) {
    console.log('FAIL');
    report.failed.push([cur.p,
      'its stored featureSet_sha1 already does not match its contents -- this def '
      + 'is refused by the editor as it stands. Not re-stamping it here.']);
    return next();
  }

  // THE CHECK. Coarse features must come back exactly as the def already had
  // them; anything else means this def would start measuring differently, and
  // that is a decision for a person, not for a batch script.
  const before = JSON.stringify(cur.cache.levels);
  const after = JSON.stringify(fresh.levels);
  if (before !== after && !acceptChanged) {
    console.log('FAIL');
    report.failed.push([cur.p,
      'RE-EXTRACTION DID NOT REPRODUCE the stored coarse features -- refusing to '
      + 'rewrite it. Open this one in the SBM studio and look at it. '
      + '(--accept-changed-features overrules, deliberately verbose)']);
    return next();
  }

  const out = cur.def;
  const sbm = sbmOf(out.featureSet[0]);
  if (sbm.__shape_cache) delete sbm.__shape_cache;
  sbm.shape_cache = fresh;

  // Move roi_refine_points in beside localization_include/exclude. Placement
  // only -- the same points, read the same way, edited the same way in the
  // studio. They are an input to the next extraction, like the regions they now
  // sit with, and nothing reads them while the machine runs. Moved rather than
  // mirrored: two copies of one value is how they come to disagree.
  if (out.featureSet[0].roi_refine_points !== undefined) {
    sbm.roi_refine_points = out.featureSet[0].roi_refine_points;
    delete out.featureSet[0].roi_refine_points;
  }

  // RE-STAMP featureSet_sha1, or the def becomes unopenable.
  //
  // The editor hashes the featureSet on load and HARD BLOCKS a mismatch -- "a
  // failed integrity check means the def must NOT be trusted for inspection",
  // which is the right call and exactly what a file edited behind its own back
  // looks like. Only keys starting with `__` at the TOP level of each
  // featureSet entry are excluded; inherentfeatures is an ordinary key, so the
  // cache inside @__SBM_INFO__ is hashed and rewriting it moves the digest.
  //
  // The first version of this script did not do this. It reported eight defs
  // upgraded, every one of them verified as reproducing its coarse features
  // exactly -- and every one of them refused by the editor afterwards. The
  // check it did have was measuring the right thing and the wrong layer.
  //
  // Same rule as InspectionEditorLogic.rootDefInfoLoading, deliberately
  // duplicated rather than approximated: same library, same strip, same order.
  out.featureSet_sha1 = defSha1(out.featureSet);

  const nWin = (fresh.roi.at || []).length;
  if (dry) {
    console.log(`ok (dry run, ${nWin} windows, ${before === after ? 'levels identical' : 'LEVELS CHANGED'})`);
    report.done.push([cur.p, 'dry']);
    return next();
  }
  // A backup beside the def, once. Restoring is then a rename rather than a
  // re-extraction on a core that may no longer exist.
  const bak = cur.p + '.bak_preselfcontained';
  if (!fs.existsSync(bak)) fs.copyFileSync(cur.p, bak);
  fs.writeFileSync(cur.p, JSON.stringify(out, null, 1));
  const kb = (fs.statSync(cur.p).size / 1024).toFixed(1);
  console.log(`ok (${nWin} windows, ${kb} kB, levels identical)`);
  report.done.push([cur.p, nWin]);
  next();
});
