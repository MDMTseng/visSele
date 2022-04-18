

const Y_Mylar=200;
const Y_Camera=190;
const Z_Camera=-30;

const Y_Pillar=100;


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



let LocaShot_prep=async (skey,trigger_tag,camera_id,soft_trigger=false,mock_img_path=undefined)=>{

  reportWait_reg(skey,trigger_tag,camera_id);
  await api.send_P(
    "CM",0,{
      type:"trigger",
      id:camera_id,
      trigger_tag:trigger_tag,
      soft_trigger:soft_trigger,
      img_path:mock_img_path,
      // img_path:"data/TEST_DEF/rule1_Locating1/KKK2.png",
      trigger_id:1,
      // img_path:camTrigInfo.img_path,
      // channel_id:50201
    })
  
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

  await G(`G01 Y${Yloc-10} Z1_${Z_Camera} R11_0`)
  let YSeq=[0,44]//rangeGen(-0,4*(11),5);
  let YRep=[];
  for(let i=0;i<YSeq.length;i++)
  {
    let repX=await LocaShot("YLOC",Yloc+YSeq[i],trigger_tag,camera_id);
    YRep.push({
      "Y_Loc":Yloc+YSeq[i],
      reports:repX.rules[0].regionInfo
    });
  }

  let R11Seq=rangeGen(-8,8,5);
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
    CalibYloc:Yloc,
    YRep,R11Rep
  }
}


async function SYSGO(y,x,nozzleIdx)
{
  // v.SYSINFO.RInfo=[9.8,21,32,43,54].map(v=>v*(10.93/11));
  let comp=SysInfoCalcCompensation_(v.SYSINFO,x,nozzleIdx)
  let theta360=(comp.theda*180/Math.PI).toFixed(4);
  let compY=comp.X;
  
  await G(`G01 Y${(y-compY).toFixed(3)} R11_${theta360} F300`)
}


function CALC_ADJ(sysInfo,h,InspLoc)
{
  let R=sysInfo.RInfo[nozzleIdx];
  
  let theda=Math.asin(h/R);
  let X=R*(1-Math.cos(theda));
  return {
    theda,
    X
  }
}



async function delay(ms=1000)
{
  return new Promise((resolve,reject)=>setTimeout(resolve,ms))
}


function closestPointOnLine(line, point)
{

  let line_={...line} ;
  

  let normalizeFactor = Math.hypot(line_.vx,line_.vy);
  line_.vx/=normalizeFactor;
  line_.vy/=normalizeFactor;

  let point_={...point};

  point_.x-=line_.x;
  point_.y-=line_.y;

  let distX= line_.vx * point_.x + line_.vy * point_.y;
  let distY=-line_.vy * point_.x + line_.vx * point_.y;

  return {
    x:line_.x+distX*line_.vx,
    y:line_.y+distX*line_.vy,
    distX,distY
  };
}

let _H_SYS_INFO_ = {
  "mmpp": 0.04207745254511293,
  "RInfo": [
      9.42274385720468,
      20.249539799638143,
      31.115348454124817,
      42.043108724196046,
      52.95121081483568
  ],
  "centre": {
      "x": 1861.75522223913,
      "y": 884.6027328565302
  },
  "CalibYloc": 190,
  "Y_vec": {
      "x": 43.97093790964301,
      "y": 1.5989431967142913
  }
}


let mylarPts=[
  {
      "area": 6281,
      "x": 1630,
      "y": 872
  },
  {
      "area": 8102,
      "x": 1370,
      "y": 885
  },
  {
      "area": 8372,
      "x": 1120,
      "y": 867
  },
  {
      "area": 8804,
      "x": 862,
      "y": 857
  },
  {
      "area": 9219,
      "x": 602,
      "y": 856
  }
];

function _HACK_SET_SYS_INFO(sysInfo=_H_SYS_INFO_)
{
  v.SYSINFO=sysInfo;
}

function getRandom(min,max){
  return Math.floor(Math.random()*max)+min;
};




function componentsPtFilter(componentSet,areaTargets,aFactor)
{
  let ptGroup=componentSet.map((compsInfo,idx)=>{
    let components=compsInfo.components;
    let areaTarget=areaTargets[idx];
    // console.log(components)

    let hFactor=0;
    let hFactorIdx=-1;
    for(let i=0;i<components.length;i++)
    {
      let comp=components[i];
      if(comp===null)
      {
        throw {info:Reps,ptComp,index:i,msg:"ptComp[i] is null"};
      }
      let factor = (comp.area<areaTarget)?(comp.area/areaTarget):(areaTarget/comp.area);
      if(hFactor<factor)
      {
        hFactor=factor;
        hFactorIdx=i;
      }
    }
    let FPts=null;
    if(hFactor>aFactor)
    {
      FPts=components[hFactorIdx];
    }


    // console.log(firstStageResult,ptG)

    return FPts;
  });

  return ptGroup;

}


