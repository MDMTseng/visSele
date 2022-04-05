#pragma once


#include "CameraLayerManager.hpp"
#include "vector"
#include "cJSON.h"
#include "acvImage.hpp"
#include "TSQueue.hpp"



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
  struct StreamingInfo
  {
    CameraLayer* camera;
    int channel_id;
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
  static std::string cameraDiscovery();
  
  cJSON* ConnectedCameraList();
  static cJSON* cameraInfo2Json(CameraLayer::BasicCameraInfo &info);
  // InspectionTarget* AddInspTargetWithCamera(int idx,std::string misc_str);
  


  ~CameraManager();
};


typedef struct image_pipe_info
{
  CameraManager::StreamingInfo StreamInfo;
  CameraLayer::frameInfo fi;
  std::string trigger_tag;
  int trigger_id;
  // FeatureManager_BacPac *bacpac;
  acvImage img;

  cJSON *report_json;
} image_pipe_info;

// struct CameraStreamingInfo
// {
//   int channel_id;
// };

class InspectionTargetManager
{
  std::mutex camCBLock;
  public:




  CameraManager camman;

  std::vector<InspectionTarget*> inspTars;

  static CameraLayer::status sCAM_CallBack(CameraLayer &cl_obj, int type, void *context);
  CameraLayer::status CAM_CallBack(CameraLayer &cl_obj, int type, void *context);


  // virtual image_pipe_info* RequestPipeInfoResource_CallBack()=0; //override this to ask resource into Q
  // virtual void CamStream_CallBack(image_pipe_info &info)=0;//override this store the info into Q

  virtual void CamStream_CallBack(CameraManager::StreamingInfo &info)=0;


  void inspTargetProcess(image_pipe_info &info);




  int getInspTarIdx(std::string id);

  bool delInspTar(std::string id);
  bool clearInspTar();
  
  bool addInspTar(InspectionTarget* inspTar,std::string id);

  InspectionTarget* getInspTar(std::string id);
  
  cJSON* genInspTarListInfo();
};




class InspectionTarget
{
  public:
  int channel_id;
  std::string id;
  
  cJSON *def=NULL;

  InspectionTarget(std::string id);
  virtual void setInspDef(const cJSON* def);
  cJSON* genInfo();

  acvImage img_buff;
  virtual InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)=0;
  virtual cJSON* fetchInspReport()=0;
  // virtual cJSON* genInspReport()=0;
  bool returnExchange(InspectionTarget_EXCHANGE* info);
  virtual void CAM_CallBack(CameraManager::StreamingInfo *srcCamSi,acvImage &img,std::string cam_id,std::string trigger_tag,int trigger_id)=0;
  // {
  //   CameraLayer::frameInfo  info=srcCamSi->camera->GetFrameInfo();
  //   printf("<<<<id:%s<<<%s  WH:%d,%d  timeStamp_us:%d\n",id.c_str(),cam_id.c_str(),img.GetWidth(),img.GetHeight(),info.timeStamp_us);
  // }

  virtual ~InspectionTarget();
};