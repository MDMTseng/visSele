
#include "InspectionTarget.hpp"

#include "logctrl.h"
#include "common_lib.h"
#include <future>
using namespace std;

cJSON* CameraManager::cameraInfo2Json(CameraLayer::BasicCameraInfo &info)
{
  std::string jsinstr=clm.CamInfo2Json(info);
  return cJSON_Parse(jsinstr.c_str());
}

InspectionTarget::InspectionTarget(std::string id,cJSON* def,InspectionTargetManager* belongMan)
{
  this->inputPoolInsufficient=false;
  this->def=NULL;
  this->id=id;
  this->addtionalInfo=NULL;
  this->belongMan=belongMan;
  
  setInspDef(def);
}




void InspectionTarget::acceptStageInfo(StageInfo* sinfo)
{
  sinfo->registerInUse(this);

  {
    const std::lock_guard<std::mutex> lock(this->input_stage_lock);
    // LOGI("accept StageInfo: src:%s",sinfo->source.c_str());
    inputPoolInsufficient=false;
    input_stage.push_back(sinfo);
  }
}



int InspectionTarget::reutrnStageInfo(StageInfo* sinfo)
{
  LOGI("getUseCount:%d  :%p",sinfo->getUseCount(),sinfo);

  belongMan->unregNrecycleStageInfo(sinfo,this);
}


bool InspectionTarget::feedStageInfo(StageInfo* sinfo)
{
  if(stageInfoFilter(sinfo))
  {
    acceptStageInfo(sinfo);
    return true;
  }
  return false;
}




int InspectionTarget::processInputStagePool()
{
  if(input_stage.size())
  {
    input_stage_lock.lock();

    for(int i=0;i<input_stage.size();i++)
    {
      input_pool.push_back(input_stage[i]);
    }
    input_stage.clear();
    input_stage_lock.unlock();
    inputPoolInsufficient=false;
  }
  else
  {
    if(inputPoolInsufficient)return 0;
  }

  int processCount = processInputPool();
  
  inputPoolInsufficient=(processCount==0);

  return processCount;

}


void InspectionTarget::setInspDef(cJSON* def)
{

  if(this->def)cJSON_Delete(this->def);
  this->def=NULL;
  // this->depSrc.clear();
  if(def)
  {
    id=JFetch_STRING_ex(def,"id","");
    name=JFetch_STRING_ex(def,"name","");
    type=JFetch_STRING_ex(def,"type","");
    this->def= cJSON_Duplicate(def, cJSON_True);
    // for(int i=0;;i++)
    // {
    //   std::string path("depSrc[");
    //   path=path+to_string(i)+"]";
    //   char* dsrc=JFetch_STRING(def,path.c_str());
    //   if(dsrc==NULL)break;
    //   this->depSrc.push_back(std::string(dsrc));
    // }
  }
}

// void InspectionTarget::inputPick(StageInfo* pool,int poolL,std::vector<StageInfo*> ret_pick)
// {
//   ret_pick.clear();
//   for(int i=0;i<poolL;i++)
//   {
//     if(pool[i]!=NULL)
//     {
//       ret_pick.push_back(pool[i]);
//       pool[i]=NULL;
//       return;
//     }
//   }
//   return;
// }

cJSON* InspectionTarget::genITInfo()
{
  cJSON* info= genITInfo_basic();
  
  {
    cJSON_AddItemToObject(info, "io",genITIOInfo() );
  }
  return info;
}

cJSON* InspectionTarget::genITInfo_basic()
{
  cJSON *obj=cJSON_CreateObject();

  // {
  //   cJSON *otherInfo=cJSON_CreateObject();
  //   cJSON_AddItemToObject(obj, "inspInfo", otherInfo);
  // }

  {
    // cJSON_AddNumberToObject(obj, "channel_id", channel_id);
    cJSON_AddStringToObject(obj, "id", id.c_str());
    cJSON_AddStringToObject(obj, "type",this->type.c_str());
    cJSON_AddStringToObject(obj, "name",this->name.c_str());
  }
  return obj;
}



