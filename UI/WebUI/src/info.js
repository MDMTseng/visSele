
let DEV_MODE=false;
try{
  DEV_MODE=__DEV_MODE__||false;
}catch(e){}

function undefVal(v,fallback){ return (v===undefined)?fallback:v}

let current_version="1.1.003";

const default_FLAG={
  version:current_version,
  ALLOW_SOFT_CAM:false,

  FI_MODE_UPLOAD_SKIP:10,
  CI_MODE_UPLOAD_SKIP:1,
  CI_MODE_StatSettingParam:{
    historyReportlimit: 1000,
    keepInTrackingTime_ms: 1000,
    minReportRepeat: 2,
    headReportSkip: 1,
    maxReportRepeat:2
  },
  FI_MODE_StatSettingParam:{
    historyReportlimit: 1000,
    keepInTrackingTime_ms: 0,
    minReportRepeat: 0,
    headReportSkip: 0,
  },




};


export function debug_SysSetting(origsetup={})
{
  origsetup.DEV_MODE=true;
  origsetup.ALLOW_SOFT_CAM=true;
  origsetup.FI_MODE_UPLOAD_SKIP=100;
  origsetup.CI_MODE_UPLOAD_SKIP=100;

  origsetup.CI_MODE_StatSettingParam={
    ...origsetup.CI_MODE_StatSettingParam,
    maxReportRepeat:2,
    historyReportlimit: 1000,
    keepInTrackingTime_ms: 0,
    minReportRepeat: 0,
    headReportSkip: 0,
  };
  // origsetup.FI_MODE_StatSettingParam={
  //   ...origsetup.FI_MODE_StatSettingParam,
  //   historyReportlimit: 1000,
  //   keepInTrackingTime_ms: 0,
  //   minReportRepeat: 0,
  //   headReportSkip: 0,
  // };


  origsetup.version=current_version;


  return origsetup;
};


export function GetDefaultSystemSetting(){
  let setting={...default_FLAG}
  if(DEV_MODE)
  {
    setting=debug_SysSetting(setting);
  }
  
  return setting;
}

export default default_FLAG;