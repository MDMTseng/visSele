
export const UI_SM_STATES = {
  SPLASH:"SPLASH",
  MAIN:"MAIN",
  EDIT_MODE:"EDIT_MODE",
  EDIT_MODE_NEUTRAL:"NEUTRAL",
  EDIT_MODE_LINE_CREATE:"LINE_CREATE",
  EDIT_MODE_ARC_CREATE:"ARC_CREATE",
  EDIT_MODE_SHAPE_EDIT:"SHAPE_EDIT",
};



export const BPG_WS_EVENT = {
  BPG_WS_REGISTER:"BPG_WS_REGISTER",
  BPG_WS_SEND:"BPG_WS_SEND",
  BPG_WS_DISCONNECT:"BPG_WS_DISCONNECT",
  BPG_WS_RECV:"BPG_WS_RECV",
};

export const UI_SM_EVENT = {
  Connected:"Connected",
  Disonnected:"Disonnected",
  _SUCCESS:"_SUCCESS",
  _FAIL:"_FAIL",
  EXIT:"EXIT",
  Edit_Mode:"Edit_Mode",
  Inspection_Report:"Inspection_Report",
  Image_Update:"Image_Update",
  Line_Edit_Mode:"Line_Edit",
  Arc_Edit_Mode:"Arc_Edit",
  Line_Create:"Line_Create",
  Arc_Create:"Arc_Create",
  Shape_Edit:"Shape_Edit",

  Control_SM_Panel:"Control_SM_Panel",
  
};




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
    misc:null
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