#include <stdio.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
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
                float length = sqrt(Lp0 * Lp0 + Lp1 * Lp1);
                L[0] = (int)round((int)Lp0 * 127 / length);
                L[1] = (int)round((int)Lp1 * 127 / length);
            }
        }
    }
}

void TargetPrep()
{
    acvImage *tar = new acvImage();
    acvImage *buff = new acvImage();
    acvLoadBitmapFile(tar, "data/target.bmp");
    buff->ReSize(tar->GetWidth(), tar->GetHeight());

    acvBoxFilter(buff, tar, 5);
    acvBoxFilter(buff, tar, 10);
    acvCloneImage(tar, tar, 0);
    acvSaveBitmapFile("data/target_soft.bmp", tar->ImageData, buff->GetWidth(), buff->GetHeight());

    acvSobelFilter(buff, tar);
    acvScalingSobelResult_n(buff);
    acvImageAdd(buff, 128);

    acvBoxFilter(tar, buff, 5);
    buff->ChannelOffset(1);
    acvImageAdd(buff, 128);
    acvBoxFilter(tar, buff, 5);
    buff->ChannelOffset(-1);
    //acvCloneImage(buff,buff,0);
    acvSaveBitmapFile("data/target_sobel.bmp", buff->ImageData, buff->GetWidth(), buff->GetHeight());
}

