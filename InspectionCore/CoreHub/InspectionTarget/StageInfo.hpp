#ifndef STAGEINFO_HPP
#define STAGEINFO_HPP
#include "common_lib.h"
#include <opencv2/opencv.hpp>
#include "InspectionTarget.hpp"


#include <CameraManager.hpp>
#include <atomic>
#include "cJSONPP.h"
class InspectionTarget;
class CameraManager;


#define STAGEINFO_LIFECYCLE_DEBUG 1

static std::atomic<int> StageInfoLiveCounter={0};




static int clear_object(cJSON* object){
  if(object==NULL)return -1;
  //check is object
  if(object->type!=cJSON_Object)return -2;

  cJSON *child = object->child;
  while (child) {
    cJSON *next = child->next;
    if (child->string) {
      cJSON *detached = cJSON_DetachItemFromObject(object, child->string);
      if (detached) {
        cJSON_Delete(detached);
      }
    }
    child = next;
  }
  
  return 0;
}
static int clear_array(cJSON* object){
  if(object==NULL)return -1;
  //check is array
  if(object->type!=cJSON_Array)return -2;
  
  // Use cJSON_DeleteItemFromArray in a loop
  while(cJSON_GetArraySize(object) > 0) {
    cJSON_DeleteItemFromArray(object, 0);
  }
  
  return 0;
}

static int clear_container(cJSON* container){
  if(container==NULL)return -1;
  //check is array
  if(container->type==cJSON_Array)
  {
    clear_array(container);
  }
  else if(container->type==cJSON_Object)
  {
    clear_object(container);
  }
  return 0;
}

class StageInfo{
  public:
  cJSON* jInfo;//generated json info from this object
  //BASIC--------------------------------

  int set_source_id(const std::string &id){//set to jInfo
    if(jInfo==NULL)return -1;
    cJSONPP jinfo(jInfo);
    jinfo.w()["source_id"]=id;
    return 0;
  }
  std::string get_source_id(){
    if(jInfo==NULL)return "";
    cJSONPP jinfo(jInfo);
    return jinfo["source_id"].asString();
  }


  
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
  // int trigger_id;
  int set_trigger_id(int id){
    if(jInfo==NULL)return -1;
    //remove the trigger_id
    cJSONPP jinfo(jInfo);
    jinfo.w()["trigger_id"]=id;
    return 0;
  }
  int get_trigger_id(){
    cJSONPP jinfo(jInfo);
    return jinfo["trigger_id"].asInt(-1);
  }
  int set_type(const std::string &type){
    if(jInfo==NULL)return -1;
    cJSONPP jinfo(jInfo);
    jinfo.w()["type"]=type;
    return 0;
  }
  std::string get_type(){
    if(jInfo==NULL)return "";
    cJSONPP jinfo(jInfo);
    return jinfo["type"].asString();
  }
  

  //IMAGE--------------------------------
  cv::Mat img;//the output image, to pass to next stage, and next stage can use it
  cv::Mat img_show;//the output image, to show in UI, 
  //it might have some debug draw on it, so it's different from img


  std::mutex lock;
  //to keep the highest resolution time
  int64_t create_time_sysTick;



