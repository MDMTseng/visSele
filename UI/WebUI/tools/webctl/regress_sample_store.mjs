// FILL AND STOP: the inspection sample buffer's rules.
//
// Two of these would pass just as happily against the behaviour they exist to
// rule out, so they are written as POSITIONS rather than membership:
//
//   - a ring evicts the oldest to make room. Here the oldest is precisely what
//     someone is keeping -- at this machine's rate a 20-deep bucket turns over
//     in about a second, so the part an operator just watched go past would be
//     gone before they could open the panel. The NEW sample is the one refused.
//   - after a delete the entries behind the hole move FORWARD and the freed
//     slot ends up at the tail, which is where the next sample lands. The panel
//     numbers entries by position, so a set comparison would pass even with the
//     order wrong -- and the order is what the operator reads off the screen.
//
// The store is driven through the two calls the reducer makes
// (noteFinalisedReports then attachImage) with synthetic reports and a
// synthetic JPEG. That is deliberate for the rules above, and it is also the
// limit of this file: it does NOT prove the reducer calls in on a real report,
// which needs a live (or fake-camera) FI run.
import { makeCtl, freshPage, sleep } from './lib_enter.mjs';
const ctl = makeCtl('http://127.0.0.1:8765');
const { ev } = ctl;
process.env.WEBCTL_COLD = '1';
await freshPage(ctl, process.argv[2] || 'http://127.0.0.1:8083/');
await sleep(4000);

let fail = 0;
const ck = (w, g, e) => {
  const ok = String(g) === String(e);
  console.log(`  ${ok ? 'ok  ' : 'FAIL'} ${w}: ${g}${ok ? '' : '  (expected ' + e + ')'}`);
  if (!ok) fail++;
};

// ---- the buckets, the pairing rule, and what a frame costs -----------------
const a = JSON.parse(String(await ev(`(function(){
  var S = window.__GP_SAMPLE_STORE__;
  if (!S) return JSON.stringify({err:'NO_STORE'});
  S.clearSampleStore(); S.setSampleStoreCap(3);
  var jr=function(st){return [{id:1,name:'w',value:1.23,status:st}];};
  var frame=function(n){return {jpegBytes:new Uint8Array(n),format:2,width:816,
    height:528,scale:3,full_width:2448,full_height:2048};};
  var res={};

  // INSPECTION_STATUS is NUMERIC (NA -128, UNSET -100, SUCCESS 0, FAILURE -1).
  // Against strings every part would land in NA -- the bucket meaning "we could
  // not tell", on a screen built to answer exactly that.
  S.noteFinalisedReports([{time_ms:1,judgeReports:jr(0)},
                          {time_ms:2,judgeReports:jr(-1)},
                          {time_ms:3,judgeReports:jr(-128)}],'FI');
  S.attachImage(frame(1000),{ppb2b:1,mmpb2b:0.0138},'D');
  var s=S.sampleStoreSnapshot();
  res.buckets=s.OK.length+'/'+s.NG.length+'/'+s.NA.length;
  // Three parts share ONE frame object; counting per entry would treble it, and
  // that number is what the memory ceiling admits against. Report bytes ARE per
  // entry (each part has its own geometry), so the frame's share is what is left
  // after subtracting them -- asserting the total would just pin whatever the
  // fixture's reports happen to serialise to.
  res.bytes=s.bytes;
  var repBytes=0;
  ['OK','NG','NA'].forEach(function(b){ s[b].forEach(function(e){ repBytes+=e.bytesReport||0; }); });
  res.frameBytes=s.bytes-repBytes;
  res.perEntryReportBytes=repBytes>0;

  // CI is refused outright: its verdict is settled a second after the frames
  // that made it, so the current image is a later part.
  S.noteFinalisedReports([{time_ms:9,judgeReports:jr(-1)}],'CI');
  S.attachImage(frame(10),{},'D');
  res.afterCI=S.sampleStoreSnapshot().NG.length;

  // a verdict with no frame behind it earns no slot
  S.noteFinalisedReports([{time_ms:8,judgeReports:jr(0)}],'FI');
  S.attachImage({jpegBytes:new Uint8Array(0)},{},'D');
  res.afterUnpaired=S.sampleStoreSnapshot().OK.length;

  // THE OVERLAY'S TWO PRECONDITIONS.
  //
  // The geometry has to survive into the entry (the panel draws searchPoints
  // and detectedLines over the frame), and isCurObj has to be stamped true --
  // the playback canvas draws trackingWindow.filter(x => x.isCurObj), and a
  // FINALISED report has it false because it means "matched in the frame being
  // processed". Without the stamp the overlay renders nothing at all and looks
  // exactly like the geometry having been dropped.
  S.clearSampleStore();
  S.noteFinalisedReports([{time_ms:7,isCurObj:false,judgeReports:jr(-1),
    detectedLines:[{id:1,x:0,y:0}],searchPoints:[{id:2,x:1,y:1}]}],'FI');
  S.attachImage(frame(10),{},'D');
  var g=S.sampleStoreSnapshot().NG[0];
  res.keptGeometry=(g&&g.report&&g.report.searchPoints)?g.report.searchPoints.length:0;
  res.isCurObj=g&&g.report?g.report.isCurObj:undefined;
  // and the stored copy must be DETACHED from the live report
  res.detached=(g&&g.report)!==undefined;
  return JSON.stringify(res);
})()`)));
if (a.err) { console.log('FAIL: ' + a.err); process.exit(1); }
console.log('buckets and pairing');
ck('OK/NG/NA bucketing', a.buckets, '1/1/1');
ck('one frame counted once for three parts', a.frameBytes, 1000);
ck('each entry carries its own report bytes', a.perEntryReportBytes, true);
ck('CI is not sampled', a.afterCI, 1);
ck('an unpaired verdict is not kept', a.afterUnpaired, 1);
ck('the geometry the overlay draws is kept', a.keptGeometry, 1);
ck('isCurObj stamped true or the overlay draws nothing', a.isCurObj, true);

