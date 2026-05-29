#!/usr/bin/env node
/* =============================================================================
 * r2_bpg.mjs — BPG BINARY PROTOCOL framing & decode-robustness QA.
 *
 * VIEWPOINT: src/UTIL/BPG_Protocol.js raw decoders, exercised DIRECTLY via the
 * new dev hook window.__GP_BPG__ (the BPG_Protocol default export, gated by
 * __DEV_MODE__). Buffers are CONSTRUCTED IN THE BROWSER (Uint8Array -> .buffer)
 * and fed to __GP_BPG__.raw2header / raw2obj / raw2Obj_IM / map_BPG_Packet2Act,
 * which take a {data: ArrayBuffer} "ws_evt" shape plus an offset. This suite is
 * 100% CORE-INDEPENDENT: no def load, no WS handshake — so it does NOT skip on
 * core-down. We only need /reload + the dev hook to appear.
 *
 * Header layout (9 bytes, see raw2header): [t0,t1, prop, pgIDhi,pgIDlo,
 *   len31..len24, len23..len16, len15..len8, len7..len0] (pgID & length BE).
 * IM extra header (15 bytes, see raw2Obj_IM): cam,sess, offX(2), offY(2),
 *   w(2), h(2), scale, full_w(2), full_h(2); image = 4*w*h RGBA bytes after it.
 *
 * TEST PLAN (each -> "[name] PASS/FAIL"; exit non-zero on real failure):
 *  T1 header-parse   : 9-byte header type "FL" prop 3 pgID 0x0102(258)
 *                      length 0x00000005(5) -> assert all four fields match.
 *  T2 header-short   : buffer <9 bytes -> raw2header returns null (guard).
 *  T3 im-oversize    : valid 9+15 header but claimed 4*w*h pixels EXCEED the
 *                      buffer -> Wave-1 bounds fix: returns OBJECT w/ image===null
 *                      (NOT a throw / RangeError).
 *  T4 im-truncated   : buffer smaller than 9+15 -> raw2Obj_IM returns null.
 *  T5 im-valid       : tiny w,h with enough trailing bytes -> image is a
 *                      Uint8ClampedArray of length 4*w*h.
 *  T6 obj-json       : payload is a JSON string -> raw2obj .data is the parsed
 *                      object.
 *  T7 obj-nonjson    : payload is non-JSON text -> .data falls back to the raw
 *                      string (try/catch path), no throw.
 *  T8 map-act        : map_BPG_Packet2Act on a known type ("RP" w/ data) maps to
 *                      an action; on an unknown type returns gracefully (no
 *                      throw); on undefined returns undefined.
 *
 * The eval bridge JSON-serializes results, so typed arrays come back as plain
 * objects — we read .constructor.name / .length INSIDE the browser and return
 * primitives. All assertions are computed in-browser and returned as flags.
 *
 * CONSTRAINTS: ONE shared browser; the parent runs QA scripts serially. Do NOT
 * run this against a live daemon while sibling agents use it.
 *
 * RUN: node tools/webctl/qa/r2_bpg.mjs   (needs webctld on :8765, app on :8081
 * in dev so __DEV_MODE__ exposes window.__GP_BPG__).
 * ===========================================================================*/

const BASE = `http://127.0.0.1:${process.env.WEBCTL_PORT || 8765}`;

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

let failures = 0;
function pass(name, note) { console.log(`[${name}] PASS${note ? ' — ' + note : ''}`); }
function fail(name, note) { console.log(`[${name}] FAIL${note ? ' — ' + note : ''}`); failures++; }
function skip(name, note) { console.log(`[${name}] SKIP${note ? ' — ' + note : ''}`); }

// Wait for the BPG dev hook after a reload. Core-INDEPENDENT.
async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_BPG__==='object' && typeof window.__GP_BPG__.raw2header==='function'"
    );
    if (ok === true) return true;
  }
  return false;
}

