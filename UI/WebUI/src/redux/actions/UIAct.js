import {INSPECTION_STATUS} from 'UTIL/BPG_Protocol';
export const UI_SM_STATES = {
  SPLASH:"SPLASH",
  MAIN:"MAIN",
  DEFCONF_MODE:"DEFCONF_MODE",
  DEFCONF_MODE_NEUTRAL:"DEFCONF_MODE_NEUTRAL",
  DEFCONF_MODE_LINE_CREATE:"LINE_CREATE",
  DEFCONF_MODE_AUX_LINE_CREATE:"AUX_LINE_CREATE",
  DEFCONF_MODE_AUX_POINT_CREATE:"AUX_POINT_CREATE",
  DEFCONF_MODE_SEARCH_POINT_CREATE:"SEARCH_POINT_CREATE",
  DEFCONF_MODE_MEASURE_CREATE:"MEASURE_CREATE",
  DEFCONF_MODE_ARC_CREATE:"ARC_CREATE",
  DEFCONF_MODE_SHAPE_EDIT:"SHAPE_EDIT",



  INSP_MODE:"INSP_MODE",
  INSP_MODE_NEUTRAL:"INSP_MODE_NEUTRAL",
  
  ANALYSIS_MODE:"ANALYSIS_MODE",
  ANALYSIS_MODE_NEUTRAL:"ANALYSIS_MODE_NEUTRAL",
};


export const SHAPE_ATTR_KEY = {
  id:"id",
  name:"name",
  color:"color",
  type:"type",
  ref:"ref",
  element:"element",
};
export const SAK = SHAPE_ATTR_KEY;


export const SHAPE_TYPE = {
  sign360:"sign360",
  point:"point",
  line:"line",
  arc:"arc",
  aux_point:"aux_point",
  aux_line:"aux_line",
  search_point:"search_point",

  measure:"measure",
  measure_subtype:{
    NA:"NA",
    //sigma:"sigma",
    distance:"distance",
    angle:"angle",
    radius:"radius",
    calc:"calc",
  }

};
export const DEFAULT_UNIT = {
  [SHAPE_TYPE.measure_subtype.sigma]:"mm",
  [SHAPE_TYPE.measure_subtype.distance]:"mm",
  [SHAPE_TYPE.measure_subtype.angle]:"º",
  [SHAPE_TYPE.measure_subtype.radius]:"mm",
  [SHAPE_TYPE.measure_subtype.calc]:"",

}

export const UI_SM_EVENT = {
  REMOTE_SYSTEM_NOT_READY:"REMOTE_SYSTEM_NOT_READY",
  REMOTE_SYSTEM_READY:"REMOTE_SYSTEM_READY",
  _SUCCESS:"_SUCCESS",
  _FAIL:"_FAIL",
  EXIT:"EXIT",
  ERROR:"ERROR",

  Version_Map_Update:"Version_Map_Update",
  Core_Status_Update:"Core_Status_Update",
  machine_custom_setting_Update:"machine_custom_setting_Update",
  Insp_Mode_Update:"InspMode_Update",
  Def_Model_Path_Update:"Def_Model_Path_Update",
  WS_channel:"WS_channel",

  Edit_Mode:"Edit_Mode",
  Insp_Mode:"Insp_Mode",
  Analysis_Mode:"Analysis_Mode",
  Inspection_Report:"Inspection_Report",
  Camera_Info_Update:"Camera_Info_Update",
  Image_Update:"Image_Update",
  StatSettingParam_Update:"StatSettingParam_Update",
  Define_File_Update:"Define_File_Update",
  SIG360_Report_Update:"SIG360_Report_Update",
  SIG360_Extraction:"SIG360_Extraction",
  PD_DATA_Update:"PD_DATA_Update",
  uInsp_Machine_Info_Update:"uInsp_Machine_Info_Update",
  Line_Create:"Line_Create",
  Arc_Create:"Arc_Create",
  Aux_Line_Create:"Aux_Line_Create",
  Search_Point_Create:"Search_Point_Create",
  Aux_Point_Create:"Aux_Point_Create",

  Shape_Edit:"Shape_Edit",
  Measure_Create:"Measure_Create",


  EC_Save_Def_Config:"EC_Save_Def_Config",
  EC_Load_Def_Config:"EC_Load_Def_Config",

  
  Control_SM_Panel:"Control_SM_Panel",
  Canvas_Mouse_Location:"Canvas_Mouse_Location"
};




export function EV_WS_ChannelUpdate(WS_CH)
{
  return {
    type: UI_SM_EVENT.WS_channel ,data:WS_CH
  }
}

