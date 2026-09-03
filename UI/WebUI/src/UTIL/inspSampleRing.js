// The last N inspected parts, with their pictures, kept per verdict.
//
// WHY IT EXISTS: an operator who sees a wrong verdict go past has, today, no
// way to look at it again. The core writes snapshots to disk, but that is a
// policy set in advance and a screen away; this is the "what was that?" that
// happens three parts later, on the inspection screen, without stopping.
//
// It is a SAMPLE, not a record. Reports and images are throttled independently
// in the core -- images stop above OK/NG/NA_MAX_FPS (6), reports never do, and
// on this machine 870 of 1470 verdicts arrived with no picture behind them. A
// part with no picture is not worth a slot here, so only paired ones are kept,
// and nothing downstream should read a count off this buffer.
//
// WHAT IS KEPT, and why so little: this repo has already paid for the other
// answer. historyReport used to hold whole reports and was measured at 22.6 kB
// per object, 97.6% of it point clouds, which came to ~1.2 GB of live heap at
// 1000 deep. So an entry is the verdict, the judge rows, and the ENCODED frame
// -- never a decoded bitmap, and never the geometry.
//
// The frame is a Uint8Array rather than a Blob, deliberately and for the same
// reason BPG_Protocol copies it into one: a Blob's payload lives outside the JS
// heap where V8 cannot see it, so retained frames create no collection pressure
// and the renderer's RSS climbs with the heap flat (measured 10.4 MB/min).
// Here they are retained ON PURPOSE, which makes that distinction worse, not
// academic.
import { mkLog } from 'UTIL/logger';
import { INSPECTION_STATUS } from 'UTIL/InspectionStatus';
const log = mkLog('ui.samplering');

export const SAMPLE_RING_BUCKETS = ['OK', 'NG', 'NA'];
export const SAMPLE_RING_CAP_DEFAULT = 20;

// A ceiling that does not depend on anyone choosing a sensible N. Three buckets
// of 20 encoded frames is a few MB; three buckets of 500 would not be, and the
// setting is reachable from a UI. Whichever limit bites first wins.
const MAX_BYTES = 64 * 1024 * 1024;

let cap = SAMPLE_RING_CAP_DEFAULT;
let store = { OK: [], NG: [], NA: [] };
let seq = 0;
let subs = new Set();

// Reports that have been finalised but have not yet been given a frame.
//
// The core sends RP and then IM inside one group, so the image that arrives
// next belongs to the reports that arrived last. That ordering is the whole
// pairing rule -- see InspectionUI.updateCanvas, which relies on the same fact
// to keep an overlay on top of its own frame.
let pending = null;

const notify = () => { subs.forEach((f) => { try { f(); } catch (e) { /* a listener must not break the ring */ } }); };
export function subscribeSampleRing(f) { subs.add(f); return () => subs.delete(f); }

export function sampleRingCap() { return cap; }
export function setSampleRingCap(n) {
  const v = Math.max(1, Math.min(500, Math.round(Number(n) || SAMPLE_RING_CAP_DEFAULT)));
  if (v === cap) return;
  cap = v;
  SAMPLE_RING_BUCKETS.forEach((b) => { if (store[b].length > cap) store[b] = store[b].slice(-cap); });
  notify();
}

export function clearSampleRing() {
  store = { OK: [], NG: [], NA: [] };
  pending = null;
  notify();
}

// DISTINCT frames, not entries.
//
// A frame holding four parts produces four entries that all point at the SAME
// Uint8Array -- that sharing is deliberate (see attachImage). Summing per entry
// therefore reports four times the memory that is actually held, which is not
// merely a cosmetic error in the MB readout: this number is what the ceiling
// below evicts against, so an over-count throws away samples to stay under a
// limit nothing was near.
export function sampleRingBytes() {
  const seen = new Set();
  let n = 0;
  SAMPLE_RING_BUCKETS.forEach((b) => store[b].forEach((e) => {
    const buf = e.img && e.img.jpegBytes;
    if (!buf || seen.has(buf)) return;
    seen.add(buf);
    n += buf.byteLength || 0;
  }));
  return n;
}

// Newest first, which is the order someone looking for "the one that just went
// past" reads in.
export function sampleRingSnapshot() {
  const out = { bytes: sampleRingBytes(), cap: cap };
  SAMPLE_RING_BUCKETS.forEach((b) => { out[b] = store[b].slice().reverse(); });
  return out;
}

export function sampleRingEntry(id) {
  for (const b of SAMPLE_RING_BUCKETS) {
    const hit = store[b].find((e) => e.id === id);
    if (hit) return hit;
  }
  return undefined;
}

