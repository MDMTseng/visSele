
import {PostfixExpCalc,ESP2POST_FuncSet,ESP2POST_FuncCallBack} from './MISC_Util';

var enc = new TextEncoder();

const BPG_header_L = 9;
let raw2header=(ws_evt:any, offset = 0)=>{
  if (( ws_evt.data instanceof ArrayBuffer) && ws_evt.data.byteLength>=BPG_header_L) {
    // var aDataArray = new Float64Array(evt.data);
    // var aDataArray = new Uint8Array(evt.data);
    var headerArray = new Uint8ClampedArray(
      ws_evt.data,offset,BPG_header_L);
    return{
      type:String.fromCharCode(headerArray[0],headerArray[1]),
      prop: headerArray[2],
      pgID: (headerArray[3]<<8)+headerArray[4],
      length:headerArray[5]<<24 | headerArray[6]<<16 |
            headerArray[7]<<8  | headerArray[8]


    };
  }
  return undefined;
};
let raw2obj_rawdata=(ws_evt:any, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj===undefined)return undefined;

  return {
    ...ret_obj,
    rawdata : new Uint8ClampedArray(ws_evt.data,
      offset+BPG_header_L,ret_obj.length)
  };
};
let raw2obj=(ws_evt:any, offset = 0)=>{
  let ret_obj = raw2obj_rawdata(ws_evt, offset);
  if(ret_obj==undefined)return undefined;

  let  enc = new TextDecoder("utf-8");
  let str = enc.decode(ret_obj.rawdata);
  //console.log(str);
  try{
    return {
      ...ret_obj,
      data:JSON.parse(str)
    }
  }
  catch(e)
  {
  }
  return {
    ...ret_obj,
    data:str
  }
};
let raw2obj_IM=(ws_evt:any, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;

  let headerL=15;
  let headerArray = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,headerL);


  let camera_id=headerArray[0];
  let format=headerArray[1];
  let offsetX=(headerArray[2]<<8)|headerArray[3];
  let offsetY=(headerArray[4]<<8)|headerArray[5];
  let width=(headerArray[6]<<8)|headerArray[7];
  let height=(headerArray[8]<<8)|headerArray[9];

  let scale=headerArray[10];
  let full_width=(headerArray[11]<<8)|headerArray[12];
  let full_height=(headerArray[13]<<8)|headerArray[14];


  let image:ImageData|HTMLImageElement|undefined=undefined;
  let image_b64:string|undefined=undefined;
  // console.log(ws_evt);
  if(format==0)
  {
    let RGBA_pix_Num = 4*width*height;
    let _image=new Uint8ClampedArray(ws_evt.data,
      offset+BPG_header_L+headerL,4*width*height);
    image=new ImageData(_image, width);
  }
  else if(format==50)
  {
    let narr=new Uint8Array(ws_evt.data,offset+BPG_header_L+headerL) as any;
    // console.log(narr);


    var enc = new TextDecoder("ascii");
    image_b64=enc.decode(narr);
    
    // image_b64=(String.fromCharCode.apply(null, narr))
    // console.log(image_b64);
    
    image= new Image();
    
    image.src=image_b64;

  }

  return {
    camera_id,
    format,
    offsetX,
    offsetY,
    width,height,
    scale,
    full_width,full_height,
    // _image,
    image
  };
};

let objbarr2raw=(type:string,prop:number,pgID:number,obj:({[key:string]:any}|undefined),barr:(Uint8Array|undefined)=undefined)=>{
  
  let str = (obj===undefined)?"":JSON.stringify(obj);

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
  if(barr!=undefined)
  {
    bbuf.set(barr, BPG_header_L+objstr_L+1);
  }
  return bbuf;
};



export function BPG_ExpCalc(postExp_:string[],funcSet:ESP2POST_FuncSet,fallbackFunctionSet:ESP2POST_FuncCallBack) {//no $ and # is allowed
  let postExp=postExp_.filter(exp=>exp!="$")
  funcSet = {
    min$: (arr:any) => Math.min(...arr),
    max$: (arr:any) =>{
      return Math.max(...arr)
    },
    "$+$": (vals:any) => vals[0] + vals[1],
    "$-$": (vals:any) => vals[0] - vals[1],
    "$*$": (vals:any) => vals[0] * vals[1],
    "$/$": (vals:any) => vals[0] / vals[1],
    "$^$": (vals:any) => Math.pow(vals[0] , vals[1]),
    ...funcSet,
    default:(exp,param)=>{
      if(fallbackFunctionSet!==undefined)
      {
        return fallbackFunctionSet(exp,param);
      }
      // if(exp=="pi")return Math.PI;
      let parsedFloat=parseFloat(exp)
      return parsedFloat;
    }
      
    
    
    //default:_=>false
  };

  return PostfixExpCalc(postExp, funcSet);
}

export const DEF_EXTENSION = "hydef";


export default { raw2header, raw2obj_rawdata, raw2obj,raw2obj_IM,objbarr2raw }

//let binaryX = BPG_Protocol.obj2raw("TC",{a:1,b:{c:2}});
//console.log(BPG_Protocol.raw2obj({data:binaryX.buffer}));
