#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include "FeatureManager_sig360_circle_line.h"
#include "FM_camera_calibration.h"
#include "FeatureManager_group.h"
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

int FeatureManager_binary_processing_group::FeatureMatching(acvImage *img)
{
  
    error=FeatureReport_ERROR::NONE;
    ldData.resize(0);
    binary_img.ReSize(img->GetWidth(),img->GetHeight());
    
    acvCloneImage( img,&binary_img, -1);
    acvThreshold(&binary_img, 80, 0);
 
    //Draw a labeling black cage for labling algo, which is needed for acvComponentLabeling
    acvDrawBlock(&binary_img, 1, 1, binary_img.GetWidth() - 2, binary_img.GetHeight() - 2);

    int FENCE_AREA = (img->GetWidth()+img->GetHeight())*2-4;//External frame
    {
      int xDist=15;
      acvDrawBlock(&binary_img, xDist, xDist, binary_img.GetWidth() - xDist, binary_img.GetHeight() - xDist);
      FENCE_AREA+=(img->GetWidth()-xDist+img->GetHeight()-xDist)*2-4;
      uint8_t *line2Fill = binary_img.CVector[xDist+3];
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
    acvComponentLabeling(&binary_img);
    acvLabeledRegionInfo(&binary_img, &ldData);

    //FENCE_AREA=110/100;
    int CLimit = (img->GetWidth()*img->GetHeight())*intrusionSizeLimitRatio;//small object=> 1920×1080=>19*10

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
    if(ldData.size()<=1)
    {
      error=FeatureReport_ERROR::GENERIC;
      for(int i=0;i<binaryFeatureBundle.size();i++)
      {
        binaryFeatureBundle[i]->ClearReport();
      }
      return 0;
    }
    ldData[1].area = 0;


    //Delete the object that has less than certain amount of area on ldData
    //acvRemoveRegionLessThan(img, &ldData, 120);


    //acvCloneImage( img,buff, -1);

  
    LOGV("_________  %f %f ",param.ppb2b,param.mmpb2b);
    for(int i=0;i<binaryFeatureBundle.size();i++)
    {
      binaryFeatureBundle[i]->setOriginalImage(img);
      binaryFeatureBundle[i]->setLabeledData(&ldData);
      binaryFeatureBundle[i]->setBacPac(bacpac);
      binaryFeatureBundle[i]->FeatureMatching(&binary_img);
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
  report.data.binary_processing_group.mmpp = bacpac->sampler->mmpp;
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

