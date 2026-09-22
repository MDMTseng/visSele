
export const EVENT = {
    
  Edit_Tar_Update:"Edit_Tar_Update",
  Edit_Tar_Ele_Trace_Update:"Edit_Tar_Ele_Trace_Update",
  Edit_Tar_Ele_Cand_Update:"Edit_Tar_Ele_Cand_Update",
  Shape_List_Update:"Shape_List_Update",
  Def_Retake:"Def_Retake",
  Instrument_Mmpp_Set:"Instrument_Mmpp_Set",
  Shape_Set:"Shape_Set",
  Shape_Decoration_ID_Order_Update:"Shape_Decoration_ID_Order_Update",
  Shape_Decoration_Extra_Info_Update:"Shape_Decoration_Extra_Info_Update",
  Shape_Decoration_Control_Margin_Info_Update:"Shape_Decoration_Control_Margin_Info_Update",
  DefFileName_Update:"DefFileName_Update",
  DefFileHash_Update:"DefFileHash_Update",
  DefFileTag_Update:"DefFileTag_Update",
  MachTag_Update:"MachTag_Update",
  Matching_Face_Update:"Matching_Face_Update",
  Matching_Angle_Margin_Deg_Update:"Matching_Angle_Margin_Deg_Update",
  Matching_Version_Update:"Matching_Version_Update",
  Inspection_Downsample_Update:"Inspection_Downsample_Update",
  Sig_Match_Sim_Thres_Update:"Sig_Match_Sim_Thres_Update",
  Morph_Mode_Update:"Morph_Mode_Update",
  Morph_TPS_Lambda_Update:"Morph_TPS_Lambda_Update",
  Morph_Max_Iter_Update:"Morph_Max_Iter_Update",
  Morph_Alpha_Update:"Morph_Alpha_Update",
  Shape_Match_Scale_Update:"Shape_Match_Scale_Update",
  Locating_Engine_Update:"Locating_Engine_Update",
  EditInfo_Patch:"EditInfo_Patch",
  InspOptionalTag_Update:"InspOptionalTag_Update",
  SUCCESS:"DEFCONF_MODE_SUCCESS",
  FAIL:"DEFCONF_MODE_FAIL",
  ERROR:"ERROR",

  DefConf_Lock_Level_Update:"DefConf_Lock_Level_Update",
}

export function Edit_Tar_Update(targetObj)
{
  return {
    type: EVENT.Edit_Tar_Update,data: targetObj
  }
}

export function Edit_Tar_Ele_Trace_Update(keyTrace)
{
  return {
    type: EVENT.Edit_Tar_Ele_Trace_Update,data: keyTrace
  }
}
export function Edit_Tar_Ele_Cand_Update(targetObj)
{
  return {
    type: EVENT.Edit_Tar_Ele_Cand_Update,data: targetObj
  }
}

export function Shape_List_Update(shapeList)
{
  return {
    type: EVENT.Shape_List_Update,data: shapeList
  }
}
// 重新設定/TAKE: the captured image is a NEW object, not the loaded def.
//
// Clearing the shape list was all this used to do, so everything else about the
// previous recipe survived the retake -- including __shape_cache, the trained
// line2Dup feature set. Worse, stampRefImagePath kept pointing the core at
// <defModelPath>.png, and _ref_image_path is the HIGHEST priority template
// source: the SBM studio trained and tested against the previous product's
// saved picture while showing the new one.
// keepMeasurements: the retake replaces the PICTURE but the measurement features
// and matching parameters stay. Everything the localizer owns still goes, because
// it describes a frame the new image does not have.
// The machine's own mm/px, from lens calibration. Only TAKE sets it, and only
// for a frame that came off the camera -- see getEditorMmpp.
export function Instrument_Mmpp_Set(mmpp)
{
  return {
    type: EVENT.Instrument_Mmpp_Set, data: mmpp
  }
}

export function Def_Retake(keepMeasurements)
{
  return {
    type: EVENT.Def_Retake, data: { keepMeasurements: !!keepMeasurements }
  }
}
export function Shape_Decoration_ID_Order_Update(shape_id_order)
{
  return {
    type: EVENT.Shape_Decoration_ID_Order_Update,data: shape_id_order
  }
}


export function DefConf_Lock_Level_Update(level)
{
  return {
    type: EVENT.DefConf_Lock_Level_Update ,data:level
  }
}



export function Shape_Decoration_Extra_Info_Update(extra_info)
{
  return {
    type: EVENT.Shape_Decoration_Extra_Info_Update,data: extra_info
  }
}

export function Shape_Decoration_Control_Margin_Info_Update(new_deco)
{
  return {
    type: EVENT.Shape_Decoration_Control_Margin_Info_Update,data: new_deco
  }
}

export function Matching_Face_Update(faceSetup)
{
  return {
    type: EVENT.Matching_Face_Update,data: faceSetup
  }
}
export function Matching_Angle_Margin_Deg_Update(deg)
{
  return {
    type: EVENT.Matching_Angle_Margin_Deg_Update,data: deg
  }
}

export function Matching_Version_Update(v)
{
  return {
    type: EVENT.Matching_Version_Update, data: v
  }
}

export function Inspection_Downsample_Update(n)
{
  return {
    type: EVENT.Inspection_Downsample_Update, data: n
  }
}

export function Sig_Match_Sim_Thres_Update(v)
{
  return {
    type: EVENT.Sig_Match_Sim_Thres_Update, data: v
  }
}

export function Morph_Mode_Update(mode)
{
  return {
    type: EVENT.Morph_Mode_Update, data: mode
  }
}

export function Morph_TPS_Lambda_Update(v)
{
  return {
    type: EVENT.Morph_TPS_Lambda_Update, data: v
  }
}

export function Morph_Max_Iter_Update(v)
{
  return {
    type: EVENT.Morph_Max_Iter_Update, data: v
  }
}

export function Morph_Alpha_Update(v)
{
  return {
    type: EVENT.Morph_Alpha_Update, data: v
  }
}

export function Shape_Match_Scale_Update(v)
{
  return {
    type: EVENT.Shape_Match_Scale_Update, data: v
  }
}

export function Locating_Engine_Update(v)
{
  return {
    type: EVENT.Locating_Engine_Update, data: v
  }
}

// Generic shallow merge into edit_info. Used for low-frequency localization
// settings (def_image_reg, roi_refine_points) that don't warrant a dedicated action.
export function EditInfo_Patch(patch)
{
  return {
    type: EVENT.EditInfo_Patch, data: patch
  }
}

export function DefFileHash_Update(hash)
{
  return {
    type: EVENT.DefFileHash_Update,data: hash
  }
}



export function Shape_Set(shape_data)
{
  return {
    type: EVENT.Shape_Set,data: {shape:shape_data.shape,id:shape_data.id}
  }
}

export function DefFileName_Update(newName)
{
  return {
    type: EVENT.DefFileName_Update,data: newName
  }
}
export function DefFileTag_Update(newInfo)
{
  return {
    type: EVENT.DefFileTag_Update,data: newInfo
  }
}
export function MachTag_Update(MachTag)
{
  return {
    type: EVENT.MachTag_Update,data: MachTag
  }
}

export function InspOptionalTag_Update(newTag)
{
  return {
    type: EVENT.InspOptionalTag_Update,data: newTag
  }
}


export function SUCCESS()
{
  return {
    type: EVENT.SUCCESS
  }
}

export function FAIL()
{
  return {
    type: EVENT.FAIL
  }
}




export function ERROR()
{
  return {
    type: EVENT.ERROR
  }
}

