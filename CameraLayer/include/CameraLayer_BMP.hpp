#ifndef CAMERALAYER_BMP_HPP
#define CAMERALAYER_BMP_HPP
#include <CameraLayer.hpp>
#include <string>
#include <acvImage_BasicTool.hpp>
#include <mutex>
#include <vector>
class CameraLayer_BMP : public CameraLayer{

    std::mutex m;
    std::string fileName;
    public:
    CameraLayer_BMP(CameraLayer_Callback cb,void* context);
    
    status LoadBMP(std::string fileName);
    std::string GetCurrentFileName(){return this->fileName;}
};



class CameraLayer_BMP_carousel : public CameraLayer_BMP{

    std::string folderName;
    std::string fileName;
    std::vector<std::string> files_in_folder;
    public:
    int fileIdx;
    CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName);
    status updateFolder(std::string folderName);
    status Trigger();
    std::string GetCurrentFileName(){return this->fileName;}
};


#endif