export function EV_machine_custom_setting_Update(info)
{
  return {
    type: UI_SM_EVENT.machine_custom_setting_Update,
    data:info
  }
}


export function EV_UI_EC_Save_Def_Config(info)
{
  return {
    type: UI_SM_EVENT.EC_Save_Def_Config,
    data:info
  }
}
export function EV_UI_EC_Load_Def_Config(info)
{
  return {
    type: UI_SM_EVENT.EC_Load_Def_Config,
    data:info
  }
}

export function EV_UI_Version_Map_Update(mapInfo)
{
  return {
    type: UI_SM_EVENT.Version_Map_Update,
    data: mapInfo
  }
}



export function EV_Core_Status_Update(coreStatus)
{
  return {
    type: UI_SM_EVENT.Core_Status_Update,
    data: coreStatus
  }
}

export function EV_UI_Insp_Mode_Update(mode)
{
  return {
    type: UI_SM_EVENT.Insp_Mode_Update,
    data: mode
  }
}


// export function EV_WS_Connected(ws_obj)
// {
//   return {
//     type: UI_SM_EVENT.Connected ,data:ws_obj
//   }
// }

// export function EV_WS_Disconnected(peer)
// {
//   return {
//     type: UI_SM_EVENT.Disonnected ,data:peer
//   }
// }

export function EV_WS_REMOTE_SYSTEM_READY(ws_obj)
{

  return {
    type: UI_SM_EVENT.REMOTE_SYSTEM_READY ,data:ws_obj
  }
}

export function EV_WS_REMOTE_SYSTEM_NOT_READY(peer)
{
  return {
    type: UI_SM_EVENT.REMOTE_SYSTEM_NOT_READY ,data:peer
  }
}

export function EV_UI_Edit_Mode()
{
  return {
    type: UI_SM_EVENT.Edit_Mode
  }
}
export function EV_UI_Insp_Mode()
{
  return {
    type: UI_SM_EVENT.Insp_Mode
  }
}
export function EV_UI_Analysis_Mode()
{
  return {
    type: UI_SM_EVENT.Analysis_Mode
  }
}


export function EV_UI_ACT(ACT,data=null,misc=null)
{
  return {
    type: ACT,
    data:data,
    misc:misc
  }
}


export function Def_Model_Path_Update(newPath)
{
  return {
    type: UI_SM_EVENT.Def_Model_Path_Update ,data:newPath
  }
}



export function EV_UI_Canvas_Mouse_Location(mouseLoc)
{
  return {
    type: UI_SM_EVENT.Canvas_Mouse_Location,
    data:mouseLoc,
  }
}


export function EV_WS_Inspection_Report(report)
{
  return {
    type: UI_SM_EVENT.Inspection_Report ,data:report
  }
}
export function EV_WS_Image_Update(ImageData)
{
  return {
    type: UI_SM_EVENT.Image_Update ,data:ImageData
  }
}

export function EV_StatSettingParam_Update(statSettingParam)
{
  return {
    type: UI_SM_EVENT.StatSettingParam_Update ,data:statSettingParam
  }
}

export function EV_Camera_Info_Update(info)
{
  return {
    type: UI_SM_EVENT.Camera_Info_Update ,data:info
  }
}

export function EV_WS_Define_File_Update(DFData)
{
  return {
    type: UI_SM_EVENT.Define_File_Update ,data:DFData
  }
}


export function EV_WS_SIG360_Extraction(data)
{
  return {
    type: UI_SM_EVENT.SIG360_Extraction ,data:data
  }
}


export function EV_WS_SIG360_Report_Update(data)
{
  return {
    type: UI_SM_EVENT.SIG360_Report_Update ,data:data
  }
}
export function EV_WS_PD_DATA_Update(data)
{
  return {
    type: UI_SM_EVENT.PD_DATA_Update ,data:data
  }
}
export function EV_WS_uInsp_Machine_Info_Update(data)
{
  return {
    type: UI_SM_EVENT.uInsp_Machine_Info_Update ,data:data
  }
}

export function EV_WS_SEND(id,tl,prop,data,uintArr,promiseCBs){
  return ({
    type:"MWWS_SEND",
    data:{
      id:id,
      data:{
        tl:tl,
        prop:prop,
        data:data,
        uintArr:uintArr
      },
      promiseCBs
    }
  });
}

export function EV_WS_Disconnect(id)
{
  return {
    type: "MWWS_DISCONNECT" ,data:{id}
  }

}