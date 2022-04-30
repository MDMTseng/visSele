

const Y_Mylar=373-7;
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
  await G(`G01 R11_${-48.5+1-2.1}  F100`)
  await G("G92 Y0 Z1_0 R11_0")
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

let LocaShot=async (skey,trigger_tag,camera_id,Yloc_stage=undefined)=>{

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
  await G(`M42 P${camTrigPIN} T1`)
  await G("G04 P20")
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
  // await G(`G01 F10`)
  let YSeq=[-11*2,0]//rangeGen(-0,4*(11),5);
  let YRep=[];

  let settleTime=50;
  for(let i=0;i<YSeq.length;i++)
  {
    let tarY=Yloc+YSeq[i];
    await G(`G01 Y${tarY-4}`)
    await G(`G01 Y${tarY} DEA50`)
    if(settleTime>0)await G("G04 P"+settleTime)
    let repX=await LocaShot("YLOC",trigger_tag,camera_id);
    YRep.push({
      "Y_Loc":tarY,
      reports:repX.rules[0].regionInfo
    });
  }

  let R11Seq=[-31,-30,-1,0,29,30];
  let R11Rep=[];
  for(let i=0;i<R11Seq.length;i++)
  {

    await G(`G01 R11_${R11Seq[i]} DEA100`)

    if(settleTime>0)await G("G04 P"+settleTime)
    let repX=await LocaShot("R11LOC",trigger_tag,camera_id);
    R11Rep.push({
      "R11_angle":R11Seq[i],
      reports:repX.rules[0].regionInfo
    });
  }


  await G(`G01 R11_0`)

  return {
    CalibYloc:Yloc,
    CalibZloc:Z_Camera,

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
function circleCenter(p1, p2, p3) {
  let ax = (p1.x + p2.x) / 2;
  let ay = (p1.y + p2.y) / 2;
  let ux = (p1.y - p2.y);
  let uy = (p2.x - p1.x);
  let bx = (p2.x + p3.x) / 2;
  let by = (p2.y + p3.y) / 2;
  let vx = (p2.y - p3.y);
  let vy = (p3.x - p2.x);
  let dx = ax - bx;
  let dy = ay - by;
  let vu = vx * uy - vy * ux;
  if (vu == 0)
      return false; // Points are collinear, so no unique solution
  let g = (dx * uy - dy * ux) / vu;
  return {x:bx + g * vx,y:by + g * vy}
}


function R11InfoFindCentre(R11_PtGroup,angleInfo)
{
  // console.log(R11_PtGroup,angleInfo)
  // dddd

  let centre1;
  {
    let outerRingPts = R11_PtGroup[1];
    let centre_1= circleCenter(outerRingPts[0], outerRingPts[2], outerRingPts[4]);
    let centre_2= circleCenter(outerRingPts[1], outerRingPts[3], outerRingPts[5]);
    centre1={x:(centre_1.x+centre_2.x)/2,y:(centre_1.y+centre_2.y)/2};
    console.log(centre1);
  }

  let centre2;
  {
    let outerRingPts = R11_PtGroup[2];//outer ring
    let centre_1= circleCenter(outerRingPts[0], outerRingPts[2], outerRingPts[4]);
    let centre_2= circleCenter(outerRingPts[1], outerRingPts[3], outerRingPts[5]);
    centre2={x:(centre_1.x+centre_2.x)/2,y:(centre_1.y+centre_2.y)/2};
    console.log(centre2);
  }
  // console.log(outerRingPts,X)
  return centre2;//{x:(centre1.x+centre2.x)/2,y:(centre1.y+centre2.y)/2};

}
function R11InfoFindCentre_(R11_PtGroup,angleInfo)
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


v.renderHook=(ctrl_or_draw,g,canvas_obj,rule)=>{
  if(v.renderHook_set===undefined)return;
  Object.keys(v.renderHook_set).forEach(key=>{
    v.renderHook_set[key](ctrl_or_draw,g,canvas_obj,rule);
  })
}
v.renderHook_set=[];

async function CalibSeq( 
  Y_areaTargets=[1031,1031,1000,1231,1231], Y_aFactor=0.7,
  R11_areaTargets=[1000,1000,1000,1000,1000], R11_aFactor=0.7,cycleCount=2)
{
  let p_resultInfo=undefined;
  if(cycleCount>1)
  {
    p_resultInfo = await CalibSeq(Y_areaTargets,Y_aFactor,R11_areaTargets,R11_aFactor,cycleCount-1);
    console.log(p_resultInfo)
  }

  let preCalib=undefined


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

  resultInfo=
    ReportCalcSysInfo(reports,
      Y_areaTargets,Y_aFactor,
      R11_areaTargets,R11_aFactor)

  
  if(cycleCount>1)
  {
    let alpha=1/cycleCount;
    console.log(resultInfo,p_resultInfo);
    resultInfo.mmpp+=(1-alpha)*(p_resultInfo.mmpp-resultInfo.mmpp);
    resultInfo.RInfo[0]+=(1-alpha)*(p_resultInfo.RInfo[0]-resultInfo.RInfo[0]);
    resultInfo.RInfo[1]+=(1-alpha)*(p_resultInfo.RInfo[1]-resultInfo.RInfo[1]);
    resultInfo.RInfo[2]+=(1-alpha)*(p_resultInfo.RInfo[2]-resultInfo.RInfo[2]);

    resultInfo.centre.x+=(1-alpha)*(p_resultInfo.centre.x-resultInfo.centre.x);
    resultInfo.centre.y+=(1-alpha)*(p_resultInfo.centre.y-resultInfo.centre.y);


    resultInfo.Y_vec.x+=(1-alpha)*(p_resultInfo.Y_vec.x-resultInfo.Y_vec.x);
    resultInfo.Y_vec.y+=(1-alpha)*(p_resultInfo.Y_vec.y-resultInfo.Y_vec.y);

  }

  if(resultInfo!==undefined)
  {
    status="OK!!"
    fillStyle="#00FF00"
  }
  else
  {
    status="Failed!!"
    fillStyle="#ff0000"
  }
  // resultInfo=v.SYSINFO;

  if(preCalib===undefined)
  {
    preCalib=JSON.parse(JSON.stringify(resultInfo))
  }
  resultInfo.golden=preCalib;
  resultInfo.golden_offset={
    x:(preCalib.centre.x-resultInfo.centre.x)*resultInfo.mmpp,
    y:(preCalib.centre.y-resultInfo.centre.y)*resultInfo.mmpp,
  }
  console.log(resultInfo);

  progressUpdate(">>")
  return resultInfo;
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

  y+=v.SYSINFO.golden_offset.x;
  h+=v.SYSINFO.golden_offset.y;
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


  let theda=Math.asin(-h/R)+theda_PyCPc;
  let theta360=(theda*180/Math.PI);
  let compY=R*(1-Math.cos(theda));

  let HACK_adj=0;//-R*0.012;//-i*0.12,
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


async function TEST_MYLAR_Check(
  YLoc=Y_Camera,
  cameraZ=Z_Camera,
  Trig_tag="TTag_2",
  postTakeCB=undefined,
  areaTargets=[10000,10000,10000,8000,5000], aFactor=0.7)
{
  // await G("G01 Y100 R11_0 F250 ACC200 DEA200")
  let delay=20;
  // SET_SYS_INFO()
  await v.lib.LocaShot_prep(
  "R11LOC",Trig_tag,"MindVision-040010720303")



  await G(`G01 Y${YLoc-2} R11_0`)
  await G(`G01 Y${YLoc} Z1_${cameraZ} R11_0`)

  await v.lib.LocaShot_take(
    "R11LOC")

  await G(`G04 P${delay}`)
  if(postTakeCB!==undefined)
    await postTakeCB()




  let report = await v.lib.LocaShot_wait(
  "R11LOC")
  
  let CUR_MYLAR_STATE=report.rules[0].regionInfo;

  let pts=componentsPtFilter(CUR_MYLAR_STATE,areaTargets,aFactor);

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

async function Motion_MylarPick(preY,Y,prepareZ,Z,pickPin=25,pickWait=50)
{

  await G(`G01 Z1_0 R11_0`)
  if(preY!==undefined)
  {

  }
  else
  {
    await G(`G01 Y${Y} `)
    await G(`G01 Z1_${Z}`)  
  }


  await G(`M42 P${pickPin} T1`)
  await G(`G04 P${pickWait}`)

  await G(`G01 Z1_0`)

}



let Mylar_YLoc=Y_Mylar;
let Mylar_YSeg=5.5;
let Mylar_Z=-40-1;
let Mylar_PreZ=Mylar_Z/3;

async function MylarWorkCycle(cycleIndex,debugMode=false)
{

  let pii=0;//cycleIndex;
  if(cycleIndex==0){
    pii=6
  }else if (cycleIndex==1){
    pii=5
  }else if (cycleIndex==2){
    pii=1
  }else{
    pii=0
  }
  let curY=Mylar_YLoc+Mylar_YSeg*(pii)+v.SYSINFO.golden_offset.x


  // x+=v.SYSINFO.golden_offset.x;
  // y+=v.SYSINFO.golden_offset.y;
  //Pick mylar------------------
  {

    await G(`G01 Z1_${Mylar_PreZ} R11_0`)
    await G(`G01 Y${curY}`)

    if(debugMode)await waitUserCheck("MYLAR_Pick")
    await G(`G01 Z1_${Mylar_Z} DEA100`)
    await G("G04 P10")
    await G(`M42 P${25} T1`)
    await G("G04 P20")
    let prepN=3;
    let middleZ=(Mylar_Z*prepN+Mylar_PreZ)/(prepN+1);
    await G(`G01 Z1_${middleZ.toFixed(4)} ACC100`)//pick up slower
    await G(`G01 Z1_${Mylar_PreZ.toFixed(4)}`)//pick up slower


  }







  //Check mylar------------------

  let speed=320;

  let preZ=-30
  let heightDiff=17+5;
  let pickZ=-69-17+heightDiff
  let PillarY=50
  let PillarX=0

  let mylar_pts=
    await TEST_MYLAR_Check(
      v.SYSINFO.CalibYloc,
      Z_Camera,
      "TTag_2",
      async()=>{

        if(debugMode)await waitUserCheck("MYLAR_Check")
        // await G(`G01 Z1_0`)

        let N=20;
        await G(`G01 Y${((PillarY*N+v.SYSINFO.CalibYloc)/(N+1)).toFixed(3)} F${speed}`)

      },
      [10000,10000,10000],
      0.7)

  await G(`G01 Z1_${preZ}`)
  console.log("----------",mylar_pts);




  //pick pillar if available------------------
  let maxSkipCount=mylar_pts.length;

  let HACK_Offset= v.HACK_Offset //[{y:-0,x:-0.34230},{y:-0.11,x:-0.3671},{y:-0-0.08,x:-0.30522-0.076},]
  // [{y:0.15,x:0.3},{y:0.0,x:0.0},{y:0.0,x:0.0}]
  
  if(HACK_Offset===undefined)
    HACK_Offset=[{y:0,x:0},{y:0.0,x:0.0},{y:0.0,x:0.0}];



  for(let i=0;i<mylar_pts.length;i++)
  {
    let mylar_pt=mylar_pts[i]
    if(mylar_pt===null||mylar_pt===undefined)
    {
      maxSkipCount--;
      continue;
    }
    console.log(">>>",i,mylar_pt);
    let YR11Info=SYSGOLOC_GetYR11(
    PillarY+((HACK_Offset===undefined)?0:HACK_Offset[i].y),//-i*0.12,//HACK i*0.1 HACK calib: the offset might be caused by camera fish eyes
    PillarX+((HACK_Offset===undefined)?0:HACK_Offset[i].x),
    mylar_pt)

    console.log(YR11Info);

    await G(`G01 Y${YR11Info.Y.toFixed(4)} R11_${YR11Info.R11.toFixed(4)} Z1_${preZ}`)

    if(debugMode)await waitUserCheck("Wait for pick",``)

    await G(`G04 P50`)
    await G(`G01 Z1_${pickZ} DEA500`)


    await G(`G01 Z1_${preZ} ACC200`)

  }
  await G(`G01 Z1_${0}`)

  if(maxSkipCount==0)return;//early quit


  let resultSecY=250;

  //Check pillar location------------------

  let pillar_pts=
    await TEST_MYLAR_Check(v.SYSINFO.CalibYloc,
      Z_Camera+heightDiff,//lift up a bit for pillar height
      "PillarCheck",
      async()=>{

        if(debugMode)await waitUserCheck("C",``)
        // await G(`G01 Z1_0`)
        await G(`G01 Y${resultSecY} Z1_0 R11_0`)//go the the result section in advance
      },
      [1200,1200,1200],
      0.7);

  console.log("----------",mylar_pts,pillar_pts);
  // let pillar_pts=await TEST_Pillar_Check()

  let isOK=true;
  let HACK_INSP_Offset=[
    {
      "x": 0,
      "y": 0,
    },
    {
      "x": 0,
      "y": -0,
    },
    {
      "x": 0,
      "y": -0,
    }]
  HACK_INSP_Offset=[
    {
      "x": 0+0.15235,
      "y": 0.0761,
    },
    {
      "x": 0.037713,
      "y": -0.113140,
    },
    {
      "x": -0.03771,
      "y": -0.11314,
    }]
  let distsInfo=[];
  {//check pillar if the location error within tolerable range
    let tolerance_mm=0.15;
    for(let i=0;i<mylar_pts.length;i++)
    {
      let mylar_pt=mylar_pts[i]
      let pillar_pt=pillar_pts[i]
      if(mylar_pt===null||mylar_pt===undefined)
      {

        distsInfo.push(null);
        continue;
      }
      if(pillar_pt===null||pillar_pt===undefined){
        isOK=false;
        distsInfo.push(null);
        continue;
      }
      
      let diffInfo={
        x:(pillar_pt.x-mylar_pt.x)*v.SYSINFO.mmpp-HACK_INSP_Offset[i].x,
        y:(pillar_pt.y-mylar_pt.y)*v.SYSINFO.mmpp-HACK_INSP_Offset[i].y
      }
      let dist=Math.hypot(diffInfo.x,diffInfo.y);

      diffInfo.dist=dist
      // if(maxDist<dist)
      // {
      //   maxDist=dist;
      // }
      distsInfo.push(diffInfo);
      if(dist>tolerance_mm)
      {        
        isOK=false;
      }

    }

  }

  //>>>true dists:0.2229,0.1576 mmpp:0.03823029755109455
  if(debugMode)
  {//check HACK_Offset is complete
    await waitUserCheck("isOK",`>>>${isOK} \n dists:${JSON.stringify(distsInfo,null,2)}\nmmpp:${v.SYSINFO.mmpp}`)
    
  }

  if(debugMode || (isOK===false && debugMode==false ))
  {
    await waitUserCheck("Override the offset data?",`dists:${JSON.stringify(distsInfo,null,2)}\nmmpp:${v.SYSINFO.mmpp}`)
      .then(_=>{
        v.HACK_Offset=distsInfo.map((dinfo,idx)=>{
          let updateX=(dinfo===null)?0:dinfo.y;//swap the xy order, since the machine coord and camera coord rotated 90 deg
          let updateY=(dinfo===null)?0:dinfo.x;
          if(Math.abs(updateX)<0.05)updateX=updateX/2;//slow approach
          if(Math.abs(updateY)<0.05)updateY=updateY/2;//slow approach


          return{
          x:HACK_Offset[idx].x+updateX,
          y:HACK_Offset[idx].y+updateY
        }})
      })
      .catch(e=>{
        console.log(">>>");
        throw(e)
      })
  }

  let resultOKSecY=resultSecY+30;
  let resultNGSecY=resultSecY;

  if(isOK)
  {
    await G(`G01 Y${resultOKSecY}`)
  }
  else
  {
    await G(`G01 Y${resultNGSecY} R11_-90`)
  }


  await G(`M42 P${25} T0`)//release

  await G(`M42 P${26} T1`)//fast blow
  await G("G04 P10")
  await G(`M42 P${26} T0`)


  // await G(`G01 Z1_0 R11_0`)



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