
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

    ret_obj.length =
      headerArray[5]<<24 | headerArray[6]<<16 |
      headerArray[7]<<8  | headerArray[8];
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

  ret_obj.rawdata = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,ret_obj.length
  );
  let  enc = new TextDecoder("utf-8");
  let str = enc.decode(ret_obj.rawdata);
  ret_obj.data = JSON.parse(str);
  return ret_obj;
};
let raw2Obj_IM=(ws_evt, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;

  let headerArray = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,6);
  ret_obj.camera_id=headerArray[0];
  ret_obj.session_id=headerArray[1];
  ret_obj.width=(headerArray[2]<<8)|headerArray[3];
  ret_obj.height=(headerArray[4]<<8)|headerArray[5];
  let RGBA_pix_Num = 4*ret_obj.width*ret_obj.height;
  

  if(true||RGBA_pix_Num == (ret_obj.length-6))
  {
    ret_obj.image=new Uint8ClampedArray(ws_evt.data,
      offset+BPG_header_L+6,4*ret_obj.width*ret_obj.height);
  }
  else
  {
    ret_obj.image=null;
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
  bbuf[3] = pgID>>1;
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



export const INSPECTION_STATUS = {
  NA:-128,
  SUCCESS:0,
  FAILURE:-1,
};




export const DEF_EXTENSION = "hydef";

export default { raw2header, raw2obj_rawdata, raw2obj,raw2Obj_IM,objbarr2raw,INSPECTION_STATUS }

//let binaryX = BPG_Protocol.obj2raw("TC",{a:1,b:{c:2}});
//console.log(BPG_Protocol.raw2obj({data:binaryX.buffer}));
