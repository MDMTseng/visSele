#pragma once




#include "CameraLayer_BMP.hpp"

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
#include "CameraLayer_HikRobot_Camera.hpp"
#endif

#ifdef FEATURE_COMPILE_W_ARAVIS
#include "CameraLayer_Aravis.hpp"
#endif


class CameraLayerManager
{
  
  std::string strJsonAppendKeyEntry(std::string key,std::string strData)
  {
    return "\"+key+\":\"+strData+\"";
  }

  std::string strJsonAppendKeyEntry(std::string key,int number)
  {
    return "\"+key+\":"+number;
  }

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
  MV_CC_DEVICE_INFO_LIST hikrobotCamList={0};

  std::string CamInfo2Json(MV_CC_DEVICE_INFO &info)
  {
    return "{"+
      strJsonAppendKeyEntry("id",info)+","+
    
    
    
    "}";
  }

  std::string CamList2Json(MV_CC_DEVICE_INFO_LIST &list)
  {
    std::string jobj="{\"cameras\":[";

    for(int i=0;i<list.nDeviceNum;i++)
    {
      
      MV_CC_DEVICE_INFO *pstMVDevInfo = list.pDeviceInfo[i];
      jobj+=CamInfo2Json(*pstMVDevInfo);
      if(i<list.nDeviceNum-1)//not the last data
      {
        jobj+=",";
      }
    }
    jobj+="]}";

    return jobj;

  }



#endif


#ifdef FEATURE_COMPILE_W_ARAVIS

  vector<CameraLayer_Aravis::cam_info> aravisCamList;

  std::string aravisCamList2Json(vector<CameraLayer_Aravis::cam_info> &list)
  {
    
  }

#endif




  void discover()
  {

    #ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK

      CameraLayer_HikRobot_Camera::listDevices(&hikrobotCamList);
    #endif


    #ifdef FEATURE_COMPILE_W_ARAVIS

      CameraLayer_Aravis::listDevices(aravisCamList, true);
    #endif


  }

  std::string genJsonStringList()
  {

  }



}

