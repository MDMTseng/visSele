#pragma once

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include "CameraLayerManager.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>
#include <InspTar_Orientation.hpp>
#include <InspTar_SurfaceCheckSimple.hpp>

using namespace cv;
// class InspectionTarget_s :public InspectionTarget
// {

// public:
//   vector <InspectionTarget_s*> *localGroup;
//   InspectionTarget_s(std::string id,cJSON* def,vector <InspectionTarget_s*> *localGroup):InspectionTarget(id)
//   {
//     this->localGroup=localGroup;
//     // setInspDef(def);
//     // report=NULL;
//   }

//   virtual bool checkRef()=0;

//   virtual acvImage* fetchImage(std::string id)=0;

//   virtual cJSON* fetchInspReport()=0;


//   virtual void setInspDef(cJSON* def)=0;

//   virtual void CAM_CallBack(image_pipe_info *pipe)=0;
  
//   virtual ~InspectionTarget_s()
//   {
//   }

// };



class InspectionTarget_TEST_IT :public InspectionTarget
{
  public:
  
  static std::string TYPE(){ return "TEST_IT"; }
  InspectionTarget_TEST_IT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):InspectionTarget(id,def,belongMan,local_env_path)
  {
  }

  virtual cJSON* genITIOInfo()
  {


    cJSON* arr= cJSON_CreateArray();

    {
      cJSON* opt= cJSON_CreateObject();
      cJSON_AddItemToArray(arr,opt);

      {
        cJSON* sarr= cJSON_CreateArray();
        
        cJSON_AddItemToObject(opt, "i",sarr );
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Image::stypeName().c_str() ));
      }

    }

    return arr;
  }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_TEST_IT::processInputStagePool,this);
  }

  int processInputPool()
  {
    int poolSize=input_pool.size();
    for(int i=0;i<poolSize;i++)
    {
      std::shared_ptr<StageInfo> curInput=input_pool[i];
      singleProcess(curInput);

      input_pool[i]=NULL;
    }
    input_pool.clear();

    return poolSize;//run all

  }


  
  void singleProcess(std::shared_ptr<StageInfo> sinfo)
  {
    // StageInfo *reportInfo=new StageInfo();
    // reportInfo->AddSharedInfo(sinfo);
    // belongMan->dispatch(reportInfo);
    LOGI("InspectionTarget_TEST_IT Got info from:%s ......",sinfo->source_id.c_str());
  }
};




