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

void printImgAscii(acvImage *img, int printwidth)
{
    int step = img->GetWidth() / printwidth;
    if (step < 1)
        step = 1;
    char grayAscii[] = "@%#x+=:-.. ";
    for (int i = img->GetROIOffsetY(); i < img->GetHeight(); i += step)
    {
        unsigned char *ImLine = img->CVector[i];
        for (int j = img->GetROIOffsetX(); j < img->GetWidth(); j += step)
        {
            int tmp = (255 - ImLine[3 * j]) * (sizeof(grayAscii) - 2) / 255;
            char c = grayAscii[tmp];
            //printf("%02x ",ImLine[3*j+2]);
            putchar(c);
        }
        putchar('\n');
    }
}

void acvScalingSobelResult_n(acvImage *src)
{
    int i, j;
    int8_t *L;

    for (i = 0; i < src->GetHeight(); i++)
    {
        L = (int8_t*)&(src->CVector[i][0]);
        for (j = 0; j < src->GetWidth(); j++, L += 3)
        {
            int8_t Lp0, Lp1;
            Lp0 = L[0];
            Lp1 = L[1];
            if (Lp0 != 0 || Lp1 != 0)
            {
                float length = hypot(Lp0,Lp1);
                if(length!=0)
                {
                  L[0] = (int)round((int)Lp0 * 127 / length);
                  L[1] = (int)round((int)Lp1 * 127 / length);
                }
            }
        }
    }
}

void preprocess(acvImage *img,
                acvImage *img_thin_blur,
                acvImage *buff)
{
    //acvIIROrder1Filter(buff, img, 1);
        /*acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);
        acvBoxFilter(buff, img, 2);*/


    acvBoxFilter(buff, img, 4);
    //acvContrast(img,img,0,1,0);
    acvCloneImage(img, img_thin_blur, 0);


/*
    acvIIROrder1Filter(buff, img, 1);
    acvContrast(img,img,0,1,0);
    acvCloneImage(img, img_thin_blur, 1);*/
    //acvSaveBitmapFile("data/preprocess_1st.bmp", img_thin_blur);

    acvThreshold(img, 250, 0);
    //acvSaveBitmapFile("data/preprocess_2st.bmp", img);
}
int Target_prep_dist(acvImage *target, acvImage *target_DistGradient, vector<acv_XY> &signature, acv_LabeledData &signInfo)
{
    int ret=0;
    ret=acvLoadBitmapFile(target, "data/target.bmp");
    if(ret!=0)
    {
      printf("%s:Cannot find data/target.bmp....\n",__func__);
      return ret;
    }
    acvImage *sign = new acvImage();
    acvImage *tmp = new acvImage();
    target_DistGradient->ReSize(target->GetWidth(), target->GetHeight());
    tmp->ReSize(target->GetWidth(), target->GetHeight());
    sign->ReSize(target->GetWidth(), target->GetHeight());

    acvCloneImage(target, tmp, -1);

    int mul = 1;

    acvBoxFilter(sign,tmp, 1);
    acvBoxFilter(sign,tmp, 1);
    acvBoxFilter(sign,tmp, 1);
    acvBoxFilter(sign,tmp, 1);
    acvBoxFilter(sign,tmp, 1);
    acvThreshold(tmp,30);
    acvDistanceTransform_Chamfer(tmp, 5 * mul, 7 * mul);
    //acvDistanceTransform_ChamferX(ss);
    acvInnerFramePixCopy(tmp, 1);

    acvDistanceTransform_Sobel(target_DistGradient, tmp);
    acvInnerFramePixCopy(target_DistGradient, 2);
    acvInnerFramePixCopy(target_DistGradient, 1);
    acvScalingSobelResult_n(target_DistGradient);

    acvImageAdd(target_DistGradient, 128);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    target_DistGradient->ChannelOffset(1);
    acvImageAdd(target_DistGradient, 128);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    acvBoxFilter(tmp, target_DistGradient, 1);
    //acvBoxFilter(ss,distGradient,15);
    target_DistGradient->ChannelOffset(-1);
    //acvSaveBitmapFile("data/target_DistGradient.bmp",tmp->ImageData,tmp->GetWidth(),tmp->GetHeight());

    acvImageAdd(target_DistGradient, -128);
    target_DistGradient->ChannelOffset(1);
    acvImageAdd(target_DistGradient, -128);
    target_DistGradient->ChannelOffset(-1);
    //acvScalingSobelResult_n(target_DistGradient);

    acvCloneImage(target, sign, -1);
    //Generate signature
    preprocess(sign, target, tmp);
    ret=acvLoadBitmapFile(tmp, "data/target_area.bmp");
    if(ret!=0)
    {
      printf("%s:Cannot find data/target_area.bmp, Use global matching instead!!\n",__func__);
      acvClear(target,1,255);
    }
    else
    {
      acvCloneImage_single(tmp,1,target,1);
    }
    //return -1;
    acvComponentLabeling(sign);

    std::vector<acv_LabeledData> ldData;
    acvLabeledRegionInfo(sign, &ldData);
    for (int i = 1; i < ldData.size(); i++)
    {
        //printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
        acvContourCircleSignature(sign, ldData[i], i, signature);
        signInfo = ldData[i];
    }

    delete (sign);
    delete (tmp);
    return 0;
}


