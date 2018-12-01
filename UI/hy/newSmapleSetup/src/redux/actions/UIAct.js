
export const UI_SM_STATES = {
  SPLASH:"SPLASH",
  MAIN:"MAIN",
  EDIT_MODE:"EDIT_MODE",
  EDIT_MODE_NEUTRAL:"NEUTRAL",
  EDIT_MODE_LINE_CREATE:"LINE_CREATE",
  EDIT_MODE_AUX_LINE_CREATE:"AUX_LINE_CREATE",
  EDIT_MODE_AUX_POINT_CREATE:"AUX_POINT_CREATE",
  EDIT_MODE_SEARCH_POINT_CREATE:"SEARCH_POINT_CREATE",
  EDIT_MODE_MEASURE_CREATE:"MEASURE_CREATE",
  EDIT_MODE_ARC_CREATE:"ARC_CREATE",
  EDIT_MODE_SHAPE_EDIT:"SHAPE_EDIT",
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
    sigma:"sigma",
    distance:"distance",
    angle:"angle",
    radius:"radius",
  }

};

export const UI_SM_EVENT = {
  Connected:"Connected",
  Disonnected:"Disonnected",
  _SUCCESS:"_SUCCESS",
  _FAIL:"_FAIL",
  EXIT:"EXIT",



  WS_channel:"WS_channel",

  Edit_Mode:"Edit_Mode",
  Inspection_Report:"Inspection_Report",
  Image_Update:"Image_Update",
  Define_File_Update:"Define_File_Update",
  SIG360_Report_Update:"SIG360_Report_Update",


  Line_Create:"Line_Create",
  Arc_Create:"Arc_Create",
  Aux_Line_Create:"Aux_Line_Create",
  Search_Point_Create:"Search_Point_Create",
  Aux_Point_Create:"Aux_Point_Create",
  Arc_Create:"Arc_Create",
  Shape_Edit:"Shape_Edit",
  Measure_Create:"Measure_Create",


  EC_Save_Def_Config:"EC_Save_Def_Config",
  EC_Load_Def_Config:"EC_Load_Def_Config",
  EC_Trigger_Inspection:"EC_Trigger_Inspection",

  
  Control_SM_Panel:"Control_SM_Panel",
};




export function EV_WS_ChannelUpdate(WS_CH)
{
  return {
    type: UI_SM_EVENT.WS_channel ,data:WS_CH
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
export function EV_UI_EC_Trigger_Inspection(info)
{
  return {
    type: UI_SM_EVENT.EC_Trigger_Inspection,
    data:info
  }
}


export function EV_WS_Connected(peer)
{
  return {
    type: UI_SM_EVENT.Connected ,data:peer
  }
}

export function EV_WS_Disconnected(peer)
{
  return {
    type: UI_SM_EVENT.Disonnected ,data:peer
  }
}

export function EV_WS_Register(ws)
{
  return {
    type: BPG_WS_EVENT.BPG_WS_REGISTER ,ws:ws
  }
}

export function EV_WS_SEND(ws,data)
{
  return {
    type: BPG_WS_EVENT.BPG_WS_SEND ,data:data,ws:ws
  }
}

export function EV_WS_RECV(evt)
{
  return {
    type: UI_SM_EVENT.BPG_WS_RECV ,data:evt
  }
}

export function EV_UI_Edit_Mode()
{
  return {
    type: UI_SM_EVENT.Edit_Mode
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

export function EV_WS_Define_File_Update(DFData)
{
  return {
    type: UI_SM_EVENT.Define_File_Update ,data:DFData
  }
}
export function EV_WS_SIG360_Report_Update(data)
{
  return {
    type: UI_SM_EVENT.SIG360_Report_Update ,data:data
  }
}
