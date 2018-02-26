#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "BinaryImageTemplateFitting.hpp"
#include "MLNN.hpp"
#include "experiment.h"

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

int testSignature(int repeatNum)
{
    vector<float> data(repeatNum);
    vector<acv_XY> tar_signature(240);
    acv_LabeledData tar_ldData;
    acvImage *target = new acvImage();
    acvImage *target_DistGradient = new acvImage();
    int ret=0;
    ret=Target_prep_dist(target, target_DistGradient, tar_signature, tar_ldData);
    if(ret!=0)
    {
      printf("%s:Cannot init target....\n",__func__);
      return ret;
    }
    //return 0;

    acvImage *image = new acvImage();
    acvImage *labelImg = new acvImage();
    acvImage *buff = new acvImage();
    std::vector<acv_LabeledData> ldData;
    ret=acvLoadBitmapFile(image, "data/test1.bmp");
    if(ret!=0)
    {
      printf("%s:Cannot find data/test1.bmp....\n",__func__);
      return ret;
    }
    buff->ReSize(image->GetWidth(), image->GetHeight());
    labelImg->ReSize(image->GetWidth(), image->GetHeight());
    vector<acv_XY> signature(tar_signature.size());
    BinaryImageTemplateFitting bitf(tar_ldData, target, image, target_DistGradient);

    std::vector<acv_XY> regionXY_;


      printf("%s:Preprocess done...\n",__func__);
    clock_t t = clock();
    for(int iterX=0;iterX<repeatNum;iterX++)
    {
      ret=acvLoadBitmapFile(image, "data/test1.bmp");

      //image->RGBToGray();
      acvCloneImage(image, labelImg, -1);
      preprocess(labelImg, image, buff);
      /*t = clock() - t;
      printf("%fms .preprocess.\n", ((double)t) / CLOCKS_PER_SEC * 1000);
      t = clock();*/

      //Create a trap to capture/link boundery object
      acvDrawBlock(labelImg, 1, 1, labelImg->GetWidth() - 2, labelImg->GetHeight() - 2);

      acvComponentLabeling(labelImg);
      acvLabeledRegionInfo(labelImg, &ldData);

      //The first(the idx 0 is not avaliable) ldData must be the trap, set area to zero
      ldData[1].area = 0;

      //Delete the object that has less than certain amount of area on ldData
      acvRemoveRegionLessThan(labelImg, &ldData, 120);

      /*t = clock() - t;
      printf("%fms ..\n", ((double)t) / CLOCKS_PER_SEC * 1000);
      t = clock();*/
      float errorSum=0;
      for (int i = 1; i < ldData.size(); i++)
      {
          //printf("%s:=====%d=======\n", __func__, i);
          acvContourCircleSignature(labelImg, ldData[i], i, signature);

          float sign_error;
          float AngleDiff = SignatureAngleMatching(signature, tar_signature, &sign_error);
          float sign_error_rev;
          SignatureReverse(signature,signature);
          float AngleDiff_rev = SignatureAngleMatching(signature, tar_signature, &sign_error_rev);


          //printf(">sign_error:%f  sign_error_rev:%f\n",sign_error,sign_error_rev);
          bool isInv=false;
          if(sign_error>sign_error_rev)
          {
              isInv=true;
              sign_error=sign_error_rev;
              AngleDiff=-AngleDiff_rev;
          }

          bitf.acvLabeledPixelExtraction(labelImg, &ldData[i], i, &regionXY_);
          float refine_error=bitf.find_subpixel_params( regionXY_,ldData[i], AngleDiff,isInv ,10, 7, 1);//Global fitting
          data[iterX]=
          180 / M_PI * atan2(bitf.NN.layers[0].W[1][0] - bitf.NN.layers[0].W[0][1], bitf.NN.layers[0].W[0][0] + bitf.NN.layers[0].W[1][1]);
          //printf(">  %f %f > %f\n",bitf.NN.layers[0].W[2][0], bitf.NN.layers[0].W[2][1],data[iterX]);


          errorSum+=refine_error;
          /*printf(">%d>sign error:%f\n",i,sign_error);
          if(refine_error>20)
          {
            printf("refine error:%f  BAD..\n\n",refine_error);
          }
          else
          {
            printf("refine error:%f\n\n",refine_error);
          }*/
          //spp.NN.layers[0].printW();

          //printf("translate:%f %f\n", tar_ldData.Center.X - ldData[i].Center.X, tar_ldData.Center.Y - ldData[i].Center.Y);
          /*
          drawSignatureInfo(image,
            ldData[i],signature,
            tar_ldData,tar_signature,AngleDiff);*/
      }
    }
    //printf("errorSum:%f ................\n\n",errorSum);
    t = clock() - t;
    printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);


    float avg_data=0;
    for(int iterX=0;iterX<repeatNum;iterX++)
    {
      avg_data+=data[iterX];
    }
    avg_data/=repeatNum;

    float dev_data=0;
    float dev_data_MAX=0;
    for(int iterX=0;iterX<repeatNum;iterX++)
    {
      float tmp=avg_data-data[iterX];
      tmp*=tmp;
      if(dev_data_MAX<tmp)dev_data_MAX=tmp;
      dev_data+=tmp;
    }
    dev_data/=repeatNum;
    printf("avg:%f  dev:%f M:%f\n",avg_data,sqrt(dev_data),sqrt(dev_data_MAX) );

    //acvLabeledColorDispersion(image,image,ldData.size()/20+5);
    //acvSaveBitmapFile("data/uu_o.bmp",image->ImageData,image->GetWidth(),image->GetHeight());
}


int testX()
{
  vector<acv_XY> tar_signature(360);

  //Just to get target signature
  {
    acvImage *target = new acvImage();

    int ret=acvLoadBitmapFile(target, "data/target.bmp");

    std::vector<acv_LabeledData> ldData;
    acvThreshold(target, 250, 0);
    acvComponentLabeling(target);
    acvLabeledRegionInfo(target, &ldData);
    acvContourCircleSignature(target, ldData[1], 1, tar_signature);
    delete(target);
  }

  acvImage *test1 = new acvImage();
  int ret=acvLoadBitmapFile(test1, "data/test1.bmp");
  acvImage *test1_buff = new acvImage();

  test1_buff->ReSize(test1->GetWidth(), test1->GetHeight());


  clock_t t = clock();


  acvThreshold(test1, 250, 0);
  ContourFeatureDetect(test1,test1_buff,tar_signature);
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
    printf("execute %d times\r\n", repeatNum);

    //ret = testSignature(repeatNum);
    ret = testX();
    {//Test intersect function
      acv_XY p1={.X=-1,.Y=1.1};
      acv_XY p2={.X= 0,.Y=1};
      acv_XY p3={.X= 1,.Y=0};
      acv_XY p4={.X= 1,.Y=-1};
      acv_XY ip = acvIntersectPoint(p1, p2, p3, p4);

      printf("acvIntersectPoint:X:%f Y:%f\r\n", ip.X,ip.Y);
    }

    {
      acv_XY p1={.X=1,.Y=0};
      acv_XY p2={.X= 20,.Y=0};
      acv_XY p3={.X=0,.Y=1};
      acv_XY cc = acvCircumcenter(p1,p2,p3);
      printf("acvCircumcenter:X:%f Y:%f\r\n", cc.X,cc.Y);
    }
    return ret;
}
