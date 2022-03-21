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

  inline static CameraLayerManager clm;
  public:
  std::vector<CameraLayer*> cameras;
  std::vector<acvImage*> buffImage;
  CameraLayer* addCamera(int idx,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx);
  CameraLayer* addCamera(std::string driverName,std::string camera_id,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx);
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
  std::mutex camCBLock;
  public:
  CameraManager camman;

  std::vector<InspectionTarget*> inspTar;

  static CameraLayer::status sCAM_CallBack(CameraLayer &cl_obj, int type, void *context);
  CameraLayer::status CAM_CallBack(CameraLayer &cl_obj, int type, void *context);


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

  bool matchCamRecvInfo(std::string cam_id,std::string trigger_id)
  {
    for(int i=0;i<camRecvInfo.size();i++)
    {
      if(camRecvInfo[i].cam_id==cam_id && (trigger_id.length()==0 || trigger_id==camRecvInfo[i].trigger_id))
      {
        return true;
      }
    }
    return false;
  }


  InspectionTarget(std::string id);
  void setInspDef(cJSON* json);
  cJSON* getInfo_cJSON();

  InspectionTarget_EXCHANGE excdata={0};
  acvImage img_buff;
  InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info);

  bool returnExchange(InspectionTarget_EXCHANGE* info);

  void CAM_CallBack(CameraLayer& srcCam,acvImage &img,std::string cam_id,std::string trigger_id)
  {
    printf("<<<<id:%s<<<%s  WH:%d,%d\n",id.c_str(),cam_id.c_str(),img.GetWidth(),img.GetHeight());
  }

  ~InspectionTarget();
};