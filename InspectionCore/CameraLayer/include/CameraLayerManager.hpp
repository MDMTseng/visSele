#pragma once




#include "CameraLayer_BMP.hpp"

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
#include "CameraLayer_HikRobot_Camera.hpp"
#endif

#ifdef FEATURE_COMPILE_W_ARAVIS
#include "CameraLayer_Aravis.hpp"
#endif


#ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK
#include "CameraLayer_GIGE_MindVision.hpp"
#endif


class CameraLayerManager
{
  
  public:
  std::vector<CameraLayer::BasicCameraInfo> camBasicInfo;
  std::string strJsonAppendKeyString(std::string key,std::string strData)
  {
    return "\""+key+"\":\""+strData+"\"";
  }
  std::string strJsonAppendKeyEntry(std::string key,std::string strData)
  {
    return "\""+key+"\":"+strData;
  }


#ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK

  // tSdkCameraDevInfo sCameraList[30];



  // std::string CamInfo2Json(CameraLayer_HikRobot_Camera::cam_info &info)
  // {
  //   return "{"
  //     +strJsonAppendKeyString("id",info.id)+","
  //     +strJsonAppendKeyString("phy_id",info.physical_id)+","
  //     +strJsonAppendKeyString("address",info.address)+","
  //     +strJsonAppendKeyString("vendor",info.vendor)+","
  //     +strJsonAppendKeyString("model",info.model)+","
  //     +strJsonAppendKeyString("serial_nbr",info.serial_nbr)+","
  //     +strJsonAppendKeyString("protocol",info.protocol)
  //   +"}";
  // }

  // std::string CamList2Json(std::vector<CameraLayer_HikRobot_Camera::cam_info> &list)
  // {
  //   std::string jobj="{\"list\":[";

  //   for(int i=0;i<list.size();i++)
  //   {
  //     CameraLayer_HikRobot_Camera::cam_info &cinfo=list[i];
  //     jobj+=CamInfo2Json(cinfo);
  //     if(i<list.size()-1)//not the last data
  //     {
  //       jobj+=",";
  //     }
  //   }
  //   jobj+="]}";

  //   return jobj;




  // int retListL = sizeof(sCameraList) / sizeof(sCameraList[0]);
  // CameraLayer_GIGE_MindVision::EnumerateDevice(sCameraList, &retListL);

  // if (retListL <= 0)
  //   return NULL;
  // for (int i = 0; i < retListL; i++)
  // {
  //   printf("CAM:%d======\n", i);
  //   printf("acDriverVersion:%s\n", sCameraList[i].acDriverVersion);
  //   printf("acFriendlyName:%s\n", sCameraList[i].acFriendlyName);
  //   printf("acLinkName:%s\n", sCameraList[i].acLinkName);
  //   printf("acPortType:%s\n", sCameraList[i].acPortType);
  //   printf("acProductName:%s\n", sCameraList[i].acProductName);
  //   printf("acProductSeries:%s\n", sCameraList[i].acProductSeries);
  //   printf("acSensorType:%s\n", sCameraList[i].acSensorType);
  //   printf("acSn:%s\n", sCameraList[i].acSn);
  //   printf("\n\n\n\n");
  // }
  // CameraLayer_GIGE_MindVision *CL_GIGE = new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV, NULL);
  // if (CL_GIGE->InitCamera(&(sCameraList[0])) == CameraLayer::ACK)
  // {
  //   return CL_GIGE;
  // }
  // delete CL_GIGE;



  // }


#endif


#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
  MV_CC_DEVICE_INFO_LIST hikrobotCamList={0};

  std::vector<CameraLayer_HikRobot_Camera::cam_info> hikrCamList;



  std::string CamInfo2Json(CameraLayer_HikRobot_Camera::cam_info &info)
  {
    return "{"
      +strJsonAppendKeyString("id",info.id)+","
      +strJsonAppendKeyString("phy_id",info.physical_id)+","
      +strJsonAppendKeyString("address",info.address)+","
      +strJsonAppendKeyString("vendor",info.vendor)+","
      +strJsonAppendKeyString("model",info.model)+","
      +strJsonAppendKeyString("serial_nbr",info.serial_nbr)+","
      +strJsonAppendKeyString("protocol",info.protocol)
    +"}";
  }

  std::string CamList2Json(std::vector<CameraLayer_HikRobot_Camera::cam_info> &list)
  {
    std::string jobj="{\"list\":[";

    for(int i=0;i<list.size();i++)
    {
      CameraLayer_HikRobot_Camera::cam_info &cinfo=list[i];
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


#ifdef FEATURE_COMPILE_W_ARAVIS
  std::vector<CameraLayer_Aravis::cam_info> aravisCamList;



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

  std::string CamList2Json(std::vector<CameraLayer_Aravis::cam_info> &list)
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



  std::string CamInfo2Json(CameraLayer::BasicCameraInfo &info);

  std::string CamList2Json(std::vector<CameraLayer::BasicCameraInfo> &list);


  int mindvision_driver_idx=-1;
  int hikrobot_driver_idx=-1;
  int aravis_driver_idx=-1;
  int bmpcarousel_driver_idx=-1;


  void discover();
  void discoverListClear();
  int discoveredCameraCount();

  std::string genJsonStringList();


  CameraLayer* connectCamera(int camera_idx,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx);
  //driver_name can be "" empty string to skip driver info locating
  CameraLayer* connectCamera(std::string driver_name,std::string cam_id,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx);


//   CameraLayer* connectCamera(std::string driver,std::string id,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx)
//   {
    
// #ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
//     if(driver=="hikrobot")
//     {
//       return NULL;
//     }
// #endif
// #ifdef FEATURE_COMPILE_W_ARAVIS
//     if(driver_idx==aravis_driver_idx)
//     {
//       if(idx<0||idx>=aravisCamList.size())return NULL;
//       if (aravisCamList[idx].available==false)return NULL;
//       try
//       {
//         return new CameraLayer_Aravis(aravisCamList[idx].id.c_str(), callback, ctx);
//       }
//       catch (const std::exception &ex)
//       {
//       }
//       return NULL;
//     }
// #endif

//     if(driver_idx==bmpcarousel_driver_idx)
//     {
//       return new CameraLayer_BMP_carousel(callback, ctx, misc);
//     }
//     return NULL;
//   }



};

