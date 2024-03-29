
import * as UIAct from 'REDUX_STORE_SRC/actions/UIAct';

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
  let headerArray = new Uint8ClampedArray(ws_evt.data,
    offset+BPG_header_L,headerL);
  ret_obj.camera_id=headerArray[0];
  ret_obj.session_id=headerArray[1];
  ret_obj.offsetX=(headerArray[2]<<8)|headerArray[3];
  ret_obj.offsetY=(headerArray[4]<<8)|headerArray[5];
  ret_obj.width=(headerArray[6]<<8)|headerArray[7];
  ret_obj.height=(headerArray[8]<<8)|headerArray[9];


  ret_obj.scale=headerArray[10];
  ret_obj.full_width=(headerArray[11]<<8)|headerArray[12];
  ret_obj.full_height=(headerArray[13]<<8)|headerArray[14];

  // console.log(ret_obj,headerArray);
  let RGBA_pix_Num = 4*ret_obj.width*ret_obj.height;
  
  //console.log(headerArray,RGBA_pix_Num,ret_obj.length-headerL);
  if(true||RGBA_pix_Num == (ret_obj.length-headerL))
  {
    ret_obj.image=new Uint8ClampedArray(ws_evt.data,
      offset+BPG_header_L+headerL,4*ret_obj.width*ret_obj.height);
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



export const INSPECTION_STATUS = {
  NA:-128,
  UNSET:-100,
  SUCCESS:0,
  FAILURE:-1,
};




function map_BPG_Packet2Act(parsed_packet)
{
  let acts=[];
  switch(parsed_packet.type )
  {
    case "HR":
    {
      /*//log.info(this.props.WS_CH);
      this.props.ACT_WS_SEND(this.props.WS_ID,"HR",0,{a:["d"]});
      
      this.props.ACT_WS_SEND(this.props.WS_ID,"LD",0,{filename:"data/default_camera_param.json"});
      break;*/
    }

    case "SS":
    {
      break;
    }
    case "IM":
    {
      let pkg = parsed_packet;
      let img = new ImageData(pkg.image, pkg.width);
      

      let objx={...pkg,img}
      delete objx["image"];
      acts.push(UIAct.EV_WS_Image_Update(
        objx
      ));
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
    case "PD":
    {
      //log.debug(report.type,report);
      acts.push(UIAct.EV_WS_PD_DATA_Update(parsed_packet.data));
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



export const DEF_EXTENSION = "hydef";

export default { raw2header, raw2obj_rawdata, raw2obj,raw2Obj_IM,objbarr2raw,INSPECTION_STATUS,map_BPG_Packet2Act }

//let binaryX = BPG_Protocol.obj2raw("TC",{a:1,b:{c:2}});
//console.log(BPG_Protocol.raw2obj({data:binaryX.buffer}));