  //TAG--------------------------------
  //pass the static tags, and other tags from inspTar
  public:
  std::vector<std::string> cached_trigger_tags;
  public:
  int set_trigger_tags(const std::vector<std::string> &tags){
    if(jInfo==NULL)return -1;
    cJSONPP jinfo(jInfo);
    auto tags_j=jinfo["trigger_tags"];
    if(tags_j.isArray()==false)
    {
      jinfo.w()["trigger_tags"]=cJSONPP::newArray();
      tags_j=jinfo["trigger_tags"];
    }

    clear_array(tags_j.get());


    for(int i=0;i<tags.size();i++)
    {
      tags_j.w()[i]=tags[i];
    }
    cached_trigger_tags=tags;
    return 0;
  }
  int push_trigger_tag(const std::string &tag){
    cJSON* tags_j=JFetch_ARRAY(jInfo,"trigger_tags");
    if(tags_j==NULL)
    {
      tags_j=cJSON_CreateArray();
      cJSON_AddItemToObject(jInfo,"trigger_tags",tags_j);
      cached_trigger_tags.clear();
    }
    cJSON_AddItemToArray(tags_j,cJSON_CreateString(tag.c_str()));
    cached_trigger_tags.push_back(tag);
    return 0;
  }
  int extract_append_trigger_tags(std::vector<std::string> &tags){
    if(jInfo==NULL)return -1;
    cJSON* tags_j=JFetch_ARRAY(jInfo,"trigger_tags");
    if(tags_j==NULL)return -2;
    int asize=cJSON_GetArraySize(tags_j);
    if(asize!=cached_trigger_tags.size())
    {
      cached_trigger_tags.clear();
      for(int i=0;i<asize;i++){
        cJSON* tag_j=cJSON_GetArrayItem(tags_j,i);
        if(tag_j==NULL)continue;
        cached_trigger_tags.push_back(tag_j->valuestring);
        
      }
    }
    tags=cached_trigger_tags;
    return 0;
  }
  int get_trigger_tags(std::vector<std::string> &tags){
    tags.clear();
    return extract_append_trigger_tags(tags);
  }


  //TIME--------------------------------
  //to track the process time
  // float process_time_us;
  int set_process_time_us(double time_us){
    cJSONPP jinfo(jInfo);
    jinfo.w()["process_time_us"]=time_us;
    return 0;
  }
  float get_process_time_us(){
    cJSONPP jinfo(jInfo);
    return jinfo["process_time_us"].asDouble();
  }



  //REFERENCE--------------------------------
  // std::vector<std::shared_ptr<StageInfo>> refInfo;//meaningful reference info, mainly for track



  //ERROR info--------------------------------
  // int error_code;
  // std::string error_msg;
  int set_error_code(int code,const std::string &msg){
    //remove the error_code and error_msg
    cJSON_DeleteItemFromObject(jInfo,"error_code");
    cJSON_DeleteItemFromObject(jInfo,"error_msg");
    if(code==0)
    {
      return 0;
    }
    cJSON_AddNumberToObject(jInfo,"error_code",code);
    cJSON_AddStringToObject(jInfo,"error_msg",msg.c_str());
    return 0;
  }
  int get_error_code(int &ret_code,std::string &ret_msg){
    ret_code=JFetch_NUMBER_ex(jInfo,"error_code");
    ret_msg=JFetch_STRING_ex(jInfo,"error_msg");
    return ret_code;
  }




  static std::string stypeName(){return "Base";}
  virtual std::string typeName(){return this->stypeName();}



