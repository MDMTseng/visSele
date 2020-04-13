

export function Num2Str_padding(pad,num)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
}

export function round(num,round_nor=1)
{
  let tmp = (1/round_nor);
  let round_nor_inv = Math.round(tmp);
  if(round_nor_inv==0)
  {
    round_nor_inv=tmp;
  }

  return Math.round(num*round_nor_inv)/round_nor_inv;
}


export function xstate_GetCurrentMainState(state)
{
  if(typeof state.value === "string")
  {
    return {state:state.value, substate:null};
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return {state:state_Str, substate:complexState[state_Str]};

}

export function xstate_GetMainState(state)
{
  if(typeof state.value === "string")
  {
    return state.value;
  }
  let complexState = state.value;
  return Object.keys(complexState)[0];
}

export function xstate_GetSubState(state)
{
  if(typeof state.value === "string")
  {
    return undefined;
  }
  let complexState = state.value;
  let state_Str = Object.keys(complexState)[0];
  return complexState[state_Str];
}


export function GetObjElement(rootObj,keyTrace,traceIdxTo=keyTrace.length-1)
{
  let obj = rootObj;
  let traceIdxTLen = traceIdxTo+1;
  if( rootObj === undefined)return;
  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    //console.log(obj,key,obj[key]);
    obj = obj[key];

    if( obj === undefined)return;
  }
  return obj;
}

export function isString (value) {
  return typeof value === 'string' || value instanceof String;
}


export function DictConv(key,dict,dictTheme)
{
  let translation = (dictTheme===undefined)?undefined:GetObjElement(dict,[dictTheme, key]);

  if(translation===undefined)
  {
    translation = GetObjElement(dict,["fallback", key]);
  }

  return translation;
}



export class websocket_autoReconnect{
  constructor(url,timeout=5000) {
      this.url=undefined;
      this.wsclose=false;
      this.reconnectionCounter=0;
      this.reconnectGap_ms=5000;
      this.connectionTimeout_ms=timeout;
      this.connectionTimer=null;
      this.readyState=undefined;
      this._connect(url);
    
  }
  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}

  onreconnection(reconnectionCounter){}
  onconnectiontimeout(){}

  setWebsocketCallbackUndefined(ws)
  {
    ws.onopen=
    ws.onmessage=
    ws.onerror=
    ws.onclose=undefined;
  }
  _connect(url) {
    if(this.wsclose)return;
    
    console.log("_connect");
    this.url=url;
    if(this.websocket!==undefined)
    {
        this.reconnectionCounter++;
        let doconnection = this.onreconnection(this.reconnectionCounter);
        if(doconnection!==undefined && doconnection!=true)
        {
          this.websocket=undefined;
          return;
        }
    }

    if(this.connectionTimer!==undefined)
      clearTimeout(this.connectionTimer);
    this.connectionTimer=undefined;

    this.websocket = new WebSocket(url);
    this.OPEN=this.websocket.OPEN;
    this.CONNECTING=this.websocket.CONNECTING;
    this.CLOSED=this.websocket.CLOSED;
    this.CLOSING=this.websocket.CLOSING;
    
    this.onNewState(this.CONNECTING);
    this.connectionTimer = setTimeout(()=>{
        this.close();
        this.onconnectiontimeout();
    },this.connectionTimeout_ms);
    // this.state.WS_DB_Insert.binaryType = "arraybuffer";

    this.websocket.onopen = (ev)=>{
      this.readyState=this.websocket.readyState;
      this.onNewState(this.readyState);
      clearTimeout(this.connectionTimer);
      this.connectionTimer=undefined;
      return this.onopen(ev);
    };
    this.websocket.onmessage =(ev)=> this.onmessage(ev);
    this.websocket.onerror =(ev)=>{
      this.readyState=this.websocket.readyState;
      this.onNewState(this.readyState);
      //setTimeout(()=>this._connect(url),10);
      return this.onerror(ev);
    }
    this.websocket.onclose =(ev)=>{
      this.readyState=this.websocket.readyState;
      this.onNewState(this.readyState);
      setTimeout(()=>this._connect(url),this.reconnectGap_ms);
      return this.onclose(ev);
    }
  }
  
  reconnect() {
    console.log(    this.wsclose,this.websocket);
    this.wsclose=false;
    if(this.websocket!==undefined)
    {
      if(this.websocket.readyState!=this.websocket.OPEN)
      {
        setWebsocketCallbackUndefined(this.websocket);
        this.websocket=undefined;
        this._connect(this.url);
      }
      else
        this.websocket.close();//this should trigger _connect affter close/error event
    }
    else
    {
      this._connect(this.url);
    }
  }
  
  onNewState(state){
    //console.log(this.curState,state);
    if(this.curState!=state)
    {
      this.onStateUpdate(state,this.curState);
      this.curState=state;
    }
  }
  onStateUpdate(curState,preState){

    //console.log("onStateUpdate",curState,preState);
  }

  close() {
    this.wsclose=true;
    
    if(this.connectionTimer!==undefined)
      clearTimeout(this.connectionTimer);
    this.connectionTimer=undefined;
    if(this.websocket!==undefined)
    {
        this.websocket.close();

    }
  }

  send(data){
    return this.websocket.send(data);
  }
}

