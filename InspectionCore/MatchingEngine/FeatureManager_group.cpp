#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include "FeatureManager_sig360_circle_line.h"
#include "FM_camera_calibration.h"
#include "FeatureManager_group.h"
#include "BackLightFieldCalib.h"
#include "LabelingCV.h"
#include "BinarizeCV.h"
/*
  FeatureManager_group_proto Section
*/
int FeatureManager_group_proto::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }
  clearFeatureGroup();
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

int FeatureManager_group_proto::parse_jobj()
{
  const char *type_str= (char *)JFetch(root,"type",cJSON_String);
  if(type_str==NULL)
  {
    LOGE("ptr: type:<%p> ",type_str);
    return -1;
  }
  LOGI("type:<%s> ",type_str);


  briThres=JFetch_NUMBER_ex(root,"briThres",80);

  // Calibration-free vignette-tolerant binarization (bg-flatten).
  binarize_method = 0;
  {
    char *bm = (char *)JFetch(root, "binarize", cJSON_String);
    if (bm != NULL && strcmp(bm, "bg_flatten") == 0) binarize_method = 1;
    bg_close_kernel = (int)JFetch_NUMBER_ex(root, "bg_close_kernel", 81);
    bg_ratio = JFetch_NUMBER_ex(root, "bg_ratio", 0.5);
    bg_downscale = (int)JFetch_NUMBER_ex(root, "bg_downscale", 4);
  }

  // Optional per-region adaptive threshold (background-evenness soft calib).
  // Schema: "adaptiveThres":{ "enable":true, "ratio":0.5, "gridW":W,"gridH":H,
  //                           "bright":[...W*H...], "dark":[...W*H...](optional) }
  useAdaptiveThres = false;
  useCalibBackground = false;
  bgThreshMap.clear();
  {
    cJSON *adaptive = cJSON_GetObjectItem(root, "adaptiveThres");
    int enable = (adaptive) ? (cJSON_IsTrue(cJSON_GetObjectItem(adaptive, "enable")) ? 1 : 0) : 0;
    const char *source = (adaptive) ? (const char *)JFetch(adaptive, "source", cJSON_String) : NULL;
    if (adaptive && enable && source && strcmp(source, "calib") == 0)
    {
      // Per-camera background: build the threshold map from the loaded
      // stage-light model (sampler) at match time. T = D + ratio*(B - D).
      edgeRatio = JFetch_NUMBER_ex(adaptive, "ratio", 0.5);
      darkLevel = JFetch_NUMBER_ex(adaptive, "dark", 0);
      useCalibBackground = true;
      useAdaptiveThres = true;
      LOGI("adaptiveThres source=calib ratio=%.2f dark=%.1f", edgeRatio, darkLevel);
    }
    else if (adaptive && enable)
    {
      edgeRatio = JFetch_NUMBER_ex(adaptive, "ratio", 0.5);
      bgMapW = (int)JFetch_NUMBER_ex(adaptive, "gridW", 0);
      bgMapH = (int)JFetch_NUMBER_ex(adaptive, "gridH", 0);
      cJSON *bright = cJSON_GetObjectItem(adaptive, "bright");
      cJSON *dark = cJSON_GetObjectItem(adaptive, "dark");
      int n = bgMapW * bgMapH;
      if (cJSON_IsArray(bright) && cJSON_GetArraySize(bright) == n && n > 0)
      {
        bgThreshMap.resize(n);
        for (int k = 0; k < n; k++)
        {
          float B = (float)cJSON_GetArrayItem(bright, k)->valuedouble;
          float D = 0.0f;
          if (cJSON_IsArray(dark) && cJSON_GetArraySize(dark) == n)
            D = (float)cJSON_GetArrayItem(dark, k)->valuedouble;
          bgThreshMap[k] = D + edgeRatio * (B - D);
        }
        useAdaptiveThres = true;
        LOGI("adaptiveThres ON %dx%d ratio=%.2f", bgMapW, bgMapH, edgeRatio);
      }
      else
      {
        LOGE("adaptiveThres enabled but bright grid size != gridW*gridH (%d)", n);
      }
    }
  }

  cJSON *featureSetList = cJSON_GetObjectItem(root,"featureSet");

  if(featureSetList==NULL)
  {
    LOGE("featureSetList array does not exists");
    return -1;
  }

  if(!cJSON_IsArray(featureSetList))
  {
    LOGE("featureSetList is not an array");
    return -1;
  }

  for (int i = 0 ; i < cJSON_GetArraySize(featureSetList) ; i++)
  {
     cJSON * featureSet = cJSON_GetArrayItem(featureSetList, i);
     int ret = addSubFeature(featureSet);
     if(ret!=0)
     {
       LOGE("Add feature[%d] failed...",i);
       return -1;
     }
  }
  return 0;
}