//Displacement, Scale, Aangle
void acvLabeledPixelExtraction(acvImage *LabelPic, acv_LabeledData *target_info, int target_idx, std::vector<acv_XY> *retData)
{
    retData->clear();
    int i, j;
    BYTE *L;

    for (i = target_info->LTBound.Y; i < target_info->RBBound.Y + 1; i++)
    {
        L = &(LabelPic->CVector[i][(int)target_info->LTBound.X * 3]);
        for (j = target_info->LTBound.X; j < target_info->RBBound.X + 1; j++, L += 3)
        {
            _24BitUnion *lebel = (_24BitUnion *)L;

            if (lebel->_3Byte.Num == target_idx)
            {
                acv_XY XY = {.X = j, .Y = i};
                retData->push_back(XY);
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
void Target_prep_dist(acvImage *target, acvImage *target_DistGradient, vector<acv_XY> &signature, acv_LabeledData &signInfo)
{
    acvLoadBitmapFile(target, "data/target.bmp");
    acvImage *sign = new acvImage();
    acvImage *tmp = new acvImage();
    target_DistGradient->ReSize(target->GetWidth(), target->GetHeight());
    tmp->ReSize(target->GetWidth(), target->GetHeight());
    sign->ReSize(target->GetWidth(), target->GetHeight());

    acvCloneImage(target, tmp, -1);

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

    acvCloneImage(target, sign, -1);
    //Generate signature
    preprocess(sign, target, tmp);

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
void DotsTransform(std::vector<acv_XY> &XY, std::vector<acv_XY> &tXY, MLNN &NN, acv_XY transOffset, float scale)
{
    vector<vector<float>> &in_vec = NN.get_input_vec();
    tXY.resize(XY.size());
    for (int j = 0; j < in_vec.size(); j++)
    {
        in_vec[j][0] = (XY[j].X - transOffset.X) * scale;
        in_vec[j][1] = (XY[j].Y - transOffset.Y) * scale;
    }

    MLNNUtil nu;
    //  nu.printMat(NN.layers[0].W);
    NN.ForwardPass();
    for (int j = 0; j < NN.p_pred_Y->size(); j++)
    {
        tXY[j].X = (*NN.p_pred_Y)[j][0] / scale + transOffset.X;
        tXY[j].Y = (*NN.p_pred_Y)[j][1] / scale + transOffset.Y;
        //printf(">>%f %f %f\n",(*NN.p_pred_Y)[j][1],in_vec[j][0],in_vec[j][1]);
    }
}

void sampleXYFromRegion(vector<acv_XY> &sampleXY, const vector<acv_XY> &regionXY, int sampleCount)
{
    sampleXY.clear();
    static int offset = rand() % (regionXY.size() / sampleCount);
    for (int i = 0; i < sampleCount; i++)
    {

        int randIdx = i * regionXY.size() / sampleCount + offset + rand() % 50;
        randIdx = randIdx % regionXY.size();
        sampleXY.push_back(regionXY[randIdx]);
    }
}

void sampleXYFromRegion_Seq(vector<acv_XY> &sampleXY, const vector<acv_XY> &regionXY, int from, int sampleCount)
{
    sampleXY.clear();
    for (int i = 0; i < sampleCount; i++)
    {
        sampleXY.push_back(regionXY[from + i]);
    }
}

#include <unistd.h>

float SignatureMatchingError(const acv_XY *signature, int offset,
                             const acv_XY *tar_signature, int arrsize, int stride)
{
    if (stride == 0)
        return -1;
    float errorSum = 0;
    int signIdx = offset % arrsize;
    if (signIdx < 0)
        signIdx += arrsize;
    int size = (arrsize - signIdx);
    int i = 0;

    for (; i < size; i += stride, signIdx += stride)
    {
        float error = signature[(signIdx)].X - tar_signature[i].X;
        errorSum += error * error;
    }
    signIdx -= arrsize;
    size = arrsize;
    for (; i < size; i += stride, signIdx += stride)
    {
        float error = signature[(signIdx)].X - tar_signature[i].X;
        errorSum += error * error;
    }
    return errorSum;
}

float SignatureMatchingError(const vector<acv_XY> &signature, int offset,
                             const vector<acv_XY> &tar_signature, int stride)
{
    return SignatureMatchingError(&(signature[0]), offset, &(tar_signature[0]), signature.size(), stride);
}
#include <float.h>
int SignareIdxOffsetMatching(const vector<acv_XY> &signature,
                             const vector<acv_XY> &tar_signature, int roughSearchSampleRate, float *min_error)
{
    if (roughSearchSampleRate < 1)
        return -1;
    int fineSreachRadious = roughSearchSampleRate - 1;
    int minErrOffset = 0;
    float minErr = FLT_MAX; //rough search
    for (int j = 0; j < tar_signature.size(); j += roughSearchSampleRate)
    {
        float error = SignatureMatchingError(signature, j, tar_signature, roughSearchSampleRate);
        if (minErr > error)
        {
            minErr = error;
            minErrOffset = j;
        }
    }
    minErr = FLT_MAX;
    int searchHead = minErrOffset - fineSreachRadious;
    minErrOffset = -1;

    float error;
    //fine search
    for (int i = 0; i < 2 * fineSreachRadious + 1; i++, searchHead++)
    {

        error = SignatureMatchingError(signature, searchHead, tar_signature, 1);
        if (minErr > error)
        {
            minErr = error;
            minErrOffset = searchHead;
        }
    }
    if (minErrOffset < 0)
        minErrOffset += tar_signature.size();
    else if (minErrOffset >= tar_signature.size())
        minErrOffset -= tar_signature.size();
    if (min_error)
        *min_error = minErr;
    return minErrOffset;
}

float SignatureAngleMatching(const vector<acv_XY> &signature,
                             const vector<acv_XY> &tar_signature, float *min_error)
{
    int matchingIdx = SignareIdxOffsetMatching(signature, tar_signature, signature.size() / 160, min_error); //magic number
    float angle = matchingIdx * 2 * M_PI / signature.size();
    if (angle < -M_PI)
        angle += 2 * M_PI;
    else if (angle > M_PI)
        angle -= 2 * M_PI;
    return angle;
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

typedef struct
{
    MLNN NN;
    MLOpt MO;
    vector<vector<float>> error_gradient;
    vector<acv_XY> regionSampleXY;

    vector<acv_XY> mappedXY;
    vector<acv_XY> errorXY;

    acv_LabeledData tar_ldData;
    acv_LabeledData src_ldData;

    acvImage *tarImg;
    acvImage *srcImg;
    acvImage *tarDistGradient;
} SPPARAMX;

void init_SPPARAMX(SPPARAMX &spp,
                   acv_LabeledData &tar_ldData,
                   acvImage *tarImg,
                   acvImage *srcImg,
                   acvImage *target_DistGradient)
{   //*******************************************
    int dim_in = 2;
    int dim_out = 2;
    int batchSize = 700;
    int NNDim[] = {dim_in, dim_out};
    MLNNUtil nu;

    nu.Init2DVec(spp.error_gradient, batchSize, dim_out);

    spp.NN.init(batchSize, NNDim, sizeof(NNDim) / sizeof(*NNDim));

    spp.MO.init(spp.NN.layers[0]);
    spp.tar_ldData = tar_ldData;
    spp.tarImg = tarImg;
    spp.srcImg = srcImg;
    spp.regionSampleXY.resize(batchSize);
    spp.tarDistGradient = target_DistGradient;
    //****************************************
}

void find_subpixel_params(SPPARAMX &spp,
                          vector<acv_XY> &tracking_region,
                          float AngleDiff, int iterCount)
{
    static int idx_c = 0;
    MLNN &NN = spp.NN;
    float scale = 1;
    //init W params
    NN.layers[0].W[0][0] = cos(AngleDiff) * scale; //Rough angle from signature
    NN.layers[0].W[1][0] = sin(AngleDiff) * scale;
    NN.layers[0].W[1][1] = NN.layers[0].W[0][0];
    NN.layers[0].W[0][1] = -NN.layers[0].W[1][0];

    NN.layers[0].W[2][0] = (spp.tar_ldData.Center.X - spp.src_ldData.Center.X); //rough offset from lebeling
    NN.layers[0].W[2][1] = (spp.tar_ldData.Center.Y - spp.src_ldData.Center.Y);

    spp.errorXY.resize(spp.regionSampleXY.size());
    spp.mappedXY.resize(spp.regionSampleXY.size());
    float alpha = 6;
    float alphaDown = (alpha - 0.2) / iterCount;
    for (int j = 0; j < iterCount; j++) //Iteration
    {
        sampleXYFromRegion(spp.regionSampleXY, tracking_region, spp.regionSampleXY.size());

        DotsTransform(spp.regionSampleXY, spp.mappedXY, NN, spp.src_ldData.Center, 1);

        //return;
        float error = acvSpatialMatchingGradient(spp.srcImg, &(spp.regionSampleXY[0]),
                      spp.tarImg, spp.tarDistGradient, &(spp.mappedXY[0]),
                      &(spp.errorXY[0]), spp.regionSampleXY.size());

        for (int k = 0; k < spp.errorXY.size(); k += 1)
        {
            spp.error_gradient[k][0] = -spp.errorXY[k].X / (spp.errorXY.size() * 256 * 128);
            spp.error_gradient[k][1] = -spp.errorXY[k].Y / (spp.errorXY.size() * 256 * 128);
        }
        NN.backProp(spp.error_gradient);
        spp.MO.update_dW();
        NN.layers[0].dW[2][0] *= 2500; //Special treat
        NN.layers[0].dW[2][1] *= 2500;
        NN.updateW(alpha);
        alpha -= alphaDown;
        //nu.printMat(NN.layers[0].dW);printf("\n");
        NN.reset_deltaW();

        //Limit transform to be only rotate and translate
        float a00 = (NN.layers[0].W[0][0] + NN.layers[0].W[1][1]) / 2;
        float a10 = (NN.layers[0].W[1][0] - NN.layers[0].W[0][1]) / 2;
        float LL = hypot(a00, a10);
        a00 /= LL;
        a10 /= LL;
        NN.layers[0].W[0][0] = a00;
        NN.layers[0].W[0][1] = -a10;
        NN.layers[0].W[1][0] = a10;
        NN.layers[0].W[1][1] = a00;

        /*if(j%1!=0)continue;
        continue;*/
        if (j != iterCount - 1)
            continue;
        printf(">%f %f %f\n",
               NN.layers[0].W[2][0], NN.layers[0].W[2][1],
               180 / M_PI * atan2(NN.layers[0].W[1][0] - NN.layers[0].W[0][1], NN.layers[0].W[0][0] + NN.layers[0].W[1][1]));

        //continue;
        sleep(1);

        acvImage buff;
        buff.ReSize(spp.tarImg->GetWidth(), spp.tarImg->GetHeight());

        acvCloneImage(spp.tarImg, &buff, 2);

        for (int k = 0; k < tracking_region.size() / spp.regionSampleXY.size(); k++)
        {
            sampleXYFromRegion_Seq(spp.regionSampleXY, tracking_region,
                                   k * spp.regionSampleXY.size(), spp.regionSampleXY.size());
            DotsTransform(spp.regionSampleXY, spp.mappedXY, NN, spp.src_ldData.Center, 1);
            for (int m = 0; m < spp.mappedXY.size(); m++)
            {
                buff.CVector[(int)round(spp.mappedXY[m].Y)][(int)round(spp.mappedXY[m].X) * 3 + 2] =
                    255 - spp.srcImg->CVector[(int)round(spp.regionSampleXY[m].Y)][(int)round(spp.regionSampleXY[m].X) * 3 + 2];
            }
        }
        //acvClear(&buff,128,1);
        char name[100];
        sprintf(name, "data/target_test_cover_%3d.bmp", idx_c++);
        acvSaveBitmapFile(name, &buff);
    }
}

int testSignature()
{

    vector<acv_XY> tar_signature(360);
    acv_LabeledData tar_ldData;
    acvImage *target = new acvImage();
    acvImage *target_DistGradient = new acvImage();
    Target_prep_dist(target, target_DistGradient, tar_signature, tar_ldData);
    //return 0;

    acvImage *image = new acvImage();
    acvImage *labelImg = new acvImage();
    acvImage *buff = new acvImage();
    std::vector<acv_LabeledData> ldData;
    acvLoadBitmapFile(image, "data/test1.bmp");
    buff->ReSize(image->GetWidth(), image->GetHeight());
    labelImg->ReSize(image->GetWidth(), image->GetHeight());
    vector<acv_XY> signature(tar_signature.size());

    SPPARAMX spp;
    init_SPPARAMX(spp, tar_ldData, target, image, target_DistGradient);
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
        spp.src_ldData = ldData[i];
        acvLabeledPixelExtraction(labelImg, &ldData[i], i, &regionXY_);

        find_subpixel_params(spp, regionXY_, AngleDiff, 10);
        spp.NN.layers[0].printW();
        //END ********************Sub-pixel leel refinment

        t = clock() - t;
        printf("%fms .Match. AngleDiff:%f,er:%f\n", ((double)t) / CLOCKS_PER_SEC * 1000,
               AngleDiff * 180 / M_PI, error);
        printf("translate:%f %f\n", tar_ldData.Center.X - ldData[i].Center.X, tar_ldData.Center.Y - ldData[i].Center.Y);
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
