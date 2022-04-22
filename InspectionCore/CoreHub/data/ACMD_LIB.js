

const Y_Mylar=200;
const Y_Camera=173;
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
  await G(`G01 R11_${-48.5+1}  F100`)
  await G("G92 T0 Z1_0 R11_0")
  CNC_api.enablePING(true);

  await G(`M42 P${camTrigPIN} T0`)
  await G(`M42 P${camTrigPIN} S1`)
  await G(`M42 P${suckPIN} T0`)
  await G(`M42 P${suckPIN} S1`)
  // await G(`M42 P${suckPIN} T1`)
  // await G("G04 P1000")
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
  let YSeq=[-11*2,0]//rangeGen(-0,4*(11),5);
  let YRep=[];
  for(let i=0;i<YSeq.length;i++)
  {
    let repX=await LocaShot("YLOC",Yloc+YSeq[i],trigger_tag,camera_id);
    YRep.push({
      "Y_Loc":Yloc+YSeq[i],
      reports:repX.rules[0].regionInfo
    });
  }

  let R11Seq=rangeGen(-12,12,2);
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


function  intersectPoint( p1, p2, p3, p4)
{
  let intersec={x:0,y:0};
  let denominator;

  let V1 = (p1.x-p2.x);
  let V2 = (p3.x-p4.x);
  let V3 = (p1.y-p2.y);
  let V4 = (p3.y-p4.y);

  denominator = V1* V4 - V3* V2;

  let V12 = (p1.x*p2.y-p1.y*p2.x);
  let V34 = (p3.x*p4.y-p3.y*p4.x);
  intersec.x=( V12 * V2 - V1 * V34 )/denominator;
  intersec.y=( V12 * V4 - V3 * V34 )/denominator;

  return intersec;
}
function R11InfoFindCentre(R11_PtGroup,angleInfo)
{
  //find min angle
  let ang0_index=angleInfo.reduce((mina_idx,ang,cur_idx)=>angleInfo[mina_idx]<ang?mina_idx:cur_idx,0);
  // let N=R11Info.length-1;
  //find max angle
  let angN_index=angleInfo.reduce((mina_idx,ang,cur_idx)=>angleInfo[mina_idx]>ang?mina_idx:cur_idx,0);

  // let angNDiff=angleInfo[angN_index]-angleInfo[ang0_index];

  {
    
    let ang0_pt0=R11_PtGroup[0][ang0_index];
    let ang0_ptN=R11_PtGroup[R11_PtGroup.length-1][ang0_index];

    let angN_pt0=R11_PtGroup[0][angN_index];
    let angN_ptN=R11_PtGroup[R11_PtGroup.length-1][angN_index];

    if(ang0_pt0===null || ang0_ptN===null || angN_pt0===null || angN_ptN===null)
    {
      throw {R11_PtGroup,angleInfo,ang0_pt0,ang0_ptN,angN_pt0,angN_ptN,msg:"R11InfoFindCentre requre points failed"};
    }

    return intersectPoint(ang0_pt0,ang0_ptN,angN_pt0,angN_ptN);
    // console.log(180-180* Math.atan2(ang0_infoVec0.y,ang0_infoVec0.x)/Math.PI);
  }

}


function TryFillUndefInfo(xList, fList,simFactor)
{
  let preDatIdx=-1;
  let nxtDatIdx=-1;
  for(let i=0;i<xList.length;i++)
  {
    if(xList[i]===null)continue;
    
    if(
    preDatIdx==-1 ||
    (xList[preDatIdx]===undefined && xList[i]===undefined) ||
    (xList[preDatIdx]!==undefined && xList[i]!==undefined))//same prop
    {
      preDatIdx = i;
      continue;
    }

    nxtDatIdx=i;

    break;

  }

  if(nxtDatIdx!=-1)
  {
    let newxList=[...xList];
    let undefIdx;
    let valIdx;
    if(newxList[nxtDatIdx]!==undefined)
    {
      undefIdx=preDatIdx;
      valIdx=nxtDatIdx;
    }
    else
    {
      undefIdx=nxtDatIdx;
      valIdx=preDatIdx;
    }

    let vObj=newxList[valIdx];
    if(vObj===null)
    {
      return undefined
    }
    let tarArea=vObj.area;

    let hFactor=0;
    let targetCand=undefined;
    fList[undefIdx].forEach((cand)=>{
      let factor=(tarArea<cand.area)?(tarArea/cand.area):(cand.area/tarArea);
      if(hFactor<factor)
      {
        hFactor=factor;
        targetCand=cand;
      }
    })

    if(hFactor<simFactor)
    {
      newxList[undefIdx]=null;
    }
    else
    {
      newxList[undefIdx]=targetCand;
    }

    // console.log(fList[undefIdx],newxList[valIdx],">>>",targetCand);
    return TryFillUndefInfo(newxList, fList,simFactor);
  }
  else
  {
    return xList;
  }
} 

