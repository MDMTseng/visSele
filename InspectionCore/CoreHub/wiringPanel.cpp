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
#define _VERSION_ "1.2"
std::timed_mutex mainThreadLock;


m_BPG_Protocol_Interface bpg_pi;



m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;


struct pipetriggerInfo_mix{
  image_pipe_info *pipeInfo;
  
  struct _triggerInfo{

    int trigger_id;
    std::string trigger_tag;
    std::string camera_id;
    uint64_t est_trigger_time_us;
  };
  _triggerInfo triggerInfo;
};
// vector<pipetriggerInfo_mix> triggerInfoBuffer(10);
// TSQueue<pipetriggerInfo_mix> triggerInfoQueue(10);


TSVector<pipetriggerInfo_mix> triggerInfoMatchingBuffer;



TSQueue<pipetriggerInfo_mix> triggerInfoMatchingQueue(10);



TSQueue<image_pipe_info *> inspQueue(10);
TSQueue<image_pipe_info *> datViewQueue(10);
TSQueue<image_pipe_info *> inspSnapQueue(5);



bool cleanUp_triggerInfoMatchingBuffer_UNSAFE()
{
  int zeroCount=0;
  for(int i=0;i<3;i++)
  {
    if(inspQueue.size()+datViewQueue.size()+inspSnapQueue.size()==0)
    {

    }
    else
    {
      i=0;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  for(int i=0;i<triggerInfoMatchingBuffer.size();i++)
  {
    if(triggerInfoMatchingBuffer[i].pipeInfo!=NULL)
    {
      bpg_pi.resPool.retResrc(triggerInfoMatchingBuffer[i].pipeInfo);
      triggerInfoMatchingBuffer[i].pipeInfo=NULL;
    }
  }
  triggerInfoMatchingBuffer.clear();
  return true;
}




class InspectionTargetManager_m:public InspectionTargetManager
{
  vector<uint8_t> frameDataBuff;

  void CamStream_CallBack(CameraManager::StreamingInfo &info){




    LOGE("============DO INSP>> waterLvL: insp:%d/%d dview:%d/%d  snap:%d/%d   poolSize:%d trigInfoMatchingSize:%d",
        inspQueue.size(), inspQueue.capacity(),
        datViewQueue.size(), datViewQueue.capacity(),
        inspSnapQueue.size(), inspSnapQueue.capacity(),
        bpg_pi.resPool.rest_size(),triggerInfoMatchingBuffer.size());




    image_pipe_info *pinfo= bpg_pi.resPool.fetchResrc_blocking();

    CameraLayer::frameInfo finfo = info.camera->GetFrameInfo();
    pinfo->StreamInfo=info;
    
    pinfo->camera_id=info.camera->getConnectionData().id;
    pinfo->fi=finfo;
    pinfo->trigger_tag="";
    pinfo->img.ReSize(finfo.width,finfo.height,3);
    CameraLayer::status st = info.camera->ExtractFrame(pinfo->img.CVector[0],3,finfo.width*finfo.height);
    LOGI(">>>CAM:%s  WH:%d %d",pinfo->camera_id.c_str(),finfo.width,finfo.height);

    pipetriggerInfo_mix pmix;
    pmix.pipeInfo=pinfo;
    
      LOGI("0");
    triggerInfoMatchingQueue.push_blocking(pmix);
      LOGI("1");

  }

};


int ReadImageAndPushToInspQueue(string path,string camera_id,string trigger_tag,int trigger_id,int channel_id)
{
  image_pipe_info *pinfo= bpg_pi.resPool.fetchResrc();
  if(pinfo==NULL)return -1;

  pinfo->StreamInfo.camera=NULL;
  pinfo->StreamInfo.channel_id=channel_id;
  pinfo->camera_id=camera_id;
  pinfo->trigger_tag=trigger_tag;

  Mat mat=imread(path.c_str());

  int H = mat.rows;
  int W = mat.cols;
  CameraLayer::frameInfo finfo;
  finfo.offset_x=finfo.offset_y=0;
  finfo.height=H;
  finfo.width=W;
  finfo.timeStamp_us=0;
  pinfo->fi=finfo;

  pinfo->img.ReSize(W,H);
  
  cv::Mat dst_mat(H,W,CV_8UC3,pinfo->img.CVector[0]);

  


  if(mat.channels()==3)
  {
    mat.copyTo(dst_mat);
  }
  else if(mat.channels()==1)
  {
    cv::cvtColor(mat,dst_mat,COLOR_GRAY2RGB);
  }
  else
  {
    bpg_pi.resPool.retResrc(pinfo);
    return -2;
  }

  inspQueue.push_blocking(pinfo);

  return 0;
}





InspectionTargetManager_m inspTarMan;

void TriggerInfoMatchingThread(bool *terminationflag)
{
  
  using Ms = std::chrono::milliseconds;
  int delayStartCounter = 10000;
  while (terminationflag && *terminationflag == false)
  {

    //   if(delayStartCounter>0)
    //   {
    //     delayStartCounter--;
    //   }
    //   else
    //   {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //   }
    pipetriggerInfo_mix headImgPipeMixInfo;

    // triggerInfoQueue.pop_blocking(&trigInfo)
    while (triggerInfoMatchingQueue.pop_blocking(headImgPipeMixInfo))
    {
      LOGI("headImgPipeMixInfo.pipeInfo:%p",headImgPipeMixInfo.pipeInfo);
      // triggerInfoMatchingBuffer.w_lock();
      
      {

        // 
        int minMatchingCost=999999;
        int minMatchingIdx=-1;

        image_pipe_info *targetImgPipe=NULL;
        pipetriggerInfo_mix::_triggerInfo targetTriggerInfo;

        if(headImgPipeMixInfo.pipeInfo!=NULL)//image/pipe info
        {
          targetImgPipe= headImgPipeMixInfo.pipeInfo;
          



          bool doMockTriggerInfo=(targetImgPipe->StreamInfo.camera!=NULL) && (targetImgPipe->StreamInfo.camera->triggerMode==0);//streaming



          if(doMockTriggerInfo)
          {
            pipetriggerInfo_mix::_triggerInfo mocktrig;
            mocktrig.camera_id=targetImgPipe->camera_id;
            mocktrig.est_trigger_time_us=targetImgPipe->fi.timeStamp_us+123;//
            mocktrig.trigger_id=-1;
            mocktrig.trigger_tag="Stream";
            
            // triggerInfoMatchingBuffer.w_unlock();
            triggerInfoMatchingBuffer.push_back({triggerInfo:mocktrig});
            // triggerInfoMatchingBuffer.w_lock();
          }

          
          for(int i=0;i<triggerInfoMatchingBuffer.size();i++)//try to find trigger info matching
          {//
            if(triggerInfoMatchingBuffer[i].pipeInfo!=NULL)continue;//skip image/pipe info

            auto _triggerInfo=triggerInfoMatchingBuffer[i].triggerInfo;
            if(_triggerInfo.camera_id!=targetImgPipe->camera_id)continue;//camera id is not match


            int cost;
            
            
            if(_triggerInfo.est_trigger_time_us==0)
            {
              cost=0;
            }
            else
            {
              cost=_triggerInfo.est_trigger_time_us-targetImgPipe->fi.timeStamp_us;
              if(cost<0)cost=-cost;

            }
          
            if(minMatchingCost>cost)
            {
              targetTriggerInfo=_triggerInfo;
              minMatchingIdx=i;
              minMatchingCost=cost;
              if(cost==0)break;
            }
          }
        }
        else//trigger info 
        {

          targetTriggerInfo=headImgPipeMixInfo.triggerInfo;
          for(int i=0;i<triggerInfoMatchingBuffer.size();i++)//try to find image/pipe info
          {//
            if(triggerInfoMatchingBuffer[i].pipeInfo==NULL)continue;//skip trigger info

            auto _PipeInfo=triggerInfoMatchingBuffer[i].pipeInfo;
            if(targetTriggerInfo.camera_id!=_PipeInfo->camera_id)continue;//camera id is not match

            
            int cost;
            
            
            if(targetTriggerInfo.est_trigger_time_us==0)
            {
              cost=0;
            }
            else
            {
              cost=_PipeInfo->fi.timeStamp_us-targetTriggerInfo.est_trigger_time_us;
              if(cost<0)cost=-cost;
            }
          



            if(minMatchingCost>cost)
            {
              targetImgPipe=_PipeInfo;
              minMatchingIdx=i;
              minMatchingCost=cost;
              if(cost==0)break;
            }
          }
        }





        if( minMatchingIdx!=-1 && minMatchingCost<1000)
        {
          LOGI("Get matching. idx:%d cost:%d  psss to next Q",minMatchingIdx,minMatchingCost);
          targetImgPipe->trigger_tag=targetTriggerInfo.trigger_tag;
          targetImgPipe->trigger_id=targetTriggerInfo.trigger_id;
          inspQueue.push_blocking(targetImgPipe);
          triggerInfoMatchingBuffer.erase(minMatchingIdx);//remove from buffer 
        }
        else
        {
          LOGI("No matching.... push in buffer");
          triggerInfoMatchingBuffer.push_back(headImgPipeMixInfo);//no match, add new data to buffer 
          LOGI("buffer size:%d",triggerInfoMatchingBuffer.size());
      LOGI(">>>>>");
        }
      }
      LOGI(">>>>>");
      
      // triggerInfoMatchingBuffer.w_unlock();
      
      LOGI(">>>>>");

    }



  }
}



void ImgPipeProcessThread(bool *terminationflag)
{
  using Ms = std::chrono::milliseconds;
  int delayStartCounter = 10000;
  while (terminationflag && *terminationflag == false)
  {

    //   if(delayStartCounter>0)
    //   {
    //     delayStartCounter--;
    //   }
    //   else
    //   {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //   }
    image_pipe_info *headImgPipe = NULL;

    while (inspQueue.pop_blocking(headImgPipe))
    {
      
      LOGI(">>>CAM:%s",headImgPipe->camera_id.c_str());
      LOGI(">>>triggerT:%s  id:%d",headImgPipe->trigger_tag.c_str(),headImgPipe->trigger_id);
      inspTarMan.inspTargetProcess(*headImgPipe);
      datViewQueue.push_blocking(headImgPipe);
      // bpg_pi.resPool.retResrc(headImgPipe);
    }
  }
}

void ImgPipeDatViewThread(bool *terminationflag)
{
  
  acvImage dataSend_buff;
  using Ms = std::chrono::milliseconds;
  while (terminationflag && *terminationflag == false)
  {
    image_pipe_info *headImgPipe = NULL;

    while (datViewQueue.pop_blocking(headImgPipe))
    {

      if(1)
      {


        int imgCHID=headImgPipe->StreamInfo.channel_id;
    
        
        // CameraLayer::BasicCameraInfo data=headImgPipe->StreamInfo.camera->getConnectionData();
        // cJSON* caminfo=CameraManager::cameraInfo2Json(data);


        cJSON* camBrifInfo=cJSON_CreateObject();
        cJSON_AddStringToObject(camBrifInfo, "trigger_tag", headImgPipe->trigger_tag.c_str());
        cJSON_AddNumberToObject(camBrifInfo, "trigger_id", headImgPipe->trigger_id);
        cJSON_AddStringToObject(camBrifInfo, "camera_id",headImgPipe->camera_id.c_str());

        bpg_pi.fromUpperLayer_DATA("CM",imgCHID,camBrifInfo);
        cJSON_Delete(camBrifInfo);
        
        
        BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)2};
        iminfo.fullHeight = headImgPipe->img.GetHeight();
        iminfo.fullWidth = headImgPipe->img.GetWidth();
        if(iminfo.scale>1)
        {
          //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
          //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
          ImageDownSampling(dataSend_buff, headImgPipe->img, iminfo.scale, NULL);
        }
        else
        {
          iminfo.scale=1;
          iminfo.img=&headImgPipe->img;
        }
        bpg_pi.fromUpperLayer_DATA("IM",imgCHID,&iminfo);
        bpg_pi.fromUpperLayer_SS(imgCHID,true);
      }
      



      if(headImgPipe->report_json)
      {
        
        for (int i = 0 ; i < cJSON_GetArraySize(headImgPipe->report_json) ; i++)
        {
          cJSON * report = cJSON_GetArrayItem(headImgPipe->report_json, i);
          double* channel_id=JFetch_NUMBER(report,"channel_id");
          int ch_id=channel_id==NULL?0:(int)*channel_id;
          bpg_pi.fromUpperLayer_DATA("RP",ch_id,report);
          bpg_pi.fromUpperLayer_SS(ch_id,true);
        }
        cJSON_Delete(headImgPipe->report_json);
        headImgPipe->report_json=NULL;


      }
      
      LOGI(">>>CAM:%s",headImgPipe->camera_id.c_str());
      //send data to view
      bpg_pi.resPool.retResrc(headImgPipe);
      
    }
  }
}