export class websocket_reqTrack{
  constructor(websocket) {
    let onopen = websocket.onopen;
    let onmessage = websocket.onmessage;
    let onerror = websocket.onerror;
    let onclose = websocket.onclose;

    this.trackWindow={};

    websocket.onopen=(ev)=>{
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onopen(ev);
    };
    websocket.onclose=(ev)=>{
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onclose(ev);
    };
    websocket.onmessage=(ev)=>{
      this.readyState=this.websocket.readyState;
      let p = JSON.parse(ev.data);
      let type=p.type;
      if(type=="ACK" || type=="NAK")
      {
        let req_id=p.req_id;
        if(req_id!==undefined)
        {
          let tobj = this.trackWindow[req_id];
          if(tobj!==undefined)
          {
            delete this.trackWindow[req_id];
            if(type=="ACK")
            {
              tobj.resolve(p);
            }
            else
            {
              tobj.reject(p);
            }
          }
          else
          {
            req_id=undefined;
          }
        }
        
        if(req_id===undefined){
          
          this.onTrackError({
            type:"ACK_TRACK_ERR",
            data:p
          });
        }
      }
      this.onmessage(ev,p);
    };
    websocket.onerror=(ev)=>{
      
      this.trackWindow={};
      this.readyState=this.websocket.readyState;
      this.onerror(ev);
    };
    this.websocket=websocket;
  }
  
  onTrackError(ev){}

  onopen(ev){}
  onmessage(ev){}
  onerror(ev){}
  onclose(ev){}


  
  close() {
    return this.websocket.close();
  }

  send_obj(data,replacer){

    let req_id = data.req_id;

    while(req_id===undefined||
      Object.keys(this.trackWindow).reduce((match,id)=>match||id === req_id,false))
      //Check existance
    {
      //if req_id is undefined / exists in the lookup table
      req_id = Math.floor(Math.random()*16777215).toString(16);
    }
    data.req_id = req_id;


    this.websocket.send(JSON.stringify(data,replacer));
  
    let trackObj={
      time:Date.now(),
      resolve:undefined,
      reject:undefined,
      data:data,
      rsp:undefined
    };
    this.trackWindow[req_id]=trackObj;
    return new Promise((resolve, reject)=>{
      trackObj.resolve=resolve;
      trackObj.reject=reject;
    });
  }
}


export function undefFallback(val,fallback) {
  return  val!==undefined?val:fallback;
}

  
export function dictLookUp(key,dict,theme) {
  let transVal;
  if(Array.isArray(key))
  {
    transVal=  GetObjElement(dict,key);
    return undefFallback(transVal,key[key.length-1]);
  }
  let dictTheme=undefFallback(theme,"fallback");
  transVal = GetObjElement(dict,[dictTheme, key]);
  return  undefFallback(transVal,key);
}


export const copyToClipboard = str => {
  const el = document.createElement('textarea');
  el.value = str;
  document.body.appendChild(el);
  el.select();
  document.execCommand('copy');
  document.body.removeChild(el);
};





export function twoEQXY(a,b,c,d,e,f) {
  /* we solve the linear system
  * ax+by=e
  * cx+dy=f
  */
 let determinant = a*d - b*c;
 if(determinant != 0) {
   let x = (e*d - b*f)/determinant;
   let y = (a*f - e*c)/determinant;
   return [x,y];
 }
 return [0/0,0/0];
}

export function Calibration_MMPP_offset(old1,new1,old2,new2,cur_mmpp,cur_offset=0) {
 //pix1=old1/cur_mmpp-cur_offset
 //mmpp*(pix1+offset)=new1
 //mmpp*pix1 + mmpp*offset = new1
 //X   *a    +           Y =    e
 //b==1

 //a = old_pix1  b=1  e=new1
 //c = old_pix2  d=1  f=new2
 let old_pix1=old1/cur_mmpp-cur_offset;
 let old_pix2=old2/cur_mmpp-cur_offset;
 let retSolver=twoEQXY(
  old_pix1,1,old_pix2,1,new1,new2);
 let [mmpp,mmpp_x_offset] = retSolver;
 return {
   mmpp,offset:(mmpp_x_offset/mmpp)
 };
}







