
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
  this->id=id;
  
  // this->camera=camera;
  // InspConf=inspectionConfig;
}



void InspectionTarget::setInspDef(cJSON* json)
{
}

cJSON* InspectionTarget::getInfo_cJSON()
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
  }
  return obj;
}


InspectionTarget_EXCHANGE* InspectionTarget::setInfo(InspectionTarget_EXCHANGE* info)
{
  rsclock.lock();
  cJSON * json=info->info;

  char *insp_type = JFetch_STRING(json, "insp_type");

  memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));


  // if(strcmp(insp_type, "snap") ==0)
  // {
  //   LOGI(">>>>>");
  //   int ret= getImage(camera,&img,0,-1);
  //   if(ret!=0)
  //   {
  //     return NULL;
  //   }
  //   LOGI(">>>>>");
  //   BPG_protocol_data_acvImage_Send_info imgInfo;
  //   imgInfo.img=&img;
  //   imgInfo.scale=1;
  //   imgInfo.offsetX=0;
  //   imgInfo.offsetY=0;
  //   imgInfo.fullHeight=img.GetHeight();
  //   imgInfo.fullWidth=img.GetWidth();


  //   excdata.imgInfo=imgInfo;
  //   excdata.isOK=true;


  //   LOGI(">>>>>");
  //   return &excdata;
  // }

  if(strcmp(insp_type, "start_stream") ==0)
  {
    // camera->TriggerMode(0);
    excdata.isOK=true;
    return &excdata;
  }
  if(strcmp(insp_type, "stop_stream") ==0)
  {
    // camera->TriggerMode(2);
    excdata.isOK=true;
    return &excdata;
  }
  excdata.isOK=false;
  return &excdata;

}

bool InspectionTarget::returnExchange(InspectionTarget_EXCHANGE* info)
{
  if(info!=&excdata)return false;

  if(info->info)
    cJSON_Delete( info->info );
  
  memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));
  
  rsclock.unlock();
  return true;
}


void InspectionTarget::CAM_CallBack(acvImage &img,CameraLayer& src)
{
  // if (type != CameraLayer::EV_IMG)
  //   return;

  // CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
  // LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
  // img.ReSize(finfo.width,finfo.height,3);

  // CameraLayer::status st = cl_obj.ExtractFrame(img.CVector[0],3,finfo.width*finfo.height);

  // BPG_protocol_data_acvImage_Send_info imgInfo=ImageDownSampling_Info(img_buff,img,4,NULL,1,0,0,-1,-1);

  // bpg_pi.fromUpperLayer_DATA("IM",channel_id,&imgInfo);
  // bpg_pi.fromUpperLayer_SS(channel_id,true,NULL,NULL);    
}

InspectionTarget::~InspectionTarget()
{
  if(InspConf)
    cJSON_Delete(InspConf);
  InspConf=NULL;

}

string CameraManager::cameraDiscovery()
{
  clm.discover();
  return clm.genJsonStringList();
}




static void _CAM_CallBack(acvImage &img,CameraLayer& src)
{
  // if (type != CameraLayer::EV_IMG)
  //   return;

  // CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
  // LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
  // img.ReSize(finfo.width,finfo.height,3);

  // CameraLayer::status st = cl_obj.ExtractFrame(img.CVector[0],3,finfo.width*finfo.height);

  // BPG_protocol_data_acvImage_Send_info imgInfo=ImageDownSampling_Info(img_buff,img,4,NULL,1,0,0,-1,-1);

  // bpg_pi.fromUpperLayer_DATA("IM",channel_id,&imgInfo);
  // bpg_pi.fromUpperLayer_SS(channel_id,true,NULL,NULL);    
}
static CameraLayer::status sCAM_CallBack(CameraLayer &cl_obj, int type, void *context)
{
  // InspectionTarget *itm=(InspectionTarget*)context;
  // itm->CAM_CallBack(cl_obj,type);






  return CameraLayer::status::ACK;
}

CameraLayer* CameraManager::addCamera(int idx,std::string misc_str)
{
  CameraLayer::BasicCameraInfo bcaminfo = clm.camBasicInfo[idx];
  if(findConnectedCameraIdx(bcaminfo.driver_name,bcaminfo.id)>=0)
  {
    return NULL;
  }



  CameraLayer *cam= clm.connectCamera(idx,misc_str,sCAM_CallBack,this);

  cameras.push_back(cam);


  return cam;
}


CameraLayer* CameraManager::addCamera(std::string driverName,std::string camera_id,std::string misc_str)
{
  // CameraLayer::BasicCameraInfo bcaminfo = clm.camBasicInfo[idx];
  {
    int idx=findConnectedCameraIdx( driverName, camera_id);
    if(idx>=0)
    {
      return NULL;
    }
  }

  CameraLayer *cam= clm.connectCamera(driverName,camera_id,misc_str,sCAM_CallBack,NULL);

  cameras.push_back(cam);


  return cam;
}

CameraLayer* CameraManager::getCamera(std::string driverName,std::string camera_id)
{
  int idx=findConnectedCameraIdx( driverName, camera_id);
  if(idx>=0)
  {
    return cameras[idx];
  }
  return NULL;
}


int CameraManager::findConnectedCameraIdx(std::string driverName,std::string camera_id)
{
  int i=0;
  for( i=0;i<cameras.size();i++)
  {
    CameraLayer::BasicCameraInfo data=cameras[i]->getConnectionData();
    if( (driverName.length()==0 || data.driver_name==driverName) && data.id==camera_id)
    {
      return i;
    }
  }
  return -1;
}


bool CameraManager::delCamera(int idx)
{
  if(idx<0||idx>=cameras.size())return false;

  CameraLayer *ispt=cameras[idx];
  delete ispt;
  cameras[idx]=NULL;
  cameras.erase(cameras.begin()+idx);
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
  for(int i=0;i<cameras.size();i++)
  {
    CameraLayer::BasicCameraInfo data=cameras[i]->getConnectionData();
    cJSON_AddItemToArray(jarr, cameraInfo2Json(data) );
  }

  return jarr;
}



CameraManager::~CameraManager()
{
  for(int i=0;i<cameras.size();i++)
  {
    delete cameras[i];
    cameras[i]=NULL;
  }
  cameras.resize(0);
}


int InspectionTargetManager::getInspTarIdx(std::string id)
{
  for(int i=0;i<inspTar.size();i++)
  {
    if(inspTar[i]->id==id)
    {
      return i;
    }
  }
  return -1;
}

InspectionTarget* InspectionTargetManager::addInspTar(std::string id)
{
  
  int idx = getInspTarIdx(id);
  if(idx>=0)
  {
    return NULL;
  }
  InspectionTarget* it=new InspectionTarget(id);
  inspTar.push_back(it);
  return it;
}


bool InspectionTargetManager::delInspTar(std::string id)
{

  int idx = getInspTarIdx(id);
  if(idx<0)
  {
    return false;
  }


  InspectionTarget *ispt=inspTar[idx];
  delete ispt;
  inspTar[idx]=NULL;
  inspTar.erase(inspTar.begin()+idx);
  return true;
}

InspectionTarget* InspectionTargetManager::getInspTar(std::string id)
{
  
  int idx = getInspTarIdx(id);
  if(idx<0)
  {
    return NULL;
  }
  return inspTar[idx];

}


cJSON* InspectionTargetManager::getInspTarListInfo()
{
  
  return NULL;//inspTar[idx];

}