class InspectionTarget_s :public InspectionTarget
{

public:
  vector <InspectionTarget_s*> *localGroup;
  InspectionTarget_s(std::string id,cJSON* def,vector <InspectionTarget_s*> *localGroup):InspectionTarget(id)
  {
    this->localGroup=localGroup;
    // setInspDef(def);
    // report=NULL;
  }

  virtual bool checkRef()=0;

  virtual acvImage* fetchImage(std::string id)=0;

  virtual cJSON* fetchInspReport()=0;


  virtual void setInspDef(cJSON* def)=0;

  virtual void CAM_CallBack(image_pipe_info *pipe)=0;
  
  virtual ~InspectionTarget_s()
  {
  }

};



class InspectionTarget_s_ColorRegionDetection :public InspectionTarget_s
{
  cJSON* nullOBJ;
  cJSON* report;
public:
  InspectionTarget_s_ColorRegionDetection(std::string id,cJSON* def,vector <InspectionTarget_s*> *localGroup):InspectionTarget_s(id,def,localGroup)
  {
    // this->localGroup=localGroup;
    nullOBJ=NULL;//cJSON_CreateNull();
    setInspDef(def);
    report=NULL;
  }

  bool checkRef()
  {
    return true;
  }

  acvImage* fetchImage(std::string id)
  {
    return NULL;
  }

