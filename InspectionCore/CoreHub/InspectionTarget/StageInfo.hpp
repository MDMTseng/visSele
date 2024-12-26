
#ifndef STAGEINFO_HPP
#define STAGEINFO_HPP
#include "common_lib.h"
#include <opencv2/opencv.hpp>
#include "InspectionTarget.hpp"


#include <CameraManager.hpp>
#include <atomic>
class InspectionTarget;
class CameraManager;


#define STAGEINFO_LIFECYCLE_DEBUG 1

static std::atomic<int> StageInfoLiveCounter={0};

class StageInfo{
  public:
  //BASIC--------------------------------
  std::string source_id;
  InspectionTarget *source;
  struct _img_prop{
    CameraLayer::frameInfo fi;
    CameraManager::StreamingInfo StreamInfo;


    //calib param
    cv::Mat cameraMatrix;
    cv::Mat distCoeffs;
    float mmpp;
    
  };
  struct _img_prop img_prop;
  int trigger_id;
  
  //IMAGE--------------------------------
  cv::Mat img;//the output image, to pass to next stage, and next stage can use it
  cv::Mat img_show;//the output image, to show in UI, 
  //it might have some debug draw on it, so it's different from img




  //TAG--------------------------------
  //pass the static tags, and other tags from inspTar
  std::vector<std::string> trigger_tags;


  //TIME--------------------------------
  //to track the process time
  float process_time_us;
  //to keep the highest resolution time
  int64_t create_time_sysTick;



  //REFERENCE--------------------------------
  std::vector<std::shared_ptr<StageInfo>> refInfo;//meaningful reference info, mainly for track



  //ERROR info--------------------------------
  int error_code;
  std::string error_msg;

  cJSON* jInfo;//generated json info from this object
  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    if(rep==NULL)
      rep=cJSON_CreateObject();
    cJSON_AddStringToObject(rep,"InspTar_id",source_id.c_str());
    cJSON_AddStringToObject(rep,"InspTar_type",typeName().c_str());
    cJSON_AddNumberToObject(rep,"trigger_id",trigger_id);
    cJSON_AddNumberToObject(rep,"process_time_us",process_time_us);
    cJSON_AddNumberToObject(rep,"time_stamp_us",img_prop.fi.timeStamp_us);


    cJSON* tagset=cJSON_CreateArray();
    cJSON_AddItemToObject(rep,"tags",tagset);
    for(auto tag:trigger_tags)
    {
      
      cJSON_AddItemToArray(tagset,cJSON_CreateString(tag.c_str()));
    }

    if(error_code!=0)
    {
      cJSON_AddNumberToObject(rep,"error_code",error_code);
      cJSON_AddStringToObject(rep,"error_msg",error_msg.c_str());
    }
    return rep;
  }




  virtual bool load(cJSON* json)
  {
    if(json==NULL)return false;
    source_id=JFetch_STRING_ex(json,"InspTar_id");

    trigger_id=JFetch_NUMBER_ex(json,"trigger_id",-1);
    process_time_us=JFetch_NUMBER_ex(json,"process_time_us",0);
    img_prop.fi.timeStamp_us=JFetch_NUMBER_ex(json,"time_stamp_us",0);
    trigger_tags.clear();
    cJSON* tags=JFetch_ARRAY(json,"tags");
    if(tags)
    {
      int size=cJSON_GetArraySize(tags);
      for(int i=0;i<size;i++)
      {
        cJSON* tag=cJSON_GetArrayItem(tags,i);
        if(tag)trigger_tags.push_back(tag->valuestring);
      }
    }
    
    error_code=JFetch_NUMBER_ex(json,"error_code",0);
    error_msg=JFetch_STRING_ex(json,"error_msg","");

    return true;
  }


  static std::string stypeName(){return "Base";}
  virtual std::string typeName(){return this->stypeName();}



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

    // for(int i=0;i<refInfo.size();i++)
    // {
    //   StageInfo* info=refInfo[i];
    //   if(info && info->isStillInUse()==false )
    //   {
    //     delete info;
    //   }
    //   refInfo[i]=NULL;
    // }
    refInfo.clear();
    StageInfoLiveCounter--;
#if STAGEINFO_LIFECYCLE_DEBUG
    LOGE("-->StageInfoLiveCounter:%d  :%p",(int)StageInfoLiveCounter,this);