// --- T1: header parse -------------------------------------------------------
async function testHeaderParse() {
  // type "FL" (0x46,0x4C), prop 3, pgID 0x0102=258, length 0x00000005=5,
  // + 5 dummy payload bytes (so length is internally consistent, though
  // raw2header does not read the body).
  const res = await ev(`(function(){
    try{
      var b=[0x46,0x4C, 0x03, 0x01,0x02, 0x00,0x00,0x00,0x05, 1,2,3,4,5];
      var ab=new Uint8Array(b).buffer;
      var h=window.__GP_BPG__.raw2header({data:ab},0);
      if(h==null)return {err:'null'};
      return {type:h.type,prop:h.prop,pgID:h.pgID,length:h.length};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.type === 'FL' && res.prop === 3 && res.pgID === 258 && res.length === 5) {
    pass('T1_header-parse', `type=FL prop=3 pgID=258 length=5`);
  } else {
    fail('T1_header-parse', `mis-parsed: ${JSON.stringify(res)}`);
  }
}

// --- T2: too-short header returns null --------------------------------------
async function testHeaderShort() {
  const res = await ev(`(function(){
    try{
      var ab=new Uint8Array([0x46,0x4C,0x03,0x01]).buffer; // 4 bytes < 9
      var h=window.__GP_BPG__.raw2header({data:ab},0);
      return {isNull: h===null};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.isNull === true) pass('T2_header-short', '<9-byte buffer -> null');
  else fail('T2_header-short', `expected null, got ${JSON.stringify(res)}`);
}

// --- T3: IM oversize claim -> image===null (Wave-1 bounds fix) --------------
async function testIMOversize() {
  // header length field is unreliable for IM (decoder ignores it), so what
  // matters: width/height claim 4*w*h pixels far beyond the actual buffer.
  // 9-byte header + 15-byte IM header present (so it does NOT take the
  // truncated-null path), but NO image bytes follow while w=h=100 -> claims
  // 40000 pixel-bytes. Must return an object with image===null, not throw.
  const res = await ev(`(function(){
    try{
      var hdr=new Array(9).fill(0);
      hdr[0]=0x49;hdr[1]=0x4D; // "IM"
      var im=new Array(15).fill(0);
      im[6]=0;im[7]=100;  // width=100
      im[8]=0;im[9]=100;  // height=100  -> 4*100*100=40000 pixel bytes claimed
      var bytes=hdr.concat(im);            // total 24 bytes, no image region
      var ab=new Uint8Array(bytes).buffer;
      var o=window.__GP_BPG__.raw2Obj_IM({data:ab},0);
      return {isObj: o!==null && typeof o==='object',
              imageNull: o!==null && o.image===null,
              width:o&&o.width, height:o&&o.height};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.isObj && res.imageNull) {
    pass('T3_im-oversize', `w=${res.width} h=${res.height} -> object, image=null (no throw)`);
  } else {
    fail('T3_im-oversize', `expected object w/ image=null, got ${JSON.stringify(res)}`);
  }
}

// --- T4: IM truncated (< 9+15) -> null --------------------------------------
async function testIMTruncated() {
  const res = await ev(`(function(){
    try{
      var hdr=new Array(9).fill(0); hdr[0]=0x49;hdr[1]=0x4D; // "IM"
      var bytes=hdr.concat(new Array(5).fill(0)); // 14 bytes < 9+15=24
      var ab=new Uint8Array(bytes).buffer;
      var o=window.__GP_BPG__.raw2Obj_IM({data:ab},0);
      return {isNull: o===null};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.isNull === true) pass('T4_im-truncated', '<9+15-byte buffer -> null');
  else fail('T4_im-truncated', `expected null, got ${JSON.stringify(res)}`);
}

// --- T5: valid tiny IM -> image is Uint8ClampedArray of length 4*w*h --------
async function testIMValid() {
  // w=2,h=2 -> 4*2*2=16 image bytes; supply exactly 16 trailing bytes.
  const res = await ev(`(function(){
    try{
      var hdr=new Array(9).fill(0); hdr[0]=0x49;hdr[1]=0x4D; // "IM"
      var im=new Array(15).fill(0);
      im[6]=0;im[7]=2; // width=2
      im[8]=0;im[9]=2; // height=2 -> 16 pixel bytes
      var img=new Array(16); for(var i=0;i<16;i++)img[i]=i;
      var bytes=hdr.concat(im).concat(img); // 9+15+16=40 bytes
      var ab=new Uint8Array(bytes).buffer;
      var o=window.__GP_BPG__.raw2Obj_IM({data:ab},0);
      if(o===null)return {err:'null'};
      return {
        ctor: o.image && o.image.constructor && o.image.constructor.name,
        len: o.image && o.image.length,
        first: o.image && o.image[0], last: o.image && o.image[15],
        width:o.width, height:o.height
      };
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.ctor === 'Uint8ClampedArray' && res.len === 16 &&
      res.first === 0 && res.last === 15 && res.width === 2 && res.height === 2) {
    pass('T5_im-valid', `image Uint8ClampedArray len=16 (4*2*2), bytes intact`);
  } else {
    fail('T5_im-valid', `expected Uint8ClampedArray len16, got ${JSON.stringify(res)}`);
  }
}

// --- T6: raw2obj JSON body parses -------------------------------------------
async function testObjJson() {
  const res = await ev(`(function(){
    try{
      var payload='{"version":"1.2.3","ok":true}';
      var body=[]; for(var i=0;i<payload.length;i++)body.push(payload.charCodeAt(i));
      var hdr=new Array(9).fill(0);
      hdr[0]=0x48;hdr[1]=0x52; // "HR"
      // length BE = body length
      var L=body.length;
      hdr[5]=(L>>>24)&255;hdr[6]=(L>>>16)&255;hdr[7]=(L>>>8)&255;hdr[8]=L&255;
      var ab=new Uint8Array(hdr.concat(body)).buffer;
      var o=window.__GP_BPG__.raw2obj({data:ab},0);
      return {dataType: typeof o.data,
              version: o.data && o.data.version,
              ok: o.data && o.data.ok};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.dataType === 'object' && res.version === '1.2.3' && res.ok === true) {
    pass('T6_obj-json', `.data parsed -> {version:"1.2.3",ok:true}`);
  } else {
    fail('T6_obj-json', `expected parsed object, got ${JSON.stringify(res)}`);
  }
}

// --- T7: raw2obj non-JSON body falls back to raw string ---------------------
async function testObjNonJson() {
  const res = await ev(`(function(){
    try{
      var payload='not-json-just-text';
      var body=[]; for(var i=0;i<payload.length;i++)body.push(payload.charCodeAt(i));
      var hdr=new Array(9).fill(0);
      hdr[0]=0x53;hdr[1]=0x53; // "SS"
      var L=body.length;
      hdr[5]=(L>>>24)&255;hdr[6]=(L>>>16)&255;hdr[7]=(L>>>8)&255;hdr[8]=L&255;
      var ab=new Uint8Array(hdr.concat(body)).buffer;
      var o=window.__GP_BPG__.raw2obj({data:ab},0);
      return {dataType: typeof o.data, data: o.data};
    }catch(e){return {err:String(e)};}
  })()`);
  if (res && res.dataType === 'string' && res.data === 'not-json-just-text') {
    pass('T7_obj-nonjson', `.data fell back to raw string (try/catch path)`);
  } else {
    fail('T7_obj-nonjson', `expected raw string fallback, got ${JSON.stringify(res)}`);
  }
}

// --- T8: map_BPG_Packet2Act known/unknown/undefined -------------------------
async function testMapAct() {
  if ((await ev("typeof window.__GP_BPG__.map_BPG_Packet2Act")) !== 'function') {
    skip('T8_map-act', 'COVERAGE GAP: __GP_BPG__.map_BPG_Packet2Act not exposed');
    return;
  }
  const res = await ev(`(function(){
    var m=window.__GP_BPG__.map_BPG_Packet2Act;
    var out={};
    // known: "RP" (report) -> EV_WS_Inspection_Report action (object).
    try{
      var a=m({type:'RP', data:{reports:[]}});
      out.knownIsAct = a!==undefined && a!==null && typeof a==='object';
    }catch(e){out.knownErr=String(e);}
    // unknown type -> graceful (default branch builds nothing) -> undefined, no throw.
    try{
      var u=m({type:'ZZ', data:{}});
      out.unknownOk = (u===undefined);
    }catch(e){out.unknownErr=String(e);}
    // undefined packet -> returns undefined (explicit guard), no throw.
    try{
      out.undefOk = (m(undefined)===undefined);
    }catch(e){out.undefErr=String(e);}
    return out;
  })()`);
  const ok = res && res.knownIsAct && res.unknownOk && res.undefOk &&
             !res.knownErr && !res.unknownErr && !res.undefErr;
  if (ok) {
    pass('T8_map-act', 'RP->action; unknown->undefined; undefined->undefined (no throw)');
  } else {
    fail('T8_map-act', `unexpected: ${JSON.stringify(res)}`);
  }
}

async function main() {
  const ready = await waitReady();
  if (!ready) {
    // Hook absent = the dev surface we are charged with testing is unavailable.
    // This is core-INDEPENDENT, so absence is a real coverage failure, not a
    // skip — but distinguish app-not-up from hook-missing for the operator.
    const appUp = await ev("typeof window").catch(() => null);
    if (appUp !== 'string') {
      fail('R2_setup', 'app/daemon not reachable on :8081/:8765 (cannot eval)');
    } else {
      fail('R2_setup', '__GP_BPG__ dev hook never appeared — is app in __DEV_MODE__?');
    }
    console.log(`\n${failures} test(s) FAILED.`);
    process.exit(1);
  }

  await testHeaderParse();
  await testHeaderShort();
  await testIMOversize();
  await testIMTruncated();
  await testIMValid();
  await testObjJson();
  await testObjNonJson();
  await testMapAct();

  if (failures) { console.log(`\n${failures} test(s) FAILED.`); process.exit(1); }
  console.log('\nAll BPG protocol tests passed.');
  process.exit(0);
}

main().catch((e) => { console.error('UNEXPECTED ERROR:', e); process.exit(1); });
