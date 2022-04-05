
#include "InspectionTarget.hpp"

#include "common_lib.h"
using namespace std;

cJSON* CameraManager::cameraInfo2Json(CameraLayer::BasicCameraInfo &info)
{
  std::string jsinstr=clm.CamInfo2Json(info);
  return cJSON_Parse(jsinstr.c_str());
}


InspectionTarget::InspectionTarget(std::string id)
{
  
  this->def=NULL;
  this->id=id;
}


void InspectionTarget::setInspDef(const cJSON* def)
{
  if(this->def)cJSON_Delete(this->def);
  this->def=NULL;
  if(def)
    this->def= cJSON_Duplicate(def, cJSON_True);
}


cJSON* InspectionTarget::genInfo()
{
  cJSON *obj=cJSON_CreateObject();

  {
    // cJSON *camInfo = cJSON_Parse(camera->getCameraJsonInfo().c_str());
    // cJSON_AddItemToObject(obj, "camera", camInfo);
  }

  {
    cJSON *otherInfo=cJSON_CreateObject();
    cJSON_AddItemToObject(obj, "inspInfo", otherInfo);
  }

  {
    cJSON_AddNumberToObject(obj, "channel_id", channel_id);
    cJSON_AddStringToObject(obj, "id", id.c_str());
  }
  return obj;
}
// cJSON* InspectionTarget::getInspResult()
// {
//   return getInfo_cJSON();
// }


InspectionTarget::~InspectionTarget()
{
  setInspDef(NULL);
}

string CameraManager::cameraDiscovery()
{
  clm.discover();
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
bool InspectionTargetManager::clearInspTar()
{

  for(int i=0;i<inspTars.size();i++)
  {
    printf("i:%d  =>%p   \n ",i,inspTars[i]);
    delete inspTars[i];
    printf("i:%d  =>%p   \n ",i,inspTars[i]);
    inspTars[i]=NULL;
  }
  inspTars.clear();
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


cJSON* InspectionTargetManager::genInspTarListInfo()
{
  
  cJSON* jarr=cJSON_CreateArray();
  for(int i=0;i<inspTars.size();i++)
  {
    cJSON_AddItemToArray(jarr, inspTars[i]->genInfo());
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


void InspectionTargetManager::inspTargetProcess(image_pipe_info &info)
{
  std::string camera_id=info.StreamInfo.camera->getConnectionData().id;

  // cJSON* reportInfo=cJSON_CreateObject();
  // cJSON_AddStringToObject(reportInfo, "trigger_tag", info.trigger_tag.c_str());
  // cJSON_AddStringToObject(reportInfo, "camera_id", camera_id.c_str());
  cJSON* reports=cJSON_CreateArray();
  // cJSON_AddItemToObject(reportInfo, "reports", reports);
  for(int i=0;i<inspTars.size();i++)
  {
    inspTars[i]->CAM_CallBack(
      &(info.StreamInfo),
      info.img, camera_id, info.trigger_tag,info.trigger_id);//no trigger id yet
    cJSON_AddItemToArray(reports, cJSON_Duplicate(inspTars[i]->fetchInspReport(),cJSON_True ));
  }
  info.report_json=reports;//collect
  // InspResult_CallBack(info);//TODO: intent code fix this 
}