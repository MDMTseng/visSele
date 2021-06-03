
#include <cstdint>
#include "MvCameraControl.h"

#include "CameraLayer.hpp"
#include "logctrl.h"
#include <cstring>
#include <vector>
#include <stdexcept>
#include "common_lib.h"

#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

bool PrintDeviceInfo(MV_CC_DEVICE_INFO *pstMVDevInfo)
{
  if (NULL == pstMVDevInfo)
  {
    printf("The Pointer of pstMVDevInfo is NULL!\n");
    return false;
  }
  if (pstMVDevInfo->nTLayerType == MV_CAMERALINK_DEVICE)
  {
    printf("chPortID: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chPortID);
    printf("chModelName: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chModelName);
    printf("chFamilyName: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chFamilyName);
    printf("chDeviceVersion: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chDeviceVersion);
    printf("chManufacturerName: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chManufacturerName);
    printf("Serial Number: [%s]\n", pstMVDevInfo->SpecialInfo.stCamLInfo.chSerialNumber);
  }
  else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
  {

    // unsigned char       CrtlInEndPoint;                             ///< [OUT] \~chinese ��������˵�           \~english Control input endpoint
    // unsigned char       CrtlOutEndPoint;                            ///< [OUT] \~chinese ��������˵�           \~english Control output endpoint
    // unsigned char       StreamEndPoint;                             ///< [OUT] \~chinese ���˵�                 \~english Flow endpoint
    // unsigned char       EventEndPoint;                              ///< [OUT] \~chinese �¼��˵�               \~english Event endpoint
    // unsigned short      idVendor;                                   ///< [OUT] \~chinese ��Ӧ��ID��             \~english Vendor ID Number
    // unsigned short      idProduct;                                  ///< [OUT] \~chinese ��ƷID��               \~english Device ID Number
    // unsigned int        nDeviceNumber;                              ///< [OUT] \~chinese �豸������             \~english Device Number
    // unsigned char       chDeviceGUID[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese �豸GUID��             \~english Device GUID Number
    // unsigned char       chVendorName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ��Ӧ������             \~english Vendor Name
    // unsigned char       chModelName[INFO_MAX_BUFFER_SIZE];          ///< [OUT] \~chinese �ͺ�����               \~english Model Name
    // unsigned char       chFamilyName[INFO_MAX_BUFFER_SIZE];         ///< [OUT] \~chinese ��������               \~english Family Name
    // unsigned char       chDeviceVersion[INFO_MAX_BUFFER_SIZE];      ///< [OUT] \~chinese �豸�汾               \~english Device Version
    // unsigned char       chManufacturerName[INFO_MAX_BUFFER_SIZE];   ///< [OUT] \~chinese ����������             \~english Manufacturer Name
    // unsigned char       chSerialNumber[INFO_MAX_BUFFER_SIZE];       ///< [OUT] \~chinese ���к�                 \~english Serial Number
    // unsigned char       chUserDefinedName[INFO_MAX_BUFFER_SIZE];    ///< [OUT] \~chinese �û��Զ�������         \~english User Defined Name
    // unsigned int        nbcdUSB;                                    ///< [OUT] \~chinese ֧�ֵ�USBЭ��          \~english Support USB Protocol
    // unsigned int        nDeviceAddress;                             ///< [OUT] \~chinese �豸��ַ               \~english Device Address

    // unsigned int        nReserved[2];                               ///<       \~chinese Ԥ��                   \~english Reserved

    printf("chUserDefinedName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    printf("chVendorName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chVendorName);
    printf("chModelName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
    printf("chFamilyName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chFamilyName);
    printf("chDeviceVersion: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chDeviceVersion);
    printf("chManufacturerName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chManufacturerName);
    printf("Serial Number: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
  }
  else
  {
    printf("Not support.\n");
  }

  return true;
}

void _CameraLayer_Callback_(CameraLayer &cl_obj, int type, void *context)
{
  LOGI(":");
}

typedef uint32_t HIKERR;

class CameraLayer_HikRobot_Camera : public CameraLayer
{

public:
  struct cam_info
  {
    std::string id;
    std::string physical_id;
    std::string address;
    std::string vendor;
    std::string model;
    std::string serial_nbr;
    std::string protocol;
    bool available;
  };

protected:
  void *handle;


  cam_info self_info;
  int takeCount;
  bool bDevConnected;
  bool threadRunningState;
  std::thread grabThread;

  
  std::mutex m;
  std::condition_variable conV;
  bool snapFlag=false;

  // std::mutex m;
  // std::condition_variable conV;
  // bool snapFlag=false;
  // static void s_STREAM_NEW_BUFFER_CB(ArvStream *stream, CameraLayer_Aravis *self);
  // void STREAM_NEW_BUFFER_CB(ArvStream *stream);
  // static void s_STREAM_CONTROL_LOST_CB(ArvStream *stream, CameraLayer_Aravis *self);
  // void STREAM_CONTROL_LOST_CB(ArvStream *stream);

  static void sExceptionCallBack(unsigned int nMsgType, void *context);
  void ExceptionCallBack(unsigned int nMsgType);

  static void sImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *context);
  void ImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo);

  void CLOSE();

  int GetIntValue(const char *strKey, MVCC_INTVALUE_EX *pIntValue)
  {
    return MV_CC_GetIntValueEx(handle, strKey, pIntValue);
  }

  int SetIntValue(const char *strKey, int64_t nValue)
  {
    return MV_CC_SetIntValueEx(handle, strKey, nValue);
  }

  int SetIntValue_w_Check(const char *strKey, int64_t target)
  {
    MVCC_INTVALUE_EX intValInfo={0};
    GetIntValue(strKey, &intValInfo);
    int value = leastSatiValue(target,intValInfo);
    return SetIntValue(strKey,value);
  }

  int GetEnumValue(const char *strKey, MVCC_ENUMVALUE *pEnumValue)
  {
    return MV_CC_GetEnumValue(handle, strKey, pEnumValue);
  }

  int SetEnumValue(const char *strKey, unsigned int nValue)
  {
    return MV_CC_SetEnumValue(handle, strKey, nValue);
  }

  int SetEnumValueByString(const char *strKey, const char *sValue)
  {
    return MV_CC_SetEnumValueByString(handle, strKey, sValue);
  }

  int GetFloatValue(const char *strKey, MVCC_FLOATVALUE *pFloatValue)
  {
    return MV_CC_GetFloatValue(handle, strKey, pFloatValue);
  }

  int SetFloatValue(const char *strKey, float fValue)
  {
    return MV_CC_SetFloatValue(handle, strKey, fValue);
  }

  int GetBoolValue(const char *strKey, bool *pbValue)
  {
    return MV_CC_GetBoolValue(handle, strKey, pbValue);
  }

  int SetBoolValue(const char *strKey, bool bValue)
  {
    return MV_CC_SetBoolValue(handle, strKey, bValue);
  }

  int GetStringValue(const char *strKey, MVCC_STRINGVALUE *pStringValue)
  {
    return MV_CC_GetStringValue(handle, strKey, pStringValue);
  }

  int SetStringValue(const char *strKey, const char *strValue)
  {
    return MV_CC_SetStringValue(handle, strKey, strValue);
  }

  int CommandExecute(const char *strKey)
  {
    return MV_CC_SetCommandValue(handle, strKey);
  }

  static int leastSatiValue(int target,MVCC_INTVALUE_EX range)
  {
    if(target<range.nMin)return range.nMin;
    if(target>range.nMax)return range.nMax;
    int value=range.nMin+((target-range.nMin)/range.nInc)*range.nInc;
    return (target>value)?value+range.nInc:value;
  }

  void grabThreadFunc()
  {

    MV_FRAME_OUT stImageInfo = {0};
    MV_DISPLAY_FRAME_INFO stDisplayInfo = {0};
    int nRet = MV_OK;
    while (threadRunningState)
    {

      nRet = MV_CC_GetImageBuffer(handle, &stImageInfo, 1000);
      if (nRet == MV_OK)
      {
        LOGI("MV_OK");
      }
      else
      {
        
        LOGI("MV_:::%X",nRet);
        // if (MV_TRIGGER_MODE_ON == m_nTriggerMode)
        // {
        //   Sleep(5);
        // }
      }
    }
  }

public:
  CameraLayer_HikRobot_Camera(MV_CC_DEVICE_INFO *devInfo, CameraLayer_Callback cb, void *context);

  // CameraLayer::status EnumerateDevice(tSdkCameraDevInfo *pCameraList, INT *piNums);
  // CameraLayer::status InitCamera(tSdkCameraDevInfo *devInfo);
  static int32_t listDevices(MV_CC_DEVICE_INFO_LIST *stDeviceList);

  CameraLayer::status TriggerMode(int type);
  CameraLayer::status Trigger();
  CameraLayer::status SnapFrame();

  int getXML(char* buffer,int bufferSize);
  
  CameraLayer::status SetROI(int x, int y, int w, int h, int zw, int zh);

  CameraLayer::status GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh);

  // CameraLayer::status SetResolution(int width, int height);

  // CameraLayer::status TriggerCount(int count);
  // CameraLayer::status RUN();
  // cam_info getCameraInfo();
  // CameraLayer::status SetAnalogGain(int gain);
  // CameraLayer::status SetOnceWB();

  // CameraLayer::status SetMirror(int Dir, int en);
  // CameraLayer::status SetROIMirror(int Dir, int en);
  // CameraLayer::status SetFrameRate(float frame_rate);
  // CameraLayer::status SetFrameRateMode(int mode);
  // CameraLayer::status GetAnalogGain(int *ret_min, int *ret_max);
  // CameraLayer::status SetExposureTime(float time_us);
  // CameraLayer::status GetExposureTime(float *ret_time_us);
  // void ContTriggerThread();
  // void ContTriggerThreadTermination();
  // acvImage *GetFrame();

  ~CameraLayer_HikRobot_Camera();
};

CameraLayer::status CameraLayer_HikRobot_Camera::SetROI(int x, int y, int w, int h, int zw, int zh)
{

  int max_w,max_h;
  
  MVCC_INTVALUE_EX WInfo={0};
  GetIntValue("Width", &WInfo);
  MVCC_INTVALUE_EX HInfo={0};
  GetIntValue("Height", &HInfo);
  max_w=WInfo.nMax;
  max_h=HInfo.nMax;
  if(x>=max_w || y>=max_h || w<0 || h<0)
  {
    return CameraLayer::NAK;
  }
  if(x<0)
  {
    w+=x;
    x=0;
  }
  if(y<0)
  {
    h+=y;
    y=0;
  }


  if(x+w>max_w)
  {
    w=max_w-x;
  }

  if(y+h>max_h)
  {
    h=max_h-y;
  }

  MV_CC_StopGrabbing(handle);
  SetIntValue_w_Check("OffsetX", (int)x);
  SetIntValue_w_Check("OffsetY", (int)y);
  SetIntValue_w_Check("Width", (int)w);
  SetIntValue_w_Check("Height",(int)h);
  MV_CC_StartGrabbing(handle);


  return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_HikRobot_Camera::GetROI(int *x, int *y, int *w, int *h, int *zw, int *zh)
{
  MVCC_INTVALUE_EX WInfo={0};
  GetIntValue("Width", &WInfo);
  MVCC_INTVALUE_EX HInfo={0};
  GetIntValue("Height", &HInfo);
  MVCC_INTVALUE_EX OXInfo={0};
  GetIntValue("OffsetX", &OXInfo);
  MVCC_INTVALUE_EX OYInfo={0};
  GetIntValue("OffsetY", &OYInfo);

  if(x)*x=OXInfo.nCurValue;
  if(y)*y=OYInfo.nCurValue;
  if(w)*w=WInfo.nCurValue;
  if(h)*h=HInfo.nCurValue;
  return CameraLayer::ACK;
}


int CameraLayer_HikRobot_Camera::getXML(char* buffer,int bufferSize)
{
  unsigned int nXMLDataLen = 0; 
  int nRet = MV_XML_GetGenICamXML(handle, (unsigned char*)buffer, bufferSize, &nXMLDataLen); 
  if(buffer==NULL && bufferSize==0)
  {
    return nXMLDataLen;
  }
  if (MV_OK != nRet)
  {
    return -1;
  }
  return nXMLDataLen;
}

void CameraLayer_HikRobot_Camera::sExceptionCallBack(unsigned int nMsgType, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;
  cl->ExceptionCallBack(nMsgType);
}

void CameraLayer_HikRobot_Camera::ExceptionCallBack(unsigned int nMsgType)
{
  LOGI("ExceptionCallBack");
}
void CameraLayer_HikRobot_Camera::sImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *context)
{
  CameraLayer_HikRobot_Camera *cl = (CameraLayer_HikRobot_Camera *)context;
  cl->ImageCallBack(pData, pFrameInfo);
}

void CameraLayer_HikRobot_Camera::ImageCallBack(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo)
{
  uint32_t tstH=pFrameInfo->nDevTimeStampHigh;
  uint32_t tstL=pFrameInfo->nDevTimeStampLow;
  uint64_t nDevTimeStamp=(((uint64_t)tstH)<<32)|tstL;//10ns
  uint64_t nHostTimeStamp=pFrameInfo->nHostTimeStamp;//1ms

  uint64_t nDevTimeStamp_us=nDevTimeStamp/100;
  uint64_t nHostTimeStamp_us=nHostTimeStamp*1000;
  
  MvGvspPixelType pType=pFrameInfo->enPixelType;
  LOGI("ImageCallBack:%d,%d %dx%d type:%0X len:%d  %d,%d,%d nDevTimeStamp:%lu host:%lu",
    pFrameInfo->nOffsetX,pFrameInfo->nOffsetY,
    pFrameInfo->nWidth,pFrameInfo->nHeight,
    pType,pFrameInfo->nFrameLen,
    pData[0],pData[1],pData[2],nDevTimeStamp_us,nHostTimeStamp_us);


  if(snapFlag)
  {
    snapFlag=0;

    conV.notify_one();
  } 
}

int32_t CameraLayer_HikRobot_Camera::listDevices(MV_CC_DEVICE_INFO_LIST *stDeviceList)
{
  if (stDeviceList == NULL)
    return MV_E_PARAMETER;
  memset(stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

  int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, stDeviceList);

  return nRet;
}

CameraLayer_HikRobot_Camera::CameraLayer_HikRobot_Camera(MV_CC_DEVICE_INFO *devInfo, CameraLayer_Callback cb, void *context)
    : CameraLayer(cb, context)
{
  bDevConnected = false;
  if (devInfo == NULL)
  {
    throw std::invalid_argument("NULL devInfo");
  }

  HIKERR nRet = MV_CC_CreateHandle(&handle, devInfo);
  if (MV_OK != nRet)
  {
    string excpMsg = "target device handle creation failed";
    throw std::invalid_argument(excpMsg);
  }

  if (devInfo->nTLayerType == MV_GIGE_DEVICE)
  { //if it's GIGE device, try detect Optimal Packet Size
    //And configure it

    do
    {
      int32_t nRet = MV_CC_GetOptimalPacketSize(handle);
      if (nRet < MV_OK)
      {
        break;
      }

      int32_t pOptimalPacketSize = (int32_t)nRet;

      nRet = MV_CC_SetIntValueEx(handle, "GevSCPSPacketSize", pOptimalPacketSize);
      if (nRet != MV_OK)
      {
        break;
      }
    } while (0);
  }

  nRet = MV_CC_OpenDevice(handle);

  if (MV_OK != nRet)
  {
    CLOSE();
    string excpMsg = "target device open/connect failed";
    throw std::invalid_argument(excpMsg);
  }
  bDevConnected = true;

  nRet = MV_CC_RegisterImageCallBackEx(handle, sImageCallBack, this);
  if (MV_OK != nRet)
  {

    CLOSE();
    string excpMsg = "RegisterImageCallBack failed";
    throw std::invalid_argument(excpMsg);
  }

  nRet = MV_CC_RegisterExceptionCallBack(handle, sExceptionCallBack, this);
  if (MV_OK != nRet)
  {
  }

  {
    
    MVCC_INTVALUE_EX intValInfo={0};
    GetIntValue("OffsetX", &intValInfo);
    LOGI("%d<[%d]<%d....%d",intValInfo.nMin,intValInfo.nCurValue,intValInfo.nMax,intValInfo.nInc);

  }


  // SetROI(1000,1000,200,200,0,0);

  MV_IMAGE_BASIC_INFO img_basic_info;
  MV_CC_GetImageInfo(handle, &img_basic_info);
  TriggerMode(1);
  MV_CC_StartGrabbing(handle);
  threadRunningState=true;
  
  SetROI(0,0,99999,99999,0,0);
  // grabThread = std::thread(&CameraLayer_HikRobot_Camera::grabThreadFunc, this);
}

CameraLayer::status CameraLayer_HikRobot_Camera::SnapFrame()
{
  
  TriggerMode(1);
  snapFlag=1;
  //trigger reset;
  LOGI("TRIGGER");
  {
    std::unique_lock<std::mutex> lock(m);
    for(int i=0;Trigger() == CameraLayer::NAK;i++)
    {
      if(i>5)
      {
        return CameraLayer::NAK;
      }
    }
    conV.wait(lock, [this] { return this->snapFlag==0; });
  }
  LOGI("END");
  return CameraLayer::ACK;
}


void CameraLayer_HikRobot_Camera::CLOSE()
{
  if (bDevConnected)
  {
    MV_CC_CloseDevice(handle);
    bDevConnected = false;
  }
  if (handle != NULL)
  {
    MV_CC_DestroyHandle(handle);
    handle = NULL;
  }
}
CameraLayer_HikRobot_Camera::~CameraLayer_HikRobot_Camera()
{
  CLOSE();
}

CameraLayer::status CameraLayer_HikRobot_Camera::TriggerMode(int type)
{
  if (type == 0) //continuous
  {
    int nRet = SetEnumValue("TriggerMode", MV_TRIGGER_MODE_OFF);
    SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  int nRet = SetEnumValue("TriggerMode", MV_TRIGGER_MODE_ON);

  // MV_CC_StopGrabbing(handle);
  if (type == 1) //software trigger
  {
    int nRet = SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_SOFTWARE);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  if (type == 2) //hardware trigger
  {
    int nRet = SetEnumValue("TriggerSource", MV_TRIGGER_SOURCE_LINE0);
    return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
  }

  return CameraLayer::NAK;
}

CameraLayer::status CameraLayer_HikRobot_Camera::Trigger()
{
  
  int nRet = CommandExecute("TriggerSoftware");
  return (MV_OK == nRet) ? CameraLayer::ACK : CameraLayer::NAK;
}
int demomain(int argc, char **argv)
{

  int nRet = MV_OK;
  std::vector<void *> cameraHandles;
  if (MV_OK != nRet)
  {
    LOGI("Enum Devices fail! nRet [0x%x]", nRet);
    return -1;
  }
  MV_CC_DEVICE_INFO_LIST stDeviceList;
  CameraLayer_HikRobot_Camera::listDevices(&stDeviceList);
  if (stDeviceList.nDeviceNum > 0)
  {
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
    {
      printf("[device %d]:", i);
      MV_CC_DEVICE_INFO *pstMVDevInfo = stDeviceList.pDeviceInfo[i];
      if (NULL == pstMVDevInfo)
      {
        return -1;
      }
      PrintDeviceInfo(pstMVDevInfo);
      void *handle = NULL;
      nRet = MV_CC_CreateHandle(&handle, pstMVDevInfo);
      if (MV_OK == nRet)
      {
        cameraHandles.push_back(handle);
      }
    }

    CameraLayer_HikRobot_Camera camera1(stDeviceList.pDeviceInfo[0], _CameraLayer_Callback_, NULL);

    if(0)
    {
      
      size_t xmlSize=camera1.getXML(NULL,0);
      LOGI("XML size:%d",xmlSize);
      char* xmlBuffer=new char[xmlSize+1];
      camera1.getXML(xmlBuffer,xmlSize+1);
      
      WriteBytesToFile((uint8_t*)xmlBuffer,xmlSize+1,"data/XML_camera.xml");
      printf("==================\n");
      delete(xmlBuffer);


    }

    camera1.SetROI(1000,1000,500,500,0,0);
    // camera1.SetIntValue_w_Check("OffsetX", 200);
    // camera1.SetIntValue_w_Check("OffsetY", 200);
    // camera1.SetIntValue_w_Check("Width", 300);
    // camera1.SetIntValue_w_Check("Height",300);
    // _sleep(1);
    // camera1.TriggerMode(0);
    while (1)
    {
      LOGI("=======================");
      LOGI("Strig1");
      camera1.Trigger();
      _sleep(2000);
      
      LOGI("Strig2");
      camera1.Trigger();
      _sleep(2000);
      
      LOGI("SnapFrame S");
      camera1.SnapFrame();
      LOGI("SnapFrame E");

      _sleep(2000);

      LOGI("cont' trig start");
      camera1.TriggerMode(0);
      _sleep(5000);
      LOGI("cont' trig end");
      camera1.TriggerMode(1);
      _sleep(1000);
    }
  }
  else
  {
    printf("Find No Devices!\n");
    return -1;
  }

  return nRet;
}