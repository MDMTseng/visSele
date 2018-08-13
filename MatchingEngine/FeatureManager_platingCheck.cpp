#include "FeatureManager_platingCheck.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>

/*
  FeatureManager_platingCheck Section
*/
FeatureManager_platingCheck::FeatureManager_platingCheck(const char *json_str): FeatureManager(json_str)
{

  //LOGI(">>>>%s>>>>",json_str);
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_platingCheck failed... " );
}

bool FeatureManager_platingCheck::check(cJSON *root)
{
  char *str;
  LOGI("FeatureManager_platingCheck>>>");
  if(!(getDataFromJsonObj(root,"type",(void**)&str)&cJSON_String))
  {
    return false;
  }
  if (strcmp("plating_check",str) == 0)
  {
    return true;
  }
  return false;
}

static void acvScalingSobelResult_max(acvImage *src)
{
    int i, j;
    int maxS=0;


    for (i = 0; i < src->GetHeight(); i++)
    {
        int8_t *pix = (int8_t*)&(src->CVector[i][0]);
        for (j = 0; j < src->GetWidth(); j++, pix+=3)
        {
          int tmp =pix[0];
          if(tmp<0)tmp=-tmp;
          if(maxS<tmp)maxS=tmp;

          tmp =pix[1];
          if(tmp<0)tmp=-tmp;
          if(maxS<tmp)maxS=tmp;
        }
    }
    if(maxS==0)return;
    for (i = 0; i < src->GetHeight(); i++)
    {
        int8_t *pix = (int8_t*)&(src->CVector[i][0]);
        for (j = 0; j < src->GetWidth(); j++, pix+=3)
        {

          pix[0] = (int)round((int)pix[0] * 127 / maxS);
          pix[1] = (int)round((int)pix[1] * 127 / maxS);
        }
    }
}

static void acvScalingSobelResult_normalize(acvImage *src)
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

static int sobelSpread(acvImage *sobel,acvImage *buff,int radius)
{
  acvImageAdd(sobel, 128);
  acvBoxFilter(buff, sobel, radius);
  acvImageAdd(sobel, -128);

  sobel->ChannelOffset(1);
  acvImageAdd(sobel, 128);
  acvBoxFilter(buff, sobel, radius);
  acvImageAdd(sobel, -128);
  //acvInnerFramePixCopy(sobel, 1);
  sobel->ChannelOffset(-1);

  acvInnerFramePixCopy(sobel, 1);
  acvScalingSobelResult_max(sobel);

  return 0;
}

int FeatureManager_platingCheck::creat_stdMapDat(FeatureManager_platingCheck::stdMapData *dat,char* f_path)
{
  acvImage tmp_img;
  int ret=acvLoadBitmapFile(&tmp_img, f_path);
  if(ret<0)
  {
    LOGE("Load image failed.... f_path:%s",f_path);
    return -1;
  }

  dat->rgb = new acvImage();
  dat->sobel = new acvImage();
  dat->rgb->ReSize(tmp_img.GetWidth(), tmp_img.GetHeight());
  dat->sobel->ReSize(tmp_img.GetWidth(), tmp_img.GetHeight());

  acvCloneImage( &tmp_img,dat->rgb, -1);
  tmp_img.RGBToGray();
  acvSobelFilter(dat->sobel,&tmp_img);
  sobelSpread(dat->sobel,&tmp_img,3);
  sobelSpread(dat->sobel,&tmp_img,3);
  sobelSpread(dat->sobel,&tmp_img,3);


  dat->sobel->ChannelOffset(2);
  acvBoxFilter(&tmp_img, dat->sobel, 3);
  acvBoxFilter(&tmp_img, dat->sobel, 3);
  dat->sobel->ChannelOffset(-2);
  return 0;
}


