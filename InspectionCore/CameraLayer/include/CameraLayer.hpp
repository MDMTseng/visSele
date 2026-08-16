#ifndef CAMERALAYER_HPP
#define CAMERALAYER_HPP
#include <stdint.h>
#include <stddef.h>

#include <acvImage.hpp>
#include <string>
#include <condition_variable>

#include <thread>
#include <mutex>
#include <vector>




class CameraLayer{
  
    public:


    typedef enum {
      RGB,
      RGB8,
      BGR8,
      Mono,
      Bayer_GR,
    }FrameExtractPixelFormat;




    struct BasicCameraInfo{
      std::string driver_name;
      std::string name;
      std::string side_name;
      std::string id;
      std::string model;
      std::string serial_number;
      std::string vender;
      void* ctx;
    };

    typedef struct {
      uint64_t timeStamp_us;
      uint32_t width;
      uint32_t height;
      float offset_x;
      float offset_y;
      float pixel_size_mm;
      int channelCount;
      int pixelBits;
      // Sensor-side frame counter. It increments for every frame the sensor
      // exposes, whether or not that frame reaches us, so a gap is an exact
      // count of what was lost in transport -- not an estimate. Consumers that
      // pair frames with external events (trigger ids, part ids) need this to
      // stay aligned across a drop. 0 / frameNumValid==false when the driver
      // cannot supply it.
      uint32_t frameNum = 0;
      bool frameNumValid = false;

      // Triggers the CAMERA counted, which is not the same population as
      // frameNum.
      //
      // frameNum counts frames the sensor exposed. A trigger the camera REFUSED
      // -- arriving inside the exposure floor -- produces no frame, so frameNum
      // stays contiguous across it and the loss is invisible. ExtTriggerCount
      // counts the triggers themselves, so `extTrigCount - frameNum` is the
      // number that were thrown away.
      //
      // On the MV-CA050-11UM there is no GenICam chunk for this: the value is
      // written INTO THE IMAGE, little-endian at the start of row 0, and the
      // byte offsets are packed by the order the fields were ENABLED rather
      // than being fixed. Enable exactly one field and the offset is 0; enable
      // more and a decoder that hardcodes an offset silently reads the wrong
      // counter. Measured 2026-08-10, see UINSP_CAVEATS.
      uint32_t extTrigCount = 0;
      bool extTrigCountValid = false;
      // Time since the previous frame's exposure, 0 when unknown (first frame,
      // or a driver with no usable timestamp).
      uint64_t interval_us = 0;
      // The camera hit its exposure floor on this frame.
      //
      // A sensor cannot start an exposure sooner than 1/ResultingFrameRate
      // after the previous one. Trigger it faster and the camera does NOT say
      // so: it delays the exposure to that floor if it has buffer room, and
      // silently refuses the trigger if it does not -- with no error and with
      // frameNum staying contiguous, because the refused frames were never
      // exposed. Measured on a MV-CA050-12UC at full frame (floor 40108us):
      // triggers 41ms apart are timestamped exactly, 40ms and closer all come
      // back at the floor, and closer than 20ms also lose a trigger outright.
      //
      // So when this is set, timeStamp_us is the truth about the EXPOSURE but
      // no longer about the trigger that asked for it -- it can sit tens of ms
      // late, and anything pairing frames to parts by time or by index is
      // being fed a lie. It is the one warning the camera itself will not give.
      bool rateSaturated = false;
    }frameInfo;
    /*// Camera's device information
    typedef struct
    {
        char acProductSeries [32]; // product family
        char acProductName [32]; // product name
        char acFriendlyName [32]; // product nickname, the user can customize to change the nickname, stored in the camera, used to distinguish between multiple cameras at the same time, you can use CameraSetFriendlyName interface to change the nickname, the device takes effect after the restart.
        char acLinkName [32]; // kernel symbolic connection name, internal use
        char acDriverVersion [32]; // driver version
        char acSensorType [32]; // sensor type
        char acPortType [32]; // Interface type
        char acSn [32]; // Product unique serial number
        uint32_t uInstance; // Instance index of the model camera on the computer, used to distinguish the same type of multi-camera
    } cameraInfo;

                                                           
    static status enumerateDevice(cameraInfo *list,int32_t *inout_listL){
        return ACK;
    };*/
                
    typedef enum {
        ACK,
        NAK,
        CB,
        NAK_OVER_CAPACITY,
        NOT_SUPPORTED,
    }status;


    typedef enum {
        EV_IMG,
        EV_INFO,
        EV_ERROR,
        EV_CTRL_LOST,
    }ev_type;



    

    //void *cameraInst;
    typedef CameraLayer::status (*CameraLayer_Callback)(
        CameraLayer &cl_obj, int type, void* context);

    int triggerMode;
    protected:


    CameraLayer::BasicCameraInfo connection_data;
    std::string connection_misc;


    CameraLayer_Callback callback;
    void* context;
    uint32_t frameTimeTag;
    int maxWidth,maxHeight;
    frameInfo fi;

    std::vector<uint64_t> error_code_list;
    std::string cam_json_info;

