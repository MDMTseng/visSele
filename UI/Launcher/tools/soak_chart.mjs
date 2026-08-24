// Turn a soak CSV into a single self-contained page of stacked, time-aligned
// traces.
//
//   node tools/soak_chart.mjs <soak.csv> <out.html> ["title"]
//
// Re-runnable while the soak is still going: it reads whatever rows exist and
// says how far in they reach, so the same file can be regenerated and
// redeployed as the run progresses.
//
// The layout is a chart recorder on purpose. Every question this data answers
// is of the form "did THAT move at the same moment THIS did" -- did the memory
// step happen when the latency spiked, was the throughput dip the same minute
// as the fault -- and stacked traces on one shared time axis is the only
// arrangement that answers it without the reader holding two pictures in their
// head. Hence also the single crosshair that reads every series at once.
import fs from 'node:fs';
import path from 'node:path';

const [, , CSV, OUT, TITLE] = process.argv;
if (!CSV || !OUT) {
  console.error('usage: node tools/soak_chart.mjs <soak.csv> <out.html> ["title"]');
  process.exit(2);
}

// --- parsing -----------------------------------------------------------------
//
// The last column is a quoted panel string, and `err` is a bare JSON array that
// may itself contain a comma ("[12,13]"). A naive split breaks on the second;
// a quote-aware split breaks on it too, because that field is not quoted. So:
// split quote-aware, then fold any surplus fields back into the err column,
// which is the only unquoted one that can hold a comma.
function splitCsv(line) {
  const out = [];
  let cur = '', q = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '"') { q = !q; continue; }
    if (c === ',' && !q) { out.push(cur); cur = ''; continue; }
    cur += c;
  }
  out.push(cur);
  return out;
}

const raw = fs.readFileSync(CSV, 'utf8').split(/\r?\n/);
const headIdx = raw.findIndex((l) => l.startsWith('t_min,'));
if (headIdx < 0) { console.error('no header row (t_min,...) in ' + CSV); process.exit(1); }
const cols = splitCsv(raw[headIdx]);
const iErr = cols.indexOf('err');

const rows = [];
for (const line of raw.slice(headIdx + 1)) {
  if (!/^\d+(\.\d+)?,/.test(line)) continue;
  let f = splitCsv(line);
  if (f.length > cols.length) {
    const extra = f.length - cols.length;
    f = [...f.slice(0, iErr), f.slice(iErr, iErr + extra + 1).join(','), ...f.slice(iErr + extra + 1)];
  }
  const o = {};
  cols.forEach((c, i) => { o[c] = f[i] === undefined ? '' : f[i]; });
  rows.push(o);
}
if (!rows.length) { console.error('no data rows yet in ' + CSV); process.exit(1); }

const num = (v) => (v === '' || v === undefined || v === 'err' || v === '?' ? null : Number(v));
const series = (name) => rows.map((r) => num(r[name]));
const t = series('t_min');

// The head of the file is the bring-up log, and it holds the operating point --
// the plate frequency and the gate ceiling this run was pinned to. Reporting a
// soak without them invites comparing two runs that were never comparable.
const preamble = raw.slice(0, headIdx).filter((l) => l.trim()).join('\n');
const grab = (re) => { const m = preamble.match(re); return m ? m[1] : null; };
const meta = {
  freq: grab(/plate freq = (\d+)/),
  sep: grab(/min_detect_sep_us = (\d+)/),
  ceiling: grab(/\(([\d.]+)\/s ceiling\)/),
  workingDir: grab(/workingDir\s+(.+)/),
  appRoot: grab(/appRoot\s+(.+)/),
};