function SYSGOLOC_GetYR11(y,h,mylar_pt)
{

  let cpL = closestPointOnLine({
    x:v.SYSINFO.centre.x,
    y:v.SYSINFO.centre.y,
    vx:v.SYSINFO.Y_vec.x,
    vy:v.SYSINFO.Y_vec.y,
  }, mylar_pt);

  let dist_C2Pt=Math.hypot(v.SYSINFO.centre.x-mylar_pt.x,v.SYSINFO.centre.y-mylar_pt.y);
  // let dist_Pc2Pt=Math.abs(cpL.distY);
  let theda_PyCPc=Math.atan2(cpL.distY,-cpL.distX);
  // console.log(
  //   mylar_pt,
  //   "<<<<<<",
  //   v.SYSINFO,dist_C2Pt*v.SYSINFO.mmpp,
  //   theda_PyCPc*180/Math.PI)
  // console.log(cpL)

  let R=dist_C2Pt*v.SYSINFO.mmpp;
  
  let theda=Math.asin(h/R)+theda_PyCPc;
  let theta360=(theda*180/Math.PI);
  let compY=R*(1-Math.cos(theda));

  // console.log(theta360,x+deltaX);
  return {
    Y:y-compY+R,
    R11:theta360
  }
  // await G(`G01 Y${(y-compY+R).toFixed(3)} R11_${theta360.toFixed(4)} Z1_${z}`)
}

async function SYSGOLOC(y,h,mylar_pt,z=0)
{
  let YR11_Info = SYSGOLOC_GetYR11(y,h,mylar_pt);
  await G(`G01 Y${(YR11_Info.Y).toFixed(3)} R11_${(YR11_Info.R11).toFixed(4)} Z1_${z}`)
}


async function TEST_MYLAR_Check()
{
  // await G("G01 Y100 R11_0 F250 ACC200 DEA200")


  await G(`G01 Y${Y_Camera-10} Z1_${0} R11_0 F250`)
  await G(`G01 Y${Y_Camera} Z1_${Z_Camera} R11_0 F250`)


  // SET_SYS_INFO()
  let Trig_tag="TTag_2"
  await v.lib.LocaShot_prep(
  "R11LOC",Trig_tag,"MindVision-040010720303")





  await v.lib.LocaShot_take(
  "R11LOC")

  let report = await v.lib.LocaShot_wait(
  "R11LOC")
  
  let CUR_MYLAR_STATE=report.rules[0].regionInfo;

  let pts=componentsPtFilter(CUR_MYLAR_STATE,[7000,8000,9000,9000,9000],0.7);

  // console.log(pts,"<<<<<<")

  return pts;
  // for(let pt of pts)
  // {


  //   await v.lib.LocaShot_prep(
  //     "R11LOC",Trig_tag,"MindVision-040010720303")
  
  

  //   await SYSGOLOC(190,-5,pt);


  //   await v.lib.LocaShot_take(
  //     "R11LOC")
  
  //   let report = await v.lib.LocaShot_wait(
  //   "R11LOC")
    

  // }
}

async function MylarWorkCycle()
{
  let mylar_pts=
  await TEST_MYLAR_Check()
  let preZ=-50
  let pickZ=-100
  let PillarY=50
  let PillarX=-5

  for(let mylar_pt of mylar_pts)
  {

  let YR11Info=SYSGOLOC_GetYR11(
  PillarY,
  PillarX,
  mylar_pt)


  await G(`G01 Y${YR11Info.Y} Z1_${YR11Info.Z1} Z1_${preZ}`)

  await G(`G01 Z1_${pickZ}`)

  await G(`G01 Z1_${preZ}`)

  }

  let pillar_pts=await TEST_Pillar_Check()


}

;({
  SYS_ZERO,
  G_PickOn,
  LocaShot,
  LocaShot_prep,
  LocaShot_take,
  LocaShot_wait,
  CalibShotSeq,
  SYSGO,SYSGOLOC,SYSGOLOC_GetYR11,
  delay,
  TEST_MYLAR_Check,
  MylarWorkCycle,
  Y_Mylar,
  Y_Camera,
  Y_Pillar,
  _HACK_SET_SYS_INFO

})