#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include<sys/time.h>
//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include <future>
#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <map>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include "CameraLayerManager.hpp"

#include "InspectionTarget.hpp"
#include "InspTar_SpriteDraw.hpp"
#include "InspTar_ImgSrc.hpp"
#include "InspTars.hpp"
#include "RingBuf.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
// #include <Python.h>
using namespace cv;

// #include <pybind11/pybind11.h>
// #include <pybind11/embed.h>
// namespace py = pybind11;
#define _VERSION_ "1.2"
std::timed_mutex mainThreadLock;


m_BPG_Protocol_Interface bpg_pi;



m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;

struct sttriggerInfo_mix{
  std::shared_ptr<StageInfo_Image>stInfo;
  
  struct _triggerInfo{

    int trigger_id;
    // std::string trigger_tag;
    std::vector<std::string> tags;
    std::string camera_id;
    uint64_t est_trigger_time_us;
    int64_t trigger_time_match_error_thres_us;
  };
  _triggerInfo triggerInfo;
};
// vector<sttriggerInfo_mix> triggerInfoBuffer(10);
// TSQueue<sttriggerInfo_mix> triggerInfoQueue(10);





TSQueue<sttriggerInfo_mix> triggerInfoMatchingQueue(100);
TSVector<sttriggerInfo_mix> triggerInfoMatchingBuffer;




TSQueue<std::shared_ptr<StageInfo_Image>> inspQueue(100);
// TSQueue<image_pipe_info *> datViewQueue(10);
// TSQueue<image_pipe_info *> inspSnapQueue(5);


int CAM1_Counter=0;
int CAM2_Counter=0;

uint64_t lastImgSendTime=0;

class InspectionTargetManager_m:public InspectionTargetManager
{
  void CamStream_CallBack(CameraManager::StreamingInfo &info){


    
    uint64_t cur_ms = current_time_ms();
    uint64_t cur_Interval =cur_ms-lastImgSendTime;
    if(lastImgSendTime==0)
    {
      cur_Interval=1;
    }
    lastImgSendTime=cur_ms;


    
    // LOGE("============DO INSP>> waterLvL: insp:%d/%d dview:%d/%d  snap:%d/%d   poolSize:%d trigInfoMatchingSize:%d",
    //     inspQueue.size(), inspQueue.capacity(),
    //     datViewQueue.size(), datViewQueue.capacity(),
    //     inspSnapQueue.size(), inspSnapQueue.capacity(),
    //     bpg_pi.resPool.rest_size(),triggerInfoMatchingBuffer.size());

    std::shared_ptr<StageInfo_Image> newStateInfo(new StageInfo_Image());

    CameraLayer::frameInfo finfo = info.camera->GetFrameInfo();

    
    
    LOGE("============DO INSP>> waterLvL: insp:%d/%d  trigInfoMatchingSize:%d  cur_Interval:%" PRIu64 "<<<tstmp_ms:%" PRIu64 "   from:%s",
        inspQueue.size(), inspQueue.capacity(),triggerInfoMatchingBuffer.size(),cur_Interval,finfo.timeStamp_us,info.camera->getConnectionData().id.c_str());


    std::shared_ptr<acvImage> img(new acvImage(finfo.width,finfo.height,3));
    CameraLayer::status st = info.camera->ExtractFrame(img->CVector[0],3,finfo.width*finfo.height);

    newStateInfo->img_prop.StreamInfo=info;
    newStateInfo->source=NULL;//info.camera->getConnectionData().id;
    newStateInfo->source_id=info.camera->getConnectionData().id;
    newStateInfo->img_prop.fi=finfo;
    // if(info.channel_id)
    // {
    //   newStateInfo->jInfo=cJSON_CreateObject();
    //   cJSON* streaming_info=cJSON_CreateObject();


    //   cJSON_AddItemToObject(newStateInfo->jInfo,"streaming_info",streaming_info);
    //   cJSON_AddNumberToObject(streaming_info, "channel_id",info.channel_id);
    // }
    
    // newStateInfo->trigger_tag="";
    newStateInfo->img_show=
    newStateInfo->img=img;
    // LOGI(">>>CAM:%s  WH:%d %d",info->camera_id.c_str(),finfo.width,finfo.height);

    sttriggerInfo_mix pmix;
    pmix.stInfo=newStateInfo;
    
      LOGI("0");
    triggerInfoMatchingQueue.push_blocking(pmix);
      LOGI("1");

  }

};


int ReadImageAndPushToInspQueue(string path,vector<string> trigger_tags,int trigger_id,int channel_id)
{
  
  std::shared_ptr<StageInfo_Image> newStateInfo(new StageInfo_Image());
  if(newStateInfo==NULL)return -1;

  newStateInfo->img_prop.StreamInfo.camera=NULL;
  newStateInfo->img_prop.StreamInfo.channel_id=channel_id;
  newStateInfo->trigger_tags=trigger_tags;

  Mat mat=imread(path.c_str());

  int H = mat.rows;
  int W = mat.cols;
  CameraLayer::frameInfo finfo;
  finfo.offset_x=finfo.offset_y=0;
  finfo.height=H;
  finfo.width=W;
  finfo.timeStamp_us=0;
  finfo.pixel_size_mm=NAN;

  newStateInfo->img_prop.mmpp=0;
  newStateInfo->img_prop.fi=finfo;

  std::shared_ptr<acvImage> img(new acvImage(W,H,3));
  newStateInfo->img=img;
  newStateInfo->trigger_id=trigger_id;
  
  cv::Mat dst_mat(H,W,CV_8UC3,img->CVector[0]);

  


  if(mat.channels()==3)
  {
    mat.copyTo(dst_mat);
  }
  else if(mat.channels()==1)
  {
    cv::cvtColor(mat,dst_mat,COLOR_GRAY2RGB);
  }
  else
  { //TODO: recycle the newStateInfo
    return -2;
  }
  LOGE("inspQueue.size()=%d ",inspQueue.size());
  inspQueue.push_blocking(newStateInfo);

  return 0;
}





InspectionTargetManager_m inspTarMan;



std::map<std::string, std::string> triggerMockFlags;

