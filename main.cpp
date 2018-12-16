#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "FeatureManager.h"
#include "MatchingEngine.h"
#include "common_lib.h"
#include "DatCH_Image.hpp"
#include "DatCH_WebSocket.hpp"
#include "DatCH_BPG.hpp"
#include <stdexcept>
#include "CameraLayer_BMP.hpp"
#include "CameraLayer_GIGE_MindVision.hpp"

std::mutex mainThreadLock;
acvImage *test1_buff;
acvImage dataSend_buff;
DatCH_BMP *imgSrc_X;
DatCH_BPG1_0 *BPG_protocol;
DatCH_WebSocket *websocket=NULL;
MatchingEngine matchingEng;
CameraLayer_GIGE_MindVision *cl_GIGEMV;
bool cameraFeedTrigger=false;
char* ReadFile(char *filename);

int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *jsonStr);

int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *defFilename);

typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst,size_t dstLen,uint8_t *src,size_t srcLen);

void ImageDownSampling(acvImage &dst,acvImage &src,int downScale)
{
  dst.ReSize(src.GetWidth()/downScale,src.GetHeight()/downScale);

  for(int i=0;i<dst.GetHeight();i++)
  {
    int src_i = i*downScale;
    for(int j=0;j<dst.GetWidth();j++)
    {
      int RSum=0,GSum=0,BSum=0;
      int src_j = j*downScale;
      for(int m=0;m<downScale;m++)
      {
        for(int n=0;n<downScale;n++)
        {
          BSum+=src.CVector[src_i+m][(src_j+n)*3];
          GSum+=src.CVector[src_i+m][(src_j+n)*3+1];
          RSum+=src.CVector[src_i+m][(src_j+n)*3+2];
        }
      }
      
      BSum/=(downScale*downScale);
      GSum/=(downScale*downScale);
      RSum/=(downScale*downScale);
      dst.CVector[i][j*3+0]=BSum;
      dst.CVector[i][j*3+1]=GSum;
      dst.CVector[i][j*3+2]=RSum;
    }
  }
}


class DatCH_CallBack_BPG : public DatCH_CallBack
{
  DatCH_BPG1_0 *self;
  bool checkTL(const char *TL,const BPG_data *dat)
  {
    if(TL==NULL)return false;
    return (TL[0] == dat->tl[0] && TL[1] == dat->tl[1]);
  }
  uint16_t TLCode(const char *TL)
  {
    return (((uint16_t)TL[0]<<8) |  TL[1]);
  }
public:
  DatCH_CallBack_BPG(DatCH_BPG1_0 *self)
  {
      this->self = self;
  }

