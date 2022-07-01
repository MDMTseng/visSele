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



class InspectionTarget_s_ColorRegionDetection :public InspectionTarget
{
public:
  InspectionTarget_s_ColorRegionDetection(std::string id,cJSON* def,InspectionTargetManager* belongMan):InspectionTarget(id,belongMan)
  {
    setInspDef(def);
  }

  virtual void setInspDef(cJSON* def)
  {
    InspectionTarget::setInspDef(def);
  }

  bool matchTriggerTag(string tarTag,cJSON* def)
  {
    cJSON* defTags=JFetch_ARRAY(def,"trigger_tag");
    int asize=cJSON_GetArraySize(defTags);
    for (int i = 0 ; i <asize ; i++)
    {
      cJSON * tag = cJSON_GetArrayItem(defTags, i);
      if(tag->type==cJSON_String)
      {
        string str = string(tag->valuestring);
        LOGI("str:%s tarTag:%s ",str.c_str(),tarTag.c_str());
        if(str==tarTag)
        {
          return true;
        }
      }
    }

    return false;
  }


  bool feedStageInfo(StageInfo* sinfo)
  {
    // string sinfo_camId= sinfo->StreamInfo.camera->getConnectionData().id;

    if(matchTriggerTag( sinfo->trigger_tag,def))// || sinfo_camId!=JFetch_STRING_ex(def,"camera_id",""))
    {
      acceptStageInfo(sinfo);
      return true;
    }
    return false;
  }

  int processInput()
  {
    int pCount=0;
    for(int i=0;i<input.size();i++)
    {
      StageInfo* stinfo=input[i];
      singleProcess(stinfo);
      pCount++;
      input[i]=NULL;//mark the data is deleted

      belongMan->recycleStageInfo(stinfo);
    }

    int availableCount=0;//warp it up(remove NULL element)
    for(int i=0;i<input.size();i++)
    {
      if(input[i])
      {
        input[availableCount]=input[i];
        availableCount++;
      }
    }
    input.resize(availableCount);
    return pCount;
  }


  void singleProcess(StageInfo* sinfo)
  {

    cJSON *report=cJSON_CreateObject();
    acvImage* srcImg=sinfo->imgSets["cam"];
    cv::Mat def_temp_img(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);

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
      
      char* defStr=cJSON_Print(regionInfo);
      LOGI(">>>%d\n%s",i,defStr);
      delete defStr;
      
    }


  }
  
  virtual ~InspectionTarget_s_ColorRegionDetection()
  {
  }

  // virtual InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)
  // {
  //   return NULL;
  // }

};



// class InspectionTarget_g :public InspectionTarget
// { 
// public:
//   struct report{
//     bool isReady;
//     std::string trigger_tag;
//     int trigger_id;
//     std::string cam_id;
//     uint64_t timeStamp_us;
//   } ;

//   report rep;

//   vector<InspectionTarget_s*> subInspList;
  
//   cJSON *inspresult=NULL;
//   InspectionTarget_g(std::string id,cJSON* def):InspectionTarget(id)

//   {
//     setInspDef(def);

//     char* defStr=cJSON_Print(this->def);
//     LOGI("\n%s",defStr);
//     delete defStr;
//   }


//   cJSON* fetchInspReport()
//   {
//     return inspresult;
//   }

  

//   void cleanInspReport()
//   {
    
//     for(int i=0;i<subInspList.size();i++)
//     {
//       subInspList[i]->cleanInspReport();
//     }
//     cJSON_Delete(inspresult);
//     inspresult=NULL;
//   }

//   cJSON* genInfo()
//   {
//     cJSON *obj=cJSON_CreateObject();

//     {
//       // cJSON *camInfo = cJSON_Parse(camera->getCameraJsonInfo().c_str());
//       // cJSON_AddItemToObject(obj, "camera", camInfo);
//     }

//     {
//       cJSON *otherInfo=cJSON_CreateArray();
//       cJSON_AddItemToObject(obj, "inspInfo", otherInfo);

//       for( auto sInsp:subInspList)
//       {
        
//         cJSON *info=cJSON_CreateObject();


//         cJSON_AddStringToObject(info, "id", sInsp->id.c_str() );
        
//         cJSON_AddItemToArray(otherInfo,info);
//       }



//     }

//     {
//       cJSON_AddNumberToObject(obj, "channel_id", channel_id);
//       cJSON_AddStringToObject(obj, "id", id.c_str());
//     }
//     return obj;
//   }

//   cJSON* genInspReport()
//   {
//     if(rep.isReady==false)return NULL;
//     cJSON *inspresult=NULL;
  
