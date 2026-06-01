// Pure SPC statistics reducers extracted from UICtrlReducer (no redux/state coupling).
import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import { mkLog } from "UTIL/logger";
const log = mkLog("editor.reducer");

  function histDataReducer(histoInfo, dataValue) {
    if (dataValue < histoInfo.xmin) {
      histoInfo.histo[0]++;
      return histoInfo;
    }
    if (dataValue > histoInfo.xmax) {
      histoInfo.histo[histoInfo.histo.length - 1]++;
      return histoInfo;
    }
    let dataRegion = histoInfo.histo.length - 2;//The first value and last value are the value excced xmin& xmax

    //If the data is in the boundary then there must be a position for it.
    let val_idx = Math.floor(dataRegion * (dataValue - histoInfo.xmin) / (histoInfo.xmax - histoInfo.xmin));
    //Suppose xmax=21 xmin=20 dataRegion=4(index 0~3)
    //idx = floor(dataRegion*(value-min)/(max-min+1));
    //=>value=21   (4*(21-20)/(21-20))=>4/1=4  :on the edge case the idx might hit the boundary
    //=>value=20   (4*(20-20)/(21-20))=>0/1=0 


    //Suppose xmax=20 xmin=10 dataRegion=14(index 0~13)
    //=>value=214   (13*(14-10)/(20-10))=>13*4/10=5.2  :on the edge case the idx might hit the boundary
    //=>value=19    (13*(19-10)/(20-10))=>13*9/10=117/10=11.7
    //=>value=19.9    (13*(19.9-10)/(20-10))=>13*9.9/10=117/10=12.87

    if (val_idx >= dataRegion) val_idx = dataRegion - 1;//Just in case it hits upper boundary;
    histoInfo.histo[val_idx + 1]++;//with 1 padding for lower over boundary data

    return histoInfo;
  }

  function statReducer_sp(stat_sp,measuredef,new_rep)
  {

    // console.log(stat_sp,measuredef,new_rep);

    if( (new_rep.status!=INSPECTION_STATUS.FAILURE && new_rep.status!=INSPECTION_STATUS.SUCCESS) || measuredef.quality_essential==false)
    {
      return stat_sp;
    }
    let new_sp={...stat_sp};
    if(new_rep.value>measuredef.USL)//upper NG
    {
      new_sp.SNG_count++;
      new_sp.consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_info=5;
    }
    else if(new_rep.value<measuredef.LSL)//lower NG
    {
      new_sp.SNG_count++;
      new_sp.consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_count++;
      new_sp.fuzzy_consecutive_SNG_info=5;
    }
    else 
    {
      new_sp.consecutive_SNG_count=0;
      if((new_sp.fuzzy_consecutive_SNG_info)>0)
      {
        new_sp.fuzzy_consecutive_SNG_info--;
      }
      else 
        new_sp.fuzzy_consecutive_SNG_count=0;
    }

    
    if(new_rep.value>measuredef.UCL)//upper NG
    {
      new_sp.CNG_count++;
      new_sp.consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_info=5;
    }
    else if(new_rep.value<measuredef.LCL)//upper NG
    {
      new_sp.CNG_count++;
      new_sp.consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_count++;
      new_sp.fuzzy_consecutive_CNG_info=5;
    }
    else
    {
      new_sp.consecutive_CNG_count=0;
      if((new_sp.fuzzy_consecutive_CNG_info)>0)
      {
        new_sp.fuzzy_consecutive_CNG_info--;
      }
      else 
        new_sp.fuzzy_consecutive_CNG_count=0;
    }


    if(new_sp.max_consecutive_SNG_count<new_sp.consecutive_SNG_count)
    {
      new_sp.max_consecutive_SNG_count=new_sp.consecutive_SNG_count;
    }
    if(new_sp.max_fuzzy_consecutive_SNG_count<new_sp.fuzzy_consecutive_SNG_count)
    {
      new_sp.max_fuzzy_consecutive_SNG_count=new_sp.fuzzy_consecutive_SNG_count;
    }

    if(new_sp.max_consecutive_CNG_count<new_sp.consecutive_CNG_count)
    {
      new_sp.max_consecutive_CNG_count=new_sp.consecutive_CNG_count;
    }
    if(new_sp.max_fuzzy_consecutive_CNG_count<new_sp.fuzzy_consecutive_CNG_count)
    {
      new_sp.max_fuzzy_consecutive_CNG_count=new_sp.fuzzy_consecutive_CNG_count;
    }
    
    return new_sp;
    // stat.sp
  }

  function statReducer(statistic, report) {

    //if the time is longer than 4s then remove it from matchingWindow
    //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
    statistic.measureList.forEach((measure) => {
      let new_rep = report.judgeReports.find((rep) => rep.id == measure.id);
      //measure.statistic
      let stat = measure.statistic;
      if (new_rep === undefined) {
        stat.count_stat.NA++;
        log.error("The incoming inspection report doesn't match the defFile");
        return;
      }

      if (new_rep.status == INSPECTION_STATUS.NA) {
        stat.count_stat.NA++;
        log.error("The measure[" + new_rep.name + "] is NA");
        return;
      }


      let nv_val = new_rep.value;

      if (stat.count_stat[new_rep.detailStatus] === undefined) {
        stat.count_stat[new_rep.detailStatus] = 0;
      }
      else {
        stat.count_stat[new_rep.detailStatus]++;
      }

      stat.count++;
      stat.sum += nv_val;
      stat.mean = stat.sum / stat.count;
      stat.sqSum += nv_val * nv_val;
      stat.variance = stat.sqSum / stat.count - stat.mean * stat.mean;//E[X^2]-E[X]^2
      stat.sigma = Math.sqrt(stat.variance);
      

      stat.sp=statReducer_sp(stat.sp,measure,new_rep);
      // console.log(new_rep.detailStatus);
      // stat.sp={

      // }


      stat.CPU = (measure.USL - stat.mean) / (3 * stat.sigma);
      stat.CPL = (stat.mean - measure.LSL) / (3 * stat.sigma);
      stat.CP = Math.min(stat.CPU, stat.CPL);
      stat.CK = (stat.mean - measure.value) / ((measure.USL - measure.LSL) / 2);
      stat.CPK = stat.CP * (1 - Math.abs(stat.CK));

      if(!(stat.MIN<=nv_val))//consider MIN=NaN as init state
      {
        stat.MIN=nv_val;
      }

      if(!(stat.MAX>=nv_val))//consider MAX=NaN as init state
      {
        stat.MAX=nv_val;
      }



      stat.histogram = histDataReducer(stat.histogram, nv_val);
      //log.info(stat);

    });

    return statistic;
  }

export { statReducer };
