// Kept inspection samples, sorted into filter groups the operator defines.
//
// WHY IT EXISTS: an operator who sees a wrong verdict go past has, today, no
// way to look at it again. The core writes snapshots to disk, but that is a
// policy set in advance and a screen away; this is the "what was that?" that
// happens three parts later, on the inspection screen, without stopping.
//
// GROUPS, NOT OK/NG/NA. Three buckets by part verdict answers "show me a bad
// one", and the question actually asked is narrower: "show me the ones where
// the outer diameter failed but the inner one passed". So a group is a
// condition per measurement -- OK / NG / NA / don't care -- and the operator
// writes as many as they need. The three verdict buckets survive as PRESETS,
// which is all they ever were: one condition on the part verdict.
//
// FIRST MATCH WINS, like a firewall rule chain. A sample lands in exactly one
// bucket, so "where did that one go" has one answer -- and the ORDER of the
// groups is therefore a real setting: narrow patterns above, broad ones below.
// A sample matching nothing is dropped; the groups are the question, and not
// matching is an answer.
//
// IT FILLS AND STOPS. Not a ring, and the difference is the point: at this
// machine's rate a 20-deep ring turns over in about a second, so the part
// someone just saw would be pushed out by the parts behind it before they could
// reach for the mouse. A full group REFUSES new samples and keeps what it has
// until a person removes something. The cost is that a full group has silently
// stopped sampling, so "full" has to be visible -- the panel says so per group
// and counts what it turned away.
//
// It is a SAMPLE, not a record. Images and reports are throttled independently
// in the core -- images stop above OK/NG/NA_MAX_FPS (6), reports never do, and
// on this machine 870 of 1470 verdicts arrived with no picture behind them. A
// part with no picture is not worth a slot, so only paired ones are kept, and
// nothing downstream should read a count off this buffer.
//
// WHAT IS KEPT: the whole report, geometry included, plus the ENCODED frame.
// historyReport learned to drop point clouds after whole reports at 1000 deep
// measured ~1.2 GB of live heap, and that lesson does NOT transfer: this fills
// and stops at a bounded number of entries where historyReport was 1000 deep
// churning at 30/s. Measured on a real record from this machine an object is
// 19.8 kB, geometry 19.4 kB of it -- and without the geometry the panel can
// show the picture and the numbers but not WHERE on the part it went wrong,
// which is the question it exists to answer.
//
// The frame is a Uint8Array rather than a Blob, for the same reason
// BPG_Protocol copies it into one: a Blob's payload lives outside the JS heap
// where V8 cannot see it, so retained frames create no collection pressure and
// the renderer's RSS climbs with the heap flat (measured 10.4 MB/min). Here
// they are retained ON PURPOSE, which makes that distinction worse, not
// academic.
import { mkLog } from 'UTIL/logger';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
const log = mkLog('ui.samplestore');

// A condition on one measurement, or on the part verdict. '*' is don't care,
// and is also what an absent condition means.
export const SAMPLE_CONDS = ['OK', 'NG', 'NA', '*'];
export const SAMPLE_CAP_DEFAULT = 20;

// Bounded regardless of what anyone types into the caps. Over it, new samples
// are refused -- the same direction as a full group, because evicting to make
// room would throw away the very thing someone is keeping.
const MAX_BYTES = 64 * 1024 * 1024;

const LS_KEY = 'insp_sample_groups';

// Starting points offered in the UI. The first three are the buckets this panel
// used to have, expressed in the model that replaced them.
export const SAMPLE_PRESETS = [
  { name: 'NG 不良', overall: 'NG', conds: {} },
  { name: 'OK 良品', overall: 'OK', conds: {} },
  { name: 'NA 無判定', overall: 'NA', conds: {} },
  { name: '自訂(逐項條件)', overall: '*', conds: {} },
];

let groups = [];                 // [{id, name, cap, overall, conds}]
let store = {};                  // groupId -> [entry]
let skipped = {};                // groupId -> count refused while full
let seq = 0;
let gseq = 0;
let subs = new Set();