/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_binary_processing_group::FeatureManager_binary_processing_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  sub_reports.resize(0);
  report.data.binary_processing_group.reports = &sub_reports;
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
  
  ClearReport();
}

int FeatureManager_binary_processing_group::clearFeatureGroup()
{
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      delete binaryFeatureBundle[i];
    }
    binaryFeatureBundle.resize(0);
    return 0;
}

int FeatureManager_binary_processing_group::addSubFeature(cJSON * subFeature)
{
  
  char *str=JFetch_STRING(subFeature,"type");
  if(str==NULL)
  {
    return -1;
  }
  FeatureManager_binary_processing *newFeature=NULL;
  if(strcmp(FeatureManager_sig360_circle_line::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_sig360_circle_line is the type...");
    newFeature = new FeatureManager_sig360_circle_line(cJSON_Print(subFeature));
  }
  else if(strcmp(FeatureManager_sig360_extractor::GetFeatureTypeName(),str) == 0)
  {
    LOGI("FeatureManager_sig360_extractor is the type...");
    newFeature = new FeatureManager_sig360_extractor(cJSON_Print(subFeature));
  }
  // else if(strcmp(FM_camera_calibration::GetFeatureTypeName(),str) == 0)
  // {

  //   LOGI("FeatureManager_camera_calibration is the type...");
  //   newFeature = new FM_camera_calibration(cJSON_Print(subFeature));
  // }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  binaryFeatureBundle.push_back(newFeature);

  sub_reports.resize(binaryFeatureBundle.size());
  report.data.binary_processing_group.reports = &sub_reports;
  report.type = FeatureReport::binary_processing_group;
  return 0;
}
void binaryDownScale(acvImage *dst,acvImage *src,int ds_Factor)
{
  dst->ReSize(src->GetWidth()/ds_Factor,src->GetHeight()/ds_Factor);
  for(int i=0;i<dst->GetHeight();i++)
  {
    for(int j=0;j<dst->GetWidth();j++)
    {
      uint8_t *d_pix=&(dst->CVector[i][j*3]);
      uint8_t *s_pix=&(src->CVector[i*ds_Factor][j*3*ds_Factor]);
      d_pix[0]=s_pix[0];
      d_pix[1]=s_pix[1];
      d_pix[2]=s_pix[2];
    }
  }
}

void labeledUpScale(acvImage *us_dst,acvImage *ds_src,int ds_Factor)
{
  // for(int i=0;i<ds_src->GetHeight();i++)
  // {
  //   for(int j=0;j<ds_src->GetWidth();j++)
  //   {
      
  //     uint8_t *s_pix=&(ds_src->CVector[i][j*3]);
  //     if(s_pix[0]!=255)
  //       LOGI("[%d %d] :%d %d %d",j,i,s_pix[0],s_pix[1],s_pix[2]);
  //   }
  // }

  uint8_t *s_pix00,*s_pix01,*s_pix10,*s_pix11;
  int swidth=ds_src->GetWidth();
  uint8_t *L_pix;
  for(int i=0;i<us_dst->GetHeight()-ds_Factor;i++)
  {

    for(int j=0;j<us_dst->GetWidth();j++)
    {
      uint8_t *d_pix=&(us_dst->CVector[i][j*3]);
      if(j%ds_Factor==0)
      {
        uint8_t *s_pix=&(ds_src->CVector[i/ds_Factor][(j/ds_Factor)*3]);
        s_pix00=s_pix;
        s_pix01=s_pix00+3;
        s_pix10=s_pix00+swidth*3;
        s_pix11=s_pix10+3;

        if(s_pix00[2]!=255)
          L_pix=s_pix00;
        else if(s_pix01[2]!=255)
          L_pix=s_pix01;
        else if(s_pix10[2]!=255)
          L_pix=s_pix10;
        else if(s_pix11[2]!=255)
          L_pix=s_pix11;
      }
      

      if(d_pix[2]!=255)
      {
        d_pix[0]=L_pix[0];
        d_pix[1]=L_pix[1];
        d_pix[2]=L_pix[2];
      }
    }
  }
}

int FeatureManager_binary_processing_group::FeatureMatching(acvImage *img)
{
  report.bacpac=bacpac;
    error=FeatureReport_ERROR::NONE;
    ldData.resize(0);
    binary_img.ReSize(img->GetWidth(),img->GetHeight());

    // Per-camera adaptive threshold: lazily build the threshold map from the
    // loaded background model (sampler->stageLightInfo), robustly cleaned of
    // scratches/dust, as T = D + ratio*(B - D).
    if (useCalibBackground && bgThreshMap.empty() && bacpac && bacpac->sampler)
    {
      stageLightParam *sl = bacpac->sampler->getStageLightInfo();
      if (sl && sl->BG_nodes.size() > 0 && sl->idxW > 0 && sl->idxH > 0)
      {
        int W = sl->idxW, H = sl->idxH;
        std::vector<float> B(W * H, NAN);
        for (BGLightNodeInfo &n : sl->BG_nodes)
          if (n.index.X >= 0 && n.index.X < W && n.index.Y >= 0 && n.index.Y < H)
            B[n.index.Y * W + n.index.X] = n.mean;

        int rejected = backLightField_robustClean(B.data(), W, H, 2.5f, 3);
        bgThreshMap.resize(W * H);
        for (int k = 0; k < W * H; k++)
          bgThreshMap[k] = darkLevel + edgeRatio * (B[k] - darkLevel);
        bgMapW = W; bgMapH = H;
        LOGI("adaptiveThres calib map %dx%d ratio=%.2f dark=%.1f rejectedZones=%d",
             W, H, edgeRatio, darkLevel, rejected);
      }
    }

    if (binarize_method == 1) // calibration-free vignette-tolerant bg-flatten
      binarize_bg_flatten_cv(img, &binary_img, bg_close_kernel, bg_ratio, bg_downscale);
    else if (useAdaptiveThres && !bgThreshMap.empty())
      acvThresholdMap(&binary_img, img, bgThreshMap.data(), bgMapW, bgMapH, 0);
    else
      acvThreshold(&binary_img,img, briThres, 0);

    int downScaleF=1;//
    acvImage *lableImg=&ds_binary_img;
    if(downScaleF==1)
    {
      lableImg=&binary_img;
    }
    else
    {
      binaryDownScale(lableImg,&binary_img,downScaleF);
    }
 
    //Draw a labeling black cage for labling algo, which is needed for acvComponentLabeling

    acvDrawBlock(lableImg, 1, 1, lableImg->GetWidth() - 2, lableImg->GetHeight() - 2);

    int FENCE_AREA = (lableImg->GetWidth()+lableImg->GetHeight())*2-4;//External frame
    {
      int xDist=15/downScaleF;
      acvDrawBlock(lableImg, xDist, xDist, lableImg->GetWidth() - xDist, lableImg->GetHeight() - xDist);
      FENCE_AREA+=(img->GetWidth()-xDist+img->GetHeight()-xDist)*2-4;
      uint8_t *line2Fill = lableImg->CVector[xDist+3];
      for(int i=1;i<xDist;i++)
      {
        line2Fill[i*3]=
        line2Fill[i*3+1]=
        line2Fill[i*3+2]=0;
      }
      FENCE_AREA+=xDist;

    }
    //The labeling starts from (1 1) => (W-2,H-2), ie. it will not touch the outmost pixel to simplify the boundary condition
    //You need to draw a black/white cage to work(not crash).
    //The advantage of black cage is you can know which area touches the boundary then we can exclude it
    // CCL once -> packed label image + acv_LabeledData (from stats), no rescan.
    acvComponentLabeling_cv(lableImg, ldData);

    //FENCE_AREA=110/100;
    int CLimit = (lableImg->GetWidth()*lableImg->GetHeight())*intrusionSizeLimitRatio;//small object=> 1920×1080=>19*10

    int intrusionObjectArea = ldData[1].area - FENCE_AREA;
    LOGI("%d>OBJ:%d  CLimit:%d",ldData[1].area,intrusionObjectArea,CLimit);
    if(intrusionObjectArea>CLimit)
    {//If the cage connects something link to the edge we don't want to do the inspection
      error=FeatureReport_ERROR::EXTERNAL_INTRUSION_OBJECT;
      
      for(int i=0;i<binaryFeatureBundle.size();i++)
      {
        binaryFeatureBundle[i]->ClearReport();
      }
      return 0;
    }

    // if(downScaleF!=1)
    // {
    //   for(int i=2;i<ldData.size();i++)
    //   {
    //     ldData[i].area*=downScaleF*downScaleF;
    //     ldData[i].Center=acvVecMult(ldData[i].Center,downScaleF);
    //     ldData[i].LTBound=acvVecMult(ldData[i].LTBound,downScaleF);
    //     ldData[i].RBBound=acvVecMult(ldData[i].RBBound,downScaleF);
    //   }
    //   labeledUpScale(&binary_img,lableImg,downScaleF);
    //   downScaleF=1;
    // }




    if(ldData.size()<=1)
    {
      error=FeatureReport_ERROR::GENERIC;
      for(int i=0;i<binaryFeatureBundle.size();i++)
      {
        binaryFeatureBundle[i]->ClearReport();
      }
      return 0;
    }


    ldData[1].area =intrusionObjectArea;

    //Delete the object that has less than certain amount of area on ldData
    //acvRemoveRegionLessThan(img, &ldData, 120);


    //acvCloneImage( img,buff, -1);

    // return 0;
    // LOGI("_________  %f %f ",param.ppb2b,param.mmpb2b);
    
    LOGI(">>>> ");
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      binaryFeatureBundle[i]->setOriginalImage(img);
      binaryFeatureBundle[i]->setLabeledData(&ldData);
      binaryFeatureBundle[i]->setBacPac(bacpac);
      binaryFeatureBundle[i]->setLabelDownSampLevel(downScaleF);
      binaryFeatureBundle[i]->FeatureMatching(lableImg);
    }
  return 0;
}