  cJSON* fetchInspReport()
  {
    // if(report==NULL)
    // {
    //   return nullOBJ;
    // }
    return report;
  }
  void cleanInspReport()
  {
    cJSON_Delete(report);
    report=NULL;
  }


  virtual void setInspDef(cJSON* def)
  {


    //clean up objects
    InspectionTarget::setInspDef(def);
    //build up objects




  }

  void CAM_CallBack(image_pipe_info *pipe)
  {
    // CameraLayer::frameInfo  info=srcCamSi->camera->GetFrameInfo();
    // LOGI("<<<<id:%s<<<%s  WH:%d,%d  timeStamp_us:%" PRId64,id.c_str(),cam_id.c_str(),img.GetWidth(),img.GetHeight(),info.timeStamp_us);
    LOGI("<<<<");
    if(report!=NULL)
    {
      cJSON_Delete(report);
      report=NULL;
    }

    if(pipe->trigger_tag!=JFetch_STRING_ex(def,"trigger_tag","") || pipe->camera_id!=JFetch_STRING_ex(def,"camera_id",""))
    {
      return;
    }
    report=cJSON_CreateObject();

    cv::Mat def_temp_img(pipe->img.GetHeight(),pipe->img.GetWidth(),CV_8UC3,pipe->img.CVector[0]);

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
    if(report)
    {
      cJSON_Delete(report);
      report=NULL;
    }
  }

  virtual InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)
  {
    return NULL;
  }

};



class InspectionTarget_g :public InspectionTarget
{ 
public:
  struct report{
    bool isReady;
    std::string trigger_tag;
    int trigger_id;
    std::string cam_id;
    uint64_t timeStamp_us;
  } ;

  report rep;

  vector<InspectionTarget_s*> subInspList;
  
  cJSON *inspresult=NULL;
  InspectionTarget_g(std::string id,cJSON* def):InspectionTarget(id)
  {
    setInspDef(def);

    char* defStr=cJSON_Print(this->def);
    LOGI("\n%s",defStr);
    delete defStr;
  }


  cJSON* fetchInspReport()
  {
    return inspresult;
  }

  

  void cleanInspReport()
  {
    
    for(int i=0;i<subInspList.size();i++)
    {
      subInspList[i]->cleanInspReport();
    }
    cJSON_Delete(inspresult);
    inspresult=NULL;
  }

  cJSON* genInfo()
  {
    cJSON *obj=cJSON_CreateObject();

    {
      // cJSON *camInfo = cJSON_Parse(camera->getCameraJsonInfo().c_str());
      // cJSON_AddItemToObject(obj, "camera", camInfo);
    }

    {
      cJSON *otherInfo=cJSON_CreateArray();
      cJSON_AddItemToObject(obj, "inspInfo", otherInfo);

      for( auto sInsp:subInspList)
      {
        
        cJSON *info=cJSON_CreateObject();


        cJSON_AddStringToObject(info, "id", sInsp->id.c_str() );
        
        cJSON_AddItemToArray(otherInfo,info);
      }



    }

    {
      cJSON_AddNumberToObject(obj, "channel_id", channel_id);
      cJSON_AddStringToObject(obj, "id", id.c_str());
    }
    return obj;
  }

  cJSON* genInspReport()
  {
    if(rep.isReady==false)return NULL;
    cJSON *inspresult=NULL;
  
    inspresult=cJSON_CreateObject();
    cJSON_AddStringToObject(inspresult,"id",this->id.c_str());
    cJSON_AddNumberToObject(inspresult,"channel_id",this->channel_id);
    cJSON_AddStringToObject(inspresult,"trigger_tag",rep.trigger_tag.c_str());
    cJSON_AddNumberToObject(inspresult,"trigger_id",rep.trigger_id);
    cJSON_AddStringToObject(inspresult,"camera_id",rep.cam_id.c_str());
    cJSON_AddNumberToObject(inspresult,"timeStamp_us",rep.timeStamp_us);


    cJSON* rep_rules=cJSON_CreateArray();
    cJSON_AddItemToObject(inspresult,"rules",rep_rules);

    
    for(int i=0;i<subInspList.size();i++)
    {
      cJSON_AddItemToArray(rep_rules,cJSON_Duplicate(subInspList[i]->fetchInspReport(),true));
    }

    return inspresult;
  }

  InspectionTarget_EXCHANGE excdata={0};
  std::timed_mutex rsclock;
  InspectionTarget_EXCHANGE* exchange(InspectionTarget_EXCHANGE* info)
  {
    rsclock.lock();
    cJSON * json=info->info;

    char *insp_type = JFetch_STRING(json, "insp_type");

    memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));

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

  
  
  virtual void setInspDef(cJSON* def)
  {

    for(int i=0;i<subInspList.size();i++)
    {
      delete subInspList[i];
      subInspList[i]=NULL;
    }
    subInspList.clear();

    //clean up objects
    InspectionTarget::setInspDef(def);
    //build up objects

    if(0){
      char* defStr=cJSON_Print(def);
      LOGI("\n%s",defStr);
      delete defStr;
    }


    
    cJSON* rules=JFetch_ARRAY(def,"rules");
    for (int i = 0 ; i < cJSON_GetArraySize(rules) ; i++)
    {
      cJSON * rule = cJSON_GetArrayItem(rules, i);
    
      std::string type=std::string(JFetch_STRING(rule,"type"));
    
      std::string id=std::string(JFetch_STRING(rule,"id"));

      
      if(type=="ColorRegionLocating")
      {
        InspectionTarget_s* subIT=
          new InspectionTarget_s_ColorRegionDetection(id,rule,&subInspList);
        subInspList.push_back(subIT);
      }
      else
      {
        //failed
      }

    }

    bool is_Ref_OK=true;
    for (int i = 0 ; i < subInspList.size() ; i++)
    {
      if(subInspList[i]->checkRef()==false)
      {
        is_Ref_OK=false;
        break;
      }
    }

  }

  void CAM_CallBack(image_pipe_info *pipe)
  {
    rep.isReady=false;
    // CameraLayer::frameInfo  info=srcCamSi->camera->GetFrameInfo();
    // LOGI("<<<<id:%s<<<%s  WH:%d,%d  timeStamp_us:%" PRId64,id.c_str(),cam_id.c_str(),img.GetWidth(),img.GetHeight(),info.timeStamp_us);

    rep.trigger_tag=pipe->trigger_tag;
    rep.trigger_id=pipe->trigger_id;
    rep.cam_id=pipe->camera_id;
    rep.timeStamp_us=pipe->fi.timeStamp_us;
    
    rep.isReady=true;


    for(int i=0;i<subInspList.size();i++)
    {
      subInspList[i]->CAM_CallBack(pipe);
      // subInspList[i]->fetchInspReport();
    }
    if(inspresult!=NULL)
    {
      cJSON_Delete(inspresult);
      inspresult=NULL;
    }
    inspresult=genInspReport();
  }

  bool returnExchange(InspectionTarget_EXCHANGE* info)
  {
    if(info!=&excdata)return false;

    if(info->info)
      cJSON_Delete( info->info );
    
    memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));
    
    rsclock.unlock();
    return true;
  }
  virtual ~InspectionTarget_g()
  {
    if(inspresult!=NULL)
    {
      cJSON_Delete(inspresult);
    }
  }
};