// Reports finalised but not yet given a frame. The core sends RP then IM inside
// one group, so the image that arrives next belongs to the reports that arrived
// last -- InspectionUI.updateCanvas relies on the same fact to keep an overlay
// on top of its own frame.
let pending = null;

const notify = () => { subs.forEach((f) => { try { f(); } catch (e) { /* a listener must not break the store */ } }); };
export function subscribeSampleStore(f) { subs.add(f); return () => subs.delete(f); }

const normCond = (c) => (SAMPLE_CONDS.indexOf(c) >= 0 ? c : '*');

export function normGroup(g) {
  const conds = {};
  const src = (g && g.conds) || {};
  Object.keys(src).forEach((k) => {
    const c = normCond(src[k]);
    if (c !== '*') conds[String(k)] = c;      // don't-care is stored as absence
  });
  return {
    id: (g && g.id) || ('g' + (++gseq) + '_' + Date.now().toString(36)),
    name: (g && typeof g.name === 'string' && g.name) ? g.name : '未命名',
    cap: Math.max(1, Math.min(500, Math.round(Number(g && g.cap) || SAMPLE_CAP_DEFAULT))),
    overall: normCond(g && g.overall),
    conds: conds,
  };
}

// ---- persistence -----------------------------------------------------------
//
// localStorage, not machine_custom_setting: these are one person's questions
// about one shift, not machine configuration, and they must not travel to the
// core or to other machines. Every access is guarded -- a browser with site
// data blocked throws on read, and losing the group list is not a reason to
// stop inspecting.
function load() {
  try {
    if (typeof window === 'undefined' || !window.localStorage) return [];
    const raw = window.localStorage.getItem(LS_KEY);
    if (!raw) return [];
    const arr = JSON.parse(raw);
    if (!Array.isArray(arr)) return [];
    return arr.map(normGroup);
  } catch (e) { log.warn('[samples] could not read the saved groups', e); return []; }
}

function save() {
  try {
    if (typeof window === 'undefined' || !window.localStorage) return;
    window.localStorage.setItem(LS_KEY, JSON.stringify(groups));
  } catch (e) { log.warn('[samples] could not save the groups', e); }
}

// Read at module load, so sampling works whether or not the panel is ever
// opened. A group list that only existed once someone looked at it would mean
// the samples you want are the ones you did not collect.
groups = load();
groups.forEach((g) => { store[g.id] = []; skipped[g.id] = 0; });

export function sampleGroups() { return groups.map((g) => ({ ...g, conds: { ...g.conds } })); }

// Replace the configuration. Buckets for groups that survive keep their
// samples: renaming a group, or reordering the chain, must not delete the
// evidence already in it.
export function setSampleGroups(next) {
  const norm = (Array.isArray(next) ? next : []).map(normGroup);
  const keep = {}, nskip = {};
  norm.forEach((g) => {
    keep[g.id] = store[g.id] || [];
    // Over its new cap (someone lowered it) a group stops collecting -- it does
    // NOT trim. Trimming here would delete kept evidence because a number moved.
    nskip[g.id] = skipped[g.id] || 0;
  });
  groups = norm;
  store = keep;
  skipped = nskip;
  save();
  notify();
}

export function clearSampleStore() {
  Object.keys(store).forEach((k) => { store[k] = []; skipped[k] = 0; });
  pending = null;
  notify();
}

export function clearSampleGroup(groupId) {
  if (!store[groupId]) return;
  store[groupId] = [];
  skipped[groupId] = 0;
  notify();
}

// Remove one, by the id the panel shows. Returns whether anything went, so a
// caller can say "already gone" rather than report a success that did not happen.
export function removeSampleEntry(id) {
  for (const k of Object.keys(store)) {
    const i = store[k].findIndex((e) => e.id === id);
    if (i >= 0) {
      store[k].splice(i, 1);
      // A freed slot means this group is collecting again, so the count of what
      // it turned away while full stops describing the present.
      skipped[k] = 0;
      notify();
      return true;
    }
  }
  return false;
}

