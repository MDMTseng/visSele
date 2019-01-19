#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "DatCH_Image.hpp"
#include "DatCH_WebSocket.hpp"
#include "DatCH_BPG.hpp"


#include <main.h>
#include <playground.h>
#include <stdexcept>

std::mutex mainThreadLock;
DatCH_BPG1_0 *BPG_protocol;
DatCH_WebSocket *websocket=NULL;
MatchingEngine matchingEng;
CameraLayer *gen_camera;

//lens1
//main.cpp  1067 main:v K: 1.00096 -0.00100092 -9.05316e-05 RNormalFactor:1296
//main.cpp  1068 main:v Center: 1295,971


//main.cpp  1075 main:v K: 0.999783 0.00054474 -0.000394607 RNormalFactor:1296
//main.cpp  1076 main:v Center: 1295,971


//lens2
//main.cpp  1061 main:v K: 0.989226 0.0101698 0.000896734 RNormalFactor:1296
//main.cpp  1062 main:v Center: 1295,971



acvRadialDistortionParam param_default={
    calibrationCenter:{1295,971},
    RNormalFactor:1296,
    K0:0.999783,
    K1:0.00054474,
    K2:-0.000394607,
    //r = r_image/RNormalFactor
    //C1 = K1/K0
    //C2 = K2/K0
    //r"=r'/K0
    //Forward: r' = r*(K0+K1*r^2+K2*r^4)
    //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
    //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
    //r/r'=r*K0/r"
};

bool cameraFeedTrigger=false;
char* ReadFile(char *filename);

int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,int repeatTime,char *jsonStr);

int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,int repeatTime,char *defFilename);

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
  
  acvImage tmp_buff;
  acvImage cacheImage;
  acvImage dataSend_buff;

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
  CameraLayer *camera=NULL;
  DatCH_CallBack_BPG(DatCH_BPG1_0 *self)
  {
      this->self = self;
  }

  static BPG_data GenStrBPGData(char *TL, char* jsonStr)
  {
    BPG_data BPG_dat={0};
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

              if(dat->size-strinL == 0 )
              {//No raw data, check "type" 

                char* type =(char* )JFetch(json,"type",cJSON_String);
                if (strcmp(type,"__CACHE_IMG__") == 0 )
                {
                  acvSaveBitmapFile(fileName,&cacheImage);
                }
              }
              else
              {

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
              }


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

              acvImage *srcImg=NULL;
              if(imgSrcPath!=NULL)
              {
                
                int ret_val = acvLoadBitmapFile(&tmp_buff,imgSrcPath);
                if(ret_val==0)
                {
                  srcImg = &tmp_buff;
                }
              }
              if(srcImg==NULL)
              {
                break;
              }

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
              BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};
              //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
              ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale);
              bpg_dat.callbackInfo = (uint8_t*)&iminfo;
              bpg_dat.callback=DatCH_BPG_acvImage_Send;

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
              LOGI("Load Image from %s",imgSrcPath);
              acvImage *srcImg=NULL;
              if(imgSrcPath!=NULL)
              {
                
                int ret_val = acvLoadBitmapFile(&tmp_buff,imgSrcPath);
                if(ret_val==0)
                {
                  srcImg = &tmp_buff;
                }
              }

              if(srcImg==NULL)
              {
                LOGV("Do camera Fetch..");
                camera->TriggerMode(1);
                LOGV("LOCK...");
                mainThreadLock.lock();
                camera->Trigger();
                LOGV("LOCK BLOCK...");
                mainThreadLock.lock();
                
                LOGV( "unlock");
                mainThreadLock.unlock();
                srcImg = camera->GetImg();
              }

              if(srcImg==NULL)
              {
                LOGE("No Image from %s, exit...",imgSrcPath);
                break;
              }

            


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
                    LOGE("Cannot read defFile from:%s",deffile);
                    break;
                  }
                  LOGV("Read deffile:%s",deffile);
                  BPG_data bpg_dat=GenStrBPGData("DF", jsonStr);
                  datCH_BPG.data.p_BPG_data=&bpg_dat;
                  self->SendData(datCH_BPG);

                  int ret = ImgInspection_JSONStr(matchingEng,srcImg,1,jsonStr);
                  free(jsonStr);
                  //acvSaveBitmapFile("data/buff.bmp",&test1_buff);

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
              BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};
              //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
              ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale);
              bpg_dat.callbackInfo = (uint8_t*)&iminfo;
              bpg_dat.callback=DatCH_BPG_acvImage_Send;              
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
                
                camera->TriggerMode(1);
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


                  camera->TriggerMode(0);
                  cameraFeedTrigger=true;
                  camera->Trigger();
                  //acvSaveBitmapFile("data/buff.bmp",&test1_buff);

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
              
              acvImage *srcImg;
              if(imgSrcPath!=NULL)
              {
                int ret_val = acvLoadBitmapFile(&tmp_buff,imgSrcPath);
                if(ret_val==0)
                {
                  srcImg = &tmp_buff;
                }
              }


              if(srcImg==NULL)
              {
                LOGV("Do camera Fetch..");
                camera->TriggerMode(1);
                LOGV("LOCK...");
                mainThreadLock.lock();
                camera->Trigger();
                LOGV("LOCK BLOCK...");
                mainThreadLock.lock();
                
                LOGV( "unlock");
                mainThreadLock.unlock();
                srcImg = camera->GetImg();
                cacheImage.ReSize(srcImg->GetWidth(),srcImg->GetHeight());
                acvCloneImage(srcImg,&cacheImage,-1);
                //acvSaveBitmapFile("data/test1.bmp",srcImg);
              }



              try {
                  ImgInspection_DefRead(matchingEng,srcImg,1,"data/featureDetect.json");
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
              BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};
              //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
              ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale);
              bpg_dat.callbackInfo = (uint8_t*)&iminfo;
              bpg_dat.callback=DatCH_BPG_acvImage_Send;              
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              BPG_protocol->SendData(datCH_BPG);


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


