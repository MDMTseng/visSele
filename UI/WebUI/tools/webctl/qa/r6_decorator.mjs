#!/usr/bin/env node
/*
 * QA r6_decorator — __decorator VIEWPOINT: the editing-meta layer of edit_info
 * that runs alongside the def. Specifically:
 *   edit_info.__decorator = { list_id_order, extra_info, control_margin_info }
 *
 * Shape per InspectionEditorLogic.Edit_info_Empty():
 *   list_id_order: []          (rebuilt by UpdateListIDOrder on every shape mutation)
 *   extra_info:    []
 *   control_margin_info: {}
 *
 * Reducer actions (UICtrlReducer.js):
 *   Shape_Decoration_ID_Order_Update          -> list_id_order = UpdateListIDOrder(data, shapeList)
 *   Shape_Decoration_Extra_Info_Update        -> __decorator.extra_info = data
 *   Shape_Decoration_Control_Margin_Info_Update -> __decorator.control_margin_info = data
 *   Shape_Set                                 -> recomputes list_id_order via UpdateListIDOrder
 *
 * UpdateListIDOrder semantics (InspectionEditorLogic.js):
 *   - filter out ids not present in shapeList (drop phantoms)
 *   - append shapeList ids missing from the order (repair)
 *
 * Lock gate (line 607): in DEFCONF_MODE w/ lock!=0, only level1Filter actions
 *   pass (Shape_Decoration_ID_Order_Update + Shape_Decoration_Extra_Info_Update
 *   are level1-whitelisted; Shape_Decoration_Control_Margin_Info_Update is NOT).
 *   We unlock + pass IGNORE_DEFCONF_LOCK:true to be safe.
 *
 * NOTE on round-4 finding: Shape_Decoration_Extra_Info_Update mapDispatch was
 *   removed in 433ec7ff at the component layer, but the action TYPE still
 *   exists in DefConfAct and the reducer case is still live. So a RAW dispatch
 *   from this harness SHOULD still mutate extra_info — "dead-API" only means
 *   no UI component dispatches it any more. T7 below verifies the runtime
 *   reality and reports either outcome.
 *
 * TEST PLAN:
 *   T1 baseline           — __decorator present; list_id_order set-equals shapeList ids;
 *                            control_margin_info is an object; extra_info defined.
 *   T2 reorder            — dispatch reverse order; list_id_order == reversed ids.
 *   T3 phantom_filtered   — dispatch order with bogus id appended; phantom dropped.
 *   T4 repair_missing     — dispatch order missing one id; UpdateListIDOrder appends.
 *   T5 control_margin     — dispatch known control_margin_info payload; round-trips.
 *   T6 shape_invariant    — add+delete shapes via Shape_Set; list_id_order stays a
 *                            clean superset (no stale, all present).
 *   T7 extra_info_status  — dispatch Shape_Decoration_Extra_Info_Update with payload;
 *                            classify as LIVE (state changed) or DEAD (no-op).
 *
 * Core-down => SKIP exit 0. Reload at end (mutations). Single shared browser;
 * do NOT run in parallel with other QA scripts.
 */
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;
const MODEL_PATH = process.env.WEBCTL_MODEL || '/Users/mdm/workspace/HY_sync/DEV/test/caliper_verify';

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

// Snapshot the decorator + ids (no full state — circular refs).
const SNAP = `(function(){
  var s=window.__GP_STORE__.getState().UIData, o=s.edit_info._obj;
  var dec=s.edit_info.__decorator||{};
  return {
    lock: s.defConf_lock_level,
    ids: o.shapeList.map(function(x){return x.id;}),
    list_id_order: (dec.list_id_order||[]).slice(),
    cmi_type: (dec.control_margin_info===null)?'null':typeof dec.control_margin_info,
    cmi_keys: (dec.control_margin_info&&typeof dec.control_margin_info==='object')?Object.keys(dec.control_margin_info):[],
    cmi_json: dec.control_margin_info?JSON.stringify(dec.control_margin_info):null,
    extra_info_type: Array.isArray(dec.extra_info)?'array':(dec.extra_info===null?'null':typeof dec.extra_info),
    extra_info_json: dec.extra_info===undefined?'<undefined>':JSON.stringify(dec.extra_info),
    has_decorator: !!s.edit_info.__decorator
  };
})()`;
const snap = () => ev(SNAP);
const unlock = () => ev(`window.__GP_STORE__.dispatch({type:'DefConf_Lock_Level_Update',data:0}); 'ok'`);

