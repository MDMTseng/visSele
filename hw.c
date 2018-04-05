#include <stdio.h>
#include <unistd.h>
#include <time.h>
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

int testX()
{

  MatchingEngine me;
  char *string = ReadFile("data/target.json");
  me.AddMatchingFeature(string);
  free(string);

  vector<acv_XY> tar_signature(360);
  //Just to get target signature
  {
    acvImage *target = new acvImage();

    int ret=acvLoadBitmapFile(target, "data/target.bmp");

    std::vector<acv_LabeledData> ldData;
    acvThreshold(target, 128, 0);
    acvComponentLabeling(target);
    acvLabeledRegionInfo(target, &ldData);
    acvContourCircleSignature(target, ldData[1], 1, tar_signature);
    delete(target);
  }

  acvImage *test1 = new acvImage();
  int ret=acvLoadBitmapFile(test1, "data/test1.bmp");
  acvImage *test1_buff = new acvImage();

  test1_buff->ReSize(test1->GetWidth(), test1->GetHeight());

/*
  printf("\"magnitude\":[");
  for(int i=0;i<tar_signature.size();i++)
  {
    printf("%f,",tar_signature[i].X);
  }printf("],\n");


  printf("\"angle\":[");
  for(int i=0;i<tar_signature.size();i++)
  {
    printf("%f,",tar_signature[i].Y);
  }printf("]\n");*/

  clock_t t = clock();
  for(int i=0;i<1;i++)
    me.FeatureMatching(test1,test1_buff,NULL);

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  //ContourFeatureDetect(test1,test1_buff,tar_signature);
  acvSaveBitmapFile("data/target_buff.bmp",test1_buff);

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


char* ReadFile(char *filename)
{
   char *buffer = NULL;
   int string_size, read_size;
   FILE *handler = fopen(filename, "r");

   if (handler)
   {
       // Seek the last byte of the file
       fseek(handler, 0, SEEK_END);
       // Offset from the first to the last byte, or in other words, filesize
       string_size = ftell(handler);
       // go back to the start of the file
       rewind(handler);

       // Allocate a string that can hold it all
       buffer = (char*) malloc(sizeof(char) * (string_size + 1) );

       // Read it all in one operation
       read_size = fread(buffer, sizeof(char), string_size, handler);

       // fread doesn't set it so put a \0 in the last position
       // and buffer is now officially a string
       buffer[read_size] = '\0';

       /*if (string_size != read_size)
       {
           // Something went wrong, throw away the memory and set
           // the buffer to NULL
           free(buffer);
           buffer = NULL;
       }
       */
       // Always remember to close the file.
       fclose(handler);
    }

    return buffer;
}

#include <vector>
int main(int argc, char** argv)
{
    int seed = time(NULL);
    srand(seed);
    int ret = 0, repeatNum=1;

    if(argc>=2)
    {
      repeatNum=simpP(argv[1]);
    }
    logi("execute %d times\r\n", repeatNum);

    //ret = testSignature(repeatNum);
    ret = testX();
    {//Test intersect function
      acv_XY p1={.X=-1,.Y=1.1};
      acv_XY p2={.X= 0,.Y=1};
      acv_XY p3={.X= 1,.Y=0};
      acv_XY p4={.X= 1,.Y=-1};
      acv_XY ip = acvIntersectPoint(p1, p2, p3, p4);

      logi("acvIntersectPoint:X:%f Y:%f\r\n", ip.X,ip.Y);
    }

    {
      acv_XY p1={.X=1,.Y=0};
      acv_XY p2={.X= 20,.Y=0};
      acv_XY p3={.X=0,.Y=1};
      acv_XY cc = acvCircumcenter(p1,p2,p3);
      logi("acvCircumcenter:X:%f Y:%f\r\n", cc.X,cc.Y);
    }
    return ret;
}