    // Opt-in, and deliberately OFF by default. The reliable trigger accounting
    // is the camera's own edge count (HikRobot's Line0RisingEdge events) plus
    // frameNum continuity; this watch is a second opinion for diagnosing a
    // suspected over-trigger, not a replacement. Leaving it dark by default
    // also means a machine that legitimately runs at the frame-rate limit does
    // not spend its life logging that it is doing so.
    bool _exposure_floor_watch = false;

    
    float ROI_x;
    float ROI_y;
    float ROI_w;
    float ROI_h;
    float pixel_size_mm=NAN;
    int frameCount;
    std::mutex m;
    CameraLayer_Callback _snap_cb;
    int snapFlag=0;
    std::condition_variable conV;
    static CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj);

    // Serializes {callback, context} against the driver frame threads.
    // SnapFrame swaps the pair in two plain writes; a hardware-triggered frame
    // landing between them used to run SNAP_Callback with the OLD context (a
    // NULL dst -> SIGSEGV in copyTo), and a snap timing out mid-invocation
    // restored the pair and returned while the frame thread was still writing
    // into the caller's stack Mat. Every driver invocation goes through
    // invokeFrameCallback below; SnapFrame swaps/restores under the same lock,
    // so a restore now waits out an in-flight invocation. This mutex guards
    // ONLY the pair and the invocation -- nothing else may take it, and the
    // invoked callbacks must never call SnapFrame (they don't: SnapFrame is
    // command-side, callbacks are frame-side).
    std::mutex cb_swap_m;
    CameraLayer::status invokeFrameCallback(int type)
    {
      std::lock_guard<std::mutex> _g(cb_swap_m);
      if (callback == NULL) return NAK;
      return callback(*this, type, context);
    }
    public:
    
    static std::string getDriverName(){
      return "_CameraLayer_";
    }

    // MUST stay virtual. Without it the derived layers' GetFrameFormat() only
    // hid this one, so every consumer -- which holds cameras as CameraLayer&
    // or CameraLayer* -- silently got this RGB default no matter what the
    // sensor was actually delivering.
    virtual FrameExtractPixelFormat GetFrameFormat()
    {
      return FrameExtractPixelFormat::RGB;
    }

    
    static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &dev){return 0;}

    // Turn the exposure-floor watch on for this camera. frameInfo::interval_us
    // is filled either way (it costs a subtraction); this only controls whether
    // frameInfo::rateSaturated gets set and whether a sustained episode is
    // logged. See frameInfo::rateSaturated for what it means.
    void SetExposureFloorWatch(bool enable){ _exposure_floor_watch = enable; }
    bool GetExposureFloorWatch() const { return _exposure_floor_watch; }

    
    CameraLayer(CameraLayer::BasicCameraInfo connection_data,std::string connection_misc,CameraLayer_Callback cb,void* context)
    {
        this->connection_data=connection_data;
        this->connection_misc=connection_misc;
        this->callback = cb;
        this->context = context;
        frameCount=0;
    }

    CameraLayer::BasicCameraInfo getConnectionData()
    {
      return connection_data;
    }

    virtual CameraLayer::status TriggerMode(int type){ triggerMode=type;return NAK;}

    virtual CameraLayer::status TriggerCount(int TYPE)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetOnceWB()
    {
        return CameraLayer::NAK;
    }
    
    virtual CameraLayer::status Trigger(){return NAK;}


    virtual CameraLayer::status StartAquisition()
    {
        return CameraLayer::NAK;
    }
    virtual CameraLayer::status StopAquisition()
    {
        return CameraLayer::NAK;
    }

    virtual std::string getCameraJsonInfo()
    {
        return cam_json_info;
    }

    virtual CameraLayer::status SetROI(int x, int y, int w, int h,int zw,int zh){return NAK;}
    
    virtual CameraLayer::status GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh){return NAK;}
    virtual CameraLayer::status SetResolution(int width,int height)
    {
        return CameraLayer::NAK;
    }
    virtual CameraLayer::status SetAnalogGain(float gain){return NAK;}
    virtual CameraLayer::status SetFormatStr(std::string fstr){return NOT_SUPPORTED;}
    virtual CameraLayer::status SetRGain(float gain){return NAK;}
    virtual CameraLayer::status SetGGain(float gain){return NAK;}
    virtual CameraLayer::status SetBGain(float gain){return NAK;}

    virtual CameraLayer::status SetBalckLevel(float blvl){return NAK;}
    virtual CameraLayer::status SetPixelSize(float pixel_size_mm){
        this->pixel_size_mm=pixel_size_mm;
        return ACK;
    }

    virtual CameraLayer::status SetGamma(float Gamma){return NAK;}

    virtual CameraLayer::status SetMirror(int Dir,int en){return NAK;}
    virtual CameraLayer::status SetROIMirror(int Dir,int en)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status GetAnalogGain(int *ret_min,int *ret_max)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetExposureTime(float time_us){return NAK;}

    virtual CameraLayer::status GetExposureTime(float *ret_time_us)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetFrameRate(float mode)
    {
        return CameraLayer::NAK;
    }
    
    virtual CameraLayer::status GetCurrentFrameDimension(int *ret_W,int *ret_H,int *ret_CH)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount){return NAK;}

    virtual CameraLayer::status isInOperation()
    {
        return CameraLayer::NAK;
    }
    
    virtual CameraLayer::status SnapAbort(int timeout_ms=-1);

    virtual CameraLayer::status SnapFrame(CameraLayer_Callback snap_cb,void *cb_param,int type=0, int timeout_ms=-1);

    virtual CameraLayer::status SetSideName(std::string side_name)
    {
        // connection_data.side_name=side_name;
        connection_data.side_name=side_name;
        return ACK;
    }
    virtual std::string GetSideName()
    {
        return connection_data.side_name;
    }

    virtual frameInfo GetFrameInfo()
    {
        return fi;
    }

    virtual  ~CameraLayer(){}
};


#endif