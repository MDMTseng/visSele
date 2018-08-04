#ifndef IMAGESOURCE_HPP
#define IMAGESOURCE_HPP

#include <string>
#include <acvImage_BasicTool.hpp>

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

class ImageSource_BMP: public ImageSource_Interface
{
  string fileName;
  acvImage *buffer;
public:
  ImageSource_BMP(acvImage *buffer): ImageSource_Interface()
  {
    this->buffer = buffer;
    fileName = "";
  }

  void SetFileName(string fileName);
  {
    this->fileName=fileName;
  }

  void* GetImage()
  {
    if(buffer==NULL || str1.empty() )return NULL;



    int ret=acvLoadBitmapFile(buffer, fileName);
    if(ret<0)return NULL;
    return buffer;
  }
};

class ImageSource_GIGE_MindVision: public ImageSource_Interface
{

public:

};


#endif
