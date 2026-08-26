
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';

import {PostfixExpCalc} from 'UTIL/MISC_Util';


import { mkLog } from 'UTIL/logger';
const log = mkLog('comm.bpg');


const BPG_header_L = 9;
let raw2header=(ws_evt, offset = 0)=>{
  if (( ws_evt.data instanceof ArrayBuffer) && ws_evt.data.byteLength>=BPG_header_L) {
    // var aDataArray = new Float64Array(evt.data);
    // var aDataArray = new Uint8Array(evt.data);
    var headerArray = new Uint8ClampedArray(
      ws_evt.data,offset,BPG_header_L);
    let ret_obj={};


    ret_obj.type = String.fromCharCode(headerArray[0],headerArray[1]);
    ret_obj.prop = headerArray[2];
    ret_obj.pgID = (headerArray[3]<<8)+headerArray[4];
    //console.log(ret_obj.pgID);

    // Parse length as UNSIGNED 32-bit big-endian. Using bitwise `<<24` would
    // treat the high bit as a sign, so any header with length >= 0x80000000
    // would decode to a negative number and crash downstream views. Use
    // multiplication for the high byte to avoid the sign issue.
    ret_obj.length =
      headerArray[5] * 0x01000000 +
      (headerArray[6] << 16) +
      (headerArray[7] << 8) +
      headerArray[8];
    return ret_obj;
  }
  return null;
};
let raw2obj_rawdata=(ws_evt, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;

  ret_obj.rawdata = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,ret_obj.length
  );

  return ret_obj;
};
let raw2obj=(ws_evt, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;

  // Defensive bounds check: a corrupt header (e.g. an oversize claimed length)
  // would otherwise crash `new Uint8ClampedArray(buf, offset, length)` with a
  // RangeError. Clip to what's actually in the buffer and treat the result as
  // a (possibly truncated) raw string.
  let avail = ws_evt.data.byteLength - (offset + BPG_header_L);
  let safeLen = (ret_obj.length >= 0 && ret_obj.length <= avail) ? ret_obj.length
              : Math.max(0, avail);
  if (safeLen !== ret_obj.length) {
    log.warn("raw2obj: header length out of bounds; clipping",
      { claimed: ret_obj.length, byteLength: ws_evt.data.byteLength, offset, used: safeLen });
  }
  ret_obj.rawdata = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L, safeLen
  );
  let  enc = new TextDecoder("utf-8");
  let str = enc.decode(ret_obj.rawdata);
  //console.log(str);
  try{
    ret_obj.data = JSON.parse(str);
  }
  catch(e)
  {
    ret_obj.data=str;
  }
  return ret_obj;
};
let raw2Obj_IM=(ws_evt, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;

  let headerL=15;

  // Defensive bounds check: the 15-byte IM extra-header must fit inside the
  // backing buffer before we build a view over it (a corrupt/truncated frame
  // would otherwise RangeError). NOTE: do NOT validate against ret_obj.length
  // (the BPG header length field) -- for IM packets that field is unreliable
  // (which is why the size-equality check below stays disabled), so trusting
  // it here would drop valid frames.
  if (offset + BPG_header_L + headerL > ws_evt.data.byteLength) {
    log.warn("raw2Obj_IM: extra-header out of bounds; dropping frame",
      { offset, byteLength: ws_evt.data.byteLength });
    return null;
  }

  let headerArray = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,headerL);
  // Core a7cd253d / docs/IMG_TRANSFER_JPEG.md: byte[0] is the format ID
  //   0 = raw RGBA (legacy)
  //   1 = 3-channel BGR JPEG
  //   2 = 1-channel grayscale JPEG (auto-selected when the source Mat is
  //       gray or BGR with B==G==R for every probed pixel)
  // byte[1] is JPEG quality (diagnostic; 0 for raw). Both JPEG variants are
  // decoded the same way (createImageBitmap handles 1- and 3-channel JPEG).
  ret_obj.format=headerArray[0];
  ret_obj.jpeg_quality=headerArray[1];
  ret_obj.offsetX=(headerArray[2]<<8)|headerArray[3];
  ret_obj.offsetY=(headerArray[4]<<8)|headerArray[5];
  ret_obj.width=(headerArray[6]<<8)|headerArray[7];
  ret_obj.height=(headerArray[8]<<8)|headerArray[9];


  ret_obj.scale=headerArray[10];
  ret_obj.full_width=(headerArray[11]<<8)|headerArray[12];
  ret_obj.full_height=(headerArray[13]<<8)|headerArray[14];

  let imgStart = offset + BPG_header_L + headerL;
  let payloadAvail = ws_evt.data.byteLength - imgStart;
  // TEMP probe: log frame meta so we can correlate corruption with format /
  // size drift. Remove once diagnosed.
  if (!window.__IM_PROBE_N) window.__IM_PROBE_N = 0;
  if (window.__IM_PROBE_N < 20) {
    window.__IM_PROBE_N++;
    console.log("IM#"+window.__IM_PROBE_N,
      "format="+ret_obj.format,
      "q="+ret_obj.jpeg_quality,
      "w="+ret_obj.width, "h="+ret_obj.height,
      "full="+ret_obj.full_width+"x"+ret_obj.full_height,
      "scale="+ret_obj.scale, "off=("+ret_obj.offsetX+","+ret_obj.offsetY+")",
      "payload="+payloadAvail);
  }

  if (ret_obj.format === 1 || ret_obj.format === 2) {
    // JPEG (1=BGR, 2=grayscale): entire remainder is the JPEG bitstream.
    // Copy out so the typed-array view stays valid after WS recycles the
    // ArrayBuffer (Blob ctor below holds a reference to whatever we hand it).
    if (payloadAvail > 0) {
      ret_obj.image=new Uint8Array(ws_evt.data, imgStart, payloadAvail);
    } else {
      log.warn("raw2Obj_IM: JPEG payload empty; image=null",
        { width: ret_obj.width, height: ret_obj.height, byteLength: ws_evt.data.byteLength });
      ret_obj.image=null;
    }
  } else {
    // Legacy raw RGBA: 4 bytes per pixel.
    let RGBA_pix_Num = 4*ret_obj.width*ret_obj.height;
    if (RGBA_pix_Num >= 0 && imgStart + RGBA_pix_Num <= ws_evt.data.byteLength)
    {
      ret_obj.image=new Uint8ClampedArray(ws_evt.data,
        imgStart,RGBA_pix_Num);
    }
    else
    {
      log.warn("raw2Obj_IM: image pixel region out of bounds; image=null",
        { width: ret_obj.width, height: ret_obj.height, RGBA_pix_Num,
          imgStart, byteLength: ws_evt.data.byteLength });
      ret_obj.image=null;
    }
  }


  return ret_obj;
};

