#pragma once


#include "CameraLayerManager.hpp"
#include "vector"
#include "map"
#include "cJSON.h"
#include "acvImage.hpp"
#include "TSQueue.hpp"
#include <future>
#include "logctrl.h"
#include <memory>


#include <CameraManager.hpp>
#include <StageInfo.hpp>


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
// enum StageInfo_CAT{

//   PURE_IMAGE,

//   LOCATING_INFO,
//   LOCATING_INFO_SIGNATURE,
//   LOCATING_INFO_SHAPE_MATCHING,

//   BLOB,//{area, point, region}[]
//   ARC,//{point, radious}[]
//   POINT,//point
//   LINE,//{point1, point2,sigma}[]

//   VALUE_RESULT,//{value,detail:[]}

  
// };



/*
_________________________________________
|          V                V          <| (the dispatched stageInfo will 
|          V                V          ^| do filter In each InspTar in list
|          V                V          ^| and stage in each accepted InspTar
|   ____________       ____________    ^| into input_stage)
|  |   Filter   |     |   Filter   |   ^| 
|__|____________|_____|____________|___^| --call inspTarProcess
|  |            |     |            |   ^|   will call every InspTar
|  |            |     |            |   ^|   as a thread load input_stage into input_pool
|  | InspTar A  |     | InspTar B  |   ^|   and start to process the input in input_pool
|  |____________|     |____________|   ^|   wait for all InspTar ends
|    dispatch   stageInfo[s]     >>    ^|   Then go over again
|                                       |
|InspTarManager ________________＿______| ---------^


*/

class exchangeCMD_ACT
{ 
  public:
  virtual void send(const char *TL, int pgID,cJSON* def)=0;
  virtual void send(const char *TL, int pgID,acvImage* img,int downSample)=0;

};

class InspectionTargetManager;
class StageInfo;
class InspectionTarget
{
  protected:
  bool asService=false;
  shared_ptr<StageInfo> cache_stage_info=NULL;
  public:
  std::string id;
  std::string type;
  std::string name;
  int stream_id;
  bool inputPoolInsufficient;
  cJSON *def=NULL;
  cJSON *additionalInfo=NULL;
  InspectionTargetManager* belongMan;

  std::vector< std::string > trigger_tags;
  
  static std::string TYPE(){ return "IT"; }
  // std::vector<StageInfo_CAT> acceptTags;
  InspectionTarget(std::string id,cJSON* def,InspectionTargetManager* belongMan);
  





  virtual ~InspectionTarget();
  virtual void setInspDef(cJSON* def);

  virtual bool exchangeCMD(cJSON* info,int info_ID,exchangeCMD_ACT &act);

  virtual cJSON* genITIOInfo()=0;
  virtual cJSON* genITInfo();
  bool isService();

  
  std::mutex input_stage_lock;
  std::vector<std::shared_ptr<StageInfo>> input_stage;
  std::vector<std::shared_ptr<StageInfo>> input_pool;
  
  virtual bool feedStageInfo(std::shared_ptr<StageInfo> sinfo);


  virtual int processInputStagePool();  


  virtual std::future<int> futureInputStagePool()=0;



  bool matchTriggerTag(string tarTag);
  protected:
  
  void attachSstaticInfo( cJSON* jobj,int trigger_id );
  virtual cJSON* genITInfo_basic();
  virtual bool stageInfoFilter(std::shared_ptr<StageInfo> sinfo)=0;
  // virtual std::vector<StageInfo*> inputPick(std::vector<StageInfo*> infoPool)=0;//returns input processed
  virtual void acceptStageInfo(std::shared_ptr<StageInfo> sinfo);

  void additionalInfoAssign(std::string key,cJSON* info);
  virtual int processInputPool()=0; 
};

class InspectionTarget_PureImage: public InspectionTarget
{
  InspectionTarget_PureImage(std::string id,cJSON* def,InspectionTargetManager* belongMan):InspectionTarget(id,def,belongMan)
  {

  }
};
class InspectionTarget_Locating: public InspectionTarget
{
  InspectionTarget_Locating(std::string id,cJSON* def,InspectionTargetManager* belongMan):InspectionTarget(id,def,belongMan)
  {
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


  int dispatch(std::shared_ptr<StageInfo> sinfo);//return how many inspection target needs it

  std::mutex recycleStageInfoMutex;
  // int unregNrecycleStageInfo(StageInfo* sinfo,InspectionTarget* from );//return 0 as destroy, other positive number means how many other inspTar still holds it 
  // int _unregNrecycleStageInfo(StageInfo* sinfo,InspectionTarget* from );//return 0 as destroy, other positive number means how many other inspTar still holds it 

  int inspTarProcess();

  bool isAllInspTarBufferClear();

  int getInspTarIdx(std::string id);

  bool delInspTar(std::string id);
  bool clearInspTar( bool rmService=false );
 
  
  bool addInspTar(InspectionTarget* inspTar,std::string id);

  void genInspTarInOutMap();

  InspectionTarget* getInspTar(std::string id);
  
  cJSON* genInspTarListInfo();
};



//custom Stage info
// class StageInfo_cusTest:public StageInfo{
//   char* charArr=NULL;
//   public:
//   static std::string stypeName(){return "cusTest";}
//   std::string typeName(){return StageInfo_cusTest::stypeName();}
//   StageInfo_cusTest()
//   {
//   }
//   ~StageInfo_cusTest()
//   {
    
//     if(charArr)
//     {
//       delete charArr;
//       charArr=NULL;
//     }
//     //this destructor will be called first,
//     //after this ~StageInfo will be called in C++ 
//   }
  
// };


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


