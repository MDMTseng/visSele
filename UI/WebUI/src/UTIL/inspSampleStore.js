// The first N inspected parts since the last clear, per verdict, with pictures.
//
// WHY IT EXISTS: an operator who sees a wrong verdict go past has, today, no
// way to look at it again. The core writes snapshots to disk, but that is a
// policy set in advance and a screen away; this is the "what was that?" that
// happens three parts later, on the inspection screen, without stopping.
//
// IT FILLS AND STOPS. It is not a ring, and the difference is the whole point:
// at this machine's rate a 20-deep ring turns over in about a second, so the
// part someone just saw would be pushed out by the parts behind it before they
// could reach for the mouse. A full bucket therefore REFUSES new samples and
// keeps what it has until a person removes something. The cost is that a full
// bucket is silently no longer sampling, so "full" has to be visible -- see the
// panel, which says so per bucket rather than just showing 20 of 20.
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
const log = mkLog('ui.samplestore');

export const SAMPLE_BUCKETS = ['OK', 'NG', 'NA'];
export const SAMPLE_CAP_DEFAULT = 20;

// A ceiling that does not depend on anyone choosing a sensible N. Three buckets
// of 20 encoded frames is a few MB; three buckets of 500 would not be, and the
// setting is reachable from a UI. Over it, new samples are refused -- the same
// direction as a full bucket, because evicting to make room would throw away
// the very thing someone is keeping.
const MAX_BYTES = 64 * 1024 * 1024;

let cap = SAMPLE_CAP_DEFAULT;
let store = { OK: [], NG: [], NA: [] };
let skipped = { OK: 0, NG: 0, NA: 0 };   // refused because the bucket was full
let seq = 0;
let subs = new Set();

// Reports that have been finalised but have not yet been given a frame.
//
// The core sends RP and then IM inside one group, so the image that arrives
// next belongs to the reports that arrived last. That ordering is the whole
// pairing rule -- see InspectionUI.updateCanvas, which relies on the same fact
// to keep an overlay on top of its own frame.
let pending = null;

const notify = () => { subs.forEach((f) => { try { f(); } catch (e) { /* a listener must not break the store */ } }); };
export function subscribeSampleStore(f) { subs.add(f); return () => subs.delete(f); }

export function sampleStoreCap() { return cap; }

// Lowering the cap does NOT drop anything already held.
//
// Every other buffer in this UI trims on a lowered limit, and here that would
// delete evidence someone deliberately kept because a number moved. A bucket
// over its cap simply refuses new samples until it is back under, which is the
// same rule as being full.
export function setSampleStoreCap(n) {
  const v = Math.max(1, Math.min(500, Math.round(Number(n) || SAMPLE_CAP_DEFAULT)));
  if (v === cap) return;
  cap = v;
  notify();
}

export function clearSampleStore() {
  store = { OK: [], NG: [], NA: [] };
  skipped = { OK: 0, NG: 0, NA: 0 };
  pending = null;
  notify();
}

export function clearSampleBucket(verdict) {
  if (!SAMPLE_BUCKETS.includes(verdict)) return;
  store[verdict] = [];
  skipped[verdict] = 0;
  notify();
}

// Remove one, by the id the panel shows. Returns whether anything went --
// callers use it to say "already gone" rather than reporting a success that
// did not happen.
export function removeSampleEntry(id) {
  for (const b of SAMPLE_BUCKETS) {
    const i = store[b].findIndex((e) => e.id === id);
    if (i >= 0) {
      store[b].splice(i, 1);
      // Freeing a slot means this bucket is collecting again, so the count of
      // what it turned away while full stops describing the present.
      skipped[b] = 0;
      notify();
      return true;
    }
  }
  return false;
}

// DISTINCT frames, not entries.
//
// A frame holding four parts produces four entries that all point at the SAME
// Uint8Array -- that sharing is deliberate (see attachImage). Summing per entry
// therefore reports four times the memory that is actually held, which is not
// merely a cosmetic error in the MB readout: this number is what the ceiling
// admits against, so an over-count would refuse samples while nothing was near
// the limit.
export function sampleStoreBytes() {
  const seen = new Set();
  let n = 0;
  SAMPLE_BUCKETS.forEach((b) => store[b].forEach((e) => {
    const buf = e.img && e.img.jpegBytes;
    if (!buf || seen.has(buf)) return;
    seen.add(buf);
    n += buf.byteLength || 0;
  }));
  return n;
}

// Oldest first, which is capture order -- and with fill-and-stop that is also
// the order they will stay in until someone removes one, so the index beside an
// entry means something stable enough to say out loud ("delete 3").
export function sampleStoreSnapshot() {
  const out = { bytes: sampleStoreBytes(), cap: cap, full: {}, skipped: {} };
  SAMPLE_BUCKETS.forEach((b) => {
    out[b] = store[b].slice();
    out.full[b] = store[b].length >= cap;
    out.skipped[b] = skipped[b];
  });
  return out;
}

export function sampleStoreEntry(id) {
  for (const b of SAMPLE_BUCKETS) {
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
// jud.status is a NUMBER (INSPECTION_STATUS: NA -128, UNSET -100, SUCCESS 0,
// FAILURE -1), not a string. Comparing it against "NA"/"FAILURE" is always
// false, which would have filed every part ever inspected under NA -- the
// bucket that means "we could not tell", on a screen built to answer exactly
// that question.
//
// Any NA wins over any NG, because an NA is "not judged" and grading a part
// that was never judged as a failure invents a verdict nobody reached.
function verdictOf(judgeReports) {
  if (!Array.isArray(judgeReports) || judgeReports.length === 0) return 'NA';
  let na = false, ng = false;
  for (const j of judgeReports) {
    const s = j && j.status;
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
    // costs. They are copied so a later mutation of the live report cannot
    // rewrite history that has already been shown to someone.
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

  const overBytes = sampleStoreBytes() >= MAX_BYTES;

  // ONE frame object shared by every part in it. A frame with four parts in it
  // produces four entries, and copying the bytes four times would quadruple the
  // cost of exactly the frames that are most worth keeping.
  const frame = {
    jpegBytes: img.jpegBytes,
    format: img.format, width: img.width, height: img.height,
    scale: img.scale, full_width: img.full_width, full_height: img.full_height,
    offsetX: img.offsetX, offsetY: img.offsetY,
  };

  let added = false;
  reports.forEach((r) => {
    const v = verdictOf(r.judgeReports);
    // FULL MEANS FULL. The new sample is the one that goes, not the oldest --
    // the oldest is what someone is keeping.
    if (store[v].length >= cap || overBytes) {
      if (skipped[v] === 0) {
        log.info('[samples] ' + v + ' bucket is full at ' + cap
          + (overBytes ? ' (and the buffer is at its memory ceiling)' : '')
          + ' -- no longer collecting until something is removed');
      }
      skipped[v]++;
      return;
    }
    store[v].push({ id: ++seq, verdict: v, at: Date.now(), img: frame,
                    camParam: camParam, defName: defName, ...r });
    added = true;
  });
  // Notify on a refusal too: the panel's "full, N turned away" counter is the
  // only thing that distinguishes a stopped buffer from a stopped machine.
  if (added || reports.length) notify();
}
