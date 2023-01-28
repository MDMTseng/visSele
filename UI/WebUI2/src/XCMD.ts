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

export async function listCMDPromise(
  api:BPG_WS,CNC_api:CNC_Perif,v:any,cmdList:string[],
  onRunCmdIdexChanged:(index:number,info:string)=>void,
  abortSig?:AbortSignal,
  onUserInputRequest?:(setting:any)=>Promise<any>)
{
  v.inCMD_Promise=true;


  function SET_DISPLAY_ID_LIST(idList:string[])
  {
    v.InspTarDispIDList=idList;
  }

  let P=Promise;
  // function PALL(ps:Promise<any>[])
  // {
  //   return Promise.all()
  // }

  async function ackG(code:string,retryCount=5)
  {
    let retArr=[];
    for(let i=0;i<retryCount;i++)
    {
      let ret = await CNC_api.send_P({"type":"GCODE","code":code}) as any
      if(ret.ack==true)return ret;
      retArr.push(ret);
      await delay(5);
    }

    throw {
      msg:`Code:${code} retryCount:${retryCount} hits retry limit`,
      results:retArr
    };
  }
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


  function InspTargetExchange(
    id:string,data:any,_PGID_:number|undefined=undefined)
  {
    return api.InspTargetExchange(id, data,_PGID_);
  }



  async function READFILE(fileName:string)
  {


    
    let pkts = await api.send_P("LD",0,{filename:fileName},undefined) as any[];

    
    let RP=pkts.find((pkt)=>pkt.type=="FL")
    
    
    if(RP===undefined)throw "READFILE cannot find "+fileName

    

    return RP.data;
  }

  async function InspTargetEnvPath(inspTarId:string)
  {
          
    let pkts=await InspTargetExchange(inspTarId,{type:"get_info"}) as any[];

    
    let IFInfo=pkts.find((pkt)=>pkt.type=="IF");
    if(IFInfo===undefined || IFInfo.data===undefined)
    {
      return undefined
    }

    return IFInfo.data.env_path;
  }


  async function InspTargetEnvFolderStructure(inspTarId:string,subpath:string="",depth:number=1)
  {
    let env_path = await InspTargetEnvPath(inspTarId)
    if(env_path === undefined)
    {
      return undefined
    }
    return (await FOLDER_STRUCT(env_path+"/"+subpath,depth));
  }




  function FOLDER_STRUCT(folder_path:string,depth=1)
  {
    return api.Folder_Struct(folder_path,depth);
  }

  function SigWait_Clear(key:string)
  {
    if(v.sigListener===undefined)return;
    if(v.sigListener[key]===undefined)return;
    if(v.sigListener[key].reject===undefined)
    {
      delete v.sigListener[key];
      return;
    }

    v.sigListener[key].reject("CLEAR key:"+key);
    delete v.sigListener[key];

  }

  function SigWait_Trig_Reject(key:string,info:any)
  {
    if(v.sigListener[key]===undefined)throw `The key:[${key}] is not registered with SigWait_Reg`
    if(v.sigListener[key].reject===undefined)
    {
      v.sigListener[key].ERR=info;
      return
    }
    v.sigListener[key].reject(info);
  }
  function SigWait_Trig(key:string,info:any)
  {
    if(v.sigListener[key]===undefined)throw `The key:[${key}] is not registered with SigWait_Reg`
    if(v.sigListener[key].resolve===undefined)
    {
      v.sigListener[key].info=info;
      return
    }
    v.sigListener[key].resolve(info);
  }


  function SigWait_Trig_ex(key:string,info:any)
  {
    if(v.sigListener===undefined)v.sigListener={};
    if(v.sigListener[key]===undefined)v.sigListener[key]={}

    // if(v.sigListener[key]===undefined)throw `The key:[${key}] is not registered with SigWait_Reg`



    if(v.sigListener[key].resolve===undefined)
    {
      v.sigListener[key].info=info;
      return
    }
    v.sigListener[key].resolve(info);
  }
  function SigWait_Reg(key:string)
  {
    if(v.sigListener===undefined)v.sigListener={};
    if(v.sigListener[key])
    {
      if(v.sigListener[key].reject)
        v.sigListener[key].reject(`The key:[${key}] has a registered SigWait already`);
    }
    v.sigListener[key]={
    }
  }


  function SigFetch(key:string)
  {
    if(v.sigListener[key])
    {
      if(v.sigListener[key].info)
      {
        return (v.sigListener[key].info);
      }
    }

    return undefined;
  }


  function SigWait(key:string,timeout_ms:number=5000)
  {
    let timeoutID=-1;

    return new Promise((resolve,reject)=>{
      
      if(v.sigListener[key])
      {
        if(v.sigListener[key].err)
        {
          reject(v.sigListener[key].err);
        }
        if(v.sigListener[key].info)
        {
          resolve(v.sigListener[key].info);
        }
        else
        {
          v.sigListener[key].resolve=resolve;
          v.sigListener[key].reject=reject;

          if(timeout_ms>=0)
          {
            timeoutID=window.setTimeout(()=>{
              reject("TIMEOUT("+timeout_ms+").... key:"+key)
              delete v.sigListener[key];
            },timeout_ms)
          }

        }
      }
      else
      {
        reject("No registered repWait info");
      }
    }).then((rep:any)=>{
      if(timeoutID!=-1)
      {
        clearTimeout(timeoutID);
        timeoutID=-1;
      }
      delete v.sigListener[key];
      return rep;
    }).catch((err:any)=>{
      if(timeoutID!=-1)
      {
        clearTimeout(timeoutID);
        timeoutID=-1;
      }
      delete v.sigListener[key];
      throw err;
    })
  }





  function reportWait_reg(key:string,inspTar_id:string,trigger_id?:number)
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
      inspTar_id,trigger_id
    }
  }

  function reportWait(key:string,timeout_ms:number=5000)
  {
    let timeoutID=-1;

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

          if(timeout_ms>=0)
          {
            timeoutID=window.setTimeout(()=>{
              reject("TIMEOUT("+timeout_ms+").... key:"+key)
              delete v.reportListener[key];
            },timeout_ms)
          }

        }
      }
      else
      {
        reject("No registered repWait info");
      }
    }).then((rep:any)=>{
      if(timeoutID!=-1)
      {
        clearTimeout(timeoutID);
        timeoutID=-1;
      }
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



  async function waitUserCallBack(title:string|undefined,info_arr:{text:string,opts:any[],callback:any,default?:any}[])
  {
    if(onUserInputRequest===undefined)return info_arr.map((info,idx)=>info.default);
    let value = await onUserInputRequest({
      title:title,
      type:"SEL_CBS",
      data:info_arr});
    if(value)return value;
    // return default_value;
    return info_arr.map((info,idx)=>info.default);
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
    let file= await READFILE(fileName);
    return eval(file)
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
      console.error(e)
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