int PerifChannel::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    char tmp[1024];
    sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s,\"CONN_ID\":%d}", raw, ID);
    // LOGI("MSG:%s", tmp);
    bpg_pi.fromUpperLayer_DATA("PD",conn_pgID,tmp);
    bpg_pi.fromUpperLayer_DATA("SS",conn_pgID,"{}");
    return 0;

  }
  printf(">>opcode:%d\n",opcode);
  return 0;


}





int _argc;
char **_argv;

machine_hash machine_h = {0};
void AttachStaticInfo(cJSON *reportJson, m_BPG_Protocol_Interface *BPG_prot_if)
{
  if (reportJson == NULL)
    return;
  char tmpStr[128];

  {
    char *tmpStr_ptr = tmpStr;
    for (int i = 0; i < sizeof(machine_h.machine); i++)
    {
      tmpStr_ptr += sprintf(tmpStr_ptr, "%02X", machine_h.machine[i]);
    }
    cJSON_AddStringToObject(reportJson, "machine_hash", tmpStr);

    if (BPG_prot_if && BPG_prot_if->cameraFramesLeft >= 0)
    {
      LOGI("BPG_prot_if->cameraFramesLeft:%d", BPG_prot_if->cameraFramesLeft);
      cJSON_AddNumberToObject(reportJson, "frames_left", BPG_prot_if->cameraFramesLeft);
    }
  }
}
// int backPackLoad(FeatureManager_BacPac &calib_bacpac,cJSON *from)
// {
// }

// int backPackDump(FeatureManager_BacPac &calib_bacpac,cJSON *dumoTo)
// {
// }

BGLightNodeInfo extractInfoFromJson(cJSON *nodeRoot) //have exception
{
  if (nodeRoot == NULL)
  {
    char ExpMsg[100];
    sprintf(ExpMsg, "ERROR: extractInfoFromJson error, nodeRoot is NULL");
    throw std::runtime_error(ExpMsg);
  }

  BGLightNodeInfo info;
  info.location.X = *JFetEx_NUMBER(nodeRoot, "location.x");
  info.location.Y = *JFetEx_NUMBER(nodeRoot, "location.y");
  info.index.X = (int)*JFetEx_NUMBER(nodeRoot, "index.x");
  info.index.Y = (int)*JFetEx_NUMBER(nodeRoot, "index.y");

  info.sigma = *JFetEx_NUMBER(nodeRoot, "sigma");
  info.samp_rate = *JFetEx_NUMBER(nodeRoot, "samp_rate");
  info.mean = *JFetEx_NUMBER(nodeRoot, "mean");
  info.error = *JFetEx_NUMBER(nodeRoot, "error");

  return info;
}

