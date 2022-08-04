

export type type_CameraInfo={
  id:string,
  driver_name:string,
  available:boolean,
  enable:boolean,
  model:string,
  name: string,
  serial_nbr:string,
  vendor:string,

  trigger_mode:number,
  analog_gain:number,
  exposure:number,
  RGain:number,
  GGain:number,
  BGain:number,
  gamma:number,
  black_level:number,
  frame_rate:number,
  mirrorX:boolean,
  mirrorY:boolean
}



export type type_IMCM=
{
  camera_id:string,
  trigger_id:number,
  trigger_tag:string,
  image_info:{
    full_height: number
    full_width: number
    height: number
    image: ImageData
    offsetX: number
    offsetY: number
    scale: number
    width: number
  }
}
