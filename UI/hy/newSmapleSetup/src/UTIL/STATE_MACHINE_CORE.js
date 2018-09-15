
class STATE_MACHINE_CORE
{
  
  constructor(target_obj, state_table) {
    this.state_table = state_table;
    this.target_obj = target_obj;

    this.state=state_table[0][0];
    this.p_state=null;
    this._invoke_(null,"_ENTER");
  }

  _invoke_(event,TYPE_Str, strict=false)
  {
    //console.log(this.state+TYPE_Str);
    let func = this.target_obj[this.state+TYPE_Str];
    if(func === undefined)
    {
        let errorMsg = "Error: Func:"+this.state+TYPE_Str+" cannot be found!!!";
        if(strict)
            throw errorMsg;
        console.log(errorMsg);
        return;
    }
    return this.target_obj[this.state+TYPE_Str](event,this);
  }

  input(event) {
    for(let i=0;i<this.state_table.length;i++)
    {
        if(this.state_table[i][0]!=this.state)
            continue;

        if(this.state_table[i][1]!=event.type)
            continue;
        let idx = this._invoke_(event,"_"+event.type+"_EVENT",true);


        if(idx  === undefined)
        {
            throw "Error: event must return next state idx";
        }

        if(idx+2>=this.state_table[i].length)
        {
            throw "Error: next state id:"+idx+" out of possible next state";
        }

        let nex_state = this.state_table[i][idx+2];

        //console.log(this.state_table[i],idx+2,nex_state);
        if(nex_state!=this.state)
        {
            let p_state = this.p_state;

            this._invoke_(event,"_EXIT");
            this.p_state = this.state;
            this.state = nex_state;
            this._invoke_(event,"_ENTER");

        }
        return;
    }

    throw  ("Error: state:"+this.state +" with event:"+event.type+ " is not in the map");
  }
  
}





class FSM_TrafficLight
{
  
  constructor(target_obj, state_table) {
    console.log(STATE_MACHINE_CORE);
    this.SM=new STATE_MACHINE_CORE(this,[//example for 
        ["RED",    "RESET",        "RED"],
        ["RED",    "CROSS_CLEAR",  "RED","YELLOW"],
        ["RED",    "TIMES_UP",     "RED","YELLOW"],

        ["YELLOW", "TIMES_UP",     "YELLOW","GREEN"],

        ["GREEN",  "TIMES_UP",     "RED"],
        ]);

    this.SM.input({type:"TIMES_UP"});
  }

  RED_ENTER()
  {
    console.log("Hello......");
  }

  RED_EXIT()
  {
    
  }

  RED_TIMES_UP_EVENT(event)
  {
    return 1;
  }

  YELLOW_ENTER()
  {

    this.SM.input({type:"TIMES_UP"});
  }

  YELLOW_EXIT()
  {
    
  }
  
  YELLOW_TIMES_UP_EVENT()
  {
    return 1;
    
  }


  GREEN_ENTER()
  {
    this.SM.input({type:"TIMES_UP"});

  }

  GREEN_EXIT()
  {
    
  }
  
  GREEN_TIMES_UP_EVENT()
  {
    
    return 0;
  }

}

export default STATE_MACHINE_CORE