var enc = new TextEncoder();
let objbarr2raw=(type,prop,pgID,obj,barr=null)=>{
  
  let str = (obj==null)?"":JSON.stringify(obj);

  let encStr= enc.encode(str);
  let objstr_L = encStr.length;
  
  let barr_L = (barr instanceof Uint8Array)?barr.length:0;
  let bbuf = new Uint8Array(BPG_header_L+objstr_L+1+barr_L);

  bbuf[0] = type.charCodeAt(0);
  bbuf[1] = type.charCodeAt(1);
  bbuf[2] = prop;
  bbuf[3] = pgID>>8;
  bbuf[4] = pgID&255;

  let data_length =bbuf.length - BPG_header_L;//Add NULL in the end of the string
  bbuf[5] = data_length>>24;
  bbuf[6] = data_length>>16;
  bbuf[7] = data_length>>8;
  bbuf[8] = data_length;
  if(objstr_L!=0)
  {
    bbuf.set(encStr, BPG_header_L);
  }
  bbuf[BPG_header_L+objstr_L]=0;//The end of string session
  if(barr_L!=0)
  {
    bbuf.set(barr, BPG_header_L+objstr_L+1);
  }
  return bbuf;
};



// R-quick-wins: moved to UTIL/InspectionStatus.js to break the BPG_Protocol <-> UIAct
// circular import. Imported locally so the default export below still bundles it,
// and re-exported as a named export for backward compatibility with consumers
// that still import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol'.
import { INSPECTION_STATUS } from './InspectionStatus';
export { INSPECTION_STATUS };




