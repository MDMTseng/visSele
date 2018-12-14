#ifndef CAMERALAYER_BMP_HPP
#define CAMERALAYER_BMP_HPP
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <queue>
class CameraLayer_BMP : public CameraLayer{

    std::mutex m;
    std::string fileName;
    public:
    CameraLayer_BMP(CameraLayer_Callback cb,void* context);
    
    status LoadBMP(std::string fileName);
    std::string GetCurrentFileName(){return this->fileName;}
};


#endif