int FeatureManager_platingCheck::parse_jobj()
{

  cJSON *featureList = cJSON_GetObjectItem(root,"features");

  if(featureList==NULL)
  {
    LOGE("features array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureList))
  {
    LOGE("features is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureList) ; i++)
  {
    cJSON * feature = cJSON_GetArrayItem(featureList, i);

    char *f_type = (char*)JFetch(feature,"type",cJSON_String);
    char *f_path = (char*)JFetch(feature,"filePath",cJSON_String);
    double *f_angle = (double*)JFetch(feature,"angle",cJSON_Number);
    if(f_type==NULL || f_path==NULL || f_angle==NULL)
    {
      LOGE("features %p type:%p angle:%p",f_type,f_path,f_angle);
      return -1;
    }
    LOGV("features %d type:%s angle:%f",i,f_type,*f_angle);
    stdMapData dat;
    if(creat_stdMapDat(&dat,f_path)!=0)
    {
      return -1;
    }

    stdMap.push_back(dat);



  }
  return 0;
}


int FeatureManager_platingCheck::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }

  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }
  return 0;
}

int FeatureManager_platingCheck::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg,cJSON *report)
{
  acvImage *tarImg = stdMap[0].rgb;
  int XFactor=3;
  moEng.RESET(10*XFactor,tarImg->GetWidth(),tarImg->GetHeight());

  acvCloneImage(img,buff, -1);
  buff->RGBToGray();


  acvBoxFilter(img,buff,  3);
  acvBoxFilter(img,buff,  3);

  int drawMargin=80;
  int iterCount=10;
  int skipF=6;
  float lRate_start=1*skipF*skipF;
  float lRate_end=0.7*skipF*skipF;
  for(int iter=0;iter<iterCount;iter++)
  {
    if(iter%1==0)moEng.regularization(0.4);
    int offsetX=iter%skipF;
    int offsetY=(iter/skipF)%skipF;
    float traningRate=lRate_start+(lRate_end-lRate_start)*iter/iterCount;
    for(int i=drawMargin+offsetY;i<buff->GetHeight()-drawMargin;i+=skipF)
    {
      for(int j=drawMargin+offsetX;j<buff->GetWidth()-drawMargin;j+=skipF)
      {
        acv_XY from={.X=j,.Y=i};
        acv_XY mapPt={0};


        moEng.Mapping(from,&mapPt);
        int mapX=round(mapPt.X);
        int mapY=round(mapPt.Y);


        uint8_t *stdMap_sobel_pix = &(stdMap[0].sobel->CVector[mapY][mapX*3]);

        uint8_t *stdMap_rgb_pix = &(stdMap[0].rgb->CVector[mapY][mapX*3]);
        uint8_t *cur_pix = &(buff->CVector[i][j*3]);

        {

          float diff = (int)cur_pix[0]-acvUnsignedMap1Sampling(stdMap[0].sobel, mapPt, 2);
          int8_t sobelX= stdMap_sobel_pix[1];
          int8_t sobelY= stdMap_sobel_pix[0];

          float diffX = -traningRate*diff*sobelX/256/128;
          float diffY = -traningRate*diff*sobelY/256/128;

          {
            acv_XY adjposition={.X=j,.Y=i};
            acv_XY adjVec={.X=diffX,.Y=diffY};
            moEng.Mapping_adjust(adjposition,adjVec);
          }

        }

      }
    }

    moEng.optimization(0.7);
  }

  if(1)for(int i=drawMargin;i<buff->GetHeight()-drawMargin;i++)
  {
    for(int j=drawMargin;j<buff->GetWidth()-drawMargin;j++)
    {
      acv_XY from={.X=j,.Y=i};
      acv_XY mapPt={0};


      moEng.Mapping(from,&mapPt);
      int mapX=round(mapPt.X);
      int mapY=round(mapPt.Y);


      uint8_t *stdMap_rgb_pix = &(stdMap[0].rgb->CVector[mapY][mapX*3]);
      uint8_t *stdMap_sobel_pix = &(stdMap[0].sobel->CVector[mapY][mapX*3]);
      uint8_t *cur_pix = &(buff->CVector[i][j*3]);
      uint8_t *input_pix = &(img->CVector[i][j*3]);

      float diff =input_pix[2] - acvUnsignedMap1Sampling(stdMap[0].rgb, mapPt, 2);
      //acvUnsignedMap1Sampling(stdMap[0].rgb, mapPt, 1)-input_pix[1];
      diff=diff*1+128;
      if(diff<0)diff=0;
      if(diff>255)diff=255;
      cur_pix[0]=round(diff);
      cur_pix[1]=cur_pix[0];
      cur_pix[2]=cur_pix[0];

      //int t=(int)acvUnsignedMap1Sampling(tarImg, mappedXY[m], 0);
      /*cur_pix[0]=(uint8_t)acvUnsignedMap1Sampling(stdMap[0].rgb, mapPt, 0);
      cur_pix[1]=(uint8_t)acvUnsignedMap1Sampling(stdMap[0].rgb, mapPt, 1);
      cur_pix[2]=(uint8_t)acvUnsignedMap1Sampling(stdMap[0].rgb, mapPt, 2);*/
    }
  }
  if(1)for(int i=0;i<moEng.morphNodes.size();i++)
  {
    acv_XY ctrlPt= moEng.morphNodes[i];

    acvDrawCrossX(buff,
      ctrlPt.X,ctrlPt.Y,
      2,255,0,0,1);
  }
  return 0;
}