cJSON* InspectionTarget::exchangeInfo(cJSON* info)
{

  if(this->addtionalInfo)cJSON_Delete(this->addtionalInfo);
  this->addtionalInfo=NULL;
  if(info)
  {
    this->addtionalInfo= cJSON_Duplicate(info, cJSON_True);
  }

  return NULL;
}

bool  InspectionTarget::isService()
{
  return asService;
}
// cJSON* InspectionTarget::getInspResult()
// {
//   return getInfo_cJSON();
// }


InspectionTarget::~InspectionTarget()
{
  setInspDef(NULL);
  exchangeInfo(NULL);

  for(int i=0;i<input_stage.size();i++)
  {
    reutrnStageInfo(input_stage[i]);
  }
  input_stage.clear();

  for(int i=0;i<input_pool.size();i++)
  {
    reutrnStageInfo(input_pool[i]);
  }
  input_pool.clear();

}



string CameraManager::cameraDiscovery(bool doDiscover)
{
  if(doDiscover)
  {
    clm.discover();
  }
  return clm.genJsonStringList();
}

CameraManager::StreamingInfo* CameraManager::addCamera(CameraLayer *cam)
{
  if(cam==NULL)
  {
    return NULL;
  }

  CameraManager::StreamingInfo info;


  info.channel_id=-1;
  info.camera=cam;//init data


  camera_streaming_infos.push_back(info);



  return &camera_streaming_infos[camera_streaming_infos.size()-1];
}



CameraManager::StreamingInfo* CameraManager::addCamera(int idx,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx)
{
  CameraLayer::BasicCameraInfo bcaminfo = clm.camBasicInfo[idx];
  if(findConnectedCameraIdx(bcaminfo.driver_name,bcaminfo.id)>=0)
  {
    return NULL;
  }



  CameraLayer *cam= clm.connectCamera(idx,misc_str,callback,ctx);

  return addCamera(cam);
}


CameraManager::StreamingInfo* CameraManager::addCamera(std::string driverName,std::string camera_id,std::string misc_str,CameraLayer::CameraLayer_Callback callback, void *ctx)
{
  // CameraLayer::BasicCameraInfo bcaminfo = clm.camBasicInfo[idx];
  {
    int idx=findConnectedCameraIdx( driverName, camera_id);
    if(idx>=0)
    {
      return NULL;
    }
  }

  CameraLayer *cam= clm.connectCamera(driverName,camera_id,misc_str,callback,ctx);

  return addCamera(cam);
}

CameraManager::StreamingInfo* CameraManager::getCamera(std::string driverName,std::string camera_id)
{
  int idx=findConnectedCameraIdx( driverName, camera_id);
  if(idx>=0)
  {
    return &camera_streaming_infos[idx];
  }
  return NULL;
}


int CameraManager::findConnectedCameraIdx(std::string driverName,std::string camera_id)
{
  int i=0;
  for( i=0;i<camera_streaming_infos.size();i++)
  {
    CameraLayer::BasicCameraInfo data=camera_streaming_infos[i].camera->getConnectionData();
    if( (driverName.length()==0 || data.driver_name==driverName) && data.id==camera_id)
    {
      return i;
    }
  }
  return -1;
}

template <class T>
static bool delMoveCameraInfo(vector<T> arr,int delIdx)
{
  if(delIdx<0||delIdx>=arr.size()-1)return false;
  if(delIdx==arr.size()-1)return true;//if it's the last element no need to swap location

  T tmp=arr[delIdx];

  arr.erase(arr.begin()+delIdx);
  arr.push_back(tmp);
  return true;
}


bool CameraManager::delCamera(int idx)
{
  if(idx<0||idx>=camera_streaming_infos.size())return false;

  CameraLayer *ispt=camera_streaming_infos[idx].camera;
  delete ispt;
  camera_streaming_infos[idx].camera=NULL;
  camera_streaming_infos.erase(camera_streaming_infos.begin()+idx);

  return true;
}


bool CameraManager::delCamera(std::string driverName,std::string camera_id)
{

  int idx=findConnectedCameraIdx( driverName, camera_id);
  if(idx>=0)
  {
    return delCamera(idx);
  }
  return false;
}


