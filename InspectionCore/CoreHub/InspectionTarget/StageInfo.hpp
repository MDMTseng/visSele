#pragma once


#include "InspectionTarget.hpp"

#include <CameraManager.hpp>

class InspectionTarget;
class CameraManager;
using namespace std;


#define STAGEINFO_LIFECYCLE_DEBUG 1

static std::atomic<int> StageInfoLiveCounter={0};

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
  float process_time_us;
  int64_t create_time_sysTick;
  // std::map<std::string,std::shared_ptr<acvImage>> imgSets;
  cJSON* jInfo;
  
  virtual cJSON* genJsonRep()
  {
    cJSON* rep=cJSON_CreateObject();
    cJSON_AddStringToObject(rep,"InspTar_id",source_id.c_str());
    cJSON_AddStringToObject(rep,"InspTar_type",typeName().c_str());
    cJSON_AddNumberToObject(rep,"trigger_id",trigger_id);
    cJSON_AddNumberToObject(rep,"process_time_us",process_time_us);

    cJSON* tagset=cJSON_CreateArray();
    cJSON_AddItemToObject(rep,"tags",tagset);
    for(auto tag:trigger_tags)
    {
      
      cJSON_AddItemToArray(tagset,cJSON_CreateString(tag.c_str()));
    }
    return rep;
  }

  virtual void genJsonRepTojInfo()
  {
    if(jInfo)
    {
      cJSON_Delete(jInfo);
      jInfo=NULL;
    }

    jInfo=genJsonRep();

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
    LOGE("++>StageInfoLiveCounter:%d  :%p",(int)StageInfoLiveCounter,this);
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
    LOGE("-->StageInfoLiveCounter:%d  :%p",(int)StageInfoLiveCounter,this);
#endif

  }

  

};


class StageInfo_Image:public StageInfo
{
  public:
  static std::string stypeName(){return "Image";}
  virtual std::string typeName(){return this->stypeName();}
};


#define STAGEINFO_CAT_NA (0)
#define STAGEINFO_CAT_OK (1)
#define STAGEINFO_CAT_NG (-1)


#define STAGEINFO_CAT_NOT_EXIST (-40000)



class StageInfo_Category:public StageInfo
{
  public:
  static std::string stypeName(){return "Category";}
  virtual std::string typeName(){return this->stypeName();}
  int category;

  virtual cJSON* genJsonRep()
  {
    cJSON* rootRep=StageInfo::genJsonRep();


    cJSON* repInfo=cJSON_CreateObject();
    cJSON_AddItemToObject(rootRep,"report",repInfo);
    cJSON_AddNumberToObject(repInfo,"category",category);
    return rootRep;
  }
};

class StageInfo_Group_Category:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "Category";}
  virtual std::string typeName(){return this->stypeName();}

  struct Group_Info{
    int category;
    int score;
  };
  vector<struct Group_Info> group_info;

  virtual cJSON* genJsonRep()
  {
    cJSON* rootRep=StageInfo_Category::genJsonRep();
    cJSON* report=cJSON_GetObjectItem(rootRep,"report");

    cJSON* g_cat=cJSON_CreateArray();
    cJSON_AddItemToObject(report,"group_category",g_cat);
    for(int i=0;i<group_info.size();i++)
    {
      cJSON *ginfo=cJSON_CreateObject();
      cJSON_AddItemToArray(g_cat,ginfo);

      cJSON_AddNumberToObject(ginfo,"category",group_info[i].category);
      cJSON_AddNumberToObject(ginfo,"score",group_info[i].score);

    }
    return rootRep;
  }
};



class StageInfo_Group:public StageInfo
{
  public:
  static string stypeName(){return "Group";}
  string typeName(){return StageInfo_Group::stypeName();}


  

};


class StageInfo_SurfaceCheckSimple:public StageInfo_Category
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
    float confidence;
  };


  
  vector<struct orient> orientation;




  virtual cJSON* genJsonRep()
  {
    cJSON* rootRep=StageInfo::genJsonRep();

    cJSON* repArray=cJSON_CreateArray();
    cJSON_AddItemToObject(rootRep,"report",repArray);
    for(int i=0;i<orientation.size();i++)
    {
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
      cJSON_AddNumberToObject(jorient,"confidence",orie.confidence);
      
      cJSON_AddBoolToObject(jorient,"flip",orie.flip);



    }

    return rootRep;
  }

  
};
