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

acvImage *test1_buff;
DatCH_BMP *imgSrc_X;
DatCH_BPG1_0 *BPG_protocol;
DatCH_WebSocket *websocket=NULL;
MatchingEngine matchingEng;
char* ReadFile(char *filename);


typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst,size_t dstLen,uint8_t *src,size_t srcLen);

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


int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *jsonStr);

int ImgInspection(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *defFilename)
{
  char *string = ReadText(defFilename);
  printf("%s\n%s\n",string,defFilename);
  int ret = ImgInspection_JSONStr(me ,test1,buff, repeatTime,string);
  free(string);
  return ret;
}

int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *jsonStr)
{

  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);


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
                  ImgInspection(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,"data/target.json");
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
          return DatCH_WS_callback(from, data, callback_param);
        break;

        default:

          LOGI("type:%d, UNKNOWN type",data.type);
      }
      return 0;
  }
};
DatCH_CallBack_T callbk_obj;


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

  BPG_data GenStrBPGData(char *TL, char* jsonStr)
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



              bpg_dat=GenStrBPGData("IM", NULL);
              bpg_dat.dat_img=imgSrc_X->GetAcvImage();
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);


            }while(false);


            sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
            bpg_dat=GenStrBPGData("SS", tmp);
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);

          }
          else if(checkTL("II",dat))
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



              bpg_dat=GenStrBPGData("IM", NULL);
              bpg_dat.dat_img=imgSrc_X->GetAcvImage();
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);



              sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
              bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);
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



              imgSrc_X->SetFileName("data/test1.bmp");


              try {
                  ImgInspection(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,"data/featureDetect.json");
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
              bpg_dat.dat_img=imgSrc_X->GetAcvImage();
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


int mainLoop()
{
printf(">>>>>\n" );
  websocket =new DatCH_WebSocket(4090);
  printf(">>>>>\n" );
  acvImage *test1 = new acvImage();

  websocket->SetEventCallBack(&callbk_obj,websocket);
  while(1)
  {
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

int simpleTest()
{
  test1_buff = new acvImage();
  test1_buff->ReSize(100,100);
  imgSrc_X = new DatCH_BMP(new acvImage());
  imgSrc_X->SetFileName("data/test1.bmp");
  ImgInspection(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,"data/test.ic.json");

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
