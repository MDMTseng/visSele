#pragma once


#include "CameraLayerManager.hpp"
#include "vector"
#include "cJSON.h"
#include "acvImage.hpp"



typedef struct BPG_protocol_data_acvImage_Send_info
{
    acvImage* img;
    uint16_t scale;
    uint16_t offsetX,offsetY;
    uint16_t fullWidth,fullHeight;

}BPG_protocol_data_acvImage_Send_info;


       
struct InspectionTarget_EXCHANGE
{
  int isOK;
  cJSON *info;
  BPG_protocol_data_acvImage_Send_info imgInfo;
};

class InspectionTarget;
class CameraManager
{
  std::vector<CameraLayer*> cameras;

  inline static CameraLayerManager clm;
  public:
  CameraLayer* addCamera(int idx,std::string misc_str);
  CameraLayer* addCamera(std::string driverName,std::string camera_id,std::string misc_str);
  CameraLayer* getCamera(std::string driverName,std::string camera_id);
  bool delCamera(int idx);
  bool delCamera(std::string driverName,std::string camera_id);
  
  int findConnectedCameraIdx(std::string driverName,std::string camera_id);
  public:
  static std::string cameraDiscovery();
  
  cJSON* ConnectedCameraList();
  static cJSON* cameraInfo2Json(CameraLayer::BasicCameraInfo &info);
  // InspectionTarget* AddInspTargetWithCamera(int idx,std::string misc_str);
  


  ~CameraManager();
};



class InspectionTargetManager
{
  public:
  CameraManager camman;

  std::vector<InspectionTarget*> inspTar;


  int getInspTarIdx(std::string id);

  InspectionTarget* addInspTar(std::string id);


  bool delInspTar(std::string id);

  InspectionTarget* getInspTar(std::string id);
  
  cJSON* getInspTarListInfo();
};




class InspectionTarget
{
  struct cameraTargetInfo{
    std::string cam_id;
    std::string trigger_id;
  };
  std::vector<cameraTargetInfo> camRecvInfo;
  cJSON *InspConf=NULL;
  std::timed_mutex rsclock;
  public:
  int channel_id;
  std::string id;
  
  bool registerCamRecvInfo(std::string cam_id,std::string trigger_id)
  {
    cameraTargetInfo cTI;
    cTI.cam_id=cam_id;
    cTI.trigger_id=trigger_id;
    camRecvInfo.push_back(cTI);
    return true;
  }

  InspectionTarget(std::string id);
  void setInspDef(cJSON* json);
  cJSON* getInfo_cJSON();

  InspectionTarget_EXCHANGE excdata={0};
  acvImage img_buff;
  InspectionTarget_EXCHANGE* setInfo(InspectionTarget_EXCHANGE* info);

  bool returnExchange(InspectionTarget_EXCHANGE* info);

  void CAM_CallBack(acvImage &img,CameraLayer& src);

  ~InspectionTarget();
};