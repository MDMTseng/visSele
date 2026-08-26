// Pure SPC statistics reducers extracted from UICtrlReducer (no redux/state coupling).
import { INSPECTION_STATUS } from 'UTIL/BPG_Protocol';
import { mkLog } from "UTIL/logger";

// The shape of a measure's statistic, in ONE place.
//
// resetStatisticState in InspectionEditorLogic built this inline, and a bucket
// restart needs exactly the same thing -- so a second copy here would drift the
// day someone adds a counter, and the two would disagree about what "empty"
// means. spcStats owns the statistics, so it owns their initial value; it
// imports nothing from the editor, so the editor can import this without a
// cycle.
export function initStatisticSPState() {
  return {
    NA_count: 0,
    CNG_count: 0,
    consecutive_CNG_count: 0,
    max_consecutive_CNG_count: 0,
    fuzzy_consecutive_CNG_count: 0,
    fuzzy_consecutive_CNG_info: 0,
    max_fuzzy_consecutive_CNG_count: 0,
    SNG_count: 0,
    consecutive_SNG_count: 0,
    fuzzy_consecutive_SNG_count: 0,
    fuzzy_consecutive_SNG_info: 0,
    max_consecutive_SNG_count: 0,
    max_fuzzy_consecutive_SNG_count: 0,
  };
}

// `measure` supplies the histogram range, which is why a bucket restart has to
// go through here: the range is derived from the LIMITS, so when the 製程
// changes the old range describes a different tolerance.
export function initMeasureStatistic(measure) {
  return {
    count_stat: { NA: 0, UOK: 0, LOK: 0, UCNG: 0, LCNG: 0, USNG: 0, LSNG: 0 },
    histogram: {
      xmin: 1.2 * (measure.LSL - measure.value) + measure.value,
      xmax: 1.2 * (measure.USL - measure.value) + measure.value,
      histo: new Array(502).fill(0),
    },
    count: 0, sum: 0, sqSum: 0, mean: 0, variance: 0, sigma: 0,
    sp: initStatisticSPState(),
    CP: 0, CK: 0, CPU: 0, CPL: 0, CPK: 0,
    MIN: NaN, MAX: NaN,
  };
}

// In place, because the bucket is referenced from measureList and callers hold
// the object.
function resetOneStat(stat, measure) {
  const fresh = initMeasureStatistic(measure);
  for (const k of Object.keys(fresh)) stat[k] = fresh[k];
}
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

  // `curMarginInfo` is the per-製程 shape list the VERDICT was graded against
  // (cur_MarginInfo in UICtrlReducer). Optional: without it this behaves as
  // before and grades statistics against the root.
  //
  // S1 and S2, which are one defect seen twice. measureList is snapshotted from
  // the ROOT shapes at reset and never follows the 製程, so:
  //
  //   * CP/CPK were computed against the root USL/LSL while the part was
  //     JUDGED against the override. A 製程 that tightens the tolerance
  //     therefore produced an optimistic CPK -- the wrong direction to be
  //     wrong in, because it says a process is more capable than it is.
  //   * samples judged under two different limit sets accumulated in one
  //     bucket, and the histogram kept the range built at reset time.
  //
  // Both are fixed by the same thing: take the limits from the same place the
  // verdict did, and when they CHANGE, start that measure's bucket again.
  // Statistics over two limit sets are not statistics about anything.
  function effectiveMeasure(measure, curMarginInfo) {
    if (!curMarginInfo) return measure;
    const override = curMarginInfo.find((s) => s.id === measure.id);
    return override || measure;
  }
  function limitKey(m) {
    return `${m.USL}|${m.LSL}|${m.value}|${m.UCL}|${m.LCL}`;
  }

  function statReducer(statistic, report, curMarginInfo) {

    //if the time is longer than 4s then remove it from matchingWindow
    //log.info(">>>push(srep_inWindow)>>",srep_inWindow);
    statistic.measureList.forEach((measure_root) => {
      // The limits the verdict used. Everything below reads `measure`, so the
      // NG classification, CP/CPK and the histogram range all move together.
      const measure = effectiveMeasure(measure_root, curMarginInfo);
      let new_rep = report.judgeReports.find((rep) => rep.id == measure.id);
      //measure.statistic
      let stat = measure_root.statistic;

      // The 製程 changed under this bucket. Start it again rather than mix.
      const lk = limitKey(measure);
      if (stat._limitKey === undefined) {
        stat._limitKey = lk;
      } else if (stat._limitKey !== lk) {
        log.info('[stats] limits changed for measure ' + measure.id +
                 ' -- restarting its bucket', { was: stat._limitKey, now: lk });
        resetOneStat(stat, measure);
        stat._limitKey = lk;
      }
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

      // Always count it, including the first one.
      //
      // This used to set an unseen status to 0 and increment only on the
      // SECOND occurrence, so any detailStatus the initialiser does not know
      // about was permanently one short. The seven known tags are pre-created
      // in InspectionEditorLogic (count_stat: {NA, UOK, LOK, UCNG, LCNG, USNG,
      // LSNG}), so this never bit -- it was waiting for the next status the
      // core learns to emit, which is exactly when nobody would be looking at
      // this line.
      if (stat.count_stat[new_rep.detailStatus] === undefined) {
        stat.count_stat[new_rep.detailStatus] = 0;
      }
      stat.count_stat[new_rep.detailStatus]++;

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
