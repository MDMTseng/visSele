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

#include "InspectionTarget.hpp"
#include "InspTars.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>

using namespace cv;
#define _VERSION_ "1.2"
std::timed_mutex mainThreadLock;


m_BPG_Protocol_Interface bpg_pi;



m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;

struct sttriggerInfo_mix{
  std::shared_ptr<StageInfo_Image>stInfo;
  
  struct _triggerInfo{

    int trigger_id;
    std::string trigger_tag;
    std::string camera_id;
    uint64_t est_trigger_time_us;
  };
  _triggerInfo triggerInfo;
};
// vector<sttriggerInfo_mix> triggerInfoBuffer(10);
// TSQueue<sttriggerInfo_mix> triggerInfoQueue(10);





TSQueue<sttriggerInfo_mix> triggerInfoMatchingQueue(10);
TSVector<sttriggerInfo_mix> triggerInfoMatchingBuffer;




TSQueue<std::shared_ptr<StageInfo_Image>> inspQueue(10);
// TSQueue<image_pipe_info *> datViewQueue(10);
// TSQueue<image_pipe_info *> inspSnapQueue(5);



uint64_t lastImgSendTime=0;

class InspectionTargetManager_m:public InspectionTargetManager
{
  vector<uint8_t> frameDataBuff;


  void CamStream_CallBack(CameraManager::StreamingInfo &info){


    
    uint64_t cur_ms = current_time_ms();
    uint64_t cur_Interval =cur_ms-lastImgSendTime;
    if(lastImgSendTime==0)
    {
      cur_Interval=1;
    }
    lastImgSendTime=cur_ms;


    LOGE("============DO INSP>> waterLvL: insp:%d/%d  trigInfoMatchingSize:%d  cur_Interval:%" PRIu64 "<<<cur_ms:%" PRIu64 "",
        inspQueue.size(), inspQueue.capacity(),triggerInfoMatchingBuffer.size(),cur_Interval,cur_ms);

    // LOGE("============DO INSP>> waterLvL: insp:%d/%d dview:%d/%d  snap:%d/%d   poolSize:%d trigInfoMatchingSize:%d",
    //     inspQueue.size(), inspQueue.capacity(),
    //     datViewQueue.size(), datViewQueue.capacity(),
    //     inspSnapQueue.size(), inspSnapQueue.capacity(),
    //     bpg_pi.resPool.rest_size(),triggerInfoMatchingBuffer.size());

    std::shared_ptr<StageInfo_Image> newStateInfo(new StageInfo_Image());

    CameraLayer::frameInfo finfo = info.camera->GetFrameInfo();

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
    newStateInfo->img=img;
    // LOGI(">>>CAM:%s  WH:%d %d",info->camera_id.c_str(),finfo.width,finfo.height);

    sttriggerInfo_mix pmix;
    pmix.stInfo=newStateInfo;
    
      LOGI("0");
    triggerInfoMatchingQueue.push_blocking(pmix);
      LOGI("1");

  }

};


int ReadImageAndPushToInspQueue(string path,string camera_id,string trigger_tag,int trigger_id,int channel_id)
{
  
  std::shared_ptr<StageInfo_Image> newStateInfo(new StageInfo_Image());
  if(newStateInfo==NULL)return -1;

  newStateInfo->img_prop.StreamInfo.camera=NULL;
  newStateInfo->img_prop.StreamInfo.channel_id=channel_id;
  newStateInfo->trigger_tags.push_back(trigger_tag);

  Mat mat=imread(path.c_str());

  int H = mat.rows;
  int W = mat.cols;
  CameraLayer::frameInfo finfo;
  finfo.offset_x=finfo.offset_y=0;
  finfo.height=H;
  finfo.width=W;
  finfo.timeStamp_us=0;
  newStateInfo->img_prop.mmpp=0;
  newStateInfo->img_prop.fi=finfo;

  std::shared_ptr<acvImage> img(new acvImage(W,H,3));
  newStateInfo->img=img;
  
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

  inspQueue.push_blocking(newStateInfo);

  return 0;
}





InspectionTargetManager_m inspTarMan;



std::map<std::string, std::string> triggerMockFlags;

