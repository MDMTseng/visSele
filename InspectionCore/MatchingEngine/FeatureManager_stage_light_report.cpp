#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <MatchingCore.h>
#include <math.h>
#include <common_lib.h>
#include <FeatureManager_stage_light_report.h>
#include <acvImage_SpDomainTool.hpp>

FeatureManager_stage_light_report::FeatureManager_stage_light_report(const char *json_str) : FeatureManager(json_str)
{
  root = NULL;
  nonBG_thres = 2;
  nonBG_spread_thres = 180;
  int ret = reload(json_str);
  if (ret)
  {
    char msgBuff[200];
    LOGS(msgBuff, "e: parse failed");
    throw std::invalid_argument(msgBuff);
  }
  report.data.stage_light_report.gridInfo = new vector<stage_light_grid_node_info>();
}

FeatureManager_stage_light_report::~FeatureManager_stage_light_report()
{
  delete report.data.stage_light_report.gridInfo;
}

int FeatureManager_stage_light_report::parse_jobj()
{
  const double *_grid_size[2] =
      {JFetch_NUMBER(root, "grid_size[0]"), JFetch_NUMBER(root, "grid_size[1]")};

  if (_grid_size[0] == NULL || _grid_size[1] == NULL)
  {
    return -1;
  }

  grid_size[0] = round(*_grid_size[0]);
  grid_size[1] = round(*_grid_size[1]);
  LOGI("grid:%d,%d", grid_size[0], grid_size[1]);
  const double *p_nonBG_thres = JFetch_NUMBER(root, "nonBG_thres");
  if (p_nonBG_thres != NULL)
  {
    this->nonBG_thres = (float)*p_nonBG_thres;
  }
  LOGI("nonBG_thres:%f  this:%p", this->nonBG_thres, this);

  const double *p_nonBG_spread_thres = JFetch_NUMBER(root, "nonBG_spread_thres");
  if (p_nonBG_spread_thres != NULL)
  {
    this->nonBG_spread_thres = (float)*p_nonBG_spread_thres;
  }

  return 0;
}