function R11InfoCalc(R11Info,mmpp,R11_areaTargets,R11_aFactor)
{
  console.log("R11InfoCalc R11Info>>",R11Info);
  let ptGroup=compRepExtractPtGroup(R11Info,R11_areaTargets,R11_aFactor);
  // checkAllPointsValid(ptGroup,R11Info)
  let circCentre = R11InfoFindCentre(ptGroup,R11Info.map(d=>d.R11_angle));


  let RGroup=ptGroup.map(ptg=>{
    let stat=ptg.reduce((infoSum,pt)=>{
      if(pt===null)return infoSum;
      if(circCentre===undefined)return infoSum;
      infoSum.count++;
      infoSum.distSum+=Math.hypot(circCentre.x-pt.x,circCentre.y-pt.y);
      return infoSum;
    },{distSum:0,count:0});
    return stat.distSum/stat.count

  }).map(RinPix=>RinPix*mmpp)



  return {
    mmpp,
    RInfo:RGroup,
    centre:circCentre
  }
}


// let sysInfo=R11InfoCalc(_R11Info,circCentre,mmpp);



function checkAllPointsValid(ptGroup,Reps)
{

  ptGroup.forEach(ptg=>{
    ptg.forEach(pt=>{

      if(pt===undefined || pt===null)
      {
        throw {info:Reps,ptGroup,msg:"Data Collection is not complete"};
      }
    })
  })
  return ptGroup;
}

function compRepExtractPtGroup(Reps,areaTargets,aFactor,doTryFill=true)
{
  let ptGGroup=
  Reps[0].reports.map((srep,idx)=>{

    let xx=Reps.map((sinfo)=>sinfo.reports[idx].components);
    return xx;
  })
  let ptGroup=ptGGroup.map((ptG,idx)=>{
    let areaTarget=areaTargets[idx];
    let firstStageResult=ptG.map(ptComp=>{
      

      let hFactor=0;
      let hFactorIdx=-1;

      for(let i=0;i<ptComp.length;i++)
      {
        let comp=ptComp[i];
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
        FPts=ptComp[hFactorIdx];
      }




      return FPts;
    })

    // console.log(firstStageResult,ptG)
    if(doTryFill)
      firstStageResult=TryFillUndefInfo(firstStageResult, ptG,1-(1-aFactor)/2)

    // console.log(firstStageResult,ptG)

    return firstStageResult;
  });

  return ptGroup;

}


function ReportCalcSysInfo(
  reports, 
  Y_areaTargets=[1031,1031,1000,1231,1231], Y_aFactor=0.7,
  R11_areaTargets=[1000,1000,1000,1000,1000], R11_aFactor=0.7
  )
  // :{
  //   CalibYloc:number,
  //   R11Rep:(CompRepType&{R11_angle:number})[],
  //   YRep:(CompRepType&{Y_Loc:number})[]})
  
  
{
  let YLocInfo=reports.YRep;
  let R11Info=reports.R11Rep;

  let SYSINFO=undefined;
  let ptGroupYvec=compRepExtractPtGroup(YLocInfo,Y_areaTargets,Y_aFactor,false);
  {
    console.log("???????",YLocInfo,ptGroupYvec);
    let pt1 = ptGroupYvec[ptGroupYvec.length-1][0];
    let pt2 = ptGroupYvec[0][ptGroupYvec[0].length-1];
    if(pt1===null ||pt2===null)
    {
      throw {ptGroupYvec,YLocInfo,msg:"Yvec rep requre point is not match"}

    }

    {
      let dist = YLocInfo[YLocInfo.length-1].Y_Loc-YLocInfo[0].Y_Loc;
      let vec = {x:pt2.x-pt1.x,y:pt2.y-pt1.y};
      let vecDist=Math.hypot(vec.x,vec.y);

      console.log(dist,vec,dist/vecDist);

      let mmpp = dist/vecDist;
      vec.x=vec.x*mmpp;
      vec.y=vec.y*mmpp;
      SYSINFO=R11InfoCalc(R11Info,mmpp,R11_areaTargets,R11_aFactor);
      SYSINFO={...SYSINFO,CalibYloc:reports.CalibYloc,mmpp:dist/vecDist,Y_vec:vec}
    }
  }
  
  return SYSINFO;
}