// --- what counts as a fault ---------------------------------------------------
//
// Deliberately broad, and each reason is kept so the ribbon can say WHICH.
// A run that stayed at state 101 the whole time but quietly grew `unanswered`
// was not a clean run, and a single "ok/not ok" column would call it one.
const faults = rows.map((r, i) => {
  const why = [];
  if (r.state !== '101') why.push('state ' + r.state);
  if (r.err && r.err !== '[]') why.push('err ' + r.err);
  if (num(r.sup_unresp)) why.push('core unresponsive');
  if (num(r.sup_missed)) why.push('missed ' + r.sup_missed + ' pings');
  if (i > 0) {
    const d = (k) => (num(r[k]) ?? 0) - (num(rows[i - 1][k]) ?? 0);
    if (d('skip') > 0) why.push('+' + d('skip') + ' skip');
    if (d('unans') > 0) why.push('+' + d('unans') + ' unanswered');
    for (const k of ['nm_orphan', 'nm_window', 'nm_consec', 'ackfalse', 'locked',
                     'unapplied', 'frame_gap', 'frame_lost', 'ts_rej', 'cal_fail', 'cal_lost']) {
      if (d(k) > 0) why.push('+' + d(k) + ' ' + k);
    }
  }
  return why;
});

const panels = [
  {
    key: 'mem', title: '記憶體', unit: 'MB',
    note: 'JS heap 與行程 RSS 分開看:heap 平而 RSS 漲,成長就不在 JS 物件上。',
    lines: [
      { name: 'Electron 總計', data: series('elRSS_MB'), color: 'var(--t1)', width: 2 },
      { name: 'renderer', data: series('rendMB'), color: 'var(--t2)' },
      { name: 'GPU 行程', data: series('gpuMB'), color: 'var(--t3)' },
      { name: '核心', data: series('coreRSS_MB'), color: 'var(--t4)' },
      { name: 'JS heap', data: series('heapMB'), color: 'var(--t5)' },
    ],
  },
  {
    key: 'dom', title: 'DOM 保留', unit: 'count',
    // The panel this run exists to read.
    //
    // Before the fix these two climbed +2725..+3600 nodes and +366..+493
    // listeners a minute and never came back down: the result cards were being
    // rendered into an antd SubMenu title, rc-menu kept a reference to every
    // child it had ever been given, and a title rebuilt each frame therefore
    // left its whole subtree reachable. A forced collection returned none of
    // it. Flat here means the retention is gone; any sustained upward slope
    // means it is not, however small the numbers look early on.
    // THREE INSTRUMENTS, AND ONLY ONE OF THEM IS ABOUT THIS UI.
    //
    // 文件內 walks the document, so it is what the page actually holds.
    // getDOMCounters does NOT collect before answering, so its 節點 is the
    // document PLUS whatever garbage has not been reclaimed yet -- which is why
    // it can swing three-fold while the document itself never moves. Reading
    // the swing as churn was the mistake this panel used to invite; a
    // MutationObserver over the whole tree records exactly zero insertions and
    // zero removals per sample in steady state.
    note: '三條線量的不是同一件事。「文件內」是這個 UI 真正掛著的節點,平的才對。'
        + '「節點」來自 getDOMCounters,回答前不做回收,所以是文件內 + 尚未回收的垃圾 —— '
        + '它的起伏是回收時機,不是 UI 的行為(churn 實測為 0)。',
    lines: [
      { name: '文件內', data: series('domNodes'), color: 'var(--t3)', width: 2 },
      { name: '節點(未回收)', data: series('dom_nodes'), color: 'var(--t1)', width: 2 },
      { name: '事件監聽器', data: series('dom_listeners'), color: 'var(--t2)' },
      { name: 'documents', data: series('dom_docs'), color: 'var(--t4)' },
    ],
  },
  {
    key: 'cpu', title: 'CPU', unit: '% of one core',
    note: '目標機只有兩顆核心 = 200%。Electron 的四個行程與核心分開量:'
        + 'app.getAppMetrics() 看不到核心,它是啟動器 spawn 出去的獨立行程。',
    lines: [
      { name: '合計', data: series('cpuTotal'), color: 'var(--t1)', width: 2 },
      { name: '核心', data: series('coreCPU'), color: 'var(--warn)' },
      { name: 'Electron', data: series('elCPU'), color: 'var(--t2)' },
    ],
  },
  {
    key: 'flow', title: '流量', unit: 'parts/s',
    note: '進料與 admit 的差是閘門擋掉的;sorted + NA 應該追平 admit,追不上就是判定沒跟上。',
    lines: [
      { name: '進料 seen', data: series('seen_s'), color: 'var(--t1)', width: 2 },
      { name: 'admit', data: series('admit_s'), color: 'var(--t3)' },
      { name: 'sorted', data: series('sorted_s'), color: 'var(--ok)' },
      { name: 'NA', data: series('na_s'), color: 'var(--warn)' },
    ],
  },
  {
    key: 'lat', title: '回報延遲', unit: 'ms',
    note: 'cam_avg / cam_max,從相機曝光算起。平均不是風險,尾巴才是 —— lat_tail 是這一分鐘內落進尾端桶的件數。',
    lines: [
      { name: '平均', data: series('lat_avg_ms'), color: 'var(--t1)', width: 2 },
      { name: '本分鐘最大', data: series('lat_max_ms'), color: 'var(--warn)' },
    ],
    bars: { name: '尾端件數', data: series('lat_tail_n'), color: 'var(--bad)' },
  },
  {
    key: 'pair', title: '時戳配對', unit: 'µs',
    note: '殘差往窗緣漂就會誤配 —— 一件料被另一件的影像判定,而那看起來完全不像錯誤。window 5000 µs。',
    lines: [
      { name: '殘差', data: series('resid_us'), color: 'var(--t1)', width: 2 },
      { name: '本分鐘最大殘差', data: series('resid_max_us'), color: 'var(--t3)' },
      { name: '最大配對間隔', data: series('dmax_us'), color: 'var(--warn)' },
    ],
  },
];

