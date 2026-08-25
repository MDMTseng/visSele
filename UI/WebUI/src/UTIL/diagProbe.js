// A read-only observation point for long-run memory behaviour.
//
// Installed as window.__DIAG__(); a soak calls it once a minute and turns what
// it returns into CSV columns. It changes nothing and allocates almost nothing
// itself.
//
// WHY THIS EXISTS. A six-hour Electron soak showed the renderer's JS heap
// climbing 6.19 MB/min, monotonic, with no sign of levelling -- 29.8 MB to
// 670 MB in 105 minutes, which on the 4 GB target machine is fatal well inside
// a shift. The same WebUI under headless Chromium, at a HIGHER load, grew
// 0.068 MB/min and oscillated between 26 and 73 MB, so the code alone does not
// explain it.
//
// Reading the source produced three plausible culprits and no way to choose
// between them. Every one of them was an argument, and an argument is not
// evidence. This reports numbers instead.
//
// THE ARRAY CENSUS IS THE POINT. It does not look for collections anyone
// suspected; it walks the store and reports the length of EVERY array it
// finds, keyed by path. Whatever is accumulating shows up as a path whose
// number climbs, including one nobody thought of -- which is the whole reason
// the guessing failed.
'use strict';

import { stripOverlayOnly } from './dbRecord';

// Bounded so the walk cannot itself become a cost. A store with a large def
// loaded has deep object graphs, and this runs on the main thread.
const MAX_DEPTH = 7;
const MAX_KEYS = 60;
const MAX_NODES = 20000;
const REPORT_TOP = 14;

function census(root) {
  const found = [];
  const seen = new WeakSet();
  let nodes = 0;
  let truncated = false;

  const walk = (o, path, depth) => {
    if (nodes++ > MAX_NODES) { truncated = true; return; }
    if (o === null || typeof o !== 'object') return;
    // Cycles, and shared subtrees counted once. Redux state is meant to be a
    // tree but `_obj` holds class instances that point back up.
    if (seen.has(o)) return;
    seen.add(o);

    if (Array.isArray(o)) {
      if (o.length) found.push([path || '(root)', o.length]);
      // Descend into a few elements only. A 5000-element array of reports is
      // the ANSWER, not something to walk into.
      const lim = Math.min(o.length, 3);
      if (depth < MAX_DEPTH) for (let i = 0; i < lim; i++) walk(o[i], path + '[]', depth + 1);
      return;
    }
    if (depth >= MAX_DEPTH) return;

    let k = 0;
    let keys;
    try { keys = Object.keys(o); } catch { return; }
    for (const key of keys) {
      if (++k > MAX_KEYS) { truncated = true; break; }
      // Skip the DOM and React internals: they are large, cyclic, and never
      // the thing a soak is looking for.
      if (key.startsWith('__react') || key === 'stateNode' || key === '_owner') continue;
      let v;
      try { v = o[key]; } catch { continue; }   // getters can throw
      if (v && typeof v === 'object') walk(v, path ? path + '.' + key : key, depth + 1);
    }
  };

  try { walk(root, '', 0); } catch { /* a census must never break the app */ }
  found.sort((a, b) => b[1] - a[1]);
  return { top: found.slice(0, REPORT_TOP), count: found.length, nodes, truncated };
}

// A census of the DOM by element kind, for the same reason as the store census
// above: something is accumulating and naming it beats guessing at it.
//
// Memory.getDOMCounters showed nodes climbing 3470/min and listeners 478/min
// with the image stream on, and flat with it off -- real retention, held by the
// tree itself, which is why forcing a collection never returned any of it.
// That says WHAT but not WHICH. Counting by tag plus the first two class names
// and diffing two samples says which.
function domCensus() {
  const counts = Object.create(null);
  let total = 0;
  try {
    for (const el of document.querySelectorAll('*')) {
      total++;
      let k = el.tagName;
      const c = el.getAttribute && el.getAttribute('class');
      if (c) k += '.' + String(c).trim().split(/\s+/).slice(0, 2).join('.');
      counts[k] = (counts[k] || 0) + 1;
    }
  } catch { /* a census must never break the app */ }
  const top = Object.entries(counts).sort((a, b) => b[1] - a[1]).slice(0, 16);
  return { total, top };
}