bool cleanUp_triggerInfoMatchingBuffer_UNSAFE()
{
  int zeroCount=0;
  LOGI("Wait for kill.....");
  for(int i=0;i<10;i++)
  {
    if(inspTarMan.isAllInspTarBufferClear())
    {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  LOGI("Ready to kill..... buffersize:%d",triggerInfoMatchingBuffer.size());
  for(int i=0;i<triggerInfoMatchingBuffer.size();i++)
  {
    if(triggerInfoMatchingBuffer[i].stInfo!=NULL)
    {

      LOGE("[%d] SRC:%s",i,triggerInfoMatchingBuffer[i].stInfo->source_id.c_str());
      
      // bpg_pi.resPool.retResrc(triggerInfoMatchingBuffer[i].pipeInfo);


      //since triggerInfoMatchingBuffer[i].stInfo is not yet dispatched, it shouldn't use recycle to delete, so deal(DELETE) it here 
      // inspTarMan.recycleStageInfo(triggerInfoMatchingBuffer[i].stInfo);
      triggerInfoMatchingBuffer[i].stInfo=NULL;
    }
    else
    {

      LOGE("[%d] trig info trigger_id:%d",i,triggerInfoMatchingBuffer[i].triggerInfo.trigger_id);
      
    }
  }
  triggerInfoMatchingBuffer.clear();
  triggerMockFlags.clear();
  return true;
}


// std::timed_mutex mainThreadLock;

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
    // triggerInfoQueue.pop_blocking(&trigInfo)
    while (1)
    {
      sttriggerInfo_mix headImgStageInfoMixInfo;
      if(triggerInfoMatchingQueue.pop_blocking(headImgStageInfoMixInfo)==false)
        break;


      LOGI(">>>>>>>>>>>>>>>>>>>");


      LOGI("headImgStageInfoMixInfo.stInfo:%p  Qsize:%d",headImgStageInfoMixInfo.stInfo.get(),triggerInfoMatchingQueue.size());
      // triggerInfoMatchingBuffer.w_lock();
      
      {

        // 
        int minMatchingCost=999999;
        int minMatchingIdx=-1;

        std::shared_ptr<StageInfo_Image> targetStageInfo;
        sttriggerInfo_mix::_triggerInfo targetTriggerInfo;

        if(headImgStageInfoMixInfo.stInfo!=NULL)
        {//image/pipe info
          targetStageInfo= headImgStageInfoMixInfo.stInfo;
          
          bool doMockTriggerInfo=(targetStageInfo->img_prop.StreamInfo.camera!=NULL) && (triggerMockFlags.find(targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id) != triggerMockFlags.end()) ;

          LOGI("  doMockTriggerInfo:%d cam:%p mode:%d ",doMockTriggerInfo,  targetStageInfo->img_prop.StreamInfo.camera  ,targetStageInfo->img_prop.StreamInfo.camera->triggerMode);

          if(doMockTriggerInfo)
          {
            sttriggerInfo_mix::_triggerInfo mocktrig;
            mocktrig.camera_id=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id;
            LOGI(">>>>>>>>>timeStamp_us::%d",targetStageInfo->img_prop.fi.timeStamp_us);
            mocktrig.trigger_time_match_error_thres_us=1000000;
            mocktrig.est_trigger_time_us=targetStageInfo->img_prop.fi.timeStamp_us;//
            mocktrig.trigger_id=-(int)(targetStageInfo->img_prop.fi.timeStamp_us);

            std::string mockTag=triggerMockFlags[mocktrig.camera_id];
            mocktrig.tags.push_back( (mockTag.length() ==0)?"_STREAM_":mockTag);
            
            // triggerInfoMatchingBuffer.w_unlock();
            triggerInfoMatchingBuffer.push_back({triggerInfo:mocktrig});
            // triggerInfoMatchingBuffer.w_lock();
          }

          
          for(int i=0;i<triggerInfoMatchingBuffer.size();i++)//try to find trigger info matching
          {//
            if(triggerInfoMatchingBuffer[i].stInfo!=NULL)continue;//skip image/pipe info

            auto _triggerInfo=triggerInfoMatchingBuffer[i].triggerInfo;
           
            // if(_triggerInfo.camera_id!=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id)continue;//camera id is not match
            auto curCamID=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id;
            if (curCamID.find(_triggerInfo.camera_id) == std::string::npos) {
              continue;
            }

            int cost;
            
            
            if(_triggerInfo.est_trigger_time_us==0)
            {
              cost=0;
            }
            else
            {
              cost=_triggerInfo.est_trigger_time_us-targetStageInfo->img_prop.fi.timeStamp_us;
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
        else 
        {//trigger info

          targetTriggerInfo=headImgStageInfoMixInfo.triggerInfo;
          for(int i=0;i<triggerInfoMatchingBuffer.size();i++)//try to find image/pipe info
          {//
            if(triggerInfoMatchingBuffer[i].stInfo==NULL)continue;//skip trigger info

            auto _stInfo=triggerInfoMatchingBuffer[i].stInfo;
            //if(targetTriggerInfo.camera_id!=_stInfo->img_prop.StreamInfo.camera->getConnectionData().id)continue;//camera id is not fully matched

            auto curCamID=_stInfo->img_prop.StreamInfo.camera->getConnectionData().id;

            if (curCamID.find(targetTriggerInfo.camera_id) == std::string::npos) {
              continue;
            }
            
            int cost;
            
            
            if(targetTriggerInfo.est_trigger_time_us==0)
            {
              cost=0;
            }
            else
            {
              cost=_stInfo->img_prop.fi.timeStamp_us-targetTriggerInfo.est_trigger_time_us;
              if(cost<0)cost=-cost;
            }
          



            if(minMatchingCost>cost)
            {
              targetStageInfo=_stInfo;
              minMatchingIdx=i;
              minMatchingCost=cost;
              if(cost==0)break;
            }
          }
        }





        if( minMatchingIdx!=-1 && (minMatchingCost<targetTriggerInfo.trigger_time_match_error_thres_us||targetTriggerInfo.est_trigger_time_us==0))
        {
          LOGI("Get matching. idx:%d cost:%d  psss to next Q stInfo:%p",minMatchingIdx,minMatchingCost,targetStageInfo.get() );
          targetStageInfo->trigger_tags=targetTriggerInfo.tags;
          auto fullCamID=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id;
          targetStageInfo->trigger_tags.push_back(fullCamID);
          LOGI("cam id:%s  ch_id:%d",fullCamID.c_str(),targetStageInfo->img_prop.StreamInfo.channel_id);
          LOGI("TId:%d  info TId:%d",targetStageInfo->trigger_id,targetTriggerInfo.trigger_id);
          targetStageInfo->trigger_id=targetTriggerInfo.trigger_id;

          inspQueue.push_blocking(targetStageInfo);
          triggerInfoMatchingBuffer.erase(minMatchingIdx);//remove from buffer 
        }
        else
        {
          LOGI("No matching.... push in buffer MixInfo.stInfo:%p",headImgStageInfoMixInfo.stInfo.get());
          triggerInfoMatchingBuffer.push_back(headImgStageInfoMixInfo);//no match, add new data to buffer 
          LOGI("buffer size:%d",triggerInfoMatchingBuffer.size());
        }
      }
      // triggerInfoMatchingBuffer.w_unlock();
      

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

    while (1)
    {
      
      std::shared_ptr<StageInfo_Image> stInfo;
      if(inspQueue.pop_blocking(stInfo)==false)break;


      // LOGI(">>>CAM:%s",headImgPipe->camera_id.c_str());
      // LOGI("id:%d trigger:",stInfo->trigger_id);
      // for(auto tag:stInfo->trigger_tags)
      //   LOGI("%s",tag.c_str());
      int acceptCount=inspTarMan.dispatch(stInfo);
      // LOGI("acceptCount:%d",acceptCount);
      if(acceptCount)
      {
        int processCount = inspTarMan.inspTarProcess();
        LOGI("processCount:%d",processCount);
      }
      else
      {
        LOGE("NO InspTar accepts stage Info recycle.....");
        //no one accept the stage info
        // inspTarMan.unregNrecycleStageInfo(stInfo,NULL);
      }

      // LOGI("......<>>>>>.....");

    }
  }
}
/*
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


        int imgCHID=headImgPipe->img_prop.StreamInfo.channel_id;
    
        
        // CameraLayer::BasicCameraInfo data=headImgPipe->img_prop.StreamInfo.camera->getConnectionData();
        // cJSON* caminfo=CameraManager::cameraInfo2Json(data);


        cJSON* camBrifInfo=cJSON_CreateObject();
        cJSON_AddStringToObject(camBrifInfo, "trigger_tag", headImgPipe->trigger_tag.c_str());
        cJSON_AddNumberToObject(camBrifInfo, "trigger_id", headImgPipe->trigger_id);
        cJSON_AddStringToObject(camBrifInfo, "camera_id",headImgPipe->camera_id.c_str());

        bpg_pi.fromUpperLayer_DATA("CM",imgCHID,camBrifInfo);
        cJSON_Delete(camBrifInfo);
        
        
        BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)6};
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

*/

int PerifChannel::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    // LOGI("<MSG:%s", raw);
    char tmp[1024];
    if(strstr((char*)raw, "\"type\":\"TriggerInfo\"") != NULL)
    {
      cJSON *json = cJSON_Parse((char *)raw);
      if(json)
      {
        string camid=JFetch_STRING_ex(json,"camera_id");
        float trigger_id=JFetch_NUMBER_ex(json,"trigger_id");

        if(trigger_id==trigger_id && camid.length()>0)
        {

        
          
          sttriggerInfo_mix trigInfo;
          trigInfo.stInfo=NULL;
          trigInfo.triggerInfo.camera_id=camid;
          trigInfo.triggerInfo.trigger_id=(int)trigger_id;
          trigInfo.triggerInfo.est_trigger_time_us=0;//force matching

          string tag=JFetch_STRING_ex(json,"tag");
          if(tag.length()>0)
          {


            // split tag by ','
            char delim=',';
            int start = 0;
            auto len = 0;

            auto tag_cstr=tag.c_str();
            for(len=0;;len++)
            {

              if(tag_cstr[start+len]=='\0')
              { 
                if(len)trigInfo.triggerInfo.tags.push_back(tag.substr(start,len));
                break;
              }
              if(tag_cstr[start+len]==delim)
              {

                if(len)trigInfo.triggerInfo.tags.push_back(tag.substr(start,len));
                start+=len+1;
                len=0;
              }
            }

            // trigInfo.triggerInfo.tags.push_back(tag);// push tag string in as is


          }
          
          triggerInfoMatchingQueue.push_blocking(trigInfo);
        

        }
        cJSON_Delete(json);
      }
    }



    
    sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s,\"CONN_ID\":%d}", raw, ID);
    LOGI("<<:%s", tmp);
    bpg_pi.fromUpperLayer_DATA("PD",conn_pgID,tmp);
    bpg_pi.fromUpperLayer_DATA("SS",conn_pgID,"{}");
    return 0;

  }
  printf(">>opcode:%d\n",opcode);
  return 0;


}



void InspectionTarget_GroupResultSave::thread_run()
{
  acvImage cacheImage;
  while(true)
  {
    vector<shared_ptr<StageInfo>> curInputGroup;
    try{
      if(datTransferQueue.pop_blocking(curInputGroup)==false)
      {
        break;
      }
    }
    catch(TS_Termination_Exception e)
    {
      break;
    }


    // LOGI(">>>processGroup:  trigger_id:%d =========",trigger_id);
    
    StageInfo_Category *catInfo=NULL;
    for(int i=0;i<curInputGroup.size();i++)
    {
      auto curInput=curInputGroup[i];
      
      LOGI("[%d]:typeName:%s from:%s  =========",i,curInput->typeName().c_str(),curInput->source_id.c_str());
      if(curInput->typeName()==StageInfo_Category::stypeName())
      {
        catInfo=(StageInfo_Category*)curInput.get();
      }
    }
    if(catInfo!=NULL && catInfo->category<0)
    {
      for(int i=0;i<curInputGroup.size();i++)
      {
        //save all
        auto curInput=curInputGroup[i];
        LOGI("[%d]:SAVE:%s  =========",i,curInput->source_id.c_str());
        LOGI("DataTransfer thread pop data: name:%s type:%s ",curInput->source_id.c_str(),curInput->typeName().c_str());
      
      }
        
    }
    else
    {
      LOGI("No StageInfo_Category.... skip");
    }
  }

}



struct TimeStampConvertParam{
  uint64_t pCoreT;//previous core time
  uint64_t pCamT;  //previous camera time
  float alpha;  //core time delta to camera time delta ratio (adjust clock speed difference)
};
/*

-step1 init param
trigger two image events and get 
pCoreT,nCoreT, //previous and next core time
pCamT,nCamT //previous and next camera time
alpha = (nCamT-pCamT)/(nCoreT-pCoreT)  //update alpha
pCoreT=nCoreT
pCamT=nCamT



-step2 predict camera time from new core time
nCamT'=(nCoreT-pCoreT)*alpha+pCamT

-step3 match real camera time (nCamT) from predicted nCamT'
nCamT

-step4 update param
alpha = (nCamT-pCamT)/(nCoreT-pCoreT)
pCoreT=nCoreT
pCamT=nCamT



*/

uint64_t CamStampPred(TimeStampConvertParam* param,uint64_t newCoreTime)
{
  if(param->pCoreT==0 || param->pCamT==0 || param->alpha==0)
  {
    return 0;
  }
  return (newCoreTime-param->pCoreT)*param->alpha+param->pCamT;
}

TimeStampConvertParam CamStampParamUpdate(TimeStampConvertParam param,uint64_t newCoreTime,uint64_t newCamTime)
{
  TimeStampConvertParam newParam=param;

  if(newParam.pCamT!=0 && newParam.pCoreT!=0)
    newParam.alpha=(double)(newCamTime-newParam.pCamT)/(newCoreTime-newParam.pCoreT);
  newParam.pCoreT=newCoreTime;
  newParam.pCamT=newCamTime;

  return newParam;
}



class InspectionTarget_JSON_CNC_Peripheral :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  int comm_pgID=-1;
  // py::module pyscript;


  std::map <int,int> CNCMsgID_PGID;
  public:

  mutex pCH_mutex;
  std::thread periodicThread;
  bool liveFlag=true;
  vector<cJSON *> periodicPullJsonCMDs;



  class PerifChannel2:public Data_JsonRaw_Layer
  {
    
    public:
    int comm_pgID=-1;
    std::mutex sendMutex;

    void lock(){
      sendMutex.lock();
    }
    void unlock(){
      sendMutex.unlock();
    }


    InspectionTarget_JSON_CNC_Peripheral *master;
    int pkt_count = 0;
    PerifChannel2(InspectionTarget_JSON_CNC_Peripheral *master):Data_JsonRaw_Layer()// throw(std::runtime_error)
    {
      this->master=master;
    }

    int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode)
    {
      static int CCC=0;
      if(opcode==1 )
      {


        char tmp[1024];
        cJSON *json = cJSON_Parse((char *)raw);
        int msgID=JFetch_NUMBER_ex(json,"id",-1);

        int retPGID=comm_pgID;
        if(msgID!=-1 && (master->CNCMsgID_PGID.find(msgID)!=master->CNCMsgID_PGID.end()))
        {
          retPGID=master->CNCMsgID_PGID[msgID];
          master->CNCMsgID_PGID.erase(msgID);
        }
      
        sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s}", raw);
        LOGI("<<:%s  retPGID:%d", tmp,retPGID);
        bpg_pi.fromUpperLayer_DATA("PD",retPGID,tmp);
        bpg_pi.fromUpperLayer_DATA("SS",retPGID,"{}");



        
        return 0;

      }
      printf(">>opcode:%d\n",opcode);
      return 0;
    }
    int recv_RESET()
    {
      // printf("Get recv_RESET\n");
      return 0;
    }
    int recv_ERROR(ERROR_TYPE errorcode)
    {
      // printf("Get recv_ERROR:%d\n",errorcode);
      return 0;
    }
    
    void connected(Data_Layer_IF* ch){
      
      printf(">>>%X connected\n",ch);
    }

    void disconnected(Data_Layer_IF* ch){
      printf(">>>%X disconnected\n",ch);
    }

    ~PerifChannel2()
    {
      close();
      printf("MData_uInsp DISTRUCT:%p\n",this);
    }

    // int send_data(int head_room,uint8_t *data,int len,int leg_room){
      
    //   // printf("==============\n");
    //   // for(int i=0;i<len;i++)
    //   // {
    //   //   printf("%d ",data[i]);
    //   // }
    //   // printf("\n");
    //   return recv_data(data,len, false);//LOOP back
    // }
  };
  PerifChannel2 *pCH= NULL;
  
  public:


  static std::string TYPE(){ return "JSON_CNC_Peripheral"; }
  InspectionTarget_JSON_CNC_Peripheral(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path),
    periodicThread(&InspectionTarget_JSON_CNC_Peripheral::periodicFunction,this)
  {

    comm_pgID=-1;
  }

  ~InspectionTarget_JSON_CNC_Peripheral()
  {

    liveFlag=false;
    periodicThread.join();
    if (pCH)
    {
      LOGI("DELETING");
      delete pCH;
      pCH = NULL;
    }

    for(int i=0;i<periodicPullJsonCMDs.size();i++)
    {
      cJSON_Delete(periodicPullJsonCMDs[i]);
    }
    periodicPullJsonCMDs.clear();
  }




  void periodicFunction()
  {
    uint8_t _buf[1000];
    while(1)
    {

      for(int i=0;i<10;i++)//just to prevent long wait before exit
      {
        if(liveFlag==false) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      {
        lock_guard<mutex> lock(pCH_mutex);
        if(pCH==NULL)continue;
        std::lock_guard<std::mutex> lock2(pCH->sendMutex);
        
        // LOGE("<periodicPullJsonCMDs.size():%d",periodicPullJsonCMDs.size());
        for(int i=0;i<periodicPullJsonCMDs.size();i++)
        {
          int ret= sendcJSONTo_perifCH(pCH,_buf, sizeof(_buf),true,periodicPullJsonCMDs[i]);
        }
        // LOGE(">>>>>>>pCH:%p");



      }
    }
  }

  void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
  }

  virtual void setInspDef(cJSON* def)
  {
    InspectionTarget_StageInfoCollect_Base::setInspDef(def);

  }



  virtual bool feedStageInfo(std::shared_ptr<StageInfo> sinfo)
  {
    if(sinfo->source==this)return false;
    return InspectionTarget_StageInfoCollect_Base::feedStageInfo(sinfo);
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
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Category::stypeName().c_str() ));
      }

    }

    return arr;
  }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_JSON_CNC_Peripheral::processInputStagePool,this);
  }

  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
  {
    bool ret = InspectionTarget::exchangeCMD(info,id,act);
    if(ret)return ret;
    string type=JFetch_STRING_ex(info,"type");

    lock_guard<mutex> lock(pCH_mutex);
    if(type=="MESSAGE")
    {
      if(pCH==NULL)
      {
        return false;
      }

      LOGE(">>>>>>>");
      bool session_ACK=false;
      cJSON *msg_obj = JFetch_OBJECT(info, "msg");
      if (msg_obj)
      {

        std::lock_guard<std::mutex> lock(pCH->sendMutex);
        uint8_t _buf[2000];
        LOGE(">>>>>>>");
        int ret= sendcJSONTo_perifCH(pCH,_buf, sizeof(_buf),true,msg_obj);
        session_ACK = (ret>=0);

        int msg_id=JFetch_NUMBER_ex(msg_obj,"id",-1);

        if(session_ACK && msg_id!=-1)
        {
          act.doSendAck=false;
          CNCMsgID_PGID[msg_id]=id;
        }
      }
      else
      {
        session_ACK=false;//send nothing
      }
      LOGE(">>>>>>>");

      return session_ACK;
    }

    if(type=="is_CONNECTED")
    {
      return pCH!=NULL;
    }


    if(type=="timer_conv_seq")
    {
      return false;
    }

    if(type=="CONNECT")
    {

      int comm_id=JFetch_NUMBER_ex(info,"comm_id",-1);

      bool session_ACK=false;
      string errMsg="";
      do{
      if (pCH)
      {
        LOGI("DELETING");
        delete pCH;
        pCH = NULL;
      }
      LOGI("DELETED...  ");
      LOGI("comm_id:%d  ",comm_id);
      
      
      Data_Layer_IF *PHYLayer=NULL;
      char *uart_name = NULL;
      

      cJSON* src_info=def;

      char *IP = NULL;
      if ( (uart_name=JFetch_STRING(src_info, "uart_name")) !=NULL)
      {
        int baudrate = (int)JFetch_NUMBER_ex(src_info, "baudrate",-1);
        if(baudrate==-1)
        {
          break;
        }


        string default_mode=JFetch_STRING_ex(src_info, "mode","8N1");

        try{
          
          PHYLayer=new Data_UART_Layer(uart_name,baudrate, default_mode.c_str());


        }
        catch(std::runtime_error &e){
          
        }

      }
      else if( (IP=JFetch_STRING(src_info, "ip"))!=NULL)
      {

        double *port_number = JFetch_NUMBER(src_info, "port");
        if (port_number == NULL)
        {
          // sprintf(err_str, "IP(%d) port_number(%d)", IP!=NULL,port_number!=NULL);
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
        pCH=new PerifChannel2(this);
        comm_pgID=comm_id;
        pCH->comm_pgID=comm_id;
        pCH->setDLayer(PHYLayer);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pCH->send_RESET();
        pCH->RESET();


        session_ACK = true;
  
      }
      else
      {
        session_ACK = false;

        LOGE("PHYLayer is not able to eatablish");
        // sprintf(err_str, "PHYLayer is not able to eatablish");
      }
    
    
    
      }while(0);

      return session_ACK;
    }

    if(type=="DISCONNECT")
    {
      if(pCH==NULL)
      {
        return false;
      }
      
      if (pCH)
      {
        LOGI("DELETING");
        delete pCH;
        pCH = NULL;
      }
      return true;
    }


    if(type=="setPeriodicPullCMDs")
    {
      cJSON* cmds=JFetch_ARRAY(info,"cmds");
      if(cmds==NULL)return false;
      for(int i=0;i<periodicPullJsonCMDs.size();i++)
      {
        cJSON_Delete(periodicPullJsonCMDs[i]);
      }
      periodicPullJsonCMDs.clear();

      for(int i=0;i<cJSON_GetArraySize(cmds);i++)
      {
        cJSON* cmd=cJSON_GetArrayItem(cmds,i);
        if(cmd==NULL)continue;
        cJSON* clonedCMD=cJSON_Duplicate(cmd,true);
        periodicPullJsonCMDs.push_back(clonedCMD);
      }

      return true;
    }

    return false;
  }


  int find(std::vector< string > tags,string target)
  {
    for(int i=0;i<tags.size();i++)
    {
      if(tags[i]==target)return i;
    }
    return -1;
  }


  void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {

  }
};



