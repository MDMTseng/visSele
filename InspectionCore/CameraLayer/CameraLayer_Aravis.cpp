
#include "CameraLayer_Aravis.hpp"
#include "logctrl.h"
#include <arv.h>
#include <stdexcept>
LOG_MODULE("cam.aravis");

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

// static: this was a global symbol in a file full of them, one collision away
// from an ODR problem at link time.
static void empty_stream(ArvStream *stream)
{
  ArvBuffer *buffer = NULL;
  while ((buffer = arv_stream_try_pop_buffer(stream)) != NULL)
  {
    arv_stream_push_buffer(stream, buffer);
  }
  return;
}


// Bounded because the base class never reads or clears error_code_list: on a
// link that is dropping packets it grew by one entry per bad frame for the
// lifetime of the process. Keeping the most recent entries is what a human
// debugging a fault actually wants anyway.
void CameraLayer_Aravis::pushErrorCode(uint64_t code)
{
  if (error_code_list.size() >= ERROR_LIST_MAX)
    error_code_list.erase(error_code_list.begin(),
                          error_code_list.begin() + (error_code_list.size() - ERROR_LIST_MAX / 2));
  error_code_list.push_back(code);
}

// See the header for why this is calibrated rather than hardcoded.
uint64_t CameraLayer_Aravis::deviceTimestampToUs(uint64_t dev_ticks, uint64_t sys_ns)
{
  if (!_ts_calibrated)
  {
    if (_ts_calib_n == 0)
    {
      _ts_calib_dev0 = dev_ticks;
      _ts_calib_sys0 = sys_ns;
      _ts_calib_n = 1;
    }
    else if (++_ts_calib_n >= TS_CALIB_FRAMES &&
             dev_ticks > _ts_calib_dev0 && sys_ns > _ts_calib_sys0)
    {
      double dev_d = (double)(dev_ticks - _ts_calib_dev0);
      double sys_us = (double)(sys_ns - _ts_calib_sys0) / 1000.0;
      double est = (sys_us > 0.0) ? dev_d / sys_us : 0.0;

      // Snap to the conventions that actually exist. The true rate is one of
      // these; snapping keeps the divisor exact instead of baking in the
      // (host-clock-limited) error of the estimate.
      static const double kRates[] = { 1000.0, 100.0, 1.0 };  // ns, 10ns, us ticks
      double best = 1000.0, best_err = 1e30;
      for (double r : kRates)
      {
        double e = (est > r) ? est / r : r / est;
        if (e < best_err) { best_err = e; best = r; }
      }
      // Only accept a plausible match; otherwise keep the ns default and say so.
      if (est > 0.0 && best_err < 1.5)
      {
        _ts_ticks_per_us = best;
        LOGI("device timestamp: measured %.4f ticks/us -> using %.0f (%.1f ns/tick)",
             est, best, 1000.0 / best);
      }
      else
      {
        LOGE("device timestamp: measured %.4f ticks/us matches no known convention "
             "-- falling back to nanoseconds; frame timestamps may be mis-scaled", est);
      }
      _ts_calibrated = true;
    }
  }

  // Until the rate is known there is no honest answer, so say so rather than
  // divide by the assumed default.
  //
  // The default is 1000 (ns) and this camera is 100 (10ns), so every frame
  // before calibration completed came out 10x too small. Measured on the real
  // plate 2026-08-05: the first sync sample read 216,513,611 where its
  // neighbours read 2,382,734,242 +/- 25 -- an outlier 2166 SECONDS wide next
  // to samples that agreed with each other to 49us. It was outvoted by the
  // median that time, but the guard on both sides is a majority vote, so a
  // camera that starts streaming just as pulses begin can fill a whole
  // bootstrap window with these and converge on nonsense.
  //
  // 0 is the "unknown" sentinel the whole stack already honours:
  // PerifTriggerPairing::pairFrame falls back to positional on it,
  // announcementLost() declines to judge, and the firmware's
  // CamClockSync::observe drops the sample. Those frames simply pair by
  // position, which is what they did before timestamps existed.
  if (!_ts_calibrated) return 0;

  return (uint64_t)(dev_ticks / _ts_ticks_per_us);
}

// Reads back what the camera actually settled on, rather than what we asked
// for. The sibling HikRobot layer has had this for a while; Aravis did not, and
// its absence is why a mis-set trigger here looks identical to a dead wire:
// every write "succeeds" (or its GError is dropped) and nothing reports that
// the camera is in a state that cannot produce frames.
void CameraLayer_Aravis::logCameraState(const char *when)
{
  static const char *kStr[] = { "TriggerSelector", "TriggerMode", "TriggerSource",
                                "TriggerActivation", "AcquisitionMode" };
  for (size_t i = 0; i < sizeof(kStr)/sizeof(kStr[0]); i++)
  {
    GError *e = NULL;
    const char *v = arv_camera_get_string(camera, kStr[i], &e);
    LOGI("[cam state %s] %-18s = %s%s", when, kStr[i],
         e ? "<err> " : (v ? v : "(null)"), e ? e->message : "");
    if (e) g_clear_error(&e);
  }

  {
    GError *e = NULL;
    gint64 burst = arv_camera_get_integer(camera, "AcquisitionBurstFrameCount", &e);
    LOGI("[cam state %s] %-18s = %lld%s", when, "BurstFrameCount",
         (long long)burst, e ? " <err>" : "");
    if (e) g_clear_error(&e);

    e = NULL;
    gboolean fre = arv_camera_get_boolean(camera, "AcquisitionFrameRateEnable", &e);
    LOGI("[cam state %s] %-18s = %d%s", when, "FrameRateEnable", (int)fre, e ? " <err>" : "");
    if (e) g_clear_error(&e);

    e = NULL;
    double rfr = arv_camera_get_float(camera, "ResultingFrameRate", &e);
    LOGI("[cam state %s] %-18s = %.2f fps%s", when, "ResultingFrameRate", rfr, e ? " <err>" : "");
    if (e) g_clear_error(&e);
  }

  gint x=0,y=0,w=0,h=0;
  arv_camera_get_region(camera, &x, &y, &w, &h, NULL);
  LOGI("[cam state %s] region = %d,%d %dx%d  payload=%d  acquiring=%d",
       when, x, y, w, h, payloadSize, (int)acquisition_started);
}

