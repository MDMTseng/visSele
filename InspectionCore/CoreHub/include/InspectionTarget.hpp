#pragma once


#include "CameraLayerManager.hpp"
#include "vector"
#include "map"
#include "cJSON.h"
#include "acvImage.hpp"
#include "TSQueue.hpp"
#include <future>



typedef struct BPG_protocol_data_acvImage_Send_info
{
    acvImage* img;
    uint16_t scale;
    uint16_t offsetX,offsetY;
    uint16_t fullWidth,fullHeight;

}BPG_protocol_data_acvImage_Send_info;


       
// struct InspectionTarget_EXCHANGE
// {
//   int isOK;
//   cJSON *info;
//   BPG_protocol_data_acvImage_Send_info imgInfo;
// };




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
  static std::string cameraDiscovery(bool doDiscover=true);
  
  cJSON* ConnectedCameraList();
  const vector<CameraManager::StreamingInfo> &ConnectedCamera_ex();
  static cJSON* cameraInfo2Json(CameraLayer::BasicCameraInfo &info);
  // InspectionTarget* AddInspTargetWithCamera(int idx,std::string misc_str);
  


  ~CameraManager();
};


// typedef struct image_pipe_info
// {
//   CameraManager::StreamingInfo StreamInfo;
//   CameraLayer::frameInfo fi;
  
//   std::string camera_id;
//   std::string trigger_tag;
//   int trigger_id;
//   // FeatureManager_BacPac *bacpac;
//   acvImage img;

//   cJSON *report_json;
// } image_pipe_info;


// struct CameraStreamingInfo
// {
//   int channel_id;
// };



class StageInfo{
  public:
  std::string type;
  std::string source;
  
  CameraManager::StreamingInfo StreamInfo;
  CameraLayer::frameInfo fi;
  

  std::vector<std::string> trigger_tags;
  int trigger_id;
  
  std::map<std::string,acvImage*> imgSets;
  cJSON *jInfo;


  std::mutex lock;
  int inUseCount;

  ~StageInfo(){
    const std::lock_guard<std::mutex> lock(this->lock);
    if(inUseCount)
    {
      throw std::string("inUseCount is not Zero:")+to_string(inUseCount);
    }

    for(auto iter =imgSets.begin(); iter != imgSets.end(); ++iter)
    {
      auto k =  iter->first;
      if(iter->second!=NULL)
      {
        delete iter->second;
        iter->second=NULL;
      }
    }
    imgSets.clear();

    if(jInfo)
    {
      cJSON_Delete(jInfo);
      jInfo=NULL;
    }
  }
  
};

//custom Stage info
class StageInfo_cusTest:public StageInfo{
  char* charArr=NULL;
  StageInfo_cusTest()
  {
    type="StageInfo_cusTest";
  }
  ~StageInfo_cusTest()
  {
    
    if(charArr)
    {
      delete charArr;
      charArr=NULL;
    }
    //this destructor will be called first,
    //after this ~StageInfo will be called in C++ 
  }
  
};

class InspectionTargetManager
{
  std::mutex camCBLock;
  public:


  int SInfoInSysCount=0;

  CameraManager camman;

  std::vector<InspectionTarget*> inspTars;

  static CameraLayer::status sCAM_CallBack(CameraLayer &cl_obj, int type, void *context);
  CameraLayer::status CAM_CallBack(CameraLayer &cl_obj, int type, void *context);


  // virtual image_pipe_info* RequestPipeInfoResource_CallBack()=0; //override this to ask resource into Q
  // virtual void CamStream_CallBack(image_pipe_info &info)=0;//override this store the info into Q

  virtual void CamStream_CallBack(CameraManager::StreamingInfo &info)=0;


  int dispatch(StageInfo* sinfo);//return how many inspection target needs it
  int recycleStageInfo(StageInfo* sinfo);//return 0 as destroy, other positive number means how many other inspTar still holds it 

  int inspTarProcess();



  int getInspTarIdx(std::string id);

  bool delInspTar(std::string id);
  bool clearInspTar( bool rmService=false );
 
  
  bool addInspTar(InspectionTarget* inspTar,std::string id);

  InspectionTarget* getInspTar(std::string id);
  
  cJSON* genInspTarListInfo();
};


class InspectionTarget
{
  protected:
  bool asService=false;
  public:
  std::string id;
  bool inputPoolInsufficient;
  cJSON *def=NULL;
  cJSON *addtionalInfo=NULL;
  InspectionTargetManager* belongMan;
  InspectionTarget(std::string id,InspectionTargetManager* belongMan);
  virtual ~InspectionTarget();
  virtual void setInspDef(cJSON* def);

  virtual void setAddtionalInfo(cJSON* info);

  virtual cJSON* genInfo();
  bool isService();

  
  std::mutex input_stage_lock;
  std::vector<StageInfo*> input_stage;
  std::vector<StageInfo*> input_pool;
  
  virtual bool feedStageInfo(StageInfo* sinfo);


  virtual int processInputStagePool();  
  virtual std::future<int> futureInputStagePool()=0;
  protected:
  
  virtual bool stageInfoFilter(StageInfo* sinfo)=0;
  // virtual std::vector<StageInfo*> inputPick(std::vector<StageInfo*> infoPool)=0;//returns input processed
  virtual void acceptStageInfo(StageInfo* sinfo);
  int reutrnStageInfo(StageInfo* sinfo);

  virtual int processInputPool()=0; 
};



/*
  
  =========DISPATCH for each inspTar=======
  dispatch StageInfo "sinfo"
    |
    V
  inspTar.feedStageInfo(sinfo)
  {
    if( inspTar.stageInfoFilter(sinfo))
    {
      inspTar.acceptStageInfo(sinfo){
        input_stage.lock()
        inspTar.input_stage.push(sinfo)
        input_stage.unlock()
      }
    }
  }
  ================

  =========Run section=======
  input_stage.lock()
  input=[...input,...input_stage ]
  input_stage=[]
  input_stage.unlock()

  while( inputPack = inputPick(input) )
  {
    Do_Some_Work(inputPack)
  }

  input=removeUsedInfo(input)


  ================







*/