function extractAtomGroup(exp_str_arr, mark = "$") {
  let lastH = exp_str_arr[exp_str_arr.length - 1];
  let matchD = lastH.match(/\(([^\(\)]+)\)/);
  //let matchD=/\([^\(\)]+\)/g.exec(lastH);
  if (matchD !== null && matchD.length > 0) {
    exp_str_arr[exp_str_arr.length - 1] = matchD[1];

    var newstr = lastH.replace(/\([^\(\)]+\)/, mark);
    exp_str_arr.push(newstr);
  }
  return exp_str_arr;
}
function extractArithGroup(expression, sep = "\\-\\+", mark = "$") {
  let regx_param;
  let regx_opera;

  if (mark == "$") mark += "$";

  regx_param = new RegExp("(.+)[" + sep + "](.+)");
  regx_opera = new RegExp(".+([" + sep + "]).+");

  let matchD = expression.match(regx_param);
  if (matchD == null) {
    return [expression];
  }
  var rep = expression.replace(regx_opera, mark + "$1" + mark);
  return [matchD[1], matchD[2], rep];
}

function extractArith(exp_stack, operator_regexp = "\\-\\+", mark = "$") {
  return exp_stack
    .map(exp => {
      if (operator_regexp == "," && exp.includes(",")) {
        if (exp.includes(",")) {
          let splitArr = exp.split(",");
          let aggStr = mark;
          for (let i = 1; i < splitArr.length; i++) aggStr += "," + mark;

          splitArr.push(aggStr);
          return splitArr;
        }
        return exp;
      }

      let parse_target = operator_regexp;
      let arrayX = extractArithGroup(exp, parse_target, mark);
      while (true) {
        let arrayY = extractArithGroup(arrayX[0], parse_target, mark);
        if (arrayY.length == 1) break;
        arrayX.shift();
        arrayX = arrayY.concat(arrayX);
      }

      return arrayX;
    })
    .flat();
}

export function Exp2PostfixExp(exp_str) {//exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  let retA = [exp_str];

  let p_Mark = "$";
  
  //exp="Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)"
  // First pass Postfix exp converter
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }

  let postExpCalc_Func = {
    default: (key, vals) => {
      if (vals !== undefined) {
        
        //console.log("ori.key",key);
        let skey = key.split("$");
        let newKey =skey.reduce((nk,ele,idx)=>{
          if(idx==0)return ele;
          let repK = ((idx-1)>=vals.length)?"$":vals[idx-1]
          return nk+"(" + repK + ")"+ele;
        },"");
        key=newKey;
        
        // key = key.replace(/\$/g, "#");
        // vals.forEach(val => {
        //   key = key.replace("#", "(" + val + ")");
        // });
        // key = key.replace(/\#/g, "$");
        
//         console.log("...",newKey,key);
      }
      return key;
    }
  };
  //console.log("retA",retA);
  //["5-1/4*3", "4+5", "3+Math.tan$/3,1+2*3/$/6", "Math.max$"]
  retA = retA.map(exp => {//parse piece expression with +-*/... operators 
    exp = exp.replace(/\$/g,"#");
    let a_post_exp = [",","\\-\\+","%","\\*\\/","\\^"].reduce(
      (pexp,exp)=>
       extractArith(pexp, exp, "$"),[exp]);
    //Assamble back with "(" ")"
    let XXY = PostfixExpCalc(a_post_exp, postExpCalc_Func);
    return XXY[0].replace(/\#/g,"$");
  });
  //console.log("retA",retA);
  //["(5)-(((1)/(4))*(3))", "(4)+(5)", "((3)+((Math.tan$)/(3))),((1)+((((2)*(3))/($))/(6)))", "Math.max$"]
  //Assamble back with "(" ")"
  retA = PostfixExpCalc(retA, postExpCalc_Func);
  
  
  //Now we have all the "(" ")" to do the 2nd pass Postfix exp converter

  //console.log("retA",retA);
  //["Math.max(((3)+((Math.tan((5)-(((1)/(4))*(3))))/(3))),((1)+((((2)*(3))/(((4)+(5))))/(6))))"]
  while (true) {
    let currentSize = retA.length;
    retA = extractAtomGroup(retA, p_Mark);

    if (currentSize == retA.length) break;
  }
  //console.log("retA",retA);
  //["3", "5", "1", "4", "$/$", "3", "$*$", "$-$", "Math.tan$", "3", "$/$", "$+$", "1", "2", "3", "$*$", "4", "5", "$+$", "$", "$/$", "6", "$/$", "$+$", "$,$", "Math.max$"]
  return retA;
}

export function PostfixExpCalc(postExp, funcSet) {
  let valStack = [];

  postExp.forEach(exp => {
    let valPush;
    
    if (exp.includes !== undefined && exp.includes("$")) {
      let groupFlagCount = exp.split("$").length - 1;
      let tmpArr = valStack.splice(valStack.length - groupFlagCount, 9999999);
      if (funcSet[exp] !== undefined) valPush = funcSet[exp](tmpArr.flat());
      else valPush = funcSet.default(exp, tmpArr.flat());

    } else {
      if (funcSet.default !== undefined) {
        valPush = funcSet.default(exp);
      } else {
        valPush = parseFloat(exp);
      }
    }

    valStack.push(valPush);
  });
  return valStack;
}

