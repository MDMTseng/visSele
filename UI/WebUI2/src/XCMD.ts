import {CORE_ID,CNC_PERIPHERAL_ID,BPG_WS,CNC_Perif,InspCamera_API} from './EXT_API';



function startsWith (str:string, needle:string) {
  var len = needle.length
  var i = -1
  while (++i < len) {
    if (str.charAt(i) !== needle.charAt(i)) {
      return false
    }
  }
  return true
}

export async function listCMDPromise(api:BPG_WS,CNC_api:CNC_Perif,v:any,cmdList:string[],onRunCmdIdexChanged:(index:number,info:string)=>void,abortSig?:AbortSignal,onUserInputRequest?:(setting:any)=>Promise<any>)
{
  v.inCMD_Promise=true;
  // v.DisplayIdList=["ruleId1","ruleId2"]

  function SET_DISPLAY_ID_LIST(idList:string[])
  {
    v.DisplayIdList=idList;
  }

  let P=Promise;
  // function PALL(ps:Promise<any>[])
  // {
  //   return Promise.all()
  // }
  function G(code:string)
  {
    return CNC_api.send_P({"type":"GCODE","code":code})
  }

  async function delay(ms=1000)
  {
    return new Promise((resolve,reject)=>setTimeout(resolve,ms))
  }
  async function LS_Save(key:string,msg:any)
  {
    localStorage.setItem("_listCMDPromise_"+key, JSON.stringify(msg))
  }
  async function LS_Load(key:string)
  {
    let read = localStorage.getItem("_listCMDPromise_"+key)
    if(read===null)return read;
    return JSON.parse(read);
  }

  function S(
    tl:string,
    prop:number,
    data?:{[key:string]:any},
    uintArr?:Uint8Array)
  {
    
    return api.send_P(tl,prop,data,uintArr)
  }
  async function READFILE(fileName:string)
  {
    let pkts = await api.send_P("LD",0,{filename:fileName},undefined) as any[];

    let RP=pkts.find((pkt)=>pkt.type=="FL")
    
    if(RP===undefined)throw "READFILE cannot find "+fileName


    return RP.data;
  }
  function reportWait_reg(key:string,trigger_tag:string,camera_id?:string,trigger_id?:number)
  {
    // console.log(key,trigger_tag,camera_id,trigger_id)
    if(v.reportListener[key])
    {
      if(v.reportListener[key].reject)
        v.reportListener[key].reject();
      
      delete v.reportListener[key];
      v.reportListener[key]=undefined;
    }
    v.reportListener[key]={
      trigger_tag,trigger_id,camera_id
    }
  }

  function reportWait(key:string)
  {
    return new Promise((resolve,reject)=>{

      if(v.reportListener[key])
      {
        if(v.reportListener[key].report)
        {
          resolve(v.reportListener[key].report);
        }
        else
        {
          v.reportListener[key].resolve=resolve;
          v.reportListener[key].reject=reject;
        }
      }
      else
      {
        reject("No registered repWait info");
      }
    }).then((rep:any)=>{
      delete v.reportListener[key];
      return rep;
    })
  }

  async function waitUserSelect(title:string,info="",arr:any[],default_value:any)
  {
    if(onUserInputRequest===undefined)return default_value;
    let value = await onUserInputRequest({
      title:title,
      type:"SELECT",
      data:arr,
      info,
      default_value:default_value});
    if(value)return value;
    return default_value;
  }
  async function waitUserSelects(title:string,info_arr:{text:string,opts:any[],default?:any}[])
  {



    // waitUserSelects("setup",[
    //   {text:"",opts:[1,2,3]},
    //   {text:">>",opts:["A","B","C"]},
    // ],[0,0])


    if(onUserInputRequest===undefined)return info_arr.map((info,idx)=>info.default);
    let value = await onUserInputRequest({
      title:title,
      type:"SELECTS",
      data:info_arr});
    if(value)return value;
    // return default_value;
    return info_arr.map((info,idx)=>info.default);
  }

  
  async function waitUserCheck(title:string,info="")
  {
    if(onUserInputRequest===undefined)return;
    await onUserInputRequest({
      title:title,
      type:"CHECK",
      info});
  }

  async function waitUserInput(obj:any,default_value:any)
  {
    if(onUserInputRequest===undefined)return default_value;
    let value = await onUserInputRequest({...obj,default_value:default_value});
    if(value)return value;
    return default_value;
  }

  let run_cmd_idx=0;
  function progressUpdate(info="")
  {
    onRunCmdIdexChanged(run_cmd_idx,info);
  }

  function NOT_ABORT()
  {
    if(abortSig&&abortSig.aborted)
    {
      throw("ACMD ABORT")
    }
  }

  async function INCLUDE(fileName:string)
  {
    return eval( await READFILE(fileName))
  }
  
  let $async=0;//just for a mark
  for(run_cmd_idx=0; run_cmd_idx<cmdList.length; run_cmd_idx++)
  {
    let cmd=cmdList[run_cmd_idx];
    v.inCMD_index=run_cmd_idx;
    progressUpdate();
    try{
      let ret;
      if(startsWith(cmd,'$async'))
        ret = await eval("(async () => {"+cmd+"})()");
      else
        ret = await eval(cmd);
    }
    catch(e){
      run_cmd_idx=-100;
      progressUpdate();
      throw({type:"ACMD exception",cmd,index:run_cmd_idx,e})
    }
    // console.log("$>",ret)
  }

  run_cmd_idx=-2;
  progressUpdate();
  
  v.inCMD_Promise=false;
  return true;
}


