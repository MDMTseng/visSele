// Pure data structures (CircularCounter, ConsumeQueue). Extracted from MISC_Util.
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




  //|0|1|2|3|4|5|6|7|8|9
  //    |t|.........|h|   h:for next data(no actual data in it)    t:tail data idx(has data)
  //f(1)--------|*|
  //r(1)--|*|
  
  f(idx=-1)//from Q idx to array idx
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
  constructor(consumePromiseFunc,QSize=200,onTerminationState=(cq=>{})) {
    
    console.log(consumePromiseFunc,QSize,onTerminationState);
    this.cC=new CircularCounter(QSize);
    this.queue=new Array(QSize);
    this.term=false;
    this.inPromise=false;
    this.onTerminationState=onTerminationState;
    this.consumePromiseFunc=consumePromiseFunc;   
    //consumePromiseFunc has to return promise
    //resolve() will kick next consume
    //reject will stop kick next consume you will need to do it manually
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

  head()
  {
    if(this.cC.size()==0)return undefined;
    let data = this.queue[this.cC.r()];
    return data;
  }

  _doTermAct()
  {
    // console.log("T2");
    if(this.term)//not in promise state, and if it's terminated, call the onTerminationState and never do any thing more
    {
      
      // console.log("T3",this.onTerminationState);
      if(this.onTerminationState!==undefined)
        this.onTerminationState(this);
      this.onTerminationState=undefined;//only call it once
      return true;
    }
    return false;
  }

  termination()
  {
    this.term=true;
    // console.log("T1");

    if(this._doTermAct())
    {
      return;
    }
  }



  kick()
  {
    //console.log("kick inPromise:"+this.inPromise);
    if(this.inPromise)
      return;

    
    if(this._doTermAct())
    {
      return;
    }
    
    this.inPromise=true;
    this.consumePromiseFunc(this).then(result=>{
      //console.log("Consume ok? result",result);
      this.inPromise=false;


      if(this._doTermAct())
      {
        return;
      }

      if(this.cC.size()!=0)
      {
        this.kick();//kick next consumption
      }
    }).catch(e=>{
      


      if(this._doTermAct())
      {
        return;
      }

      console.log("Consume failed... e=",e);
      this.inPromise=false;
    });
  }
}
