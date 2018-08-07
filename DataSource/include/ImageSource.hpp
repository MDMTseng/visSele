#ifndef IMAGESOURCE_HPP
#define IMAGESOURCE_HPP

#include <string>
#include <acvImage_BasicTool.hpp>
class ImageSource_Interface;

typedef struct ImageSource_Data_error
{

  enum error_enum
  {
    NO_ENOUGH_BUFFER,
    FILE_READ_FAILED,
    UNKNOWN_FILE_NAME
  } code;

};

enum ImageSource_DataType
{
  ImageSource_DataType_error,
  ImageSource_DataType_BMP_Read,
  ImageSource_DataType_raw,
  ImageSource_DataType_END,
};

typedef struct ImageSource_Data_BMP_Read
{
    acvImage* img;
};

typedef struct ImageSource_Data
{
  enum ImageSource_DataType type;
  union data    
  {
    ImageSource_Data_error error;
    ImageSource_Data_BMP_Read BMP_Read;
    char*     raw;
  }data;
}ImageSource_Data;

typedef int (*ImageSource_Event_callback)(ImageSource_Interface *interface, ImageSource_Data data, void* callback_param);
class ImageSource_Interface
{
protected:
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

  virtual void* GetImage()
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

  virtual acvImage* GetAcvImage()
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

    if(callback!=NULL)
    {
      GetAcvImage();
    }
  }

  void* GetImage()
  {
    return GetAcvImage();
  }
  ImageSource_Data GenErrorMsg(ImageSource_Data_error::error_enum code)
  {
      ImageSource_Data error_data;
      error_data.type = ImageSource_DataType_error;
      error_data.data.error.code=code;
      return error_data;
  }

  acvImage* GetAcvImage()
  {
    if(buffer==NULL)
    {
      callback(this, GenErrorMsg(ImageSource_Data_error::error_enum::NO_ENOUGH_BUFFER), callback_param);
      return NULL;
    }

    if(fileName.empty() )
    {
      callback(this, GenErrorMsg(ImageSource_Data_error::error_enum::UNKNOWN_FILE_NAME), callback_param);
      return NULL;
    }
    int ret=acvLoadBitmapFile(buffer, fileName.c_str());
    if(ret<0)
    {
      callback(this, GenErrorMsg(ImageSource_Data_error::error_enum::FILE_READ_FAILED), callback_param);
      return NULL;
    }

    if(callback!=NULL)
    {
      ImageSource_Data img_data;
      img_data.type = ImageSource_DataType_BMP_Read;
      img_data.data.BMP_Read.img=buffer;
      callback(this, img_data, callback_param);
    }
    return buffer;
  }
};


class ImageSource_GIGE_MindVision: public ImageSource_acvImageInterface
{
  string fileName;
public:
  ImageSource_GIGE_MindVision(acvImage *buffer): ImageSource_acvImageInterface(buffer)
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


#endif
