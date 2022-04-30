
#include "CameraLayer_Aravis.hpp"
#include "logctrl.h"
#include <arv.h>
#include <stdexcept>
using namespace std;

static CameraLayer_Aravis::cam_info getDeviceIndex(const char *device_id)
{
  vector<CameraLayer_Aravis::cam_info> infoList;
  CameraLayer_Aravis::listDevices(infoList, false);
  for (int i = 0; i < infoList.size(); i++)
  {
    if (infoList[i].id.compare(device_id) == 0)
    {
      return infoList[i];
    }
  }
  CameraLayer_Aravis::cam_info ci;
  return ci;
}

static CameraLayer_Aravis::cam_info getDeviceInfo(int index, bool tryOpen)
{
  const char *device_id = arv_get_device_id(index);

  CameraLayer_Aravis::cam_info ci;
  ci.id = string(device_id);
  ci.physical_id = string(arv_get_device_physical_id(index));
  ci.protocol = string(arv_get_device_protocol(index));
  ci.serial_nbr = string(arv_get_device_serial_nbr(index));
  ci.vendor = string(arv_get_device_vendor(index));
  ci.address = string(arv_get_device_address(index));
  ci.model = string(arv_get_device_model(index));
  ci.available = false;
  if (tryOpen)
  {
    GError *error = NULL;
    ArvDevice *device = arv_open_device(device_id, &error);
    if (ARV_IS_DEVICE(device))
    {
      ci.available = true;
      g_object_unref(device);
    }
    if (error)
      g_clear_error(&error);
  }
  return ci;
}


int CameraLayer_Aravis::listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist)
{
  std::vector<cam_info> tmpList;
  listDevices(tmpList,true);
  for(int i=0;i<tmpList.size();i++)
  {
    CameraLayer::BasicCameraInfo info;
    
    info.name=tmpList[i].physical_id;
    info.id=tmpList[i].id;
    info.model=tmpList[i].model;
    info.serial_number=tmpList[i].serial_nbr;
    info.vender=tmpList[i].vendor;
    info.ctx=NULL;
    info.driver_name=getDriverName();
    devlist.push_back(info);
  }

  return tmpList.size();


}




void CameraLayer_Aravis::s_STREAM_NEW_BUFFER_CB(ArvStream *stream, CameraLayer_Aravis *self)
{
  self->STREAM_NEW_BUFFER_CB(stream);
}

void empty_stream(ArvStream *stream)
{
  ArvBuffer *buffer = NULL;
  while (buffer = arv_stream_try_pop_buffer(stream))
  {
    arv_stream_push_buffer(stream, buffer);
  }
  return;
}


CameraLayer::FrameExtractPixelFormat CameraLayer_Aravis::GetFrameFormat()
{

  ArvPixelFormat format = arv_buffer_get_image_pixel_format	(_frame_cache_buffer);

  if(format == ARV_PIXEL_FORMAT_MONO_8)
  {
    return CameraLayer::FrameExtractPixelFormat::Mono;
  }

  if(format == ARV_PIXEL_FORMAT_BAYER_GR_8)
  {
    return CameraLayer::FrameExtractPixelFormat::Bayer_GR;
  }

  return CameraLayer::FrameExtractPixelFormat::RGB;
}


