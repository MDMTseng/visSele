

export function Num2Str_padding(pad,num)
{
  var str="0000000000000000"+(num);
  return str.substr(-pad);
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



export function GetObjElement(rootObj,keyTrace,traceIdxTo=keyTrace.length-1)
{
  let obj = rootObj;
  let traceIdxTLen = traceIdxTo+1;
  for (let i=0;i<traceIdxTLen;i++) {
    let key = keyTrace[i];
    console.log(obj,key,obj[key]);
    obj = obj[key];

    if( obj === undefined)return;
  }
  return obj;
}
