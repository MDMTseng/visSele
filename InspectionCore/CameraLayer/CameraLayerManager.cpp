

#include "CameraLayerManager.hpp"

  std::string CameraLayerManager::CamInfo2Json(CameraLayer::BasicCameraInfo &info)
  {
    return "{"
      +strJsonAppendKeyString("driver_name",info.driver_name)+","
      +strJsonAppendKeyString("name",info.name)+","
      +strJsonAppendKeyString("id",info.id)+","
      +strJsonAppendKeyString("vendor",info.model)+","
      +strJsonAppendKeyString("model",info.serial_number)+","
      +strJsonAppendKeyString("serial_nbr",info.vender)
    +"}";
  }

  std::string CameraLayerManager::CamList2Json(std::vector<CameraLayer::BasicCameraInfo> &list)
  {
    std::string jobj="[";

    for(int i=0;i<list.size();i++)
    {
      jobj+=CamInfo2Json(list[i]);
      if(i<list.size()-1)//not the last data
      {
        jobj+=",";
      }
    }
    jobj+="]";

    return jobj;

  }

  void CameraLayerManager::discover()
  {
    camBasicInfo.clear();
    int count=0;



    #ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK
    //   tSdkCameraDevInfo sCameraList[10];
    //   int retListL = sizeof(sCameraList) / sizeof(sCameraList[0]);
    //   CameraLayer_GIGE_MindVision::EnumerateDevice(sCameraList, &retListL);

    //   // if (retListL <= 0)
    //   //   return NULL;
    //   for (int i = 0; i < retListL; i++)
    //   {
    //     printf("CAM:%d======\n", i);
    //     printf("acDriverVersion:%s\n", sCameraList[i].acDriverVersion);
    //     printf("acFriendlyName:%s\n", sCameraList[i].acFriendlyName);
    //     printf("acLinkName:%s\n", sCameraList[i].acLinkName);
    //     printf("acPortType:%s\n", sCameraList[i].acPortType);
    //     printf("acProductName:%s\n", sCameraList[i].acProductName);
    //     printf("acProductSeries:%s\n", sCameraList[i].acProductSeries);
    //     printf("acSensorType:%s\n", sCameraList[i].acSensorType);
    //     printf("acSn:%s\n", sCameraList[i].acSn);
    //     printf("\n\n\n\n");
    //   }
      CameraLayer_GIGE_MindVision::listAddDevices(camBasicInfo);

      for(int i=0;i<camBasicInfo.size();i++)
      {
        LOGI("name:%s  sn:%s",camBasicInfo[i].name.c_str(),camBasicInfo[i].serial_number.c_str());
      }

      mindvision_driver_idx=count++;
    #endif
    #ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
      // CameraLayer_HikRobot_Camera::
      
      // CameraLayer_HikRobot_Camera::listDevices(&hikrobotCamList);
      CameraLayer_HikRobot_Camera::listAddDevices(camBasicInfo);
      // CameraLayer_HikRobot_Camera::listDevices(&hikrobotCamList);



      hikrobot_driver_idx=count++;
    #endif


    #ifdef FEATURE_COMPILE_W_ARAVIS

      CameraLayer_Aravis::listAddDevices(camBasicInfo);
      // CameraLayer_Aravis::listDevices(aravisCamList, true);
      aravis_driver_idx=count++;

      
    #endif
    

    CameraLayer_BMP_carousel::listAddDevices(camBasicInfo);
    bmpcarousel_driver_idx=count++;

  }


  std::string CameraLayerManager::genJsonStringList()
  {

    return CamList2Json(camBasicInfo);
  }

  // std::string genCameraBasicInfoList()
  // {

  //   return jobj;
  // }

//   CameraLayer* connectCamera(int driver_idx,int idx,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx)
//   {
    
// #ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
//     if(driver_idx==hikrobot_driver_idx)
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

  CameraLayer* CameraLayerManager::connectCamera(int camera_idx,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx)
  {
    if(camera_idx<0 || camera_idx>=camBasicInfo.size())return NULL;
    // printf(">>camera_idx:%d>>\n",camera_idx);

// #ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK

// #endif

#ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK

    if(camBasicInfo[camera_idx].driver_name==CameraLayer_GIGE_MindVision::getDriverName())
    {
      printf(">>>>>>driver_name:%s>>\n",camBasicInfo[camera_idx].driver_name.c_str());
      try
      {
        return new CameraLayer_GIGE_MindVision(camBasicInfo[camera_idx],misc, callback, ctx);
      }
      catch (const std::exception &ex)
      {
      }
      return NULL;
    }





#endif


#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK

    if(camBasicInfo[camera_idx].driver_name==CameraLayer_HikRobot_Camera::getDriverName())
    {
      printf(">>>>>>driver_name:%s>>\n",camBasicInfo[camera_idx].driver_name.c_str());
      try
      {
        return new CameraLayer_HikRobot_Camera(camBasicInfo[camera_idx],misc, callback, ctx);
      }
      catch (const std::exception &ex)
      {
      }
      return NULL;
    }





#endif
#ifdef FEATURE_COMPILE_W_ARAVIS
  
    if(camBasicInfo[camera_idx].driver_name==CameraLayer_Aravis::getDriverName())
    {
      printf(">>>>>>driver_name:%s>>\n",camBasicInfo[camera_idx].driver_name.c_str());
      try
      {
        return new CameraLayer_Aravis(camBasicInfo[camera_idx],misc, callback, ctx);
      }
      catch (const std::exception &ex)
      {
      }
      return NULL;
    }

#endif


    if(camBasicInfo[camera_idx].driver_name==CameraLayer_BMP_carousel::getDriverName())
    {
      return new CameraLayer_BMP_carousel(camBasicInfo[camera_idx],misc,callback, ctx);
    }

    return NULL;
  }

  //driver_name can be "" empty string to skip driver info locating
  CameraLayer* CameraLayerManager::connectCamera(std::string driver_name,std::string cam_id,std::string misc,CameraLayer::CameraLayer_Callback callback, void* ctx)
  {
    for(int i=0;i<camBasicInfo.size();i++)
    {
      //LOGI("driver_name:%s id:%s",camBasicInfo[i].driver_name.c_str(),camBasicInfo[i].id.c_str());
      if((  driver_name.length()==0 ||camBasicInfo[i].driver_name==driver_name) && camBasicInfo[i].id==cam_id)
      {
        return connectCamera(i, misc, callback, ctx);
      }
    }
    return NULL;
  }