  static BPG_data GenStrBPGData(char *TL, char* jsonStr)
  {
    BPG_data BPG_dat;
    BPG_dat.tl[0]=TL[0];
    BPG_dat.tl[1]=TL[1];
    if(jsonStr ==NULL)
    {
      BPG_dat.size=0;
    }
    else
    {
      BPG_dat.size=strlen(jsonStr);
    }
    BPG_dat.dat_raw =(uint8_t*) jsonStr;

    return BPG_dat;
  }
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
  {

      //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);
      switch(data.type)
      {
        case DatCH_Data::DataType_error:
        {
          LOGE("error code:%d..........",data.data.error.code);
        }
        break;

        //Connection layer of the BPG protocol
        case DatCH_Data::DataType_websock_data://App -(prot)>[here] WS //Final stage of outcoming data
        {
          DatCH_Data ret = websocket->SendData(data);
        }
        break;

        case DatCH_Data::DataType_BPG:// WS -(prot)>[here] App //Final stage of incoming data
        {
          BPG_data *dat = data.data.p_BPG_data;

          LOGI("DataType_BPG>>>>%c%c>",dat->tl[0],dat->tl[1]);
          if(checkTL("HR",dat))
          {
            LOGI("DataType_BPG>>>>%s",dat->dat_raw);

            LOGI("Hello ready.......");
          }
          else if(checkTL("SV",dat))//Data from UI to save file
          {
            LOGI("DataType_BPG>>STR>>%s",dat->dat_raw);
            cJSON *json = cJSON_Parse((char*)dat->dat_raw);
            if (json == NULL)
            {
              LOGE("JSON parse failed");
              break;
            }
            do{

              char* fileName =(char* )JFetch(json,"filename",cJSON_String);
              if (fileName == NULL)
              {
                LOGE("No entry:\"filename\" in it");
                break;
              }
              int strinL = strlen((char*)dat->dat_raw)+1;
              LOGI("DataType_BPG>>BIN>>%s",byteArrString(dat->dat_raw+strinL,dat->size-strinL));

              FILE *write_ptr;

              write_ptr = fopen(fileName,"wb");  // w for write, b for binary
              if(write_ptr==NULL)
              {
                LOGE("File open failed");
                break;
              }
              fwrite(dat->dat_raw+strinL,dat->size-strinL,1,write_ptr); // write 10 bytes from our buffer

              fclose (write_ptr);

            }while(false);
            cJSON_Delete(json);
          }
          else if(checkTL("LD",dat))
          {


            DatCH_Data datCH_BPG=
              BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);
            char tmp[100];
            int session_id = rand();
            sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"DF\",\"IM\"]}",session_id);
            BPG_data bpg_dat=GenStrBPGData("SS", tmp);
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);

            do{
              cJSON *json = cJSON_Parse((char*)dat->dat_raw);
              if (json == NULL)
              {
                LOGE("JSON parse failed");
              
                break;
              }
              char* imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
              if (imgSrcPath == NULL)
              {
                LOGE("No entry:imgSrcPath in it");

                break;
              }
              char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
              if (deffile == NULL)
              {
                LOGE("No entry:\"deffile\" in it");
                break;
              }

            
              imgSrc_X->SetFileName(imgSrcPath);


              try {
                  char *jsonStr = ReadText(deffile);
                  if(jsonStr == NULL)
                  {
                    LOGE("Cannot read defFile from:%s",jsonStr);
                    break;
                  }
                  LOGV("Read deffile:%s",deffile);
                  BPG_data bpg_dat=GenStrBPGData("DF", jsonStr);
                  datCH_BPG.data.p_BPG_data=&bpg_dat;
                  self->SendData(datCH_BPG);

              }
              catch (std::invalid_argument iaex) {
                  LOGE( "Caught an error!");
              }

              //TODO:HACK: 4X4 times scale down for transmission speed, bpg_dat.scale is not used for now
              bpg_dat=GenStrBPGData("IM", NULL);
              bpg_dat.scale = 4;
              
              acvThreshold(imgSrc_X->GetAcvImage(), 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
              ImageDownSampling(dataSend_buff,*imgSrc_X->GetAcvImage(),bpg_dat.scale);
              bpg_dat.dat_img=&dataSend_buff;
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);


            }while(false);


            sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
            bpg_dat=GenStrBPGData("SS", tmp);
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);

          }
          else if(checkTL("II",dat))//[I]mage [I]nspection
          {
            cJSON *json = cJSON_Parse((char*)dat->dat_raw);
            if (json == NULL)
            {
              LOGE("JSON parse failed");
              break;
            }
            do{
              char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
              if (deffile == NULL)
              {
                LOGE("No entry:\"deffile\" in it");
                break;
              }

              char* imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
              if (imgSrcPath == NULL)
              {
                LOGE("No entry:imgSrcPath in it");
                break;
              }
            
              imgSrc_X->SetFileName(imgSrcPath);


              DatCH_Data datCH_BPG=
                BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

              char tmp[100];
              int session_id = rand();
              sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"DF\",\"RP\",\"IM\"]}",session_id);
              BPG_data bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);

              try {
                  char *jsonStr = ReadText(deffile);
                  if(jsonStr == NULL)
                  {
                    LOGE("Cannot read defFile from:%s",jsonStr);
                    break;
                  }
                  LOGV("Read deffile:%s",deffile);
                  BPG_data bpg_dat=GenStrBPGData("DF", jsonStr);
                  datCH_BPG.data.p_BPG_data=&bpg_dat;
                  self->SendData(datCH_BPG);

                  int ret = ImgInspection_JSONStr(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,jsonStr);
                  free(jsonStr);
                  //acvSaveBitmapFile("data/buff.bmp",test1_buff);

                  const FeatureReport * report = matchingEng.GetReport();

                  if(report!=NULL)
                  {
                    cJSON* jobj = matchingEng.FeatureReport2Json(report);
                    cJSON_AddNumberToObject(jobj, "session_id", session_id);
                    char * jstr  = cJSON_Print(jobj);
                    cJSON_Delete(jobj);

                    //LOGI("__\n %s  \n___",jstr);
                    BPG_data bpg_dat=GenStrBPGData("RP", jstr);
                    datCH_BPG.data.p_BPG_data=&bpg_dat;
                    self->SendData(datCH_BPG);

                    delete jstr;
                  }
                  else
                  {
                    sprintf(tmp,"{\"session_id\":%d}",session_id);
                    BPG_data bpg_dat=GenStrBPGData("RP", tmp);
                    datCH_BPG.data.p_BPG_data=&bpg_dat;
                    self->SendData(datCH_BPG);
                  }
              }
              catch (std::invalid_argument iaex) {
                  LOGE( "Caught an error!");
              }


              //TODO:HACK: 4X4 times scale down for transmission speed
              bpg_dat=GenStrBPGData("IM", NULL);
              bpg_dat.scale = 4;
              ImageDownSampling(dataSend_buff,*test1_buff,bpg_dat.scale);
              bpg_dat.dat_img=&dataSend_buff;
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);



              sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
              bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);
            }while(false);


          }
          else if(checkTL("CI",dat))//[C]ontinuous [I]nspection
          {
            cJSON *json = cJSON_Parse((char*)dat->dat_raw);
            if (json == NULL)
            {
              LOGE("JSON parse failed");
              break;
            }
            do{
              char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
              if (deffile == NULL)
              {
                LOGE("No entry:\"deffile\" in it");
                cameraFeedTrigger=false;
                break;
              }

              try {
                
                  DatCH_Data datCH_BPG=
                    BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

                  char *jsonStr = ReadText(deffile);
                  if(jsonStr == NULL)
                  {
                    LOGE("Cannot read defFile from:%s",jsonStr);
                    break;
                  }

                  LOGV("Read deffile:%s",deffile);
                  BPG_data bpg_dat=GenStrBPGData("DF", jsonStr);
                  datCH_BPG.data.p_BPG_data=&bpg_dat;
                  self->SendData(datCH_BPG);

                                  
                  matchingEng.ResetFeature();
                  matchingEng.AddMatchingFeature(jsonStr);


                  free(jsonStr);


                  cl_GIGEMV->TriggerMode(1);
                  cl_GIGEMV->Trigger();
                  cameraFeedTrigger=true;
                  //acvSaveBitmapFile("data/buff.bmp",test1_buff);

              }
              catch (std::invalid_argument iaex) {
                  LOGE( "Caught an error!");
              }

            }while(false);


          }
          else if(checkTL("EX",dat))
          {
            LOGI("Trigger.......");

            {
              DatCH_Data datCH_BPG=
                BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

              char tmp[100];
              int session_id = rand();
              sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"SG\",\"IM\"]}",session_id);
              BPG_data bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);



              char* imgSrcPath=NULL; 
              cJSON *json = cJSON_Parse((char*)dat->dat_raw);
              if (json != NULL)
              {
                imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
                if (imgSrcPath == NULL)
                {
                  LOGE("No entry:imgSrcPath in it");

                }
              }
              
              acvImage *srcImg=NULL;
              if(imgSrcPath!=NULL)
              {
                imgSrc_X->SetFileName(imgSrcPath);
                srcImg = imgSrc_X->GetAcvImage();
              }

              if(srcImg==NULL)
              {
                cl_GIGEMV->TriggerMode(1);
                LOGE("LOCK...");
                mainThreadLock.lock();
                cl_GIGEMV->Trigger();
                LOGE("LOCK BLOCK...");
                mainThreadLock.lock();
                
                LOGE( "unlock");
                mainThreadLock.unlock();
                srcImg = cl_GIGEMV->GetImg();
                acvSaveBitmapFile("data/test1.bmp",srcImg);
              }



              try {
                  ImgInspection_DefRead(matchingEng,srcImg,test1_buff,1,"data/featureDetect.json");
                  const FeatureReport * report = matchingEng.GetReport();

                  if(report!=NULL)
                  {
                    cJSON* jobj = matchingEng.FeatureReport2Json(report);
                    cJSON_AddNumberToObject(jobj, "session_id", session_id);
                    char * jstr  = cJSON_Print(jobj);
                    cJSON_Delete(jobj);

                    //LOGI("__\n %s  \n___",jstr);
                    BPG_data bpg_dat=GenStrBPGData("SG", jstr);//SG report : signature360
                    datCH_BPG.data.p_BPG_data=&bpg_dat;
                    self->SendData(datCH_BPG);

                    delete jstr;
                  }
                  else
                  {
                    sprintf(tmp,"{\"session_id\":%d}",session_id);
                    BPG_data bpg_dat=GenStrBPGData("SG", tmp);
                    datCH_BPG.data.p_BPG_data=&bpg_dat;
                    self->SendData(datCH_BPG);
                  }
              }
              catch (std::invalid_argument iaex) {
                  LOGE( "Caught an error!");
              }



              bpg_dat=GenStrBPGData("IM", NULL);

                          
            
              //TODO:HACK: 4X4 times scale down for transmission speed
              bpg_dat.scale = 4;
              ImageDownSampling(dataSend_buff,*test1_buff,bpg_dat.scale);
              bpg_dat.dat_img=&dataSend_buff;
              //acvCloneImage( bpg_dat.dat_img,bpg_dat.dat_img, 2);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);



              sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
              bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);
            }


          }

        }
        break;
        default:
          LOGI("type:%d, UNKNOWN type",data.type);
      }
      return 0;
  }
};