async function CalibSeq( 
  Y_areaTargets=[1031,1031,1000,1231,1231], Y_aFactor=0.7,
  R11_areaTargets=[1000,1000,1000,1000,1000], R11_aFactor=0.7)
{
  let status="RUNNING"
  let fillStyle="#ffAA00"

  let seqRep=undefined;
  let resultInfo=undefined;

  let dbgSwitchIdx=0;

  v.renderHook=
  (ctrl_or_draw,g,canvas_obj,rule)=>{
  try{
    if(ctrl_or_draw==true)return;
    let ctx = g.ctx;
    if(rule.id=="rule1"){
      // canvas_obj.rUtil.drawCross(ctx, v.ptx, 45);
      ctx.fillStyle = fillStyle;
      ctx.font = "100px Arial";
      ctx.fillText(status,100,100)
      if(seqRep)
      {

        // console.log(seqRep)
        dbgSwitchIdx++;



        seqRep.YRep.forEach((rrep,idx)=>{
          let hue=(60*(idx)/seqRep.YRep.length)+0
          // console.log(rrep.reports,hue)

          ctx.strokeStyle = 
          ctx.fillStyle = "hsl("+hue.toFixed(2)+",100%,50%)";
          rrep.reports.forEach(srep=>{

            srep.components.forEach(scomp=>{

              canvas_obj.rUtil.drawCross(ctx, {
                x:scomp.x,
                y:scomp.y,
              }, 5);

              ctx.font = "4px Arial";
              ctx.fillText(scomp.area,scomp.x,scomp.y)

  
            })
          })
          
        })


        seqRep.R11Rep.forEach((rrep,idx)=>{
          let hue=(60*(idx)/seqRep.R11Rep.length)+180
          // console.log(rrep.reports,hue)

          ctx.strokeStyle = 
          ctx.fillStyle = "hsl("+hue.toFixed(2)+",100%,50%)";
          rrep.reports.forEach(srep=>{

            srep.components.forEach(scomp=>{

              canvas_obj.rUtil.drawCross(ctx, {
                x:scomp.x,
                y:scomp.y,
              }, 5);

              ctx.font = "4px Arial";
              ctx.fillText(scomp.area,scomp.x,scomp.y)

  
            })
          })
          
        })




      }
      if(resultInfo)
      {
        canvas_obj.rUtil.drawCross(ctx, {
          x:resultInfo.centre.x,
          y:resultInfo.centre.y,
        }, 100);

        let vecLen=100;
        ctx.beginPath();
        ctx.moveTo(resultInfo.centre.x-vecLen*resultInfo.Y_vec.x, resultInfo.centre.y-vecLen*resultInfo.Y_vec.y);
        ctx.lineTo(resultInfo.centre.x, resultInfo.centre.y);
        //ctx.closePath();
        ctx.stroke();
      }
    }
  }catch(e)
  {
    console.error(e)
  }
    
  }
  progressUpdate(">>")



  status="CalibShotSeq!!"
  progressUpdate(">>")
  let reports=await v.lib.CalibShotSeq(v.lib.Y_Camera,"Locating1","MindVision-040010720303")
  seqRep=reports;

  console.log("reports:",reports)
  status="ReportCalcSysInfo!!"
  progressUpdate(">>")

  v.SYSINFO=
    ReportCalcSysInfo(reports,
      Y_areaTargets,Y_aFactor,
      R11_areaTargets,R11_aFactor)

  if(v.SYSINFO!==undefined)
  {
    status="OK!!"
    fillStyle="#00FF00"
  }
  else
  {
    status="Failed!!"
    fillStyle="#ff0000"
  }
  resultInfo=v.SYSINFO;
  console.log(resultInfo);
  progressUpdate(">>")

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

  let HACK_adj=-R*0.012;//-i*0.12,
  //HACK i*0.1 HACK adjust: the offset might be caused by camera fish eyes
  //do Camera calib after
  
  // console.log(theta360,x+deltaX);
  return {
    Y:y-compY+R+HACK_adj,
    R11:theta360
  }
  // await G(`G01 Y${(y-compY+R).toFixed(3)} R11_${theta360.toFixed(4)} Z1_${z}`)
}