class InspectionTarget_ColorRegionDetection :public InspectionTarget
{
public:
  InspectionTarget_ColorRegionDetection(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
    :InspectionTarget(id,def,belongMan,local_env_path)
  {
    type=InspectionTarget_ColorRegionDetection::TYPE();
  }

  static std::string TYPE(){ return "ColorRegionDetection"; }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_ColorRegionDetection::processInputStagePool,this);
  }

  int processInputPool()
  {
    int poolSize=input_pool.size();
    for(int i=0;i<poolSize;i++)
    {
      std::shared_ptr<StageInfo> curInput=input_pool[i];
      singleProcess(curInput);

      input_pool[i]=NULL;
    }
    input_pool.clear();


    return poolSize;//run all

  }

  

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
  {
    bool ret = InspectionTarget::exchangeCMD(info,id,act);
    if(ret)return ret;
    std::string type=JFetch_STRING_ex(info,"type");

    return false;
  }

  // bool feedStageInfo(StageInfo* sinfo)
  // {
  //   // string sinfo_camId= sinfo->StreamInfo.camera->getConnectionData().id;

  //   if(matchTriggerTag( sinfo->trigger_tag))// || sinfo_camId!=JFetch_STRING_ex(def,"camera_id",""))
  //   {
  //     acceptStageInfo(sinfo);
  //     return true;
  //   }
  //   return false;
  // }

  // int process()
  // {

  //   int pCount=0;
  //   StageInfo* fetchInfo;
  //   while(inputQ.pop(fetchInfo))
  //   {
  //     pCount++;
  //     singleProcess(fetchInfo);
  //     reutrnStageInfo(fetchInfo);
  //   }

  //   return pCount;
  // }


  virtual cJSON* genITIOInfo()
  {


    
    cJSON* arr= cJSON_CreateArray();

    {
      cJSON* opt= cJSON_CreateObject();
      cJSON_AddItemToArray(arr,opt);

      {
        cJSON* sarr= cJSON_CreateArray();
        
        cJSON_AddItemToObject(opt, "i",sarr );
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Image::stypeName().c_str() ));
      }

      {
        cJSON* sarr= cJSON_CreateArray();
        
        cJSON_AddItemToObject(opt, "o",sarr );
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo::stypeName().c_str() ));
      }




    }

    return arr;

  }

  void singleProcess(std::shared_ptr<StageInfo> sinfo)
  {

    cJSON *report=cJSON_CreateObject();
    auto srcImg=sinfo->img;


    acvImage *copyImg=new acvImage();
    copyImg->ReSize(srcImg.get());
    acvCloneImage(srcImg.get(),copyImg,-1);
    cv::Mat def_temp_img(copyImg->GetHeight(),copyImg->GetWidth(),CV_8UC3,copyImg->CVector[0]);


    // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);
    cJSON* rep_regionInfo=cJSON_CreateArray();
    
    cJSON_AddStringToObject(report,"id",id.c_str());
    cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);
    for(int i=0;;i++)
    {
      std::string key="regionInfo["+std::to_string(i)+"]";
      
      LOGI("key:%s",key.c_str());
      cJSON *regionInfo=JFetch_OBJECT(def,key.c_str());
      if(regionInfo==NULL)break;


      cJSON *region_report=cJSON_CreateObject();

      
      cJSON_AddNumberToObject(region_report,"idx",i);
      cJSON_AddItemToArray(rep_regionInfo,region_report);

      try{
        int X=(int)*JFetEx_NUMBER(regionInfo,"region[0]");
        int Y=(int)*JFetEx_NUMBER(regionInfo,"region[1]");
        int W=(int)*JFetEx_NUMBER(regionInfo,"region[2]");
        int H=(int)*JFetEx_NUMBER(regionInfo,"region[3]");


        cv::Mat def_temp_img_ROI = def_temp_img(cv::Rect(X, Y, W, H));


        

        if(1){
          
          Mat img_HSV;
          cvtColor(def_temp_img_ROI, img_HSV, COLOR_BGR2HSV);


          double l_h=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.h",0);
          double l_s=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.s",0);
          double l_v=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.v",0);

          double h_h=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.h",180);
          double h_s=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.s",255);
          double h_v=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.v",255);

          LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
          cv::Scalar rangeH=cv::Scalar(h_h,h_s,h_v);
          cv::Scalar rangeL=cv::Scalar(l_h,l_s,l_v);

          Mat img_HSV_threshold;
          inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);
          // cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);


          {
            double *colorThres=JFetch_NUMBER(regionInfo,"colorThres");
            if(colorThres)
            {
              cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
              cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
            }
          }


          {

            Mat labels;
            Mat1i stats;
            Mat centroids;
            // img_HSV_threshold =  cv::Scalar::all(255) - img_HSV_threshold;
            int n_labels = connectedComponentsWithStats(img_HSV_threshold, labels, stats, centroids);

            

            cJSON* components=cJSON_CreateArray();
            cJSON_AddItemToObject(region_report,"components",components);




            
            {
              double *resultOverlayAlpha=JFetch_NUMBER(regionInfo,"resultOverlayAlpha");
              if(resultOverlayAlpha && *resultOverlayAlpha>0 && *resultOverlayAlpha<=1)
              {
                // cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);
                Mat img_HSV_threshold_rgb;
                cv::cvtColor(img_HSV_threshold,img_HSV_threshold_rgb,COLOR_GRAY2RGB);
                addWeighted( 
                img_HSV_threshold_rgb, *resultOverlayAlpha, 
                def_temp_img_ROI, 1-*resultOverlayAlpha, 0.0, 
                def_temp_img_ROI);
                // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
                // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
              }
            }
              // cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);

            for(int k=1;k<n_labels;k++)
            {
              int area=stats.at<int>(k,cv::CC_STAT_AREA);
              
              if(area<100)continue;
              LOGI("comp[%d] area:%d",k,area);

              Point center = Point(centroids.at<double>(k, 0), centroids.at<double>(k, 1));
              // // circle center;
              // circle( def_temp_img_ROI, center, 1, Scalar(0,100,100), 3, LINE_AA);

              
              cJSON *comp_info=cJSON_CreateObject();
              
              cJSON_AddItemToArray(components,comp_info);

              
              cJSON_AddNumberToObject(comp_info,"area",area);
              cJSON_AddNumberToObject(comp_info,"x",center.x+X);
              cJSON_AddNumberToObject(comp_info,"y",center.y+Y);



            }
            // LOGI("idx:%d  colorThres>%f  region:[%d,%d,%d,%d],n_labels:%d",i,colorThres,
            //   X,Y,W,H,n_labels);



          }






          if(0){

              

            double dp=JFetch_NUMBER_ex(regionInfo,"hough_circle.dp",1);
            double minDist=JFetch_NUMBER_ex(regionInfo,"hough_circle.minDist",def_temp_img_ROI.rows/16);
            double param1=JFetch_NUMBER_ex(regionInfo,"hough_circle.param1",100);
            double param2=JFetch_NUMBER_ex(regionInfo,"hough_circle.param2",30);
            int minRadius=(int)JFetch_NUMBER_ex(regionInfo,"hough_circle.minRadius",5);
            int maxRadius=(int)JFetch_NUMBER_ex(regionInfo,"hough_circle.maxRadius",15);
            vector<Vec3f> circles;
            
            cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 7, 7), 0, 0 );
            // medianBlur(img_HSV_threshold, img_HSV_threshold, 3);
            cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);
            // cv::blur(def_temp_img_ROI, def_temp_img_ROI, 3);



            HoughCircles(img_HSV_threshold, circles, HOUGH_GRADIENT, dp,
                        minDist,  // change this value to detect circles with different distances to each other
                        param1, param2, minRadius, maxRadius // change the last two parameters
            );
            for( size_t i = 0; i < circles.size(); i++ )
            {
                Vec3i c = circles[i];
                Point center = Point(c[0], c[1]);
                // circle center
                // circle( def_temp_img_ROI, center, 1, Scalar(0,100,100), 3, LINE_AA);
                // circle outline
                int radius = c[2];
                circle( def_temp_img_ROI, center, radius, Scalar(255,0,255), 3, LINE_AA);
            }
              
          }






        }





        if(0){

          cv::Mat grayscaleMat (def_temp_img_ROI.size(), CV_8U);
          cv::cvtColor(def_temp_img_ROI, grayscaleMat,COLOR_RGB2GRAY);
          // cv::Mat image1=def_temp_img_ROI.copyTo(def_temp_img_ROI);





          double dp=JFetch_NUMBER_ex(regionInfo,"hough_circle.dp",1);
          double minDist=JFetch_NUMBER_ex(regionInfo,"hough_circle.minDist",grayscaleMat.rows/16);
          double param1=JFetch_NUMBER_ex(regionInfo,"hough_circle.param1",100);
          double param2=JFetch_NUMBER_ex(regionInfo,"hough_circle.param2",30);
          int minRadius=(int)JFetch_NUMBER_ex(regionInfo,"hough_circle.minRadius",5);
          int maxRadius=(int)JFetch_NUMBER_ex(regionInfo,"hough_circle.maxRadius",15);

          
          medianBlur(grayscaleMat, grayscaleMat, 1);
          vector<Vec3f> circles;
          HoughCircles(grayscaleMat, circles, HOUGH_GRADIENT, dp,
                      minDist,  // change this value to detect circles with different distances to each other
                      param1, param2, minRadius, maxRadius // change the last two parameters
          );
          for( size_t i = 0; i < circles.size(); i++ )
          {
              Vec3i c = circles[i];
              Point center = Point(c[0], c[1]);
              // circle center
              circle( def_temp_img_ROI, center, 1, Scalar(0,100,100), 3, LINE_AA);
              // circle outline
              int radius = c[2];
              circle( def_temp_img_ROI, center, radius, Scalar(255,0,255), 3, LINE_AA);
          }


        }




      }catch(...)
      {
        
        LOGE("....ERROR....");
      }
      
      // char* defStr=cJSON_Print(regionInfo);
      // LOGI(">>>%d\n%s",i,defStr);
      // delete defStr;

      
    }
    

    
    std::shared_ptr<StageInfo> reportInfo(new StageInfo());
    reportInfo->sharedInfo.push_back(sinfo);
    reportInfo->source=this;
    reportInfo->source_id=id;
    reportInfo->img=std::shared_ptr<acvImage>(copyImg);
    reportInfo->trigger_id=sinfo->trigger_id;

    reportInfo->sharedInfo.push_back(sinfo);
    reportInfo->trigger_tags.push_back(id);

    // reportInfo->fi=sinfo->fi;
    // reportInfo->StreamInfo=sinfo->StreamInfo;
    // reportInfo->trigger_tag=sinfo->trigger_tag;

    reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
    reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",-1);
    LOGI("CHID:%d",reportInfo->img_prop.StreamInfo.channel_id);

    
    cJSON *freport=cJSON_CreateObject();
    cJSON_AddItemToObject(freport,"report",rep_regionInfo);
    reportInfo->jInfo=freport;

    attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

    belongMan->dispatch(reportInfo);
  }
  
  virtual ~InspectionTarget_ColorRegionDetection()
  {
  }

  // virtual InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)
  // {
  //   return NULL;
  // }

};