// DISTINCT frames, not entries.
//
// A frame holding four parts produces four entries pointing at the SAME
// Uint8Array (deliberate -- see attachImage), so summing per entry reports four
// times the memory actually held. The report's bytes ARE per entry: each part
// has its own geometry, and at ~19 kB an object that is no longer a rounding
// error beside the frame. This number is what the ceiling admits against.
export function sampleStoreBytes() {
  const seen = new Set();
  let n = 0;
  Object.keys(store).forEach((k) => store[k].forEach((e) => {
    n += e.bytesReport || 0;
    const buf = e.img && e.img.jpegBytes;
    if (!buf || seen.has(buf)) return;
    seen.add(buf);
    n += buf.byteLength || 0;
  }));
  return n;
}

// Oldest first, which is capture order -- and with fill-and-stop that is the
// order they stay in until someone removes one, so the index beside an entry is
// stable enough to say out loud ("delete 3").
export function sampleStoreSnapshot() {
  return {
    bytes: sampleStoreBytes(),
    groups: groups.map((g) => ({
      ...g, conds: { ...g.conds },
      items: (store[g.id] || []).slice(),
      full: (store[g.id] || []).length >= g.cap,
      skipped: skipped[g.id] || 0,
    })),
  };
}

export function sampleStoreEntry(id) {
  for (const k of Object.keys(store)) {
    const hit = store[k].find((e) => e.id === id);
    if (hit) return hit;
  }
  return undefined;
}

// jud.status is a NUMBER (INSPECTION_STATUS: NA -128, UNSET -100, SUCCESS 0,
// FAILURE -1), not a string. Comparing it against "NA"/"FAILURE" is always
// false, which would file every part ever inspected under NA -- the verdict
// meaning "we could not tell", on a screen built to answer exactly that.
function condOf(status) {
  if (status === INSPECTION_STATUS.FAILURE) return 'NG';
  if (status === INSPECTION_STATUS.SUCCESS) return 'OK';
  return 'NA';         // NA, UNSET, and undefined: nobody reached a verdict
}

// THE PART VERDICT, which is what the screen showed.
//
// Each judge row's status has already been settled by resultGrading (it
// overwrites the core's with the UI's and counts disagreements in
// window.__gradeMismatch). Any NA wins over any NG, because an NA is "not
// judged" and grading a part that was never judged as a failure invents a
// verdict nobody reached.
export function overallVerdict(judgeReports) {
  if (!Array.isArray(judgeReports) || judgeReports.length === 0) return 'NA';
  let na = false, ng = false;
  for (const j of judgeReports) {
    const c = condOf(j && j.status);
    if (c === 'NA') na = true;
    else if (c === 'NG') ng = true;
  }
  if (na) return 'NA';
  return ng ? 'NG' : 'OK';
}

// Does this part satisfy one group? Every stated condition must hold; anything
// unstated is don't care.
//
// A measurement with NO ROW in the report counts as NA rather than as a
// mismatch: a measure that did not run is precisely "no verdict was reached",
// and that is a case worth being able to ask for.
export function matchesGroup(judgeReports, g) {
  if (!g) return false;
  if (g.overall !== '*' && overallVerdict(judgeReports) !== g.overall) return false;
  const ids = Object.keys(g.conds || {});
  for (const id of ids) {
    const row = (judgeReports || []).find((j) => j && String(j.id) === id);
    if (condOf(row && row.status) !== g.conds[id]) return false;
  }
  return true;
}

