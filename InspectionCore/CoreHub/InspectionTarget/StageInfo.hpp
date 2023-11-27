#pragma once


#include "InspectionTarget.hpp"

#include <CameraManager.hpp>
#include <atomic>
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
  std::shared_ptr<acvImage> img_show;
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
  
  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    if(rep==NULL)
      rep=cJSON_CreateObject();
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





  virtual void genJsonRepTojInfo(uint64_t brifVector=0xFFFF)
  {
    if(jInfo)
    {
      cJSON_Delete(jInfo);
      jInfo=NULL;
    }

    jInfo=attachJsonRep(jInfo,brifVector);

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


#define STAGEINFO_CAT_UNSET (-999999)
#define STAGEINFO_CAT_NA (0)
#define STAGEINFO_CAT_OK (1)
#define STAGEINFO_CAT_OK2 (2)
#define STAGEINFO_CAT_NG (-1)
#define STAGEINFO_CAT_NG2 (-2)
#define STAGEINFO_CAT_NG3 (-3)
#define STAGEINFO_CAT_NG4 (-4)

#define STAGEINFO_CAT_NOT_EXIST (-40000)


class StageInfo_Category:public StageInfo
{
  public:
  static std::string stypeName(){return "Category";}
  virtual std::string typeName(){return this->stypeName();}
  int category;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);


    cJSON* repInfo=cJSON_CreateObject();
    cJSON_AddItemToObject(rootRep,"report",repInfo);
    cJSON_AddNumberToObject(repInfo,"category",category);

    return rootRep;
  }
};


class StageInfo_Value:public StageInfo
{
  public:
  static std::string stypeName(){return "Value";}
  virtual std::string typeName(){return this->stypeName();}
  int value;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);

    cJSON_AddNumberToObject(rootRep,"report",value);
    return rootRep;
  }
};



#define STAGEINFO_CAT_SCS_PT_OVER_SIZE (-700)
#define STAGEINFO_CAT_SCS_LINE_OVER_LEN (-701)


#define STAGEINFO_CAT_SCS_COLOR_CORRECTION_THRES_LIMIT (-750)

#define STAGEINFO_CAT_SCS_EXTRA_STAT (51001)
// #define STAGEINFO_CAT_SCS_TOTAL_ELE_COUNT (-50070)
// #define STAGEINFO_CAT_SCS_TOTAL_ELE_AREA (-50071)
// #define STAGEINFO_CAT_SCS_MAX_LINE_LENGTH (-50073)
// #define STAGEINFO_CAT_SCS_TOTAL_BLOB_AREA (-50075)