// THE VERDICT THIS BUCKETS BY is the one on the screen.
//
// Each judge row carries a status that resultGrading has already settled (it
// overwrites the core's with the UI's and counts the disagreements in
// window.__gradeMismatch). Bucketing by anything else would put a part in a
// bucket that contradicts the colour the operator saw, which is the opposite of
// helpful when the question is "why did that one look wrong".
//
// Any NA wins over any NG, because an NA is "not judged" and grading a part
// that was never judged as a failure invents a verdict nobody reached.
// jud.status is a NUMBER (INSPECTION_STATUS: NA -128, UNSET -100, SUCCESS 0,
// FAILURE -1), not a string. Comparing it against "NA"/"FAILURE" is always
// false, which would have filed every part ever inspected under NA -- the
// bucket that means "we could not tell", on a screen built to answer exactly
// that question.
function verdictOf(judgeReports) {
  if (!Array.isArray(judgeReports) || judgeReports.length === 0) return 'NA';
  let na = false, ng = false;
  for (const j of judgeReports) {
    const s = j && j.status;
    // UNSET counts as NA for the same reason NA does: nobody reached a verdict.
    if (s === undefined || s === INSPECTION_STATUS.NA || s === INSPECTION_STATUS.UNSET) na = true;
    else if (s === INSPECTION_STATUS.FAILURE) ng = true;
  }
  if (na) return 'NA';
  return ng ? 'NG' : 'OK';
}

// FI ONLY, and it refuses rather than guesses.
//
// In CI a part's verdict is settled when its object times out of the tracking
// window -- keepInTrackingTime_ms later, one second on this bench. By then the
// frame on screen is a later one, quite possibly of a different part, so
// pairing "the verdict that just landed" with "the picture that just arrived"
// would attach the wrong photograph. For a tool whose only job is explaining a
// verdict, a confidently wrong picture is worse than none, so CI is not
// sampled at all until a frame can be carried on the tracking entry itself.
export function noteFinalisedReports(reports, inspMode) {
  if (inspMode !== 'FI') { pending = null; return; }
  if (!Array.isArray(reports) || reports.length === 0) { pending = null; return; }
  pending = reports.map((r) => ({
    time_ms: r && r.time_ms,
    // The judge rows only -- see the note at the top about what the geometry
    // costs. dclone is not used: these are plain rows and the reducer is done
    // with them, but they are copied so a later mutation of the live report
    // cannot rewrite history that has already been shown to someone.
    judgeReports: JSON.parse(JSON.stringify((r && r.judgeReports) || [])),
    cx: r && r.cx, cy: r && r.cy, rotate: r && r.rotate, isFlipped: r && r.isFlipped,
  }));
}

// Pair the frame that just arrived with the reports that arrived just before
// it. Called from the Image_Update path.
export function attachImage(img, camParam, defName) {
  if (pending === null) return;
  const reports = pending;
  pending = null;
  if (!img || !img.jpegBytes || !img.jpegBytes.byteLength) return;   // sample: paired only

  // ONE frame object shared by every part in it. A frame with four parts in it
  // produces four entries, and copying the bytes four times would quadruple the
  // cost of exactly the frames that are most worth keeping.
  const frame = {
    jpegBytes: img.jpegBytes,
    format: img.format, width: img.width, height: img.height,
    scale: img.scale, full_width: img.full_width, full_height: img.full_height,
    offsetX: img.offsetX, offsetY: img.offsetY,
  };

  reports.forEach((r) => {
    const v = verdictOf(r.judgeReports);
    const e = { id: ++seq, verdict: v, at: Date.now(), img: frame,
                camParam: camParam, defName: defName, ...r };
    store[v].push(e);
    if (store[v].length > cap) store[v] = store[v].slice(-cap);
  });

  // The ceiling, enforced oldest-first across all three buckets. Dropping is
  // announced: a sample buffer that silently shrinks looks identical to a
  // machine that stopped producing parts.
  let guard = 0;
  while (sampleRingBytes() > MAX_BYTES && guard++ < 10000) {
    let oldest = null, ob = null;
    SAMPLE_RING_BUCKETS.forEach((b) => {
      const f = store[b][0];
      if (f && (oldest === null || f.id < oldest.id)) { oldest = f; ob = b; }
    });
    if (!oldest) break;
    store[ob].shift();
    log.warn('[samplering] over ' + Math.round(MAX_BYTES / 1048576)
      + ' MB -- dropped the oldest ' + ob + ' sample');
  }
  notify();
}