const last = rows[rows.length - 1];
const first = rows[0];

// Slope of a column against t, in units per minute. The first sample is taken
// during start-up -- the UI is still settling and the counters have not reached
// their working level -- so it is excluded rather than allowed to tilt the fit.
function slopePerMin(col) {
  const pts = [];
  for (let i = 0; i < rows.length; i++) {
    const v = num(rows[i][col]);
    if (v == null || t[i] == null || t[i] < 0.5) continue;
    pts.push([t[i], v]);
  }
  if (pts.length < 5) return null;
  const n = pts.length;
  const sx = pts.reduce((a, p) => a + p[0], 0);
  const sy = pts.reduce((a, p) => a + p[1], 0);
  const sxx = pts.reduce((a, p) => a + p[0] * p[0], 0);
  const sxy = pts.reduce((a, p) => a + p[0] * p[1], 0);
  const d = n * sxx - sx * sx;
  return d === 0 ? null : (n * sxy - sx * sy) / d;
}
const summary = [
  { label: '狀態', value: last.state === '101' ? '檢測中' : last.state,
    tone: last.state === '101' && last.err === '[]' ? 'ok' : 'bad' },
  { label: '進料', value: (last.seen_s || '—') + ' /s' },
  { label: '延遲 平均', value: (last.lat_avg_ms || '—') + ' ms' },
  { label: '配對殘差', value: (last.resid_us || '—') + ' µs' },
  { label: 'Electron RSS', value: (last.elRSS_MB || '—') + ' MB',
    sub: (() => { const a = num(first.elRSS_MB), b = num(last.elRSS_MB);
      return a != null && b != null ? (b - a >= 0 ? '+' : '') + (b - a) + ' MB 自起點' : ''; })() },
  { label: '核心 RSS', value: (last.coreRSS_MB || '—') + ' MB' },
  { label: 'CPU 合計', value: (last.cpuTotal || '—') + ' %',
    sub: `核心 ${last.coreCPU || '—'} + Electron ${last.elCPU || '—'}(單核百分比)` },
  { label: 'SKIP / UNANS', value: `${last.skip} / ${last.unans}`,
    tone: (num(last.skip) - num(first.skip) > 0 || num(last.unans) - num(first.unans) > 0) ? 'warn' : 'ok',
    sub: (() => { const ds = num(last.skip) - num(first.skip), du = num(last.unans) - num(first.unans);
      return (ds || du) ? `本次 +${ds} / +${du}` : '本次未增加'; })() },
  // Least squares, not last-minus-first: a single noisy end sample would
  // otherwise decide the verdict on the one number this run was made to
  // produce. The threshold is deliberately generous -- the defect was three
  // orders of magnitude above it.
  (() => {
    const m = slopePerMin('dom_nodes'), l = slopePerMin('dom_listeners');
    const fmt1 = (v) => (v == null ? '—' : (v >= 0 ? '+' : '') + v.toFixed(1));
    return { label: 'DOM 斜率', value: fmt1(m) + ' /min',
      tone: m == null ? undefined : (m > 60 ? 'bad' : m > 12 ? 'warn' : 'ok'),
      sub: `監聽器 ${fmt1(l)} /min` };
  })(),
  { label: '故障分鐘', value: String(faults.filter((f) => f.length).length),
    tone: faults.some((f) => f.length) ? 'bad' : 'ok', sub: `共 ${rows.length} 個取樣點` },
];

