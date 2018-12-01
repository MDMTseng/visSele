
export const EVENT = {
    
  Edit_Tar_Update:"Edit_Tar_Update",
  Edit_Tar_Ele_Trace_Update:"Edit_Tar_Ele_Trace_Update",
  Edit_Tar_Ele_Cand_Update:"Edit_Tar_Ele_Cand_Update",
  Shape_List_Update:"Shape_List_Update",
  Shape_Set:"Shape_Set",

  SUCCESS:"EDIT_MODE_SUCCESS",
  FAIL:"EDIT_MODE_FAIL",

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

export function Shape_Set(shape_data)
{
  return {
    type: EVENT.Shape_Set,data: {shape:shape_data.shape,id:shape_data.id}
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


