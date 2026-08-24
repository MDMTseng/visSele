import WebSocket from 'ws';
const pat=(process.argv[2]||'subscribe').toLowerCase();
const ws=new WebSocket('ws://127.0.0.1:4091');let hits=0;
ws.on('open',()=>ws.send(JSON.stringify({type:'subscribe',id:'1',minSeverityNumber:0,includeEphemeral:true,backlog:{tailN:60000}})));
ws.on('message',(d)=>{let m;try{m=JSON.parse(d.toString())}catch{return}
 const items=m.items||(m.type==='log'?[m]:[]);
 for(const it of items){const b=String(it.body||'');if(b.toLowerCase().includes(pat)){hits++;if(hits<=12)console.log(b.slice(0,160));}}});
setTimeout(()=>{console.log('### hits:',hits);process.exit(0)},20000);