#endif

  }




  static bool cJSON2Mat(cJSON* jObj,cv::Mat& mat)
  {
    float rows_f=JFetch_NUMBER_ex(jObj,"rows");
    float cols_f=JFetch_NUMBER_ex(jObj,"cols");
    if(rows_f!=rows_f||cols_f!=cols_f)
    {
      LOGE("cJSON2Mat: rows or cols does not match");
      mat=cv::Mat();
      return false;
    }

    int rows=round(rows_f);
    int cols=round(cols_f);

    cJSON* mat_j=cJSON_GetObjectItem(jObj,"mat");
    if(mat_j==NULL)
    {
      LOGE("cJSON2Mat: mat is NULL");
      mat=cv::Mat();
      return false;
    }

    int array_size=cJSON_GetArraySize(mat_j);
    if(array_size!=rows*cols)
    {
      LOGE("cJSON2Mat: mat.size(%d) does not match rows(%d)*cols(%d)",array_size,rows,cols);
      mat=cv::Mat();
      return false;
    }

    mat=cv::Mat(rows,cols,CV_64F);

    for(int i=0;i<rows;i++)
    {
      for(int j=0;j<cols;j++)
      {
        mat.at<double>(i,j)=cJSON_GetArrayItem(mat_j,i*cols+j)->valuedouble;
      }
    }
    return true;
  }

  static bool cJSONArray2Point2fArray(cJSON* jArray,std::vector<cv::Point2f>& points)
  {
    int array_size=cJSON_GetArraySize(jArray);
    if(array_size==0)
    {
      LOGE("cJSON2Point2fArray: points is NULL");
      return false;
    }

    points.resize(array_size);
    for(int i=0;i<array_size;i++)
    {
      cJSON* jPoint=cJSON_GetArrayItem(jArray,i);
      if(jPoint==NULL)
      {
        points.clear();
        LOGE("cJSONArray2Point2fArray: point %d is NULL",i);
        return false;
      }
      if(cJSON_GetArraySize(jPoint)!=2)
      {
        points.clear();
        LOGE("cJSONArray2Point2fArray: point %d is not a 2D point",i);
        return false;
      }
      points[i]=cv::Point2f(cJSON_GetArrayItem(jPoint,0)->valuedouble,cJSON_GetArrayItem(jPoint,1)->valuedouble);
    }
    return true;
  }
  static bool cJSONArray2Point3fArray(cJSON* jArray,std::vector<cv::Point3f>& points)
  {
    int array_size=cJSON_GetArraySize(jArray);
    if(array_size==0)
    {
      LOGE("cJSON2Point2fArray: points is NULL");
      return false;
    }

    points.resize(array_size);
    for(int i=0;i<array_size;i++)
    {
      cJSON* jPoint=cJSON_GetArrayItem(jArray,i);
      if(jPoint==NULL)
      {
        points.clear();
        LOGE("cJSONArray2Point2fArray: point %d is NULL",i);
        return false;
      }
      if(cJSON_GetArraySize(jPoint)!=3)
      {
        points.clear();
        LOGE("cJSONArray2Point2fArray: point %d is not a 2D point",i);
        return false;
      }
      points[i]=cv::Point3f(cJSON_GetArrayItem(jPoint,0)->valuedouble,cJSON_GetArrayItem(jPoint,1)->valuedouble,cJSON_GetArrayItem(jPoint,2)->valuedouble);
    }
    return true;
  }



  static void Point2fArray2cJSONArray(cJSON* jArray,std::vector<cv::Point2f>& ret_points)
  {
    for(int i=0;i<ret_points.size();i++)
    {
      cJSON* jPoint=cJSON_CreateArray();
      cJSON_AddItemToArray(jArray,jPoint);
      cJSON_AddItemToArray(jPoint,cJSON_CreateNumber(ret_points[i].x));
      cJSON_AddItemToArray(jPoint,cJSON_CreateNumber(ret_points[i].y));
    }
  }

  static void Point3fArray2cJSONArray(cJSON* jArray,std::vector<cv::Point3f>& ret_points)
  {
    for(int i=0;i<ret_points.size();i++)
    {
      cJSON* jPoint=cJSON_CreateArray();
      cJSON_AddItemToArray(jArray,jPoint);
      cJSON_AddItemToArray(jPoint,cJSON_CreateNumber(ret_points[i].x));
      cJSON_AddItemToArray(jPoint,cJSON_CreateNumber(ret_points[i].y));
      cJSON_AddItemToArray(jPoint,cJSON_CreateNumber(ret_points[i].z));
    }
  }

  static void Mat2cJson(cJSON* jObj,cv::Mat mat)
  {
    cJSON_AddNumberToObject(jObj,"rows",mat.rows);
    cJSON_AddNumberToObject(jObj,"cols",mat.cols);
    cJSON* array_mat=cJSON_CreateArray();
    cJSON_AddItemToObject(jObj,"mat",array_mat);

    for(int i=0;i<mat.rows;i++)
    {
      for(int j=0;j<mat.cols;j++)
      {
        cJSON_AddItemToArray(array_mat,cJSON_CreateNumber(mat.at<double>(i,j)));
      }
    }

    // {//test
    //   cv::Mat readMat;
    //   cJSON2Mat(jObj,readMat);
    //   String str_readMat="readMat:";
    //   str_readMat<<readMat;
    //   LOGI("%s",str_readMat.c_str());
    // }
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

#define IS_STAGEINFO_CAT_NOT_AVAILABLE(cat) ((cat)==STAGEINFO_CAT_NA || (cat)==STAGEINFO_CAT_NOT_EXIST || (cat)==STAGEINFO_CAT_UNSET)

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


class StageInfo_Empty:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "Empty";}
  virtual std::string typeName(){return this->stypeName();}



  virtual cJSON* attachJsonRep(cJSON* _rootRep,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo_Category::attachJsonRep(_rootRep,brifVector);

    cJSON* report=cJSON_GetObjectItem(rootRep,"report");
    cJSON_AddNumberToObject(report,"category",STAGEINFO_CAT_UNSET);
    return rootRep;
  }
};