// Where the commonest instance of a selector lives. Once the census names a
// growing element, this says which component is mounting it -- without which
// the answer is a tag name and a grep.
function domWhere(key) {
  try {
    for (const el of document.querySelectorAll('*')) {
      let k = el.tagName;
      const c = el.getAttribute && el.getAttribute('class');
      if (c) k += '.' + String(c).trim().split(/\s+/).slice(0, 2).join('.');
      if (k !== key) continue;
      const chain = [];
      let n = el.parentElement, hops = 0;
      while (n && hops++ < 8) {
        let t = n.tagName;
        const cc = n.getAttribute && n.getAttribute('class');
        if (cc) t += '.' + String(cc).trim().split(/\s+/).slice(0, 2).join('.');
        chain.push(t);
        n = n.parentElement;
      }
      return chain.join(' < ');
    }
  } catch { /* nothing to say */ }
  return '';
}

export function installDiagProbe(store) {
  if (typeof window === 'undefined') return;

  // rAF cadence, measured rather than assumed.
  //
  // The leading hypothesis for the difference between the two soaks is that
  // this window is occluded, so Chromium throttles rendering while the
  // WebSocket keeps delivering -- anything released on the next paint would
  // then never be released. That is testable: count actual animation frames.
  let rafTicks = 0;
  let rafSince = performance.now();
  const beat = () => { rafTicks++; requestAnimationFrame(beat); };
  requestAnimationFrame(beat);

  // MAIN-THREAD STALLS.
  //
  // Reported from the field: the screen freezes for seconds at a time while the
  // plate keeps turning and the core keeps inspecting. That shape says the
  // renderer's main thread is held by one synchronous task -- the core runs in
  // its own process and cannot be affected by it -- and nothing here recorded
  // it, so every occurrence was an anecdote.
  //
  // A timer that should fire every 250 ms cannot fire late unless the thread
  // was busy, so its lateness IS the stall, measured in the only place that can
  // see it. rAF cannot do this job: Chromium throttles animation frames when
  // the window is occluded, so a long gap there is ambiguous and a long gap
  // here is not.
  const STALL_MS = 250;
  // 1000 ms was too high to be useful: the field report is "frequent", and a
  // bench run sat at a 818 ms maximum without ever tripping it, so the log said
  // nothing while the instrument had the answer. What matters is the
  // DISTRIBUTION -- a screen that misses a few frames and a screen that freezes
  // for ten seconds are different faults, and a single threshold cannot tell
  // them apart. Buckets do.
  const STALL_WARN_MS = 400;
  const stall = { worst: 0, count: 0, last: 0, b400: 0, b1s: 0, b3s: 0, maxDropMB: 0 };
  try {
    // The heap is read on the SAME tick as the lateness, because a collection's
    // fingerprint is a sharp drop in used heap and the only way to attribute a
    // stall to one is to have both numbers from the same instant. --trace-gc
    // would say it directly, but Electron on Windows does not surface the
    // renderer's stdout, so the flag produced nothing.
    const heap = () => {
      const m = performance.memory;
      return m ? m.usedJSHeapSize / 1048576 : 0;
    };
    let prevHeap = heap();
    let due = performance.now() + STALL_MS;
    setInterval(() => {
      const now = performance.now();
      const late = now - due;
      due = now + STALL_MS;
      const h = heap();
      const drop = prevHeap - h;
      prevHeap = h;
      if (drop > stall.maxDropMB) stall.maxDropMB = drop;
      if (late <= 100) return;
      if (late > stall.worst) stall.worst = late;
      if (late > 400) stall.b400++;
      if (late > 1000) stall.b1s++;
      if (late > 3000) stall.b3s++;
      if (late > STALL_WARN_MS) {
        stall.count++;
        stall.last = Date.now();
        // Loud on purpose: this is the one event where the log timestamp is
        // worth more than the counter, because it can be lined up against the
        // core's own log to say which side stopped first.
        try {
          console.warn(`[stall] main thread blocked ${Math.round(late)} ms `
            + `heap ${h.toFixed(1)} MB drop ${drop.toFixed(1)} MB`);
        } catch {}
      }
    }, STALL_MS);
  } catch { /* a probe must never break the app */ }

  // The stall counter says WHEN and HOW LONG; longtask says WHO. Chromium
  // reports any task over 50 ms here, with an attribution naming the frame it
  // ran in -- which separates "our JS" from "an extension, the compositor, or
  // the embedder" without a profiler attached to a machine on a factory floor.
  const tasks = { worst: 0, worstName: '' };
  try {
    new PerformanceObserver((list) => {
      for (const e of list.getEntries()) {
        if (e.duration <= tasks.worst) continue;
        tasks.worst = e.duration;
        const a = (e.attribution && e.attribution[0]) || {};
        tasks.worstName = [e.name, a.containerType, a.containerName, a.containerSrc]
          .filter(Boolean).join('/').slice(0, 60);
      }
    }).observe({ entryTypes: ['longtask'] });
  } catch { /* not every runtime reports longtask */ }

  // Same for messages: if frames arrive and paints do not, the ratio says so.
  //
  // The byte count matters as much as the rate. Renderer RSS was growing
  // 10.4 MB/min at 5.23 image frames/s -- 33 KB per frame, which is the size of
  // an ENCODED JPEG, not of a decoded bitmap (816x528x4 = 1.72 MB, fifty times
  // larger). Recording the actual payload size turns that arithmetic from a
  // suggestive coincidence into a measurement.
  let wsMsgs = 0;
  let wsBytes = 0;
  let lastBytes = 0;
  let lastImgW = 0, lastImgH = 0, lastScale = 0;
  window.__DIAG_WS_TICK__ = (n, w, h, scale) => {
    wsMsgs++;
    if (typeof n === 'number') { wsBytes += n; lastBytes = n; }
    if (typeof w === 'number') lastImgW = w;
    if (typeof h === 'number') lastImgH = h;
    if (typeof scale === 'number') lastScale = scale;
  };

  // Called by the harness once it knows which key grew.
  window.__DIAG_WHERE__ = (key) => domWhere(key);

  // CHURN, not level. Memory.getDOMCounters reports a level, and the level is
  // "the document plus whatever garbage has not been collected yet" -- which is
  // why it swings three-fold between samples while the document itself never
  // moves. A level can say how much; it can never say WHO. A MutationObserver
  // sees each insertion and removal at the moment it happens, so the churn
  // names the component that has to be fixed instead of a number that has to be
  // interpreted. Subtrees are counted whole: removing one row throws away its
  // cells too, and those are the nodes that end up as garbage.
  const churn = { added: 0, removed: 0, by: Object.create(null) };
  try {
    const key = (n) => {
      if (n.nodeType === 3) return '#text';
      if (n.nodeType !== 1) return '#n' + n.nodeType;
      let k = n.tagName;
      const c = n.getAttribute && n.getAttribute('class');
      if (c) k += '.' + String(c).trim().split(/\s+/).slice(0, 2).join('.');
      return k;
    };
    const bump = (n) => {
      const k = key(n);
      churn.by[k] = (churn.by[k] || 0) + 1;
      let total = 1;
      const kids = n.childNodes;
      if (kids) for (let i = 0; i < kids.length; i++) total += bump(kids[i]);
      return total;
    };
    new MutationObserver((recs) => {
      for (const r of recs) {
        for (const n of r.addedNodes) churn.added += bump(n);
        for (const n of r.removedNodes) churn.removed += bump(n);
      }
    }).observe(document.documentElement, { childList: true, subtree: true });
  } catch { /* a probe must never break the app */ }

  // WHERE THE UNCOLLECTED NODES COME FROM.
  //
  // The observer above sees only nodes that ENTER THE TREE. A node created and
  // never inserted is invisible to it -- which is exactly the shape of what
  // getDOMCounters keeps reporting: churn reads zero, the document is frozen at
  // its slot-pool size, and the counter still swings by a thousand. So the
  // creation side has to be instrumented separately, at the factory rather than
  // at the tree.
  //
  // Counting is a bare increment on a hot path, so it must stay a bare
  // increment. The stack is what actually names the caller, and capturing one
  // costs far too much to do per call -- so exactly one is taken per distinct
  // kind per sample window, on that kind's first appearance. That is enough to
  // name a creator and cheap enough to leave on.
  const made = { total: 0, by: Object.create(null), where: Object.create(null) };
  try {
    const tally = (kind) => {
      made.total++;
      made.by[kind] = (made.by[kind] || 0) + 1;
      if (made.where[kind] === undefined) {
        const st = (new Error().stack || '').split('\n').slice(2, 5)
          .map((l) => l.trim().replace(/^at\s+/, '').slice(0, 70)).join(' < ');
        made.where[kind] = st;
      }
    };
    const wrap = (obj, name, kindOf) => {
      const orig = obj[name];
      if (typeof orig !== 'function') return;
      obj[name] = function (...a) {
        try { tally(kindOf(a)); } catch { /* never break the app */ }
        return orig.apply(this, a);
      };
    };
    wrap(Document.prototype, 'createElement', (a) => String(a[0]).toLowerCase());
    wrap(Document.prototype, 'createElementNS', (a) => String(a[1]).toLowerCase() + ':ns');
    wrap(Document.prototype, 'createTextNode', () => '#text');
    wrap(Document.prototype, 'createDocumentFragment', () => '#fragment');
    wrap(Node.prototype, 'cloneNode', function (a) { return 'clone' + (a[0] ? ':deep' : ''); });
    // innerHTML parses markup into nodes without ever calling createElement.
    const ih = Object.getOwnPropertyDescriptor(Element.prototype, 'innerHTML');
    if (ih && ih.set) {
      Object.defineProperty(Element.prototype, 'innerHTML', {
        ...ih,
        set(v) { try { tally('innerHTML'); } catch {} return ih.set.call(this, v); },
      });
    }
  } catch { /* a probe must never break the app */ }

  window.__DIAG__ = () => {
    const now = performance.now();
    const secs = Math.max(0.001, (now - rafSince) / 1000);
    const rafHz = rafTicks / secs;
    const msgHz = wsMsgs / secs;
    const imgKBps = wsBytes / 1024 / secs;
    rafTicks = 0; wsMsgs = 0; wsBytes = 0; rafSince = now;

    const m = (typeof performance !== 'undefined' && performance.memory) || {};
    let c = { top: [], count: 0, nodes: 0, truncated: false };
    try { c = census(store.getState()); } catch { /* reported as an empty census */ }

    return {
      rafHz: +rafHz.toFixed(2),
      msgHz: +msgHz.toFixed(2),
      imgKBps: +imgKBps.toFixed(1),
      lastFrameKB: +(lastBytes / 1024).toFixed(1),
      // What the core actually sent, not what the UI asked for. imgW/imgH are
      // the transmitted pixels; imgScale is the down-sample level the core
      // applied, so 2448/imgW should equal imgScale and a mismatch means the
      // request and the stream have drifted apart.
      imgW: lastImgW, imgH: lastImgH, imgScale: lastScale,
      // Times the WebUI's own grading disagreed with the verdict the core
      // acted on. Must stay 0; anything else means the screen and the sorter
      // are describing different parts. See resultGrading.
      gradeMismatch: window.__gradeMismatch || 0,
      // Do the tag's limits actually reach the verdict?
      //
      // THE FIRST VERSION OF THIS CHECK PROVED NOTHING. It compared the tag's
      // override rows against _obj.shapeList and required them to be equal --
      // but shapeList is the ROOT table, and resultGrading merges the override
      // onto it per report (see root_MarginInfo / cur_MarginInfo there). Equal
      // therefore meant the override changed nothing, and a run of zeroes was
      // read as "the 製程 limits are applied" when it said the opposite.
      //
      // What settles it is jud.lim: the limits the verdict was actually
      // computed from, stamped alongside it. For every graded report whose id
      // has an override, lim must equal the OVERRIDE, not the root. Flipped
      // parts are skipped rather than guessed at -- effectiveLimits may pick a
      // _b field there, and a check that cannot tell an override from a flip
      // is the same mistake again.
      ...(function () {
        try {
          const st = store.getState();
          const ei = st.UIData.edit_info;
          const tags = ei.inspOptionalTag || [];
          const cmi = (ei.__decorator || {}).control_margin_info || {};
          const tag = tags.find((t) => cmi[t] !== undefined);
          if (tag === undefined) return { tagActive: '', tagApplied: 0, tagDrift: 0 };
          const byId = new Map();
          for (const row of cmi[tag]) byId.set(row.id, row);
          // ROOT COMES FROM THE LOADED FILE, NOT FROM shapeList.
          //
          // On entering inspection the tag's overrides are written INTO
          // _obj.shapeList (see the write-through in InspectionUI's
          // componentDidMount), so by the time this runs shapeList already
          // holds the overridden numbers. Comparing against it made every
          // override look identical to its own root, every field was skipped as
          // "changes nothing", and tagApplied reported a confident zero on a
          // machine that was applying them correctly. loadedDefFile is the
          // recipe as it sits on disk and is the only untouched copy.
          const root = new Map();
          const froot = (((ei.loadedDefFile || {}).featureSet || [])[0] || {}).features || [];
          for (const f of froot) root.set(f.id, f);

          // edit_info.reportStatisticState, NOT UIData.reportStatisticState.
          // The wrong path yielded undefined, the loop ran zero times, and the
          // probe reported a confident tagApplied=0 for two soaks -- a check
          // that cannot fail is not a check. seen counts the reports actually
          // examined, so a zero can be told apart from a blind spot.
          const tw = ((st.UIData.edit_info || {}).reportStatisticState || {}).trackingWindow || [];
          let applied = 0, drift = 0, seen = 0, flipped = 0, nolim = 0, norow = 0;
          for (const closeRep of tw) {
            // judgeReports, NOT reports. The graded entries -- the ones
            // resultGrading stamped jud.lim onto -- live under judgeReports;
            // `reports` is undefined on a tracking entry, so the loop ran zero
            // times and the probe reported a confident zero. Third defect in
            // this one check: wrong state path, wrong field, and a counter that
            // sat behind a filter. seen is now incremented before ANY skip,
            // precisely so "saw nothing" can never again read as "found nothing".
            for (const jud of (closeRep.judgeReports || [])) {
              seen++;
              if (closeRep.isFlipped) { flipped++; continue; }
              const row = byId.get(jud.id);
              if (!row) { norow++; continue; }
              if (!jud.lim) { nolim++; continue; }
              // EVERY FIELD THE OVERRIDE DEFINES, with no "does it differ from
              // the root" filter.
              //
              // That filter was the right idea and unimplementable here: there
              // is no untouched root at runtime. Entering inspection writes the
              // overrides into _obj.shapeList, and loadedDefFile is regenerated
              // from it when the wire def is sent, so both copies already agree
              // with the override -- every field was skipped as "changes
              // nothing" and the probe reported zero on a machine applying them
              // correctly.
              //
              // Whether an override differs from the recipe's own number is a
              // property of the RECIPE. What the software has to answer is
              // narrower, and is what this counts: did the verdict use the
              // 製程's number? applied is the coverage, drift must stay 0.
              for (const k of ['value', 'USL', 'LSL', 'UCL', 'LCL']) {
                if (row[k] === undefined) continue;
                applied++;
                if (jud.lim[k] !== row[k]) drift++;
              }
            }
          }
          // Why an applied of zero is zero. Without these, 'no override took'
          // and 'every part was flipped so nothing was checked' look the same.
          return { tagActive: tag, tagApplied: applied, tagDrift: drift, tagSeen: seen,
                   tagWhy: `flip=${flipped} norow=${norow} nolim=${nolim}` };
        } catch { return { tagActive: '?', tagApplied: -1, tagDrift: -1, tagSeen: -1 }; }
      })(),
      vis: (typeof document !== 'undefined' && document.visibilityState) || '?',
      hidden: (typeof document !== 'undefined' && document.hidden) || false,
      heapMB: +(((m.usedJSHeapSize || 0) / 1048576)).toFixed(1),
      arrayCount: c.count,
      nodes: c.nodes,
      truncated: c.truncated,
      // "path=length", biggest first. Diff two samples and the growing one is
      // the leak.
      top: c.top.map(([p, n]) => p + '=' + n),
      // Per-sample, then reset: a rate, not a running total.
      churn: (() => {
        const top = Object.entries(churn.by)
          .sort((a, b) => b[1] - a[1]).slice(0, 12).map(([k, n]) => k + '=' + n);
        const o = { added: churn.added, removed: churn.removed, top };
        churn.added = 0; churn.removed = 0; churn.by = Object.create(null);
        return o;
      })(),
      // Worst main-thread stall since the last sample, and how many exceeded
      // one second. A frozen screen with a running machine lands here.
      ...(function () {
        const o = { stallMaxMs: Math.round(stall.worst), stallCount: stall.count,
                    stall400: stall.b400, stall1s: stall.b1s, stall3s: stall.b3s,
                    heapDropMB: +stall.maxDropMB.toFixed(1),
                    taskMaxMs: Math.round(tasks.worst), taskWorst: tasks.worstName };
        stall.worst = 0; stall.count = 0;
        stall.b400 = 0; stall.b1s = 0; stall.b3s = 0; stall.maxDropMB = 0;
        tasks.worst = 0; tasks.worstName = '';
        return o;
      })(),
      // WHAT THE REPORT COSTS, split by what is in it.
      //
      // The overlay fields are archived with the measurements unless they are
      // pruned, and "it feels big" is not a number. This measures the report as
      // it stands and again with the overlay-only fields removed, so the share
      // is a fact rather than an estimate -- and the same figure bounds what
      // the core spends printing them, since cJSON writes every one of those
      // doubles at full precision.
      ...(function () {
        try {
          const st = store.getState();
          const rs = (st.UIData.edit_info || {}).reportStatisticState || {};
          const rep = rs.newAddedReport;
          if (!rep || !rep.length) return { repBytes: 0, repBytesLean: 0, calHits: 0 };
          const full = JSON.stringify(rep).length;
          const lean = JSON.stringify(stripOverlayOnly(rep)).length;
          let hits = 0;
          const count = (n) => {
            if (Array.isArray(n)) { n.forEach(count); return; }
            if (!n || typeof n !== 'object') return;
            for (const k of Object.keys(n)) {
              if (k === 'cal_hits' && Array.isArray(n[k])) hits += n[k].length;
              else count(n[k]);
            }
          };
          count(rep);
          return { repBytes: full, repBytesLean: lean, calHits: hits };
        } catch { return { repBytes: -1, repBytesLean: -1, calHits: -1 }; }
      })(),
      // Nodes MANUFACTURED since the last sample, whether or not they ever
      // reached the tree. made.total minus churn.added is the part that never
      // did -- the part the counter has been showing all along.
      made: (() => {
        const top = Object.entries(made.by)
          .sort((a, b) => b[1] - a[1]).slice(0, 10)
          .map(([k, n]) => k + '=' + n + (made.where[k] ? '@' + made.where[k] : ''));
        const o = { total: made.total, top };
        made.total = 0; made.by = Object.create(null); made.where = Object.create(null);
        return o;
      })(),
      dom: (() => { const d = domCensus();
        return { total: d.total, top: d.top.map(([k, n]) => k + '=' + n) }; })(),
    };
  };
}
