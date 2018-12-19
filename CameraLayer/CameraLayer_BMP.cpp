
#include "CameraLayer_BMP.hpp"

#include <dirent.h> 


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
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
        ret_status=ACK;
        callback(*this,CameraLayer::EV_IMG,context);
    }
    m.unlock();
    return ret_status;
}

CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName):
    CameraLayer_BMP(cb,context)
{
    updateFolder(folderName);
    fileIdx=0;
}


CameraLayer::status CameraLayer_BMP_carousel::updateFolder(std::string folderName)
{
    this->folderName  = folderName;
    files_in_folder.resize(0);
    DIR *d;
    struct dirent *dir;
    d = opendir(folderName.c_str());
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            std::string str(dir->d_name);
            files_in_folder.push_back(str);
        }
        closedir(d);
        
        return ACK;
    }
    else
    {
        return NAK;
    }
}



CameraLayer::status CameraLayer_BMP_carousel::Trigger()
{
    if(files_in_folder.size()==0)return NAK;

    if(fileIdx>=files_in_folder.size())
    {
        fileIdx=0;
    }
    fileIdx++;
    return LoadBMP(files_in_folder[fileIdx]);
}