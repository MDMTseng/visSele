
let DEV_MODE=false;
try{
  DEV_MODE=__DEV_MODE__||false;
}catch(e){}

function undefVal(v,fallback){ return (v===undefined)?fallback:v}

let current_version="1.2";

const default_FLAG={
  version:current_version,
  ALLOW_SOFT_CAM:false,

  // Inspection-mode overlay: per-caliper hits (green=inlier, red=outlier).
  // Only meaningful for line/arc shapes whose def has locating=='caliper'.
  // Toggleable at runtime from the inspection UI; mirrored on the renderer
  // via EverCheckCanvasComponent before each draw.
  SHOW_CALIPER_HITS_INSP:true,

  FI_MODE_UPLOAD_SKIP:10,
  CI_MODE_UPLOAD_SKIP:1,
  CI_MODE_StatSettingParam:{
    historyReportlimit: 1000,
    keepInTrackingTime_ms: 1000,
    minReportRepeat: 2,
    headReportSkip: 1,
    maxReportRepeat:5
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
  // The bench's "1006 disconnect" mystery (2026-08-16): it was never the
  // network. queryCam polls camera_info every ~2s; whenever the fake camera
  // is not acquiring (trigger_mode 1 between sessions) cam_status != 0, the
  // app dispatches WS_ERROR -> REMOTE_SYSTEM_NOT_READY -> SPLASH, and with
  // ALLOW_SOFT_CAM=false it refuses to auto-reconnect a soft camera -- so a
  // dev bench cycled SPLASH->MAIN->SPLASH every ~5s forever. Dev builds work
  // against the soft camera on purpose; let them.
  origsetup.ALLOW_SOFT_CAM=true;
  origsetup.FI_MODE_UPLOAD_SKIP=100;
  origsetup.CI_MODE_UPLOAD_SKIP=100;

  // origsetup.CI_MODE_StatSettingParam={
  //   ...origsetup.CI_MODE_StatSettingParam,
  //   maxReportRepeat:2,
  //   historyReportlimit: 1000,
  //   keepInTrackingTime_ms: 0,
  //   minReportRepeat: 0,
  //   headReportSkip: 0,
  // };
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