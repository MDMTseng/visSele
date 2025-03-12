import {PostfixExpCalc,ESP2POST_FuncSet,ESP2POST_FuncCallBack} from './MISC_Util';

var enc = new TextEncoder();

const BPG_header_L = 9;

const raw2header=(ws_evt:any, offset = 0)=>{
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




var Ascii_Decoder = new TextDecoder("ascii");
let IM_CACHE_BUFF:Map<number,HTMLImageElement> = new Map();
// const magicCacheThreshold = 5;
// const blobUrlCache = new Set<Image>();
let raw2obj_IM=(ws_evt:any, offset = 0)=>{
  let ret_obj = raw2header(ws_evt, offset);
  if(ret_obj==null)return null;




  let headerLengthFieldSize=4;
  let headerLengthField = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,headerLengthFieldSize);

  let headerLength = headerLengthField[0]<<24 | headerLengthField[1]<<16 |
            headerLengthField[2]<<8  | headerLengthField[3];

  let headerJsonBuffer = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L+headerLengthFieldSize,headerLength);



  let headerJsonString = new TextDecoder("utf-8").decode(headerJsonBuffer);
  console.log(headerLength,headerJsonString);
  let headerJson = JSON.parse(headerJsonString);

  console.log("[IM]img header",headerLength,headerJson);


  let camera_id=headerJson.camera_id;
  let format=headerJson.format;
  let offsetX=headerJson.offset[0]??0;
  let offsetY=headerJson.offset[1]??0;
  let width=headerJson.size[0];
  let height=headerJson.size[1];

  let scale=headerJson.scale??1;
  let full_width=headerJson.full_width??width;
  let full_height=headerJson.full_height??height;
  let imbuff_id=headerJson.imbuff_id??-999;

  let is_base64_encoded=headerJson.base64_encoded??false;


  console.log("format",format,headerLengthFieldSize,headerLength);


  let image:ImageData|HTMLImageElement|undefined=undefined;
  // console.log(ws_evt);
  if(IM_CACHE_BUFF.has(imbuff_id))
  {
    image=IM_CACHE_BUFF.get(imbuff_id);
    console.log("get from cache",image);
  }
  else if(format=="raw")
  {
    let RGBA_pix_Num = 4*width*height;
    let _image=new Uint8ClampedArray(ws_evt.data,
      offset+
      BPG_header_L+
      headerLengthFieldSize+
      headerLength,
      
      4*width*height);
    image=new ImageData(_image, width);
  }
  else if(format=="jpg"||format=="png")
  {
    const narr = new Uint8Array(ws_evt.data, 
      offset+
      BPG_header_L+
      headerLengthFieldSize+
      headerLength);



    if(is_base64_encoded)
    {
      // image_b64=(String.fromCharCode.apply(null, narr))
      // console.log(image_b64);
      
      image= new Image();
      
      image.src=Ascii_Decoder.decode(narr);;
  
    }
    else
    {
      const blob = new Blob([narr], { type: (format=="jpg")?'image/jpeg':'image/png' });
      const blobUrl = URL.createObjectURL(blob);
      const img = new Image();
  
      // Add onload handler to revoke URL only after image is loaded
      img.onload = () => {
        console.log("[IM]img onload");
        // URL.revokeObjectURL(blobUrl);
      };
  
      img.src = blobUrl;
      image = img;
    }
  }

  let data={
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

  console.log("data",data);
  return data;
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
