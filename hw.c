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


void preprocess_IIR(acvImage *img, acvImage *buff)
{
    //acvBoxFilter(buff,img,4);

    acvIIROrder1Filter(buff, img, 2);
    acvThreshold(img, 100, 1);

    acvIIROrder1Filter(buff, img, 2);
    //acvBoxFilter(buff,img,5);
    acvThreshold(img, 255 - 15, 1);
}

void preprocess(acvImage *img,
                acvImage *img_thin_blur,
                acvImage *buff)
{
    acvBoxFilter(buff, img, 4);
    acvThreshold_single(img, 100, 0);
    acvCloneImage(img, img_thin_blur, -1);
    acvBoxFilter(buff, img_thin_blur, 3);
    //acvCloneImage(img_thin_blur,img_thin_blur,0);

    acvBoxFilter(buff, img, 5);
    acvThreshold(img, 255 - 15, 0);
}
void Target_prep_dist(acvImage *soft_target, acvImage *label_target, acvImage *target_DistGradient, vector<acv_XY> &signature, acv_LabeledData &signInfo)
{
    acvLoadBitmapFile(soft_target, "data/target.bmp");
    acvImage *tmp = new acvImage();
    target_DistGradient->ReSize(soft_target->GetWidth(), soft_target->GetHeight());
    tmp->ReSize(soft_target->GetWidth(), soft_target->GetHeight());

    acvCloneImage(soft_target, tmp, -1);

    int mul = 1;
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

    label_target->ReSize(soft_target->GetWidth(), soft_target->GetHeight());
    acvCloneImage(soft_target, label_target, -1);
    //Generate signature
    preprocess(label_target, soft_target, tmp);

    acvComponentLabeling(label_target);

    std::vector<acv_LabeledData> ldData;
    acvLabeledRegionInfo(label_target, &ldData);
    for (int i = 1; i < ldData.size(); i++)
    {
        //printf("%d:%03d %f %f\n",i,ldData[i].area,ldData[i].Center.X,ldData[i].Center.Y) ;
        acvContourCircleSignature(label_target, ldData[i], i, signature);
        signInfo = ldData[i];
    }
    delete (tmp);
    return;
}

void Target_prep_sobel(acvImage *target, acvImage *target_DistGradient)
{
    acvLoadBitmapFile(target, "data/target.bmp");
    acvImage *tmp = new acvImage();
    target_DistGradient->ReSize(target->GetWidth(), target->GetHeight());
    tmp->ReSize(target->GetWidth(), target->GetHeight());

    acvThreshold(target, 128);
    acvBoxFilter(tmp, target, 1);
    acvBoxFilter(tmp, target, 1);
    acvCloneImage(target, tmp, 1);
    acvCloneImage(target, target, 0);

    int mul = 1;
    acvDistanceTransform_Chamfer(tmp, 5 * mul, 7 * mul);
    //acvDistanceTransform_ChamferX(ss);
    acvInnerFramePixCopy(tmp, 1);

    acvDistanceTransform_Sobel(target_DistGradient, tmp);
    acvInnerFramePixCopy(target_DistGradient, 2);
    acvInnerFramePixCopy(target_DistGradient, 1);
    acvScalingSobelResult_n(target_DistGradient);

    /*

    acvImageAdd(target_DistGradient,128);
    acvBoxFilter(tmp,target_DistGradient,15);
    acvBoxFilter(tmp,target_DistGradient,15);
    acvImageAdd(target_DistGradient,-128);
    target_DistGradient->ChannelOffset(1);
    acvImageAdd(target_DistGradient,128);
    acvBoxFilter(tmp,target_DistGradient,15);
    acvBoxFilter(tmp,target_DistGradient,15);
    //acvBoxFilter(ss,distGradient,15);
    acvImageAdd(target_DistGradient,-128);
    target_DistGradient->ChannelOffset(-1);
    acvScalingSobelResult_n(target_DistGradient);

    */
    delete (tmp);
    return;
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
    acvImage *target_soft = new acvImage();
    acvImage *target_label = new acvImage();
    acvImage *target_DistGradient = new acvImage();

    Target_prep_dist(target_soft,target_label, target_DistGradient, tar_signature, tar_ldData);


    acvImage *image = new acvImage();
    acvImage *labelImg = new acvImage();
    acvImage *buff = new acvImage();
    std::vector<acv_LabeledData> ldData;
    acvLoadBitmapFile(image, "data/test1.bmp");
    buff->ReSize(image->GetWidth(), image->GetHeight());
    labelImg->ReSize(image->GetWidth(), image->GetHeight());
    vector<acv_XY> signature(tar_signature.size());
    BinaryImageTemplateFitting bitf(tar_ldData, target_soft, image, target_DistGradient);

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
        printf("%s:=====%d=======\n", __func__, i);
        acvContourCircleSignature(labelImg, ldData[i], i, signature);

        float error;
        float AngleDiff = SignatureAngleMatching(signature, tar_signature, &error);

        //******************Sub-pixel leel refinment
        bitf.acvLabeledPixelExtraction(target_label, &tar_ldData, 1, &regionXY_);
        bitf.find_subpixel_params( regionXY_,ldData[i], AngleDiff, 10);
        //spp.NN.layers[0].printW();
        //END ********************Sub-pixel leel refinment

        t = clock() - t;
        printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);
        //printf("translate:%f %f\n", tar_ldData.Center.X - ldData[i].Center.X, tar_ldData.Center.Y - ldData[i].Center.Y);
        /*
        drawSignatureInfo(image,
          ldData[i],signature,
          tar_ldData,tar_signature,AngleDiff);*/
        t = clock();
    }

    //acvLabeledColorDispersion(image,image,ldData.size()/20+5);
    //acvSaveBitmapFile("data/uu_o.bmp",image->ImageData,image->GetWidth(),image->GetHeight());
}

#include <vector>
int main()
{
    testSignature();

    int ret = 0;
    printf("Hello, World! %d", ret);
    return ret;
}
