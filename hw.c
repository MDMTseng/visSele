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
    BYTE *L;

    for (i = 0; i < src->GetHeight(); i++)
    {
        L = &(src->CVector[i][0]);
        for (j = 0; j < src->GetWidth(); j++, L += 3)
        {
            char Lp0, Lp1;
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
    acvBoxFilter(buff, img, 4);
    acvBoxFilter(buff, img, 4);

    //acvThreshold_single(img, 30, 0);
    acvContrast(img,img,0,1,0);
    acvCloneImage(img, img_thin_blur, 0);
    //acvSaveBitmapFile("data/preprocess_1st.bmp", img_thin_blur);

    acvBoxFilter(buff, img, 3);
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
    acvThreshold(tmp,128);
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


int testSignature()
{

    vector<acv_XY> tar_signature(360);
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

    clock_t t = clock();

    //image->RGBToGray();
    acvCloneImage(image, labelImg, -1);
    preprocess(labelImg, image, buff);
    t = clock() - t;
    printf("%fms .preprocess.\n", ((double)t) / CLOCKS_PER_SEC * 1000);
    t = clock();

    //Create a trap to capture/link boundery object
    acvDrawBlock(labelImg, 1, 1, labelImg->GetWidth() - 2, labelImg->GetHeight() - 2);

    acvComponentLabeling(labelImg);
    acvLabeledRegionInfo(labelImg, &ldData);

    //The first(the idx 0 is not avaliable) ldData must be the trap, set area to zero
    ldData[1].area = 0;

    //Delete the object that has less than certain amount of area on ldData
    acvRemoveRegionLessThan(labelImg, &ldData, 120);

    t = clock() - t;
    printf("%fms ..\n", ((double)t) / CLOCKS_PER_SEC * 1000);
    t = clock();

    for (int i = 1; i < ldData.size(); i++)
    {
        //printf("%s:=====%d=======\n", __func__, i);
        acvContourCircleSignature(labelImg, ldData[i], i, signature);

        float sign_error;
        float AngleDiff = SignatureAngleMatching(signature, tar_signature, &sign_error);
        float sign_error_rev;
        SignatureReverse(signature,signature);
        float AngleDiff_rev = SignatureAngleMatching(signature, tar_signature, &sign_error_rev);


                printf(">sign_error:%f  sign_error_rev:%f\n",sign_error,sign_error_rev);
        bool isInv=false;
        if(sign_error>sign_error_rev)
        {
            isInv=true;
            sign_error=sign_error_rev;
            AngleDiff=-AngleDiff_rev;
        }

        bitf.acvLabeledPixelExtraction(labelImg, &ldData[i], i, &regionXY_);
        float refine_error=bitf.find_subpixel_params( regionXY_,ldData[i], AngleDiff,isInv , 10);//Global fitting

        printf(">%d>sign error:%f\n",i,sign_error);
        if(refine_error>20)
        {
          printf("refine error:%f  BAD..\n\n",refine_error);
        }
        else
        {
          printf("refine error:%f\n\n",refine_error);
        }
        //spp.NN.layers[0].printW();

        //printf("translate:%f %f\n", tar_ldData.Center.X - ldData[i].Center.X, tar_ldData.Center.Y - ldData[i].Center.Y);
        /*
        drawSignatureInfo(image,
          ldData[i],signature,
          tar_ldData,tar_signature,AngleDiff);*/
    }

    t = clock() - t;
    printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);
    //acvLabeledColorDispersion(image,image,ldData.size()/20+5);
    //acvSaveBitmapFile("data/uu_o.bmp",image->ImageData,image->GetWidth(),image->GetHeight());
}

#include <vector>
int main()
{
    int ret = 0;
    ret = testSignature();

    printf("Hello, World! %d", ret);
    return ret;
}