const FeatureReport* FeatureManager_binary_processing_group::GetReport()
{
  if(binaryFeatureBundle.size()!=sub_reports.size())
  {
    sub_reports.resize(binaryFeatureBundle.size());
  }
  
  report.data.binary_processing_group.error = error;

  for(int i=0;i<sub_reports.size();i++)
  {
    sub_reports[i] = binaryFeatureBundle[i]->GetReport();
  }
  report.type = FeatureReport::binary_processing_group;
  report.data.binary_processing_group.reports = &sub_reports;
  report.data.binary_processing_group.labeledData = &ldData;
  report.data.binary_processing_group.subFeatureDefSha1 = subFeatureDefSha1;
  report.data.binary_processing_group.mmpp = bacpac->sampler->mmpP_ideal();
  return &report;
}


void FeatureManager_binary_processing_group::ClearReport()
{
  if(binaryFeatureBundle.size()!=sub_reports.size())
  {
    sub_reports.resize(binaryFeatureBundle.size());
  }

  for(int i=0;i<sub_reports.size();i++)
  {
    binaryFeatureBundle[i]->ClearReport();
  }
  report.type = FeatureReport::binary_processing_group;
  report.data.binary_processing_group.reports = &sub_reports;
  report.data.binary_processing_group.labeledData = &ldData;
  report.data.binary_processing_group.error=error=FeatureReport_ERROR::NONE;
}

