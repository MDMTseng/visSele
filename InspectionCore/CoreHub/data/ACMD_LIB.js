



let G_PickOn=async(pos_mm,r11, speed_mmps,  pickPin_suck, pickPin_blow,pickup=true,prelocating=false,locatingSpeed=NaN,locatingAcc=NaN)=>{
  let GCODE_Arr=[];

  let headPoseDown=0;

  headPoseDown=-110;

  let pinPreTrigger=60;//early pick
  if(pickup==false)
  {
    pinPreTrigger=80;
  }
  //less means earlier


  if(locatingSpeed!=locatingSpeed)locatingSpeed=speed_mmps;
  let locatingSpeed_CodeP=" F"+locatingSpeed.toFixed(2);
  let locatingAcc_CodeP=(locatingAcc==locatingAcc)?" ACC"+(locatingAcc*0.8).toFixed(2)+" DEA-"+locatingAcc.toFixed(2):"";
  

  pos_mm=pos_mm.toFixed(2);
  if(prelocating)
  {
    await G("G01 R11_"+r11+" Y"+pos_mm+" Z1_"+(headPoseDown*pinPreTrigger/100).toFixed(2)+locatingSpeed_CodeP+locatingAcc_CodeP);  
  }
  else
  {
    await G("G01 Y"+pos_mm+" Z1_"+0+locatingSpeed_CodeP+locatingAcc_CodeP);
    await G("G04 P"+0);
    await G("G01 R11_"+r11+" Z1_"+(headPoseDown*pinPreTrigger/100).toFixed(2)+" F"+speed_mmps);  
  }




    
  // await G("M42 P"+pickPin_suck+" S"+(pickup?1:0) );
  // await G("M42 P"+pickPin_blow+" S"+(pickup?0:1) );
  let ZSpeed=speed_mmps/2;
  await G("G01 Z1_"+headPoseDown+" F"+ZSpeed);
  let pinKeepDelay_ms=10;
  await G("G04 P"+pinKeepDelay_ms);
  // await G("M42 P"+pickPin_blow+" S0" );
  if(pickup)
    await G("G01 Z1_-23"+" F"+speed_mmps+" ACC"+(locatingAcc*0.3).toFixed(2));//slow accleration pick up
  else 
    await G("G01 Z1_"+(headPoseDown+60)+" F"+ZSpeed);


  return GCODE_Arr;

}

let camTrigPIN=2;
let suckPIN=25;

let SYS_ZERO=async ()=>{


  CNC_api.enablePING(false);
  await G("G28")
  await G("G01 R11_-48.5")
  await G("G92 T0 Z1_0 R11_0")
  CNC_api.enablePING(true);

  await G(`M42 P${camTrigPIN} S1`)
  await G(`M42 P${suckPIN} S1`)
  await G(`M42 P${suckPIN} T1`)
  await G("G04 P1000")
  await G(`M42 P${suckPIN} T0`)
}

let LocaShot_prep=async (skey,trigger_tag,camera_id)=>{
  await api.send_P(
    "CM",0,{
      type:"trigger",
      id:camera_id,
      trigger_tag:trigger_tag,
      // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
      trigger_id:1,
      // img_path:camTrigInfo.img_path,
      // channel_id:50201
    })

  reportWait_reg(skey,trigger_tag,camera_id);
  
}


let LocaShot_take=async (skey)=>{
  await G(`M42 P${camTrigPIN} T1`)
  await G("G04 P100")
  await G(`M42 P${camTrigPIN} T0`)
}

let LocaShot_wait=(skey)=>{
  return reportWait(skey);
}

let LocaShot=async (skey,Yloc,trigger_tag,camera_id,Yloc_stage=undefined)=>{

  await api.send_P(
    "CM",0,{
      type:"trigger",
      id:camera_id,
      trigger_tag:trigger_tag,
      // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
      trigger_id:1,
      // img_path:camTrigInfo.img_path,
      // channel_id:50201
    })

  reportWait_reg(skey,trigger_tag,camera_id);
  
  // await G(`G01 Y${Yloc+0.1}`)
  await G(`G01 Y${Yloc}`)


  await G(`M42 P${camTrigPIN} T1`)
  await G("G04 P100")
  await G(`M42 P${camTrigPIN} T0`)

  if(Yloc_stage)
  {
    await G(`G01 Y${Yloc_stage}`)
  }
  return await reportWait(skey);

}

function rangeGen(from,to,count)
{
  let seq=[];
  for(let i=0;i<count;i++)
  {
    seq.push(from+(to-from)*(i/(count-1)));
  }
  return seq
}

let CalibShotSeq=async (Yloc,trigger_tag,camera_id)=>{

  await G(`G01 Z1_-30 R11_0`)
  let YSeq=rangeGen(-2,2,5);
  let YRep=[];
  for(let i=0;i<YSeq.length;i++)
  {
    let repX=await LocaShot("YLOC",Yloc+YSeq[i],trigger_tag,camera_id);
    YRep.push({
      "Y_Loc":Yloc+YSeq[i],
      reports:repX.rules[0].regionInfo
    });
  }

  let R11Seq=rangeGen(-5,5,5);
  let R11Rep=[];
  for(let i=0;i<R11Seq.length;i++)
  {

    await G(`G01 R11_${R11Seq[i]}`)
    let repX=await LocaShot("R11LOC",Yloc,trigger_tag,camera_id);
    R11Rep.push({
      "R11_angle":R11Seq[i],
      reports:repX.rules[0].regionInfo
    });
  }


  await G(`G01 R11_0`)

  return {
    YRep,R11Rep
  }
}


async function SYSGO(y,x,nozzleIdx)
{
  // v.SYSINFO.RInfo=[9.8,21,32,43,54].map(v=>v*(10.93/11));
  let comp=SysInfoCalcCompensation_(v.SYSINFO,x,nozzleIdx)
  let theta360=(comp.theda*180/Math.PI).toFixed(3);
  let compY=comp.X;
  
  await G(`G01 Y${(y-compY).toFixed(3)} R11_${theta360}`)
}




;({
  SYS_ZERO,
  G_PickOn,
  LocaShot,
  LocaShot_prep,
  LocaShot_take,
  LocaShot_wait,
  CalibShotSeq,
  SYSGO,
  Y_Mylar:200,
  Y_Camera:180,
  Y_Pillar:100,

})