cJSON* CameraManager::ConnectedCameraList()
{

  cJSON* jarr=cJSON_CreateArray();
  for(int i=0;i<camera_streaming_infos.size();i++)
  {
    CameraLayer::BasicCameraInfo data=camera_streaming_infos[i].camera->getConnectionData();
    cJSON* caminfo=cameraInfo2Json(data);
    
    cJSON_AddNumberToObject(caminfo, "channel_id", camera_streaming_infos[i].channel_id);
    cJSON_AddItemToArray(jarr, caminfo );
  }

  return jarr;
}

const vector<CameraManager::StreamingInfo> & CameraManager::ConnectedCamera_ex()
{
  return camera_streaming_infos;
}


CameraManager::~CameraManager()
{
  for(int i=0;i<camera_streaming_infos.size();i++)
  {
    delete camera_streaming_infos[i].camera;
    camera_streaming_infos[i].camera=NULL;
  }
  camera_streaming_infos.resize(0);
}


int InspectionTargetManager::getInspTarIdx(std::string id)
{
  for(int i=0;i<inspTars.size();i++)
  {
    if(inspTars[i]->id==id)
    {
      return i;
    }
  }
  return -1;
}

bool InspectionTargetManager::addInspTar(InspectionTarget* inspTar,std::string id)
{
  
  int idx = getInspTarIdx(id);
  if(idx>=0)
  {
    return false;
  }
  inspTars.push_back(inspTar);
  return true;
}


bool InspectionTargetManager::delInspTar(std::string id)
{

  int idx = getInspTarIdx(id);
  if(idx<0)
  {
    return false;
  }


  InspectionTarget *ispt=inspTars[idx];
  delete ispt;
  inspTars[idx]=NULL;
  inspTars.erase(inspTars.begin()+idx);
  return true;
}
bool InspectionTargetManager::clearInspTar(bool rmService)
{
  
  for(int i=0;i<inspTars.size();i++)
  {
    printf("i:%d  =>%p :%s   \n ",i,inspTars[i],inspTars[i]->id.c_str());
    delete inspTars[i];
    printf("-:%d  =>%p   \n ",i,inspTars[i]);
    inspTars[i]=NULL;
  }

  int remainCount=0;
  for(int i=0;i<inspTars.size();i++)
  {
    if( inspTars[i])
    {
      inspTars[remainCount]= inspTars[i];
      remainCount++;
    }
  }
  inspTars.resize(remainCount);
  return true;
}

InspectionTarget* InspectionTargetManager::getInspTar(std::string id)
{
  
  int idx = getInspTarIdx(id);
  if(idx<0)
  {
    return NULL;
  }
  return inspTars[idx];

}



int InspectionTargetManager::dispatch(StageInfo* sinfo)
{
  if(sinfo==NULL)return -1;
  int acceptCount=0;
  for(int i=0;i<inspTars.size();i++)
  {
    if(inspTars[i]->feedStageInfo(sinfo)==true)
    {
      acceptCount++;
    }
  }
  if(acceptCount==0)
  {
    
    LOGI("id:%d trigger:",sinfo->trigger_id);
    for(auto tag:sinfo->trigger_tags)
      LOGI("%s",tag.c_str());
    LOGE("No one accepts StageInfo: from:%s type:%s ",sinfo->source_id.c_str(),sinfo->typeName().c_str());
    LOGE("Recycle.... ");
    unregNrecycleStageInfo(sinfo,NULL);
  }
  else
  {
    SInfoInSysCount++;
    
    // LOGE("++>>>>SInfoInSysCount:%d ",SInfoInSysCount);
  }
  
  return acceptCount;
}