CameraLayer::FrameExtractPixelFormat CameraLayer_Aravis::GetFrameFormat()
{
  // _frame_cache_buffer is only live inside the EV_IMG callback; everywhere
  // else it is NULL, and this used to hand that NULL straight to Aravis.
  if (_frame_cache_buffer == NULL)
  {
    return CameraLayer::FrameExtractPixelFormat::RGB;
  }

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


// Whether the camera is still THERE, asked of the camera.
//
// This used to be `return ACK;` under a //TODO. Nothing else in Core0_1 ever
// checks either -- EV_CTRL_LOST is delivered to a callback that drops every
// non-image event -- so "is the camera connected?" had exactly one answer in
// the whole system, and it was yes. Unplug the camera, reload the WebUI, and
// the panel would still show its id, model and serial: cam_json_info is
// captured at construction and this said the device was fine, so every layer
// downstream agreed on a camera that was physically not attached.
//
// Two checks, cheapest first:
//   1. control-lost already fired -- Aravis told us the device went away.
//      Authoritative and free.
//   2. a real register read. Needed because a USB device can be yanked without
//      the control-lost signal arriving promptly, and because this is also the
//      only way to notice a camera that came back on a different handle.
CameraLayer::status CameraLayer_Aravis::isInOperation()
{
    if (_ctrl_lost || camera == NULL)
        return CameraLayer::NAK;

    // One register read. On a live USB3 link this is well under a millisecond;
    // on a dead one it fails rather than hanging, which is the entire point.
    GError *error = NULL;
    arv_camera_get_integer(camera, "Width", &error);
    if (error)
    {
        LOGE("camera probe failed (%s) -- treating as disconnected",
             error->message ? error->message : "no message");
        g_clear_error(&error);
        // Latch it: once a probe has failed the object is not going to heal
        // itself, and re-probing a dead device on every UI poll is a stall
        // waiting to happen.
        _ctrl_lost = true;
        acquisition_started = false;
        return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
}


CameraLayer::status bayerToRGB(uint8_t* img_dat, uint8_t* imgBuffer, int width, int height, int channelCount, uint64_t format)
{
    if (channelCount != 3) return CameraLayer::NAK; // Only support RGB output

    int redRowStart, blueRowStart, redFirst;
    
    // Set parameters based on Bayer pattern
    switch (format) {
        case ARV_PIXEL_FORMAT_BAYER_RG_8:
            redRowStart = 0; // Red on even rows
            blueRowStart = 1; // Blue on odd rows
            redFirst = 1; // Red is on the first pixel of even rows
            break;
        case ARV_PIXEL_FORMAT_BAYER_GR_8:
            redRowStart = 0; // Red on even rows
            blueRowStart = 1; // Blue on odd rows
            redFirst = 0; // Green is on the first pixel of even rows
            break;
        case ARV_PIXEL_FORMAT_BAYER_GB_8:
            redRowStart = 1; // Red on odd rows
            blueRowStart = 0; // Blue on even rows
            redFirst = 0; // Green is on the first pixel of odd rows
            break;
        case ARV_PIXEL_FORMAT_BAYER_BG_8:
            redRowStart = 1; // Red on odd rows
            blueRowStart = 0; // Blue on even rows
            redFirst = 1; // Blue is on the first pixel of even rows
            break;
        default:
            return CameraLayer::NAK; // Invalid format
    }

    // Loop through the image (excluding the borders for safety)
    for (int i = 1; i < height - 1; i++) {
        int rowIsRed = (i % 2 == redRowStart); // Determine if current row is a "Red" row
        uint8_t* bayerRow = img_dat + i * width;
        uint8_t* prevRow = img_dat + (i - 1) * width;
        uint8_t* nextRow = img_dat + (i + 1) * width;
        uint8_t* rgbRow = imgBuffer + i * width * channelCount;

        for (int j = 1; j < width - 1; j++) {
            int colIsRed = (j % 2 == redFirst); // Determine if current pixel is a Red pixel
            int colIsBlue = !colIsRed; // If not Red, then it's Blue (or Green)

            int R = 0, G = 0, B = 0;

            // Determine pixel type and interpolate based on its neighbors
            if (rowIsRed && colIsRed) {
                // Red pixel
                R = bayerRow[j];
                G = (bayerRow[j - 1] + bayerRow[j + 1] + prevRow[j] + nextRow[j]) / 4;
                B = (prevRow[j - 1] + prevRow[j + 1] + nextRow[j - 1] + nextRow[j + 1]) / 4;
            } else if (!rowIsRed && colIsBlue) {
                // Blue pixel
                B = bayerRow[j];
                G = (bayerRow[j - 1] + bayerRow[j + 1] + prevRow[j] + nextRow[j]) / 4;
                R = (prevRow[j - 1] + prevRow[j + 1] + nextRow[j - 1] + nextRow[j + 1]) / 4;
            } else if (rowIsRed && !colIsRed) {
                // Green pixel in a Red row (Green-Red pixel)
                G = bayerRow[j];
                R = (bayerRow[j - 1] + bayerRow[j + 1]) / 2;
                B = (prevRow[j] + nextRow[j]) / 2;
            } else {
                // Green pixel in a Blue row (Green-Blue pixel)
                G = bayerRow[j];
                B = (bayerRow[j - 1] + bayerRow[j + 1]) / 2;
                R = (prevRow[j] + nextRow[j]) / 2;
            }

            // Store the result in the RGB buffer
            rgbRow[j * 3 + 0] = B;
            rgbRow[j * 3 + 1] = G;
            rgbRow[j * 3 + 2] = R;
        }
    }

    return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_Aravis::ExtractFrame(uint8_t *imgBuffer, int channelCount, size_t pixelCount)
{

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

  // Constant for the life of a session; one line per frame said nothing new.
  { static unsigned _lc = 0; if ((_lc++ % 200) == 0)
      LOGI(">>>>format:%0X  img_size:%d  wh:%d,%d",format,img_size,w,h); }
  if(format == ARV_PIXEL_FORMAT_MONO_8)
  {
    

    if(channelCount==3)
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
    }
    else if(channelCount==1)
    {
      memcpy(imgBuffer,img_dat, w * h*channelCount);
    }
    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  if(format == ARV_PIXEL_FORMAT_BGR_8_PACKED)
  {
    
    if(channelCount!=3)return NAK;
    // memcpy(imgBuffer,img_dat, w * h*3);

    memcpy(imgBuffer,img_dat, w * h*3);
    // LOGI("fi.timeStamp_us:%llu",fi.timeStamp_us);
    // LOGI("xywh:%d,%d %d,%d",x,y,w,h);

    // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
    return ACK;

  }
  else if(format == ARV_PIXEL_FORMAT_BAYER_GR_8 || format == ARV_PIXEL_FORMAT_BAYER_GB_8)
  {
    return bayerToRGB(img_dat, imgBuffer, w, h, channelCount, ARV_PIXEL_FORMAT_BAYER_GB_8);
  }
  else if(format == ARV_PIXEL_FORMAT_BAYER_RG_8)
  {

    if(channelCount != 3) return NAK;

    // Iterate over the image (excluding the borders)
    for(int i = 1; i < h - 2; i++)
    {
        int skipX = 1;
        uint8_t* bayerPix = img_dat + i * w + skipX;
        uint8_t* bayerPix_NL = img_dat + (i + 1) * w + skipX;
        uint8_t* bayerPix_PL = img_dat + (i - 1) * w + skipX;
        uint8_t* tarPix = imgBuffer + ((i * w) + skipX) * channelCount;

        int y_type = 2 * ((i - 1) & 1);  // Determine row type (even or odd)
        for(int j = 1; j < w - 2; j++)
        {
            int x_type = (j - 1) & 1;  // Determine column type (even or odd)
            int pix_Type = x_type + y_type;
            int R = 0, G = 0, B = 0;

            if(pix_Type == 0)
            {
                // Red pixel (even row, even column)
                R = bayerPix[0];
                G = (bayerPix[1] + bayerPix[-1] + bayerPix_NL[0] + bayerPix_PL[0]) / 4;
                B = (bayerPix_NL[1] + bayerPix_NL[-1] + bayerPix_PL[1] + bayerPix_PL[-1]) / 4;
            }
            else if(pix_Type == 1)
            {
                // Green pixel (even row, odd column)
                R = (bayerPix[-1] + bayerPix[1]) / 2;
                G = bayerPix[0];
                B = (bayerPix_NL[0] + bayerPix_PL[0]) / 2;
            }
            else if(pix_Type == 2)
            {
                // Green pixel (odd row, even column)
                R = (bayerPix_PL[0] + bayerPix_NL[0]) / 2;
                G = bayerPix[0];
                B = (bayerPix[-1] + bayerPix[1]) / 2;
            }
            else // pix_Type == 3
            {
                // Blue pixel (odd row, odd column)
                R = (bayerPix_PL[1] + bayerPix_PL[-1] + bayerPix_NL[1] + bayerPix_NL[-1]) / 4;
                G = (bayerPix[-1] + bayerPix[1] + bayerPix_NL[0] + bayerPix_PL[0]) / 4;
                B = bayerPix[0];
            }

            // Assign RGB values to the output buffer
            tarPix[0] = R;
            tarPix[1] = G;
            tarPix[2] = B;

            // Move to the next pixel
            bayerPix++;
            bayerPix_NL++;
            bayerPix_PL++;
            tarPix += channelCount;
        }
    }

    return ACK;
}


  else
  {
    LOGE("format:%0x transform is not impl yet.... ",format);
    return NAK;
  }
  // LOGI("img.size:%d ", img_size);


  return ACK;
}

void CameraLayer_Aravis::STREAM_NEW_BUFFER_CB(ArvStream *stream)
{
  frameCount++;
  if (takeCount >= 0)
    LOGI(">>>>>takeCount:%d", takeCount);

  if (takeCount == 0)
  {
    arv_camera_stop_acquisition(camera, NULL);
    acquisition_started=false;
    // Cleared on every exit from here on: leaving a stale pointer to a buffer
    // that has already gone back to the stream is how a later ExtractFrame
    // ends up reading a frame the driver is busy refilling.
    _frame_cache_buffer = NULL;
    empty_stream(stream);
    return;
  }

  ArvBuffer *buffer = arv_stream_try_pop_buffer(stream);
  if (buffer == NULL)
  {
    LOGI("buffer pop failed...");

    _frame_cache_buffer = NULL;
    pushErrorCode(130130131);
    callback(*this, CameraLayer::EV_ERROR, context);
    return;
  }

  int bufferStatus = arv_buffer_get_status(buffer);
  if (bufferStatus == ARV_BUFFER_STATUS_SUCCESS)// || bufferStatus == ARV_BUFFER_STATUS_SIZE_MISMATCH)
  {

    _frame_cache_buffer = buffer;


    gint x, y, w, h;
    arv_buffer_get_image_region(buffer, &x, &y, &w, &h);
    // Raw device ticks, whose unit is device-specific -- converted, not assumed.
    guint64 dev_ticks = arv_buffer_get_timestamp(buffer);
    guint64 sys_ns    = arv_buffer_get_system_timestamp(buffer);
    frameInfo _fi;
    _fi.timeStamp_us = deviceTimestampToUs(dev_ticks, sys_ns);
    _fi.offset_x = x;
    _fi.offset_y = y;
    _fi.width = w;
    _fi.height = h;
    _fi.pixel_size_mm=pixel_size_mm;

    // The camera's own frame counter. Aravis has always exposed this and the
    // layer never passed it on, so consumers could not tell a frame that was
    // never exposed from one lost in transport -- exactly the distinction
    // needed when chasing missing frames. (The HikRobot layer already does
    // this via nFrameNum; this brings Aravis in line.)
    _fi.frameNum = (uint32_t)arv_buffer_get_frame_id(buffer);
    _fi.frameNumValid = true;

    // Exposure-floor watch. The interval is taken from the DEVICE timestamp,
    // not the host clock: host arrival is smeared by transport and scheduling,
    // while the device stamp is when the sensor actually exposed -- which is
    // precisely the quantity that gets pinned to the floor.
    //
    // Only once the timestamp scale has settled. deviceTimestampToUs() starts
    // out assuming nanosecond ticks and corrects itself over the first frames,
    // so intervals before that are off by the calibration factor (10x on this
    // body) and the frame where it snaps shows a nonsense jump -- which the
    // floor check would read as a saturation episode.
    if (_ts_calibrated && _prev_frame_us != 0 &&
        _fi.timeStamp_us > _prev_frame_us)
    {
      _fi.interval_us = _fi.timeStamp_us - _prev_frame_us;
      if (_exposure_floor_watch && _floor_us > 0.0 &&
          (double)_fi.interval_us <= _floor_us * (1.0 + SATURATION_TOL))
      {
        _fi.rateSaturated = true;
        _saturated_run++;
        if (_saturated_run >= SATURATION_RUN_ALERT && !_saturation_reported)
        {
          _saturation_reported = true;
          LOGE("camera is being triggered faster than it can expose: %d frames "
               "in a row at the %.0fus floor (ResultingFrameRate limit). "
               "Triggers are being delayed or silently dropped -- frame "
               "timestamps no longer identify the trigger that caused them.",
               _saturated_run, _floor_us);
          pushErrorCode(130130140);
        }
      }
      else
      {
        _saturated_run = 0;
        _saturation_reported = false;
      }
    }
    // Left at 0 until the scale is trustworthy, so the first interval measured
    // after calibration cannot straddle the change.
    _prev_frame_us = _ts_calibrated ? _fi.timeStamp_us : 0;

    ArvPixelFormat format = arv_buffer_get_image_pixel_format	(_frame_cache_buffer);
    if(format==ARV_PIXEL_FORMAT_MONO_8)
    {
      _fi.channelCount=1;
      _fi.pixelBits=8;
    }
    // The `true ||` that used to lead this condition made the whole format
    // test dead code: EVERY non-Mono8 format was reported as 8-bit 3-channel,
    // including 16-bit, packed 12-bit and YUV. Consumers sized buffers from
    // that lie. Only the formats ExtractFrame can actually decode are claimed
    // as 3-channel now; anything else is reported honestly so the frame is
    // rejected instead of misread.
    else if(format==ARV_PIXEL_FORMAT_BGR_8_PACKED || format==ARV_PIXEL_FORMAT_BAYER_GR_8 || format==ARV_PIXEL_FORMAT_BAYER_GB_8 || format==ARV_PIXEL_FORMAT_BAYER_RG_8)
    {
      _fi.channelCount=3;//3 channel
      _fi.pixelBits=8*_fi.channelCount;
    }
    else
    {
      LOGE("unsupported pixel format %0X -- ExtractFrame cannot decode it", format);
      _fi.channelCount=0;
      _fi.pixelBits=0;
    }

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
    if(bufferStatus==ARV_BUFFER_STATUS_TIMEOUT||
    bufferStatus==ARV_BUFFER_STATUS_MISSING_PACKETS||
    bufferStatus==ARV_BUFFER_STATUS_WRONG_PACKET_ID||
    bufferStatus==ARV_BUFFER_STATUS_SIZE_MISMATCH||
    bufferStatus==ARV_BUFFER_STATUS_ABORTED
    )
    {

      pushErrorCode(bufferStatus);
    }
    // data->error_count++;
    // Was a GNU `field: value` designated initializer -- a non-standard C++
    // extension whose member order here did not even match the declaration
    // order in CameraLayer.hpp, and which silently left the remaining fields
    // (channelCount, pixelBits, pixel_size_mm, frameNum...) to the aggregate
    // rules. A value-initialized frameInfo says exactly the same thing --
    // "nothing valid in here" -- with none of the ambiguity, and it also
    // clears frameNumValid, which the old form did not name at all.
    frameInfo _fi = frameInfo();
    fi = _fi;
    callback(*this, CameraLayer::EV_ERROR, context);
  }
  // If the camera reports SIZE_MISMATCH the buffer was sized for the previous
  // payload (e.g. ROI grew). Recreate at the current payload before re-pushing
  // so we don't loop forever on the same bad buffer.
  if (bufferStatus == ARV_BUFFER_STATUS_SIZE_MISMATCH) {
    int cur_payload = arv_camera_get_payload(camera, NULL);
    if (cur_payload > 0) {
      payloadSize = cur_payload;
      g_object_unref(buffer);
      arv_stream_push_buffer(stream, arv_buffer_new(payloadSize, NULL));
      _frame_cache_buffer = NULL;
      return;
    }
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
  LOGE("CTRL lost %s",connection_data.name.c_str());

  pushErrorCode(130130130);

  // The one fact everything downstream needs, recorded where it can be asked
  // for. Without it isInOperation() had to guess, and it guessed ACK.
  _ctrl_lost = true;

  // The device is gone: every Aravis call from here on will fail. Record that
  // in our own state so the acquisition_started flag stops claiming the camera
  // is streaming, and drop the frame cache -- no further frames are coming and
  // the buffer it points at will never be handed back.
  acquisition_started = false;
  _frame_cache_buffer = NULL;

  // NOTE: still no reconnect attempt here -- the layer only reports upward and
  // leaves recovery to the owner, which is why a lost camera needs an explicit
  // teardown and re-construct rather than healing itself.
  callback(*this, CameraLayer::EV_CTRL_LOST, context);


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

  arv_camera_set_trigger(camera, "Software", NULL);



  
  {
    // This block used to query the camera's supported formats, log them, throw
    // the array away without g_free (a leak per construction), and then force
    // BGR_8_PACKED regardless of whether the camera offered it -- swallowing
    // the error, so construction carried on with an unknown format. It also
    // looped on n_pixel_formats without checking the array was non-NULL, so a
    // failed query dereferenced NULL a garbage number of times.
    GError *err = NULL;

    guint n_pixel_formats = 0;
    gint64 *formats = arv_camera_dup_available_pixel_formats(camera, &n_pixel_formats, NULL);

    bool has_mono8 = false, has_bgr8 = false;
    if (formats != NULL)
    {
      for (guint i = 0; i < n_pixel_formats; i++)
      {
        LOGI("supported pixel format: %0llX", (unsigned long long)formats[i]);
        if ((ArvPixelFormat)formats[i] == ARV_PIXEL_FORMAT_MONO_8)       has_mono8 = true;
        if ((ArvPixelFormat)formats[i] == ARV_PIXEL_FORMAT_BGR_8_PACKED) has_bgr8  = true;
      }
      g_free(formats);
    }
    else
    {
      LOGE("could not enumerate pixel formats -- leaving the camera on its current one");
    }

    // Preference unchanged (colour -> BGR8, mono stays Mono8); the difference
    // is that it is now chosen from what the camera actually advertises rather
    // than set blind. Must happen before arv_camera_create_stream.
    if (has_bgr8)
    {
      arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_BGR_8_PACKED, &err);
    }
    else if (has_mono8)
    {
      arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_8, &err);
    }

    if (err)
    {
      LOGE("set_pixel_format ERR code:%d msg:%s", err->code, err->message);
      g_clear_error(&err);
    }

    // Loud if we end up somewhere ExtractFrame cannot decode: the symptom
    // otherwise is a camera that opens cleanly and delivers nothing.
    ArvPixelFormat settled = arv_camera_get_pixel_format(camera, NULL);
    if (settled != ARV_PIXEL_FORMAT_MONO_8 &&
        settled != ARV_PIXEL_FORMAT_BGR_8_PACKED &&
        settled != ARV_PIXEL_FORMAT_BAYER_GR_8 &&
        settled != ARV_PIXEL_FORMAT_BAYER_GB_8 &&
        settled != ARV_PIXEL_FORMAT_BAYER_RG_8)
    {
      LOGE("camera settled on pixel format %0X, which ExtractFrame cannot decode "
           "-- frames will be rejected", settled);
    }
  }

  stream = arv_camera_create_stream(camera, stream_cb, NULL, NULL);
  
  // arv_camera_set_chunk_mode(camera,true,NULL);

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

  SetROI(0,0,999999,999999,0,0);//MAX ROI
  payloadSize = arv_camera_get_payload(camera, NULL);

  // GigE jumbo-frame negotiation: try to use the largest packet size the link
  // supports, falling back to a smaller size on failure. Big throughput win
  // when the network supports jumbo frames; harmless otherwise.
  if (arv_camera_is_gv_device(camera))
  {
    arv_camera_gv_set_packet_size_adjustment(camera, ARV_GV_PACKET_SIZE_ADJUSTMENT_ON_FAILURE);
    GError *psz_err = NULL;
    guint psz = arv_camera_gv_auto_packet_size(camera, &psz_err);
    if (psz_err) { LOGE("gv_auto_packet_size: %s", psz_err->message); g_clear_error(&psz_err); }
    else LOGI("gv packet size negotiated: %u bytes", psz);
  }

  LOGI("payloadSize:%d", payloadSize);
  if (stream != NULL)
  { // GigE/USB3 best practice is 4-10 buffers to absorb consumer jitter; 2 was
    // too few and produced packet/frame drops under load.
    for (int i = 0; i < STREAM_BUFFER_COUNT; i++)
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


  // Keep the handler ids: without them the destructor had no way to detach
  // these closures (both capture `this`) before freeing what they touch.
  //
  // Note this arms the stream callback while the constructor is still running.
  // That is survivable now only because every member the callback reads has a
  // default initializer in the header -- previously they were indeterminate,
  // and a takeCount that happened to be 0 stopped acquisition on frame one.
  _sig_new_buffer = g_signal_connect(stream, "new-buffer",
                                     G_CALLBACK(s_STREAM_NEW_BUFFER_CB), this);

  arv_stream_set_emit_signals(stream, TRUE);
  ArvDevice *ctrl_dev = arv_camera_get_device(camera);
  if (ctrl_dev)
    _sig_control_lost = g_signal_connect(ctrl_dev, "control-lost",
                                         G_CALLBACK(s_STREAM_CONTROL_LOST_CB), this);

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


  logCameraState("before-trigger-setup");

  // These used to pass NULL for the GError, so a rejected write was invisible
  // -- and a camera left in a state that cannot produce frames is
  // indistinguishable from a dead trigger wire. Report them.
  {
    GError *e = NULL;
    arv_camera_set_string (camera, "BalanceWhiteAuto", "Off", &e);
    if (e) { LOGI("BalanceWhiteAuto=Off: %s", e->message); g_clear_error(&e); }

    // Line0 (the opto input the sorter drives), NOT "Anyway". "Anyway" accepts
    // a trigger from any source including the camera's internal one, so the
    // sensor free-runs between hardware pulses -- frames the machine never
    // asked for. It triggers correctly, so it looks fine, but the core pairs
    // cam_trig{tid} to frames FIFO and that only holds while the two streams
    // are 1:1. See the TriggerMode() copy of this comment for the measured
    // damage. Fall back to "Anyway" for bodies without Line0, and say which one
    // took -- getting it wrong is invisible until the pairing has drifted.
    const char *tsrc = "Line0";
    arv_camera_set_string (camera, "TriggerSource", tsrc, &e);
    if (e)
    {
      LOGE("TriggerSource=Line0 REJECTED (%s) -- falling back to Anyway", e->message);
      g_clear_error(&e);
      tsrc = "Anyway";
      arv_camera_set_string (camera, "TriggerSource", tsrc, &e);
      if (e) { LOGE("TriggerSource=Anyway REJECTED: %s", e->message); g_clear_error(&e); tsrc = NULL; }
    }
    if (tsrc) LOGI("TriggerSource=%s", tsrc);

    arv_camera_set_string (camera, "TriggerMode", "On", &e);
    if (e) { LOGE("TriggerMode=On REJECTED: %s", e->message); g_clear_error(&e); }

    // ONE frame per trigger -- and it has to be done HERE, not only in
    // TriggerMode(), because this constructor configures triggering inline and
    // never calls TriggerMode() at all. Left at its default of 5, every trigger
    // demands five full frames: at this layer's forced BGR8 full frame that is
    // 5 x 15 MB = 75 MB per trigger, which saturates the link, and the camera
    // then silently refuses triggers outright -- no error, no frames, and
    // indistinguishable from a dead trigger wire. Measured: with burst=5 the
    // layer captured 0 frames from 40 pulses; with burst=1, 35.
    // Must precede start_acquisition: this node is locked while streaming.
    arv_camera_set_integer (camera, "AcquisitionBurstFrameCount", 1, &e);
    if (e) { LOGI("AcquisitionBurstFrameCount=1: %s (absent on some models)", e->message); g_clear_error(&e); }
  }
  GError *err = NULL;

  arv_camera_stop_acquisition (camera, &err);
  if (err)
  {
    LOGE("ERR code:%d msg:%s", err->code, err->message);
    g_clear_error(&err);
  }
  
  
  arv_camera_start_acquisition (camera, &err);
  if (err)
  {
    LOGE("ERR code:%d msg:%s", err->code, err->message);
    g_clear_error(&err);
  }
  else
  {
    acquisition_started=true;
    // This path starts acquisition inline instead of via StartAquisition(), so
    // it has to arm the floor watch itself -- the same omission that left
    // AcquisitionBurstFrameCount at its factory default here for so long.
    refreshExposureFloor();
  }

  logCameraState("after-ctor");

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
  // The old body was two unrefs and nothing else, which left three ways for
  // the stream thread to run against a destroyed object:
  //   - acquisition was never stopped, so frames kept arriving;
  //   - "new-buffer"/"control-lost" were never disconnected, and both closures
  //     carry `this`;
  //   - `camera` was unreffed FIRST, while the callback dereferences it
  //     (arv_camera_stop_acquisition, arv_camera_get_payload).
  // Silence the source, disconnect, and only then release -- camera last,
  // since the stream holds the device.
  if (stream)
  {
    arv_stream_set_emit_signals(stream, FALSE);
    if (_sig_new_buffer && g_signal_handler_is_connected(stream, _sig_new_buffer))
      g_signal_handler_disconnect(stream, _sig_new_buffer);
    _sig_new_buffer = 0;
  }

  if (camera)
  {
    ArvDevice *dev = arv_camera_get_device(camera);
    if (dev && _sig_control_lost && g_signal_handler_is_connected(dev, _sig_control_lost))
      g_signal_handler_disconnect(dev, _sig_control_lost);
    _sig_control_lost = 0;

    if (acquisition_started)
    {
      arv_camera_stop_acquisition(camera, NULL);
      acquisition_started = false;
    }
  }

  // Nothing can be mid-callback past this point, so the cached pointer is
  // meaningless and the chunk state can go.
  _frame_cache_buffer = NULL;

  if (chunk_parser)
  {
    g_object_unref(chunk_parser);
    chunk_parser = NULL;
  }
  if (chunks)
  {
    g_strfreev(chunks);
    chunks = NULL;
  }

  if (stream)
  {
    g_object_unref(stream);
    stream = NULL;
  }
  if (camera)
  {
    g_object_unref(camera);
    camera = NULL;
  }
}
CameraLayer::status CameraLayer_Aravis::SetMirror(int Dir, int en)
{
  bool doChange=false;
  if(Dir==0)
  {
    if(prev_MirrorX!=en)
    {
      doChange=true;
      prev_MirrorX=en;
    }
  }
  if(Dir==1)
  {
    if(prev_MirrorY!=en)
    {
      doChange=true;
      prev_MirrorY=en;
    }
  }

  if(doChange==false)return CameraLayer::ACK;

  // Restore what we found. This used to stop and then unconditionally start,
  // so toggling a mirror on a stopped camera silently began streaming, and
  // acquisition_started was never updated either way -- leaving the flag
  // claiming "stopped" while the camera was actually running.
  const bool was_running = acquisition_started;
  if (was_running) StopAquisition();

  GError *err = NULL;
  if(Dir==0)
  {
    arv_camera_set_boolean(camera,"ReverseX",en,&err);
  }
  if(Dir==1)
  {
    arv_camera_set_boolean(camera,"ReverseY",en,&err);
  }

  CameraLayer::status ret = CameraLayer::ACK;
  if (err)
  {
    // Previously all three Aravis calls here passed NULL for the error, so a
    // camera that does not expose ReverseX/ReverseY failed silently and this
    // still returned ACK.
    LOGE("SetMirror dir %d: %s", Dir, err->message);
    g_clear_error(&err);
    ret = CameraLayer::NAK;
  }

  if (was_running) StartAquisition();

  return ret;
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


  gint	xo_inc = arv_camera_get_x_offset_increment	(camera,NULL);
  gint	yo_inc = arv_camera_get_y_offset_increment	(camera,NULL);
  gint	w_inc = arv_camera_get_width_increment	(camera,NULL);
  gint	h_inc = arv_camera_get_height_increment	(camera,NULL);

  // These four are used as divisors immediately below. Every call passes NULL
  // for the GError, so a camera that does not expose the increment features
  // returns 0 silently -- and 0 here was an integer division by zero, i.e. a
  // SIGFPE that takes the whole process down on a ROI change. 1 means "no
  // granularity constraint", which is the right fallback.
  if (xo_inc <= 0) { LOGE("x offset increment unavailable, assuming 1"); xo_inc = 1; }
  if (yo_inc <= 0) { LOGE("y offset increment unavailable, assuming 1"); yo_inc = 1; }
  if (w_inc  <= 0) { LOGE("width increment unavailable, assuming 1");    w_inc  = 1; }
  if (h_inc  <= 0) { LOGE("height increment unavailable, assuming 1");   h_inc  = 1; }

  x=x/xo_inc*xo_inc;
  y=y/yo_inc*yo_inc;
  w=w/w_inc*w_inc;
  h=h/h_inc*h_inc;


  bool doChange=false;
  {
    if(prev_ROI_X!=x||prev_ROI_Y!=y||prev_ROI_W!=w||prev_ROI_H!=h)
    {
      doChange=true;
      prev_ROI_X=x;
      prev_ROI_Y=y;
      prev_ROI_W=w;
      prev_ROI_H=h;
    }
  }

  if(doChange==false)return CameraLayer::ACK;
  LOGI("xywh:%d,%d %d,%d >+>%d,%d %d,%d ", x, y, w, h,xo_inc,yo_inc,w_inc,h_inc);



  // Was a hardcoded 1000 ms sleep on the caller's thread, used as a "wait for
  // the camera to settle" after stopping. StopAquisition() already does the
  // same stop plus a 50 ms settle -- the value that was actually tuned for
  // reliable parameter writes (see the comment on StopAquisition) -- so go
  // through it instead of blocking a full second inside a setter.
  const bool was_running = acquisition_started;
  if (was_running)
  {
    StopAquisition();
  }




  arv_camera_set_region(camera, (int)x, (int)y, (int)w, (int)h, &err);

  CameraLayer::status retState=CameraLayer::ACK;
  if (err)
  {
    LOGI("ERR code:%d msg:%s", err->code, err->message);
    g_clear_error(&err);
    
    retState=CameraLayer::NAK;
  }
  else
  {

    GetROI(&x, &y, &w, &h, NULL, NULL);
    LOGI("xywh:%d,%d %d,%d", x, y, w, h);
  }


  
  // was_running, NOT acquisition_started: StopAquisition() above cleared the
  // flag, so testing it here would never restart. Going through
  // StartAquisition() also re-sizes the buffer pool, which matters precisely
  // here -- changing the region is what changes the payload, and stale-sized
  // buffers are what produce SIZE_MISMATCH on the first frames back.
  if (was_running)
  {
    StartAquisition();
  }


  // if(acquisition_started)
  // {
  //   arv_camera_start_acquisition (camera, &err);
  //   if (err)
  //   {
  //     LOGI("ERR code:%d msg:%s", err->code, err->message);
  //     g_clear_error(&err);
  //   }
  // }

  return retState;
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




    // {
    //   arv_camera_stop_acquisition(camera, &err);
    //   if(err)
    //   {
    //     LOGE("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    //     err=NULL;
    //     g_clear_error(&err);
    //     return CameraLayer::NAK;
    //   }
    // }


  takeCount = -1;

  // ONE frame per trigger. Measured on the bench: these bodies expose only
  // TriggerSelector=FrameBurstStart (there is no FrameStart trigger), so
  // AcquisitionBurstFrameCount *is* the frames-per-trigger control -- and it
  // defaults to 5. Left alone, every trigger produces five frames: five times
  // the payload, on a link whose bandwidth is exactly what decides whether the
  // camera can accept the next trigger at all. The pipeline pairs one frame to
  // one part, so anything but 1 also desynchronises that pairing.
  // Acquisition-control nodes are locked while the camera is streaming --
  // writing this one mid-stream fails with a USB3Vision write_memory error and
  // leaves the control endpoint unhappy. Stop, write, restore.
  {
    const bool burst_was_running = acquisition_started;
    if (burst_was_running) StopAquisition();

    GError *burst_err = NULL;
    arv_camera_set_integer(camera, "AcquisitionBurstFrameCount", 1, &burst_err);
    if (burst_err)
    {
      LOGI("AcquisitionBurstFrameCount=1: %s (harmless if the node is absent)",
           burst_err->message);
      g_clear_error(&burst_err);
    }

    if (burst_was_running) StartAquisition();
  }

  //0 for continuous, 1 for soft trigger, 2 for HW trigger
  if (type == 0)
  {
    LOGI(">>>>TriggerMode off");
    {
      arv_camera_set_string (camera, "TriggerMode", "Off", &err);
      if(err)
      {
        LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        return CameraLayer::NAK;
      }
      else
      {
        
        LOGE("TriggerMode OFF");
      }
    }



  }
  else
  {

    {
      arv_camera_set_string (camera, "TriggerMode", "On", &err);
      if(err)
      {
        LOGE("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        return CameraLayer::NAK;
      }
      else
      {
        
        LOGE("TriggerMode ON");
      }
    }

    // Prefer the physical trigger input over "Anyway".
    //
    // "Anyway" means the camera accepts a trigger from ANY source -- including
    // its own internal one -- so the sensor keeps free-running between hardware
    // pulses. It does trigger correctly (proven 20/20 on the bench), which is
    // why it looked fine, but the extra frames are the problem downstream: the
    // core pairs cam_trig{tid} to frames FIFO, and that is only correct while
    // the two streams are 1:1. Measured with "Anyway": ~213 announced parts vs
    // 449 frames and 273 "frame with no pending trigger", with report latency
    // climbing without bound as the pairing drifted.
    //
    // Line0 is the opto input the sorter drives. Fall back to "Anyway" if a
    // body does not expose it, so other cameras keep working -- and log which
    // one actually took, because getting this wrong is invisible until the
    // pairing has already drifted.
    //
    // Both triggered modes point here, including type 1. That looks wrong --
    // type 1 is nominally "soft trigger" -- but trigger_mode:1 is what the
    // WebUI sends to mean "stop free-running", and on this machine the camera
    // must still answer the plate while it is in that state. Selecting
    // Software here instead made the camera deaf to Line0 the moment the UI
    // paused the stream, which is the whole sensor gone (observed 2026-08-05:
    // FI armed Line0, an ST {"CameraSetting":{"trigger_mode":1}} arrived a
    // moment later, and from then on the camera only answered software
    // triggers).
    //
    // A software trigger gets the Software source for the instant it needs it
    // -- see Trigger() -- which is where that requirement actually belongs.
    const char *trig_src = "Line0";
    arv_camera_set_string (camera, "TriggerSource", trig_src, &err);
    if(err)
    {
      LOGE("TriggerSource %s rejected (d%d c:%d m:%s) -- falling back to Anyway",
           trig_src, err->domain,err->code,err->message);
      err=NULL;
      g_clear_error(&err);
      trig_src = "Anyway";
      arv_camera_set_string (camera, "TriggerSource", trig_src, &err);
      if(err)
      {
        LOGE("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
        err=NULL;
        g_clear_error(&err);
        trig_src = NULL;
      }
    }
    if(trig_src)
      LOGE("TriggerSource %s", trig_src);
    arv_camera_set_string (camera, "TriggerActivation", "RisingEdge", &err);

    if(err)
    {
      LOGE("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      err=NULL;
      g_clear_error(&err);
    }
    else
    {
      
      LOGE("TriggerActivation RisingEdge");
    }
    
    if(err)
    {
      // LOGD("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
      // err=NULL;
      // g_clear_error(&err);
      // return CameraLayer::NAK;
    }
  }

  // {
  //   arv_camera_start_acquisition(camera, &err);
  //   if(err)
  //   {
  //     LOGE("e:d%d c:%d m:%s\n",err->domain,err->code,err->message);
  //     err=NULL;
  //     g_clear_error(&err);
  //     return CameraLayer::NAK;
  //   }
  //   acquisition_started=true;
  // }

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

  // arv_camera_software_trigger() is silently ignored by a camera whose
  // TriggerSource is the physical line -- the call succeeds, ACK is returned,
  // and no frame ever arrives. That is what broke the WebUI's take-new-image,
  // and with a negative snap timeout it hung the core instead of failing.
  //
  // The triggered modes hold Line0 because the machine has to keep answering
  // the plate (see TriggerMode). So borrow the Software source for the instant
  // of the trigger and hand it straight back -- neither requirement has to
  // lose. Only in a triggered mode: in free-run there is nothing to restore.
  const bool borrow_src = (triggerMode == 1 || triggerMode == 2);
  if (borrow_src)
  {
    GError *se = NULL;
    arv_camera_set_string (camera, "TriggerSource", "Software", &se);
    if (se) { LOGE("TriggerSource Software rejected: %s", se->message); g_clear_error(&se); }
  }

  arv_camera_software_trigger (camera,&err);

  if (borrow_src)
  {
    GError *se = NULL;
    arv_camera_set_string (camera, "TriggerSource", "Line0", &se);
    if (se)
    {
      g_clear_error(&se);
      arv_camera_set_string (camera, "TriggerSource", "Anyway", &se);
      if (se) { LOGE("could not restore TriggerSource: %s", se->message); g_clear_error(&se); }
    }
  }

  // NOT acquisition_started=true here. The start_acquisition call it used to
  // sit next to is commented out, so the flag was simply a lie -- and it is
  // load-bearing: SetROI (stop/restart around the region write) and
  // StopAquisition both branch on it, so a lying flag made them act on a
  // camera that was never streaming.
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


CameraLayer::status CameraLayer_Aravis::SetGamma(float gamma)
{
  GError *err = NULL;
  
  
  arv_camera_set_boolean (camera, "GammaEnable", true, NULL);
  arv_camera_set_float(camera,"Gamma",gamma,&err);

  if (err != NULL)
  {
    
    LOGI("gamma failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;

  
}


CameraLayer::status CameraLayer_Aravis::SetBalckLevel(float blvl)
{
  GError *err = NULL;
  
  
  arv_camera_set_boolean (camera, "BlackLevelEnable", true, NULL);
  arv_camera_set_integer(camera,"BlackLevel",(int)blvl,&err);

  if (err != NULL)
  {
    
    LOGI("BlackLevel failed:d%d c:%d m:%s\n",err->domain,err->code,err->message);
    g_clear_error(&err);

    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;

  
}

CameraLayer::status CameraLayer_Aravis::SetRGain(float gain)
{
  GError *err = NULL;
  
  // arv_camera_set_integer(camera,"RGain",(int)gain,&err);
  
  arv_camera_set_string (camera, "BalanceRatioSelector", "Red",NULL);
  arv_camera_set_integer(camera,"BalanceRatio",(int)gain,&err);


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
  // arv_camera_set_integer(camera,"GGain",(int)gain,&err);

  arv_camera_set_string (camera, "BalanceRatioSelector", "Green",NULL);
  arv_camera_set_integer(camera,"BalanceRatio",(int)gain,&err);

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
  // arv_camera_set_integer(camera,"BGain",(int)gain,&err);

  arv_camera_set_string (camera, "BalanceRatioSelector", "Blue",NULL);
  arv_camera_set_integer(camera,"BalanceRatio",(int)gain,&err);

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
  else if(isinf(frame_rate) || frame_rate>max)
  {
    // "Faster than the sensor can go" means "don't limit me" -- the WebUI's
    // setCameraSpeed_HIGHEST() sends the finite sentinel 9999999 for exactly
    // that. Returning NAK here was actively harmful: the request was rejected
    // and whatever limit was already in force stayed. Observed on the real
    // machine -- something pushed framerate:8 at inspection start, the
    // immediately-following framerate:9999999 was refused, and the camera ran
    // the whole production test pinned at 8 fps (125ms exposure floor) while
    // the machine fed it parts far faster. The camera then silently refuses
    // the surplus triggers, which surfaces only as pairing drift several
    // layers up. Disable the limiter instead, and say so.
    LOGI("SetFrameRate %.0f exceeds max %.2f -> disabling the limiter", frame_rate, max);
    arv_camera_set_boolean (camera, "AcquisitionFrameRateEnable", false, &err);
    if (err != NULL)
    {
      LOGE("AcquisitionFrameRateEnable(false): %s", err->message);
      g_clear_error(&err);
      return CameraLayer::NAK;
    }
    return CameraLayer::ACK;
  }
  else if(frame_rate<min)return CameraLayer::NAK;




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

// Real Aravis StopAquisition / StartAquisition. The base class default returns
// NAK, so CameraSetup's StopAquisition() call was a silent no-op before --
// meaning the first ~1-2 parameter writes after construction would race the
// streaming pipeline and get dropped by the camera firmware (a well-known
// GenICam GigE quirk). Now CameraSetup:
//   stop  -> sleep 50ms (settle)  ->  apply params  ->  start  -> sleep 50ms
// produces reliable param writes on Aravis cameras.
CameraLayer::status CameraLayer_Aravis::StopAquisition()
{
  if (!camera) return CameraLayer::NAK;
  if (!acquisition_started) return CameraLayer::ACK;
  GError *err = NULL;
  arv_camera_stop_acquisition(camera, &err);
  acquisition_started = false;
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  if (err) {
    LOGE("StopAquisition: %s", err->message);
    g_clear_error(&err);
    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;
}
// The exposure floor moves with ROI and pixel format, so it has to be re-read
// every time acquisition starts rather than sampled once at construction.
// ResultingFrameRate is what the camera itself says it can sustain with the
// current settings; measured against hardware triggers it is exact, not
// advisory -- triggers spaced 1ms above it are honoured and 1ms below it are
// clamped. Exposure time only enters once it exceeds the readout time
// (full frame: no effect until ~40ms), so this covers that case too.
void CameraLayer_Aravis::refreshExposureFloor()
{
  _prev_frame_us = 0;
  _saturated_run = 0;
  _saturation_reported = false;
  _floor_us = 0.0;
  if (!camera) return;
  GError *err = NULL;
  double rfr = arv_camera_get_float(camera, "ResultingFrameRate", &err);
  if (err)
  {
    // Not every body exposes it. Skip the check rather than invent a floor.
    LOGI("ResultingFrameRate unavailable (%s); exposure-floor watch disabled",
         err->message);
    g_clear_error(&err);
    return;
  }
  if (rfr > 0.0)
  {
    _floor_us = 1000000.0 / rfr;
    LOGI("exposure floor: %.0fus (ResultingFrameRate %.2f fps)", _floor_us, rfr);
  }
}

CameraLayer::status CameraLayer_Aravis::StartAquisition()
{
  if (!camera) return CameraLayer::NAK;
  if (acquisition_started) return CameraLayer::ACK;
  // ROI / pixel format may have changed while stopped, which changes payload.
  //
  // Recycling buffers in place does NOT work: arv_stream_try_pop_buffer only
  // returns COMPLETED buffers, so any still submitted to the USB transfer
  // layer at the old size stay in the stream. Pushing new-size buffers on top
  // leaves the stream holding two sizes at once, and the device then refuses
  // the next AcquisitionStart -- "USB3Vision write_memory error" -- which is
  // unrecoverable for the life of the process, because acquisition_started
  // stays false and every later start fails the same way. Observed today:
  // changing the ROI (payload 5013504 -> 544960) killed imaging until the
  // camera was physically unplugged.
  //
  // Buffer size is a property of the stream, so change it the way Aravis
  // intends: tear the stream down and build a new one. Mirrors the destructor's
  // ordering -- silence the source, disconnect, drop the cached pointer, and
  // only then release.
  if (stream) {
    int cur_payload = arv_camera_get_payload(camera, NULL);
    if (cur_payload > 0 && cur_payload != payloadSize) {
      LOGI("StartAquisition: payload %d -> %d, rebuilding stream",
           payloadSize, cur_payload);
      payloadSize = cur_payload;

      arv_stream_set_emit_signals(stream, FALSE);
      if (_sig_new_buffer && g_signal_handler_is_connected(stream, _sig_new_buffer))
        g_signal_handler_disconnect(stream, _sig_new_buffer);
      _sig_new_buffer = 0;
      // Points into a buffer owned by the stream about to be released.
      _frame_cache_buffer = NULL;
      g_object_unref(stream);
      stream = NULL;

      stream = arv_camera_create_stream(camera, stream_cb, NULL, NULL);
      if (stream == NULL) {
        LOGE("StartAquisition: stream rebuild failed -- no frames until reconnect");
        return CameraLayer::NAK;
      }
      for (int i = 0; i < STREAM_BUFFER_COUNT; i++)
        arv_stream_push_buffer(stream, arv_buffer_new(payloadSize, NULL));
      _sig_new_buffer = g_signal_connect(stream, "new-buffer",
                                         G_CALLBACK(s_STREAM_NEW_BUFFER_CB), this);
      arv_stream_set_emit_signals(stream, TRUE);
    }
  }
  GError *err = NULL;
  arv_camera_start_acquisition(camera, &err);
  if (err) {
    // A camera left streaming by a process that died rejects AcquisitionStart
    // ("USB3Vision write_memory error (invalid-parameter)") -- it is already
    // acquiring. The new process cannot know that: acquisition_started is
    // false on a fresh start, so StopAquisition() is skipped and every start
    // fails forever. The core then runs with no frames at all, and the only
    // cure was unplugging the camera.
    //
    // That is not a rare state here: the core has crashed and been killed
    // repeatedly during bring-up. So on failure, stop unconditionally and try
    // once more. Costs nothing on the healthy path -- this runs only after a
    // start has already failed.
    LOGE("StartAquisition: %s -- stopping and retrying once", err->message);
    g_clear_error(&err);
    GError *stop_err = NULL;
    arv_camera_stop_acquisition(camera, &stop_err);
    if (stop_err) g_clear_error(&stop_err);   // expected if it really was idle
    arv_camera_start_acquisition(camera, &err);
    if (err) {
      LOGE("StartAquisition retry: %s", err->message);
      g_clear_error(&err);
      return CameraLayer::NAK;
    }
    LOGI("StartAquisition: recovered a camera left streaming by a dead process");
  }
  acquisition_started = true;
  refreshExposureFloor();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  return CameraLayer::ACK;
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