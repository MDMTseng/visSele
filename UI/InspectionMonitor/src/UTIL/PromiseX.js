export default function(){
  let promiseSet={
    promise:null,
    _isFulfilled:false,
    _isRejected:false,
    callBack:{
      res:null,
      rej:null
    },
    IsFulfilled:null,IsRejected:null,IsSettled:null,
    value:null
  };

  promiseSet.IsFulfilled=function(){
    return this._isFulfilled;
  }.bind(promiseSet);
  promiseSet.IsRejected=function(){
    return this._isRejected;
  }.bind(promiseSet);
  promiseSet.IsSettled=function(){
    return this._isRejected||this._isFulfilled;
  }.bind(promiseSet);


  promiseSet.promise=new Promise((res,rej)=>
  {
      promiseSet.callBack.res=function(data)
      {
        if(this.IsSettled())
        {
          throw new Error("This Promise has settled");
        }
        this.callBack.res=null;
        this.callBack.rej=null;
        this.value=data;
        res(data);
        this._isFulfilled=true;
      }.bind(promiseSet);


      promiseSet.callBack.rej=function(data)
      {
        if(this.IsSettled())
          throw new Error("This Promise has settled");
        this.callBack.res=null;
        this.callBack.rej=null;
        this.value=data;
        rej(data);
        this._isRejected=true;
      }.bind(promiseSet);
  });
  return promiseSet;
};

/*
let promiseSets=[Promise_(),Promise_(),Promise_()];
promiseSets.map((promiseSet,idx)=>promiseSet.promise.then((d)=>{console.log(d)}).catch((e)=>{console.log(e)}));
promiseSets.map((promiseSet,idx)=>
  promiseSet.resource={
    id:"promiseSet_"+idx,
    data:idx,
    resDelay:Math.floor((Math.random() * 30))/10
  }//set delay resolve 0~3000ms
);
console.log("resDelay:"+promiseSets.map((promiseSet)=>promiseSet.resource.resDelay));


Promise.all(promiseSets.map((promiseSet)=>promiseSet.promise)).
then((datas)=>{

  console.log("promise Resolved:");
  console.log(datas);
}).catch((info)=>
  console.log("promise Failed:"+info)
);


promiseSets.map((promiseSet)=>//attach timeout 2.5s
{
  setTimeout(function(){
    return this.IsSettled()?null:
      this.callBack.rej("Timeout:ID:"+this.resource.id);
  }.bind(promiseSet)
  ,2500);
});


promiseSets.map((promiseSet)=>//attach resolve timer
{
  setTimeout(function(){
    return this.IsSettled()?null:
      this.callBack.res(this.resource);
  }.bind(promiseSet)
  ,promiseSet.resource.resDelay*1000);
});
*/