class ScriptChannel:public Data_JsonRaw_Layer
{
  private:
  std::unique_ptr<FILE, decltype(&pclose)> scriptCMD_pipe;
  protected:
  int sendID=0;
  mutex promiseTable_LOCK;
  std::map<int, std::promise<cJSON*>> idPromise;


  public:
  ScriptChannel(std::string env_path,std::string cmd,std::string com_ip,int ip_port):Data_JsonRaw_Layer(),
  scriptCMD_pipe(popen(("cd "+env_path+"&&"+cmd+" --port="+to_string(ip_port)).c_str(), "r"), pclose)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Data_Layer_IF *PHYLayer=new Data_TCP_Layer(com_ip.c_str(),ip_port);

    setDLayer(PHYLayer);
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));
    send_RESET();
    send_data(0,(uint8_t*)"\n",1,0);
    RESET();

    LOGE(">>>ScriptChannel>>");
  }

  int PromiseTryFulfill(int id,cJSON* json)
  {
    lock_guard<mutex> lock(promiseTable_LOCK);
    if(idPromise.find(id)!=idPromise.end())
    {
      idPromise[id].set_value(json);
      idPromise.erase(id);
      return 0;
    }
    return -1;
  }

  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode)
  {
    cJSON *json=NULL;

    do{

      if(opcode==1 )
      {
        char tmp[1024];
        // LOGI("raw:%s",raw);
        cJSON *json = cJSON_Parse((char *)raw);

        int id=JFetch_NUMBER_ex(json,"id",-1);

        if(id>=0&&PromiseTryFulfill(id,json)==0)break;


      }

    }while(false);

    if(json!=NULL)
    {
      cJSON_Delete(json);
    }
    return 0;
  }
  int recv_RESET()
  {
    // printf("Get recv_RESET\n");
    return 0;
  }
  int recv_ERROR(ERROR_TYPE errorcode)
  {
    // printf("Get recv_ERROR:%d\n",errorcode);
    return 0;
  }
  
  bool dlayerConnected=false;
  void connected(Data_Layer_IF* ch){
    printf(">>>%X connected\n",ch);
    dlayerConnected=true;
  }

  void disconnected(Data_Layer_IF* ch){
    printf(">>>%X disconnected\n",ch);
    dlayerConnected=false;
  }

  bool isRunning()
  {
    // LOGE("RUN:%d",feof(scriptCMD_pipe.get()));
    return dlayerConnected && !feof(scriptCMD_pipe.get());
  }

  ~ScriptChannel()
  {

    {
      char tmp[128];
      int len=sprintf(tmp,"{\"type\":\"EXIT\"}\n");
      send_data(0,(uint8_t*)tmp,len,0);
    }


    close();
    printf("MData_uInsp DISTRUCT:%p\n",this);
  }

  

  std::future<cJSON*> send_future(cJSON* json,int *ret_id=NULL)
  {
    lock_guard<mutex> lock(promiseTable_LOCK);
    sendID++;
    std::promise<cJSON*> prom;
    std::future<cJSON*> fut = prom.get_future();
    idPromise[sendID]=std::move(prom);

    char buffer[5000];

    if(ret_id)*ret_id=sendID;
    cJSON_AddNumberToObject(json,"id",sendID);
    cJSON_PrintPreallocated(json,buffer,sizeof(buffer),false);
    size_t blen=strlen(buffer);
    // if(buffer[blen-1]=='}')
    // {
    //   char* tail=&buffer[blen-1];
    //   blen+=sprintf(tail,",\"id\":%d}",sendID)-1;
    // }

    send_data(0,(uint8_t*)buffer,blen,0);
    send_data(0,(uint8_t*)"\n",1,0);
    return fut;

  }



  cJSON* send_waitfor_return(cJSON* json,int waitTime_ms)
  {
    int id=-1;
    auto prom=send_future(json,&id);
    if(prom.wait_for(std::chrono::milliseconds(waitTime_ms))==std::future_status::ready)
    {
      return prom.get();
    }
    {
      lock_guard<mutex> lock(promiseTable_LOCK);
      idPromise[id].set_exception(std::make_exception_ptr(std::runtime_error("timeout")));
      idPromise.erase(id);
    }
    return NULL;
  }
};