void drawSignatureInfo(acvImage *img,
                       const acv_LabeledData &ldData, const vector<acv_XY> &signature,
                       const acv_LabeledData &tar_ldData, const vector<acv_XY> &tar_signature, float angleDiff)
{

    //printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
    //acvDrawBlock(ss,ldData[i].LTBound.X-1,ldData[i].LTBound.Y-1,ldData[i].RBBound.X+1,ldData[i].RBBound.Y+1);
    acv_XY preXY;
    for (int j = 0; j < signature.size(); j++)
    {
        acv_XY nowXY;
        nowXY.Y = tar_signature[j].X * sin(tar_signature[j].Y + angleDiff) + ldData.Center.Y;
        nowXY.X = tar_signature[j].X * cos(tar_signature[j].Y + angleDiff) + ldData.Center.X;
        if (j == 0)
            preXY = nowXY;
        acvDrawLine(img, round(preXY.X), round(preXY.Y),
                    round(nowXY.X), round(nowXY.Y), 2, 1, 0, 5);
        preXY = nowXY;

        /*
        int R=image->GetHeight()-signature[j].X;
        acvDrawLine(image,(j-1)*image->GetWidth()/signature.size(),preR,
          j*image->GetWidth()/signature.size(),R,i+3,0,0,1);


        int tar_R=image->GetHeight()-tar_signature[j].X;
        acvDrawLine(image,(j-1)*image->GetWidth()/tar_signature.size(),pretar_R,
          j*image->GetWidth()/tar_signature.size(),tar_R,i+5,0,0,1);
        preR=R;
        pretar_R=tar_R;*/
    }
}

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

int SignatureGenerator()
{
  //Just to get target signature
  if(0){
    vector<acv_XY> tar_signature(360);
    acvImage *target = new acvImage();
    int ret=acvLoadBitmapFile(target, "data/target.bmp");
    acvThreshold(target, 128, 0);
    std::vector<acv_LabeledData> ldData;
    acvComponentLabeling(target);
    acvLabeledRegionInfo(target, &ldData);
    acvContourCircleSignature(target, ldData[1], 1, tar_signature);
    delete(target);
  }
  return 0;

  // clock_t t = clock();
  // acvLoadBitmapFile(target, "data/target.bmp");
  // acvThreshold(target, 128, 0);
  // zlibDeflate_testX(target,test1_buff,RGB2BW_collapse,BW2RGB_uncollapse);

  // LOGI("compress:%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);

  // acvSaveBitmapFile("data/zlib_test.bmp",target);

}