// Exported BOTH ways on purpose. It has always been reachable only through the
// default export, so `import * as BPG from '...'` compiles clean and hands back
// undefined at runtime -- and the call sites are inside protocol callbacks,
// where the TypeError does not reach anything the operator sees. That cost an
// afternoon: an image switch that loaded in the core (so the inspection moved)
// and never reached the canvas (so the picture did not).
export function map_BPG_Packet2Act(parsed_packet)
{
  let acts=[];
  if(parsed_packet===undefined)
  {
    return undefined;
  }
  switch(parsed_packet.type )
  {
    case "HR":
    {
      /*//log.info(this.props.WS_CH);
      this.props.ACT_WS_SEND_BPG(this.props.CORE_ID,"HR",0,{a:["d"]});
      
      this.props.ACT_WS_SEND_BPG(this.props.CORE_ID,"LD",0,{filename:"data/default_camera_param.json"});
      break;*/
    }

    case "SS":
    {
      break;
    }
    case "IM":
    {
      let pkg = parsed_packet;
      // Counts IMAGE frames specifically, not all traffic: the open question
      // is whether frames arrive faster than paints retire them, and only
      // image frames carry anything worth retiring. See UTIL/diagProbe.js.
      // Bytes AND geometry. The stream resolution is negotiated from the
      // canvas size, and until now nothing downstream could say what the core
      // actually settled on -- a preview one step too coarse and a preview
      // that is correct differ only in how they look, which is exactly the
      // kind of judgement that should not be left to the eye. pkg.scale is
      // the core's down-sample level straight out of the IM header.
      if (typeof window !== 'undefined' && window.__DIAG_WS_TICK__) {
        window.__DIAG_WS_TICK__(pkg.image ? pkg.image.byteLength : 0,
                                pkg.width, pkg.height, pkg.scale);
      }
      // Skip if raw2Obj_IM bailed (bounds/empty payload — image:null).
      if (!pkg.image) break;
      let objx = { ...pkg };
      delete objx.image;
      if (pkg.format === 1 || pkg.format === 2) {
        // JPEG (1=BGR, 2=grayscale): hand the ENCODED BYTES on as a
        // Uint8Array. Copy them — the underlying ArrayBuffer is the WS message
        // buffer, which gets recycled on the next onmessage.
        //
        // NOT A BLOB, and the difference is not cosmetic.
        //
        // A Blob's payload lives in Chromium's blob store, OUTSIDE the JS heap,
        // and V8 has no idea how large it is. Redux keeps only the newest
        // frame, so the previous ones become garbage immediately -- but their
        // JS wrappers are a few dozen bytes each, the heap sits at 29.8 MB, and
        // V8 therefore never feels any reason to collect. Measured 2026-08-23:
        // renderer RSS climbed 10.4 MB/min for ten minutes with the heap
        // FLAT and every array in the store unchanged, and one forced
        // HeapProfiler.collectGarbage handed back 260 MB at once. Nothing was
        // retained; nothing was collected either.
        //
        // An ArrayBuffer's bytes are external memory that V8 DOES account for,
        // so the same garbage now creates proportional collection pressure and
        // is reclaimed on the normal schedule. Same data, same copy, visible to
        // the collector.
        const bytes = new Uint8Array(pkg.image.byteLength);
        bytes.set(pkg.image);
        objx.jpegBytes = bytes;
      } else {
        // Raw RGBA (legacy / format === 0).
        objx.img = new ImageData(pkg.image, pkg.width);
      }
      acts.push(UIAct.EV_WS_Image_Update(objx));

      break;
    }
    case "IR":
    case "RP":
    {
      let report =parsed_packet;
      acts.push(UIAct.EV_WS_Inspection_Report(report.data));
      break;
    }
    case "DF":
    {
      let report =parsed_packet;
      //log.debug(report.type,report);
      
      acts.push(UIAct.EV_WS_Define_File_Update(report.data));
      break;
    }
    case "FL":
    {
      let report =parsed_packet;
      //log.error(report.type,report);
      if(report.data.type === "binary_processing_group" )
      {
        
        acts.push(UIAct.EV_WS_Inspection_Report(report.data));
      }
      break;
    }
    case "SG":
    {
      let report =parsed_packet;
      //log.debug(report.type,report);
      acts.push(UIAct.EV_WS_SIG360_Report_Update(report.data));
      break;
    }
    default:
    {
      let report =parsed_packet;
      let act = (report);
    }



  }
  return acts[0];
}


export function BPG_ExpCalc(postExp_,funcSet,fallbackFunctionSet) {//no $ and # is allowed
  let postExp=postExp_.filter(exp=>exp!="$")
  funcSet = {
    min$: arr => Math.min(...arr),
    max$: arr =>{
      return Math.max(...arr)
    },
    "$+$": vals => vals[0] + vals[1],
    "$-$": vals => vals[0] - vals[1],
    "$*$": vals => vals[0] * vals[1],
    "$/$": vals => vals[0] / vals[1],
    "$^$": vals => Math.pow(vals[0] , vals[1]),
    ...funcSet,
    default:(exp,param)=>{
      if(fallbackFunctionSet!==undefined)
      {
        return fallbackFunctionSet(exp,param);
      }
      // if(exp=="pi")return Math.PI;
      let parsedFloat=parseFloat(exp)
      return parsedFloat==exp?parsedFloat:NaN;
    }
      
    
    
    //default:_=>false
  };

  return PostfixExpCalc(postExp, funcSet);
}

export const DEF_EXTENSION = "hydef";