int FeatureManager_binary_processing_group::parse_jobj()
{
  double *val= JFetch_NUMBER(root,"intrusionSizeLimitRatio");

  intrusionSizeLimitRatio=(val!=NULL)?*val:0;
  
  LOGV("intrusionSizeLimitRatio:%f  ptr:%p",intrusionSizeLimitRatio,val);

  FeatureManager_group_proto::parse_jobj();

  strcpy(subFeatureDefSha1,"");
  const char *sSet_sha1= JFetch_STRING(root,"featureSet_sha1");
  if(sSet_sha1!=NULL)
  {
    strncpy(subFeatureDefSha1,sSet_sha1,sizeof(subFeatureDefSha1));
    subFeatureDefSha1[sizeof(subFeatureDefSha1)-1]=='\0';
  }

  return 0;
}



/*
  FeatureManager_binary_processing_group Section
*/
FeatureManager_group::FeatureManager_group(const char *json_str):
  FeatureManager_group_proto(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_circle_line failed... " );
  
  ClearReport();
  
}

int FeatureManager_group::clearFeatureGroup()
{
    for(int i=0;i<featureBundle.size();i++)
    {
      delete featureBundle[i];
    }
    featureBundle.resize(0);
    return 0;
}

int FeatureManager_group::addSubFeature(cJSON * subFeature)
{
  char *str=JFetch_STRING(subFeature,"type");
  if(str==NULL)
  {
    return -1;
  }
  FeatureManager *newFeature=NULL;
  if(strcmp(FeatureManager_group::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_group is the type...:%s",cJSON_Print(subFeature));
    newFeature = new FeatureManager_group(cJSON_Print(subFeature));
  }
  else if(strcmp(FeatureManager_binary_processing_group::GetFeatureTypeName(),str) == 0)
  {

    LOGI("FeatureManager_binary_processing_group is the type...");
    newFeature = new FeatureManager_binary_processing_group(cJSON_Print(subFeature));
  }
  else
  {
    LOGE("Cannot find a corresponding type...");
    return -1;
  }
  featureBundle.push_back(newFeature);
  return 0;
}


int FeatureManager_group::FeatureMatching(acvImage *img)
{
  for(int i=0;i<featureBundle.size();i++)
  {
    //featureBundle[i]->param;
    featureBundle[i]->setBacPac(bacpac);
    featureBundle[i]->FeatureMatching(img);
  }
  return 0;
}