class InspectionTarget_DataThreadedProcess :public InspectionTarget
{
  protected:
  TSQueue<std::shared_ptr<StageInfo>> datTransferQueue;
  std::thread runThread;
  int realTimeDropFlag;
  public:
  
  static std::string TYPE(){ return "DataTransfer"; }
  InspectionTarget_DataThreadedProcess(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget(id,def,belongMan,local_env_path),
    datTransferQueue(10),
    runThread(&InspectionTarget_DataThreadedProcess::thread_run,this)
  {
    realTimeDropFlag=-1;
  }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_DataThreadedProcess::processInputStagePool,this);
  }


  


  int processInputPool()
  {
    int poolSize=input_pool.size();
    for(int i=0;i<poolSize;i++)
    {
      std::shared_ptr<StageInfo> curInput=input_pool[i];
      
      
      try{
        
        if(curInput->img_prop.StreamInfo.channel_id==0)
        {//no enough info return...
          LOGE("---no channel_id available");
          
          LOGE("PUSH Failed....");
        }
        else if(realTimeDropFlag<=0 && datTransferQueue.push(curInput))
        {
          if(realTimeDropFlag>=0)
            realTimeDropFlag++;
          LOGI("PUSH PUSH datTransferQueue.size:%d",datTransferQueue.size());
        }
        else
        {
          LOGE("PUSH Failed....");
        }
      }
      catch(TS_Termination_Exception e)
      {
        
        LOGE("TS_Termination_Exception....");
        break;
      }


      input_pool[i]=NULL;
      // reutrnStageInfo(curInput);//remember to recycle the StageInfo
    }
    input_pool.clear();

    return poolSize;//run all

  }
  virtual void thread_run()=0;
  ~InspectionTarget_DataThreadedProcess()
  {
    datTransferQueue.termination_trigger();
    runThread.join();
    
  }

};