int FeatureManager_stage_light_report::reload(const char *json_str)
{
  if (root)
  {
    cJSON_Delete(root);
  }

  root = cJSON_Parse(json_str);
  if (root == NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if (ret_err != 0)
  {
    reload("");
    return -2;
  }
  return 0;
}

float NormalDist(float x, float u, float sigma)
{
  const float sqrt_2_pi = 2.50662827463;
  float x_sub_u_div_sigma = (x - u) / sigma;
  return exp(-x_sub_u_div_sigma * x_sub_u_div_sigma / 2) / sigma / (sqrt_2_pi);
}

int GaussianDistFitting(float *io_mean, float *io_sigma, int *pdf, int *cdf, int infoL, int iterMax = 10, float deltaMin = 0.001)
{
  int pdfSum = cdf[infoL - 1];
  float mean = *io_mean;
  float sigma = *io_sigma;
  int iter = 0;

  //LOGI("iter:%d sigma:%f mean:%f",iter,sigma,mean);
  for (iter = 0; iter < iterMax; iter++)
  {

    if (sigma < 1)
      sigma = 1;
    float mean_nxt = 0;
    float sigma_nxt = 0;
    float wsum = 0;
    float swsum = 0;
    //LOGI("iter:%d sigma:%f mean:%f",iter,sigma,mean);
    //float cutOffWindow=NormalDist(2*sigma,0,sigma);
    for (int i = 0; i < infoL; i++)
    {
      if (abs(i - mean) > (5 * sigma))
        continue;
      float window = NormalDist(i, mean, 2 * sigma);
      float prob = ((float)pdf[i]) / pdfSum;
      float wprob = window * prob;

      mean_nxt += i * wprob;
      sigma_nxt += (i - mean) * (i - mean) * prob;

      wsum += wprob;
      swsum += prob;
    }
    mean_nxt /= wsum;
    sigma_nxt = sqrt(sigma_nxt / swsum);
    float diffX = abs(mean - mean_nxt) + abs(sigma_nxt - sigma);
    mean = mean_nxt;
    sigma = sigma_nxt;
    //LOGI("sigma:%f mean_delta:%f mean:%f s_nxt:%f",sigma,mean_nxt,mean,sigma_nxt);

    if (diffX < deltaMin)
      break;
  }

  //LOGI("iter:%d sigma:%f mean:%f",iter,sigma,mean);
  *io_sigma = sigma;
  *io_mean = mean;

  if (false)
  {
    float fsigma = 0;
    float fsigma_wsum = 0;

    for (int i = 0; i < infoL; i++)
    {
      if (abs(i - mean) > (5 * sigma))
        continue;
      float window = NormalDist(i, mean, 2 * sigma);
      float prob = ((float)pdf[i]) / pdfSum;
      float wprob = window * prob;
      fsigma += (i - mean) * (i - mean) * prob;
      fsigma_wsum += prob;
    }

    *io_sigma = sqrt(fsigma / fsigma_wsum);
  }

  if (iter < iterMax)
    return 0;
  return 1;
}

int OTSU_Histo_Threshold(const int *hist, int histLen = 256)
/* binarization by Otsu's method 
	based on maximization of inter-class variance */

{
  const int GRAYLEVEL = 256;
  if (histLen > GRAYLEVEL)
  {
    return -1;
  }
  float prob[GRAYLEVEL], omega[GRAYLEVEL]; /* prob of graylevels */
  float myu[GRAYLEVEL];                    /* mean value for separation */
  float max_sigma, sigma[GRAYLEVEL];       /* inter-class variance */
  int i, x, y;                             /* Loop variable */
  int threshold;                           /* threshold for binarization */

  float totalCount;
  for (i = 0; i < histLen; i++)
  {
    totalCount += hist[i];
  }
  for (i = 0; i < histLen; i++)
  {
    prob[i] = (double)hist[i] / totalCount;
  }

  /* omega & myu generation */
  omega[0] = prob[0];
  myu[0] = 0.0; /* 0.0 times prob[0] equals zero */
  for (i = 1; i < histLen; i++)
  {
    omega[i] = omega[i - 1] + prob[i];
    myu[i] = myu[i - 1] + i * prob[i];
  }

  /* sigma maximization
     sigma stands for inter-class variance 
     and determines optimal threshold value */
  threshold = 0;
  max_sigma = 0.0;
  for (i = 0; i < histLen - 1; i++)
  {
    if (omega[i] != 0.0 && omega[i] != 1.0)
      sigma[i] = pow(myu[histLen - 1] * omega[i] - myu[i], 2) /
                 (omega[i] * (1.0 - omega[i]));
    else
      sigma[i] = 0.0;
    if (sigma[i] > max_sigma)
    {
      max_sigma = sigma[i];
      threshold = i;
    }
  }
  return threshold;
}

int backLightBlockCalc(acvImage *img, int X, int Y, int W, int H, stage_light_grid_node_info *ret_info)
{
  if (ret_info == NULL)
    return -1;
  if (img == NULL)
    return -2;

  if (X < 0 || Y < 0 || W < 0 || H < 0 || (X + W) > img->GetWidth() || (Y + H) > img->GetHeight())
  {
    LOGE("Out of bound, X:%d Y:%d X2:%d Y2:%d im_W:%d im_H:%d", X, Y, X + W, Y + H, img->GetWidth(), img->GetHeight());
    return -3;
  }
  
  ret_info->backLightMean = 0;
  ret_info->backLightSigma = 0;
  ret_info->sampRate = 0;


  const int histoSteps = 256;
  ret_info->nodeLocation.X = X + W / 2.0;
  ret_info->nodeLocation.Y = Y + H / 2.0;
  // ret_info->imageMax=0;
  // ret_info->imageMin=99999;

  float fu_mins = 0;
  float samp_rate_mins = 0;
  float fsigma_min = 9999;

  float fu_maxs = 0;
  float fsigma_max = 0;

  int maxSampCount = 0;
  int iterCount = 1;
  int samplingSpace = H * W;
  int samplingCount = samplingSpace * 100 / iterCount / 100 / 2;
  for (int iter = 0; iter < iterCount; iter++)
  {

    int backLightHisto[histoSteps] = {0};
    int CDF[histoSteps] = {0};

    int reSampCount = 0;
    int totalSampleCount = 0;

    int min_bri_thres = 50;
    for (int i = 0; i < (samplingCount + reSampCount); i++)
    {
      int sx = X + (rand() % W);
      int sy = Y + (rand() % H);
      int bri = img->CVector[sy][sx * 3];
      if (bri < min_bri_thres)
      {
        if (reSampCount < samplingCount / 2)
          reSampCount++;
        continue;
      }
      backLightHisto[bri]++;
      totalSampleCount++;
    }
    //LOGI("totalSampleCount:%d samplingCount:%d",totalSampleCount,samplingCount);

    //LOGI("totalSampleCount:%d samplingCount:%d X:%d,Y:%d",totalSampleCount,samplingCount,X,Y);
    if (totalSampleCount < (samplingCount / 20))
    {
      continue;
    }
    if (maxSampCount < totalSampleCount)
    {
      maxSampCount = totalSampleCount;
    }

    int zeroBriCount = backLightHisto[0]; //Backup
    backLightHisto[0] = 0;                //set zero bri as

    for (int i = 0; i < min_bri_thres; i++)
    {
      backLightHisto[i] = 0;
    }

    //int thres = OTSU_Histo_Threshold(backLightHisto);

    CDF[0] = backLightHisto[0];
    for (int i = 1; i < histoSteps; i++)
    {
      CDF[i] = backLightHisto[i];
      CDF[i] += CDF[i - 1];
      //LOGI("bri:%d==>pdf:%6d cdf:%6d",i,backLightHisto[i],CDF[i]);
    }
    int totalC = CDF[histoSteps - 1];

    //initial estimation
    int maxSumCentral;
    int maxSum = 0;
    int segWidth = 10;

    for (int i = segWidth; i < histoSteps; i++)
    {
      int segSum = CDF[i] - CDF[i - segWidth];
      if (maxSum < segSum)
      {
        maxSumCentral = i - (segWidth / 2);
        maxSum = segSum;
      }
    }
    // if(X==0 && Y==0)
    // {
    //   for(int k=0;k<256;k++)

    //     LOGI("CDF[%d]:%d",k,CDF[k]);

    //   LOGI("mC:%d mS:%d",maxSumCentral,maxSum);
    // }

    //LOGI("maxSumCentral:%d segMaxSum:%d",maxSumCentral,maxSum);

    //Gaussian dist matching
    int iterationMax = 10;
    float fu = maxSumCentral;
    float fsigma = segWidth / 5;
    //LOGI("u:%f sigma:%f",fu,fsigma);
    GaussianDistFitting(&fu, &fsigma, backLightHisto, CDF, histoSteps, iterationMax);
    if (fsigma_min > fsigma)
    {
      fsigma_min = fsigma;
      fu_mins = fu;
      samp_rate_mins = (float)totalSampleCount / samplingCount;
    }

    if (fsigma_max < fsigma)
    {
      fsigma_max = fsigma;
      fu_maxs = fu;
    }
  }

  if (maxSampCount == 0)
    return -3;
  //LOGI("Min u:%f sigma:%f sigmaDiff:%f",fu_mins,fsigma_min, fsigma_max-fsigma_min);

  ret_info->backLightMean = fu_mins;
  ret_info->backLightSigma = fsigma_min;
  ret_info->sampRate = samp_rate_mins;
  return 0;
}

int backLightNonBackGroundExclusion(acvImage *img,acvImage *backGround,acvImage *buffer,
  int nonBG_thres,int nonBG_spread_thres)
{
  backGround->ReSize(img);
  buffer->ReSize(img);

  acvCloneImage(img,buffer, -1);
  
  {
    acvBoxFilter(backGround, buffer, 1);
    acvBoxFilter(backGround, buffer, 1);
    acvSobelFilter(backGround, buffer, 1);

    //CH1 Y dir, 
    //CH2 X dir 
    //CH3 grayLevel

    acvImage *sobelImg = backGround;
    for (int i = 0; i < sobelImg->GetHeight(); i++)
    {
      for (int j = 0; j < sobelImg->GetWidth(); j++)
      {
        int sobelY = (int8_t)sobelImg->CVector[i][j * 3];
        int sobelX = (int8_t)sobelImg->CVector[i][j * 3 + 1];
        int bri = sobelImg->CVector[i][j * 3 + 2];

        int edgeResp = sobelX * sobelX + sobelY * sobelY;

        if (edgeResp > nonBG_thres) //edgeRegion
        {
          sobelImg->CVector[i][j * 3] =0;
        }
        else
        {
          sobelImg->CVector[i][j * 3] =255;
        }
      }
    }

  }
  {
    //backGround
    //CH1 BG/non BG, 
    //CH2 X dir 
    //CH3 grayLevel
    acvBoxFilter(buffer, backGround, 2); //spread the black area
    acvBoxFilter(buffer, backGround, 2); //spread the black area

    for (int i = 0; i < backGround->GetHeight(); i++)
      for (int j = 0; j < backGround->GetWidth(); j++)
      {
        int bri;
        if (backGround->CVector[i][j * 3] < nonBG_spread_thres) //edgeRegion
        {
          bri=0;
        }
        else
        {
          bri = backGround->CVector[i][j * 3 + 2];
        }

        backGround->CVector[i][j * 3]=
        backGround->CVector[i][j * 3+1]=
        backGround->CVector[i][j * 3+2]=bri;
      }
  }

  return 0;
}


int FeatureManager_stage_light_report::FeatureMatching(acvImage *p_img)
{
  report.bacpac=bacpac;
  LOGI("T,nonBG_thres:%f  this:%p", this->nonBG_thres, this);
  report.type = FeatureReport::stage_light_report;
  report.data.stage_light_report.gridInfo->clear();
  report.data.stage_light_report.gridInfo->reserve(grid_size[0] * grid_size[1]);

  report.data.stage_light_report.targetImageDim[0] = p_img->GetWidth();
  report.data.stage_light_report.targetImageDim[1] = p_img->GetHeight();
  LOGI("T0");

  acvImage &img_wo_edge = cacheImage2;
  backLightNonBackGroundExclusion(
    p_img,
    &img_wo_edge,
    &cacheImage,
    nonBG_thres, nonBG_spread_thres);
  

  int Width = img_wo_edge.GetRealWidth();
  int Height = img_wo_edge.GetRealHeight();
  int sBlockW = Width / grid_size[0];
  int sBlockH = Height / grid_size[1];
  for (int i = 0; i < grid_size[1]; i++)
  {
    for (int j = 0; j < grid_size[0]; j++)
    {
      stage_light_grid_node_info info;
      info.nodeIndex.X = j;
      info.nodeIndex.Y = i;
      // if(j==0)
      //   LOGI("==============");
      int ret = backLightBlockCalc(&img_wo_edge, sBlockW * j, sBlockH * i, sBlockW, sBlockH, &info);
      info.error = ret;
      if (ret != 0)
      {
      }

      //LOGI("Max:%f Min:%f",info.backLightMax,info.backLightMin);
      //LOGI("bl_sigma:%f bl_mean:%f",info.backLightSigma,info.backLightMean);
      report.data.stage_light_report.gridInfo->push_back(info);
      //LOGI("                       X:%f Y:%f",info.nodeLocation.X,info.nodeLocation.Y);
    }
  }
  return 0;
}

const FeatureReport *FeatureManager_stage_light_report::GetReport()
{
  return &report;
}

void FeatureManager_stage_light_report::ClearReport()
{
  FeatureManager::ClearReport();
}