class InspectionTarget_JSON_Peripheral :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  int comm_pgID=-1;
  // py::module pyscript;


  public:

  std::map <int,int64_t> processTimeRecord;
  struct triggerInfoCacheData{
    int tid;
    uint64_t uInsp_time_us;
  };
  std::mutex bTrigInfoRecord_LOCK;
  std::vector <triggerInfoCacheData> bTrigInfoRecordBuffer;
  std::map <string,TimeStampConvertParam> CamStampConvertSet;

  float processTime_MaxDelay=0;
  float processTime_AvgDelay=0;
  float processTime_LPDelay=0;
  int processTime_OverTimeCount=0;
  int processTime_OverTimeCountMax=0;
  int processTime_AvgDelay_Count=0;


  std::thread periodicThread;
  bool liveFlag=true;

  class PerifChannel2:public Data_JsonRaw_Layer
  {
    
    public:
    int fastTestRetCatFlag=STAGEINFO_CAT_UNSET;
    int comm_pgID=-1;
    std::mutex sendMutex;

    void lock(){
      sendMutex.lock();
    }
    void unlock(){
      sendMutex.unlock();
    }



    InspectionTarget_JSON_Peripheral *master;
    int pkt_count = 0;
    PerifChannel2(InspectionTarget_JSON_Peripheral *master):Data_JsonRaw_Layer()// throw(std::runtime_error)
    {
      this->master=master;
    }

    int testCounter=0;
    int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode)
    {
      static int CCC=0;
      if(opcode==1 )
      {

        bool passUp=true;
        char tmp[1024];



        // if(pyscript.is_none()==false)
        // {
        //   auto pyMapFunc=pyscript.attr("jsonPerifMsgProcess");
        //   if(pyMapFunc.is_none()==false)
        //   {
        //     pyMapFunc(json);
        //   }
        // }

        //call a python function "TT" in pyscript



        if(strstr((char*)raw, "\"type\":\"bTrigInfo\"") != NULL)
        {
          LOGE(">>>");
          if(master->sCH==NULL)
          {
            LOGE("sCH==NULL");
            return -1;
          }
          cJSON *json = cJSON_Parse((char *)raw);

          cJSON *rep=master->sCH->send_waitfor_return(json,1000);

          LOGE(">>>rep:%p",rep);
          if(rep)
          {

            cJSON *trigInfoArr=JFetch_ARRAY(rep,"data");
            int arrl=cJSON_GetArraySize(trigInfoArr);
            for(int i=0;i<arrl;i++)
            {
              cJSON *jtrigInfo=cJSON_GetArrayItem(trigInfoArr,i);
              if(jtrigInfo==NULL)continue;
              



              LOGI("trigInfo");
              sttriggerInfo_mix trigInfo;
              trigInfo.stInfo=NULL;
              trigInfo.triggerInfo.camera_id=(JFetch_STRING_ex(jtrigInfo,"camera_id"));

              trigInfo.triggerInfo.trigger_id=(JFetch_NUMBER_ex(jtrigInfo,"trigger_id",-1));
              master->processTimeRecord[trigInfo.triggerInfo.trigger_id]=cv::getTickCount();
              trigInfo.triggerInfo.est_trigger_time_us=JFetch_NUMBER_ex(jtrigInfo,"est_trigger_time_us",-1);
              trigInfo.triggerInfo.trigger_time_match_error_thres_us=JFetch_NUMBER_ex(jtrigInfo,"trigger_time_match_error_thres_us",-1);

              {
                cJSON* tags=JFetch_ARRAY(jtrigInfo,"tags");
                if(tags)
                {
                  for(int i=0;i<cJSON_GetArraySize(tags);i++)
                  {
                    cJSON* tag=cJSON_GetArrayItem(tags,i);
                    if(tag)
                    {
                      trigInfo.triggerInfo.tags.push_back(std::string(tag->valuestring));
                    }
                  }
                }
              }

              triggerInfoMatchingQueue.push_blocking(trigInfo);

            }

            cJSON_Delete(rep);

          }
          cJSON_Delete(json);


        }

        if(false && strstr((char*)raw, "\"type\":\"bTrigInfo\"") != NULL)
        {
          passUp=false;
          cJSON *json = cJSON_Parse((char *)raw);
          if(json)
          {
            int tidx=JFetch_NUMBER_ex(json,"tidx",-1);
            
            tidx+=98;
            //TODO:translate to trigger info
            if(tidx==1)
            {

              std::lock_guard<std::mutex> lock(master->bTrigInfoRecord_LOCK);
              int tid=JFetch_NUMBER_ex(json,"tid",-1);


              int64_t usH=JFetch_NUMBER_ex(json,"usH",-1);
              int64_t usL=JFetch_NUMBER_ex(json,"usL",-1);

              int64_t uInsp_time_us=(usH==-1 || usL==-1)?0:((usH<<32)|usL);


              LOGI("bTrigInfo tidx:%d tid:%d", tidx,tid);

              {
                string cam_id="K44478350";

                int64_t time_us=0;
                if(master->CamStampConvertSet.find(cam_id)!=master->CamStampConvertSet.end())
                {
                  TimeStampConvertParam camParm= master->CamStampConvertSet[cam_id];
                  time_us=CamStampPred(&camParm,uInsp_time_us);
                  if(time_us==0)time_us=0;//NOTE: CamStampPred return 0 means not ready
                }
                else
                {
                  master->CamStampConvertSet[cam_id].alpha=
                  master->CamStampConvertSet[cam_id].pCamT=
                  master->CamStampConvertSet[cam_id].pCoreT=0;
                }

                sttriggerInfo_mix trigInfo;
                trigInfo.stInfo=NULL;
                trigInfo.triggerInfo.camera_id=cam_id;

                trigInfo.triggerInfo.trigger_id=tid;
                trigInfo.triggerInfo.est_trigger_time_us=time_us;//force matching
                trigInfo.triggerInfo.trigger_time_match_error_thres_us=2000;
                trigInfo.triggerInfo.tags.push_back("CAM_A");
                trigInfo.triggerInfo.tags.push_back("s_uINSP_A");
                triggerInfoMatchingQueue.push_blocking(trigInfo);
              }

              {

                string cam_id="K44478343";

                int64_t time_us=0;
 
                if(master->CamStampConvertSet.find(cam_id)!=master->CamStampConvertSet.end())
                {
                  TimeStampConvertParam camParm= master->CamStampConvertSet[cam_id];
                  time_us=CamStampPred(&camParm,uInsp_time_us);
                  if(time_us==0)time_us=0;//NOTE: CamStampPred return 0 means not ready
                }
                else
                {
                  master->CamStampConvertSet[cam_id].alpha=
                  master->CamStampConvertSet[cam_id].pCamT=
                  master->CamStampConvertSet[cam_id].pCoreT=0;
                }

                sttriggerInfo_mix trigInfo;
                trigInfo.triggerInfo.trigger_id=tid;
                trigInfo.triggerInfo.est_trigger_time_us=time_us;//force matching
                trigInfo.triggerInfo.trigger_time_match_error_thres_us=2000;
                trigInfo.triggerInfo.tags.push_back("CAM_B");
                trigInfo.triggerInfo.tags.push_back("s_uINSP_B");
                triggerInfoMatchingQueue.push_blocking(trigInfo);
              }

              if(fastTestRetCatFlag!=STAGEINFO_CAT_UNSET)
              {
                          
                std::lock_guard<std::mutex> lock(sendMutex); 
                
                cJSON *rep = cJSON_CreateObject();
                cJSON_AddStringToObject(rep,"type","report");
                cJSON_AddNumberToObject(rep,"tid",tid);
                cJSON_AddNumberToObject(rep,"cat",fastTestRetCatFlag);

                uint8_t _buf[1000];
                // LOGE(">>>>>>>pCH:%p");
                int ret= sendcJSONTo_perifCH(this,_buf, sizeof(_buf),true,rep);
                cJSON_Delete(rep);
              }
              else
              {
                //check if processTimeRecord[tid] exists

                master->processTimeRecord[tid]=cv::getTickCount();
                // if(processTimeRecord.find(tid)!=processTimeRecord.end())
                // {
                //   int64 t1 = cv::getTickCount();
                //   uint64_t processTime=processTimeRecord[tid];
                // }
              }
              {

                for(int i=0;i<master->bTrigInfoRecordBuffer.size();i++)
                {
                  if(master->bTrigInfoRecordBuffer[i].tid==-1)
                  {
                    master->bTrigInfoRecordBuffer[i].tid=tid;
                    master->bTrigInfoRecordBuffer[i].uInsp_time_us=uInsp_time_us;
                    break;
                  }
                }
              }
            }
            if(tidx==99)//Just for test
            {
              testCounter++;
              
              int tid=JFetch_NUMBER_ex(json,"tid",-1);
              cJSON *rep = cJSON_CreateObject();
              cJSON_AddStringToObject(rep,"type","report");
              if(testCounter>20)
              {
                // tid=-1;
                testCounter=0;
              }

              cJSON_AddNumberToObject(rep,"tid",tid);
              
              cJSON_AddNumberToObject(rep,"cat",0xFFFF);

              uint8_t _buf[1000];
              // LOGE(">>>>>>>pCH:%p");
              int ret= sendcJSONTo_perifCH(this,_buf, sizeof(_buf),true,rep);
              cJSON_Delete(rep);

            }
            cJSON_Delete(json);
          }
          json=NULL;
        }
        
        if(passUp)
        {
          sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s}", raw);
          // LOGI("<<:%s  comm_pgID:%d", tmp,comm_pgID);
          // int stream_id =JFetch_NUMBER_ex(master->additionalInfo,"stream_info.stream_id",-1);
          // if(stream_id!=-1)
          {
            bpg_pi.fromUpperLayer_DATA("PD",comm_pgID,tmp);
            bpg_pi.fromUpperLayer_DATA("SS",comm_pgID,"{}");
          }
        }
        return 0;

      }
      printf(">>opcode:%d\n",opcode);
      return 0;
    }
    int recv_RESET()
    {
      // printf("Get recv_RESET\n");
      return 0;
    }
    int recv_ERROR(ERROR_TYPE errorcode)
    {
      // printf("Get recv_ERROR:%d\n",errorcode);
      return 0;
    }
    
    void connected(Data_Layer_IF* ch){
      testCounter=0;
      printf(">>>%X connected\n",ch);
    }

    void disconnected(Data_Layer_IF* ch){
      printf(">>>%X disconnected\n",ch);
    }

    ~PerifChannel2()
    {
      close();
      printf("MData_uInsp DISTRUCT:%p\n",this);
    }

    // int send_data(int head_room,uint8_t *data,int len,int leg_room){
      
    //   // printf("==============\n");
    //   // for(int i=0;i<len;i++)
    //   // {
    //   //   printf("%d ",data[i]);
    //   // }
    //   // printf("\n");
    //   return recv_data(data,len, false);//LOOP back
    // }
  };
  PerifChannel2 *pCH= NULL;

  class ScriptChannel2:public ScriptChannel
  {
    
    public:


    ScriptChannel2(std::string env_path,std::string cmd,std::string com_ip,int ip_port):
    ScriptChannel(env_path,cmd,com_ip,ip_port)
    {

    }
    
    int testCounter=0;
    int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode)
    {


      cJSON *json=NULL;

      if(opcode==1 )do{

      
        char tmp[1024];
        // LOGI("raw:%s",raw);
        cJSON *json = cJSON_Parse((char *)raw);

        int id=JFetch_NUMBER_ex(json,"id",-1);

        if(id>=0&&PromiseTryFulfill(id,json)==0)break;

        std::string type=JFetch_STRING_ex(json,"type");

        if(type=="DBGMSG")
        {
          // LOGI("DBGMSG:%s",JFetch_STRING_ex(json,"msg").c_str());
          LOGI("DBGMSG:%s",raw);
        }
      }while(false);

      if(json!=NULL)
      {
        cJSON_Delete(json);
      }
      return 0;
    }
    
    ~ScriptChannel2()
    {
    }

  };
  
  std::string local_env_path;
  ScriptChannel2 *sCH= NULL;


  mutex pCH_mutex;
  vector<cJSON *> periodicPullJsonCMDs;

  int ImgSaveCountDown_OK=0;
  int ImgSaveCountDown_NG=0;
  int ImgSaveCountDown_NG2=0;
  int ImgSaveCountDown_NG3=0;
  int ImgSaveCountDown_NA=0;

  int FetchCountDown_OK=0;
  int FetchCountDown_NG=0;
  int FetchCountDown_NG2=0;
  int FetchCountDown_NG3=0;
  int FetchCountDown_NA=0;

  std::mutex recentSrcLock;
  vector<vector<std::shared_ptr<StageInfo>>> recentSrcStageInfoSet;
  RingBufIdxCounter<int> recentSrcStageInfoSetIdx;

  int64 scriptT;
  public:

  bool file_exists(const std::string& filename) {
      std::ifstream file(filename.c_str());
      return file.good();
  }

  void unloadScript()
  {
    if(sCH)
    {
      delete sCH;
      sCH=NULL;
    }
  }

  bool reloadScript()
  {
    unloadScript();

    try{
      sCH=new ScriptChannel2(local_env_path,
          JFetch_STRING_ex(def,"scriptInfo.run_cmd"),
          "127.0.0.1",
          JFetch_NUMBER_ex(def,"scriptInfo.IPC_port"));

    }
    catch(std::runtime_error &e){
      LOGE("ScriptChannel2 fail:%s",e.what());
      return false;
    }


    {
      LOGE("Send defInfo....");
      cJSON *json = cJSON_CreateObject();
      cJSON_AddStringToObject(json,"type","defInfo");
      cJSON_AddItemToObject(json,"defInfo",def);


      cJSON *rep =sCH->send_waitfor_return(json,5000);




      cJSON_DetachItemFromObject(json,"defInfo");
      cJSON_Delete(json);


      if(rep!=NULL)
      {
        LOGE("Send defInfo.... done!");
        cJSON_Delete(rep);
      }
      else
      {
        LOGE("Send defInfo.... timeout!");
        unloadScript();
        return false;
      }
    }

    return true;

    // {
    //   int len=sprintf((char*)_buf,"{\"type\":\"EXIT\"}\n");
    //   sCH->send_data(0,(uint8_t*)_buf,len,0);
    // }


    // std::string result;
    // std::array<char, 128> buffer;
    // while (fgets(buffer.data(), buffer.size(), sCH->scriptCMD_pipe.get()) != nullptr) {
    //   result += buffer.data();
    // }
    // LOGE("PY script output:%s",result.c_str());

  }

  static std::string TYPE(){ return "JSON_Peripheral"; }
  InspectionTarget_JSON_Peripheral(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path),
    recentSrcStageInfoSetIdx(100),bTrigInfoRecordBuffer(100),liveFlag(true),
    periodicThread(&InspectionTarget_JSON_Peripheral::periodicFunction,this)
  {
    this->local_env_path=local_env_path;
    comm_pgID=-1;
    recentSrcStageInfoSet.resize(recentSrcStageInfoSetIdx.space());
    for(int i=0;i<bTrigInfoRecordBuffer.size();i++)
    {
      bTrigInfoRecordBuffer[i].tid=-1;
    }


    reloadScript();
    // LOGE("PY script output:%s",exec(("python3 "+local_env_path+"/script.py").c_str()).c_str());

  }


  void periodicFunction()
  {
    uint8_t _buf[1000];
    while(1)
    {

      for(int i=0;i<10;i++)//just to prevent long wait before exit
      {
        if(liveFlag==false) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      {
        lock_guard<mutex> lock(pCH_mutex);
        if(pCH==NULL)continue;
        std::lock_guard<std::mutex> lock2(pCH->sendMutex);
        
        // LOGE("<periodicPullJsonCMDs.size():%d",periodicPullJsonCMDs.size());
        for(int i=0;i<periodicPullJsonCMDs.size();i++)
        {
          cJSON* cmd=periodicPullJsonCMDs[i];
          if(cmd==NULL)continue;
          int ret= sendcJSONTo_perifCH(pCH,_buf, sizeof(_buf),true,cmd);

        }
        // LOGE(">>>>>>>pCH:%p");



      }
    }
  }

  ~InspectionTarget_JSON_Peripheral()
  {
    unloadScript();
    liveFlag=false;
    periodicThread.join();
    

    if(sCH)
    {
      delete sCH;
      sCH=NULL;
    }
    for(int i=0;i<periodicPullJsonCMDs.size();i++)
    {
      cJSON_Delete(periodicPullJsonCMDs[i]);
    }
    periodicPullJsonCMDs.clear();

  }

  void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
  }

  virtual void setInspDef(cJSON* def)
  {
    InspectionTarget_StageInfoCollect_Base::setInspDef(def);

    scriptNeedInit=true;
  }



  virtual bool feedStageInfo(std::shared_ptr<StageInfo> sinfo)
  {
    if(sinfo->source==this)return false;
    return InspectionTarget_StageInfoCollect_Base::feedStageInfo(sinfo);
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
        cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Category::stypeName().c_str() ));
      }

    }

    return arr;
  }

  std::future<int> futureInputStagePool()
  {
    return std::async(launch::async,&InspectionTarget_JSON_Peripheral::processInputStagePool,this);
  }

  string TEST_mode="";
  int TEST_mode_counter=0;
  int TEST_mode_counter_MOD=0;
  int TEST_mode_count1=0;
  int TEST_mode_count2=0;
  int cacheStageInfoTID_START=-100000000;
  int cacheStageInfoTID=-100000000;
  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
  {
    bool ret = InspectionTarget::exchangeCMD(info,id,act);
    if(ret)return ret;

    lock_guard<mutex> lock(pCH_mutex);
    string type=JFetch_STRING_ex(info,"type");

    if(type=="MESSAGE")
    {
      if(pCH==NULL)
      {
        return false;
      }

      LOGE(">>>>>>>");
      bool session_ACK=false;
      cJSON *msg_obj = JFetch_OBJECT(info, "msg");
      if (msg_obj)
      {

        std::lock_guard<std::mutex> lock(pCH->sendMutex);
        uint8_t _buf[2000];
        LOGE(">>>>>>>");
        int ret= sendcJSONTo_perifCH(pCH,_buf, sizeof(_buf),true,msg_obj);
        session_ACK = (ret>=0);
      }
      else
      {
        session_ACK=false;//send nothing
      }
      LOGE(">>>>>>>");

      return session_ACK;
    }

    if(type=="is_CONNECTED")
    {
      return pCH!=NULL;
    }
    if(type=="reloadscript")
    {
      reloadScript();
      return true;
    }


    if(type=="timer_conv_seq")
    {
      return false;
    }

    if(type=="CONNECT")
    {

      int comm_id=JFetch_NUMBER_ex(info,"comm_id",-1);

      bool session_ACK=false;
      string errMsg="";
      do{
      if (pCH)
      {
        LOGI("DELETING");
        delete pCH;
        pCH = NULL;
      }
      LOGI("DELETED...  ");
      LOGI("comm_id:%d  ",comm_id);
      
      
      Data_Layer_IF *PHYLayer=NULL;
      char *uart_name = NULL;
      

      cJSON* src_info=def;

      char *IP = NULL;
      if ( (uart_name=JFetch_STRING(src_info, "uart_name")) !=NULL)
      {
        int baudrate = (int)JFetch_NUMBER_ex(src_info, "baudrate",-1);
        if(baudrate==-1)
        {
          break;
        }


        string default_mode=JFetch_STRING_ex(src_info, "mode","8N1");

        try{
          
          PHYLayer=new Data_UART_Layer(uart_name,baudrate, default_mode.c_str());


        }
        catch(std::runtime_error &e){
          
        }

      }
      else if( (IP=JFetch_STRING(src_info, "ip"))!=NULL)
      {

        double *port_number = JFetch_NUMBER(src_info, "port");
        if (port_number == NULL)
        {
          // sprintf(err_str, "IP(%d) port_number(%d)", IP!=NULL,port_number!=NULL);
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
        pCH=new PerifChannel2(this);
        comm_pgID=comm_id;
        pCH->comm_pgID=comm_id;
        pCH->setDLayer(PHYLayer);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pCH->send_RESET();
        pCH->RESET();


        session_ACK = true;
  
      }
      else
      {
        session_ACK = false;

        LOGE("PHYLayer is not able to eatablish");
        // sprintf(err_str, "PHYLayer is not able to eatablish");
      }
    
    
    
      }while(0);

      return session_ACK;
    }

    if(type=="DISCONNECT")
    {
      if(pCH==NULL)
      {
        return false;
      }
      
      if (pCH)
      {
        LOGI("DELETING");
        delete pCH;
        pCH = NULL;
      }
      return true;
    }

    if(type=="TEST_MODE")
    {
      string mode=JFetch_STRING_ex(info,"mode");
      TEST_mode=mode;
      TEST_mode_count1=JFetch_NUMBER_ex(info,"space[0]",1);
      TEST_mode_count2=JFetch_NUMBER_ex(info,"space[1]",1);
      TEST_mode_counter_MOD=TEST_mode_count1+TEST_mode_count2;
      if(TEST_mode=="OK_OK")
      {
        return true;
      }
      if(TEST_mode=="NG_NG")
      {
        return true;
      }
      if(TEST_mode=="NA_NA")
      {
        return true;
      }
      if(TEST_mode=="OK_NG")
      {
        return true;
      }
      if(TEST_mode=="OK_NG_NA")
      {
        TEST_mode_counter_MOD=3;
        return true;
      }
      if(TEST_mode=="OK_NA")
      {
        return true;
      }
      if(TEST_mode=="NG_NA")
      {
        return true;
      }


      TEST_mode="";
      return true;

    }
    
    if(type=="is_Script_Running")
    {
      return sCH!=NULL && sCH->isRunning();
    }

    if(type=="SrcImgSaveCountDown")
    {
      ImgSaveCountDown_OK=JFetch_NUMBER_ex(info,"count_OK",ImgSaveCountDown_OK);
      ImgSaveCountDown_NG=JFetch_NUMBER_ex(info,"count_NG",ImgSaveCountDown_NG);
      ImgSaveCountDown_NG2=JFetch_NUMBER_ex(info,"count_NG2",ImgSaveCountDown_NG2);
      ImgSaveCountDown_NG3=JFetch_NUMBER_ex(info,"count_NG3",ImgSaveCountDown_NG3);
      ImgSaveCountDown_NA=JFetch_NUMBER_ex(info,"count_NA",ImgSaveCountDown_NA);
      return true;
    }


    if(type=="GetFetchSrcTIDList")
    {
        
      std::lock_guard<std::mutex> lock(recentSrcLock); 
      cJSON* arr= cJSON_CreateArray();

      for(int i=0;i<recentSrcStageInfoSetIdx.size();i++)
      {
        int idx = recentSrcStageInfoSetIdx.getHead(i+1);
        if(recentSrcStageInfoSet[idx].size()==0)continue;
        cJSON_AddItemToArray(arr,cJSON_CreateNumber(recentSrcStageInfoSet[idx][0]->trigger_id));

      }

      act.send("RP",id,arr);
      cJSON_Delete(arr);arr=NULL;
      return true;
    }


    if(type=="FetchCountDown")
    {
        
      FetchCountDown_OK=JFetch_NUMBER_ex(info,"count_OK",FetchCountDown_OK);
      FetchCountDown_NG=JFetch_NUMBER_ex(info,"count_NG",FetchCountDown_NG);
      FetchCountDown_NG2=JFetch_NUMBER_ex(info,"count_NG2",FetchCountDown_NG2);
      FetchCountDown_NG3=JFetch_NUMBER_ex(info,"count_NG3",FetchCountDown_NG3);
      FetchCountDown_NA=JFetch_NUMBER_ex(info,"count_NA",FetchCountDown_NA);
      return true;
    }




    if(type=="TriggerFetchSrc")
    {
      int targetIdx=-1;
      {
        
        std::lock_guard<std::mutex> lock(recentSrcLock); 
        

        float ftargetTID=JFetch_NUMBER_ex(info,"trigger_id");
        if(ftargetTID!=ftargetTID)return false;
        int targetTID=ftargetTID;
            // LOGI("<<<<targetTID:%d>>>>",targetTID);
        for(int i=0;i<recentSrcStageInfoSetIdx.size();i++)
        {
          int idx = recentSrcStageInfoSetIdx.getTail(i);
          auto &infoSet=recentSrcStageInfoSet[idx];

            // LOGI("<<<<idx:%d>>>>",idx);
          if(infoSet.size()==0)continue;
            // LOGI("<<<<infoSet tid:%d  size:%d>>>>",infoSet[0]->trigger_id,infoSet.size());
          if(infoSet[0]->trigger_id!=targetTID)continue;
          targetIdx=i;
          break;
        }

      }

      if(targetIdx>=0)
      {
        auto &infoSet=recentSrcStageInfoSet[targetIdx];





        for(int j=0;j<infoSet.size();j++)
        {

          auto src = dynamic_cast<StageInfo_Image*>(infoSet[j].get());


          if(src==NULL)continue;
          
          LOGI("SEND....");
          shared_ptr<StageInfo_Image> pkt(new StageInfo_Image());
          pkt->img=src->img;
          pkt->img_prop=src->img_prop;
          pkt->img_show=src->img_show;
          pkt->process_time_us=src->process_time_us;
          pkt->sharedInfo=src->sharedInfo;

          pkt->source=src->source;
          pkt->source_id=src->source_id;


          pkt->trigger_tags=src->trigger_tags;
          pkt->trigger_tags.push_back("s_uInspCache_");
          cacheStageInfoTID--;
          pkt->trigger_id=cacheStageInfoTID;//cacheStageInfoIssueTID-src->trigger_id;
          belongMan->dispatch(pkt);


          processTimeRecord[cacheStageInfoTID]=cv::getTickCount();  






        }
        while (belongMan->inspTarProcess())
        {
        }
        return true;
      }

          LOGI("END....");
      return true;
    }


    if(type=="ClearFetchSrc")
    {
      recentSrcStageInfoSetIdx.clear();
      return true;
    }


    if(type=="FastTestRetCatFlag")
    {
      if(pCH==NULL)return false;

      pCH->fastTestRetCatFlag=JFetch_NUMBER_ex(info,"cat",STAGEINFO_CAT_UNSET);
      return true;
    }



    if(type=="GetProcessTimeInfo")//processTime_MaxDelay
    {
      {
        cJSON* ret_info= cJSON_CreateObject();

        cJSON_AddNumberToObject(ret_info,"Max",(int)processTime_MaxDelay);
        cJSON_AddNumberToObject(ret_info,"Avg",(int)processTime_AvgDelay);
        cJSON_AddNumberToObject(ret_info,"LP",(int)processTime_LPDelay);
        cJSON_AddNumberToObject(ret_info,"OTCountMax",(int)processTime_OverTimeCountMax);
        cJSON_AddNumberToObject(ret_info,"Count",(int)processTime_AvgDelay_Count);

        act.send("RP",id,ret_info);
        cJSON_Delete(ret_info);

      }


      if(JFetch_TRUE(info,"reset"))
      {
        processTime_AvgDelay_Count=0;
        processTime_MaxDelay=0;
        processTime_LPDelay=NAN;
        processTime_OverTimeCountMax=0;
      }
      return true;
    }

    if(type=="setPeriodicPullCMDs")
    {
      cJSON* cmds=JFetch_ARRAY(info,"cmds");
      if(cmds==NULL)return false;
      for(int i=0;i<periodicPullJsonCMDs.size();i++)
      {
        cJSON_Delete(periodicPullJsonCMDs[i]);
      }
      periodicPullJsonCMDs.clear();

      for(int i=0;i<cJSON_GetArraySize(cmds);i++)
      {
        cJSON* cmd=cJSON_GetArrayItem(cmds,i);
        if(cmd==NULL)continue;
        cJSON* clonedCMD=cJSON_Duplicate(cmd,true);
        periodicPullJsonCMDs.push_back(clonedCMD);
      }

      return true;
    }

    return false;
  }


  int find(std::vector< string > tags,string target)
  {
    for(int i=0;i<tags.size();i++)
    {
      if(tags[i]==target)return i;
    }
    return -1;
  }


  // int ResultA()
  // {
    
  // }

bool scriptNeedInit=true;

void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
{
  int catSum;

  cJSON *ignore_indexes=NULL;
  cJSON *script_result=NULL;

  if(sCH==NULL)
  {
    LOGE("sCH==NULL");
    return;
  }
  if(true)
  {
    scriptT=cv::getTickCount();
    cJSON *info = cJSON_CreateObject();
    cJSON_AddNumberToObject(info, "trigger_id", trigger_id);
    cJSON_AddStringToObject(info, "type", "reports");
    cJSON *jgroup = cJSON_CreateArray();
    cJSON_AddItemToObject(info, "group", jgroup);
    for (int i = 0; i < group.size(); i++)
    {
      cJSON *genrep = cJSON_CreateObject();
      genrep=group[i]->attachJsonRep(genrep,0);
      cJSON_AddItemToArray(jgroup,genrep);
    }

    cJSON* result=sCH->send_waitfor_return(info,1000);
    cJSON_Delete(info);
    if(result)
    {
        // char *buffer = cJSON_PrintUnformatted(result);
        // LOGE(">>:%s",buffer);

        catSum=JFetch_NUMBER_ex(result,"category",STAGEINFO_CAT_UNSET);
        ignore_indexes=JFetch_ARRAY(result,"ignore_indexes");
        if(ignore_indexes)
        {
          cJSON_DetachItemFromObject(result, "ignore_indexes");
        }
        script_result=result;
        // cJSON_Delete(result);
        // delete buffer;

    }
    else
    {
      return;//Error
    }




  }

  {
    std::lock_guard<std::mutex> lock(bTrigInfoRecord_LOCK);
    vector<triggerInfoCacheData> cachedData;
    for(int i=0;i<bTrigInfoRecordBuffer.size();i++)
    {
      triggerInfoCacheData &info=bTrigInfoRecordBuffer[i];
      if(info.tid==trigger_id)
      {
        cachedData.push_back(info);
        info.tid=-1;//mark unocupied
      }
    }

    if(cachedData.size()>0)
    for(int i=0;i<group.size();i++)
    {

      auto d_img_info = dynamic_cast<StageInfo_Image*>(group[i].get());
      if(d_img_info==NULL)continue;
      

      for(int j=0;j<cachedData.size();j++)
      {
        triggerInfoCacheData info=cachedData[i];
        
        LOGI("tstmp_us:%" PRIu64 " trig_us:%" PRIu64,d_img_info->img_prop.fi.timeStamp_us,info.uInsp_time_us);
        if(d_img_info->img_prop.StreamInfo.camera==NULL)continue;

        CameraLayer::BasicCameraInfo camInfo=d_img_info->img_prop.StreamInfo.camera->getConnectionData();


        for(auto it = CamStampConvertSet.begin(); it != CamStampConvertSet.end(); ++it) {
          //check if camInfo.name contains it->first
          if(camInfo.name.find(it->first)==string::npos)continue;

          it->second=CamStampParamUpdate(it->second,info.uInsp_time_us,d_img_info->img_prop.fi.timeStamp_us);



        }
      }
    }




  }


    {

      bool doSave=false;
      if(catSum==STAGEINFO_CAT_OK &&  ImgSaveCountDown_OK>0)
      {
        ImgSaveCountDown_OK--;
        doSave=true;
      }
      if(catSum==STAGEINFO_CAT_NG &&  ImgSaveCountDown_NG>0)
      {
        ImgSaveCountDown_NG--;
        doSave=true;
      }
      if(catSum==STAGEINFO_CAT_NG2 &&  ImgSaveCountDown_NG2>0)
      {
        ImgSaveCountDown_NG2--;
        doSave=true;
      }
      if(catSum==STAGEINFO_CAT_NG3 &&  ImgSaveCountDown_NG3>0)
      {
        ImgSaveCountDown_NG3--;
        doSave=true;
      }
      if(catSum==STAGEINFO_CAT_NA &&  ImgSaveCountDown_NA>0)
      {
        ImgSaveCountDown_NA--;
        doSave=true;
      }


      if(doSave)
      {

        for(int i=0;i<group.size();i++)
        {

          auto d_img_info = dynamic_cast<StageInfo_Image*>(group[i].get());
          if(d_img_info==NULL)continue;
          LOGI("IMG>>src:%s",d_img_info->source_id.c_str());


          shared_ptr<StageInfo_Image> reportInfo(new StageInfo_Image());

          reportInfo->img_show=d_img_info->img_show;
          reportInfo->img_prop=d_img_info->img_prop;
          reportInfo->img=d_img_info->img;
          reportInfo->create_time_sysTick=d_img_info->create_time_sysTick;
          reportInfo->source=this;
          reportInfo->source_id=id;
          reportInfo->trigger_id=d_img_info->trigger_id;

          reportInfo->trigger_tags=d_img_info->trigger_tags;
          reportInfo->trigger_tags.push_back("CAT_"+to_string(catSum));
          reportInfo->trigger_tags.push_back("IMG_SAVE");

          belongMan->dispatch(reportInfo,NULL,"ImDataSave");


        }

      }





    }

    {
      bool doFetch=false;
      if(catSum==STAGEINFO_CAT_OK && FetchCountDown_OK!=0)
      {
        if(FetchCountDown_OK>0)
          FetchCountDown_OK--;
        doFetch=true;
      }
      if(catSum==STAGEINFO_CAT_NG && FetchCountDown_NG!=0)
      {
        if(FetchCountDown_NG>0)
          FetchCountDown_NG--;
        doFetch=true;
      }
      if(catSum==STAGEINFO_CAT_NG2 && FetchCountDown_NG2!=0)
      {
        if(FetchCountDown_NG2>0)
          FetchCountDown_NG2--;
        doFetch=true;
      }
      if(catSum==STAGEINFO_CAT_NG3 && FetchCountDown_NG3!=0)
      {
        if(FetchCountDown_NG3>0)
          FetchCountDown_NG3--;
        doFetch=true;
      }
      if(catSum==STAGEINFO_CAT_NA && FetchCountDown_NA!=0)
      {
        if(FetchCountDown_NA>0)
          FetchCountDown_NA--;
        doFetch=true;
      }


      if(doFetch)
      {
        std::lock_guard<std::mutex> lock(recentSrcLock); 

        if(recentSrcStageInfoSetIdx.space()==0)
        {//if full, wipe tail
          int tail_idx = recentSrcStageInfoSetIdx.getTail();
          recentSrcStageInfoSetIdx.consumeTail();
          recentSrcStageInfoSet[tail_idx].clear();
        }
        
        {//push new info in head
          int head_idx = recentSrcStageInfoSetIdx.getHead();
          recentSrcStageInfoSet[head_idx]=group;
          recentSrcStageInfoSetIdx.pushHead();

        }
      }
    }



    LOGI("final CAT:%d",catSum);



    shared_ptr<StageInfo_Category> reportInfo(new StageInfo_Category());
    reportInfo->category=catSum;
    reportInfo->process_time_us=0;
    reportInfo->trigger_id=trigger_id;
    reportInfo->source=this;
    reportInfo->source_id=id;
    reportInfo->trigger_tags.push_back(id);
    reportInfo->genJsonRepTojInfo();

    {
      cJSON* repInfoObj=JFetch_OBJECT(reportInfo->jInfo,"report");
      cJSON_AddItemToObject(repInfoObj,"ignore_indexes",ignore_indexes);
      cJSON_AddItemToObject(repInfoObj,"script_result",script_result);


      cJSON* jGRepsArr=cJSON_CreateArray();
      cJSON_AddItemToObject(repInfoObj,"group",jGRepsArr);

      for(int i=0;i<group.size();i++)
      {
        cJSON* srep=cJSON_CreateObject();
        group[i]->attachJsonRep(srep,0);
        cJSON_AddItemToArray(jGRepsArr,srep);
      }
    }
    // reportInfo->jInfo



    reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
    reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",1);


    belongMan->dispatch(reportInfo);

    if(TEST_mode.length()!=0)
    {
      if(TEST_mode=="OK_OK")
      {
        catSum=STAGEINFO_CAT_OK;
      }
      else if(TEST_mode=="NG_NG")
      {
        catSum=STAGEINFO_CAT_NG;
      }
      else if(TEST_mode=="NA_NA")
      {
        catSum=STAGEINFO_CAT_NA;
      }
      else if(TEST_mode=="OK_NG")
      {
        catSum=((TEST_mode_counter<TEST_mode_count1))?STAGEINFO_CAT_OK:STAGEINFO_CAT_NG;
      }
      else if(TEST_mode=="OK_NA")
      {
        catSum=((TEST_mode_counter<TEST_mode_count1))?STAGEINFO_CAT_OK:STAGEINFO_CAT_NA;
      }
      else if(TEST_mode=="NG_NA")
      {
        catSum=((TEST_mode_counter&1)==0)?STAGEINFO_CAT_NG:STAGEINFO_CAT_NA;
        catSum=((TEST_mode_counter<TEST_mode_count1))?STAGEINFO_CAT_NG:STAGEINFO_CAT_NA;
      }
      else if(TEST_mode=="OK_NG_NA")
      {
        int tt=TEST_mode_counter%3;
        catSum=STAGEINFO_CAT_OK;
        if(tt==1)catSum=STAGEINFO_CAT_NG;
        if(tt==2)catSum=STAGEINFO_CAT_NA;
      }
      TEST_mode_counter=(TEST_mode_counter+1)%TEST_mode_counter_MOD;

    }


    int send_trigger_id=trigger_id;

    if(processTimeRecord.find(trigger_id)!=processTimeRecord.end())
    {
      auto recTime=processTimeRecord[trigger_id];
      processTimeRecord.erase(trigger_id);
      auto curTime=cv::getTickCount();
      
      double timeDIff_ms = 1000*(curTime-recTime)/cv::getTickFrequency();

      LOGI("tid:%d processTime:%f",trigger_id,timeDIff_ms);

      if(processTime_MaxDelay<timeDIff_ms)
      {
        processTime_MaxDelay=timeDIff_ms;
      }

      processTime_AvgDelay=(processTime_AvgDelay*processTime_AvgDelay_Count+timeDIff_ms)/(++processTime_AvgDelay_Count);
      
      static int64 preT;
      if(processTime_LPDelay!=processTime_LPDelay)
      {
        processTime_LPDelay=timeDIff_ms;
      }
      else
      {
        float sampTDiff=(float)(curTime-preT)/cv::getTickFrequency();//sec
        float alpha=1-exp(-sampTDiff/0.05);
        processTime_LPDelay=processTime_LPDelay*(1-alpha)+timeDIff_ms*alpha;
      }


      if(timeDIff_ms>JFetch_NUMBER_ex(def,"overtime_ms",130))
      {

        LOGE("OVERTIME......");
        catSum=STAGEINFO_CAT_NA;
        processTime_OverTimeCount++;
        if(processTime_OverTimeCount>200)
        {
          send_trigger_id=9999999;
        }

        if(processTime_OverTimeCountMax<processTime_OverTimeCount)
        {
          processTime_OverTimeCountMax=processTime_OverTimeCount;
        }

      }
      else
      {
        processTime_OverTimeCount=0;
      }

      preT=curTime;
    }
    else
    {
      LOGI("tid:%d No processed time",trigger_id);
    }


    if(pCH && pCH->fastTestRetCatFlag!=STAGEINFO_CAT_UNSET)
    {
      LOGI("fastTestRetCatFlag:%d...trigger_id:%d.",pCH->fastTestRetCatFlag,trigger_id);
    }
    else if(trigger_id<0)
    {
      LOGI("TEST set don't send to Peripheral....");
    }
    else if(pCH)
    {
      switch ((catSum))
      {
      case STAGEINFO_CAT_OK:
        catSum=1;
        break;
      
      case STAGEINFO_CAT_NG:
      
        catSum=2;
        break;
      case STAGEINFO_CAT_NG2:
      
        catSum=3;
        break;
      case STAGEINFO_CAT_NG3:
      
        catSum=4;
        break;
      
      case STAGEINFO_CAT_NA:
      default://unknown
        catSum=0xFFFF;
        break;
      }
      
      std::lock_guard<std::mutex> lock(pCH->sendMutex); 
      
      cJSON *rep = cJSON_CreateObject();
      cJSON_AddStringToObject(rep,"type","report");
      cJSON_AddNumberToObject(rep,"tid",send_trigger_id);
      cJSON_AddNumberToObject(rep,"cat",catSum);

      uint8_t _buf[1000];
      // LOGE(">>>>>>>pCH:%p");
      int ret= sendcJSONTo_perifCH(pCH,_buf, sizeof(_buf),true,rep);
      cJSON_Delete(rep);

    }
    else
    {
      LOGI("Peripheral is not connected yet....");
    }

  }

};