class StageInfo_LineFitting:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "LineFitting";}
  virtual std::string typeName(){return this->stypeName();}

  float pt1_x,pt1_y,pt2_x,pt2_y;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    LOGE("StageInfo_LineFitting::attachJsonRep");
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);


    cJSON* report=cJSON_CreateObject();
    cJSON_AddItemToObject(rootRep,"report",report);

    //add result_cat
    cJSON_AddNumberToObject(rootRep,"category",category);

    if(!IS_STAGEINFO_CAT_NOT_AVAILABLE(category))
    {//add pt1 and pt2
      cJSON *pt1=cJSON_CreateObject();
      cJSON_AddItemToObject(report,"pt1",pt1);
      cJSON_AddNumberToObject(pt1,"x",pt1_x);
      cJSON_AddNumberToObject(pt1,"y",pt1_y);

      cJSON *pt2=cJSON_CreateObject();
      cJSON_AddItemToObject(report,"pt2",pt2);
      cJSON_AddNumberToObject(pt2,"x",pt2_x);
      cJSON_AddNumberToObject(pt2,"y",pt2_y);

    }


    return rootRep;
  }
};



class StageInfo_ArcFitting:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "ArcFitting";}
  virtual std::string typeName(){return this->stypeName();}

  float center_x,center_y,radius;
  float angle_start,angle_end;
  float sigma;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);

    cJSON* report=cJSON_CreateObject();
    cJSON_AddItemToObject(rootRep,"report",report);

    //add result_cat
    cJSON_AddNumberToObject(rootRep,"category",category);

    LOGE("MAKE json rep for ArcFitting");
    if(!IS_STAGEINFO_CAT_NOT_AVAILABLE(category))
    {//add pt1 and pt2
      cJSON *center=cJSON_CreateObject();
      cJSON_AddItemToObject(report,"center",center);
      cJSON_AddNumberToObject(center,"x",center_x);
      cJSON_AddNumberToObject(center,"y",center_y);

      cJSON_AddNumberToObject(report,"radius",radius);
      cJSON_AddNumberToObject(report,"sigma",sigma);
      cJSON_AddNumberToObject(report,"angle_start",angle_start);
      cJSON_AddNumberToObject(report,"angle_end",angle_end);

    }


    return rootRep;
  }
};



