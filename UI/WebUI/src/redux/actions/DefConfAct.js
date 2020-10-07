
export const EVENT = {
    
  Edit_Tar_Update:"Edit_Tar_Update",
  Edit_Tar_Ele_Trace_Update:"Edit_Tar_Ele_Trace_Update",
  Edit_Tar_Ele_Cand_Update:"Edit_Tar_Ele_Cand_Update",
  Shape_List_Update:"Shape_List_Update",
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
  IntrusionSizeLimitRatio_Update:"IntrusionSizeLimitRatio_Update",
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

export function IntrusionSizeLimitRatio_Update(ratio)
{
  return {
    type: EVENT.IntrusionSizeLimitRatio_Update,data: ratio
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