void zlibDeflate_testX(acvImage *img,acvImage *buff,IMG_COMPRESS_FUNC collapse_func, IMG_COMPRESS_FUNC uncollapse_func)
{


  int tLen=img->GetHeight()*img->GetWidth();
  int imgc_Len=3*img->GetHeight()*img->GetWidth();

  if(collapse_func)
  {
    imgc_Len= collapse_func(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
    img->CVector[0],3*img->GetHeight()*img->GetWidth());
  }


  size_t compresSize = 3*buff->GetHeight()*buff->GetWidth();

    compresSize = zlibDeflate(buff->CVector[0],3*buff->GetHeight()*buff->GetWidth(),
                img->CVector[0], imgc_Len,5);

  printf("Compressed size is: %lu/%lu:%.5f\n",  compresSize,imgc_Len,(float)compresSize/(imgc_Len));

  size_t unCompresSize = zlibInflate(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
                buff->CVector[0], compresSize);
  printf("Uncompressed size is: %lu\n",  unCompresSize);


  imgc_Len = unCompresSize;
  if(uncollapse_func)
  {
    imgc_Len= uncollapse_func(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
    img->CVector[0],unCompresSize);
  }

  printf("imgc_Len size is: %lu\n",  imgc_Len);

}


int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *defFilename)
{
  char *string = ReadText(defFilename);
  printf("%s\n%s\n",string,defFilename);
  int ret = ImgInspection_JSONStr(me ,test1,buff, repeatTime,string);
  free(string);
  return ret;
}