bool cleanUp_triggerInfoMatchingBuffer_UNSAFE()
{
  int zeroCount=0;
  for(int i=0;i<3;i++)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  for(int i=0;i<triggerInfoMatchingBuffer.size();i++)
  {
    if(triggerInfoMatchingBuffer[i].stInfo!=NULL)
    {
      // bpg_pi.resPool.retResrc(triggerInfoMatchingBuffer[i].pipeInfo);


      //since triggerInfoMatchingBuffer[i].stInfo is not yet dispatched, it shouldn't use recycle to delete, so deal(DELETE) it here 
      // inspTarMan.recycleStageInfo(triggerInfoMatchingBuffer[i].stInfo);
      triggerInfoMatchingBuffer[i].stInfo=NULL;
    }
  }
  triggerInfoMatchingBuffer.clear();
  triggerMockFlags.clear();
  return true;
}


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
          
          bool doMockTriggerInfo=(targetStageInfo->StreamInfo.camera!=NULL) && (triggerMockFlags.find(targetStageInfo->StreamInfo.camera->getConnectionData().id) != triggerMockFlags.end()) ;

          LOGI("  doMockTriggerInfo:%d cam:%p mode:%d ",doMockTriggerInfo,  targetStageInfo->img_prop.StreamInfo.camera  ,targetStageInfo->img_prop.StreamInfo.camera->triggerMode);

          if(doMockTriggerInfo)
          {
            sttriggerInfo_mix::_triggerInfo mocktrig;
            mocktrig.camera_id=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id;
            
            mocktrig.est_trigger_time_us=targetStageInfo->img_prop.fi.timeStamp_us+123;//
            mocktrig.trigger_id=-1;
            std::string mockTag=triggerMockFlags[mocktrig.camera_id];
            mocktrig.trigger_tag=(mockTag.length() ==0)?"_STREAM_":mockTag;
            
            // triggerInfoMatchingBuffer.w_unlock();
            triggerInfoMatchingBuffer.push_back({triggerInfo:mocktrig});
            // triggerInfoMatchingBuffer.w_lock();
          }

          
          for(int i=0;i<triggerInfoMatchingBuffer.size();i++)//try to find trigger info matching
          {//
            if(triggerInfoMatchingBuffer[i].stInfo!=NULL)continue;//skip image/pipe info

            auto _triggerInfo=triggerInfoMatchingBuffer[i].triggerInfo;
           
            if(_triggerInfo.camera_id!=targetStageInfo->img_prop.StreamInfo.camera->getConnectionData().id)continue;//camera id is not match


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
            if(targetTriggerInfo.camera_id!=_stInfo->img_prop.StreamInfo.camera->getConnectionData().id)continue;//camera id is not matching

            
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





        if( minMatchingIdx!=-1 && minMatchingCost<1000)
        {
          LOGI("Get matching. idx:%d cost:%d  psss to next Q stInfo:%p",minMatchingIdx,minMatchingCost,targetStageInfo.get() );
          targetStageInfo->trigger_tags.push_back(targetTriggerInfo.trigger_tag);
          targetStageInfo->trigger_tags.push_back(targetTriggerInfo.camera_id);
          LOGI("cam id:%s  ch_id:%d",targetTriggerInfo.camera_id.c_str(),targetStageInfo->img_prop.StreamInfo.channel_id);
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
      LOGI("id:%d trigger:",stInfo->trigger_id);
      for(auto tag:stInfo->trigger_tags)
        LOGI("%s",tag.c_str());
      int acceptCount=inspTarMan.dispatch(stInfo);
      LOGI("acceptCount:%d",acceptCount);
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

      LOGI("......<>>>>>.....");

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


void InspectionTarget_DataTransfer::thread_run()
{
  
  acvImage cacheImage;
  while(true)
  {
    std::shared_ptr<StageInfo> curInput;
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




    LOGI("DataTransfer thread pop data: name:%s type:%s ",curInput->source_id.c_str(),curInput->typeName().c_str());

    // for ( const auto &kyim : curInput->imgSets ) {
    //     LOGI("[%s]:%p",kyim.first.c_str(),kyim.second.get());
    // }

    LOGI("curInput->jInfo:%p ",curInput->jInfo);
    int imgCHID=curInput->img_prop.StreamInfo.channel_id;
    int downSample=curInput->img_prop.StreamInfo.downsample;
    if(downSample<1)
    {
      downSample=10;
    }
    // curInput->imgSets./

    LOGI("imgCHID:%d ",imgCHID);


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


      std::shared_ptr<acvImage> im2send=curInput->img;
      if(im2send!=NULL)
      {
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
      
      bpg_pi.fromUpperLayer_SS(imgCHID,true);
      
      if(realTimeDropFlag>0)
        realTimeDropFlag--;


    }
  }
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

          {
            
            sttriggerInfo_mix mocktrig;
            mocktrig.stInfo=NULL;
            mocktrig.triggerInfo.camera_id=id;
            mocktrig.triggerInfo.trigger_id=(int)JFetch_NUMBER_ex(json, "trigger_id",-1);
            mocktrig.triggerInfo.est_trigger_time_us=0;//force matching
            mocktrig.triggerInfo.trigger_tag=JFetch_STRING_ex(json, "trigger_tag","_SW_TRIG_");
            
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

        InspectionTarget* inspTar=NULL;
        if(defInfo!=NULL)
        {
          std::string type=JFetch_STRING_ex(defInfo,"type");
        
          std::string id=JFetch_STRING_ex(defInfo,"id");
          
          LOGI(">>>id:%s Add type:%s",id.c_str(),type.c_str());
          if(type==InspectionTarget_ColorRegionDetection::TYPE())
          {
            inspTar = new InspectionTarget_ColorRegionDetection(id,defInfo,&inspTarMan);
          }
          else if(type==InspectionTarget_TEST_IT::TYPE())
          {
            inspTar = new InspectionTarget_TEST_IT(id,defInfo,&inspTarMan);
          }
          else if(type==InspectionTarget_DataTransfer::TYPE())
          {
            inspTar = new InspectionTarget_DataTransfer(id,defInfo,&inspTarMan);
          }
          else if(type==InspectionTarget_Orientation_ColorRegionOval::TYPE())
          {
            inspTar = new InspectionTarget_Orientation_ColorRegionOval(id,defInfo,&inspTarMan);
          }
          else if(type==InspectionTarget_SurfaceCheckSimple::TYPE())
          {
            inspTar = new InspectionTarget_SurfaceCheckSimple(id,defInfo,&inspTarMan);
          }
          else if(type==InspectionTarget_Orientation_ShapeBasedMatching::TYPE())
          {
            inspTar = new InspectionTarget_Orientation_ShapeBasedMatching(id,defInfo,&inspTarMan);
          }
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
