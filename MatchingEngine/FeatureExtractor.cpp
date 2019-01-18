#include "FeatureManager.h"
#include "logctrl.h"
#include <stdexcept>
#include <MatchingCore.h>
#include <common_lib.h>
#include <FeatureManager_binary_processing.h>



FeatureManager_sig360_extractor::FeatureManager_sig360_extractor(const char *json_str): FeatureManager_binary_processing(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FeatureManager_sig360_extractor failed... " );
}

int FeatureManager_sig360_extractor::parse_jobj()
{
  cJSON *subObj = cJSON_GetObjectItem(root,"type");
  const char *type_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"ver");
  const char *ver_str = subObj?subObj->valuestring:NULL;
  const double *mmpp  = JFetch_NUMBER(root,"mmpp");
  if(type_str==NULL||ver_str==NULL||mmpp==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  mmpp(number):<%p>",type_str,ver_str,mmpp);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  mmpp(number):<%f>",type_str,ver_str,*mmpp);



  return 0;
}


int FeatureManager_sig360_extractor::reload(const char *json_str)
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

int FeatureManager_sig360_extractor::FeatureMatching(acvImage *img,acvImage *buff,acvImage *dbg)
{
  
  vector<acv_LabeledData> &ldData=*this->_ldData;
  signature.resize(360);
  detectedCircles.resize(0);
  detectedLines.resize(0);
  int idx=-1;
  int maxArea=0;
  for(int i=1;i<ldData.size();i++)
  {
    if(ldData[i].area<=0)continue;
    if(maxArea<ldData[i].area)
    {
      idx = i;
      maxArea=ldData[i].area;
    }
  }
  if(idx==-1)
  {
    LOGE("Cannot find one component for extractor");
    report.data.sig360_extractor.error =
    FeatureReport_sig360_extractor::ONLY_ONE_COMPONENT_IS_ALLOWED;
    return -1;
  }
  acv_XY center=ldData[idx].Center;
  LOGI("Find the component => idx:%d",idx);
  LOGI(">>>Center X:%f Y:%f...",ldData[idx].Center.X,ldData[idx].Center.Y);
  LOGI(">>>LTBound X:%f Y:%f...",ldData[idx].LTBound.X,ldData[idx].LTBound.Y);
  LOGI(">>>RBBound X:%f Y:%f...",ldData[idx].RBBound.X,ldData[idx].RBBound.Y);
  acvContourCircleSignature(img, ldData[idx], idx, signature);
  acvCloneImage( img,buff, -1);
  //MatchingCore_CircleLineExtraction(img,buff,ldData,detectedCircles,detectedLines);

  LOGI(">>>detectedCircles:%d",detectedCircles.size());
  LOGI(">>>detectedLines:%d",detectedLines.size());

  {
    report.data.sig360_extractor.LTBound=ldData[idx].LTBound;
    report.data.sig360_extractor.RBBound=ldData[idx].RBBound;
    report.data.sig360_extractor.Center=ldData[idx].Center;
    report.data.sig360_extractor.area=ldData[idx].area;

    report.type = FeatureReport::sig360_extractor;
    report.data.sig360_extractor.signature = &signature;
    report.data.sig360_extractor.detectedCircles = &detectedCircles;
    report.data.sig360_extractor.detectedLines = &detectedLines;
  }


#if 0
  for(int i=0;i<detectedCircles.size();i++)
  {
      if(detectedCircles[i].s>0.9)
      {
        logv(">>SKIP...\n");
        continue;
      }
      LOGV(">>cc.X:%f cc.Y:%f r:%f;   s:%f pts:%d",
        detectedCircles[i].circle.circumcenter.X-center.X,detectedCircles[i].circle.circumcenter.Y-center.Y,
        detectedCircles[i].circle.radius,
        detectedCircles[i].s,detectedCircles[i].matching_pts
      );
      acvDrawCircle(buff,
        detectedCircles[i].circle.circumcenter.X, detectedCircles[i].circle.circumcenter.Y,
        detectedCircles[i].circle.radius,
        20,255, 0, 0);

  }

  for(int i=0;i<detectedLines.size();i++)
  {
    acv_Line line = detectedLines[i].line;
    line.line_anchor.X-=center.X;
    line.line_anchor.Y-=center.Y;
    LOGV(">>anchor.X:%f anchor.Y:%f vec.X:%f vec.Y:%f;  s:%f pts:%d",
      line.line_anchor.X,line.line_anchor.Y,
      line.line_vec.X,line.line_vec.Y,
      detectedLines[i].s,detectedLines[i].matching_pts
    );
    float mult=100;
    acvDrawLine(buff,
      detectedLines[i].end_pos.X,detectedLines[i].end_pos.Y,
      detectedLines[i].end_neg.X,detectedLines[i].end_neg.Y,
      20,255,128);
    detectedLines[i].end_pos.X-=center.X;
    detectedLines[i].end_pos.Y-=center.Y;
    detectedLines[i].end_neg.X-=center.X;
    detectedLines[i].end_neg.Y-=center.Y;
    LOGV(">>end_pos.X:%f end_pos.Y:%f end_neg.X:%f end_neg.Y:%f\n",
      detectedLines[i].end_pos.X,detectedLines[i].end_pos.Y,
      detectedLines[i].end_neg.X,detectedLines[i].end_neg.Y
    );
  }

  logv("\"magnitude\":[");
  for(int i=0;i<signature.size();i++)
  {
    logv("%f,",signature[i].X);
  }logv("],\n");


  logv("\"angle\":[");
  for(int i=0;i<signature.size();i++)
  {
    logv("%f,",signature[i].Y);
  }logv("]\n");

#endif
  return 0;
}

const FeatureReport* FeatureManager_sig360_extractor::GetReport()
{
  report.type = FeatureReport::sig360_extractor;

  report.data.sig360_extractor.signature = &signature;
  report.data.sig360_extractor.detectedCircles = &detectedCircles;
  report.data.sig360_extractor.detectedLines = &detectedLines;
  return &report;
}