class StageInfo_DirectionalCaliper:public StageInfo_Category
{
  public:
  static std::string stypeName(){return "DirectionalCaliper";}
  virtual std::string typeName(){return this->stypeName();}

  float location_x,location_y;

  virtual cJSON* attachJsonRep(cJSON* rep=NULL,uint64_t brifVector=-1)
  {
    cJSON* rootRep=StageInfo::attachJsonRep(rep,brifVector);

    cJSON* report=cJSON_CreateObject();
    cJSON_AddItemToObject(rootRep,"report",report);

    //add result_cat
    cJSON_AddNumberToObject(rootRep,"category",category);

    if(!IS_STAGEINFO_CAT_NOT_AVAILABLE(category))
    {//add pt1 and pt2
      cJSON *location=cJSON_CreateObject();
      cJSON_AddItemToObject(report,"location",location);
      cJSON_AddNumberToObject(location,"x",location_x);
      cJSON_AddNumberToObject(location,"y",location_y);
    }


    return rootRep;
  }
};



class StageInfo_Orientation:public StageInfo
{
  public:


  
  static string stypeName(){return "Orientation";}
  string typeName(){return StageInfo_Orientation::stypeName();}

  struct orient{
    cv::Point2f center;
    float angle;
    bool flip;
    float confidence;
  };


  
  vector<struct orient> orientation;


  virtual bool load(cJSON* json)
  {
    StageInfo::load(json);
    cJSON* repArray=JFetch_ARRAY(json,"report");
    if(repArray==NULL)return false;

    int size=cJSON_GetArraySize(repArray);
    for(int i=0;i<size;i++)
    {
      cJSON* jorient=cJSON_GetArrayItem(repArray,i);
      if(jorient==NULL)continue;

      struct orient orie;
      orie.center.x=JFetch_NUMBER_ex(jorient,"center.x",0);
      orie.center.y=JFetch_NUMBER_ex(jorient,"center.y",0);
      orie.angle=JFetch_NUMBER_ex(jorient,"angle",0);
      orie.confidence=JFetch_NUMBER_ex(jorient,"confidence",0);
      orie.flip=JFetch_TRUE(jorient,"flip");

      orientation.push_back(orie);

    }

    return true;
  }




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

        cJSON_AddNumberToObject(center,"x",orie.center.x);
        cJSON_AddNumberToObject(center,"y",orie.center.y);
      }
      // cJSON_AddNumberToObject(jorient,"area",tarArea);

      cJSON_AddNumberToObject(jorient,"angle",orie.angle);
      cJSON_AddNumberToObject(jorient,"confidence",orie.confidence);
      
      cJSON_AddBoolToObject(jorient,"flip",orie.flip);



    }

    return rootRep;
  }

  
};


class StageInfo_DimMeasure:public StageInfo_Orientation
{
  public:
  static std::string stypeName(){return "DimMeasure";}
  virtual std::string typeName(){return this->stypeName();}






	enum ResultType{
		LINE=0,
		POINT=1,
		CIRCLE=3,
		VALUE=4,
		CATEGORY=5,
	} ;

	// struct Point2D{
	// 	float x;
	// 	float y;
	// };
	struct LINE_RESULT{
		cv::Point2f pt1;
		cv::Point2f pt2;
    float sigma;

	};

	struct POINT_RESULT{
		cv::Point2f pt1;//it's the target point
	};

	struct CIRCLE_RESULT{
		cv::Point2f c;
		float r;
	};

	struct VALUE_RESULT{
		float value;
    
	};

	struct CATEGORY_RESULT{
		int category;
	};

  struct MeasureResultInfo
	{
			// FeatureType feature_type;
			ResultType result_type;
			union{
				LINE_RESULT line;
				POINT_RESULT point;
				CIRCLE_RESULT circle;
				VALUE_RESULT value;
				CATEGORY_RESULT category;
			}report;

      cJSON* dbg_info;
			int error_code;
			string error_msg;
	};



  struct CategoryResultInfo
	{
    int category;
    vector<int> sub_category;
  };
  struct DimMeasureResultInfo{
    vector<MeasureResultInfo> measureList;

    vector<CategoryResultInfo> categoryList;
  };
	vector<DimMeasureResultInfo> DimMeasureResultList;


