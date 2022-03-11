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
  public:
  std::string strJsonAppendKeyString(std::string key,std::string strData)
  {
    return "\""+key+"\":\""+strData+"\"";
  }
  std::string strJsonAppendKeyEntry(std::string key,std::string strData)
  {
    return "\""+key+"\":"+strData;
  }

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
  MV_CC_DEVICE_INFO_LIST hikrobotCamList={0};

  std::string CamInfo2Json(MV_CC_DEVICE_INFO &info)
  {
    return "{"+
      strJsonAppendKeyEntry("id",info.)+","+
    
    
    
    "}";
  }

  std::string CamList2Json(MV_CC_DEVICE_INFO_LIST &list)
  {
    std::string jobj="{\"list\":[";

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



  std::string CamInfo2Json(CameraLayer_Aravis::cam_info &info)
  {
    return "{"
      +strJsonAppendKeyString("id",info.id)+","
      +strJsonAppendKeyString("phy_id",info.physical_id)+","
      +strJsonAppendKeyString("address",info.address)+","
      +strJsonAppendKeyString("vendor",info.vendor)+","
      +strJsonAppendKeyString("model",info.model)+","
      +strJsonAppendKeyString("serial_nbr",info.serial_nbr)+","
      +strJsonAppendKeyString("protocol",info.protocol)+","
      +strJsonAppendKeyEntry("available",info.available?"true":"false")
    +"}";
  }

  std::string CamList2Json(vector<CameraLayer_Aravis::cam_info> &list)
  {
    std::string jobj="{\"list\":[";

    for(int i=0;i<list.size();i++)
    {
      CameraLayer_Aravis::cam_info &cinfo=list[i];
      jobj+=CamInfo2Json(cinfo);
      if(i<list.size()-1)//not the last data
      {
        jobj+=",";
      }
    }
    jobj+="]}";

    return jobj;

  }


#endif

  int hikrobot_driver_idx=-1;
  int aravis_driver_idx=-1;
  int bmpcarousel_driver_idx=-1;


  void discover()
  {
    int count=0;
    #ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK

      CameraLayer_HikRobot_Camera::listDevices(&hikrobotCamList);
      hikrobot_driver_idx=count++;
    #endif


    #ifdef FEATURE_COMPILE_W_ARAVIS

      CameraLayer_Aravis::listDevices(aravisCamList, true);
      aravis_driver_idx=count++;
    #endif

    bmpcarousel_driver_idx=count++;

  }


  std::string genJsonStringList()
  {
    std::string jobj=std::string("{")
    +"\"aravis\":"+CamList2Json(aravisCamList)+","
    +"\"bmpcarousel\":{}"//+","
    +"}";

    return jobj;
  }


  CameraLayer* connectCamera(int driver_idx,int idx,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx)
  {
    
#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
    if(driver_idx==hikrobot_driver_idx)
    {
      return NULL;
    }
#endif
#ifdef FEATURE_COMPILE_W_ARAVIS
    if(driver_idx==aravis_driver_idx)
    {
      if(idx<0||idx>=aravisCamList.size())return NULL;
      if (aravisCamList[idx].available==false)return NULL;
      try
      {
        return new CameraLayer_Aravis(aravisCamList[idx].id.c_str(), callback, ctx);
      }
      catch (const std::exception &ex)
      {
      }
      return NULL;
    }
#endif

    if(driver_idx==bmpcarousel_driver_idx)
    {
      return new CameraLayer_BMP_carousel(callback, ctx, misc);
    }
    return NULL;
  }

};