int m_BPG_Protocol_Interface::toUpperLayer(BPG_protocol_data bpgdat) 
{
  //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);

    BPG_protocol_data *dat = &bpgdat;

    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //      dat->pgID);
    cJSON *json = cJSON_Parse((char *)dat->dat_raw);
    char err_str[500] = "\0";
    bool session_ACK = false;
    char tmp[200];    //For string construct json reply
  do
  {

    // if (checkTL("GS", dat) == false)
    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //       dat->pgID);
    if (checkTL("HR", dat))
    {
      LOGI("DataType_BPG>>>>%s", dat->dat_raw);

      LOGI("Hello ready.......");
      session_ACK = true;
    }
    else if (checkTL("SV", dat)) //Data from UI to save file
    {
      LOGI("DataType_BPG>>STR>>%s", dat->dat_raw);

      if (json == NULL)
      {
        snprintf(err_str, sizeof(err_str), "JSON parse failed");
        LOGE("%s", err_str);
        break;
      }
      do
      {

        char *fileName = (char *)JFetch(json, "filename", cJSON_String);
        if (fileName == NULL)
        {
          snprintf(err_str, sizeof(err_str), "No entry:'filename' in it");
          LOGE("%s", err_str);
          break;
        }

        {

          char dirPath[200];
          strcpy(dirPath, fileName);
          char *dir = dirname(dirPath);
          bool dirExist = isDirExist(dir);

          if (dirExist == false && getDataFromJson(json, "make_dir", NULL) == cJSON_True)
          {
            int ret = cross_mkdir(dir);
            dirExist = isDirExist(dir);
          }
          if (dirExist == false)
          {
            snprintf(err_str, sizeof(err_str), "No Dir %s exist", dir);
            LOGE("%s", err_str);
            break;
          }
        }

        LOGE("fileName: %s", fileName);
        int strinL = strlen((char *)dat->dat_raw) + 1;

        if (dat->size - strinL == 0)
        { //No raw data, check "type"

          char *type = (char *)JFetch(json, "type", cJSON_String);
          if (strcmp(type, "__CACHE_IMG__") == 0)
          {
            LOGE("__CACHE_IMG__ %d x %d", cacheImage.GetWidth(), cacheImage.GetHeight());
            if (cacheImage.GetWidth() * cacheImage.GetHeight() > 10) //HACK: just a hacky way to make sure the cache image is there
            {
              SaveIMGFile(fileName, &cacheImage);
              session_ACK=true;
            }
            else
            {
              session_ACK = false;
            }
          }
        
        }
        else
        {

          LOGI("DataType_BPG>>BIN>>%s", byteArrString(dat->dat_raw + strinL, dat->size - strinL));

          FILE *write_ptr;

          write_ptr = fopen(fileName, "wb"); // w for write, b for binary
          if (write_ptr == NULL)
          {
            snprintf(err_str, sizeof(err_str), "file:%s File open failed", fileName);
            LOGE("%s", err_str);
            break;
          }
          fwrite(dat->dat_raw + strinL, dat->size - strinL, 1, write_ptr); // write 10 bytes from our buffer

          fclose(write_ptr);
          session_ACK = true;
        }

      } while (false);
    }
    else if (checkTL("FB", dat)) //[F]ile [B]rowsing
    {

      do
      {

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed");
          LOGE("%s", err_str);
          break;
        }

        char *pathStr = (char *)JFetch(json, "path", cJSON_String);
        if (pathStr == NULL)
        {
          //ERROR
          snprintf(err_str, sizeof(err_str), "No 'path' entry in the JSON");
          LOGE("%s", err_str);
          break;
        }

        int depth = 1;
        double *p_depth = JFetch_NUMBER(json, "depth");
        if (p_depth != NULL)
        {
          depth = (int)*p_depth;
        }
        LOGI("DEPTH:%d",depth);
        {
          cJSON *cjFileStruct = cJSON_DirFiles(pathStr, NULL, depth);

          char *fileStructStr = NULL;

          if (cjFileStruct == NULL)
          {
            cjFileStruct = cJSON_CreateObject();
            snprintf(err_str, sizeof(err_str), "File Structure is NULL");
            LOGI("W:%s", err_str);

            session_ACK = false;
          }
          else
          {

            session_ACK = true;
          }

          fromUpperLayer_DATA("FS",dat->pgID,cjFileStruct);
          cJSON_Delete(cjFileStruct);
        }

      } while (false);
    }
    else if (checkTL("CM", dat)) //[C]amera [M]anager
    {
      session_ACK = false;
      char *type_str = JFetch_STRING(json, "type");
      if (strcmp(type_str, "discover") ==0)
      {
        session_ACK = true;

        string discoverInfo_str = CameraManager::cameraDiscovery(JFetch_TRUE(json, "do_refresh"));
            
        cJSON *discoverInfo = cJSON_Parse(discoverInfo_str.c_str());
        fromUpperLayer_DATA("CM",dat->pgID,discoverInfo);
        cJSON_Delete(discoverInfo);
      }
      else if(strcmp(type_str, "connect") ==0)
      {do{
        
        session_ACK = false;

        string miscStr="";
        {
          char* misc=JFetch_STRING(json, "misc");
          if(misc)
          {
            string str(misc);
            miscStr=misc;
          }
        }
        
        
        double *cam_idx = JFetch_NUMBER(json, "idx");
        CameraManager::StreamingInfo* gcami=NULL;
        if(cam_idx!=NULL)
        {

          gcami = inspTarMan.camman.addCamera((int)*cam_idx,miscStr,InspectionTargetManager::sCAM_CallBack,&inspTarMan);

        }
        else
        {
          char *_driver_name = JFetch_STRING(json, "driver_name");
          std::string driver_name=_driver_name==NULL?"":std::string(_driver_name);
          char *_cam_id = JFetch_STRING(json, "id");
          std::string cam_id=_cam_id==NULL?"":std::string(_cam_id);
          if(_cam_id)
          {
            gcami = inspTarMan.camman.addCamera(driver_name,cam_id,miscStr,InspectionTargetManager::sCAM_CallBack,&inspTarMan);
          }
        }

        
        if(gcami)
        {
          gcami->camera->TriggerMode(1);
          CameraLayer::BasicCameraInfo data=gcami->camera->getConnectionData();
          gcami->camera->SetExposureTime(20000);
          cJSON* info = inspTarMan.camman.cameraInfo2Json(data);
          LOGI("dat->pgID:%d",dat->pgID);
          gcami->channel_id=dat->pgID;
          
          fromUpperLayer_DATA("CM",dat->pgID,info);
          cJSON_Delete(info);

        }
        // LOGI(">>>insptar:%p",insptar);
        session_ACK = (gcami!=NULL);



          
        
      }while(0);}
      
      else if(strcmp(type_str, "disconnect") ==0)
      {do{
      
        session_ACK = false;
        char *_driver_name = JFetch_STRING(json, "driver_name");
        std::string driver_name=_driver_name==NULL?"":std::string(_driver_name);
        char *_cam_id = JFetch_STRING(json, "id");
        std::string cam_id=_cam_id==NULL?"":std::string(_cam_id);
        if(_cam_id)
        {

          session_ACK = inspTarMan.camman.delCamera(driver_name,cam_id);
        }

      }while(0);}
      else if(strcmp(type_str, "set_camera_channel_id") ==0)
      {do{
      
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id==NULL)break;
        std::string id=std::string(_cam_id);
        CameraManager::StreamingInfo *cami = inspTarMan.camman.getCamera("",id);
        if(cami==NULL)break;
        cami->channel_id=dat->pgID;

        session_ACK = true;

      }while(0);}
      else if(strcmp(type_str, "connected_camera_list") ==0)
      {do{
      
        session_ACK = true;

        
        cJSON* camList = inspTarMan.camman.ConnectedCameraList();

        fromUpperLayer_DATA("CM",dat->pgID,camList);
        cJSON_Delete(camList);


      }while(0);}

      else if(strcmp(type_str, "start_stream") ==0)
      {do{
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id==NULL)break;
        std::string id=std::string(_cam_id);
        CameraManager::StreamingInfo * cami = inspTarMan.camman.getCamera("",id);
        if(cami==NULL)break;
        cami->camera->TriggerMode(0);

        session_ACK = true;
      }while(0);}
      else if(strcmp(type_str, "stop_stream") ==0)
      {do{
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id==NULL)break;
        std::string id=std::string(_cam_id);
        CameraManager::StreamingInfo * cami = inspTarMan.camman.getCamera("",id);
        if(cami==NULL)break;
        cami->camera->TriggerMode(1);

        session_ACK = true;
      }while(0);}

      else if(strcmp(type_str, "setting") ==0)
      {do{
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id==NULL)break;
        std::string id=std::string(_cam_id);
        CameraManager::StreamingInfo * cami = inspTarMan.camman.getCamera("",id);
        if(cami==NULL)break;

        LOGI("CAMERA:%s",id.c_str());

        double *exposure = JFetch_NUMBER(json, "exposure");
        if(exposure)
        {
          cami->camera->SetExposureTime(*exposure);
          LOGI("Exposure:%f",*exposure);
        }


        {
          double *val = JFetch_NUMBER(json, "trigger_mode");
          if (val)
          {
            cami->camera->TriggerMode((int)*val);
            // retV = 0;
          }
        }



        double *analog_gain = JFetch_NUMBER(json, "analog_gain");
        if(analog_gain)
        {
          cami->camera->SetAnalogGain(*analog_gain);
          LOGI("analog_gain:%f",*analog_gain);
        }

        {

          if(JFetch_TRUE(json, "WB_ONCE"))
          {
            cami->camera->SetOnceWB();
          }


          double *RGain = JFetch_NUMBER(json, "RGain");
          if(RGain)
          {
            cami->camera->SetRGain(*RGain);
            LOGI("RGain:%f",*RGain);
          }

          double *GGain = JFetch_NUMBER(json, "GGain");
          if(GGain)
          {
            cami->camera->SetGGain(*GGain);
            LOGI("GGain:%f",*GGain);
          }

          double *BGain = JFetch_NUMBER(json, "BGain");
          if(BGain)
          {
            cami->camera->SetBGain(*BGain);
            LOGI("BGain:%f",*BGain);
          }


          
          double *frame_rate = JFetch_NUMBER(json, "frame_rate");
          if(frame_rate)
          {
            cami->camera->SetFrameRate (*frame_rate);
            LOGI("SetFrameRate:%f",*frame_rate);
          }

          double *black_level = JFetch_NUMBER(json, "black_level");
          if(black_level)
          {
            cami->camera->SetBalckLevel (*black_level);
            LOGI("black_level:%f",*black_level);
          }

          if(JFetch_TRUE(json, "mirrorX"))
          {
            cami->camera->SetMirror(0,1);
          }
          if(JFetch_FALSE(json, "mirrorX"))
          {
            cami->camera->SetMirror(0,0);
          }

          if(JFetch_TRUE(json, "mirrorY"))
          {
            cami->camera->SetMirror(1,1);
          }
          if(JFetch_FALSE(json, "mirrorY"))
          {
            cami->camera->SetMirror(1,0);
          }

        }



        session_ACK = true;
      }while(0);}
      else if(strcmp(type_str, "clean_trigger_info_matching_buffer") ==0)
      {do{
        cleanUp_triggerInfoMatchingBuffer_UNSAFE();
        LOGI("cleanUp complete!!");
        session_ACK = true;
      }while(0);}
      else if(strcmp(type_str, "trigger") ==0)
      {do{
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id==NULL)break;
        std::string id=std::string(_cam_id);
        char *_img_path = JFetch_STRING(json, "img_path");
        if(_img_path)
        {
          
          char *_trigger_tag = JFetch_STRING(json, "trigger_tag");
          double *_trigger_id = JFetch_NUMBER(json, "trigger_id");
          double channel_id = JFetch_NUMBER_ex(json, "channel_id");

          if(channel_id!=channel_id)
          {
            CameraManager::StreamingInfo * cami = inspTarMan.camman.getCamera("",id);
            if(cami)
            {
              channel_id=cami->channel_id;
            }
          }

          if(_trigger_tag==NULL ||  _trigger_id==NULL || channel_id!=channel_id)
          {
            sprintf(err_str, "trigger_tag:%p trigger_id:%p channel_id:%f", _trigger_tag,_trigger_id,channel_id);
            LOGI("%s",err_str);
            break;
          }
          
          LOGI("_img_path:%s _cam_id:%s _trigger_tag:%s",_img_path,_cam_id,_trigger_tag);
          LOGI("_trigger_id:%d _channel_id:%d",(int)*_trigger_id,(int)channel_id);
          int ret=
            ReadImageAndPushToInspQueue(
              std::string(_img_path),
              id,
              std::string(_trigger_tag),
              (int)*_trigger_id,
              (int)channel_id);
        }
        else
        {

          if(JFetch_TRUE(json, "soft_trigger"))
          {
            {
              auto xx = inspTarMan.camman.ConnectedCamera_ex();
              for(auto cam : xx)
              {
                LOGI(">>CAM>%s",cam.camera->getConnectionData().id.c_str());
              }
            }
            CameraManager::StreamingInfo * cami = inspTarMan.camman.getCamera("",id);
            if(cami)
            {

              LOGI("cami:%p",cami);

              // cami->camera->TriggerMode(1);
              CameraLayer::status st=cami->camera->Trigger();
              LOGI("Trigger:%d",st);
            }
          }

          char *_trigger_tag = JFetch_STRING(json, "trigger_tag");
          std::string trigger_tag=_trigger_tag==NULL?"SW_TRIG":std::string(_trigger_tag);
          LOGI(">>>trigger_tag:%s",trigger_tag.c_str());
          {
            
            pipetriggerInfo_mix mocktrig;
            mocktrig.pipeInfo=NULL;
            mocktrig.triggerInfo.camera_id=id;
            double *_trigger_id = JFetch_NUMBER(json, "trigger_id");
            mocktrig.triggerInfo.trigger_id=(_trigger_id==NULL)?-1:(int)*_trigger_id;
            mocktrig.triggerInfo.est_trigger_time_us=0;//force matching
            mocktrig.triggerInfo.trigger_tag=trigger_tag;
            
            triggerInfoMatchingQueue.push_blocking(mocktrig);
          }




          session_ACK = true;
      
        }






      }while(0);}
    }    
    else if (checkTL("IT", dat)) //[I]nsp [T]arget
    {do{
      
      session_ACK = false;
      char *type_str = JFetch_STRING(json, "type");
      if(strcmp(type_str, "create") ==0)
      {
        char *_id = JFetch_STRING(json, "id");
        if(_id==NULL)break;
        std::string id=std::string(_id);

        if(inspTarMan.getInspTar(id)!=NULL)
        {
          snprintf(err_str, sizeof(err_str), "InspTar: id:%s is already existed", _id);
          break;
        }



        
        char* insp_type=JFetch_STRING(json, "definfo.type");
        InspectionTarget* inspTar=NULL;
        if(true || strcmp(insp_type, "m") ==0)
        {
          inspTar=new InspectionTarget_g(id,JFetch_OBJECT(json, "definfo"));
        }

        if(inspTar)
        {
          inspTar->channel_id=dat->pgID;
          session_ACK=inspTarMan.addInspTar(inspTar,id);
        }

        if(session_ACK==false)delete inspTar;
      }
      else if(strcmp(type_str, "delete") ==0)
      {
        
        char *_id = JFetch_STRING(json, "id");
        if(_id==NULL)break;
        std::string id=std::string(_id);
        session_ACK=inspTarMan.delInspTar(id);
        
      }
      else if(strcmp(type_str, "delete_all") ==0)
      {
        session_ACK=inspTarMan.clearInspTar();
      }
      else if(strcmp(type_str, "list") ==0)
      {
        
        
            
        cJSON *IT_list =inspTarMan.genInspTarListInfo();
        fromUpperLayer_DATA("IT",dat->pgID,IT_list);
        cJSON_Delete(IT_list);
        
      }
      else if(strcmp(type_str, "exchange") ==0)
      {
        
        char *_id = JFetch_STRING(json, "id");
        if(_id==NULL)break;
        std::string id=std::string(_id);
        InspectionTarget *insptar= inspTarMan.getInspTar(id);

      }
    }while(0);}
    else if (checkTL("GS", dat)) //[G]et [S]etting
    {
      
      session_ACK = false;
      cJSON *items = JFetch_ARRAY(json, "items");
      if (items != NULL)
      {
        session_ACK = true;

        cJSON *retArr = cJSON_CreateObject();
        char chBuff[120];
        session_ACK = true;

        for (int k = 0;; k++)
        {
          sprintf(chBuff, "items[%d]", k);
          char *itemType = JFetch_STRING(json, chBuff);
          if (itemType == NULL)
            break;
          if (strcmp(itemType, "binary_path") == 0)
          {
            realfullPath(_argv[0], chBuff);
            cJSON_AddStringToObject(retArr, itemType, chBuff);
          }
          else if (strcmp(itemType, "data_path") == 0)
          {
            realfullPath("./", chBuff);
            cJSON_AddStringToObject(retArr, itemType, chBuff);
          }
        }

        fromUpperLayer_DATA("GS",dat->pgID,retArr);
        cJSON_Delete(retArr);
      }
    }
    else if (checkTL("LD", dat)) //[L]oa[D] file
    {
      
      session_ACK = true;
      LOGI("DataType_BPG:[%c%c] data:\n%s", dat->tl[0], dat->tl[1],(char *)dat->dat_raw);
      do
      {

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          session_ACK=false;
          break;
        }

        char *filename = (char *)JFetch(json, "filename", cJSON_String);
        if (filename != NULL)
        {
          try
          {
            char *fileStr = ReadText(filename);
            if (fileStr == NULL)
            {
              snprintf(err_str, sizeof(err_str), "Cannot read file from:%s", filename);
              LOGE("%s", err_str);
              session_ACK=false;
              break;
            }
            LOGI("Read deffile:%s", filename);
            fromUpperLayer_DATA("FL",dat->pgID, fileStr);
            free(fileStr);
          }
          catch (std::invalid_argument iaex)
          {
            snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
            LOGE("%s", err_str);
            session_ACK=false;
            break;
          }
        }

        char *imgSrcPath = (char *)JFetch(json, "imgsrc", cJSON_String);
        if (imgSrcPath != NULL)
        {

          int ret_val = LoadIMGFile(&tmp_buff, imgSrcPath);
          if (ret_val == 0)
          {
            acvImage *srcImg = NULL;
            srcImg = &tmp_buff;
            cacheImage.ReSize(srcImg);
            acvCloneImage(srcImg, &cacheImage, -1);

            int default_scale = 2;

            double *DS_level = JFetch_NUMBER(json, "down_samp_level");
            if (DS_level)
            {
              default_scale = (int)*DS_level;
              if (default_scale <= 0)
                default_scale = 1;
            }
            //TODO:HACK: 4 times scale down for transmission speed, bpg_dat.scale is not used for now

            BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)default_scale};

            iminfo.fullHeight = srcImg->GetHeight();
            iminfo.fullWidth = srcImg->GetWidth();
            //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
            //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff, *srcImg, iminfo.scale, NULL);

            fromUpperLayer_DATA("IM",dat->pgID,&iminfo);


          }
          else
          {
            
            session_ACK=false;
            break;
          }
        }


      } while (false);
    }
    else if (checkTL("PD", dat)) //[P]eripheral [D]evice
    {
      char *type = JFetch_STRING(json, "type");

      double *_CONN_ID = JFetch_NUMBER(json, "CONN_ID");
      int CONN_ID=-1;
      if(_CONN_ID)
      {
        CONN_ID=(int)*_CONN_ID;
      }


      do{
        if(strcmp(type, "CONNECT") == 0)
        {
          if(CONN_ID!=-1)
          {
            sprintf(err_str, "CONNECT should not have CONN_ID(%d)", CONN_ID);
            break;
          }

          
          delete_PeripheralChannel();
          // char *conn_type = JFetch_STRING(json, "type");

          // if(strcmp(conn_type, "uart") == 0)
          // {
            
          // }
          // else if(strcmp(conn_type, "IP") == 0 || conn_type==NULL)
          // {
            
          // }
          
          int avail_CONN_ID=714;
          Data_Layer_IF *PHYLayer=NULL;
          char *uart_name = NULL;
          
          char *IP = NULL;
          if ( (uart_name=JFetch_STRING(json, "uart_name")) !=NULL)
          {
            double *baudrate = JFetch_NUMBER(json, "baudrate");
            char *default_mode="8N1";
            char *mode = JFetch_STRING(json, "mode");
            if(mode==NULL)
            {
              mode=default_mode;
            }

            if(baudrate==NULL)
            {
              sprintf(err_str, "baudrate is not defined");
              break;
            }



            try{
              
              PHYLayer=new Data_UART_Layer(uart_name,(int)*baudrate, mode);


            }
            catch(std::runtime_error &e){
             
            }

          }
          else if( (IP=JFetch_STRING(json, "ip"))!=NULL)
          {

            double *port_number = JFetch_NUMBER(json, "port");
            if (port_number == NULL)
            {
              sprintf(err_str, "IP(%d) port_number(%d)", IP!=NULL,port_number!=NULL);
              break;
            }
          

            try{
              
              PHYLayer=new Data_TCP_Layer(IP,(int)*port_number);

            }
            catch(std::runtime_error &e){
            }



          }

          if(PHYLayer!=NULL)
          {
            perifCH=new PerifChannel();
            perifCH->ID=avail_CONN_ID;
            perifCH->conn_pgID=dat->pgID;
            perifCH->setDLayer(PHYLayer);

            perifCH->send_RESET();
            perifCH->send_RESET();
            perifCH->RESET();


            session_ACK = true;

            sprintf(tmp, "{\"type\":\"CONNECT\",\"CONN_ID\":%d}", avail_CONN_ID);
            fromUpperLayer_DATA("PD",dat->pgID,tmp);

          }
          else
          {
            session_ACK = false;

            LOGE("PHYLayer is not able to eatablish");
            sprintf(err_str, "PHYLayer is not able to eatablish");
          }

          // if(perifCH!=NULL)
          // {
          //   sprintf(err_str, "perifCH still in connected state");
          //   break;
          // }


        }
        else if(strcmp(type, "DISCONNECT") == 0)
        {

          if(perifCH==NULL || perifCH->ID != CONN_ID)
          {
            sprintf(err_str, "CONN_ID(%d)  perifCH exist:%p or current perifCH has different CONN_ID", CONN_ID, perifCH);
            break;
          }
          
          if(CONN_ID==-1 || perifCH->ID == CONN_ID)
          {//disconnect
            delete_PeripheralChannel();
            session_ACK = true;
          }
          else
          {
            sprintf(err_str, "CONN_ID(%d)  dose not match ", CONN_ID);
            break;
          }


        }
        else if(strcmp(type, "MESSAGE") == 0)
        {
          if(CONN_ID==-1 || perifCH==NULL ||perifCH->ID != CONN_ID)
          {
            sprintf(err_str, "CONN_ID(%d)  perifCH exist:%d or current perifCH has different CONN_ID", CONN_ID, perifCH!=NULL);
            break;
          }

          cJSON *msg_obj = JFetch_OBJECT(json, "msg");
          if (msg_obj)
          {
            uint8_t _buf[2000];
            int ret= sendcJSONTo_perifCH(perifCH,_buf, sizeof(_buf),true,msg_obj);
            session_ACK = (ret>=0);
          }
          else
          {
            session_ACK=true;//send nothing
          }


        }
      }while(false);


    }
    sprintf(tmp, "{\"start\":false,\"cmd\":\"%c%c\",\"ACK\":%s,\"errMsg\":\"%s\"}",
            dat->tl[0], dat->tl[1], (session_ACK) ? "true" : "false", err_str);
    fromUpperLayer_DATA("SS",dat->pgID,tmp);
    cJSON_Delete(json);
  }
  while(0);
  // if (doExit)
  // {
  //   exit(0);
  // }

  return 0;
}