  cJSON* GenJsonFromResult(MeasureResultInfo &result)
  {
    cJSON* jresult=cJSON_CreateObject();
    switch(result.result_type)
    {
      case LINE:
      {
        cJSON* jpt1=cJSON_CreateObject();
        cJSON_AddItemToObject(jresult,"pt1",jpt1);
        cJSON_AddNumberToObject(jpt1,"x",result.report.line.pt1.x);
        cJSON_AddNumberToObject(jpt1,"y",result.report.line.pt1.y);

        cJSON* jpt2=cJSON_CreateObject();
        cJSON_AddItemToObject(jresult,"pt2",jpt2);
        cJSON_AddNumberToObject(jpt2,"x",result.report.line.pt2.x);
        cJSON_AddNumberToObject(jpt2,"y",result.report.line.pt2.y);


        break;
      }
      case POINT:
      {
        cJSON* jpt1=cJSON_CreateObject();
        cJSON_AddItemToObject(jresult,"pt1",jpt1);
        cJSON_AddNumberToObject(jpt1,"x",result.report.point.pt1.x);
        cJSON_AddNumberToObject(jpt1,"y",result.report.point.pt1.y);

        break;
      }
      case CIRCLE:
      {

        cJSON* jcentre=cJSON_CreateObject();
        cJSON_AddItemToObject(jresult,"c",jcentre);
        cJSON_AddNumberToObject(jcentre,"x",result.report.circle.c.x);
        cJSON_AddNumberToObject(jcentre,"y",result.report.circle.c.y);
        cJSON_AddNumberToObject(jresult,"r",result.report.circle.r);
        break;
      }
      case VALUE:
      {
        cJSON_AddNumberToObject(jresult,"value",result.report.value.value);
        break;
      }
      case CATEGORY:
      {
        cJSON_AddNumberToObject(jresult,"category",result.report.category.category);
        break;
      }
      default:
      {
        string error_msg="No serial code for result type:"+std::to_string(result.result_type);
        cJSON_AddStringToObject(jresult,"serial_error",error_msg.c_str());
        break;
      }

    }
    if(result.dbg_info!=NULL)
    {
      cJSON_AddItemToObject(jresult,"dbg_info",result.dbg_info);
      result.dbg_info=NULL;
    }
    if(result.error_code!=0)
    {
      cJSON_AddNumberToObject(jresult,"error_code",result.error_code);
      cJSON_AddStringToObject(jresult,"error_msg",result.error_msg.c_str());
    }
    return jresult;
  }




  cJSON* GenJsonFromResult(CategoryResultInfo &result)
  {
    cJSON* jresult=cJSON_CreateObject();
    cJSON_AddNumberToObject(jresult,"category",result.category);

    cJSON* jsub_category=cJSON_CreateArray();
    cJSON_AddItemToObject(jresult,"sub_category",jsub_category);
    for(int i=0;i<result.sub_category.size();i++)
    {
      cJSON_AddNumberToObject(jsub_category,std::to_string(i).c_str(),result.sub_category[i]);
    }
    return jresult;
  }


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

        cJSON_AddNumberToObject(center,"x",orie.center.x);
        cJSON_AddNumberToObject(center,"y",orie.center.y);

      }
      // cJSON_AddNumberToObject(jorient,"area",tarArea);

      cJSON_AddNumberToObject(jorient,"angle",orie.angle);
      cJSON_AddNumberToObject(jorient,"confidence",orie.confidence);
      
      cJSON_AddBoolToObject(jorient,"flip",orie.flip);


      {

        cJSON* jmresultArray=cJSON_CreateArray();
        cJSON_AddItemToObject(jorient,"element_report",jmresultArray);
        auto &result=DimMeasureResultList[i];
        for(int j=0;j<result.measureList.size();j++)
        {
          LOGE("genjRes:%d",j);
          cJSON* jresult=GenJsonFromResult(result.measureList[j]);
          cJSON_AddItemToArray(jmresultArray,jresult);
        }

        
        cJSON* jcatArray=cJSON_CreateArray();
        cJSON_AddItemToObject(jorient,"category_report",jcatArray);
        for(int j=0;j<result.categoryList.size();j++)
        {
          LOGE("genjRes:%d",j);
          cJSON* jresult=GenJsonFromResult(result.categoryList[j]);
          cJSON_AddItemToArray(jcatArray,jresult);
        }
      }





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


#endif