#ifndef CAMERALAYER_HPP
#define CAMERALAYER_HPP
#include <stdint.h>
#include <stddef.h>

#include <acvImage.hpp>
#include <string>

class CameraLayer{

    public:
    
    typedef struct {
      uint64_t timeStamp_100us;
      uint32_t width;
      uint32_t height;
      float offset_x;
      float offset_y;
    }frameInfo;
    protected:
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
                

    //void *cameraInst;
    typedef void (*CameraLayer_Callback)(
        CameraLayer &cl_obj, int type, void* context);
    acvImage img,img_load;
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
    public:
    

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




    CameraLayer(CameraLayer_Callback cb,void* context)
    {
        this->callback = cb;
        this->context = context;
    }

    virtual CameraLayer::status TriggerMode(int type)
    {
        return CameraLayer::NAK;
    }


    virtual CameraLayer::status TriggerCount(int TYPE)
    {
        return CameraLayer::NAK;
    }

    
    virtual CameraLayer::status Trigger()
    {
        return CameraLayer::NAK;
    }


    virtual CameraLayer::status RUN()
    {
        return CameraLayer::NAK;
    }

    virtual std::string getCameraJsonInfo()
    {
        return cam_json_info;
    }

    virtual CameraLayer::status SetROI(float x, float y, float w, float h,int zw,int zh)
    {
        return CameraLayer::NAK;
    }
    
    virtual CameraLayer::status GetROI(float *x, float *y, float *w, float *h,int*zw,int *zh)
    {
        
        return CameraLayer::NAK;
    }
    virtual CameraLayer::status SetResolution(int width,int height)
    {
        return CameraLayer::NAK;
    }
    virtual CameraLayer::status SetAnalogGain(int gain)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetMirror(int Dir,int en)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status GetAnalogGain(int *ret_min,int *ret_max)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetExposureTime(double time_us)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status GetExposureTime(double *ret_time_us)
    {
        return CameraLayer::NAK;
    }

    virtual CameraLayer::status SetFrameRateMode(int mode)
    {
        return CameraLayer::NAK;
    }
    
    virtual acvImage* GetFrame()
    {
        return &img;
    }

    
    virtual CameraLayer::status SnapFrame()
    {
        return CameraLayer::NAK;
    }


    virtual frameInfo GetFrameInfo()
    {
      return fi;
    }

    virtual  ~CameraLayer(){}
};


#endif