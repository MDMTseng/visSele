


class BPG_WebSocket
{

    constructor(Url)
    {
        this.url=Url;
        this.websocket= new WebSocket(this.url);

        this.websocket.binaryType = "arraybuffer"; 

        this.websocket.onopen = this.onopen.bind(this);
        this.websocket.onclose = this.onclose.bind(this);
        this.websocket.onmessage = this.onmessage.bind(this);
        this.websocket.onerror = this.onerror.bind(this);

        this.onopen_bk=(evt)=>{};
        this.onclose_bk=(evt)=>{};
        this.onmessage_bk=(evt)=>{};
        this.onerror_bk=(evt)=>{};

    }

    onopen(evt){
        console.log(this.url,"onopen");
        this.onopen_bk(evt);
    } 
    onclose(evt){
        
        console.log(this.url,"onclose");
        this.onclose_bk(evt);
    }
    send(OBJ){
        this.websocket.send(OBJ);
    }

    onmessage(evt){
        console.log(">>>>");
        console.log(this.url,"onmessage");
        this.onmessage_bk(evt);
    }

    onerror(evt){
        console.log(this.url,"onerror");
        this.onerror_bk(evt);
        
    }

}

export default {BPG_WebSocket}
