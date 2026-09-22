
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
  // Whether the CORE sends per-caliper hits with every report. Off by default
  // on the inspection screen (switchable there); the def editor forces it on
  // while it is open.
  EMIT_CALIPER_HITS:false,

  FI_MODE_UPLOAD_SKIP:10,
  CI_MODE_UPLOAD_SKIP:1,
  CI_MODE_StatSettingParam:{
    historyReportlimit: 100,
    keepInTrackingTime_ms: 1000,
    minReportRepeat: 2,
    headReportSkip: 1,
    maxReportRepeat:5
  },
  // SI averages the PICTURE in the core, so the tracking window must NOT
  // average the measurements on top of it -- that would be averaging twice,
  // and the second one is the one that cannot be reproduced from any image.
  // Same shape as FI: no repeat, no blending.
  SI_MODE_UPLOAD_SKIP:1,
  SI_MODE_StatSettingParam:{
    historyReportlimit: 100,
    keepInTrackingTime_ms: 0,
    minReportRepeat: 0,
    headReportSkip: 0,
  },
  // The core's settle-and-average parameters, pushed on entering SI.
  // diff_* are 8-bit levels against the running average; 6 is about 3x the
  // measured background noise sigma (2.1) on this bench, and a 1 px shift of
  // the part moves 2% of the pixels -- orders of magnitude of margin.
  SI_MODE_PARAM:{
    diff_global: 6.0,
    diff_local: 40,
    diff_skip: 10,
    // N: frames averaged into the one inspection.
    avg_frames: 5,
    // Still frames thrown away before the averaging starts. The diff gate says
    // the scene stopped changing by ITS threshold; a hand that has just let go
    // can be under that threshold and still settling.
    //
    // Deliberately NOT SI_MODE_StatSettingParam.headReportSkip, even though it
    // means the same kind of thing: that object is read by the tracking-window
    // reducer, where headReportSkip > 0 keeps a report OUT of the statistics
    // until it counts down. Putting SI's value there would have silently
    // dropped every SI report from the charts.
    head_skip: 1,
  },
  FI_MODE_StatSettingParam:{
    historyReportlimit: 100,
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
  //   historyReportlimit: 100,
  //   keepInTrackingTime_ms: 0,
  //   minReportRepeat: 0,
  //   headReportSkip: 0,
  // };
  // origsetup.FI_MODE_StatSettingParam={
  //   ...origsetup.FI_MODE_StatSettingParam,
  //   historyReportlimit: 100,
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