// ---------------------------------------------------------------------------
// File-browser filters.
//
// The def folder is a Resilio Sync share: the whole fleet reads and writes it,
// so a directory listing contains more than recipes. Three kinds of impostor
// pass a naive `name.includes(".hydef")`, and each one looks like a working
// recipe in the picker:
//
//   test1.hydef.!sync                a download still in progress — a partial file
//   test1.Conflict.2026-08-26.hydef  another machine's version of the same recipe
//   .sync/  .SyncArchive/            Resilio's own state, and its history of every
//                                    def anyone ever deleted or overwrote
//
// Picking the first loads half a def; the second silently runs someone else's
// recipe; the third offers a browsable museum of stale versions. So: match the
// extension at the END of the name, and hide the sync machinery outright.
// ---------------------------------------------------------------------------

const SYNC_DIR_NAMES = new Set([".sync", ".syncarchive", ".syncid", ".synctemp", ".bts"]);
// Resilio has used both `.!sync` and the older `.bts` for in-progress files, as a
// suffix and (for hidden placeholders) as a prefix. Conflict copies carry
// `.Conflict.` or, in older builds, `.conflict_`.
const SYNC_PARTIAL_RE = /(^\.!sync|\.!sync$|^\.bts$|\.bts$)/i;
const SYNC_CONFLICT_RE = /\.conflict[._]/i;

/** True for anything Resilio put in the folder that is not a user file. */
export function isSyncArtifact(fileInfo) {
  const name = (fileInfo && fileInfo.name) || "";
  if (fileInfo && fileInfo.type === "DIR") return SYNC_DIR_NAMES.has(name.toLowerCase());
  return SYNC_PARTIAL_RE.test(name) || SYNC_CONFLICT_RE.test(name);
}

/**
 * Browser filter for one file extension: directories through, files only when
 * the name actually ends in `.<ext>`, sync artefacts never.
 */
export function makeExtensionFilter(ext) {
  const suffix = "." + String(ext).toLowerCase();
  return (fileInfo) => {
    if (isSyncArtifact(fileInfo)) return false;
    if (fileInfo && fileInfo.type === "DIR") return true;
    const name = (fileInfo && fileInfo.name) || "";
    return name.toLowerCase().endsWith(suffix);
  };
}

/** The def-file filter. Use this rather than open-coding the extension test. */
export const defFileFilter = makeExtensionFilter(DEF_EXTENSION);



// R-quick-wins: renamed from CameraCtrl to avoid a footgun name collision with
// canvas/CameraCtrl.ts (the viewport controller). This one is the hardware-camera
// transfer/setup wrapper sent over BPG. Imports should reference CameraTransferCtrl
// directly; the legacy `CameraCtrl` alias below preserves backward-compat.
export class CameraTransferCtrl {
  constructor(setting) {
    this.data = {
      DoImageTransfer: true,
      cameraFrameRate: 30,
    };
    this.ws_ch = setting.ws_ch;

    this.ev_frameRateChange = setting.ev_frameRateChange;
    if (this.ev_frameRateChange === undefined)
      this.ev_frameRateChange = () => { };

    this.setCameraFrameRate(8);
  }



  setCameraImageTransfer(doTransfer) {
    if (doTransfer === undefined) doTransfer = !this.data.DoImageTransfer;
    this.data.DoImageTransfer = doTransfer;
    this.ws_ch({ DoImageTransfer: doTransfer });
  }


  setCameraFrameRate(framerate) {
    if (this.data.framerate == framerate) return;
    log.info("setCameraFrameRate:" + framerate);
    this.data.framerate = framerate;

    this.ev_frameRateChange(framerate);
    this.ws_ch({ CameraSetting: { framerate: framerate } });
  }


  setImageCropParam(cropWindow,downSampleFactor=8) {


    let obj={};
    if(cropWindow!==undefined)
    {
      obj.ImageTransferSetup={
        crop:cropWindow
      };
    }
    
    if(downSampleFactor!==undefined)
    {
      obj.CameraSetting={
        down_samp_level:downSampleFactor
      };
    }
    this.ws_ch(obj);
  }



  setCameraSpeed_HIGHEST() {
    this.setCameraFrameRate(9999999);
  }
  setCameraSpeed_HIGH() {
    this.setCameraFrameRate(20);
  }
  setCameraSpeed_LOW() {
    this.setCameraFrameRate(2);
  }

}

// R-quick-wins: alias to the old name to preserve backward-compat for any
// consumer still importing `CameraCtrl` from this module. New code should use
// CameraTransferCtrl to avoid the canvas/CameraCtrl.ts name collision.
export const CameraCtrl = CameraTransferCtrl;



export default { raw2header, raw2obj_rawdata, raw2obj,raw2Obj_IM,objbarr2raw,INSPECTION_STATUS,map_BPG_Packet2Act }

//let binaryX = BPG_Protocol.obj2raw("TC",{a:1,b:{c:2}});
//console.log(BPG_Protocol.raw2obj({data:binaryX.buffer}));
