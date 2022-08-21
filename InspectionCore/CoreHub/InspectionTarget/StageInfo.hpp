#pragma once


#include "InspectionTarget.hpp"

#include <CameraManager.hpp>

class InspectionTarget;
class CameraManager;
using namespace std;


#define STAGEINFO_LIFECYCLE_DEBUG 1

static int StageInfoLiveCounter=0;

class StageInfo{
  public:
  std::string source_id;
  InspectionTarget *source;
  
  std::shared_ptr<acvImage> img;
  struct _img_prop{
    CameraLayer::frameInfo fi;
    float mmpp;
    CameraManager::StreamingInfo StreamInfo;
    
  };
  struct _img_prop img_prop;
  
  
  static std::string stypeName(){return "Base";}
  virtual std::string typeName(){return this->stypeName();}
  
  // std::vector<StageInfo_CAT> catInfo;
  std::vector<std::string> trigger_tags;
  int trigger_id;
  
  // std::map<std::string,std::shared_ptr<acvImage>> imgSets;
  cJSON* jInfo;
  
  virtual void dataSetToJInfo()
  {
    if(jInfo)
    {
      cJSON_Delete(jInfo);
      jInfo=NULL;
    }

    jInfo=cJSON_CreateObject();
    cJSON_AddStringToObject(jInfo,"InspTar_id",source_id.c_str());
    cJSON_AddStringToObject(jInfo,"InspTar_type",typeName().c_str());

  }

  std::mutex lock;

  std::vector<std::shared_ptr<StageInfo>> sharedInfo;
  StageInfo(){
    img_prop.StreamInfo=(CameraManager::StreamingInfo){0};
    img_prop.mmpp=0;
    img_prop.fi=( CameraLayer::frameInfo){0};
    StageInfoLiveCounter++;
    source=NULL;
    source_id="";
    trigger_id=-1;
    jInfo=NULL;

#if STAGEINFO_LIFECYCLE_DEBUG
    LOGE("++>StageInfoLiveCounter:%d  :%p",StageInfoLiveCounter,this);
#endif
  }
  virtual ~StageInfo(){
    // for(auto iter =imgSets.begin(); iter != imgSets.end(); ++iter)
    // {
    //   auto k =  iter->first;
    //   if(iter->second!=NULL)
    //   {
    //     delete iter->second;
    //     iter->second=NULL;
    //   }
    // }
    // imgSets.clear();

    if(jInfo)
    {
      cJSON_Delete(jInfo);
      jInfo=NULL;
    }

    // for(int i=0;i<sharedInfo.size();i++)
    // {
    //   StageInfo* info=sharedInfo[i];
    //   if(info && info->isStillInUse()==false )
    //   {
    //     delete info;
    //   }
    //   sharedInfo[i]=NULL;
    // }
    sharedInfo.clear();
    StageInfoLiveCounter--;
#if STAGEINFO_LIFECYCLE_DEBUG
    LOGE("-->StageInfoLiveCounter:%d  :%p",StageInfoLiveCounter,this);
#endif

  }

  

};


class StageInfo_Image:public StageInfo
{
  public:
  static std::string stypeName(){return "Image";}
  virtual std::string typeName(){return this->stypeName();}
};




class StageInfo_Category:public StageInfo
{
  public:
  static std::string stypeName(){return "Category";}
  virtual std::string typeName(){return this->stypeName();}
  int category;


  virtual void dataSetToJInfo()
  {
    StageInfo::dataSetToJInfo();
    cJSON* repInfo=cJSON_CreateObject();
    cJSON_AddItemToObject(jInfo,"report",repInfo);
    cJSON_AddNumberToObject(repInfo,"category",category);
  }
};




class StageInfo_Group:public StageInfo
{
  public:
  static string stypeName(){return "Group";}
  string typeName(){return StageInfo_Group::stypeName();}


  

};


class StageInfo_SurfaceCheckSimple:public StageInfo
{
  public:
  static string stypeName(){return "SurfaceCheckSimple";}
  string typeName(){return this->stypeName();}

};

class StageInfo_Orientation:public StageInfo
{
  public:
  static string stypeName(){return "Orientation";}
  string typeName(){return StageInfo_Orientation::stypeName();}

  struct orient{
    acv_XY center;
    float angle;
    bool flip;
  };


  
  vector<struct orient> orientation;




  virtual void dataSetToJInfo()
  {
    StageInfo::dataSetToJInfo();

    StageInfo::dataSetToJInfo();
    cJSON* repArray=cJSON_CreateArray();
    cJSON_AddItemToObject(jInfo,"report",repArray);
    for(int i=0;i<orientation.size();i++)
    {
      LOGI(">>>");
      cJSON *jorient=cJSON_CreateObject();
      cJSON_AddItemToArray(repArray,jorient);
      orient orie=orientation[i];
      {
        
        cJSON *center=cJSON_CreateObject();

        
        cJSON_AddItemToObject(jorient,"center",center);

        cJSON_AddNumberToObject(center,"x",orie.center.X);
        cJSON_AddNumberToObject(center,"y",orie.center.Y);

      }
      // cJSON_AddNumberToObject(jorient,"area",tarArea);

      cJSON_AddNumberToObject(jorient,"angle",orie.angle);
      cJSON_AddBoolToObject(jorient,"flip",orie.flip);

    }
  }

  
};
