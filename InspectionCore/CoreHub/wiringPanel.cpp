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

#define _VERSION_ "1.2"
std::timed_mutex mainThreadLock;







m_BPG_Protocol_Interface bpg_pi;



m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;

class InspectionTarget
{
  CameraLayer *camera=NULL;
  cJSON *InspConf=NULL;
  std::timed_mutex rsclock;
  public:
  int channel_id;
  
  InspectionTarget()
  {
    // this->camera=camera;
    // InspConf=inspectionConfig;
  }

  void assignCamera(CameraLayer *cam)
  {
    camera=cam;
  }

  void setInspDef(cJSON* json)
  {
  }

  cJSON* getInfo_cJSON()
  {
    cJSON *obj=cJSON_CreateObject();

    {
      cJSON *camInfo = cJSON_Parse(camera->getCameraJsonInfo().c_str());
      cJSON_AddItemToObject(obj, "camera", camInfo);
    }

    {
      cJSON *otherInfo=cJSON_CreateObject();
      cJSON_AddItemToObject(obj, "inspInfo", otherInfo);
    }

    {
      cJSON_AddNumberToObject(obj, "channel_id", channel_id);
    }
    return obj;
  }



  InspectionTarget_EXCHANGE excdata={0};
  acvImage img;
  acvImage img_buff;
  InspectionTarget_EXCHANGE* setInfo(InspectionTarget_EXCHANGE* info)
  {
    rsclock.lock();
    cJSON * json=info->info;

    char *insp_type = JFetch_STRING(json, "insp_type");

    memset(&excdata,0,sizeof(InspectionTarget_EXCHANGE));


    if(strcmp(insp_type, "snap") ==0)
    {
      LOGI(">>>>>");
      int ret= getImage(camera,&img,0,-1);
      if(ret!=0)
      {
        return NULL;
      }
      LOGI(">>>>>");
      BPG_protocol_data_acvImage_Send_info imgInfo;
      imgInfo.img=&img;
      imgInfo.scale=1;
      imgInfo.offsetX=0;
      imgInfo.offsetY=0;
      imgInfo.fullHeight=img.GetHeight();
      imgInfo.fullWidth=img.GetWidth();


      excdata.imgInfo=imgInfo;
      excdata.isOK=true;


      LOGI(">>>>>");
      return &excdata;
    }

    if(strcmp(insp_type, "start_stream") ==0)
    {
      camera->TriggerMode(0);
      excdata.isOK=true;
      return &excdata;
    }
    if(strcmp(insp_type, "stop_stream") ==0)
    {
      camera->TriggerMode(2);
      excdata.isOK=true;
      return &excdata;
    }
    excdata.isOK=false;
    return &excdata;

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
  

  static CameraLayer::status sCAM_CallBack(CameraLayer &cl_obj, int type, void *context)
  {
    InspectionTarget *itm=(InspectionTarget*)context;
    itm->CAM_CallBack(cl_obj,type);
    return CameraLayer::status::ACK;
  }

  void CAM_CallBack(CameraLayer &cl_obj, int type)
  {
    if (type != CameraLayer::EV_IMG)
      return;

    CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
    LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
    img.ReSize(finfo.width,finfo.height,3);

    CameraLayer::status st = cl_obj.ExtractFrame(img.CVector[0],3,finfo.width*finfo.height);

    BPG_protocol_data_acvImage_Send_info imgInfo=ImageDownSampling_Info(img_buff,img,4,NULL,1,0,0,-1,-1);

    bpg_pi.fromUpperLayer_DATA("IM",channel_id,&imgInfo);
    bpg_pi.fromUpperLayer_SS(channel_id,true,NULL,NULL);    
  }

  ~InspectionTarget()
  {
    if(InspConf)
      cJSON_Delete(InspConf);
    InspConf=NULL;

    
    if(camera)
      delete( camera );
    camera=NULL;
  }

};


class InspectionTargetManager
{
  vector<InspectionTarget*> inspTar;


  inline static CameraLayerManager clm;
  public:
  static InspectionTarget* createInspTargetWithCamera(int idx,std::string misc_str)
  {
    InspectionTarget* insptar=new InspectionTarget();
    CameraLayer *cam= clm.connectCamera(idx,misc_str,InspectionTarget::sCAM_CallBack,insptar);
    insptar->assignCamera(cam);
    return insptar;
  }
  public:
  static string cameraDiscovery()
  {
    clm.discover();
    return clm.genJsonStringList();
  }



  
  InspectionTarget* AddInspTargetWithCamera(int idx,std::string misc_str)
  {
    InspectionTarget* insptar=createInspTargetWithCamera(idx, misc_str);
    inspTar.push_back(insptar);
    return insptar;
  }



  
  bool DelInspTargetWithCamera(int idx)
  {
    if(idx<0||idx>=inspTar.size())return false;

    InspectionTarget *ispt=inspTar[idx];
    delete ispt;
    inspTar[idx]=NULL;
    inspTar.erase(inspTar.begin()+idx);
    return true;
  }