function ExpCalc(exp, funcSet) {
  let postExp_ = Exp2PostfixExp(exp);
  
  let postExp=postExp_.filter(exp=>exp!="$")
  // console.log(postExp);
  funcSet = {
    ...funcSet,
    min$: arr => Math.min(...arr),
    max$: arr => Math.max(...arr),
    "$+$": vals => vals[0] + vals[1],
    "$-$": vals => vals[0] - vals[1],
    "$*$": vals => vals[0] * vals[1],
    "$/$": vals => vals[0] / vals[1],
    "$^$": vals => Math.pow(vals[0] , vals[1]),
    "$": vals => vals,
  };

  return PostfixExpCalc(postExp, funcSet)[0];
}




export class CircularCounter{
  constructor(total_size)
  {
    this._total_size=total_size;
    this.clear();
  }
  
  size()
  {
    return this._size;
  }

  clear()
  {
    this._sidx=0;
    this._eidx=0;
    this._size=0;
  }
  tsize()
  {
    return this._total_size;
  }

  f(idx=-1)
  {
    idx+=1;
    if(idx>this._size)return -1;
    idx+=this._sidx-idx+this._total_size;
    return idx%this._total_size;
  }
  
  r(idx=0)
  {
    if(idx>=this._size)return -1;
    idx=this._eidx+idx;
    return idx%this._total_size;
  }
  
  enQ(force=false)
  {
    //if(!force && this._size>=this._total_size)return false;
    
    if(this._size==this._total_size)
    {
      if(!force)return false;
      //if we force push data then we deQ one data out
      deQ();
    }
    this._sidx+=1;
    this._sidx%=this._total_size;
    this._size++;
    return true;
  }
  
  deQ()
  {
    if(this._size<=0)return false;
    this._eidx+=1;
    this._eidx%=this._total_size;
    this._size--;
    return true;
  }
}




export class ConsumeQueue{
  constructor(consumePromiseFunc,QSize=200) {
    this.cC=new CircularCounter(QSize);
    this.queue=new Array(QSize);
    this.term=false;
    this.inPromise=false;

    this.consumePromiseFunc=consumePromiseFunc;
  }
  size()
  {
    return this.cC.size();
  }
  f(idx)
  {
    let qidx = this.cC.f(idx);
    if(qidx===-1)return undefined;
    return this.queue[qidx];
  }


  enQ(data)
  {
    if(this.cC.size()>=this.cC.tsize())
    {
      //full or error
      return false;
    }
    this.queue[this.cC.f()]=data;
    this.cC.enQ();
    return true;
  }

  deQ()
  {
    if(this.cC.size()==0)return undefined;
    let data = this.queue[this.cC.r()];
    this.cC.deQ();
    return data;
  }
  termination()
  {
    this.term=true;
  }

  kick()
  {
    //console.log("kick inPromise:"+this.inPromise);
    if(this.inPromise)
      return;
    
    this.inPromise=true;
    this.consumePromiseFunc(this).then(result=>{
      //console.log("Consume ok? result",result);
      this.inPromise=false;
      if(this.term)return;
      if(this.cC.size()!=0)
      {
        this.kick();//kick next consumption
      }
    }).catch(e=>{
      
      console.log("Consume failed... e=",e);
      this.inPromise=false;
    });
  }
}



// let exp_str = "Math.max(3+Math.tan(5-1/4*3)/3,1+2*3/(4+5)/6)";
// //"3+tan(5-1/4*3)/3"
// //"1+2*3/(4+5)/6"

// console.log(//eval(exp_str),
//   ExpCalc(exp_str, {
//     "$>$?$:$":(vals)=>{
//       return vals[0]>vals[1]?vals[2]:vals[3];
//     },
//     "Math.sin$":(vals)=>{
//       return Math.sin(vals[0]);
//     },
//     "Math.tan$":(vals)=>{
//       return Math.tan(vals[0]);
//     },
//     "Math.min$":(vals)=> Math.min(...vals),
//     "Math.max$":(vals)=> Math.max(...vals),
//     default: (key, vals) =>{
//       if(vals===undefined)//it's a single value parsing
//       {
//         let pv=parseFloat(key);
//         if(pv!=pv)
//           {
//             throw "ERROR: key:"+key+" is not parsible!";
//           }
//         return pv;
//       }
      
//       if(key.match(/^\$[\,\$]+$/gm)!==null)
//         return vals;
//       throw "ERROR: "+key+" is not defined!";
//       return vals;
//     }
//   })
// );