class InspectionTarget_DataTransfer :public InspectionTarget_DataThreadedProcess
{
  public:

  static std::string TYPE(){ return "DataTransfer"; }
  InspectionTarget_DataTransfer(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
    :InspectionTarget_DataThreadedProcess(id,def,belongMan,local_env_path)
  {
    datTransferQueue.resize(300);
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

  int force_down_scale=-1;
  float downSampFactor=1;
  int downSampResolutionCap=500000;

  int queue_size_image_transfer_skip=10;
  int image_transfer_rest_ms=10;

  std::map<int, int64_t> imageSendLastTime_ms;
  bool exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
  {
    bool ret = InspectionTarget::exchangeCMD(info,id,act);
    if(ret)return ret;
    string type=JFetch_STRING_ex(info,"type");
    if(type=="force_down_scale")
    {
      force_down_scale=JFetch_NUMBER_ex(info,"scale",-1);
      return true;
    }
    if(type=="down_samp_factor")
    {
      downSampFactor=JFetch_NUMBER_ex(info,"factor",1);
      return true;
    }
    if(type=="down_samp_resolution_cap")
    {
      downSampResolutionCap=JFetch_NUMBER_ex(info,"cap",5000000);
      return true;
    }


    if(type=="queue_size_image_transfer_skip")
    {
      int queue_size_image_transfer_skip=JFetch_NUMBER_ex(info,"size",10);
      return true;
    }


    return false;
  }

  void thread_run()
  {
    
    acvImage cacheImage;
    while(true)
    {
      std::shared_ptr<StageInfo> curInput;
      // LOGI("<<<<<size():%d",datTransferQueue.size());
      // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test
            
      try{
        if(datTransferQueue.pop_blocking(curInput)==false)
        {
          break;
        }
      }
      catch(TS_Termination_Exception e)
      {
        break;
      }


      int DBG_USECOUNT=curInput.use_count();
      // LOGI("<<<<<size():%d usecount:%d",datTransferQueue.size(),DBG_USECOUNT);

      // LOGI("DataTransfer thread pop data: name:%s ",curInput->source_id.c_str());
      // LOGI("DataTransfer thread pop data: type:%s ",curInput->typeName().c_str());

      // for ( const auto &kyim : curInput->imgSets ) {
      //     LOGI("[%s]:%p",kyim.first.c_str(),kyim.second.get());
      // }

      // LOGI("curInput->jInfo:%p ",curInput->jInfo);
      int imgCHID=curInput->img_prop.StreamInfo.channel_id;
      int downSample=curInput->img_prop.StreamInfo.downsample;
      if(downSample<1)
      {
        downSample=1;
      }
      //downSampleAdj=datTransferQueue.size();

      // int sameCHID_inQ_count=datTransferQueue.size();
      // sameCHID_inQ_count-=5;
      // if(sameCHID_inQ_count<0)sameCHID_inQ_count=0;
      // // {

      // //   int QL=datTransferQueue.size();
      // //   for(int i=0;i<QL;i++)
      // //   {
      // //     datTransferQueue.
      // //   }
      // // }

      // downSample+=(int)(sameCHID_inQ_count*downSampFactor);
      // curInput->imgSets./

      

      // LOGI("imgCHID:%d ",imgCHID);

      {
        // CameraLayer::BasicCameraInfo data=headImgPipe->img_prop.StreamInfo.camera->getConnectionData();
        // cJSON* caminfo=CameraManager::cameraInfo2Json(data);


        cJSON* camBrifInfo=cJSON_CreateObject();
        // cJSON_AddStringToObject(camBrifInfo, "trigger_tag", curInput->trigger_tag.c_str());
        cJSON_AddNumberToObject(camBrifInfo, "trigger_id", curInput->trigger_id);
        // cJSON_AddStringToObject(camBrifInfo, "camera_id",curInput->camera_id.c_str());

        bpg_pi.fromUpperLayer_DATA("CM",imgCHID,camBrifInfo);
        cJSON_Delete(camBrifInfo);


        if(curInput->jInfo)
          bpg_pi.fromUpperLayer_DATA("RP",imgCHID,curInput->jInfo);


        std::shared_ptr<acvImage> im2send=curInput->img_show;

        LOGE("downSample %d src:%s im2send:%p",downSample,curInput->source_id.c_str(),im2send.get());

        bool okToSend=(im2send && downSample<5);
        


        auto curTime=cv::getTickCount();

        if(okToSend)
        { 
          int diffTime_ms=99999;

          if(imageSendLastTime_ms.find(imgCHID)==imageSendLastTime_ms.end())
          {
        // double timeDIff_ms = 1000*(curTime-recTime)/cv::getTickFrequency();
            imageSendLastTime_ms[imgCHID]=curTime;
          }
          else
          {
            diffTime_ms=1000*(curTime-imageSendLastTime_ms[imgCHID])/cv::getTickFrequency();
          }

          if(diffTime_ms>500)//just send
          {
            okToSend=true;
          }
          else if(datTransferQueue.size()>queue_size_image_transfer_skip)
          {
            okToSend=false;
          }

          if(okToSend)
          {
            imageSendLastTime_ms[imgCHID]=curTime;
          }

        }

        if(okToSend)
        {

          int W=im2send->GetWidth();
          int H=im2send->GetHeight();
          int downSampleAdj=(int)round(sqrt(W*H/(5000000)));
          LOGI("%dx%d=%d,ds:%d",W,H,W*H,downSampleAdj);
          if(downSampleAdj<1)downSampleAdj=1;

          Mat _CV_Img(im2send->GetHeight(),im2send->GetWidth(),CV_8UC3,im2send->CVector[0]);

          int dsW=im2send->GetWidth()/downSampleAdj;
          int dsH=im2send->GetHeight()/downSampleAdj;
          Mat CV_Img=_CV_Img;
          if(downSampleAdj==1)
          {
            CV_Img=_CV_Img;
          }
          else
          {

            resize(_CV_Img, CV_Img, Size(dsW,dsH), INTER_LINEAR);
          }


          int compressionRate=95-datTransferQueue.size()*5;
          if(compressionRate<20)compressionRate=20;

          vector<unsigned char> img_encode;
          string base64Header="";
          if(1)//use jpg
          {
            std::vector<int> param(2);
            param[0] = cv::IMWRITE_JPEG_QUALITY;
            param[1] = compressionRate;//default(95) 0-100


            cv::imencode(".jpeg", CV_Img, img_encode, param);
            base64Header="data:image/jpg;base64,";
          }
          else//webp slow
          {
            std::vector<int> param(2);
            param[0] = cv::IMWRITE_WEBP_QUALITY;
            param[1] = compressionRate;//default(95) 0-100
            cv::imencode(".webp", CV_Img, img_encode, param);
            base64Header="data:image/webp;base64,";
          }

        // LOGE("OK..");
          //convert img_encode into base64 format
          string base64_img_encode=base64_encode(base64Header.c_str(),(unsigned char*)img_encode.data(), img_encode.size());

        // LOGE("OK..");
          if(base64_img_encode.length()>0)
          {
          // LOGI("base64_img_encode:%c %c...",base64_img_encode[0],base64_img_encode[1]);

            // printf("\n\nbase64_img_encode_size:%d\n\n",base64_img_encode.size());
            // printf(base64_img_encode.c_str());
            // printf("\n\n\n\n");
            BPG_protocol_data_ImgB64_Send_info imgb64Info;

            imgb64Info.imgb64=&base64_img_encode[0];
            imgb64Info.imgb64_L=base64_img_encode.length();
            imgb64Info.offsetX=imgb64Info.offsetY=0;
            imgb64Info.fullHeight=CV_Img.rows;
            imgb64Info.fullWidth=CV_Img.cols;
            imgb64Info.scale=downSampleAdj;
            
            bpg_pi.fromUpperLayer_DATA("IM",imgCHID,&imgb64Info);
            
          }


          LOGI("COMPRESSION:: %dx%d =>%dx%d ds:%d  jpeg:rate:%d size:%d",
            im2send->GetWidth(),im2send->GetHeight(),dsW,dsH,downSampleAdj,compressionRate,img_encode.size());

          if(image_transfer_rest_ms>0)
          {
            std::this_thread::sleep_for(std::chrono::milliseconds(image_transfer_rest_ms));
          }

        }
        else
        {
        }


        if(0&&im2send!=NULL && force_down_scale<999)
        {

          

          float pscale=sqrt(im2send->GetHeight()*im2send->GetWidth()/(downSampResolutionCap));
          int new_downSample=pscale<=0?1:(int)pscale;
          if(downSample<new_downSample)downSample=new_downSample;
          if(force_down_scale!=-1)downSample=force_down_scale;
          BPG_protocol_data_acvImage_Send_info iminfo = {img : &cacheImage, scale : (uint16_t)downSample};
          iminfo.fullHeight = im2send->GetHeight();
          iminfo.fullWidth = im2send->GetWidth();
          if(iminfo.scale>1)
          {
            //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
            //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(cacheImage, *im2send, iminfo.scale, NULL);
          }
          else
          {
            iminfo.scale=1;
            iminfo.img=im2send.get();
          }
          bpg_pi.fromUpperLayer_DATA("IM",imgCHID,&iminfo);

        }
        
        // LOGE("OK..");
        bpg_pi.fromUpperLayer_SS(imgCHID,true);
        
        if(realTimeDropFlag>0)
          realTimeDropFlag--;

        // LOGE("OK..");
      }
    
      


    }
  }




};





class InspectionTarget_StageInfoImageSave :public InspectionTarget_DataThreadedProcess
{
  public:
  std::string mark="SIIS";
  static std::string TYPE(){ return "StageInfoImageSave"; }
  InspectionTarget_StageInfoImageSave(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
    :InspectionTarget_DataThreadedProcess(id,def,belongMan,local_env_path),mark("SIIS")
  {
    datTransferQueue.resize(9999);
  }




  virtual int processInputPool()
  {

    int ret = InspectionTarget_DataThreadedProcess::processInputPool();

    // LOGI("PPUSH:datTransferQueue:size:%d",datTransferQueue.size());
    return ret;
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

  long long timeInMilliseconds(void) {
      struct timeval tv;

      gettimeofday(&tv,NULL);
      return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
  }
  bool exchangeCMD(cJSON* info,int info_ID,exchangeCMD_ACT &act)
  {
    std::string type=JFetch_STRING_ex(info,"type");

    return InspectionTarget_DataThreadedProcess::exchangeCMD(info,info_ID,act);
  }
  int saveCount=0;
  void thread_run()
  {
    
    // acvImage cacheImage;
    while(true)
    {
      std::shared_ptr<StageInfo> curInput;
      // LOGI("<<<<<size():%d",datTransferQueue.size());
      // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test
            
      try{
        // LOGI("TryReadNew");
        if(datTransferQueue.pop_blocking(curInput)==false)
        {
          LOGI("TryReadTailed");
          break;
        }
      }
      catch(TS_Termination_Exception e)
      {
        LOGI("TS_Termination_Exception");
        break;
      }


      auto d_sinfo = dynamic_cast<StageInfo_Image *>(curInput.get());
      if(d_sinfo==NULL) {
        LOGE("sinfo type is StageInfo_Image only, does not match.....");
        continue;
      }
      std::vector<std::string> &tags=d_sinfo->trigger_tags;

      bool doSkip=false;
      for(int i=0;i<tags.size();i++)
      {
        if(tags[i]==mark)
        {//this is from saved image here
          doSkip=true;
          break;
        }
      }
      if(doSkip)continue;


      //[ms][trigger id][tags]
      //t:3409329 tid:451 tags:test1,fff,wfgh
      
      int trigID=d_sinfo->trigger_id;

      string tags_str=mark;
      for(int i=0;i<tags.size();i++)
      {
        tags_str+=","+tags[i];
      }
      std::string filename="tid="+to_string(trigID)+" tags="+tags_str+" t="+to_string(timeInMilliseconds())+".png";

      LOGI("SAVE image filename:%s ",filename.c_str());
      LOGI("local_env_path:%s ",local_env_path.c_str());


      auto srcImg=d_sinfo->img;

      Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);
        
      imwrite(local_env_path+"/"+filename, CV_srcImg);  
      saveCount++;
      LOGI("SAVE image DONE c:%d",saveCount);
    }


    LOGI("InspectionTarget_StageInfoImageSave ended.....");
  }




};






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


class exchangeCMD_ACTx: public exchangeCMD_ACT
{ 
  acvImage dataSend_buff;
  virtual void send(const char *TL, int pgID,cJSON* def){
    
    bpg_pi.fromUpperLayer_DATA(TL,pgID,def);

  };
  virtual void send(const char *TL, int pgID,acvImage* img,int downSample){
    acvImage* p_img=img;
    if(p_img==NULL)return;
    BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)downSample};

    iminfo.fullHeight = p_img->GetHeight();
    iminfo.fullWidth = p_img->GetWidth();
    iminfo.offsetX=0;
    iminfo.offsetY=0;
    if(downSample<=1)
    {
      iminfo.scale=1;
      iminfo.img=p_img;
      bpg_pi.fromUpperLayer_DATA(TL,pgID,&iminfo);
      return;
    }


    //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
    //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
    ImageDownSampling(dataSend_buff, *p_img, iminfo.scale, NULL);

    bpg_pi.fromUpperLayer_DATA(TL,pgID,&iminfo);
  };

};

exchangeCMD_ACTx exchCMDact;

int m_BPG_Protocol_Interface::toUpperLayer(BPG_protocol_data bpgdat) 
{
  //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);
  exchCMDact.doSendAck=true;
    BPG_protocol_data *dat = &bpgdat;

    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //      dat->pgID);
    cJSON *json = cJSON_Parse((char *)dat->dat_raw);
    char err_str[500] = "\0";
    bool session_ACK = false;
    bool noInstantACK = false;
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
          if (strcmp(type, "rename") == 0)
          {

            string fromFileName = JFetch_STRING_ex(json, "from");
            if(fromFileName.length()>0)
            {
              session_ACK=(rename(fromFileName.c_str(), fileName)==0);
            }


          }
        
        }
        else
        {

          // LOGI("DataType_BPG>>BIN>>%s", byteArrString(dat->dat_raw + strinL, dat->size - strinL));

          FILE *write_ptr;

          //In windows we disable the readonly flag
          #ifdef _WIN32
            _chmod(fileName, _S_IWRITE);
          #endif

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
        LOGE("===========start_stream===========");
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
          double *gamma = JFetch_NUMBER(json, "gamma");
          if(gamma)
          {
            cami->camera->SetGamma(*gamma);
            LOGI("gamma:%f",*gamma);
          }
        }