class InspectionTarget_StageInfoCollect_Base :public InspectionTarget
{
  
  protected:


  struct infoGroupinfo{
    // int trigger_id;
    std::vector< std::shared_ptr<StageInfo> >  group;
    
  };

  std::map<int,  struct infoGroupinfo> input_stage_sets;
  std::map<int,  struct infoGroupinfo> input_pool_sets;
  
  cJSON * match_tag_sets;
  public:
  
  static std::string TYPE(){ return "StageInfoCollect_Base"; }
  InspectionTarget_StageInfoCollect_Base(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget(id,def,belongMan,local_env_path)
  {
    setInspDef(def);
  }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_StageInfoCollect_Base::processInputStagePool,this);
  }


  void setInspDef(cJSON* def)
  {
    InspectionTarget::setInspDef(def);
    LOGE("akjdfjkaflkajsndfklasjkldj=============");
    match_tag_sets=NULL;
    if(def)
    {
      match_tag_sets=JFetch_ARRAY(this->def,"match_tag_sets");
    }
  }


  bool tagMatchingWhiteListIndex(int whiteListidx,vector<string> &tagArr)
  {
    if(match_tag_sets==NULL)return false;
    
    int set_size=cJSON_GetArraySize(match_tag_sets);
    if(whiteListidx>=set_size)return false;
    


    cJSON *tags = cJSON_GetArrayItem(match_tag_sets,whiteListidx);

    return tagMatching(tags,tagArr);
  }

  int tagMatchingWhiteList(vector<string> &tagArr)
  {

    int set_size=cJSON_GetArraySize(match_tag_sets);
    for(int i=0;i<set_size;i++)
    {
      if(tagMatchingWhiteListIndex(i,tagArr))
      {
        return i;
      }
    }
    return -1;
  }




  int processInputStagePool()
  {
    {
      std::lock_guard<std::mutex> _(input_stage_lock);
      for (auto const& stage_set : input_stage_sets)
      {
        auto &igi=stage_set.second;
        bool isFullMatch=true;
        for(int i=0;i<igi.group.size();i++)
        {
          if(igi.group[i].get()==NULL)
          {
            isFullMatch=false;
            break;
          }
        }

        if(isFullMatch)
        {
          input_pool_sets.insert ( std::pair<int, struct infoGroupinfo >(stage_set.first,stage_set.second) );

        }

      }


      for (auto const& stage_set : input_pool_sets)
      {

          input_stage_sets.erase(stage_set.first);
      }

    }


    int processCount = processInputPool();
    
    inputPoolInsufficient=(processCount==0);

    return processCount;

  }



  bool feedStageInfo(std::shared_ptr<StageInfo> sinfo)
  {
    std::lock_guard<std::mutex> _(input_stage_lock);
    int info_tid=sinfo->trigger_id;
    bool isMatched=false;

    if (input_stage_sets.find(info_tid) == input_stage_sets.end()) 
    { 
      int matching_idx=tagMatchingWhiteList(sinfo->trigger_tags);

      if(matching_idx>=0)//matched with idx in WhiteList
      {
        struct infoGroupinfo igi;
        int set_size=cJSON_GetArraySize(match_tag_sets);
        for(int i=0;i<set_size;i++)
        {
          igi.group.push_back(NULL);//fill with empty slot
        }
        igi.group[matching_idx]=sinfo;
        input_stage_sets.insert ( std::pair<int, struct infoGroupinfo >(info_tid,igi) );
        isMatched=true;
      }

    }
    else
    { 
      auto &stage_set=input_stage_sets[info_tid];
      for(int i=0;i<stage_set.group.size();i++)
      {
        if(stage_set.group[i].get()==NULL)//a slot need to be fill
        {
          if(tagMatchingWhiteListIndex(i,sinfo->trigger_tags))//try match
          {
            isMatched=true;
            stage_set.group[i]=sinfo;
            break;
          }
        }
      }

    }


    return isMatched;
  }



  

  virtual cJSON* genITIOInfo()
  {

    cJSON* arr= cJSON_CreateArray();

    {
      cJSON* opt= cJSON_CreateObject();
      cJSON_AddItemToArray(arr,opt);

      {
        cJSON* sarr= cJSON_CreateArray();
        
        cJSON_AddItemToObject(opt, "i",sarr );
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo::stypeName().c_str() ));
      }
    }

    return arr;
  }

  int processInputPool()
  {
    int processedSize=0;
    for (auto const& stage_set : input_pool_sets)
    {
      processGroup(stage_set.first,stage_set.second.group);
      processedSize++;
    }

    input_pool_sets.clear();

    return processedSize;//run all

  }

  virtual void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {
    
    LOGI("processGroup:  trigger_id:%d =========",trigger_id);
    for(int i=0;i<group.size();i++)
    {
      LOGI("[%d]:from:%s  =========",i,group[i]->source_id.c_str());
    }
  }

  ~InspectionTarget_StageInfoCollect_Base()
  {
    
  }
};