const payload = {
  t, panels: panels.map((p) => ({ ...p })), faults,
  meta, summary,
  spanMin: t[t.length - 1],
  rows: rows.length,
  generated: new Date().toISOString().replace('T', ' ').slice(0, 19),
  source: path.basename(CSV),
  title: TITLE || 'visSele soak',
};

fs.writeFileSync(OUT, render(payload));
console.log(`${OUT}  (${rows.length} samples, ${payload.spanMin} min, `
          + `${faults.filter((f) => f.length).length} fault ticks)`);

// -----------------------------------------------------------------------------
function render(d) {
  const J = JSON.stringify(d).replace(/</g, '\\u003c');
  return `<title>${esc(d.title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600&family=IBM+Plex+Sans+Condensed:wght@500;600;700&family=IBM+Plex+Sans:wght@400;500&display=swap">
<style>
/* An instrument readout, committed to one dark world on purpose -- this is a
   chart recorder, not a document, and a light variant of it would be a
   different object. Every colour is painted explicitly so the page holds
   whatever ground the viewer's theme puts behind it. */
:root{
  --ground:#0d1014; --panel:#161a20; --line:#232a33; --line-soft:#1b212a;
  --ink:#e4e8ee; --ink-dim:#8b95a3; --ink-faint:#5b6673;
  --t1:#5ec8f2; --t2:#b98cff; --t3:#63b0d8; --t4:#7f8fa6; --t5:#3f6b86;
  --ok:#45d19a; --warn:#f0b02e; --bad:#ff6b6b;
  --mono:"IBM Plex Mono",ui-monospace,Consolas,monospace;
  --cond:"IBM Plex Sans Condensed","IBM Plex Sans",system-ui,sans-serif;
  --sans:"IBM Plex Sans","Microsoft JhengHei",system-ui,sans-serif;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--ground);color:var(--ink);font-family:var(--sans);font-size:15px;line-height:1.55}
.wrap{max-width:1180px;margin:0 auto;padding:34px 22px 70px}

header{display:flex;flex-wrap:wrap;gap:16px 28px;align-items:flex-end;justify-content:space-between;
  padding-bottom:18px;border-bottom:1px solid var(--line)}
h1{font-family:var(--cond);font-weight:700;font-size:30px;letter-spacing:.01em;margin:0;text-wrap:balance}
.sub{color:var(--ink-dim);font-size:13px;font-family:var(--mono)}

/* Operating point. A soak reported without it invites comparing two runs that
   were never comparable. */
.op{display:flex;flex-wrap:wrap;gap:8px;margin:18px 0 26px}
.op span{font-family:var(--mono);font-size:12px;color:var(--ink-dim);
  border:1px solid var(--line);border-radius:999px;padding:4px 11px}
.op b{color:var(--ink);font-weight:500}

.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(158px,1fr));gap:11px;margin-bottom:30px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:9px;padding:12px 14px;
  border-left:3px solid var(--line)}
.card.ok{border-left-color:var(--ok)} .card.warn{border-left-color:var(--warn)} .card.bad{border-left-color:var(--bad)}
.card .k{font-size:11px;letter-spacing:.12em;text-transform:uppercase;color:var(--ink-dim);font-family:var(--cond);font-weight:600}
.card .v{font-family:var(--mono);font-size:21px;font-weight:500;font-variant-numeric:tabular-nums;margin-top:3px}
.card .s{font-family:var(--mono);font-size:11px;color:var(--ink-faint);margin-top:2px}

.panel{background:var(--panel);border:1px solid var(--line);border-radius:9px;margin-bottom:13px;overflow:hidden}
.phead{display:flex;flex-wrap:wrap;align-items:baseline;gap:10px 16px;padding:11px 15px 9px}
.ptitle{font-family:var(--cond);font-weight:600;font-size:16px;letter-spacing:.02em}
.punit{font-family:var(--mono);font-size:11px;color:var(--ink-faint)}
.pnote{flex:1 1 340px;font-size:12px;color:var(--ink-dim)}
.legend{display:flex;flex-wrap:wrap;gap:4px 14px;padding:0 15px 9px;font-family:var(--mono);font-size:11.5px}
.legend i{display:inline-block;width:11px;height:2.5px;border-radius:2px;vertical-align:middle;margin-right:6px}
.legend b{font-weight:500;font-variant-numeric:tabular-nums}
.cv{display:block;width:100%;height:158px;cursor:crosshair}

.ribbon{padding:11px 15px 15px}
.rlabel{font-family:var(--cond);font-weight:600;font-size:16px;margin-bottom:8px}
.rbar{display:flex;gap:1px;height:22px;border-radius:4px;overflow:hidden;background:var(--line-soft)}
.rbar div{flex:1 1 0;min-width:1px;background:var(--ok);opacity:.5}
.rbar div.bad{background:var(--bad);opacity:1}
.rlist{margin-top:9px;font-family:var(--mono);font-size:12px;color:var(--ink-dim);
  max-height:150px;overflow:auto}
.rlist div{padding:1px 0}
.rlist .tm{color:var(--warn)}
.clean{color:var(--ok);font-family:var(--mono);font-size:12.5px}

footer{margin-top:26px;color:var(--ink-faint);font-family:var(--mono);font-size:11.5px}
</style>
<div class="wrap">
  <header>
    <div>
      <h1 id="h1"></h1>
      <div class="sub" id="sub"></div>
    </div>
  </header>
  <div class="op" id="op"></div>
  <div class="cards" id="cards"></div>
  <div id="panels"></div>
  <div class="panel ribbon">
    <div class="rlabel">故障</div>
    <div class="rbar" id="rbar"></div>
    <div id="rlist" class="rlist"></div>
  </div>
  <footer id="foot"></footer>
</div>
<script>
const D = ${J};

const $ = (id) => document.getElementById(id);
const el = (tag, cls, text) => { const n = document.createElement(tag);
  if (cls) n.className = cls; if (text !== undefined) n.textContent = text; return n; };
const css = (v) => getComputedStyle(document.documentElement).getPropertyValue(v).trim();

$('h1').textContent = D.title;
$('sub').textContent = D.rows + ' samples · ' + D.spanMin + ' min · ' + D.source + ' · ' + D.generated;

for (const [k, v] of Object.entries({
  'plate freq': D.meta.freq, 'gate ceiling': D.meta.ceiling ? D.meta.ceiling + '/s' : null,
  'min_detect_sep_us': D.meta.sep,
})) if (v) { const s = el('span'); s.append(k + ' '); s.append(el('b', null, v)); $('op').appendChild(s); }

for (const c of D.summary) {
  const n = el('div', 'card' + (c.tone ? ' ' + c.tone : ''));
  n.appendChild(el('div', 'k', c.label));
  n.appendChild(el('div', 'v', c.value));
  if (c.sub) n.appendChild(el('div', 's', c.sub));
  $('cards').appendChild(n);
}

// --- drawing -------------------------------------------------------------------
// Canvas rather than hand-built SVG: these are generated traces, redrawn on
// every hover and every resize.
const charts = [];

function niceMax(v) {
  if (!(v > 0)) return 1;
  const e = Math.pow(10, Math.floor(Math.log10(v)));
  for (const m of [1, 1.5, 2, 2.5, 3, 4, 5, 7.5, 10]) if (v <= m * e) return m * e;
  return 10 * e;
}
function niceMin(v) { return v >= 0 ? 0 : -niceMax(-v); }

function build(p) {
  const wrap = el('div', 'panel');
  const head = el('div', 'phead');
  head.appendChild(el('div', 'ptitle', p.title));
  head.appendChild(el('div', 'punit', p.unit));
  head.appendChild(el('div', 'pnote', p.note || ''));
  wrap.appendChild(head);

  const leg = el('div', 'legend');
  const vals = [];
  for (const s of p.lines) {
    const item = el('span');
    const sw = el('i'); sw.style.background = s.color;
    item.appendChild(sw);
    item.append(s.name + ' ');
    const b = el('b', null, '');
    item.appendChild(b);
    leg.appendChild(item);
    vals.push(b);
  }
  if (p.bars) {
    const item = el('span');
    const sw = el('i'); sw.style.background = p.bars.color;
    item.appendChild(sw); item.append(p.bars.name + ' ');
    const b = el('b', null, ''); item.appendChild(b); leg.appendChild(item); vals.push(b);
  }
  wrap.appendChild(leg);

  const cv = el('canvas', 'cv');
  wrap.appendChild(cv);
  $('panels').appendChild(wrap);
  charts.push({ p, cv, vals });
}
D.panels.forEach(build);

let hoverX = null;

function draw(c) {
  const { p, cv } = c;
  const dpr = window.devicePixelRatio || 1;
  const w = cv.clientWidth, h = cv.clientHeight;
  cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
  const x = cv.getContext('2d');
  x.setTransform(dpr, 0, 0, dpr, 0, 0);
  x.clearRect(0, 0, w, h);

  const padL = 52, padR = 12, padT = 10, padB = 20;
  const iw = w - padL - padR, ih = h - padT - padB;
  const tmax = D.t[D.t.length - 1] || 1;

  let lo = 0, hi = 0;
  for (const s of p.lines) for (const v of s.data) if (v != null) { if (v > hi) hi = v; if (v < lo) lo = v; }
  if (p.bars) for (const v of p.bars.data) if (v != null && v > hi) hi = v;
  hi = niceMax(hi || 1); lo = niceMin(lo);
  const X = (tm) => padL + (tm / tmax) * iw;
  const Y = (v) => padT + ih - ((v - lo) / (hi - lo)) * ih;

  // Grid: half-hour ticks, because that is the unit anyone reads a six-hour run
  // in, and a zero line whenever the scale crosses it.
  x.strokeStyle = css('--line-soft'); x.lineWidth = 1;
  x.fillStyle = css('--ink-faint');
  x.font = '10px ' + css('--mono');
  const step = tmax > 240 ? 60 : tmax > 90 ? 30 : tmax > 20 ? 10 : 1;
  for (let tm = 0; tm <= tmax + 0.001; tm += step) {
    const px = Math.round(X(tm)) + .5;
    x.beginPath(); x.moveTo(px, padT); x.lineTo(px, padT + ih); x.stroke();
    x.textAlign = 'center'; x.fillText(String(Math.round(tm)), px, h - 6);
  }
  x.textAlign = 'right';
  for (const frac of [0, .5, 1]) {
    const v = lo + (hi - lo) * frac;
    const py = Math.round(Y(v)) + .5;
    x.strokeStyle = (v === 0 && lo < 0) ? css('--line') : css('--line-soft');
    x.beginPath(); x.moveTo(padL, py); x.lineTo(padL + iw, py); x.stroke();
    x.fillStyle = css('--ink-faint');
    x.fillText(fmt(v), padL - 7, py + 3.5);
  }

  if (p.bars) {
    x.fillStyle = p.bars.color.startsWith('var') ? css(p.bars.color.slice(4, -1)) : p.bars.color;
    const bw = Math.max(1.5, iw / Math.max(D.t.length, 1) * .55);
    p.bars.data.forEach((v, i) => { if (!v) return;
      const px = X(D.t[i]); x.fillRect(px - bw / 2, Y(v), bw, padT + ih - Y(v)); });
  }

  for (const s of p.lines) {
    x.strokeStyle = s.color.startsWith('var') ? css(s.color.slice(4, -1)) : s.color;
    x.lineWidth = s.width || 1.3;
    x.lineJoin = 'round';
    x.beginPath();
    let pen = false;
    s.data.forEach((v, i) => {
      if (v == null) { pen = false; return; }
      const px = X(D.t[i]), py = Y(v);
      if (!pen) { x.moveTo(px, py); pen = true; } else x.lineTo(px, py);
    });
    x.stroke();
  }

  // Fault marks on every panel, not only the ribbon: a spike means little until
  // you can see whether something went wrong at the same minute.
  x.fillStyle = css('--bad');
  D.faults.forEach((f, i) => { if (!f.length) return;
    x.globalAlpha = .16; x.fillRect(X(D.t[i]) - 1, padT, 2, ih); x.globalAlpha = 1; });

  if (hoverX != null) {
    const px = Math.max(padL, Math.min(padL + iw, hoverX));
    x.strokeStyle = css('--ink-dim'); x.lineWidth = 1;
    x.setLineDash([3, 3]);
    x.beginPath(); x.moveTo(px + .5, padT); x.lineTo(px + .5, padT + ih); x.stroke();
    x.setLineDash([]);
  }
}

function fmt(v) {
  const a = Math.abs(v);
  if (a >= 1000) return (v / 1000).toFixed(a >= 10000 ? 0 : 1) + 'k';
  if (a >= 10 || a === 0) return String(Math.round(v));
  return v.toFixed(1);
}

// The crosshair reads EVERY panel at the same instant, which is the whole
// reason they share an axis.
function readout(idx) {
  for (const c of charts) {
    let k = 0;
    for (const s of c.p.lines) { const v = s.data[idx]; c.vals[k++].textContent = v == null ? '—' : fmt(v); }
    if (c.p.bars) { const v = c.p.bars.data[idx]; c.vals[k++].textContent = v == null ? '—' : fmt(v); }
  }
  $('sub').textContent = idx == null
    ? D.rows + ' samples · ' + D.spanMin + ' min · ' + D.source + ' · ' + D.generated
    : 't = ' + D.t[idx] + ' min' + (D.faults[idx] && D.faults[idx].length ? '  ⚠ ' + D.faults[idx].join(', ') : '');
}

function nearest(cv, clientX) {
  const r = cv.getBoundingClientRect();
  const padL = 52, padR = 12;
  const iw = r.width - padL - padR;
  const tmax = D.t[D.t.length - 1] || 1;
  const tm = ((clientX - r.left - padL) / iw) * tmax;
  let best = 0, bd = Infinity;
  D.t.forEach((v, i) => { const dd = Math.abs(v - tm); if (dd < bd) { bd = dd; best = i; } });
  return best;
}

for (const c of charts) {
  c.cv.addEventListener('mousemove', (e) => {
    const idx = nearest(c.cv, e.clientX);
    const r = c.cv.getBoundingClientRect();
    hoverX = e.clientX - r.left;
    readout(idx);
    charts.forEach(draw);
  });
  c.cv.addEventListener('mouseleave', () => { hoverX = null; readout(D.t.length - 1); charts.forEach(draw); });
}

// --- fault ribbon ---------------------------------------------------------------
const bar = $('rbar');
D.faults.forEach((f, i) => {
  const seg = el('div', f.length ? 'bad' : '');
  seg.title = 't=' + D.t[i] + ' min' + (f.length ? ': ' + f.join(', ') : ': clean');
  bar.appendChild(seg);
});
const list = $('rlist');
const bad = D.faults.map((f, i) => [i, f]).filter(([, f]) => f.length);
if (!bad.length) list.appendChild(el('div', 'clean',
  '全部 ' + D.rows + ' 個取樣點都是 state 101、無錯誤,且 skip / unanswered / nomatch / link / 相機幀 全部沒有增加。'));
else for (const [i, f] of bad) {
  const row = el('div');
  row.appendChild(el('span', 'tm', String(D.t[i]).padStart(6) + ' min  '));
  row.append(f.join(' · '));
  list.appendChild(row);
}

$('foot').textContent = 'appRoot ' + (D.meta.appRoot || '?') + '   ·   workingDir ' + (D.meta.workingDir || '?');

readout(D.t.length - 1);
charts.forEach(draw);
let rt; addEventListener('resize', () => { clearTimeout(rt); rt = setTimeout(() => charts.forEach(draw), 80); });
</script>`;
}

function esc(s) {
  return String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
}