// FI ONLY, and it refuses rather than guesses.
//
// In CI a part's verdict is settled when its object times out of the tracking
// window -- keepInTrackingTime_ms later, one second on this bench. By then the
// frame on screen is a later one, quite possibly of a different part, so
// pairing "the verdict that just landed" with "the picture that just arrived"
// would attach the wrong photograph. For a tool whose only job is explaining a
// verdict, a confidently wrong picture is worse than none.
export function noteFinalisedReports(reports, inspMode) {
  if (inspMode !== 'FI') { pending = null; return; }
  if (!Array.isArray(reports) || reports.length === 0) { pending = null; return; }
  pending = [];
  reports.forEach((r) => {
    // A DETACHED copy, through JSON: the live report is still referenced by the
    // tracking window and the DB upload, and a stored sample that changes when
    // they mutate it is history rewriting itself after someone has read it. The
    // round trip also drops functions and live object references, so an entry
    // retains its own bytes and nothing else.
    let copy;
    try { copy = JSON.parse(JSON.stringify(r)); }
    catch (e) { log.warn('[samples] a report would not serialise -- skipped', e); return; }
    // The playback canvas draws trackingWindow.filter(x => x.isCurObj), and by
    // the time a report is FINALISED that flag is false -- it means "matched in
    // the frame being processed", and finalising happens once the object has
    // left. Stored, it means "the object this sample is of", which is what the
    // panel shows. Without the stamp the overlay draws NOTHING and looks
    // exactly like the geometry having been dropped.
    copy.isCurObj = true;
    pending.push(copy);
  });
  if (pending.length === 0) pending = null;
}

// Pair the frame that just arrived with the reports that arrived just before
// it, and sort each part into the FIRST group that accepts it.
export function attachImage(img, camParam, defName) {
  if (pending === null) return;
  const reports = pending;
  pending = null;
  if (groups.length === 0) return;                                   // nothing asked for
  if (!img || !img.jpegBytes || !img.jpegBytes.byteLength) return;   // sample: paired only

  const overBytes = sampleStoreBytes() >= MAX_BYTES;

  // ONE frame object shared by every part in it. Copying the bytes per part
  // would multiply the cost of exactly the frames most worth keeping.
  const frame = {
    jpegBytes: img.jpegBytes,
    format: img.format, width: img.width, height: img.height,
    scale: img.scale, full_width: img.full_width, full_height: img.full_height,
    offsetX: img.offsetX, offsetY: img.offsetY,
  };

  let touched = false;
  reports.forEach((r) => {
    const jr = r.judgeReports || [];
    const g = groups.find((cand) => matchesGroup(jr, cand));
    if (!g) return;                        // matched nothing: dropped, by design
    touched = true;
    const bucket = store[g.id] || (store[g.id] = []);
    // FULL MEANS FULL. The new sample is the one that goes, not the oldest --
    // the oldest is what someone is keeping.
    if (bucket.length >= g.cap || overBytes) {
      if ((skipped[g.id] || 0) === 0) {
        log.info('[samples] group "' + g.name + '" is full at ' + g.cap
          + (overBytes ? ' (and the buffer is at its memory ceiling)' : '')
          + ' -- no longer collecting until something is removed');
      }
      skipped[g.id] = (skipped[g.id] || 0) + 1;
      return;
    }
    let bytesReport = 0;
    try { bytesReport = JSON.stringify(r).length; } catch (e) { /* counted as 0 */ }
    bucket.push({
      id: ++seq, groupId: g.id, groupName: g.name, at: Date.now(),
      verdict: overallVerdict(jr), img: frame,
      camParam: camParam, defName: defName,
      // The whole report, for the overlay: RepDisplay puts it into
      // trackingWindow and the canvas draws its lines, circles and search
      // points. This is why the geometry is kept at all.
      report: r,
      bytesReport: bytesReport,
      // Lifted for the panel's table and caption. Same objects, not copies.
      judgeReports: jr,
      time_ms: r.time_ms, cx: r.cx, cy: r.cy,
      rotate: r.rotate, isFlipped: r.isFlipped,
    });
  });
  // Notify on a refusal too: a group's "full, N turned away" counter is the
  // only thing that distinguishes a stopped buffer from a stopped machine.
  if (touched) notify();
}
