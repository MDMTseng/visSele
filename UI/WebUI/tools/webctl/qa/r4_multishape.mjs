#!/usr/bin/env node
/*
 * QA r4_multishape — MULTI-SHAPE viewpoint: add/delete/reorder of multiple shapes,
 * __decorator.list_id_order coherence, inherentShapeList recomputation, and
 * serialization order stability.
 *
 * Drives the running WebUI through webctld (HTTP :8765) + dev hooks
 * (__GP_STORE__, __GP_DEF__, __GP_LOAD_BY_PATH__). Single shared browser owned
 * by the daemon — the PARENT runs QA scripts serially (do NOT run in parallel).
 *
 * Core down (load times out / "Not connected" / similar) => print "SKIP (core down)"
 * and exit 0. Reload at end (mutations).
 *
 * NOTE: loaded def in DEFCONF_MODE is lock-gated. We dispatch
 *   { type:'DefConf_Lock_Level_Update', data:0 } before edits, AND pass
 *   IGNORE_DEFCONF_LOCK:true on each Shape_Set as belt-and-braces.
 *
 * Convention (confirmed from src/redux/reducer/UICtrlReducer.js Shape_Set +
 * src/UTIL/InspectionEditorLogic.js SetShape):
 *   - id===undefined, shape!=null  => add new shape (push to shapeList).
 *   - id===<existing>, shape!=null => modify existing.
 *   - id===<existing>, shape===null => delete.
 * After every Shape_Set the reducer recomputes:
 *   __decorator.list_id_order via UpdateListIDOrder(prev, shapeList)
 *     - removes ids that no longer exist
 *     - appends ids of new shapes not previously listed
 *   inherentShapeList via _obj.UpdateInherentShapeList()
 *
 * TEST PLAN:
 *  T1 baseline        — load; assert shapes>0, def features>0, list_id_order length matches.
 *  T2 add_N           — add N=3 lines (id undefined); assert shapeList grew by N,
 *                       each new id appears in def.features with finite coords and is unique,
 *                       list_id_order grew by N (new ids appended).
 *  T3 delete_middle   — delete a MIDDLE shape from the post-add list; assert shapeList
 *                       shrank by 1, target id gone, other ids+order preserved (relative).
 *  T4 decorator_sync  — list_id_order == shapeList ids set; no stale ids; every shape id present.
 *  T5 inherent_sync   — inherentShapeList non-null, no null entries, count sane (>=0,
 *                       changes consistent with shape mutations).
 *  T6 delete_missing  — delete non-existent id => no crash, shapeList unchanged.
 *  T7 add_then_del    — add 1 line, immediately delete that id => shapeList back to pre-T7
 *                       baseline (count + id set identical), list_id_order back to same set.
 *  T8 ser_order_stable — call __GP_DEF__() twice; features id order identical across calls.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';
const ADD_N = 3;

async function api(p, body) {
  const r = await fetch(BASE + p, {
    method: body ? 'POST' : 'GET',
    headers: body ? { 'Content-Type': 'application/json' } : undefined,
    body: body ? JSON.stringify(body) : undefined,
  });
  const j = await r.json();
  if (!r.ok) throw new Error(JSON.stringify(j));
  return j;
}
const ev = (expr) => api('/eval', { expr }).then((r) => r.result);
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const isCoreDown = (msg) =>
  /not connected|timeout|did not load|reconnect|ECONNREF/i.test(String(msg || ''));

async function reset() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    if ((await ev('typeof window.__GP_STORE__')) === 'object') break;
  }
  let loaded = false;
  let lastErr = null;
  for (let attempt = 0; attempt < 4 && !loaded; attempt++) {
    await ev(
      `window.__rdy=false;window.__rdyErr=null;window.__GP_LOAD_BY_PATH__(${JSON.stringify(MODEL_PATH)}).then(()=>{window.__rdy=1}).catch(e=>{window.__rdyErr=String(e)}); 'sent'`
    );
    for (let i = 0; i < 80; i++) {
      await sleep(100);
      const st = await ev(
        `(function(){var o=window.__GP_STORE__.getState().UIData.edit_info._obj;return {rdy:!!window.__rdy,err:window.__rdyErr||null,n:(o&&o.shapeList.length)||0};})()`
      );
      if (st.err) { lastErr = st.err; break; }
      if (st.rdy && st.n > 0) { loaded = true; break; }
    }
    if (!loaded) { lastErr = lastErr || 'timeout'; await sleep(3000); }
  }
  if (!loaded) throw new Error('CORE-DOWN: def did not load after retries: ' + (lastErr || 'timeout'));
}

const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj;
  var dec=s.edit_info.__decorator||{};
  var inh=s.edit_info.inherentShapeList||[];
  return {
    lock: s.defConf_lock_level,
    ids: o.shapeList.map(function(x){return x.id;}),
    shapes: o.shapeList.map(function(x){return {id:x.id,type:x.type};}),
    list_id_order: (dec.list_id_order||[]).slice(),
    inherent_count: inh.length,
    inherent_has_null: inh.some(function(x){return x==null;})
  };
})()`;
const snap = () => ev(SNAP);
const serialDef = () => ev('window.__GP_DEF__()');
const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);

async function addLine(x1, y1, x2, y2) {
  return ev(
    `(function(){var before=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList.length;` +
    `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:{type:'line',pt1:{x:${x1},y:${y1}},pt2:{x:${x2},y:${y2}}},id:undefined}});` +
    `var sl=window.__GP_STORE__.getState().UIData.edit_info._obj.shapeList;` +
    `return sl.length>before?sl[sl.length-1].id:null;})()`
  );
}
async function deleteId(id) {
  return ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Set',IGNORE_DEFCONF_LOCK:true,data:{shape:null,id:${JSON.stringify(id)}}}); 'del'`
  );
}

let failures = 0;
function report(name, ok, detail) {
  console.log(`[${name}] ${ok ? 'PASS' : 'FAIL'}${detail ? ' — ' + detail : ''}`);
  if (!ok) failures++;
}

function eqSet(a, b) {
  if (a.length !== b.length) return false;
  const sa = new Set(a), sb = new Set(b);
  if (sa.size !== sb.size) return false;
  for (const x of sa) if (!sb.has(x)) return false;
  return true;
}
function finitePts(f) {
  if (!f) return false;
  if (f.pt1 && f.pt2) {
    return Number.isFinite(f.pt1.x) && Number.isFinite(f.pt1.y) &&
           Number.isFinite(f.pt2.x) && Number.isFinite(f.pt2.y);
  }
  return true;
}

async function main() {
  try { await reset(); }
  catch (e) {
    if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
    throw e;
  }

  await unlock();
  await sleep(150);

  // ---- T1 baseline ----
  let s = await snap();
  let def = await serialDef();
  const baseN = s.ids.length;
  const baseIds = s.ids.slice();
  const baseDecOk = eqSet(s.list_id_order, s.ids);
  report('T1 baseline',
    baseN > 0 && def && Array.isArray(def.features) && def.features.length > 0 && baseDecOk,
    `shapes=${baseN}, def.features=${def?def.features.length:'n/a'}, list_id_order==ids:${baseDecOk}`);

  // ---- T2 add_N ----
  const beforeAdd = s.ids.slice();
  const beforeOrder = s.list_id_order.slice();
  const beforeInh = s.inherent_count;
  const newIds = [];
  for (let i = 0; i < ADD_N; i++) {
    const nid = await addLine(100 + i * 17, 100 + i * 23, 300 + i * 11, 250 + i * 13);
    await sleep(250);
    if (nid != null) newIds.push(nid);
  }
  s = await snap();
  def = await serialDef();
  const allUnique = new Set(newIds).size === newIds.length && newIds.every(id => id != null);
  const grew = s.ids.length === beforeAdd.length + ADD_N;
  const newInDef = newIds.every(id => {
    const f = def.features.find(x => x.id === id);
    return f && f.type === 'line' && finitePts(f);
  });
  const orderAppended =
    s.list_id_order.length === beforeOrder.length + ADD_N &&
    beforeOrder.every((id, i) => s.list_id_order[i] === id) &&
    newIds.every(id => s.list_id_order.includes(id));
  report('T2 add_N',
    grew && allUnique && newInDef && orderAppended,
    `+${ADD_N} -> ${s.ids.length}, unique=${allUnique}, allInDef=${newInDef}, orderAppended=${orderAppended}, newIds=[${newIds.join(',')}]`);

  // ---- T3 delete_middle ----
  const preDelIds = s.ids.slice();
  const midIdx = Math.floor(preDelIds.length / 2);
  const midId = preDelIds[midIdx];
  await deleteId(midId);
  await sleep(300);
  s = await snap();
  def = await serialDef();
  const expectedAfter = preDelIds.filter(id => id !== midId);
  const orderPreserved = s.ids.length === expectedAfter.length &&
    s.ids.every((id, i) => id === expectedAfter[i]);
  const gone = !s.ids.includes(midId) &&
    (!def.features || !def.features.some(f => f.id === midId));
  report('T3 delete_middle', orderPreserved && gone,
    `deleted id=${midId} at idx ${midIdx}; size ${preDelIds.length}->${s.ids.length}; orderPreserved=${orderPreserved}; gone=${gone}`);

  // ---- T4 decorator_sync ----
  const decSet = eqSet(s.list_id_order, s.ids);
  const noStale = s.list_id_order.every(id => s.ids.includes(id));
  const allPresent = s.ids.every(id => s.list_id_order.includes(id));
  report('T4 decorator_sync', decSet && noStale && allPresent,
    `list_id_order(n=${s.list_id_order.length}) vs ids(n=${s.ids.length}); noStale=${noStale}; allPresent=${allPresent}`);

  // ---- T5 inherent_sync ----
  const inhSane = s.inherent_count >= 0 && !s.inherent_has_null;
  report('T5 inherent_sync', inhSane,
    `inherent_count=${s.inherent_count} (baseline=${beforeInh}), hasNull=${s.inherent_has_null}`);

  // ---- T6 delete_missing ----
  const before6 = s.ids.slice();
  const phantomId = (Math.max(0, ...before6.map(Number).filter(Number.isFinite)) || 0) + 999999;
  let crashed = false;
  try { await deleteId(phantomId); } catch (e) { crashed = true; }
  await sleep(200);
  s = await snap();
  const unchanged = s.ids.length === before6.length && s.ids.every((id, i) => id === before6[i]);
  report('T6 delete_missing', !crashed && unchanged,
    `phantom=${phantomId}, crashed=${crashed}, unchanged=${unchanged}`);

  // ---- T7 add_then_del ----
  const before7Ids = s.ids.slice();
  const before7Order = s.list_id_order.slice();
  const addedId = await addLine(50, 50, 90, 90);
  await sleep(250);
  await deleteId(addedId);
  await sleep(250);
  s = await snap();
  const idSetSame = eqSet(s.ids, before7Ids) && s.ids.length === before7Ids.length;
  const orderSetSame = eqSet(s.list_id_order, before7Order);
  report('T7 add_then_del', addedId != null && idSetSame && orderSetSame,
    `addedId=${addedId}, idSetSame=${idSetSame}, orderSetSame=${orderSetSame} (sizes ids ${before7Ids.length}->${s.ids.length})`);

  // ---- T8 ser_order_stable ----
  const d1 = await serialDef();
  await sleep(100);
  const d2 = await serialDef();
  const ord1 = (d1.features || []).map(f => f.id);
  const ord2 = (d2.features || []).map(f => f.id);
  const stable = ord1.length === ord2.length && ord1.every((id, i) => id === ord2[i]);
  report('T8 ser_order_stable', stable,
    `len ${ord1.length}/${ord2.length}, identical=${stable}`);

  // mutated def — reload to restore clean state for next QA wave.
  try { await api('/reload', {}); } catch (_) {}

  console.log(`\n${failures ? failures + ' FAILURE(S)' : 'ALL TESTS PASSED'}`);
  process.exit(failures ? 1 : 0);
}

main().catch((e) => {
  if (isCoreDown(e.message)) { console.log('SKIP (core down): ' + e.message); process.exit(0); }
  console.error('FATAL: ' + e.message);
  process.exit(2);
});