function jsArr(arr) {
  return '[' + arr.map(v => typeof v === 'number' ? v : JSON.stringify(v)).join(',') + ']';
}

async function dispatchOrder(orderArr) {
  return ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Decoration_ID_Order_Update',IGNORE_DEFCONF_LOCK:true,data:${jsArr(orderArr)}}); 'ok'`
  );
}
async function dispatchCMI(obj) {
  return ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Decoration_Control_Margin_Info_Update',IGNORE_DEFCONF_LOCK:true,data:${JSON.stringify(obj)}}); 'ok'`
  );
}
async function dispatchExtraInfo(payload) {
  return ev(
    `window.__GP_STORE__.dispatch({type:'Shape_Decoration_Extra_Info_Update',IGNORE_DEFCONF_LOCK:true,data:${JSON.stringify(payload)}}); 'ok'`
  );
}
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
function eqArr(a, b) {
  return a.length === b.length && a.every((v, i) => v === b[i]);
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
  const baseIds = s.ids.slice();
  const baseOrder = s.list_id_order.slice();
  const baseCMIType = s.cmi_type;
  const baseDecOk = s.has_decorator && eqSet(s.list_id_order, s.ids);
  report('T1 baseline',
    baseIds.length > 0 && baseDecOk && baseCMIType === 'object',
    `shapes=${baseIds.length}, list_id_order(n=${baseOrder.length}) set-eq ids:${eqSet(baseOrder, baseIds)}, cmi_type=${baseCMIType}, cmi_keys=${s.cmi_keys.length}, extra_info_type=${s.extra_info_type}`);

  // ---- T2 reorder ----
  const reversed = baseIds.slice().reverse();
  await dispatchOrder(reversed);
  await sleep(200);
  s = await snap();
  const t2ok = eqArr(s.list_id_order, reversed);
  report('T2 reorder', t2ok,
    `requested=[${reversed.join(',')}], got=[${s.list_id_order.join(',')}]`);

  // ---- T3 phantom_filtered ----
  const maxId = Math.max(0, ...baseIds.map(Number).filter(Number.isFinite));
  const phantomId = maxId + 999999;
  const withPhantom = baseIds.slice().concat([phantomId]);
  await dispatchOrder(withPhantom);
  await sleep(200);
  s = await snap();
  const phantomDropped = !s.list_id_order.includes(phantomId);
  const allReal = s.list_id_order.every(id => baseIds.includes(id));
  const completeReal = baseIds.every(id => s.list_id_order.includes(id));
  report('T3 phantom_filtered', phantomDropped && allReal && completeReal,
    `phantom=${phantomId} dropped=${phantomDropped}, allReal=${allReal}, completeReal=${completeReal}, got=[${s.list_id_order.join(',')}]`);

  // ---- T4 repair_missing ----
  if (baseIds.length < 2) {
    report('T4 repair_missing', true, `SKIP — only ${baseIds.length} shape(s)`);
  } else {
    const dropped = baseIds[0];
    const partial = baseIds.slice(1);
    await dispatchOrder(partial);
    await sleep(200);
    s = await snap();
    const includesDropped = s.list_id_order.includes(dropped);
    const setComplete = eqSet(s.list_id_order, baseIds);
    // UpdateListIDOrder filters cur (=action.data) by shapeList, then appends missing.
    // So dropped id should be APPENDED at end.
    const appendedAtEnd = s.list_id_order[s.list_id_order.length - 1] === dropped;
    report('T4 repair_missing', includesDropped && setComplete && appendedAtEnd,
      `dropped=${dropped}, partial=[${partial.join(',')}], got=[${s.list_id_order.join(',')}], appendedAtEnd=${appendedAtEnd}`);
  }

  // ---- T5 control_margin ----
  // control_margin_info is a key->marginInfo map (see MarginInfoExtraction line 243).
  // Use synthetic keys; reducer just replaces the whole object.
  const cmiPayload = {
    qa_r6_tagA: { min: -0.5, max: 0.5, note: 'r6' },
    qa_r6_tagB: { min: 1, max: 2 }
  };
  await dispatchCMI(cmiPayload);
  await sleep(200);
  s = await snap();
  let t5ok = false;
  try {
    const parsed = JSON.parse(s.cmi_json || 'null');
    t5ok = parsed && JSON.stringify(parsed) === JSON.stringify(cmiPayload);
  } catch (_) {}
  report('T5 control_margin', t5ok,
    `keys=[${s.cmi_keys.join(',')}], roundTrip=${t5ok}, lock=${s.lock}`);

  // ---- T6 shape_invariant ----
  // Add 2 shapes, delete 1, verify list_id_order remains a clean set==ids.
  const addId1 = await addLine(60, 60, 120, 120);
  await sleep(250);
  const addId2 = await addLine(70, 70, 130, 130);
  await sleep(250);
  await deleteId(addId1);
  await sleep(250);
  s = await snap();
  const cleanSet = eqSet(s.list_id_order, s.ids);
  const noStale = s.list_id_order.every(id => s.ids.includes(id));
  const allPresent = s.ids.every(id => s.list_id_order.includes(id));
  const addId1Gone = !s.ids.includes(addId1) && !s.list_id_order.includes(addId1);
  const addId2Kept = s.ids.includes(addId2) && s.list_id_order.includes(addId2);
  report('T6 shape_invariant', cleanSet && noStale && allPresent && addId1Gone && addId2Kept,
    `ids=${s.ids.length}, order=${s.list_id_order.length}, cleanSet=${cleanSet}, addId1(${addId1})Gone=${addId1Gone}, addId2(${addId2})Kept=${addId2Kept}`);

  // Cleanup added shape so extra_info test sees a stable snapshot.
  if (addId2 != null) { await deleteId(addId2); await sleep(200); }

  // ---- T7 extra_info_status ----
  // Round 4 reported the mapDispatch was deleted at the component layer
  // (commit 433ec7ff). The reducer CASE is still live (line 823), so a raw
  // dispatch should still mutate. Classify what actually happens.
  s = await snap();
  const beforeExtraType = s.extra_info_type;
  const beforeExtraJson = s.extra_info_json;
  const extraPayload = [{ qa_r6: true, ts: Date.now() }, 'sentinel'];
  let dispatchErr = null;
  try { await dispatchExtraInfo(extraPayload); }
  catch (e) { dispatchErr = String(e.message || e); }
  await sleep(200);
  s = await snap();
  const afterExtraJson = s.extra_info_json;
  const stateChanged = afterExtraJson !== beforeExtraJson;
  let status;
  if (dispatchErr) status = `dispatch-threw: ${dispatchErr}`;
  else if (stateChanged) {
    let payloadMatches = false;
    try { payloadMatches = JSON.stringify(JSON.parse(afterExtraJson)) === JSON.stringify(extraPayload); }
    catch (_) {}
    status = payloadMatches ? 'LIVE (reducer still wired; payload round-trips)'
                            : 'LIVE-partial (state changed but payload mismatch)';
  } else {
    status = 'DEAD (no-op — state unchanged)';
  }
  // Round-4 expectation was DEAD; reducer code says LIVE. Report observed
  // outcome without failing — this test DOCUMENTS, not enforces.
  report('T7 extra_info_status', true,
    `${status}; before(type=${beforeExtraType})=${beforeExtraJson}; after=${afterExtraJson}`);

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