  const vector<InspectionTarget*>& getInspTars()
  {
    return inspTar;
  }


  ~InspectionTargetManager()
  {
    for(int i=0;i<inspTar.size();i++)
    {
      delete inspTar[i];
      inspTar[i]=NULL;
    }
    inspTar.resize(0);
  }
};



InspectionTargetManager inspTarMan;



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
    char err_str[1000] = "\0";
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

        string discoverInfo_str = InspectionTargetManager::cameraDiscovery();
            
        cJSON *discoverInfo = cJSON_Parse(discoverInfo_str.c_str());
        fromUpperLayer_DATA("CM",dat->pgID,discoverInfo);
        cJSON_Delete(discoverInfo);
      }
      else if(strcmp(type_str, "connect") ==0)
      {do{
        
        double *cam_idx = JFetch_NUMBER(json, "cam_idx");
        
        session_ACK = false;
        
        LOGI(">>>");
        if(cam_idx==NULL)break;

        LOGI(">>>");

        string miscStr="";
        {
          char* misc=JFetch_STRING(json, "misc");
          if(misc)
          {
            string str(misc);
            miscStr=misc;
          }
        }
        
        
        
        {
          const vector<InspectionTarget *> &insptars=inspTarMan.getInspTars();
          InspectionTarget *insptar=NULL;
          for(int i=0;i<insptars.size();i++)
          {
            if(insptars[i]->channel_id==dat->pgID)
            {
              insptar=insptars[i];
            }
          }

          if(insptar==NULL)
          {
            InspectionTarget *insptar = inspTarMan.AddInspTargetWithCamera((int)*cam_idx,miscStr);
            
            if(insptar)
            {
              insptar->channel_id=dat->pgID;
            }
          }

          if(insptar)
          {
            cJSON* info = insptar->getInfo_cJSON();

            fromUpperLayer_DATA("CM",dat->pgID,info);
            cJSON_Delete(info);
          }
          // LOGI(">>>insptar:%p",insptar);
          session_ACK = (insptar!=NULL);


        }



          
        
      }while(0);}
      
      else if(strcmp(type_str, "disconnect") ==0)
      {do{
      
        double *_channel_id = JFetch_NUMBER(json, "channel_id");
        if(_channel_id==NULL)
        {
          break;
        }
        int channel_id=(int)*_channel_id;

        
        LOGI(">>>>>channel_id:%d",channel_id);
        const vector<InspectionTarget *> &insptars=inspTarMan.getInspTars();
        InspectionTarget *targetToSet=NULL;
        for(int i=0;i<insptars.size();i++)
        {
          
          LOGI(">>>>>insptars[%d].chid:%d",i,insptars[i]->channel_id);
          if(insptars[i]->channel_id==channel_id)
          {
            session_ACK=true;
            inspTarMan.DelInspTargetWithCamera(i);
            break;
          }
        }
        
          
        
      }while(0);}
      else if(strcmp(type_str, "get_insp_targets") ==0)
      {
        
        cJSON *retArr = cJSON_CreateArray();
        const vector<InspectionTarget *> &insptars=inspTarMan.getInspTars();
        for(int i=0;i<insptars.size();i++)
        {
          cJSON_AddItemToArray(retArr, insptars[i]->getInfo_cJSON());
        }
        session_ACK = true;
        
        fromUpperLayer_DATA("CM",dat->pgID,retArr);
        cJSON_Delete(retArr);
      }
      else if(strcmp(type_str, "target_exchange") ==0)
      {do{
        
        double *_channel_id = JFetch_NUMBER(json, "channel_id");
        if(_channel_id==NULL)
        {
          break;
        }
        int channel_id=(int)*_channel_id;

        
        LOGI(">>>>>channel_id:%d",channel_id);
        const vector<InspectionTarget *> &insptars=inspTarMan.getInspTars();
        InspectionTarget *targetToSet=NULL;
        for(int i=0;i<insptars.size();i++)
        {
          
          LOGI(">>>>>insptars[%d].chid:%d",i,insptars[i]->channel_id);
          if(insptars[i]->channel_id==channel_id)
          {
            targetToSet=insptars[i];
            break;
          }
        }
        
        if(targetToSet!=NULL)
        {
          InspectionTarget_EXCHANGE input={0};
          input.info=json;
          
          LOGI(">>>>>json:%p",json);
          InspectionTarget_EXCHANGE *retExch = targetToSet->setInfo(&input);
          if(retExch!=NULL)
          {
            fromUpperLayer_DATA("CM",dat->pgID,retExch);
            session_ACK=retExch->isOK;
            targetToSet->returnExchange(retExch);
          }
        }
        else
        {
          session_ACK=false;
        }

      }while(0);}

    }
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