int ImgInspection(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime)
{

  LOGI("================================");
  buff->ReSize(test1->GetWidth(), test1->GetHeight());

  clock_t t = clock();
  for(int i=0;i<repeatTime;i++)
    me.FeatureMatching(test1,buff,NULL);

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  return 0;
  //ContourFeatureDetect(test1,test1_buff,tar_signature);
  //acvSaveBitmapFile("data/target_buff.bmp",test1_buff);

}



int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *jsonStr)
{

  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);
  ImgInspection(me,test1,buff,repeatTime);
  return 0;

}


void CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void* context)
{
  
  if(!cameraFeedTrigger)
  {
    /*LOGE( "lock");
    mainThreadLock.lock();*/
    LOGE( "unlock");
    mainThreadLock.unlock();
    return;
  }
  CameraLayer_GIGE_MindVision &cl_GMV=*((CameraLayer_GIGE_MindVision*)&cl_obj);
  int ret = ImgInspection(matchingEng,cl_GMV.GetImg(),test1_buff,1);

  /*LOGE( "lock");
  mainThreadLock.lock();*/
  do{
    char tmp[100];
    int session_id = rand();
    sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"RP\",\"IM\"]}",session_id);
    BPG_data bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);

    DatCH_Data datCH_BPG=
      BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);

    try {

        const FeatureReport * report = matchingEng.GetReport();

        if(report!=NULL)
        {
          cJSON* jobj = matchingEng.FeatureReport2Json(report);
          cJSON_AddNumberToObject(jobj, "session_id", session_id);
          char * jstr  = cJSON_Print(jobj);
          cJSON_Delete(jobj);

          //LOGI("__\n %s  \n___",jstr);
          BPG_data bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("RP", jstr);
          datCH_BPG.data.p_BPG_data=&bpg_dat;
          BPG_protocol->SendData(datCH_BPG);

          delete jstr;
        }
        else
        {
          sprintf(tmp,"{\"session_id\":%d}",session_id);
          BPG_data bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("RP", tmp);
          datCH_BPG.data.p_BPG_data=&bpg_dat;
          BPG_protocol->SendData(datCH_BPG);
        }
    }
    catch (std::invalid_argument iaex) {
        LOGE( "Caught an error!");
    }


    //TODO:HACK: 4X4 times scale down for transmission speed
    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("IM", NULL);
    bpg_dat.scale = 4;
    //ImageDownSampling(dataSend_buff,*cl_GMV.GetImg(),bpg_dat.scale);
    ImageDownSampling(dataSend_buff,*test1_buff,bpg_dat.scale);
    bpg_dat.dat_img=&dataSend_buff;
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);



    sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);

    if(cameraFeedTrigger)
    {
      Sleep(500);
      cl_GIGEMV->Trigger();
    }
  }while(false);


  /*LOGE( "unlock");
  mainThreadLock.unlock();*/

}