int ImgInspection(MatchingEngine &me ,acvImage *test1,acvImage *buff,int repeatTime,char *defFilename)
{
  me.ResetFeature();
  char *string = ReadText(defFilename);
  me.AddMatchingFeature(string);
  free(string);
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


int DatCH_WS_callback(DatCH_Interface *ch_interface, DatCH_Data data, void* callback_param)
{
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
          if(ws->default_peer == NULL){
            ws->default_peer = ws_data.peer;
          }
          printf("OPENING peer %s:%d\n",
             inet_ntoa(ws_data.peer->getAddr().sin_addr),
             ntohs(ws_data.peer->getAddr().sin_port));

      break;

      case websock_data::eventType::HAND_SHAKING_FINISHED:

          LOGI("HAND_SHAKING: host:%s orig:%s key:%s res:%s\n",
            ws_data.data.hs_frame.host,
            ws_data.data.hs_frame.origin,
            ws_data.data.hs_frame.key,
            ws_data.data.hs_frame.resource);

          if(1)
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
      break;
      case websock_data::eventType::DATA_FRAME:
          printf("DATA_FRAME >> frameType:%d frameL:%d data_ptr=%p\n",
              ws_data.data.data_frame.type,
              ws_data.data.data_frame.rawL,
              ws_data.data.data_frame.raw
              );

          if(false&&ws_data.data.data_frame.type == WS_DFT_TEXT_FRAME)
          {

            imgSrc_X->SetFileName("data/test1.bmp");

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

class DatCH_CallBack_T : public DatCH_CallBack
{
public:
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
  {

      LOGI("DatCH_CallBack_T:%s_______type:%d________", __func__,data.type);
      switch(data.type)
      {
        case DatCH_Data::DataType_error:
        {
          LOGE("%s: error code:%d..........", __func__,data.data.error.code);
        }
        break;
        case DatCH_Data::DataType_BMP_Read:
        {

          acvImage *test1 = data.data.BMP_Read.img;

          ImgInspection(matchingEng,test1,test1_buff,1,"data/target.json");
        }
        break;

        case DatCH_Data::DataType_websock_data:
          //LOGI("%s:type:DatCH_Data::DataType_websock_data", __func__);
          return DatCH_WS_callback(from, data, callback_param);
        break;

        default:

          LOGI("%s:type:%d, UNKNOWN type", __func__,data.type);
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
          LOGE("%s: error code:%d..........", __func__,data.data.error.code);
        }
        break;

        case DatCH_Data::DataType_websock_data://App -(prot)>[here] WS
        {
          DatCH_Data ret = websocket->SendData(data);
        }
        break;

        case DatCH_Data::DataType_BPG:// WS -(prot)>[here] App
        {
          BPG_data *dat = data.data.p_BPG_data;

          LOGI("%s:DataType_BPG>>>>%c%c>", __func__,dat->tl[0],dat->tl[1]);
          dat->dat_raw[dat->size]='\0';
          LOGI("%s:DataType_BPG>>>>%s", __func__,dat->dat_raw);

          if(checkTL("HR",dat))
          {
            LOGI("%s:Hello ready.......", __func__);
          }
          else if(checkTL("TG",dat))
          {
            LOGI("%s:Trigger.......", __func__);

            {
              DatCH_Data datCH_BPG=
                BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

              char tmp[100];
              int session_id = rand();
              sprintf(tmp,"{\"session_id\":%d, \"start\":true}",session_id);
              BPG_data bpg_dat=GenStrBPGData("SS", tmp);
              datCH_BPG.data.p_BPG_data=&bpg_dat;
              self->SendData(datCH_BPG);



              imgSrc_X->SetFileName("data/test1.bmp");


              try {
                  ImgInspection(matchingEng,imgSrc_X->GetAcvImage(),test1_buff,1,"data/target.json");
                  const FeatureReport * report = matchingEng.GetReport();

                  if(report!=NULL)
                  {
                    cJSON* jobj = matchingEng.FeatureReport2Json(report);
                    cJSON_AddNumberToObject(jobj, "session_id", session_id);
                    char * jstr  = cJSON_Print(jobj);
                    cJSON_Delete(jobj);

                    //LOGI("__\n %s  \n___",jstr);
                    BPG_data bpg_dat=GenStrBPGData("IR", jstr);
                    datCH_BPG.data.p_BPG_data=&bpg_dat;
                    self->SendData(datCH_BPG);

                    delete jstr;
                  }
                  else
                  {
                    sprintf(tmp,"{\"session_id\":%d}",session_id);
                    BPG_data bpg_dat=GenStrBPGData("IR", tmp);
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
          LOGI("%s:type:%d, UNKNOWN type", __func__,data.type);
      }
      return 0;
  }
};



int testX(int repeatTime)
{
  bool doCallbackStyle=false;

  acvImage *test1 = new acvImage();
  DatCH_BMP imgSrc1(test1);
  if(doCallbackStyle)
    imgSrc1.SetEventCallBack(&callbk_obj,NULL);
  imgSrc1.SetFileName("data/test1.bmp");
  if(!doCallbackStyle)
  {
    DatCH_acvImageInterface *imgSrc_g = &imgSrc1;
    imgSrc_g->GetAcvImage();
    ImgInspection(matchingEng,test1,test1_buff,repeatTime,"data/target.json");
    acvSaveBitmapFile("data/test1_buff.bmp",test1_buff);

    const FeatureReport * report = matchingEng.GetReport();
    if(report!=NULL)
    {
      cJSON* jobj = matchingEng.FeatureReport2Json(report);
      char * jstr  = cJSON_Print(jobj);
      cJSON_Delete(jobj);
      LOGI("...\n%s\n...",jstr);
      delete jstr;
    }

  }
  return 0;
}




int test_featureDetect()
{
  acvImage *test1 = new acvImage();
  DatCH_BMP imgSrc1(test1);
  imgSrc1.SetFileName("data/target.bmp");
  ImgInspection(matchingEng,imgSrc1.GetAcvImage(),test1_buff,1,"data/featureDetect.json");
  delete test1;
  return 0;
}

int simpP(char* strNum)
{
  int Num =0;
  char c;
  while( (c = *strNum) )
  {
    if( c>'9' || c<'0' )
      return 0;
    Num=(Num*10)+(c-'0');
    strNum++;
  }

  return Num;
}


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

#include <vector>
int main(int argc, char** argv)
{

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
  int seed = time(NULL);
  srand(seed);
  int ret = 0, repeatNum=1;

  if(argc>=2)
  {
    repeatNum=simpP(argv[1]);
  }
  //test_featureDetect();
  ret = testX(repeatNum);
  logi("execute %d times\r\n", repeatNum);

  return ret;
}
