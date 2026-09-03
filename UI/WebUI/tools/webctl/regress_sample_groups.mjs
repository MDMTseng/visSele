// The sample buffer's rules, driven through the two calls the reducer makes.
//
// Several of these would pass against the behaviour they exist to rule out, so
// they are asserted as WHICH-BUCKET and as POSITIONS rather than as membership:
//
//   - first match wins. "the sample is somewhere" is true of first-match and of
//     match-all; what differs is whether it is in ONE bucket, and which one.
//   - fill and stop. A ring holds the same COUNT as fill-and-stop; what differs
//     is which sample a full group turns away -- and the old ones are precisely
//     what somebody is keeping.
//   - delete shifts up and the tail refills. The panel numbers entries by
//     position, so an order that silently reversed would still pass a set check
//     and be wrong on every screen.
//
// Not covered: the reducer calling in on a real report, the panel, and the save
// round trip. Those need a live (or fake-camera) FI run.
import { makeCtl, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;
const URL = process.argv[2] || 'http://127.0.0.1:8083/';
process.env.WEBCTL_COLD = '1';
await freshPage(ctl, URL);
await sleep(4000);

let fail = 0;
const ck = (w, g, e) => {
  const ok = String(g) === String(e);
  console.log(`  ${ok ? 'ok  ' : 'FAIL'} ${w}: ${g}${ok ? '' : '  (expected ' + e + ')'}`);
  if (!ok) fail++;
};

// Shared helpers, re-injected per block. status: 0 SUCCESS, -1 FAILURE, -128 NA.
const PRE = [
  'var S = window.__GP_SAMPLE_STORE__;',
  'var frame = function(n){ return { jpegBytes:new Uint8Array(n), format:2, width:816,',
  '  height:528, scale:3, full_width:2448, full_height:2048 }; };',
  'var part = function(rows, t, extra){',
  '  var r = { time_ms:t, judgeReports: rows.map(function(x,i){',
  '    return { id:x[0], name:"m"+x[0], value:1+i, status:x[1] }; }) };',
  '  if (extra) for (var k in extra) r[k]=extra[k];',
  '  return r; };',
  'var feed = function(parts, img){ S.noteFinalisedReports(parts, "FI");',
  '  S.attachImage(img || frame(10), {ppb2b:1,mmpb2b:0.0138}, "D"); };',
  'var where = function(id){ var s=S.sampleStoreSnapshot(), out=[];',
  '  s.groups.forEach(function(g){ if(g.items.some(function(e){return e.id===id;}))',
  '    out.push(g.name); }); return out.join("+") || "nowhere"; };',
  'var counts = function(){ return S.sampleStoreSnapshot().groups',
  '  .map(function(g){ return g.name+":"+g.items.length; }).join(" "); };',
  'var pos = function(name){ var g=S.sampleStoreSnapshot().groups',
  '    .find(function(x){return x.name===name;});',
  '  return g ? g.items.map(function(e,i){return (i+1)+":"+e.time_ms;}).join(" ") : "no group"; };',
].join('\n');

const run = async (body) => JSON.parse(String(await ev('(function(){' + PRE + body + '})()')));

// ---- no groups means nothing is collected ----------------------------------
console.log('no groups');
const a = await run(`
  if(!S) return JSON.stringify({err:'NO_STORE'});
  S.setSampleGroups([]); S.clearSampleStore();
  feed([part([[1,-1]], 100)]);
  return JSON.stringify({ groups:S.sampleGroups().length,
                          bytes:S.sampleStoreSnapshot().bytes });
`);
if (a.err) { console.log('FAIL: ' + a.err); process.exit(1); }
ck('no groups configured', a.groups, 0);
ck('and nothing is kept', a.bytes, 0);

// ---- first match wins, and order is a setting ------------------------------
console.log('first match wins');
const b = await run(`
  // NARROW first: measure 10 NG while measure 3 is OK. BROAD second: any NG.
  S.setSampleGroups([
    {name:'10NG_3OK', cap:5, overall:'*', conds:{'10':'NG','3':'OK'}},
    {name:'anyNG',    cap:5, overall:'NG', conds:{}},
  ]);
  S.clearSampleStore();
  var res={};
  feed([part([[10,-1],[3,0]], 200)]);          // matches BOTH
  var s=S.sampleStoreSnapshot();
  res.both = where(s.groups[0].items[0].id);
  res.counts = counts();
  feed([part([[10,-1],[3,-1]], 201)]);         // matches only the broad one
  res.counts2 = counts();
  S.setSampleGroups([                          // reversed: broad first
    {name:'anyNG',    cap:5, overall:'NG', conds:{}},
    {name:'10NG_3OK', cap:5, overall:'*', conds:{'10':'NG','3':'OK'}},
  ]);
  S.clearSampleStore();
  feed([part([[10,-1],[3,0]], 202)]);
  res.reversed = counts();
  return JSON.stringify(res);
`);
ck('a sample matching two groups is in ONE', b.both, '10NG_3OK');
ck('and it is the first one', b.counts, '10NG_3OK:1 anyNG:0');
ck('a sample matching only the broad group', b.counts2, '10NG_3OK:1 anyNG:1');
ck('order decides: broad first starves the narrow one', b.reversed, 'anyNG:1 10NG_3OK:0');

// ---- conditions, dont-care, missing rows, and dropping ---------------------
console.log('conditions');
const c = await run(`
  S.setSampleGroups([
    {name:'m10NA', cap:5, overall:'*', conds:{'10':'NA'}},
    {name:'m10OK', cap:5, overall:'*', conds:{'10':'OK'}},
  ]);
  S.clearSampleStore();
  var res={};
  feed([part([[3,0]], 300)]);                  // measure 10 has NO ROW -> NA
  res.missingRow = counts();
  S.clearSampleStore();
  feed([part([[10,-128],[3,0]], 301)]);        // explicit NA
  res.explicitNA = counts();
  S.clearSampleStore();
  feed([part([[10,0],[3,-1]], 302)]);          // measure 3 NG, nobody asked
  res.dontCare = counts();
  S.clearSampleStore();
  feed([part([[10,-1]], 303)]);                // matches neither -> dropped
  res.dropped = counts();
  res.droppedBytes = S.sampleStoreSnapshot().bytes;
  return JSON.stringify(res);
`);
ck('a measure with no row counts as NA', c.missingRow, 'm10NA:1 m10OK:0');
ck('an explicit NA matches too', c.explicitNA, 'm10NA:1 m10OK:0');
ck('a measure nobody asked about is ignored', c.dontCare, 'm10NA:0 m10OK:1');
ck('a sample matching nothing is dropped', c.dropped, 'm10NA:0 m10OK:0');
ck('and costs nothing', c.droppedBytes, 0);

// ---- fill and stop, delete, shift up, refill at the tail -------------------
console.log('fill and stop');
const d = await run(`
  S.setSampleGroups([{name:'NG', cap:4, overall:'NG', conds:{}}]);
  S.clearSampleStore();
  var res={};
  [10,20,30,40].forEach(function(t){ feed([part([[1,-1]], t)]); });
  res.filled = pos('NG');
  res.full = S.sampleStoreSnapshot().groups[0].full;

  feed([part([[1,-1]], 50)]);
  res.afterRefused = pos('NG');
  res.skipped = S.sampleStoreSnapshot().groups[0].skipped;

  var gid = S.sampleGroups()[0].id;
  S.setSampleGroups([{id:gid, name:'NG', cap:2, overall:'NG', conds:{}}]);
  res.afterLowerCap = S.sampleStoreSnapshot().groups[0].items.length;
  S.setSampleGroups([{id:gid, name:'NG', cap:4, overall:'NG', conds:{}}]);

  var id2 = S.sampleStoreSnapshot().groups[0].items[1].id;
  res.removed = S.removeSampleEntry(id2);
  res.removeMissing = S.removeSampleEntry(999999);
  res.afterDelete = pos('NG');
  res.fullAfterDelete = S.sampleStoreSnapshot().groups[0].full;

  feed([part([[1,-1]], 60)]);
  res.afterRefill = pos('NG');
  return JSON.stringify(res);
`);
ck('filled in capture order', d.filled, '1:10 2:20 3:30 4:40');
ck('full at the cap', d.full, true);
ck('a new sample while full changes NOTHING', d.afterRefused, '1:10 2:20 3:30 4:40');
ck('and is counted as turned away', d.skipped, 1);
ck('a lowered cap keeps what is held', d.afterLowerCap, 4);
ck('remove reports success', d.removed, true);
ck('removing a missing id reports false', d.removeMissing, false);
ck('deleting #2 shifts the rest UP', d.afterDelete, '1:10 2:30 3:40');
ck('and it is no longer full', d.fullAfterDelete, false);
ck('the next sample fills the TAIL', d.afterRefill, '1:10 2:30 3:40 4:60');

// ---- reconfiguring, and what an entry costs --------------------------------
console.log('reconfiguring, and what it costs');
const e = await run(`
  S.setSampleGroups([
    {name:'A', cap:5, overall:'NG', conds:{}},
    {name:'B', cap:5, overall:'OK', conds:{}},
  ]);
  S.clearSampleStore();
  var res={};
  var f = frame(1000);
  feed([part([[1,-1]],1), part([[1,-1]],2), part([[1,-1]],3)], f);
  var s=S.sampleStoreSnapshot();
  res.three = s.groups[0].items.length;
  var repBytes=0;
  s.groups.forEach(function(g){ g.items.forEach(function(x){ repBytes += x.bytesReport||0; }); });
  res.frameBytes = s.bytes - repBytes;
  res.perEntryReports = repBytes > 0;

  S.clearSampleStore();
  S.noteFinalisedReports([part([[1,-1]],9,{isCurObj:false,
    detectedLines:[{id:1}], searchPoints:[{id:2}]})], 'FI');
  S.attachImage(frame(10), {}, 'D');
  var g0=S.sampleStoreSnapshot().groups[0].items[0];
  res.keptGeometry = g0.report.searchPoints.length;
  res.isCurObj = g0.report.isCurObj;

  var cfg = S.sampleGroups();
  S.setSampleGroups([ cfg[1], Object.assign({}, cfg[0], {name:'A renamed'}) ]);
  var s2=S.sampleStoreSnapshot();
  res.afterReorder = s2.groups.map(function(g){return g.name+':'+g.items.length;}).join(' ');
  return JSON.stringify(res);
`);
ck('three parts in one frame make three entries', e.three, 3);
ck('the shared frame is counted once', e.frameBytes, 1000);
ck('each entry carries its own report bytes', e.perEntryReports, true);
ck('the geometry the overlay draws is kept', e.keptGeometry, 1);
ck('isCurObj stamped true or the overlay draws nothing', e.isCurObj, true);
ck('rename and reorder keep the samples', e.afterReorder, 'B:0 A renamed:1');

// ---- the groups survive a reload (localStorage) ----------------------------
console.log('persistence');
await ev(`window.__GP_SAMPLE_STORE__.setSampleGroups([
  {name:'persisted', cap:7, overall:'NG', conds:{'42':'NA'}}])`);
await freshPage(ctl, URL);
await sleep(4000);
const f = JSON.parse(String(await ev(`(function(){
  var g = window.__GP_SAMPLE_STORE__.sampleGroups();
  return JSON.stringify({ n:g.length, name:g[0]&&g[0].name, cap:g[0]&&g[0].cap,
                          overall:g[0]&&g[0].overall,
                          cond:g[0]&&g[0].conds&&g[0].conds['42'] });})()`)));
ck('the group list survived a reload', f.n, 1);
ck('with its name', f.name, 'persisted');
ck('its cap', f.cap, 7);
ck('its overall condition', f.overall, 'NG');
ck('and its per-measure condition', f.cond, 'NA');

// Leave the bench as it was found -- these are somebody's real filter groups.
await ev(`window.__GP_SAMPLE_STORE__.setSampleGroups([])`);

console.log(fail ? `\n${fail} FAILED` : '\nPASS');
process.exit(fail ? 1 : 0);