// V DatCH_BPG1_0::SendData(BPG_data data)                 $$ Application layer $$  DatCH_CallBack_BPG.callback({type:DataType_BPG}) 
//                                                     $$ BPG_protocol(DatCH_CallBack_BPG) $$     DatCH_BPG1_0::Process_websock_data^
//DatCH_CallBack_BPG.callback({type:DataType_websock_data})                         
//                websocket->SendData(data);       $$ DatCH_WebSocket(DatCH_CallBack_T) $$  DatCH_CallBack_T::BPG_protocol.SendData({type:DataType_websock_data})^         

class DatCH_CallBack_T : public DatCH_CallBack
{
    
  int DatCH_WS_callback(DatCH_Interface *ch_interface, DatCH_Data data, void* callback_param)
  {
    //first stage of incoming data
    //and first stage of outcoming data if needed
    if(data.type!=DatCH_Data::DataType_websock_data)return -1;
    DatCH_WebSocket *ws=(DatCH_WebSocket*)callback_param;
    websock_data ws_data = *data.data.p_websocket;
    LOGI("SEND>>>>>>..websock_data..\n");
    if( (BPG_protocol->MatchPeer(NULL) || BPG_protocol->MatchPeer(ws_data.peer)))
    {
      LOGI("SEND>>>>>>..MatchPeer..\n");
      BPG_protocol->SendData(data);// WS [here]-(prot)> App
    }


    switch(ws_data.type)
    {
        case websock_data::eventType::OPENING:
            printf("OPENING peer %s:%d  sock:%d\n",
              inet_ntoa(ws_data.peer->getAddr().sin_addr),
              ntohs(ws_data.peer->getAddr().sin_port),ws_data.peer->getSocket());
            if(ws->default_peer == NULL){
              ws->default_peer = ws_data.peer;
            }
            else
            {
            }
        break;

        case websock_data::eventType::HAND_SHAKING_FINISHED:

            LOGI("HAND_SHAKING: host:%s orig:%s key:%s res:%s\n",
              ws_data.data.hs_frame.host,
              ws_data.data.hs_frame.origin,
              ws_data.data.hs_frame.key,
              ws_data.data.hs_frame.resource);

            if(ws->default_peer == ws_data.peer )
            {
              LOGI("SEND>>>>>>..HAND_SHAKING_FINISHED..\n");
              DatCH_Data datCH_BPG=
                BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

              LOGI("SEND>>>>>>..GenMsgType..\n");
              BPG_data BPG_dat;
              datCH_BPG.data.p_BPG_data=&BPG_dat;
              BPG_dat.tl[0]='H';
              BPG_dat.tl[1]='R';
              char tmp[]="{\"AA\":5}";
              BPG_dat.size=sizeof(tmp)-1;
              BPG_dat.dat_raw =(uint8_t*) tmp;
              //App [here]-(prot)> WS
              BPG_protocol->SendData(datCH_BPG);
            }
            else
            {
              ws->disconnect(ws_data.peer->getSocket());
            }
        break;
        case websock_data::eventType::DATA_FRAME:
            printf("DATA_FRAME >> frameType:%d frameL:%d data_ptr=%p\n",
                ws_data.data.data_frame.type,
                ws_data.data.data_frame.rawL,
                ws_data.data.data_frame.raw
                );

            if(false&&ws_data.data.data_frame.type == WS_DFT_TEXT_FRAME)
            {

              //imgSrc_X->SetFileName("data/test1.bmp");

              try {
                  ImgInspection_DefRead(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,"data/target.json");
              }
              catch (std::invalid_argument iaex) {
                  LOGE( "Caught an error!");
              }

              const FeatureReport * report = matchingEng.GetReport();

              if(false && report!=NULL)
              {
                cJSON* jobj = matchingEng.FeatureReport2Json(report);
                char * jstr  = cJSON_Print(jobj);
                cJSON_Delete(jobj);
                LOGI("...\n%s\n...",jstr);
                DatCH_Data ret = ws->SendData(jstr,strlen(jstr));
                delete jstr;
                if(ret.type!=DatCH_Data::DataType_ACK)
                {
                  if(ret.type==DatCH_Data::DataType_error)
                  {
                    LOGI("...\nERROR:%d....\n...",ret.data.error.code);
                  }
                  break;
                }
              }
              printf("Start to send....\n");

            }
            else
            {

            }


        break;
        case websock_data::eventType::CLOSING:

            printf("CLOSING peer %s:%d\n",
              inet_ntoa(ws_data.peer->getAddr().sin_addr), ntohs(ws_data.peer->getAddr().sin_port));
            cameraFeedTrigger=false;
        break;
        default:
          return -1;
    }
    return 0;

  }
public:
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
  {

      LOGI("DatCH_CallBack_T:_______type:%d________",data.type);
      int ret_val=0;
      switch(data.type)
      {
        case DatCH_Data::DataType_error:
        {
          LOGE("error code:%d..........",data.data.error.code);
        }
        break;
        case DatCH_Data::DataType_BMP_Read:
        {

          //acvImage *test1 = data.data.BMP_Read.img;

          //ImgInspection(matchingEng,test1,test1_buff,1,"data/target.json");
        }
        break;

        case DatCH_Data::DataType_websock_data:
          //LOGI("%s:type:DatCH_Data::DataType_websock_data", __func__);
          /*LOGV("lock");
          mainThreadLock.lock();*/
          ret_val =  DatCH_WS_callback(from, data, callback_param);
          /*LOGV("unlock");
          mainThreadLock.unlock();*/
        break;

        default:

          LOGI("type:%d, UNKNOWN type",data.type);
      }
      
      
      return ret_val;
  }
};
DatCH_CallBack_T callbk_obj;