int sendResultTo_perifCH(PerifChannel *perifCH,int uInspStatus, uint64_t timeStamp_100us,int count)
{
  uint8_t buffx[200];
  
  int ret= printfTo_perifCH(perifCH,buffx, sizeof(buffx),true,
    "{"
    "\"type\":\"inspRep\",\"status\":%d,"
    "\"idx\":%d,\"count\":%d,"
    "\"time_100us\":%lu"
    "}", uInspStatus, 1, count, timeStamp_100us);
  return ret;
}


          

int m_BPG_Link_Interface_WebSocket::ws_callback(websock_data data, void *param)
{
  // LOGI(">>>>data.type:%d",data.type);
  // printf("%s:BPG_Link_Interface_WebSocket type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());



  switch(data.type)
  {
    case websock_data::OPENING:
      if (default_peer != NULL && default_peer != data.peer)
      {
        disconnect(data.peer->getSocket());
        return 1;
      }
    break;  
    case websock_data::CLOSING:
    case websock_data::ERROR_EV: 
    {
      if (data.peer == default_peer)
      {
        default_peer = NULL;
      LOGI("CLOSING peer %s:%d\n",
            inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
      bpg_pi.cameraFramesLeft = 0;
      if(bpg_pi.camera!=NULL)
      {
        bpg_pi.camera->TriggerMode(1);
      }
      bpg_pi.delete_PeripheralChannel();
    }


    }
    return 0;

    case websock_data::HAND_SHAKING_FINISHED:
    {
      
      LOGI("OPENING peer %s:%d  sock:%d\n",
           inet_ntoa(data.peer->getAddr().sin_addr),
           ntohs(data.peer->getAddr().sin_port), data.peer->getSocket());

      if (default_peer == NULL)
      {
        default_peer = data.peer;
        
        BPG_protocol_data bpg_dat = bpg_pi.GenStrBPGData("HR", "{\"version\":\"" _VERSION_ "\"}"); //[F]older [S]truct
        bpg_dat.pgID = 0xFF;
        bpg_pi.fromUpperLayer(bpg_dat);
      }
    }
    return 0;

    case websock_data::DATA_FRAME:
    {
      data.data.data_frame.raw[data.data.data_frame.rawL] = '\0';
      // LOGI(">>>>data raw:%s", data.data.data_frame.raw);
      if (bpg_prot)
      {
        toUpperLayer(data.data.data_frame.raw, data.data.data_frame.rawL, data.data.data_frame.isFinal);
      }
      else
      {
        return -1;
      }
    }
    return 0;

  }

  return -3;
}



bool terminationFlag = false;
int mainLoop(bool realCamera = false)
{
  /**/

  LOGI(">>>>>\n");
  bool pass = false;
  int retryCount = 0;
  while (!pass && !terminationFlag)
  {
    try
    {
      int port = 4090;
      LOGI("Try to open websocket... port:%d\n", port);
      ifwebsocket=new m_BPG_Link_Interface_WebSocket(port);

      //
      pass = true;
    }
    catch (exception &e)
    {
      retryCount++;
      int delaySec = 5;
      LOGE("websocket server open retry:%d wait for %dsec", retryCount, delaySec);
      std::this_thread::sleep_for(std::chrono::milliseconds(delaySec * 1000));
    }
  }




  std::thread TrigInfMatThread(TriggerInfoMatchingThread, &terminationFlag);
  setThreadPriority(TrigInfMatThread, SCHED_RR, -20);

  std::thread InspThread(ImgPipeProcessThread, &terminationFlag);
  setThreadPriority(InspThread, SCHED_RR, -20);
  std::thread ActionThread(ImgPipeDatViewThread, &terminationFlag);
  setThreadPriority(ActionThread, SCHED_RR, 0);
  // std::thread _inspSnapSaveThread(InspSnapSaveThread, &terminationFlag);
  // setThreadPriority(_inspSnapSaveThread, SCHED_RR, 19);






  ifwebsocket->setUpperLayer(&bpg_pi);
  bpg_pi.setLink(ifwebsocket);
  // mjpegS = new MJPEG_Streamer2(7603);
  LOGI("SetEventCallBack is set...");

  int count=0;
  while (1)
  {


    // if(clientSMEM_SEND_CH)
    // {
    //   sprintf((char*)clientSMEM_SEND_CH->getPtr(),">>>%d",count++);
    //   clientSMEM_SEND_CH->s_post();
    //   clientSMEM_SEND_CH->s_wait_remote();
    // }
    // LOGI("GO RECV");
    // mjpegS->fdEventFetch(&fdset);

    // LOGI("WAIT..");
    fd_set fd_s = ifwebsocket->get_fd_set();
    int maxfd = ifwebsocket->findMaxFd();
    if (select(maxfd + 1, &fd_s, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }

    ifwebsocket->runLoop(&fd_s, NULL);
  }

  return 0;
}
void sigroutine(int dunno)
{ /* 信號處理常式，其中dunno將會得到信號的值 */
  switch (dunno)
  {
  case SIGINT:
    LOGE("Get a signal -- SIGINT \n");
    LOGE("Tear down websocket.... \n");
    delete ifwebsocket;
    
    terminationFlag = true;
    LOGE("SIGINT exit.... \n");
    break;
  }
  return;
}


#include <vector>
int cp_main(int argc, char **argv)
{
  // {

  //   tmpMain();
  // }

  srand(time(NULL));

  // calib_bacpac.sampler = new ImageSampler();
  // neutral_bacpac.sampler = new ImageSampler();


/*auto lambda = []() { LOGV("Hello, Lambda"); };
  lambda();*/
#ifdef __WIN32__
  {
    WSADATA wsaData;
    int iResult;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
      printf("WSAStartup failed with error: %d\n", iResult);
      return 1;
    }

    LOGI("WIN32 WSAStartup ret:%d", iResult);
  }

  char buffer[512]; //force output run with buffer mode(print when buffer is full) instead of line buffered mode
  //this speeds up windows print dramaticlly
  setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

#endif

  _argc = argc;
  _argv = argv;
  for (int i = 0; i < argc; i++)
  {
    bool doMatch = false;
    char *str = PatternRest(argv[i], "CamInitStyle=");//CamInitStyle={str}
    if(str)
    {
      
      if (strcmp(str, "0") == 0)
      {
        doMatch=true;
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
      }
    }

    str = PatternRest(argv[i], "chdir=");
    if(str!=NULL)
    {
      LOGI("parse....   chdir=%s",str);
      doMatch=true;
      chdir(str);
    }

    if (doMatch)
    {
      LOGE("CMD param[%d]:%s ...OK", i, argv[i]);
    }
    else
    {
      LOGE("unknown param[%d]:%s", i, argv[i]);
    }
  }

  signal(SIGINT, sigroutine);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
