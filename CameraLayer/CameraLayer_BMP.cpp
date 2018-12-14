
#include "CameraLayer_BMP.hpp"



CameraLayer_BMP::CameraLayer_BMP(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{

}

CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();
    this->fileName = fileName;
    int ret = acvLoadBitmapFile(&img, fileName.c_str());
    if(ret!=0)
    {
        ret_status=NAK;
        callback(*this,CameraLayer::EV_ERR,context);
    }
    else
    {
        ret_status=ACK;
        callback(*this,CameraLayer::EV_IMG,context);
    }
    m.unlock();
    return ret_status;
}