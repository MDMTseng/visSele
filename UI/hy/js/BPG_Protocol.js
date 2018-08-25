var BPG_Protocol = {

    raw2header:(ws_evt, offset = 0)=>{

    let str_end_padding = true;
    let BPG_header_L = 7;
    if (( ws_evt.data instanceof ArrayBuffer) && ws_evt.data.byteLength>=BPG_header_L) {
        // var aDataArray = new Float64Array(evt.data);
        // var aDataArray = new Uint8Array(evt.data);
        var headerArray = new Uint8ClampedArray(
          ws_evt.data,offset,offset+BPG_header_L);
        let ret_obj={};


        ret_obj.type = String.fromCharCode(headerArray[0],headerArray[1]);
        ret_obj.prop = headerArray[2];
        ret_obj.length =
          headerArray[3]<<24 | headerArray[4]<<16 |
          headerArray[5]<<8  | headerArray[6];
        return ret_obj;
      }
      return null;
    },
    raw2obj_rawdata:(ws_evt, offset = 0)=>{
      let BPG_header_L = 7;
      let ret_obj = BPG_Protocol.raw2header(ws_evt, offset);
      if(ret_obj==null)return null;

      ret_obj.rawdata = new Uint8ClampedArray(ws_evt.data,
        offset+BPG_header_L,ret_obj.length
      );

      return ret_obj;
    },
    raw2obj:(ws_evt, offset = 0)=>{
      let BPG_header_L = 7;
      let ret_obj = BPG_Protocol.raw2header(ws_evt, offset);
      if(ret_obj==null)return null;

      ret_obj.rawdata = new Uint8ClampedArray(ws_evt.data,
        offset+BPG_header_L,ret_obj.length-1
      );
      let  enc = new TextDecoder("utf-8");
      let str = enc.decode(ret_obj.rawdata);
      ret_obj.data = JSON.parse(str);
      return ret_obj;
    },

    obj2raw:(type,data)=>{
      let str = JSON.stringify(data);
      let str_end_padding = true;
      let BPG_header_L = 7;
      let bbuf = new Uint8Array(BPG_header_L+str.length+((str_end_padding)?1:0));

      bbuf[0] = type.charCodeAt(0);
      bbuf[1] = type.charCodeAt(1);
      bbuf[2] = 0;

      let data_length =str.length+((str_end_padding)?1:0);//Add NULL in the end of the string
      bbuf[3] = data_length>>24;
      bbuf[4] = data_length>>16;
      bbuf[5] = data_length>>8;
      bbuf[6] = data_length;
      let i;
      for( i=BPG_header_L ;i<bbuf.length; i++)
      {
        bbuf[i]= str.charCodeAt(i-BPG_header_L);
      }
      return bbuf;
    }
};



//let binaryX = BPG_Protocol.obj2raw("TC",{a:1,b:{c:2}});
//console.log(BPG_Protocol.raw2obj({data:binaryX.buffer}));
