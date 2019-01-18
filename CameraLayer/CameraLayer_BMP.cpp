
#include "CameraLayer_BMP.hpp"

#include <logctrl.h> 
#include <dirent.h> 
#include <thread>


CameraLayer_BMP::CameraLayer_BMP(CameraLayer_Callback cb,void* context):CameraLayer(cb,context)
{

}

CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();
    LOGV("Loading:%s",fileName.c_str());
    this->fileName = fileName;
    int ret = acvLoadBitmapFile(&img, fileName.c_str());
    if(ret!=0)
    {
        ret_status=NAK;
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
        /*for(int i=0;i<img.GetHeight();i++)//Add noise
        {
            for(int j=0;j<img.GetWidth();j++)
            {
                int d = img.CVector[i][j*3];
                float u = rand() / (double)RAND_MAX;
                float v = rand() / (double)RAND_MAX;
                float x = sqrt(-2 * log(u)) * cos(2 * M_PI * v) * 20 + 0;


                d+=(int)x;
                if(d<0)d=0;
                else if(d>255)d=255;
                
                img.CVector[i][j*3] = d;
                img.CVector[i][j*3+1] = d;
                img.CVector[i][j*3+2] = d;

            }
        }*/


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
    cameraThread=NULL;
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
            if(dir->d_name[0]=='.')continue;
            std::string str(dir->d_name);
            str = folderName+"/"+str;
            LOGV("FILE::%s",str.c_str());
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


CameraLayer::status CameraLayer_BMP_carousel::LoadNext()
{
    updateFolder(this->folderName);
    if(files_in_folder.size()==0)return NAK;
    fileIdx++;
    if(fileIdx>=files_in_folder.size())
    {
        fileIdx=0;
    }
    return LoadBMP(files_in_folder[fileIdx]);
}

CameraLayer::status CameraLayer_BMP_carousel::Trigger()
{
    imageTakingCount+=1;
    
    LOGV("imageTakingCount:%d",imageTakingCount);
    return TriggerMode(0);
}

void CameraLayer_BMP_carousel::ContTriggerThread( )
{
    while( ThreadTerminationFlag == 0)
    {
        if(imageTakingCount>0)
        {
            LOGV("imageTakingCount:%d",imageTakingCount);
            LoadNext();
            imageTakingCount--;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    //ThreadTerminationFlag = 0;

}


CameraLayer::status CameraLayer_BMP_carousel::TriggerMode(int mode)
{
    if(mode==0)
    {
        ThreadTerminationFlag = 0;
        if(cameraThread == NULL)
        {
            cameraThread = new std::thread(&CameraLayer_BMP_carousel::ContTriggerThread, this);
        }
    }
    else
    {
        ThreadTerminationFlag = 1;
        if(cameraThread)
        {
            cameraThread->join();
            delete cameraThread;
            cameraThread = NULL;
        }
    }
    return ACK;
}