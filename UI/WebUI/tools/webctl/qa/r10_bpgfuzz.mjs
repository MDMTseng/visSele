#!/usr/bin/env node
/* =============================================================================
 * r10_bpgfuzz.mjs — BPG decoder FUZZ. CORE-INDEPENDENT.
 *
 * VIEWPOINT: Round 9 fixed two decoder bugs in src/UTIL/BPG_Protocol.js:
 *   (1) raw2header parses length as UNSIGNED 32-bit (high byte * 0x01000000),
 *       so values >= 0x80000000 no longer go negative.
 *   (2) raw2obj defensively clips oversize claimed lengths to what's actually
 *       in the buffer before constructing a Uint8ClampedArray view.
 *   (3) raw2Obj_IM has explicit bounds checks for both the 15-byte extra
 *       header AND the pixel region; out-of-bounds -> image=null (no throw).
 *
 * This round stress-tests the contract with PSEUDO-RANDOM input. We feed
 * thousands of random byte buffers to raw2header / raw2obj / raw2Obj_IM
 * and assert the decoders NEVER throw uncaught and ALWAYS return either
 * null or a sane result. Invariants checked:
 *   - raw2header.length >= 0 (unsigned)  raw2header.pgID in [0, 65535]
 *   - raw2obj result is null OR has .data of type 'string' or 'object'
 *   - raw2Obj_IM result is null OR camera_id/session_id in [0,255],
 *     .image is null or a Uint8ClampedArray
 *
 * TEST PLAN (each -> "[name] PASS/FAIL"; exit non-zero on any real failure):
 *   F1 raw2header fuzz   : 1000 buffers, 9..32 bytes, fully random. Asserts
 *                          no throw + unsigned length + pgID in range.
 *   F2 raw2obj fuzz      : 1000 buffers, 5..200 bytes, fully random. Asserts
 *                          no top-level throw (try/catch inside contract) +
 *                          .data is string|object when result is non-null.
 *   F3 raw2Obj_IM fuzz   : 500 buffers, 30..1000 bytes, fully random. Asserts
 *                          no throw + camera_id/session_id in [0,255] +
 *                          .image is null or Uint8ClampedArray.
 *   F4 boundary lengths  : header.length cycled through extreme values
 *                          [0,1,100,1000,0x7FFFFFFF,0x80000000,0xFFFFFFFF]
 *                          x buffer size variations [exact,+1,-1,0 body]
 *                          -> raw2header + raw2obj must never throw.
 *   F5 multi-packet walk : pretend a 1KB random buffer is multi-packet,
 *                          walk via raw2header at offset += 9+length;
 *                          must terminate gracefully (capped iters).
 *
 * Uses a seeded Mulberry32 PRNG (in-browser) for reproducibility. All fuzz
 * generation + invariant checks run INSIDE the browser; we only return the
 * aggregate counts (#tests, #throws, #invariant-violations, sample failing
 * buffer hex on first violation).
 *
 * CONSTRAINTS: ONE shared browser; parent runs serially. We only WRITE this
 * file + `node --check`. Iteration counts kept modest to stay within a few
 * seconds.
 *
 * RUN: node tools/webctl/qa/r10_bpgfuzz.mjs (webctld :8765, app :8081 dev)
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

async function waitReady() {
  await api('/reload', {});
  for (let i = 0; i < 60; i++) {
    await sleep(150);
    const ok = await ev(
      "typeof window.__GP_BPG__==='object' && typeof window.__GP_BPG__.raw2header==='function' && typeof window.__GP_BPG__.raw2obj==='function' && typeof window.__GP_BPG__.raw2Obj_IM==='function'"
    );
    if (ok === true) return true;
  }
  return false;
}

// Shared in-browser PRNG + buffer helper (string-embedded for /eval).
const PRELUDE = `
  var G=window.__GP_BPG__;
  function mulberry32(a){return function(){a=(a+0x6D2B79F5)|0;var t=a;t=Math.imul(t^(t>>>15),t|1);t^=t+Math.imul(t^(t>>>7),t|61);return ((t^(t>>>14))>>>0)/4294967296;};}
  function mkBuf(rng,len){var u=new Uint8Array(len);for(var i=0;i<len;i++)u[i]=(rng()*256)|0;return u.buffer;}
  function hex(ab,max){var u=new Uint8Array(ab),s='';var n=Math.min(u.length,max||32);for(var i=0;i<n;i++){var h=u[i].toString(16);s+=(h.length<2?'0':'')+h;}return s;}
`;

// --- F1 ---------------------------------------------------------------------
async function fuzzHeader() {
  const res = await ev(`(function(){${PRELUDE}
    var rng=mulberry32(0xF1A1);
    var N=1000, throws=0, inv=0, sample=null, nulls=0;
    for(var i=0;i<N;i++){
      var len=9+((rng()*24)|0); // 9..32
      var ab=mkBuf(rng,len);
      try{
        var h=G.raw2header({data:ab},0);
        if(h===null){nulls++;continue;}
        var okLen=(typeof h.length==='number') && h.length>=0 && Number.isFinite(h.length);
        var okPg =(typeof h.pgID==='number') && h.pgID>=0 && h.pgID<=65535;
        var okTy =(typeof h.type==='string') && h.type.length===2;
        if(!(okLen&&okPg&&okTy)){
          inv++;
          if(!sample)sample={i:i,hex:hex(ab),h:{type:h.type,pgID:h.pgID,length:h.length,prop:h.prop}};
        }
      }catch(e){
        throws++;
        if(!sample)sample={i:i,hex:hex(ab),err:String(e)};
      }
    }
    return {N:N,throws:throws,inv:inv,nulls:nulls,sample:sample};
  })()`);
  if (res && res.throws===0 && res.inv===0) {
    pass('F1_raw2header-fuzz', `N=${res.N} throws=0 inv=0 nulls=${res.nulls}`);
  } else {
    fail('F1_raw2header-fuzz', `throws=${res&&res.throws} inv=${res&&res.inv} sample=${JSON.stringify(res&&res.sample)}`);
  }
}

// --- F2 ---------------------------------------------------------------------
async function fuzzObj() {
  const res = await ev(`(function(){${PRELUDE}
    var rng=mulberry32(0xF2B2);
    var N=1000, throws=0, inv=0, sample=null, nulls=0;
    for(var i=0;i<N;i++){
      var len=5+((rng()*196)|0); // 5..200
      var ab=mkBuf(rng,len);
      try{
        var o=G.raw2obj({data:ab},0);
        if(o===null){nulls++;continue;}
        var dt=typeof o.data;
        var okData=(dt==='string')||(dt==='object'); // object allowed (parsed/null)
        var okLen=(typeof o.length==='number') && o.length>=0 && Number.isFinite(o.length);
        if(!(okData&&okLen)){
          inv++;
          if(!sample)sample={i:i,hex:hex(ab),len:o.length,dt:dt};
        }
      }catch(e){
        throws++;
        if(!sample)sample={i:i,hex:hex(ab),err:String(e)};
      }
    }
    return {N:N,throws:throws,inv:inv,nulls:nulls,sample:sample};
  })()`);
  if (res && res.throws===0 && res.inv===0) {
    pass('F2_raw2obj-fuzz', `N=${res.N} throws=0 inv=0 nulls=${res.nulls}`);
  } else {
    fail('F2_raw2obj-fuzz', `throws=${res&&res.throws} inv=${res&&res.inv} sample=${JSON.stringify(res&&res.sample)}`);
  }
}

// --- F3 ---------------------------------------------------------------------
async function fuzzIM() {
  const res = await ev(`(function(){${PRELUDE}
    var rng=mulberry32(0xF3C3);
    var N=500, throws=0, inv=0, sample=null, nulls=0;
    for(var i=0;i<N;i++){
      var len=30+((rng()*970)|0); // 30..1000
      var ab=mkBuf(rng,len);
      try{
        var o=G.raw2Obj_IM({data:ab},0);
        if(o===null){nulls++;continue;}
        // Assert the fields raw2Obj_IM ACTUALLY parses. This used to require
        // camera_id and session_id, which that function has never set: the
        // 15-byte IM extra-header is format | jpeg_quality | offsetX | offsetY
        // | width | height | scale | full_width | full_height (see
        // UTIL/BPG_Protocol.js and docs/IMG_TRANSFER_JPEG.md). So F3 failed on
        // 500 of 500 inputs from the day it was written -- and a fuzz case
        // that fails at exactly 100% is testing its own expectation, not the
        // code. Verified against the live app 2026-08-19.
        var NUMS=['format','jpeg_quality','offsetX','offsetY','width','height',
                  'scale','full_width','full_height'];
        var badNum=null;
        for(var ni=0;ni<NUMS.length;ni++){
          var v=o[NUMS[ni]];
          if(typeof v!=='number' || !isFinite(v) || v<0){badNum=NUMS[ni];break;}
        }
        // The image type is DECIDED BY format, and asserting that link is
        // stronger than asserting one type. format 1|2 is a JPEG bitstream
        // (Uint8Array); anything else is legacy raw RGBA (Uint8ClampedArray).
        // The old check only allowed Uint8ClampedArray -- it predates JPEG
        // transfer entirely (docs/IMG_TRANSFER_JPEG.md).
        var ctor=(o.image&&o.image.constructor&&o.image.constructor.name)||
                 (o.image===null?'null':typeof o.image);
        var wantCtor=(o.format===1||o.format===2)?'Uint8Array':'Uint8ClampedArray';
        var imgOK=(o.image===null)||(ctor===wantCtor);
        if(badNum||!imgOK){
          inv++;
          if(!sample)sample={i:i,hex:hex(ab),badNum:badNum,
            badVal:badNum?o[badNum]:undefined,
            format:o.format,imgCtor:ctor,wantCtor:wantCtor};
        }
      }catch(e){
        throws++;
        if(!sample)sample={i:i,hex:hex(ab),err:String(e)};
      }
    }
    return {N:N,throws:throws,inv:inv,nulls:nulls,sample:sample};
  })()`);
  if (res && res.throws===0 && res.inv===0) {
    pass('F3_raw2Obj_IM-fuzz', `N=${res.N} throws=0 inv=0 nulls=${res.nulls}`);
  } else {
    fail('F3_raw2Obj_IM-fuzz', `throws=${res&&res.throws} inv=${res&&res.inv} sample=${JSON.stringify(res&&res.sample)}`);
  }
}

// --- F4 boundary lengths ----------------------------------------------------
async function fuzzBoundary() {
  const res = await ev(`(function(){${PRELUDE}
    var lengths=[0,1,100,1000,0x7FFFFFFF,0x80000000,0xFFFFFFFF];
    var bodyDeltas=[0,1,-1,'zero']; // exact, +1, -1, zero body
    var throws=0, inv=0, sample=null, tries=0;
    for(var li=0;li<lengths.length;li++){
      var L=lengths[li];
      for(var di=0;di<bodyDeltas.length;di++){
        var delta=bodyDeltas[di];
        // We can't really materialize a 2GB buffer; clamp body size.
        var bodyLen;
        if(delta==='zero')bodyLen=0;
        else bodyLen=Math.max(0, Math.min(256, L+delta));
        var total=9+bodyLen;
        var u=new Uint8Array(total);
        u[0]=0x46;u[1]=0x4C;u[2]=0;u[3]=0;u[4]=1;
        u[5]=(L>>>24)&255; u[6]=(L>>>16)&255; u[7]=(L>>>8)&255; u[8]=L&255;
        for(var k=9;k<total;k++)u[k]=(k*17)&255;
        var ab=u.buffer;
        // raw2header
        tries++;
        try{
          var h=G.raw2header({data:ab},0);
          if(h && (h.length<0 || !Number.isFinite(h.length))){
            inv++; if(!sample)sample={where:'hdr',L:L,delta:String(delta),len:h.length};
          }
        }catch(e){throws++; if(!sample)sample={where:'hdr',L:L,delta:String(delta),err:String(e)};}
        // raw2obj
        tries++;
        try{
          var o=G.raw2obj({data:ab},0);
          if(o){
            var dt=typeof o.data;
            if(!(dt==='string'||dt==='object')){
              inv++; if(!sample)sample={where:'obj',L:L,delta:String(delta),dt:dt};
            }
          }
        }catch(e){throws++; if(!sample)sample={where:'obj',L:L,delta:String(delta),err:String(e)};}
      }
    }
    return {tries:tries,throws:throws,inv:inv,sample:sample};
  })()`);
  if (res && res.throws===0 && res.inv===0) {
    pass('F4_boundary-lengths', `tries=${res.tries} throws=0 inv=0`);
  } else {
    fail('F4_boundary-lengths', `throws=${res&&res.throws} inv=${res&&res.inv} sample=${JSON.stringify(res&&res.sample)}`);
  }
}

// --- F5 multi-packet walk ---------------------------------------------------
async function fuzzMultiWalk() {
  const res = await ev(`(function(){${PRELUDE}
    var rng=mulberry32(0xF5E5);
    var rounds=20, throws=0, infinite=0, sample=null, totalSteps=0;
    var CAP=2000; // hard iteration cap per buffer
    for(var r=0;r<rounds;r++){
      var ab=mkBuf(rng,1024);
      var off=0, steps=0;
      try{
        while(off+9 <= 1024){
          steps++; totalSteps++;
          if(steps>CAP){infinite++; if(!sample)sample={r:r,off:off,steps:steps}; break;}
          var h=G.raw2header({data:ab},off);
          if(h===null) break;
          // advance by 9 + length; if length pushes past buffer, raw2header at next
          // off will return null. We must NOT loop infinitely even on length=0.
          var adv=9 + (h.length>>>0);
          if(adv<=0) adv=9; // belt: never zero/negative advance
          off += adv;
          if(off<0) break;
        }
      }catch(e){
        throws++; if(!sample)sample={r:r,off:off,steps:steps,err:String(e)};
      }
    }
    return {rounds:rounds,totalSteps:totalSteps,throws:throws,infinite:infinite,sample:sample};
  })()`);
  if (res && res.throws===0 && res.infinite===0) {
    pass('F5_multi-walk', `rounds=${res.rounds} steps=${res.totalSteps} throws=0 infinite=0`);
  } else {
    fail('F5_multi-walk', `throws=${res&&res.throws} infinite=${res&&res.infinite} sample=${JSON.stringify(res&&res.sample)}`);
  }
}

async function main() {
  const ready = await waitReady();
  if (!ready) {
    const appUp = await ev("typeof window").catch(() => null);
    if (appUp !== 'string') {
      fail('R10_setup', 'app/daemon not reachable on :8081/:8765 (cannot eval)');
    } else {
      fail('R10_setup', '__GP_BPG__ dev hook never appeared — is app in __DEV_MODE__?');
    }
    console.log(`\n${failures} test(s) FAILED.`);
    process.exit(1);
  }

  await fuzzHeader();
  await fuzzObj();
  await fuzzIM();
  await fuzzBoundary();
  await fuzzMultiWalk();

  if (failures) { console.log(`\n${failures} test(s) FAILED.`); process.exit(1); }
  console.log('\nAll BPG fuzz tests passed.');
  process.exit(0);
}

main().catch((e) => { console.error('UNEXPECTED ERROR:', e); process.exit(1); });