void initGIGE(CameraLayer_GIGE_MindVision *CL_GIGE)
{
  
  tSdkCameraDevInfo sCameraList[10];
  int retListL = sizeof(sCameraList)/sizeof(sCameraList[0]);
  cl_GIGEMV->EnumerateDevice(sCameraList,&retListL);
  
  if(retListL<=0)return;
	for (int i=0; i< retListL;i++)
	{
		printf("CAM:%d======\n", i);
		printf("acDriverVersion:%s\n", sCameraList[i].acDriverVersion);
		printf("acFriendlyName:%s\n", sCameraList[i].acFriendlyName);
		printf("acLinkName:%s\n", sCameraList[i].acLinkName);
		printf("acPortType:%s\n", sCameraList[i].acPortType);
		printf("acProductName:%s\n", sCameraList[i].acProductName);
		printf("acProductSeries:%s\n", sCameraList[i].acProductSeries);
		printf("acSensorType:%s\n", sCameraList[i].acSensorType);
		printf("acSn:%s\n", sCameraList[i].acSn);
		printf("\n\n\n\n");
	}
  
  cl_GIGEMV->InitCamera(&(sCameraList[0]));
}
int mainLoop()
{
  
  cl_GIGEMV=new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV,NULL);
  initGIGE(cl_GIGEMV);
  cl_GIGEMV->TriggerMode(1);
  printf(">>>>>\n" );
  websocket =new DatCH_WebSocket(4090);
  printf(">>>>>\n" );
  acvImage *test1 = new acvImage();

  websocket->SetEventCallBack(&callbk_obj,websocket);
  while(1)
  {
    LOGV(">>>>>");
      websocket->runLoop(NULL);
  }
  delete test1;
  return 0;
}

