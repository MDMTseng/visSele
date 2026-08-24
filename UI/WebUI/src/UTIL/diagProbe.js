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
      // Do the tag's limits actually reach both consumers?
      //
      // The wire def is generated from _obj.shapeList after the tag overrides
      // are merged in, and the local grading overlays the same tag table -- so
      // in THEORY core and screen judge with identical numbers. This checks it
      // in practice: for the active tag, every override row must match what
      // shapeList now holds. tagApplied counts rows checked; tagDrift counts
      // rows where any of the five limits differ, and must stay 0.
      ...(function () {
        try {
          const ei = store.getState().UIData.edit_info;
          const tags = ei.inspOptionalTag || [];
          const cmi = (ei.__decorator || {}).control_margin_info || {};
          const tag = tags.find((t) => cmi[t] !== undefined);
          if (tag === undefined) return { tagActive: '', tagApplied: 0, tagDrift: 0 };
          let applied = 0, drift = 0;
          for (const row of cmi[tag]) {
            const shape = ei._obj.shapeList.find((s) => s.id === row.id);
            if (!shape) continue;
            let touched = false, differs = false;
            for (const k of ['value', 'USL', 'LSL', 'UCL', 'LCL']) {
              if (row[k] === undefined) continue;
              touched = true;
              if (shape[k] !== row[k]) differs = true;
            }
            if (!touched) continue;
            applied++;
            if (differs) drift++;
          }
          return { tagActive: tag, tagApplied: applied, tagDrift: drift };
        } catch { return { tagActive: '?', tagApplied: -1, tagDrift: -1 }; }
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
      dom: (() => { const d = domCensus();
        return { total: d.total, top: d.top.map(([k, n]) => k + '=' + n) }; })(),
    };
  };
}