int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,int repeatTime,char *defFilename)
{
  char *string = ReadText(defFilename);
  //printf("%s\n%s\n",string,defFilename);
  int ret = ImgInspection_JSONStr(me ,test1, repeatTime,string);
  free(string);
  return ret;
}


int ImgInspection(MatchingEngine &me ,acvImage *test1,acvRadialDistortionParam param,int repeatTime)
{

  LOGI("================================");
  clock_t t = clock();
  for(int i=0;i<repeatTime;i++)
  {
    me.setRadialDistortionParam(param);
    me.FeatureMatching(test1);
  }

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  return 0;
  //ContourFeatureDetect(test1,&test1_buff,tar_signature);
  //acvSaveBitmapFile("data/target_buff.bmp",&test1_buff);

}



int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,int repeatTime,char *jsonStr)
{

  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);
  ImgInspection(me,test1,param_default,repeatTime);
  return 0;

}


void CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void* context)
{
  static acvImage test1_buff;
  LOGV("cameraFeedTrigger:%d",cameraFeedTrigger); 
  if(!cameraFeedTrigger)
  {
    /*LOGE( "lock");
    mainThreadLock.lock();*/
    LOGE( "unlock");
    mainThreadLock.unlock();
    return;
  }
  CameraLayer &cl_GMV=*((CameraLayer*)&cl_obj);
  int ret = ImgInspection(matchingEng,cl_GMV.GetImg(),param_default,1);

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

    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("IM", NULL);
    BPG_data_acvImage_Send_info iminfo={img:&test1_buff,scale:4};
    //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
    ImageDownSampling(test1_buff,*cl_GMV.GetImg(),iminfo.scale);
    bpg_dat.callbackInfo = (uint8_t*)&iminfo;
    bpg_dat.callback=DatCH_BPG_acvImage_Send;              
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);



    sprintf(tmp,"{\"session_id\":%d, \"start\":false}",session_id);
    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);

    //acvSaveBitmapFile("data/MVCamX.bmp",&test1_buff);
    //exit(0);
    if(cameraFeedTrigger)
    {
      LOGV("cameraFeedTrigger:%d Get Next frame...",cameraFeedTrigger);
      //std::this_thread::sleep_for(std::chrono::milliseconds(500));
      cl_GMV.Trigger();
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

          //ImgInspection(matchingEng,test1,&test1_buff,1,"data/target.json");
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


void initCamera(CameraLayer_GIGE_MindVision *CL_GIGE)
{
  
  tSdkCameraDevInfo sCameraList[10];
  int retListL = sizeof(sCameraList)/sizeof(sCameraList[0]);
  CL_GIGE->EnumerateDevice(sCameraList,&retListL);
  
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
  
  CL_GIGE->InitCamera(&(sCameraList[0]));
}
void initCamera(CameraLayer_BMP_carousel *CL_bmpc)
{
  
}


CameraLayer *getCamera(bool realCamera=false)
{

  CameraLayer *camera;
  if(realCamera)
  {
    CameraLayer_GIGE_MindVision *camera_GIGE;
    camera_GIGE=new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV,NULL);
    LOGV("initCamera");
    initCamera(camera_GIGE);
    camera=camera_GIGE;

  }
  else
  {
    CameraLayer_BMP_carousel *camera_BMP;
    LOGV("CameraLayer_BMP_carousel");
    camera_BMP=new CameraLayer_BMP_carousel(CameraLayer_Callback_GIGEMV,NULL,"data/BMP_carousel_test");
    camera=camera_BMP;
  }
  return camera;
}