//     inspresult=cJSON_CreateObject();
//     cJSON_AddStringToObject(inspresult,"id",this->id.c_str());
//     cJSON_AddNumberToObject(inspresult,"channel_id",this->channel_id);
//     cJSON_AddStringToObject(inspresult,"trigger_tag",rep.trigger_tag.c_str());
//     cJSON_AddNumberToObject(inspresult,"trigger_id",rep.trigger_id);
//     cJSON_AddStringToObject(inspresult,"camera_id",rep.cam_id.c_str());
//     cJSON_AddNumberToObject(inspresult,"timeStamp_us",rep.timeStamp_us);


//     cJSON* rep_rules=cJSON_CreateArray();
//     cJSON_AddItemToObject(inspresult,"rules",rep_rules);

    
//     for(int i=0;i<subInspList.size();i++)
//     {
//       cJSON_AddItemToArray(rep_rules,cJSON_Duplicate(subInspList[i]->fetchInspReport(),true));
//     }

//     return inspresult;
//   }

//   InspectionTarget_EXCHANGE excdata={0};
//   std::timed_mutex rsclock;
//   InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)
//   {
//     rsclock.lock();
//     cJSON * json=info->info;

//     char *insp_type = JFetch_STRING(json, "insp_type");

//     memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));

//     if(strcmp(insp_type, "start_stream") ==0)
//     {
//       // camera->TriggerMode(0);
//       excdata.isOK=true;
//       return &excdata;
//     }
//     if(strcmp(insp_type, "stop_stream") ==0)
//     {
//       // camera->TriggerMode(2);
//       excdata.isOK=true;
//       return &excdata;
//     }
//     excdata.isOK=false;
//     return &excdata;

//   }

  
  
//   virtual void setInspDef(cJSON* def)
//   {

//     for(int i=0;i<subInspList.size();i++)
//     {
//       delete subInspList[i];
//       subInspList[i]=NULL;
//     }
//     subInspList.clear();

//     //clean up objects
//     InspectionTarget::setInspDef(def);
//     //build up objects

//     if(0){
//       char* defStr=cJSON_Print(def);
//       LOGI("\n%s",defStr);
//       delete defStr;
//     }


    
//     cJSON* rules=JFetch_ARRAY(def,"rules");
//     for (int i = 0 ; i < cJSON_GetArraySize(rules) ; i++)
//     {
//       cJSON * rule = cJSON_GetArrayItem(rules, i);
    
//       std::string type=std::string(JFetch_STRING(rule,"type"));
    
//       std::string id=std::string(JFetch_STRING(rule,"id"));

      
//       if(type=="ColorRegionLocating")
//       {
//         InspectionTarget_s* subIT=
//           new InspectionTarget_s_ColorRegionDetection(id,rule,&subInspList);
//         subInspList.push_back(subIT);
//       }
//       else if(type=="ShapeLocating")
//       {
//         InspectionTarget_s* subIT=
//           new InspectionTarget_s_ColorRegionDetection(id,rule,&subInspList);
//         subInspList.push_back(subIT);
//       }
//       else
//       {
//         //failed
//       }

//     }

//     bool is_Ref_OK=true;
//     for (int i = 0 ; i < subInspList.size() ; i++)
//     {
//       if(subInspList[i]->checkRef()==false)
//       {
//         is_Ref_OK=false;
//         break;
//       }
//     }

//   }

//   void CAM_CallBack(image_pipe_info *pipe)
//   {
//     rep.isReady=false;
//     // CameraLayer::frameInfo  info=srcCamSi->camera->GetFrameInfo();
//     // LOGI("<<<<id:%s<<<%s  WH:%d,%d  timeStamp_us:%" PRId64,id.c_str(),cam_id.c_str(),img.GetWidth(),img.GetHeight(),info.timeStamp_us);

//     rep.trigger_tag=pipe->trigger_tag;
//     rep.trigger_id=pipe->trigger_id;
//     rep.cam_id=pipe->camera_id;
//     rep.timeStamp_us=pipe->fi.timeStamp_us;
    
//     rep.isReady=true;


//     for(int i=0;i<subInspList.size();i++)
//     {
//       subInspList[i]->CAM_CallBack(pipe);
//       // subInspList[i]->fetchInspReport();
//     }
//     if(inspresult!=NULL)
//     {
//       cJSON_Delete(inspresult);
//       inspresult=NULL;
//     }
//     inspresult=genInspReport();
//   }

//   bool returnExchange(InspectionTarget_EXCHANGE* info)
//   {
//     if(info!=&excdata)return false;

//     if(info->info)
//       cJSON_Delete( info->info );
    
//     memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));
    
//     rsclock.unlock();
//     return true;
//   }
//   virtual ~InspectionTarget_g()
//   {
//     if(inspresult!=NULL)
//     {
//       cJSON_Delete(inspresult);
//     }
//   }
// };