// ---- fill and stop, delete, shift up, refill at the tail -------------------
const b = JSON.parse(String(await ev(`(function(){
  var S = window.__GP_SAMPLE_STORE__;
  S.clearSampleStore(); S.setSampleStoreCap(4);
  var jr=[{id:1,name:'w',value:1,status:-1}];
  var feed=function(t){S.noteFinalisedReports([{time_ms:t,judgeReports:jr}],'FI');
    S.attachImage({jpegBytes:new Uint8Array(10),format:2,width:816,height:528,
                   full_width:2448,full_height:2048},{},'D');};
  var pos=function(){return S.sampleStoreSnapshot().NG
      .map(function(e,i){return (i+1)+':'+e.time_ms;}).join(' ');};
  var res={};
  [10,20,30,40].forEach(feed);
  res.filled=pos(); res.fullBefore=S.sampleStoreSnapshot().full.NG;

  feed(50);
  res.afterRefused=pos(); res.skipped=S.sampleStoreSnapshot().skipped.NG;

  // lowering the cap must not delete what someone deliberately kept
  S.setSampleStoreCap(2);
  res.afterLowerCap=S.sampleStoreSnapshot().NG.length;
  S.setSampleStoreCap(4);

  var id2=S.sampleStoreSnapshot().NG[1].id;
  res.removed=S.removeSampleEntry(id2);
  res.removeMissing=S.removeSampleEntry(999999);
  res.afterDelete=pos(); res.fullAfterDelete=S.sampleStoreSnapshot().full.NG;

  feed(60);
  res.afterRefill=pos(); res.fullAgain=S.sampleStoreSnapshot().full.NG;

  S.clearSampleBucket('NG');
  var s=S.sampleStoreSnapshot();
  res.afterBucketClear=s.OK.length+'/'+s.NG.length+'/'+s.NA.length;
  S.clearSampleStore();
  res.afterClearAll=S.sampleStoreSnapshot().NG.length;
  return JSON.stringify(res);
})()`)));
console.log('fill and stop');
ck('filled in capture order', b.filled, '1:10 2:20 3:30 4:40');
ck('full at the cap', b.fullBefore, true);
ck('a new sample while full changes NOTHING', b.afterRefused, '1:10 2:20 3:30 4:40');
ck('and is counted as turned away', b.skipped, 1);
ck('a lowered cap keeps what is held', b.afterLowerCap, 4);
ck('remove reports success', b.removed, true);
ck('removing a missing id reports false', b.removeMissing, false);
ck('deleting #2 shifts the rest UP', b.afterDelete, '1:10 2:30 3:40');
ck('and it is no longer full', b.fullAfterDelete, false);
ck('the next sample fills the TAIL', b.afterRefill, '1:10 2:30 3:40 4:60');
ck('full again', b.fullAgain, true);
ck('clearing one bucket leaves the others', b.afterBucketClear, '0/0/0');
ck('clear all empties it', b.afterClearAll, 0);

console.log(fail ? `\n${fail} FAILED` : '\nPASS');
process.exit(fail ? 1 : 0);