int mainLoop(bool realCamera=false)
{
  /**/
  
  printf(">>>>>\n" );
  websocket =new DatCH_WebSocket(4090);
  printf(">>>>>\n" );
  
  BPG_protocol = new DatCH_BPG1_0(NULL);
  DatCH_CallBack_BPG *cb = new DatCH_CallBack_BPG(BPG_protocol);
  CameraLayer *camera = getCamera(realCamera);
  
  LOGV("DatCH_BPG1_0");
  cb->camera = camera;
  BPG_protocol->SetEventCallBack(cb,NULL);

  LOGV("TriggerMode(1)");
  camera->TriggerMode(1);
  acvImage *test1 = new acvImage();

  websocket->SetEventCallBack(&callbk_obj,websocket);

  while(websocket->runLoop(NULL) == 0)
  {
    
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


int simpleTest()
{
  acvImage test1_buff;
  //return testGIGE();;
  CameraLayer_BMP cl_BMP(CameraLayer_Callback_BMP,NULL);

  CameraLayer::status ret = cl_BMP.LoadBMP("data/testInsp.bmp");
  if(ret != CameraLayer::ACK)
  {
    LOGE("LoadBMP failed: ret:%d",ret);
    return -1;
  }
  ImgInspection_DefRead(matchingEng,cl_BMP.GetImg(),1,"data/test.ic.json");

  acvSaveBitmapFile("data/buff.bmp",&test1_buff);
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
  srand(time(NULL));
  /*auto lambda = []() { LOGV("Hello, Lambda"); };
  lambda();*/
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

  if(0){


    acvImage calibImage;
    acvImage test_buff;
    int ret_val = acvLoadBitmapFile(&calibImage,"data/calibration_Img/lens1_2.bmp");
    if(ret_val!=0)return -1;
    ImgInspection_DefRead(matchingEng,&calibImage,1,"data/cameraCalibration.json");

    
    const FeatureReport * report = matchingEng.GetReport();

    if(report!=NULL)
    {
      cJSON* jobj = matchingEng.FeatureReport2Json(report);
      //cJSON_AddNumberToObject(jobj, "session_id", session_id);
      char * jstr  = cJSON_Print(jobj);
      cJSON_Delete(jobj);

      LOGI("__\n %s  \n___",jstr);

      delete jstr;
    }
    return 0;
  }



  signal(SIGINT, sigroutine);
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(false);
}
