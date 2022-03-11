
#include <cstdint>
#include "MvCameraControl.h"

#include "CameraLayer.hpp"
#include "CameraLayer_HikRobot_Camera.hpp"

#include "logctrl.h"
#include <cstring>
#include <vector>
#include <stdexcept>
#include "common_lib.h"

#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

void _CameraLayer_Callback_(CameraLayer &cl_obj, int type, void *context)
{
  LOGI(":");
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

    if (1)
    {

      size_t xmlSize = camera1.getXML(NULL, 0);
      LOGI("XML size:%d", xmlSize);
      char *xmlBuffer = new char[xmlSize + 1];
      camera1.getXML(xmlBuffer, xmlSize + 1);

      WriteBytesToFile((uint8_t *)xmlBuffer, xmlSize + 1, "data/XML_camera.xml");
      printf("==================\n");
      delete (xmlBuffer);
    }

    camera1.SetExposureTime(1000000);
    camera1.SetFrameRate(10000);

    // camera1.SetROI(1000, 1000, 500, 500, 0, 0);
    // camera1.SetIntValue_w_Check("OffsetX", 200);
    // camera1.SetIntValue_w_Check("OffsetY", 200);
    // camera1.SetIntValue_w_Check("Width", 300);
    // camera1.SetIntValue_w_Check("Height",300);
    // _sleep(1);
    // camera1.TriggerMode(0);
    camera1.TriggerMode(2);
    while (1)
    {
      // while (1)_sleep(2000);
      // LOGI("=======================");

      // LOGI("Strig1");
      // camera1.Trigger();
      // _sleep(2000);

      // LOGI("Strig2");
      // camera1.Trigger();
      // _sleep(2000);

      // LOGI("SnapFrame S");
      // camera1.SnapFrame();
      // LOGI("SnapFrame E");
      // _sleep(2000);

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