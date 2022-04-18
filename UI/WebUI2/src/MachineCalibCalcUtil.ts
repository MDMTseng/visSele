



import { ResultType } from 'antd/lib/result';
import { PointsFitCircle,intersectPoint, VEC2D} from './UTIL/MathTools';


type CompDatType=VEC2D&{area:number}|null
type CompRepType={
  reports:{

    components:CompDatType[]
  }[]
}

function R11InfoFindCentre(R11_PtGroup: CompDatType[][],angleInfo:number[])
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
      return undefined;
    }

    return intersectPoint(ang0_pt0,ang0_ptN,angN_pt0,angN_ptN);
    // console.log(180-180* Math.atan2(ang0_infoVec0.y,ang0_infoVec0.x)/Math.PI);
  }

}


function TryFillUndefInfo(xList:CompDatType[], fList:CompDatType[][],simFactor:number):any
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
    let undefIdx:number;
    let valIdx:number;
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
    let targetCand:any=undefined;
    fList[undefIdx].forEach((cand:any)=>{
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

function compRepExtractPtGroup(Reps:CompRepType[],areaTargets:number[],aFactor:number,doTryFill=true)
{
  let ptGGroup=
  Reps[0].reports.map((srep,idx:number)=>{

    let xx=Reps.map((sinfo)=>sinfo.reports[idx].components);
    return xx;
  })
  let ptGroup=ptGGroup.map((ptG:CompDatType[][],idx:number)=>{
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

      let FPts:CompDatType=null;
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


function checkAllPointsValid(ptGroup:CompDatType[][],Reps:CompRepType[])
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

function R11InfoCalc(R11Info:(CompRepType&{R11_angle:number})[],mmpp:number)
{
  let ptGroup=compRepExtractPtGroup(R11Info,[1331,1231,1231,1231,1229],0.7);
  checkAllPointsValid(ptGroup,R11Info)
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



export function ReportCalcSysInfo(
  reports:{
    CalibYloc:number,
    R11Rep:(CompRepType&{R11_angle:number})[],
    YRep:(CompRepType&{Y_Loc:number})[]})
  
  
{
  let YLocInfo=reports.YRep;
  let R11Info=reports.R11Rep;

  let SYSINFO:any;
  let ptGroupYvec=compRepExtractPtGroup(YLocInfo,[1331,1231,1231,1231,1231],0.7,false);
  {
    let pt1 = ptGroupYvec[4][0];
    let pt2 = ptGroupYvec[0][1];
    if(pt1!==null &&pt2!==null)
    {
      let dist = YLocInfo[1].Y_Loc-YLocInfo[0].Y_Loc;
      let vec = {x:pt2.x-pt1.x,y:pt2.y-pt1.y};
      let vecDist=Math.hypot(vec.x,vec.y);

      console.log(dist,vec,dist/vecDist);

      let mmpp = dist/vecDist;
      vec.x=vec.x*mmpp;
      vec.y=vec.y*mmpp;
      SYSINFO=R11InfoCalc(R11Info,mmpp);
      SYSINFO={...SYSINFO,CalibYloc:reports.CalibYloc,mmpp:dist/vecDist,Y_vec:vec}
    }
  }
  
  return SYSINFO;
}

export function CALC_ADJ(sysInfo:ReturnType<typeof ReportCalcSysInfo>,h:number,nozzleIdx:number )
{
  let R=sysInfo.RInfo[nozzleIdx];
  
  let theda=Math.asin(h/R);
  let X=R*(1-Math.cos(theda));
  return {
    theda,
    X
  }
}



export function SysInfoCalcCompensation(sysInfo:ReturnType<typeof ReportCalcSysInfo>,h:number,nozzleIdx:number)
{
  return CALC_ADJ(sysInfo,h,nozzleIdx);
}





// let adjInfoG=[0,1,2,3].map(idx=>CALC_ADJ(sysInfo,-1,idx)).map(info=>({...info,theda360:info.theda*180/Math.PI}));

// console.log(sysInfo);
// console.log(adjInfoG);








// let sysInfoY=compRepExtractPtGroup(_YLocInfo,[500,700,1000,1000],0.7);
// console.log(_YLocInfo,sysInfoY);