int STAGEINFO_SCS_CAT_BASIC_reducer(int sum_cat,int cat);
class StageInfo_SurfaceCheckSimple:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "SurfaceCheckSimple";}
  virtual std::string typeName(){return this->stypeName();}





  const static int id_UNSET=-1;
  const static int id_HSVSeg=1;
  const static int id_SigmaThres=2;
  const static int id_ScanPoint=3;
  const static int id_DirectionalDiff=4;


  const static int id_PassThru=5;


  const static int id_CALC=100;

  float pixel_size;
  struct Ele_info{//in subregion we have several elements(dot line....)
    // int type;

    int category;

    union content
    {
      struct
      {
        int perimeter;
        int x,y;
        int w,h;
        float angle;


        int area;
        int length;

      } line;

      struct
      {      
        int perimeter;  
        int x,y;
        int w,h;
        float angle;

        int area;

      } point;


      
      struct
      {
        char type[16];
        int value;
        float difference;

      } extra_stat;


    } data;

  };

  struct SubRegion_Info{//for every oriantation info we may have multiple subregions
    int category;
    float score;
    string name;
    vector<Ele_info> elements;
    int type;

    union {
      struct
      {
        int element_count;//in pixel
        int element_area;
        int blob_area;
        int max_line_length;

      } hsvseg_stat;


      struct
      {
      } sigmathres_stat;

      struct
      {
      } scanpoint_stat;

      struct
      {
        int blob_count;
      } scanPoint_stat;

      struct
      {
        std::vector<std::string> *p_compile_fail_info;
      } calc_stat;

    };
  };
  
  struct MatchRegion_Info{//per oriantation info
    int category;
    int score;
    vector<SubRegion_Info> subregions;
  };
  vector<struct MatchRegion_Info> match_reg_info;

  virtual cJSON* attachJsonRep(cJSON* _rootRep,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo_Category::attachJsonRep(_rootRep,brifVector);
    
    if(pixel_size==pixel_size)
      cJSON_AddNumberToObject(rootRep,"pixel_size",pixel_size);
    

    cJSON* report=cJSON_GetObjectItem(rootRep,"report");

    cJSON* g_cat=cJSON_CreateArray();
    cJSON_AddItemToObject(report,"sub_reports",g_cat);
    for(int i=0;i<match_reg_info.size();i++)
    {

      cJSON *ginfo=cJSON_CreateObject();
      cJSON_AddItemToArray(g_cat,ginfo);

      cJSON_AddNumberToObject(ginfo,"category",match_reg_info[i].category);
      cJSON_AddNumberToObject(ginfo,"score",match_reg_info[i].score);


      cJSON* subregions=cJSON_CreateArray();
      cJSON_AddItemToObject(ginfo,"sub_regions",subregions);
      for(int j=0;j<match_reg_info[i].subregions.size();j++)
      {
        SubRegion_Info &subreg=match_reg_info[i].subregions[j];

        cJSON *jsubreg=cJSON_CreateObject();
        cJSON_AddItemToArray(subregions,jsubreg);
        cJSON_AddNumberToObject(jsubreg,"category",subreg.category);
        cJSON_AddNumberToObject(jsubreg,"score",subreg.score);


        switch(subreg.type)
        {
          case id_HSVSeg:
          {
            cJSON_AddNumberToObject(jsubreg,"element_count",subreg.hsvseg_stat.element_count);
            cJSON_AddNumberToObject(jsubreg,"element_area",subreg.hsvseg_stat.element_area);
            cJSON_AddNumberToObject(jsubreg,"blob_area",subreg.hsvseg_stat.blob_area);
            cJSON_AddNumberToObject(jsubreg,"max_line_length",subreg.hsvseg_stat.max_line_length);


            if(brifVector!=0){

            cJSON* elements=cJSON_CreateArray();

            cJSON_AddItemToObject(jsubreg,"elements",elements);

            for(int k=0;k<subreg.elements.size();k++)
            {
              Ele_info &einfo =subreg.elements[k];



              cJSON *ele=cJSON_CreateObject();
              cJSON_AddItemToArray(elements,ele);
              cJSON_AddNumberToObject(ele,"category",einfo.category);
              

              switch(einfo.category)
              {
                case STAGEINFO_CAT_SCS_PT_OVER_SIZE:
                {

                  cJSON_AddNumberToObject(ele,"area",einfo.data.point.area);
                  cJSON_AddNumberToObject(ele,"perimeter",einfo.data.point.perimeter);
                  cJSON_AddNumberToObject(ele,"x",einfo.data.point.x);
                  cJSON_AddNumberToObject(ele,"y",einfo.data.point.y);
                  cJSON_AddNumberToObject(ele,"w",einfo.data.point.w);
                  cJSON_AddNumberToObject(ele,"h",einfo.data.point.h);
                  cJSON_AddNumberToObject(ele,"angle",einfo.data.point.angle);
                  break;
                }


                case STAGEINFO_CAT_SCS_LINE_OVER_LEN:
                {

                  cJSON_AddNumberToObject(ele,"area",einfo.data.line.area);
                  cJSON_AddNumberToObject(ele,"perimeter",einfo.data.line.perimeter);
                  cJSON_AddNumberToObject(ele,"x",einfo.data.line.x);
                  cJSON_AddNumberToObject(ele,"y",einfo.data.line.y);
                  cJSON_AddNumberToObject(ele,"w",einfo.data.line.w);
                  cJSON_AddNumberToObject(ele,"h",einfo.data.line.h);
                  cJSON_AddNumberToObject(ele,"angle",einfo.data.line.angle);
                  cJSON_AddNumberToObject(ele,"length",einfo.data.line.length);
                  break;
                }

                case STAGEINFO_CAT_SCS_EXTRA_STAT:
                {
                  cJSON_AddStringToObject(ele,"type",einfo.data.extra_stat.type);
                  cJSON_AddNumberToObject(ele,"value",einfo.data.extra_stat.value);
                  cJSON_AddNumberToObject(ele,"difference",einfo.data.extra_stat.difference);



                  
                  break;
                }


                default:
                break;
                
              }



            }
            
            }

            break;
          }
          case id_ScanPoint:
          {
            cJSON_AddNumberToObject(jsubreg,"blob_count",subreg.scanPoint_stat.blob_count);
            break;
          }
          case id_CALC:
          {

            auto &compile_fail_info=*(subreg.calc_stat.p_compile_fail_info);
            if(compile_fail_info.size()>0 && i==0)//attach compile error only to the first subregion
            {

              cJSON* compile_error=cJSON_CreateArray();

              cJSON_AddItemToObject(jsubreg,"compile_error",compile_error);

              for(int k=0;k<compile_fail_info.size();k++)
              {
                cJSON_AddItemToArray(compile_error,cJSON_CreateString(compile_fail_info[k].c_str()));
              }
            }
            break;
          }

        }

      }

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




  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);

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





class StageInfo_SorterInfo:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "SorterInfo";}
  virtual std::string typeName(){return this->stypeName();}
  int category;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);

    cJSON_AddNumberToObject(rootRep,"category",category);
    return rootRep;
  }
};