async function SYSGOLOC(y,h,mylar_pt,z=0)
{
  let YR11_Info = SYSGOLOC_GetYR11(y,h,mylar_pt);
  await G(`G01 Y${(YR11_Info.Y).toFixed(3)} R11_${(YR11_Info.R11).toFixed(4)} Z1_${z}`)
}


async function TEST_MYLAR_Check(YLoc=Y_Camera,postTakeCB=undefined)
{
  // await G("G01 Y100 R11_0 F250 ACC200 DEA200")

  // SET_SYS_INFO()
  let Trig_tag="TTag_2"
  await v.lib.LocaShot_prep(
  "R11LOC",Trig_tag,"MindVision-040010720303")



  await G(`G01 Y${YLoc-2} R11_0`)
  await G(`G01 Y${YLoc} Z1_${Z_Camera} R11_0`)

  await v.lib.LocaShot_take(
    "R11LOC")

  await G(`G04 P50`)
  if(postTakeCB!==undefined)
    await postTakeCB()




  let report = await v.lib.LocaShot_wait(
  "R11LOC")
  
  let CUR_MYLAR_STATE=report.rules[0].regionInfo;

  let pts=componentsPtFilter(CUR_MYLAR_STATE,[10000,10000,10000,8000,5000],0.7);

  console.log(pts,"<<<<<<",CUR_MYLAR_STATE)

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

async function MylarWorkCycle(y,x,speed=280)
{
  let preZ=-0
  let pickZ=-25
  let PillarY=y
  let PillarX=-x

  let mylar_pts=
    await TEST_MYLAR_Check(v.SYSINFO.CalibYloc,async()=>{

      await G(`G01 Z1_0`)
      await G(`G01 Y${PillarY} F${speed}`)

  })

  await G(`G01 Y${PillarY}`)
  await G(`G01 Z1_${preZ}`)
  console.log("----------",mylar_pts);
  // ddd
  for(let i=0;i<3;i++)
  {
    let mylar_pt=mylar_pts[i]
    if(mylar_pt===null||mylar_pt===undefined)continue;
    console.log(">>>",i,mylar_pt);
    let YR11Info=SYSGOLOC_GetYR11(
    PillarY,//-i*0.12,//HACK i*0.1 HACK calib: the offset might be caused by camera fish eyes
    PillarX,
    mylar_pt)

    console.log(YR11Info);

    let Trig_tag="TTag_2"
    // await v.lib.LocaShot_prep(
    // "R11LOC",Trig_tag,"MindVision-040010720303")






    await G(`G01 Y${YR11Info.Y.toFixed(4)} R11_${YR11Info.R11.toFixed(4)} Z1_${preZ}`)

    // await v.lib.LocaShot_take(
    //   "R11LOC")
    
    //   let report = await v.lib.LocaShot_wait(
    //   "R11LOC")


    await waitUIInfo({
      title:"AAAA"})
    await G(`G01 Z1_${pickZ} ACC300 DEA300`)


    await G(`G01 Z1_${preZ} ACC300 DEA300`)

  }
  await G(`G01 Z1_${0}`)

  // let pillar_pts=await TEST_Pillar_Check()


}

;({
  SYS_ZERO,
  G_PickOn,
  LocaShot,
  LocaShot_prep,
  LocaShot_take,
  LocaShot_wait,
  CalibShotSeq,CalibSeq,
  SYSGO,SYSGOLOC,SYSGOLOC_GetYR11,
  delay,
  TEST_MYLAR_Check,
  MylarWorkCycle,
  Y_Mylar,
  Y_Camera,
  Z_Camera,
  Y_Pillar,
  _HACK_SET_SYS_INFO

})