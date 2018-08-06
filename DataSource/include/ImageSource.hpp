#ifndef IMAGESOURCE_HPP
#define IMAGESOURCE_HPP

#include <string>
#include <acvImage_BasicTool.hpp>
class ImageSource_Interface;
typedef int (*ImageSource_Event_callback)(ImageSource_Interface *interface, void* data, void* callback_param);
class ImageSource_Interface
{
private:
  ImageSource_Event_callback callback;
  void* callback_param;
public:
  ImageSource_Interface()
  {
    callback=NULL;
    callback_param = NULL;
  }


  void SetEventCallBack(ImageSource_Event_callback callback, void* callback_param)
  {
    this->callback = callback;
    this->callback_param = callback_param;
  }

  void* GetImage()
  {
    return NULL;
  }
};

class ImageSource_acvImageInterface: public ImageSource_Interface
{
protected:
  acvImage *buffer;
public:
  ImageSource_acvImageInterface(acvImage *buffer): ImageSource_Interface()
  {
    this->buffer = buffer;
  }

  acvImage* GetAcvImage()
  {
    return buffer;
  }
};
class ImageSource_BMP: public ImageSource_acvImageInterface
{
  string fileName;
public:
  ImageSource_BMP(acvImage *buffer): ImageSource_acvImageInterface(buffer)
  {
    fileName = "";
  }

  void SetFileName(string fileName)
  {
    this->fileName=fileName;
  }

  void* GetImage()
  {
    return GetAcvImage();
  }
  acvImage* GetAcvImage()
  {
    if(buffer==NULL || fileName.empty() )return NULL;
    int ret=acvLoadBitmapFile(buffer, fileName.c_str());
    if(ret<0)return NULL;
    return buffer;
  }
};

class ImageSource_GIGE_MindVision: public ImageSource_Interface
{

public:

};


#endif