  StageInfo(){
    StageInfoLiveCounter++;
    img_prop.StreamInfo=(CameraManager::StreamingInfo){0};
    img_prop.mmpp=0;
    img_prop.fi=( CameraLayer::frameInfo){0};
    source=NULL;
    jInfo=cJSON_CreateObject();
    set_source_id("");
    set_trigger_id(-1);
    
    cJSON_AddStringToObject(jInfo,"type",this->stypeName().c_str());

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
    // refInfo.clear();
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



  static int attach_Point2f_to_json(cJSON* dst_jobj,string key,const cv::Point2f &pt){
    if(dst_jobj==NULL)return -1;
    cJSON* jpt=cJSON_CreateObject();
    cJSON_AddItemToObject(dst_jobj,key.c_str(),jpt);
    cJSON_AddNumberToObject(jpt,"x",pt.x);
    cJSON_AddNumberToObject(jpt,"y",pt.y);
    return 0;
  }

  static int Point2f_from_json(cJSON* src_jobj,string key,cv::Point2f &ret_pt){
    if(src_jobj==NULL)return -1;
    cJSON* jpt=cJSON_GetObjectItem(src_jobj,key.c_str());
    if(jpt==NULL)return -2;
    ret_pt.x=JFetch_NUMBER_ex(jpt,"x");
    ret_pt.y=JFetch_NUMBER_ex(jpt,"y");
    return 0;
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


// class StageInfo_Image:public StageInfo
// {
//   public:
//   static std::string stypeName(){return "Image";}
//   virtual std::string typeName(){return this->stypeName();}
// };


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


#define STAGEINFO_CAT_SCS_PT_OVER_SIZE (-700)
#define STAGEINFO_CAT_SCS_LINE_OVER_LEN (-701)


#define STAGEINFO_CAT_SCS_COLOR_CORRECTION_THRES_LIMIT (-750)

#define STAGEINFO_CAT_SCS_EXTRA_STAT (51001)
// #define STAGEINFO_CAT_SCS_TOTAL_ELE_COUNT (-50070)
// #define STAGEINFO_CAT_SCS_TOTAL_ELE_AREA (-50071)
// #define STAGEINFO_CAT_SCS_MAX_LINE_LENGTH (-50073)
// #define STAGEINFO_CAT_SCS_TOTAL_BLOB_AREA (-50075)


int STAGEINFO_SCS_CAT_BASIC_reducer(int sum_cat,int cat);
class StageInfo_SurfaceCheckSimple:public StageInfo
{
  public:
  static std::string stypeName(){return "SurfaceCheckSimple";}
  virtual std::string typeName(){return this->stypeName();}


  StageInfo_SurfaceCheckSimple():StageInfo(){
    cJSON* repArray=cJSON_CreateArray();
    cJSON_AddItemToObject(jInfo,"report",repArray);
  }

  int set_category(int cat){
    cJSON_AddNumberToObject(jInfo,"category",cat);
    return 0;
  }


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
  };
  
  struct MatchRegion_Info{//per oriantation info
    int category;
    int score;
    vector<SubRegion_Info> subregions;
  };
  // vector<struct MatchRegion_Info> match_reg_info;


  int get_report_count(){
    
    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL)
    {
      repArray=cJSON_CreateArray();
      cJSON_AddItemToObject(this->jInfo,"report",repArray);
    }
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    return cJSON_GetArraySize(repArray);
  } 


  int set_report_object(int index,const MatchRegion_Info &orie, uint64_t brifVector=0){

    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL)
    {
      repArray=cJSON_CreateArray();
      cJSON_AddItemToObject(this->jInfo,"report",repArray);
    }
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    int asize=cJSON_GetArraySize(repArray);
    if(index>asize)return -2;
    if(index<0)index+=asize;
    if(index<0)return  -3;


    // LOGE("idx:%d  asize:%d",index,asize);
    cJSON *ginfo=NULL;
    if(index==asize)//append new
    {
      ginfo=cJSON_CreateObject();
      cJSON_AddItemToArray(repArray,ginfo);
    }
    else
    {
      ginfo=cJSON_GetArrayItem(repArray,index);
      //clear all items in the object
    }
    if(ginfo==NULL)return -5;
    clear_object(ginfo);

    cJSON_AddNumberToObject(ginfo,"category",orie.category);
    cJSON_AddNumberToObject(ginfo,"score",orie.score);


    cJSON* subregions=cJSON_CreateArray();
    cJSON_AddItemToObject(ginfo,"sub_regions",subregions);
    for(int j=0;j<orie.subregions.size();j++)
    {
      const SubRegion_Info &subreg=orie.subregions[j];

      cJSON *jsubreg=cJSON_CreateObject();
      cJSON_AddItemToArray(subregions,jsubreg);
      cJSON_AddNumberToObject(jsubreg,"category",subreg.category);
      cJSON_AddNumberToObject(jsubreg,"score",subreg.score);


      switch(subreg.type)
      {
        case id_HSVSeg:
        {
          // cJSON_AddNumberToObject(jsubreg,"element_count",subreg.hsvseg_stat.element_count);
          // cJSON_AddNumberToObject(jsubreg,"element_area",subreg.hsvseg_stat.element_area);
          // cJSON_AddNumberToObject(jsubreg,"blob_area",subreg.hsvseg_stat.blob_area);
          // cJSON_AddNumberToObject(jsubreg,"max_line_length",subreg.hsvseg_stat.max_line_length);


          if(brifVector!=0){

          cJSON* elements=cJSON_CreateArray();

          cJSON_AddItemToObject(jsubreg,"elements",elements);

          for(int k=0;k<subreg.elements.size();k++)
          {
            const Ele_info &einfo =subreg.elements[k];



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
          // cJSON_AddNumberToObject(jsubreg,"blob_count",subreg.scanPoint_stat.blob_count);
          break;
        }
        case id_CALC:
        {

          // auto &compile_fail_info=*(subreg.calc_stat.p_compile_fail_info);
          // if(compile_fail_info.size()>0 && i==0)//attach compile error only to the first subregion
          // {

          //   cJSON* compile_error=cJSON_CreateArray();

          //   cJSON_AddItemToObject(jsubreg,"compile_error",compile_error);

          //   for(int k=0;k<compile_fail_info.size();k++)
          //   {
          //     cJSON_AddItemToArray(compile_error,cJSON_CreateString(compile_fail_info[k].c_str()));
          //   }
          // }
          break;
        }

      }

    }


    return 0;
  }


  int push_report_object(const MatchRegion_Info &mri){
    // match_reg_info.push_back(mri);
    return set_report_object(get_report_count(),mri);
  }



};

class StageInfo_Empty:public StageInfo
{
  public:
  static std::string stypeName(){return "Empty";}
  virtual std::string typeName(){return this->stypeName();}


};



class StageInfo_Orientation:public StageInfo
{
  public:

  StageInfo_Orientation():StageInfo(){
    cJSONPP jinfo(jInfo);
    jinfo.w()["report"]=cJSONPP::newArray();

  }


  static string stypeName(){return "Orientation";}
  string typeName(){return StageInfo_Orientation::stypeName();}


  struct orient{
    cv::Point2f center;
    float angle;
    bool flip;
    float confidence;
  };
  // vector<struct orient> orientation;

  float get_mmpp(){

    cJSONPP jinfo(jInfo);
    return jinfo["mmpp"].asDouble();
  }
  int set_mmpp(float mmpp){

    cJSONPP jinfo(jInfo);
    jinfo.w()["mmpp"]=mmpp;
    return 0;
  }



  int get_report_count(){
    
    cJSONPP jinfo(jInfo);
    return jinfo["report"].length();
  }

  static int get_orient_from_json(cJSON* jObj,orient &ret_orie){
    cJSONPP jinfo(jObj);
    ret_orie.center.x=jinfo["center"]["x"].asDouble();
    ret_orie.center.y=jinfo["center"]["y"].asDouble();
    ret_orie.angle=jinfo["angle"].asDouble();
    ret_orie.confidence=jinfo["confidence"].asDouble();
    ret_orie.flip=jinfo["flip"].asBool();
    return 0;
  }
  static int set_orient_to_json(cJSON* jObj,const orient &orie){
    cJSONPP jinfo(jObj);
    jinfo.w()["center"]["x"]=orie.center.x;
    jinfo.w()["center"]["y"]=orie.center.y;
    jinfo.w()["angle"]=orie.angle;
    jinfo.w()["confidence"]=orie.confidence;
    jinfo.w()["flip"]=orie.flip;
    return 0;
  }


  int get_report_object(int index,orient &ret_orie){

    cJSONPP jinfo(jInfo);

    auto repArray=jinfo["report"];
    if(repArray.isArray()==false) return -1;


    int asize=repArray.length();
    if(index>=asize)return -2;
    if(index<0)index+=asize;
    if(index<0)return  -3;

    auto jorient=repArray[index];
    if(jorient.isObject()==false)return -4;

    get_orient_from_json(jorient.get(),ret_orie);
    return 0;
  }
  int set_report_object(int index,const orient &orie){

    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL)
    {
      repArray=cJSON_CreateArray();
      cJSON_AddItemToObject(this->jInfo,"report",repArray);
    }
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    int asize=cJSON_GetArraySize(repArray);
    if(index>asize)return -2;
    if(index<0)index+=asize;
    if(index<0)return  -3;


    LOGE("idx:%d  asize:%d",index,asize);
    cJSON *jorient=NULL;
    if(index==asize)//append new
    {
      jorient=cJSON_CreateObject();
      cJSON_AddItemToArray(repArray,jorient);
    }
    else
    {
      jorient=cJSON_GetArrayItem(repArray,index);
    }
    if(jorient==NULL)return -5;

  

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
  int push_report_object(const orient &orie){
    return set_report_object(get_report_count(),orie);
  }
  
  void clear_report(){
    cJSON* repArray = cJSON_DetachItemFromObject(this->jInfo, "report");
    if (repArray != NULL) {
      cJSON_Delete(repArray);
    }

    //create new report array
    repArray=cJSON_CreateArray();
    cJSON_AddItemToObject(this->jInfo,"report",repArray);
  }
};


class StageInfo_DimMeasure:public StageInfo
{
  public:
  static std::string stypeName(){return "DimMeasure";}
  virtual std::string typeName(){return this->stypeName();}


  StageInfo_DimMeasure():StageInfo(){
    cJSON* repArray=cJSON_CreateArray();
    cJSON_AddItemToObject(jInfo,"report",repArray);
  }



	enum ResultType{
		LINE=0,
		POINT=1,
		CIRCLE=3,
		VALUE=4,
		CATEGORY=5,

    UNSET=9999
	} ;

	// struct Point2D{
	// 	float x;
	// 	float y;
	// };

	struct LINE_RESULT{
		cv::Point2f pt1;
		cv::Point2f pt2;
    float sigma;
    static int from_json(cJSON* jObj,LINE_RESULT &ret_result){
      if(jObj==NULL)return -1;
      Point2f_from_json(jObj,"pt1",ret_result.pt1);
      Point2f_from_json(jObj,"pt2",ret_result.pt2);
      ret_result.sigma=JFetch_NUMBER_ex(jObj,"sigma");
      return 0;
    }
    static int to_json(cJSON* jresult,const LINE_RESULT &result){
      attach_Point2f_to_json(jresult,"pt1",result.pt1);
      attach_Point2f_to_json(jresult,"pt2",result.pt2);
      cJSON_AddNumberToObject(jresult,"sigma",result.sigma);
      return 0;
    }
	};

	struct POINT_RESULT{
		cv::Point2f pt1;//it's the target point
    static int from_json(cJSON* jObj,POINT_RESULT &ret_result){
      if(jObj==NULL)return -1;
      Point2f_from_json(jObj,"pt1",ret_result.pt1);
      return 0;
    }
    static int to_json(cJSON* jresult,const POINT_RESULT &result){
      attach_Point2f_to_json(jresult,"pt1",result.pt1);
      return 0;
    }
	};

	struct CIRCLE_RESULT{
		cv::Point2f c;
		float r;
    static int from_json(cJSON* jObj,CIRCLE_RESULT &ret_result){
      if(jObj==NULL)return -1;
      Point2f_from_json(jObj,"c",ret_result.c);
      ret_result.r=JFetch_NUMBER_ex(jObj,"r");
      return 0;
    }
    static int to_json(cJSON* jresult,const CIRCLE_RESULT &result){
      attach_Point2f_to_json(jresult,"c",result.c);
      cJSON_AddNumberToObject(jresult,"r",result.r);
      return 0;
    }
	};

	struct VALUE_RESULT{
		float value;
    static int from_json(cJSON* jObj,VALUE_RESULT &ret_result){
      if(jObj==NULL)return -1;
      ret_result.value=JFetch_NUMBER_ex(jObj,"value");
      return 0;
    }
    static int to_json(cJSON* jresult,const VALUE_RESULT &result){
      cJSON_AddNumberToObject(jresult,"value",result.value);
      return 0;
    }
	};

	struct CATEGORY_RESULT{
		int category;
    static int from_json(cJSON* jObj,CATEGORY_RESULT &ret_result){
      if(jObj==NULL)return -1;
      ret_result.category=JFetch_NUMBER_ex(jObj,"category");
      return 0;
    }
    static int to_json(cJSON* jresult,const CATEGORY_RESULT &result){
      cJSON_AddNumberToObject(jresult,"category",result.category);
      return 0;
    }
	};

  struct MeasureResultInfo
	{
			// FeatureType feature_type;
			ResultType result_type=UNSET;

      LINE_RESULT line;
      POINT_RESULT point;
      CIRCLE_RESULT circle;
      VALUE_RESULT value;
      CATEGORY_RESULT category;

      cJSON* dbg_info;
			int error_code;
			string error_msg;

      static int from_json(cJSON* jObj,MeasureResultInfo &ret_result)
      {
        ret_result.result_type=(ResultType)JFetch_NUMBER_ex(jObj,"result_type",UNSET);
       
        
        switch(ret_result.result_type)
        {
          case LINE:
            LINE_RESULT::from_json(jObj,ret_result.line);
            break;
          case POINT:
            POINT_RESULT::from_json(jObj,ret_result.point);
            break;
          case CIRCLE:
            CIRCLE_RESULT::from_json(jObj,ret_result.circle);
            break;
          case VALUE:
            VALUE_RESULT::from_json(jObj,ret_result.value);
            break;
          case CATEGORY:
            CATEGORY_RESULT::from_json(jObj,ret_result.category);
            break;
          default:
            return -1;
        }
        
        return 0;
      }
      static int to_json(cJSON* jresult,const MeasureResultInfo &result){
        cJSON_AddNumberToObject(jresult,"result_type",result.result_type);
        cJSON_AddItemToObject(
          jresult,
          "dbg_info",
          cJSON_Duplicate(result.dbg_info,true));
        switch(result.result_type)
        {
          case LINE:
            LINE_RESULT::to_json(jresult,result.line);
            break;
          case POINT:
            POINT_RESULT::to_json(jresult,result.point);
            break;
          case CIRCLE:
            CIRCLE_RESULT::to_json(jresult,result.circle);
            break;
          case VALUE:
            VALUE_RESULT::to_json(jresult,result.value);
            break;
          case CATEGORY:
            CATEGORY_RESULT::to_json(jresult,result.category);
            break;
          default:
            return -1;
        }
        return 0;
      }
  };

  struct CategoryResultInfo
	{
    int category;
    vector<int> sub_category;
    static int from_json(cJSON* jObj,CategoryResultInfo &ret_result){
      ret_result.category=JFetch_NUMBER_ex(jObj,"category",STAGEINFO_CAT_UNSET);
      ret_result.sub_category.clear();
      cJSON* jsub_category=JFetch_ARRAY(jObj,"sub_category");
      if(jsub_category==NULL || !cJSON_IsArray(jsub_category))
      {
        return -1;
      }
    
      int asize=cJSON_GetArraySize(jsub_category);
      for(int j=0;j<asize;j++)
      {
        cJSON* jsub_category_item=cJSON_GetArrayItem(jsub_category,j);
        if(jsub_category_item==NULL)continue;
        ret_result.sub_category.push_back(JFetch_NUMBER_ex(jsub_category_item,std::to_string(j).c_str(),STAGEINFO_CAT_UNSET));
      }
    
      return 0;
    }
    static int to_json(cJSON* jresult,const CategoryResultInfo &result){
      cJSON_AddNumberToObject(jresult,"category",result.category);
      cJSON* jsub_category=cJSON_CreateArray();
      cJSON_AddItemToObject(jresult,"sub_category",jsub_category);
      for(int i=0;i<result.sub_category.size();i++)
      {
        cJSON_AddNumberToObject(jsub_category,std::to_string(i).c_str(),result.sub_category[i]);
      }
      return 0;
    }
  };
  struct DimMeasureResultInfo{
    StageInfo_Orientation::orient pose;
    vector<MeasureResultInfo> measureList;

    vector<CategoryResultInfo> categoryList;
  };
	// vector<DimMeasureResultInfo> DimMeasureResultList;


  int get_report_count(){
    
    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL)
    {
      repArray=cJSON_CreateArray();
      cJSON_AddItemToObject(this->jInfo,"report",repArray);
    }
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    return cJSON_GetArraySize(repArray);
  }



  int get_report_object(int index,DimMeasureResultInfo &ret_info){

    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    int asize=cJSON_GetArraySize(repArray);
    if(index>=asize)return -2;
    if(index<0)index+=asize;
    if(index<0)return  -3;

    cJSON* info=cJSON_GetArrayItem(repArray,index);
    if(info==NULL)return -4;

    ret_info.categoryList.clear();

    cJSON* jcategoryList=JFetch_ARRAY(info,"category_list");
    if(jcategoryList!=NULL && cJSON_IsArray(jcategoryList))
    {
      int asize=cJSON_GetArraySize(jcategoryList);
      for(int i=0;i<asize;i++)
      {
        cJSON* jcategory=cJSON_GetArrayItem(jcategoryList,i);
        if(jcategory==NULL)continue;
        CategoryResultInfo category;
        CategoryResultInfo::from_json(jcategory,category);

        ret_info.categoryList.push_back(category);
      }
      
      
    }

    ret_info.measureList.clear();
    cJSON* jmeasureList=JFetch_ARRAY(info,"measure_list");
    if(jmeasureList!=NULL && cJSON_IsArray(jmeasureList))
    {
      int asize=cJSON_GetArraySize(jmeasureList);
      for(int i=0;i<asize;i++)
      {
        cJSON* jmeasure_item=cJSON_GetArrayItem(jmeasureList,i);
        if(jmeasure_item==NULL)continue;
        MeasureResultInfo measure;
        MeasureResultInfo::from_json(jmeasure_item,measure);
        ret_info.measureList.push_back(measure);
      }
    }

    StageInfo_Orientation::get_orient_from_json(info,ret_info.pose);
    // ret_info.pose.

    return 0;
  }



  
  int set_report_object(int index,const DimMeasureResultInfo &result){
    cJSON* repArray=JFetch_ARRAY(this->jInfo,"report");
    if(repArray==NULL)
    {
      repArray=cJSON_CreateArray();
      cJSON_AddItemToObject(this->jInfo,"report",repArray);
    }
    if(repArray==NULL || !cJSON_IsArray(repArray)) return -1;
    int asize=cJSON_GetArraySize(repArray);
    if(index>asize)return -2;
    if(index<0)index+=asize;
    if(index<0)return  -3;
    cJSON* info;
    if(index==asize)
    {
      info=cJSON_CreateObject();
      cJSON_AddItemToArray(repArray,info);
    }
    else
    {
      info=cJSON_GetArrayItem(repArray,index);
    }
    
    //clean up child of info

    clear_object(info);


    //add new child
    cJSON* jcategoryList=cJSON_CreateArray();
    cJSON_AddItemToObject(info,"category_report",jcategoryList);
    cJSON* jmeasureList=cJSON_CreateArray();
    cJSON_AddItemToObject(info,"element_report",jmeasureList);
    
    // result.pose

    for(int i=0;i<result.categoryList.size();i++)
    {
      CategoryResultInfo category=result.categoryList[i];
      cJSON* jcategory=cJSON_CreateObject();
      cJSON_AddItemToArray(jcategoryList,jcategory);
      CategoryResultInfo::to_json(jcategory,category);
    }

    for(int i=0;i<result.measureList.size();i++)
    {
      MeasureResultInfo measure=result.measureList[i];
      cJSON* jmeasure=cJSON_CreateObject();
      cJSON_AddItemToArray(jmeasureList,jmeasure);
      MeasureResultInfo::to_json(jmeasure,measure);
    }
    

    
    StageInfo_Orientation::set_orient_to_json(info,result.pose);

    return 0;

  }

  float get_mmpp(){
    return JFetch_NUMBER_ex(this->jInfo,"mmpp");
  }
    
  int set_mmpp(float mmpp){
    cJSON_AddNumberToObject(jInfo,"mmpp",mmpp);
    return 0;
  }


};


#endif