        {
          double *val = JFetch_NUMBER(json, "trigger_mode");
          if (val)
          {
            LOGE("trigger_mode:%f",*val);
            if(cami->camera->TriggerMode((int)*val)==CameraLayer::NAK)
            {
              LOGE("FAILED");
            }
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


          char *FormatStr = JFetch_STRING(json, "FormatStr");
          if(FormatStr)
          {
            cami->camera->SetFormatStr(string(FormatStr));
            LOGI("FormatStr:%s",FormatStr);
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


          {
            cJSON *roi=JFetch_OBJECT(json,"ROI");
            if(roi)
            {
              double x=JFetch_NUMBER_ex(roi, "x");
              double y=JFetch_NUMBER_ex(roi, "y");
              double w=JFetch_NUMBER_ex(roi, "w");
              double h=JFetch_NUMBER_ex(roi, "h");
              if(x==x && y==y && w==w && h==h)
              {
                
                cami->camera->SetROI((int)x,(int)y,(int)w,(int)h,0,0);
              }
              LOGI("ROI: %f,%f,%f,%f<<<<",x,y,w,h);
            }

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


          double *pixel_size = JFetch_NUMBER(json, "pixel_size");
          if(pixel_size)
          {
            cami->camera->SetPixelSize (*pixel_size);
            LOGI("pixel_size:%f",*pixel_size);
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
      else if(strcmp(type_str, "trigger_info_mocking") ==0)
      {
        char *_cam_id = JFetch_STRING(json, "id");
        if(_cam_id!=NULL)
        {
          std::string id = std::string(_cam_id);
          if(JFetch_TRUE(json, "remove"))
          {

            session_ACK = triggerMockFlags.erase(id)>0;
            
          }
          else
          {
            // triggerMockFlags.insert({id,JFetch_STRING_ex(json, "tag")});
            triggerMockFlags[id]=JFetch_STRING_ex(json, "tag");
            session_ACK = true;
          }
        }
      }
      else if(strcmp(type_str, "trigger") ==0)
      {do{
        char *_img_path = JFetch_STRING(json, "img_path");
        if(_img_path)
        {
          
          vector<string> tags;
          {

            cJSON* trigger_tags = JFetch_ARRAY(json, "trigger_tags");
            if(trigger_tags)
            {
              int tagsLen=cJSON_GetArraySize(trigger_tags);
              for(int i=0;i<tagsLen;i++)
              {
                cJSON * item= cJSON_GetArrayItem(trigger_tags,i);
                if(item->type & cJSON_String)
                {
                  tags.push_back(string(item->valuestring));
                }
              }
            }
          }

          LOGI("_img_path:%s tags:%d",_img_path,tags.size());




          
          int ret=
            ReadImageAndPushToInspQueue(
              std::string(_img_path),
              tags,
              (int)JFetch_NUMBER_ex(json, "trigger_id",-1),
              (int)JFetch_NUMBER_ex(json,"channel_id",-1));
        }
        else
        {

          char *_cam_id = JFetch_STRING(json, "id");
          if(_cam_id==NULL)break;
          std::string id=std::string(_cam_id);
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

          if(JFetch_TRUE(json, "mocking_trigger_info"))
          {
            
            sttriggerInfo_mix mocktrig;
            mocktrig.stInfo=NULL;
            mocktrig.triggerInfo.camera_id=id;
            mocktrig.triggerInfo.trigger_id=(int)JFetch_NUMBER_ex(json, "trigger_id",-1);
            mocktrig.triggerInfo.est_trigger_time_us=0;//force matching
            mocktrig.triggerInfo.tags.push_back( std::string( JFetch_STRING_ex(json, "trigger_tag","_SW_TRIG_")) );
            
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
      // {
      //   char* jsonStr=cJSON_Print(json);
      //   LOGI("jsonStr:\n%s",jsonStr);
      //   delete jsonStr;
      // }




      if(strcmp(type_str, "create") ==0)
      {
        char *_id = JFetch_STRING(json, "id");
        if(_id==NULL)break;
        std::string id=std::string(_id);


        InspectionTarget* iptar=inspTarMan.getInspTar(id);

        cJSON *defInfo = JFetch_OBJECT(json, "definfo");

        if(iptar!=NULL)
        {
          snprintf(err_str, sizeof(err_str), "InspTar create: id:%s is already existed", _id);
          
          LOGI(">err_str:%s",err_str);
          break;
        }


        std::string env_path = JFetch_STRING_ex(json, "env_path");

        InspectionTarget* inspTar=NULL;
        if(defInfo!=NULL)
        {
          std::string type=JFetch_STRING_ex(defInfo,"type");
        
          std::string id=JFetch_STRING_ex(defInfo,"id");
          
          LOGI(">>>id:%s Add type:%s",id.c_str(),type.c_str());
          if(type==InspectionTarget_ColorRegionDetection::TYPE())
          {
            inspTar = new InspectionTarget_ColorRegionDetection(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_TEST_IT::TYPE())
          {
            inspTar = new InspectionTarget_TEST_IT(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_DataTransfer::TYPE())
          {
            inspTar = new InspectionTarget_DataTransfer(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_StageInfoImageSave::TYPE())
          {
            inspTar = new InspectionTarget_StageInfoImageSave(id,defInfo,&inspTarMan,env_path);
          }
          
          else if(type==InspectionTarget_ReduceCategorize::TYPE())
          {
            inspTar = new InspectionTarget_ReduceCategorize(id,defInfo,&inspTarMan,env_path);
          }
          
          else if(type==InspectionTarget_GroupResultSave::TYPE())
          {
            inspTar = new InspectionTarget_GroupResultSave(id,defInfo,&inspTarMan,env_path);
          }

          else if(type==InspectionTarget_Orientation_ColorRegionOval::TYPE())
          {
            inspTar = new InspectionTarget_Orientation_ColorRegionOval(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_SurfaceCheckSimple::TYPE())
          {
            inspTar = new InspectionTarget_SurfaceCheckSimple(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_Orientation_ShapeBasedMatching::TYPE())
          {
            inspTar = new InspectionTarget_Orientation_ShapeBasedMatching(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_JSON_Peripheral::TYPE())
          {
            inspTar = new InspectionTarget_JSON_Peripheral(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_JSON_CNC_Peripheral::TYPE())
          {
            inspTar = new InspectionTarget_JSON_CNC_Peripheral(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_StageInfoCollect_Base::TYPE())
          {
            inspTar = new InspectionTarget_StageInfoCollect_Base(id,defInfo,&inspTarMan,env_path);
          }
          else if(type==InspectionTarget_SpriteDraw::TYPE())
          {
            inspTar = new InspectionTarget_SpriteDraw(id,defInfo,&inspTarMan,env_path);
          }
          // else if(type==InspectionTarget_ImgSrc::TYPE())
          // {
          //   inspTar = new InspectionTarget_ImgSrc(id,defInfo,&inspTarMan,env_path);
          // }

          
          else
          {
            //failed
          }
        }


        if(inspTar)
        {
          session_ACK=inspTarMan.addInspTar(inspTar,id);
        }

        if(session_ACK==false)delete inspTar;
      }
      else if(strcmp(type_str, "update")==0)
      {
         char *_id = JFetch_STRING(json, "id");
        if(_id==NULL)break;
        std::string id=std::string(_id);


        InspectionTarget* iptar=inspTarMan.getInspTar(id);

        cJSON *defInfo = JFetch_OBJECT(json, "definfo");
        if(iptar==NULL)
        {
          snprintf(err_str, sizeof(err_str), "InspTar update: id:%s is not found", _id);
          LOGI(">err_str:%s",err_str);
          break;
        }
        if(defInfo==NULL)
        {
          snprintf(err_str, sizeof(err_str), "InspTar update: defInfo is not found");
          LOGI(">err_str:%s",err_str);
          break;
        }



        session_ACK=true;
        iptar->setInspDef(defInfo);

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
        
        LOGI("delete_all");
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
        InspectionTarget *insptar= inspTarMan.getInspTar(std::string(_id));
        if(insptar==NULL)break;

        session_ACK=insptar->exchangeCMD(JFetch_OBJECT(json,"data"),dat->pgID,exchCMDact);
        
        if(exchCMDact.doSendAck==false)
        {
          noInstantACK=true;
        }
        // if(reply!=NULL)
        // {
        //   session_ACK=true;
        //   fromUpperLayer_DATA("IT",dat->pgID,reply);
        //   cJSON_Delete(reply);
        // }

        // if(JFetch_NUMBER(json,"channel_id")!=NULL)//if there is channel_id in json, then exchangeCMD has to deal with reply itself
        // {
        //   noInstantACK=true;
        // }


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
      int CONN_ID=JFetch_NUMBER_ex(json, "CONN_ID",-1);

      do{
        if(CONN_ID==-1)break;
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
            PerifChannel* pch=new PerifChannel();
            perifCH=pch;
            pch->ID=avail_CONN_ID;
            pch->conn_pgID=dat->pgID;
            pch->setDLayer(PHYLayer);

            pch->send_RESET();
            pch->send_RESET();
            pch->RESET();
 

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
    
    
    if(noInstantACK==false)
    {
      sprintf(tmp, "{\"start\":false,\"cmd\":\"%c%c\",\"ACK\":%s,\"errMsg\":\"%s\"}",
              dat->tl[0], dat->tl[1], (session_ACK) ? "true" : "false", err_str);
      fromUpperLayer_DATA("SS",dat->pgID,tmp);
    }
    
    
    
    
    cJSON_Delete(json);
  }
  while(0);
  // if (doExit)
  // {
  //   exit(0);
  // }

  return 0;
}

int sendResultTo_perifCH(Data_JsonRaw_Layer *perifCH,int uInspStatus, uint64_t timeStamp_100us,int count)
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
int mainLoop(int chPort = 4090)
{
  /**/

  LOGI(">>>>>\n");
  bool pass = false;
  int retryCount = 0;
  while (!pass && !terminationFlag)
  {
    try
    {
      LOGI("Try to open websocket... port:%d\n", chPort);
      ifwebsocket=new m_BPG_Link_Interface_WebSocket(chPort);

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
  // std::thread ActionThread(ImgPipeDatViewThread, &terminationFlag);
  // setThreadPriority(ActionThread, SCHED_RR, 0);
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
    inspTarMan.clearInspTar();
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
  int chPort=4090;
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



    {
      str=PatternRest(argv[i], "port=");
      if(str!=NULL)
      {
        chPort=atoi(str);
        doMatch=true;
      }
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
  return mainLoop(chPort);
}
