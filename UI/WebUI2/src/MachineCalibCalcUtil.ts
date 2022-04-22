



import { ResultType } from 'antd/lib/result';
import { PointsFitCircle,intersectPoint, VEC2D} from './UTIL/MathTools';


type CompDatType=VEC2D&{area:number}|null
type CompRepType={
  reports:{

    components:CompDatType[]
  }[]
}


// let adjInfoG=[0,1,2,3].map(idx=>CALC_ADJ(sysInfo,-1,idx)).map(info=>({...info,theda360:info.theda*180/Math.PI}));

// console.log(sysInfo);
// console.log(adjInfoG);








// let sysInfoY=compRepExtractPtGroup(_YLocInfo,[500,700,1000,1000],0.7);
// console.log(_YLocInfo,sysInfoY);