CameraLayer::status CameraLayer_Aravis::ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount)
{

  LOGI(">>>>");
  if (_frame_cache_buffer == NULL)
  {
    return NAK;
  }

  
  gint x, y, w, h;
  arv_buffer_get_image_region(_frame_cache_buffer, &x, &y, &w, &h);
  if (pixelCount < w * h)
  {
    return NAK;
  }

  
  size_t img_size = 0;
  uint8_t *img_dat = (uint8_t *)arv_buffer_get_data(_frame_cache_buffer, &img_size);
  
  ArvPixelFormat format = arv_buffer_get_image_pixel_format	(_frame_cache_buffer);

  LOGI(">>>>format:%0X  img_size:%d  wh:%d,%d",format,img_size,w,h);
  if(format == ARV_PIXEL_FORMAT_MONO_8)
  {
    

    for(int i=0;i<h;i++)
    {
      uint8_t* src_Pix_Gray=img_dat+i*w;
      uint8_t* tar_Pix=imgBuffer+(i*w)*channelCount;
      for(int j=0;j<w;j++)
      {
        for(int k=0;k<channelCount;k++)
        {
          *tar_Pix=*src_Pix_Gray;
          tar_Pix++;
        }
        src_Pix_Gray++;
      }
    }
    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  else if(format == ARV_PIXEL_FORMAT_BAYER_GR_8)
  {
    if(channelCount!=3)return NAK;


    //G R G R G R
    //B G B G B G
    //G R G R G R
    //B G B G B G
    //G R G R G R

    // int h2=h-2;
    // int w2=w-2;


    for(int i=1;i<h-2;i++)
    {
      int skipX=1;
      uint8_t* bayerPix=img_dat+i*w+skipX;
      uint8_t* bayerPix_NL=img_dat+(i+1)*w+skipX;
      uint8_t* bayerPix_PL=img_dat+(i-1)*w+skipX;
      uint8_t* tarPix=imgBuffer+((i*w)+skipX)*channelCount;

      int y_type=2*(i&1);
      for(int j=1;j<w-2;j++)
      {
        
        int x_type=j&1;
        int pix_Type=x_type+y_type;
        int R=0,G=0,B=0;

        if(pix_Type==0)
        {
          
          //B G B G B G
          //G R[G]R G R
          //B G B G B G
          G=(bayerPix[0]+bayerPix_NL[-1]+bayerPix_NL[1]+bayerPix_PL[-1]+bayerPix_PL[1])/5;
          R=(bayerPix[1]+bayerPix[-1])/2;
          B=(bayerPix_NL[0]+bayerPix_PL[0])/2;

        }
        else if(pix_Type==1)
        {
          
          //B G B G B G
          //G R G[R]G R
          //B G B G B G
          G=(bayerPix[1]+bayerPix[-1]+bayerPix_NL[0]+bayerPix_PL[0])/4;
          R=bayerPix[0];
          B=(bayerPix_NL[-1]+bayerPix_NL[1]+bayerPix_PL[-1]+bayerPix_PL[1])/4;
        }
        else if(pix_Type==2)
        {
          //G R G R G R
          //B G[B]G B G
          //G R G R G R
          G=(bayerPix[1]+bayerPix[-1]+bayerPix_NL[0]+bayerPix_PL[0])/4;
          B=bayerPix[0];
          R=(bayerPix_NL[-1]+bayerPix_NL[1]+bayerPix_PL[-1]+bayerPix_PL[1])/4;
        }
        else//3
        {
          //G R G R G R
          //B G B[G]B G
          //G R G R G R
          G=(bayerPix[0]+bayerPix_NL[-1]+bayerPix_NL[1]+bayerPix_PL[-1]+bayerPix_PL[1])/5;
          B=(bayerPix[1]+bayerPix[-1])/2;
          R=(bayerPix_NL[0]+bayerPix_PL[0])/2;
        }






        tarPix[0]=B;
        tarPix[1]=G;
        tarPix[2]=R;
        bayerPix++;
        bayerPix_NL++;
        bayerPix_PL++;
        tarPix+=channelCount;
      }

    }
    return ACK;
  }
  else
  {
    return NAK;
  }
  // LOGI("img.size:%d ", img_size);


  return ACK;
}

void CameraLayer_Aravis::STREAM_NEW_BUFFER_CB(ArvStream *stream)
{

  if (takeCount >= 0)
    LOGI(">>>>>takeCount:%d", takeCount);

  if (takeCount == 0)
  {
    arv_camera_stop_acquisition(camera, NULL);
    acquisition_started=false;
    empty_stream(stream);
    return;
  }

  ArvBuffer *buffer = arv_stream_try_pop_buffer(stream);
  if (buffer == NULL)
  {
    LOGI("buffer pop failed...");
    return;
  }

  int bufferStatus = arv_buffer_get_status(buffer);
  if (bufferStatus == ARV_BUFFER_STATUS_SUCCESS || bufferStatus == ARV_BUFFER_STATUS_SIZE_MISMATCH)
  {

    _frame_cache_buffer = buffer;


    gint x, y, w, h;
    arv_buffer_get_image_region(buffer, &x, &y, &w, &h);
    guint64 timeStamp_us = arv_buffer_get_timestamp(buffer);
    frameInfo _fi;
    _fi.timeStamp_us = timeStamp_us;
    _fi.offset_x = x;
    _fi.offset_y = y;
    _fi.width = w;
    _fi.height = h;
    fi=_fi;
    callback(*this, CameraLayer::EV_IMG, context);

    if (takeCount > 0)
    {
      takeCount--;
    }

    if (takeCount == 0)
    {
      arv_camera_stop_acquisition(camera, NULL);
      acquisition_started=false;
    }
  }
  else
  {
    LOGE("bufferStatus:%d", bufferStatus);
    // data->error_count++;
    frameInfo _fi = {
      timeStamp_us : 0,
      width : 0,
      height : 0,
      offset_x : 0,
      offset_y : 0,
    };
    fi = _fi;
    callback(*this, CameraLayer::EV_ERROR, context);
  }
  arv_stream_push_buffer(stream, buffer);
  _frame_cache_buffer = NULL;
  // LOGI("buffer status:%d has chunks:%d",arv_buffer_get_status (buffer),arv_buffer_has_chunks (buffer));
  // if (arv_buffer_get_status (buffer) == ARV_BUFFER_STATUS_SUCCESS) {
  //   size_t size = 0;
  //   uint8_t* dat=(uint8_t*)arv_buffer_get_data (buffer, &size);
  //   LOGI("data.len:%d ",size);
  //   // data->transferred += size;
  // } else {
  //   // data->error_count++;
  // }

  // if (chunks != NULL && arv_buffer_has_chunks (buffer) ) {
  //   int i;

  //   for (i = 0; chunks[i] != NULL; i++) {

  //     LOGI("chunks[%d]:%s ",i,chunks[i] );
  //     gint64 integer_value;
  //     GError *error = NULL;

  //     integer_value = arv_chunk_parser_get_integer_value (chunk_parser, buffer, chunks[i], &error);
  //     if (error == NULL)
  //       g_print ("%s = %" G_GINT64_FORMAT "\n", chunks[i], integer_value);
  //     else {
  //       double float_value;

  //       g_clear_error (&error);
  //       float_value = arv_chunk_parser_get_float_value (chunk_parser, buffer, chunks[i], &error);
  //       if (error == NULL)
  //         g_print ("%s = %g\n", chunks[i], float_value);
  //       else
  //         g_clear_error (&error);
  //     }
  //   }
  // }

  /* Image processing here */
}

void CameraLayer_Aravis::s_STREAM_CONTROL_LOST_CB(ArvStream *stream, CameraLayer_Aravis *self)
{
  self->STREAM_CONTROL_LOST_CB(stream);
}

void CameraLayer_Aravis::STREAM_CONTROL_LOST_CB(ArvStream *stream)
{
  LOGI("CTRL lost");
}

static void
stream_cb(void *user_data, ArvStreamCallbackType type, ArvBuffer *buffer)
{
  bool arv_option_realtime = false;
  bool arv_option_high_priority = true;
  if (type == ARV_STREAM_CALLBACK_TYPE_INIT)
  {
    if (arv_option_realtime)
    {
      if (!arv_make_thread_realtime(10))
        printf("Failed to make stream thread realtime\n");
    }
    else if (arv_option_high_priority)
    {
      if (!arv_make_thread_high_priority(-10))
        printf("Failed to make stream thread high priority\n");
    }
  }
}

CameraLayer_Aravis::CameraLayer_Aravis(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context): CameraLayer(camInfo,misc,cb, context)
{
  if (camInfo.driver_name != CameraLayer_Aravis::getDriverName())
  {
    
    string excpMsg = "param_driver_name:" + camInfo.driver_name + " != "+CameraLayer_Aravis::getDriverName();
    throw std::invalid_argument(excpMsg);
  }
  self_info = getDeviceIndex(camInfo.id.c_str());
  GError *error = NULL;
  camera = arv_camera_new(camInfo.id.c_str(), &error);
  if (camera == NULL)
  {
    string excpMsg = "deviceID:" + camInfo.id + " is not available";
    throw std::invalid_argument(excpMsg);
  }

  // SetROI(0,0,200,200,0,0);
  if (arv_camera_is_uv_device(camera))
  {
    guint min, max;

    arv_camera_uv_get_bandwidth_bounds(camera, &min, &max, NULL);
    printf("uv bandwidth limit    = %d [%d..%d]\n", arv_camera_uv_get_bandwidth(camera, NULL), min, max);
  }
  if (arv_camera_is_gv_device(camera))
  {
    printf("gv n_stream channels  = %d\n", arv_camera_gv_get_n_stream_channels(camera, NULL));
    printf("gv current channel    = %d\n",
           arv_camera_gv_get_current_stream_channel(camera, NULL));
    g_print("gv packet delay       = %" G_GINT64_FORMAT " ns\n",
            arv_camera_gv_get_packet_delay(camera, NULL));
    printf("gv packet size        = %d bytes\n", arv_camera_gv_get_packet_size(camera, NULL));
  }

  arv_camera_set_trigger(camera, "Software", NULL);

  stream = arv_camera_create_stream(camera, stream_cb, NULL, NULL);

  // arv_camera_set_chunk_mode(camera,true,NULL);

  LOGI(">>>>");
  chunk_parser = NULL;
  chunks = NULL;

  //The chunk data is not available for now
  const char *arv_option_chunks = NULL; //"Timestamp";
  if (arv_option_chunks)
  {
    arv_camera_set_chunks(camera, arv_option_chunks, &error);
    if (error)
    {
      LOGI("arv_camera_set_chunks ERR>:%d %d %s", error->domain, error->code, error->message);

      g_clear_error(&error);
    }
    else if (arv_option_chunks != NULL)
    {
      char *striped_chunks = g_strdup(arv_option_chunks);
      arv_str_strip(striped_chunks, " ,:;", ',');
      chunks = g_strsplit_set(striped_chunks, ",", -1);
      g_free(striped_chunks);

      chunk_parser = arv_camera_create_chunk_parser(camera);

      for (int i = 0; chunks[i] != NULL; i++)
      {
        char *chunk = g_strdup_printf("Chunk%s", chunks[i]);

        g_free(chunks[i]);
        chunks[i] = chunk;
      }
    }
  }
  // SetROI(20, 20, 500, 500, 0, 0); //reset the ROI
  
  SetROI(0,0,999999,999999,0,0);
  payloadSize = arv_camera_get_payload(camera, NULL);

  LOGI("payloadSize:%d", payloadSize);
  if (stream != NULL)
  { //push 1 buffer for now
    for (int i = 0; i < 1; i++)
    {
      arv_stream_push_buffer(stream, arv_buffer_new(payloadSize, NULL));
    }
  }
  else
  {

    g_object_unref(camera);
    string excpMsg = "deviceID:" + camInfo.id + " stream creation failed";
    throw std::invalid_argument(excpMsg);
  }

  g_signal_connect(stream, "new-buffer", G_CALLBACK(s_STREAM_NEW_BUFFER_CB), this);

  arv_stream_set_emit_signals(stream, TRUE);
  g_signal_connect(arv_camera_get_device(camera), "control-lost",
                   G_CALLBACK(s_STREAM_CONTROL_LOST_CB), NULL);

  arv_camera_set_acquisition_mode (camera, ARV_ACQUISITION_MODE_CONTINUOUS, NULL);
  arv_camera_set_exposure_time_auto (camera,ARV_AUTO_OFF,NULL);
  
  // arv_c  amera_get_binning (camera, &dx, &dy, NULL);

  //usb/ethernet device
  if (arv_camera_is_uv_device(camera))
  {
    guint min, max;

    arv_camera_uv_get_bandwidth_bounds(camera, &min, &max, NULL);
    printf("uv bandwidth limit    = %d [%d..%d]\n", arv_camera_uv_get_bandwidth(camera, NULL), min, max);
  }

  if (arv_camera_is_gv_device(camera))
  {
    printf("gv n_stream channels  = %d\n", arv_camera_gv_get_n_stream_channels(camera, NULL));
    printf("gv current channel    = %d\n",
           arv_camera_gv_get_current_stream_channel(camera, NULL));
    g_print("gv packet delay       = %" G_GINT64_FORMAT " ns\n",
            arv_camera_gv_get_packet_delay(camera, NULL));
    printf("gv packet size        = %d bytes\n", arv_camera_gv_get_packet_size(camera, NULL));
  }



  {
    char buff[1000];
    snprintf(buff, sizeof(buff),
             "{\
      \"type\":\"CameraLayer_Aravis\",\
      \"id\":\"%s\",\
      \"physical_id\":\"%s\",\
      \"address\":\"%s\",\
      \"model\":\"%s\",\
      \"protocol\":\"%s\",\
      \"serial_nbr\":\"%s\",\
      \"vendor\":\"%s\"\
    }",
            

    self_info.id.c_str(),
    self_info.physical_id.c_str(),
    self_info.address.c_str(),
    self_info.model.c_str(),
    self_info.protocol.c_str(),
    self_info.serial_nbr.c_str(),
    self_info.vendor.c_str()
    );
    cam_json_info.assign(buff);
  }
  //

  // CameraLayer_Aravis::cam_info &ci=self_info;
  // LOGI("===============");

  // LOGI("id:%s",ci.id.c_str());
  // LOGI("physical_id:%s",ci.physical_id.c_str());
  // LOGI("address:%s",ci.address.c_str());
  // LOGI("model:%s",ci.model.c_str());
  // LOGI("protocol:%s",ci.protocol.c_str());
  // LOGI("serial_nbr:%s",ci.serial_nbr.c_str());
  // LOGI("vendor:%s",ci.vendor.c_str());
  // LOGI("available:%d",ci.available);
}

CameraLayer_Aravis::cam_info CameraLayer_Aravis::getCameraInfo()
{
  return self_info;
}

void CameraLayer_Aravis::listDevices(vector<cam_info> &ret_infoList, bool tryOpen)
{
  ret_infoList.resize(0);
  arv_update_device_list();
  int n_devices = arv_get_n_devices();

  if (n_devices > 0)
  {
    for (int i = 0; i < n_devices; i++)
    {
      struct cam_info ci = getDeviceInfo(i, tryOpen);
      ret_infoList.push_back(ci);
    }
  }
}

CameraLayer_Aravis::~CameraLayer_Aravis()
{
  g_object_unref(camera);
  camera = NULL;
  if (stream)
  {
    g_object_unref(stream);
    camera = NULL;
  }
}
CameraLayer::status CameraLayer_Aravis::SetMirror(int Dir, int en)
{

  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::SetROIMirror(int Dir, int en)
{
  return CameraLayer::NAK;
}

// bool lockXXX=false;
CameraLayer::status CameraLayer_Aravis::SetROI(int x, int y, int w, int h, int zw, int zh)
{

  // if(lockXXX)return ACK;
  // lockXXX=true;



  int max_w, max_h;
  arv_camera_get_sensor_size(camera, &max_w, &max_h, NULL);

  if (x >= max_w || y >= max_h || w < 0 || h < 0)
  {
    return CameraLayer::NAK;
  }
  if (x < 0)
  {
    w += x;
    x = 0;
  }
  if (y < 0)
  {
    h += y;
    y = 0;
  }

  if (x + w > max_w)
  {
    w = max_w - x;
  }

  if (y + h > max_h)
  {
    h = max_h - y;
  }
  // x=0;
  // // y=0;
  // w=max_w;

  GError *err = NULL;

  if(acquisition_started)
  {
    arv_camera_stop_acquisition (camera, &err);
    if (err)
    {
      LOGI("ERR code:%d msg:%s", err->code, err->message);
      g_clear_error(&err);
    }
    // sleep(1);
    this_thread::sleep_for(chrono::milliseconds(1000) );
  }

  gint	xo_inc = arv_camera_get_x_offset_increment	(camera,NULL);
  gint	yo_inc = arv_camera_get_y_offset_increment	(camera,NULL);
  gint	w_inc = arv_camera_get_width_increment	(camera,NULL);
  gint	h_inc = arv_camera_get_height_increment	(camera,NULL);

  x=x/xo_inc*xo_inc;
  y=y/yo_inc*yo_inc;
  w=w/w_inc*w_inc;
  h=h/h_inc*h_inc;



  LOGI("xywh:%d,%d %d,%d >+>%d,%d %d,%d ", x, y, w, h,xo_inc,yo_inc,w_inc,h_inc);



  arv_camera_set_region(camera, (int)x, (int)y, (int)w, (int)h, &err);
  if (err)
  {
    LOGI("ERR code:%d msg:%s", err->code, err->message);
    g_clear_error(&err);
    
    
    if(acquisition_started)
    {
      arv_camera_start_acquisition (camera, &err);
      if (err)
      {
        LOGI("ERR code:%d msg:%s", err->code, err->message);
        g_clear_error(&err);
      }
    }
    return CameraLayer::NAK;
  }

  GetROI(&x, &y, &w, &h, NULL, NULL);
  LOGI("xywh:%d,%d %d,%d", x, y, w, h);


    
  if(acquisition_started)
  {
    arv_camera_start_acquisition (camera, &err);
    if (err)
    {
      LOGI("ERR code:%d msg:%s", err->code, err->message);
      g_clear_error(&err);
    }
  }

  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_Aravis::GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh)
{
  GError *err = NULL;
  int _x, _y, _w, _h;
  arv_camera_get_region(camera, &_x, &_y, &_w, &_h, &err);

  if (err)
  {
    g_clear_error(&err);
    return CameraLayer::NAK;
  }
  if (x)
    *x = _x;
  if (y)
    *y = _y;
  if (w)
    *w = _w;
  if (h)
    *h = _h;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::TriggerMode(int type)
{
  CameraLayer::TriggerMode(type);
  GError *err = NULL;




    {
      arv_camera_stop_acquisition(camera, &err);
      if(err)
      {
        LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        return CameraLayer::NAK;
      }
    }


  takeCount = -1;
  //0 for continuous, 1 for soft trigger, 2 for HW trigger
  if (type == 0)
  {

    {
      arv_camera_set_string (camera, "TriggerMode", "Off", &err);
      if(err)
      {
        LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        return CameraLayer::NAK;
      }
    }



  }
  else
  {

    {
      arv_camera_set_string (camera, "TriggerMode", "On", &err);
      if(err)
      {
        LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        return CameraLayer::NAK;
      }
    }

    arv_camera_set_string (camera, "TriggerSource", "Anyway", &err);
    if(err)
    {
      // LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      // err=NULL;
      // g_clear_error(&err);
      // return CameraLayer::NAK;
    }
  }

  {
    arv_camera_start_acquisition(camera, &err);
    if(err)
    {
      LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      err=NULL;
      g_clear_error(&err);
      return CameraLayer::NAK;
    }
    acquisition_started=true;
  }

  // if (type == 1)
  // {
  //   arv_camera_set_trigger(camera, "Software", &err);
  // }
  // else
  // {
  //   arv_camera_set_trigger(camera, "Line1", &err);
  // }
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::RUN()
{
  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::TriggerCount(int count)
{
  takeCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_Aravis::Trigger()
{
  GError *err = NULL;
  if (takeCount >= 0)
    takeCount++;
  arv_camera_software_trigger (camera,&err);

  // arv_camera_start_acquisition(camera, NULL);
  acquisition_started=true;
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetResolution(int width, int height)
{

  return CameraLayer::NAK;
}
CameraLayer::status CameraLayer_Aravis::SetAnalogGain(float gain)
{
  GError *err = NULL;
  bool isAva = arv_camera_is_gain_available(camera, &err);
  if (err != NULL)
  {
    g_clear_error(&err);
    return CameraLayer::NAK;
  }
  if (!isAva)
  {
    return CameraLayer::NAK;
  }
  arv_camera_set_gain(camera, gain, &err); //
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetOnceWB()
{
  
  GError *err = NULL;
  arv_camera_execute_command 		(camera, "WBOnce",&err);
  
  if (err != NULL)
  {
    
    LOGI("WBOnce failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  
  // arv_camera_set_integer(camera,"RGain",1200,NULL);
  // arv_camera_set_integer(camera,"GGain",1200,NULL);
  // arv_camera_set_integer(camera,"BGain",1200,NULL);

  
  {
    int rg=arv_camera_get_integer(camera, "RGain", NULL);
    int gg=arv_camera_get_integer(camera, "GGain", NULL);
    int bg=arv_camera_get_integer(camera, "BGain", NULL);

    printf("CameraLayer_Aravis::SetOnceWB:RGBGain  %d %d %d\n",rg,gg,bg);
    // return CameraLayer::NAK;
  }


  return CameraLayer::ACK;
}



CameraLayer::status CameraLayer_Aravis::SetRGain(float gain)
{
  GError *err = NULL;
  arv_camera_set_integer(camera,"RGain",(int)gain,&err);

  if (err != NULL)
  {
    
    LOGI("WBOnce failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;

  
}
CameraLayer::status CameraLayer_Aravis::SetGGain(float gain)
{
  GError *err = NULL;
  arv_camera_set_integer(camera,"GGain",(int)gain,&err);

  if (err != NULL)
  {
    
    LOGI("failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;

  
}
CameraLayer::status CameraLayer_Aravis::SetBGain(float gain)
{
  GError *err = NULL;
  arv_camera_set_integer(camera,"BGain",(int)gain,&err);

  if (err != NULL)
  {
    
    LOGI("failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;

  
}

CameraLayer::status CameraLayer_Aravis::SetFrameRate(float frame_rate)
{

  GError *err = NULL;

  if(frame_rate!=frame_rate || frame_rate<0)
  {
      
    {

      arv_camera_set_boolean (camera, "AcquisitionFrameRateEnable", false, &err);

      if (err != NULL)
      {
        
        LOGD("AcquisitionFrameRateEnable:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        g_clear_error(&err);

        return CameraLayer::NAK;
        // return CameraLayer::NAK;
      }
      
    }
    return CameraLayer::ACK;
  }

  

  double min=NAN,max=NAN;
  {

    arv_camera_get_frame_rate_bounds (camera, &min, &max, &err);

    if (err != NULL)
    {
      
      LOGD("FrameRateBound:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      g_clear_error(&err);
      return CameraLayer::NAK;
      // return CameraLayer::NAK;
    }

    LOGD("M:%f m:%f\n",max,min);
  }

  if(frame_rate==0)frame_rate=min;  
  else if(isinf(frame_rate))frame_rate=max;
  else if(frame_rate<min || frame_rate>max)return CameraLayer::NAK;




  {

    arv_camera_set_boolean (camera, "AcquisitionFrameRateEnable", true, &err);

    if (err != NULL)
    {
      
      LOGD("AcquisitionFrameRateEnable:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      g_clear_error(&err);

      // return CameraLayer::NAK;
    }
  }





  arv_camera_set_float (camera,"AcquisitionFrameRate", frame_rate, &err);

  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  LOGD("SetFrameRate:d%d c:%d m:%s\n",err->domain,err->code,err->message);
  g_clear_error(&err);
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::GetAnalogGain(int *ret_min, int *ret_max)
{
  GError *err = NULL;
  double min, max;
  arv_camera_get_gain_bounds(camera,
                             &min,
                             &max, &err);

  if (ret_min)
  {
    *ret_min = (int)min;
  }
  if (ret_max)
  {
    *ret_max = (int)max;
  }
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::SetExposureTime(float time_us)
{
  GError *err = NULL;
  arv_camera_set_exposure_time(camera, time_us, &err);
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_Aravis::GetExposureTime(float *ret_time_ms)
{
  GError *err = NULL;
  double time = arv_camera_get_exposure_time(camera, &err);
  if (ret_time_ms)
    *ret_time_ms = time;
  if (err == NULL)
  {
    return CameraLayer::ACK;
  }
  g_clear_error(&err);
  return CameraLayer::NAK;
}