class InspectionTarget_ReduceCategorize :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  public:

  
  InspectionTarget_ReduceCategorize(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path)
    {}
  static std::string TYPE(){ return "ReduceCategorize"; }
  void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {
    

    shared_ptr<StageInfo_Category> reportInfo(new StageInfo_Category());

  
    reportInfo->source=this;
    reportInfo->source_id=id;
    
    reportInfo->trigger_id=trigger_id;
    reportInfo->trigger_tags.push_back(reportInfo->source_id);
    reportInfo->category=-909;
    reportInfo->jInfo=NULL;
    belongMan->dispatch(reportInfo);

  }

  ~InspectionTarget_ReduceCategorize()
  {
  }
  

};





class InspectionTarget_GroupResultSave :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  TSQueue< std::vector< std::shared_ptr<StageInfo> > > datTransferQueue;


  std::thread runThread;
  public:

  
  InspectionTarget_GroupResultSave(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path),
    datTransferQueue(10),runThread(&InspectionTarget_GroupResultSave::thread_run,this)
    {}
  static std::string TYPE(){ return "GroupResultSave"; }
  void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {
    
    // if(datTransferQueue.push(group)==false)
    // {
    //   LOGI("datTransferQueue.push FAILED.....");
    // }

  }

  void thread_run();

  ~InspectionTarget_GroupResultSave()
  {
    datTransferQueue.termination_trigger();
    runThread.join();
    
  }
  

};