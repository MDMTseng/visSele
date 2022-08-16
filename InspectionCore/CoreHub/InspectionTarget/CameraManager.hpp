#pragma once


#include "CameraLayerManager.hpp"

class CameraManager
{

       

  inline static CameraLayerManager clm;
  public:
  struct StreamingInfo
  {
    CameraLayer* camera;
    int channel_id;
    int downsample;
  };

  protected:
  StreamingInfo* addCamera(CameraLayer *cam);

  public:

  std::vector<StreamingInfo> camera_streaming_infos;

  StreamingInfo* addCamera(int idx,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx);
  StreamingInfo* addCamera(std::string driverName,std::string camera_id,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx);
  
  StreamingInfo* getCamera(std::string driverName,std::string camera_id);

  // StreamingInfo* getCameraStreamingInfo(std::string driverName,std::string camera_id);
  // StreamingInfo* getCameraStreamingInfo(CameraLayer* camera);


  bool delCamera(int idx);
  bool delCamera(std::string driverName,std::string camera_id);
  
  int findConnectedCameraIdx(std::string driverName,std::string camera_id);
  public:
  static std::string cameraDiscovery(bool doDiscover=true);
  
  cJSON* ConnectedCameraList();
  const vector<CameraManager::StreamingInfo> &ConnectedCamera_ex();
  static cJSON* cameraInfo2Json(CameraLayer::BasicCameraInfo &info);
  // InspectionTarget* AddInspTargetWithCamera(int idx,std::string misc_str);
  


  ~CameraManager();
};

