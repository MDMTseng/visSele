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
    }status;


    typedef enum {
        EV_IMG,
        EV_ERROR
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
    std::string cam_json_info;

    
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
    public:
    
    static std::string getDriverName(){
      return "_CameraLayer_";
    }

    FrameExtractPixelFormat GetFrameFormat()
    {
      return FrameExtractPixelFormat::RGB;
    }

    
    static int listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &dev){return 0;}

    
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


    virtual frameInfo GetFrameInfo()
    {
        return fi;
    }

    virtual  ~CameraLayer(){}
};


#endif