#ifndef CAMERALAYER_HPP
#define CAMERALAYER_HPP
#include <stdint.h>
#include <stddef.h>

#include <acvImage.hpp>


class CameraLayer{
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
    acvImage img;
    CameraLayer_Callback callback;
    void* context;
    
    public:
    

    typedef enum {
        ACK,
        NAK,
        CB,
        NAK_OVER_CAPACITY,
    }status;


    typedef enum {
        EV_IMG,
        EV_ERR,
    }ev_type;


    CameraLayer(CameraLayer_Callback cb,void* context)
    {
        this->callback = cb;
        this->context = context;
    }

    status TriggerMode(int type)
    {
        return NAK;
    }


    status TriggerCount(int TYPE)
    {
        return NAK;
    }

    status RUN()
    {
        return NAK;
    }


    status SetCrop(int x,int y, int width,int height)
    {
        return NAK;
    }
    status SetResolution(int width,int height)
    {
        return NAK;
    }
    status SetAnalogGain(int min,int max)
    {
        return NAK;
    }

    status GetAnalogGain(int *ret_min,int *ret_max)
    {
        return NAK;
    }

    status SetExposureTime(double time_ms)
    {
        return NAK;
    }

    status GetExposureTime(double *ret_time_ms)
    {
        return NAK;
    }
    public:

    acvImage* GetImg()
    {
        return &img;
    }

};


#endif