void sigroutine(int dunno) { /* 信號處理常式，其中dunno將會得到信號的值 */
  switch (dunno) {
    case SIGINT:
      LOGE("Get a signal -- SIGINT \n");
      LOGE("Tear down websocket.... \n");
      delete websocket;
    break;
  }
  return;
}


void CameraLayer_Callback_BMP(CameraLayer &cl_obj, int type, void* context)
{
  CameraLayer_BMP &clBMP=*((CameraLayer_BMP*)&cl_obj);
  LOGV("Called.... %d, filename:%s",type,clBMP.GetCurrentFileName().c_str());
}


int testGIGE()
{
  
  cl_GIGEMV=new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV,NULL);

  initGIGE(cl_GIGEMV);

  cl_GIGEMV->SetAnalogGain(1500);
  cl_GIGEMV->SetExposureTime(5);
  cl_GIGEMV->TriggerMode(1);
  cl_GIGEMV->Trigger();
  Sleep(1000);
  
  LOGV("SAVE:::::, %p   WH:%d,%d",cl_GIGEMV->GetImg()->CVector[0],cl_GIGEMV->GetImg()->GetWidth(),cl_GIGEMV->GetImg()->GetHeight());
  acvSaveBitmapFile("data/MVCam.bmp",cl_GIGEMV->GetImg());

  LOGV("OK:::::");

  return 0;
}


int simpleTest()
{
  //return testGIGE();;
  CameraLayer_BMP cl_BMP(CameraLayer_Callback_BMP,NULL);

  test1_buff = new acvImage();
  test1_buff->ReSize(100,100);
  CameraLayer::status ret = cl_BMP.LoadBMP("data/testInsp.bmp");
  if(ret != CameraLayer::ACK)
  {
    LOGE("LoadBMP failed: ret:%d",ret);
    return -1;
  }
  ImgInspection_DefRead(matchingEng,cl_BMP.GetImg(),test1_buff,1,"data/test.ic.json");

  acvSaveBitmapFile("data/buff.bmp",test1_buff);
  const FeatureReport * report = matchingEng.GetReport();

  if(report!=NULL)
  {
    cJSON* jobj = matchingEng.FeatureReport2Json(report);
    char * jstr  = cJSON_Print(jobj);
    cJSON_Delete(jobj);
    LOGI("...\n%s\n...",jstr);
    
  }
  printf("Start to send....\n");


  return 0;
}

#include <vector>
int main(int argc, char** argv)
{
  
  //return simpleTest();
  #ifdef __WIN32__
  {
      WSADATA wsaData;
      int iResult;
      // Initialize Winsock
      iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
      if (iResult != 0) {
          printf("WSAStartup failed with error: %d\n", iResult);
          return 1;
      }
      
  }
  #endif



  signal(SIGINT, sigroutine);
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  test1_buff = new acvImage();
  test1_buff->ReSize(100,100);
  imgSrc_X = new DatCH_BMP(new acvImage());
  BPG_protocol = new DatCH_BPG1_0(NULL);
  BPG_protocol->SetEventCallBack(new DatCH_CallBack_BPG(BPG_protocol),NULL);
  return mainLoop();
}