int InspectionTargetManager::unregNrecycleStageInfo(StageInfo* sinfo,InspectionTarget* from )
{
  std::lock_guard<std::mutex> _(recycleStageInfoMutex);

  return _unregNrecycleStageInfo(sinfo,from);
}
int InspectionTargetManager::_unregNrecycleStageInfo(StageInfo* sinfo,InspectionTarget* from )
{

  if(sinfo==NULL)return -1;
  if(from)
    sinfo->unregisterInUse(from);
  // LOGE("recycle: src:%s  inUseCount:%d",sinfo->source.c_str(),sinfo->inUseCount);
  if(sinfo->isStillInUse())
  {
    return sinfo->getUseCount();
  }

  for(auto sharedInfo :sinfo->sharedInfo )
  {
    if(sharedInfo)
      _unregNrecycleStageInfo(sharedInfo,from);
  }
  sinfo->sharedInfo.clear();//do clear the vector, or the StageInfo destructor will delete sharedInfo again

  delete sinfo;
  return 0;
}

int InspectionTargetManager::inspTarProcess()
{
  int totalProcessCount=0;
  while(1)
  {

    std::vector<std::future<int>> fut_vec;
    int curProcessCount=0;
    for (int i = 0; i < inspTars.size(); i++)
      fut_vec.push_back(inspTars[i]->futureInputStagePool());

    for (int i = 0; i < fut_vec.size(); ++i)
      curProcessCount += fut_vec[i].get();

    LOGI(">>>curProcessCount:%d",curProcessCount);
    if(curProcessCount==0)
    {
      break;
    }
    totalProcessCount+=curProcessCount;
  }




  return totalProcessCount;
}




cJSON* InspectionTargetManager::genInspTarListInfo()
{
  
  cJSON* jarr=cJSON_CreateArray();
  for(int i=0;i<inspTars.size();i++)
  {
    cJSON_AddItemToArray(jarr, inspTars[i]->genITInfo());
  }

  return jarr;
}





CameraLayer::status InspectionTargetManager::sCAM_CallBack(CameraLayer &cl_obj, int type, void *context)
{
  if(context==NULL)return CameraLayer::status::NAK;
  InspectionTargetManager* itman = (InspectionTargetManager*)context;
  return itman->CAM_CallBack(cl_obj,type,context);
}

CameraLayer::status InspectionTargetManager::CAM_CallBack(CameraLayer &cl_obj, int type, void *context)
{
  if (type != CameraLayer::EV_IMG)return CameraLayer::status::NAK;
  std::lock_guard<std::mutex> _(camCBLock);

  CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
  for(int i=0;i<camman.camera_streaming_infos.size();i++)
  {
    if(camman.camera_streaming_infos[i].camera==(&cl_obj))
    {
      



      // acvImage bufImg;
      // // CameraStreamingInfo *streamInfo=NULL;
      // // acvImage bufImg= camman.camera_streaming_infos[i].buffImage;

      // // // LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
      // bufImg.ReSize(finfo.width,finfo.height,3);
      

      // CameraLayer::status st = cl_obj.ExtractFrame(bufImg.CVector[0],3,finfo.width*finfo.height);


      CamStream_CallBack(camman.camera_streaming_infos[i]);

      return CameraLayer::status::ACK;
    }
  }
  // printf(">>>>>>>>>\n");
  return CameraLayer::status::NAK;
}


// void InspectionTargetManager::inspTargetProcess(image_pipe_info &info)
// {
//   std::string camera_id=info.camera_id;

//   // cJSON* reportInfo=cJSON_CreateObject();
//   // cJSON_AddStringToObject(reportInfo, "trigger_tag", info.trigger_tag.c_str());
//   // cJSON_AddStringToObject(reportInfo, "camera_id", camera_id.c_str());
//   cJSON* reports=cJSON_CreateArray();
//   // cJSON_AddItemToObject(reportInfo, "reports", reports);
//   for(int i=0;i<inspTars.size();i++)
//   {
//     inspTars[i]->CAM_CallBack(&info);
//       // &(info.StreamInfo),
//       // info.img, camera_id, info.trigger_tag,info.trigger_id);//no trigger id yet
//     cJSON_AddItemToArray(reports, cJSON_Duplicate(inspTars[i]->fetchInspReport(),cJSON_True ));
//   }
//   info.report_json=reports;//collect
//   // InspResult_CallBack(info);//TODO: intent code fix this 
// }