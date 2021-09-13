
let DEV_MODE=false;
try{
  DEV_MODE=__DEV_MODE__||false;
}catch(e){}



export default {
    version:"1.1.001",
    FLAGS:{
      DEV_MODE,
      ALLOW_SOFT_CAM:DEV_MODE,
      CI_INSP_DO_NOT_STACK_REPORT:DEV_MODE,
      CI_INSP_SEND_REP_TO_DB_SKIP:DEV_MODE?100:undefined
    }
};