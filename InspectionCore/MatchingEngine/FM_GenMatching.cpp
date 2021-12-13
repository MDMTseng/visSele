#include "FM_GenMatching.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>
// #include <acvImage_.hpp>
#include <rotCaliper.h>
#include <UTIL.hpp>

#include <algorithm>


// using namespace cv;

// Mat bg = Mat(800, 800, CV_8UC3, {0, 0, 0});

FM_GenMatching::FM_GenMatching(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
  reload(json_str);
  backGroundTemplate.ReSize(1,1);
}

FM_GenMatching::~FM_GenMatching()
{
  ClearReport();
}
void FM_GenMatching::ClearReport()
{
  if(report.data.cjson_report.cjson!=NULL)
  {
    
    cJSON_Delete(report.data.cjson_report.cjson);

    report.data.cjson_report.cjson=NULL;
  }

  report.type=FeatureReport::cjson;



}

int FM_GenMatching::parse_jobj()
{
  LOGI(">>>parse_jobj>>>");
  SetParam(root);

  return 0;
}


int FM_GenMatching::reload(const char *json_str)
{
  LOGI(">>>reload>>>");
  report.data.cjson_report.cjson=NULL;


  root = cJSON_Parse(json_str);
  // if (root == NULL)
  // {
  //   LOGE("cJSON parse failed");
  //   return -1;
  // }
  // int ret_err = parse_jobj();
  // if (ret_err != 0)
  // {
  //   featureCircleList.resize(0);
  //   featureLineList.resize(0);
  //   feature_signature.RESET(0);
  //   reload("");
  //   return -2;
  // }
  parse_jobj();

  // cJSON_Delete(root);

  return 0;
}

#define SETSPARAM_NUMBER(json,structVarAssign,pName) {double *tmpN; if((tmpN=JFetch_NUMBER(json,pName))) structVarAssign*tmpN;}

#define RETSPARAM_NUMBER(json,structVar,pName) {cJSON_AddNumberToObject(json, pName, structVar);}


#define SETSPARAM_INT_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=(int),pName)
#define SETSPARAM_DOU_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=,pName)
#define SETPARAM_INT_NUMBER(json,pName) SETSPARAM_INT_NUMBER(json,this->pName,#pName)
#define SETPARAM_DOU_NUMBER(json,pName) SETSPARAM_DOU_NUMBER(json,this->pName,#pName)
#define RETPARAM_NUMBER(json,pName) RETSPARAM_NUMBER(json,this->pName,#pName)
  



static acv_XY readXY(cJSON *jsonParam)
{
  
  acv_XY xy={NAN,NAN};
  double* num=JFetch_NUMBER(jsonParam,"x");
  if(num!=NULL)xy.X=*num;
  else return xy;


  num=JFetch_NUMBER(jsonParam,"y");
  if(num!=NULL)xy.Y=*num;
  else{
    xy.X=xy.Y=NAN;
    return xy;
  }  
  return xy;
}

static double JFetch_NUMBER_V(cJSON *json, char* path)
{
  double* p_n=JFetch_NUMBER(json,path);
  if(p_n==NULL)return NAN;
  return *p_n;
}

cJSON * FM_GenMatching::SetParam0(cJSON *jsonParam)
{
  // SETPARAM_INT_NUMBER(jsonParam,inspectionStage);
  
  // SETPARAM_INT_NUMBER(jsonParam,HFrom);
  // SETPARAM_INT_NUMBER(jsonParam,HTo);
  // SETPARAM_INT_NUMBER(jsonParam,SMax);
  // SETPARAM_INT_NUMBER(jsonParam,SMin);
  // SETPARAM_INT_NUMBER(jsonParam,VMax);
  // SETPARAM_INT_NUMBER(jsonParam,VMin);



  // SETPARAM_INT_NUMBER(jsonParam,boxFilter1_Size);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter1_thres);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter2_Size);
  // SETPARAM_INT_NUMBER(jsonParam,boxFilter2_thres);



  // SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatio);
  // SETPARAM_DOU_NUMBER(jsonParam,minHeadArea);
  // SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatioMargin);
  // SETPARAM_DOU_NUMBER(jsonParam,FacingThreshold);
  

  // SETPARAM_DOU_NUMBER(jsonParam,cableSeachingRatio);
  // SETPARAM_INT_NUMBER(jsonParam,cableCount);
  // SETPARAM_INT_NUMBER(jsonParam,cableTableCount);

  // int vType = getDataFromJson(jsonParam, "backgroundFlag", NULL);
  // if(vType==cJSON_True)
  // {
  //   backgroundFlag=true;
  // }
  // else if(vType==cJSON_False)
  // {
  //   backgroundFlag=false;
  // }

  // acv_XY vec_btm =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_btm")); 
  // acv_XY vec_side =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_side")); 
  // acv_XY pt_cornor =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.pt_cornor")); 


  // regionInfo.resize(0);
  // if(!isnan(vec_btm.X) && !isnan(vec_side.X) && !isnan(pt_cornor.X))
  // {
  //   LOGI("GO................");
    
  //   float L_btm=hypot(vec_btm.Y,vec_btm.X);
  //   for(int i=0;;i++)
  //   {
  //     char tmpPath[50];
  //     sprintf(tmpPath,"regionInfo.regions[%d]",i);
  //     cJSON *region = JFetch_OBJECT(jsonParam,tmpPath);
  //     if(region==NULL)break;
  //     acv_XY pt1 =readXY(JFetch_OBJECT(region,"pt1")); 
  //     acv_XY pt2 =readXY(JFetch_OBJECT(region,"pt2")); 
  //     double* p_margin = JFetch_NUMBER(region,"margin");
  //     double* p_id = JFetch_NUMBER(region,"id");
  //     double margin=p_margin==NULL?NAN:*p_margin;
  //     int id=p_id==NULL?-1:(int)*p_id;
  //     regionInfo_single rIs;
  //     rIs.normalized_pt1=rIs.pt1=pt1;
  //     rIs.normalized_pt1.X/=L_btm;
  //     rIs.normalized_pt1.Y/=L_btm;

  //     rIs.normalized_pt2=rIs.pt2=pt2;
  //     rIs.normalized_pt2.X/=L_btm;
  //     rIs.normalized_pt2.Y/=L_btm;
  //     rIs.margin=margin;
  //     rIs.id=id;

  //     rIs.RGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[0]");
  //     rIs.RGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[1]");
  //     rIs.RGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[2]");
  //     rIs.sgRGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[0]");
  //     rIs.sgRGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[1]");
  //     rIs.sgRGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[2]");


  //     // regionInfo.push_back(rIs);
  //     // LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,pt1.X,pt1.Y,pt2.X,pt2.Y);
  //     LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,rIs.normalized_pt1.X,rIs.normalized_pt1.Y,rIs.normalized_pt2.X,rIs.normalized_pt2.Y);
  //   }


  // }


  // cJSON* ret_jobj = NULL;
  // if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  // {
  //   ret_jobj = cJSON_CreateObject();
  //   RETPARAM_NUMBER(ret_jobj,HFrom);
  //   RETPARAM_NUMBER(ret_jobj,HTo);
  //   RETPARAM_NUMBER(ret_jobj,SMax);
  //   RETPARAM_NUMBER(ret_jobj,SMin);
  //   RETPARAM_NUMBER(ret_jobj,VMax);
  //   RETPARAM_NUMBER(ret_jobj,VMin);

  //   RETPARAM_NUMBER(ret_jobj,boxFilter1_Size);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter1_thres);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter2_Size);
  //   RETPARAM_NUMBER(ret_jobj,boxFilter2_thres);

  //   RETPARAM_NUMBER(ret_jobj,targetHeadWHRatio);
  //   RETPARAM_NUMBER(ret_jobj,minHeadArea);
  //   RETPARAM_NUMBER(ret_jobj,targetHeadWHRatioMargin);
  //   RETPARAM_NUMBER(ret_jobj,FacingThreshold);

  //   RETPARAM_NUMBER(ret_jobj,cableSeachingRatio);
  //   RETPARAM_NUMBER(ret_jobj,cableCount);
  //   RETPARAM_NUMBER(ret_jobj,cableTableCount);
  // }
  // // float cableSeachingRatio=0.2;


  // const int cableCount=12;
  return NULL;
  // const int cableTableCount=2;
}

cJSON * FM_GenMatching::SetParam1(cJSON *jsonParam)
{
  
  // SETSPARAM_INT_NUMBER(jsonParam,insp02.inspectionType,"inspectionType");
  // SETSPARAM_NUMBER(jsonParam,insp02.pos.X=(int),"pos.X");
  // SETSPARAM_NUMBER(jsonParam,insp02.pos.Y=(int),"pos.Y");


  cJSON* ret_jobj = NULL;
  if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  {
    ret_jobj = cJSON_CreateObject();
  }
  return ret_jobj;
}



void loadTempPy(line2Dup::TemplatePyramid *dstTP,cJSON *TP_info)
{
  // temp_tp.resize();
  int pyLevels = cJSON_GetArraySize(TP_info);

  line2Dup::TemplatePyramid &_dstTP=*dstTP;
  _dstTP.resize(pyLevels);

  
  for (int i = 0; i < pyLevels; i++)
  {
    cJSON *_template = cJSON_GetArrayItem(TP_info, i);
    SETSPARAM_NUMBER(_template,_dstTP[i].tl_x=(int),"x");
    SETSPARAM_NUMBER(_template,_dstTP[i].tl_y=(int),"y");
    SETSPARAM_NUMBER(_template,_dstTP[i].width=(int),"w");
    SETSPARAM_NUMBER(_template,_dstTP[i].height=(int),"h");
    SETSPARAM_NUMBER(_template,_dstTP[i].pyramid_level=(int),"pyramid_level");
    SETSPARAM_NUMBER(_template,_dstTP[i].angle=(int),"angle");


    LOGI("idx:%d  xy:%d,%d, wh:%d,%d pyLevel:%d angle:%f",i,_dstTP[i].tl_x,_dstTP[i].tl_y,_dstTP[i].width,_dstTP[i].height,_dstTP[i].pyramid_level,_dstTP[i].angle);

    cJSON *features = cJSON_GetObjectItem(_template, "features");


    int featureCount = cJSON_GetArraySize(features);
    vector<line2Dup::Feature>  &dstFeatures=_dstTP[i].features;
    dstFeatures.resize(featureCount);

    for (int j = 0; j < featureCount; j++)
    {
      cJSON *feature = cJSON_GetArrayItem(features, j);
      // dstFeatures[j].label=j;
      SETSPARAM_NUMBER(feature,dstFeatures[j].label=(int),"label");
      SETSPARAM_NUMBER(feature,dstFeatures[j].theta=(float),"theta");

      SETSPARAM_NUMBER(feature,dstFeatures[j].x=(int),"x");
      SETSPARAM_NUMBER(feature,dstFeatures[j].y=(int),"y");


      LOGI("==[%d]  xy:%d,%d, theta:%f label:%d",
        j,
        dstFeatures[j].x,dstFeatures[j].y,
        dstFeatures[j].theta,
        dstFeatures[j].label);
    }

  }


  


}



cJSON * dumpTempPy(line2Dup::TemplatePyramid *TP,cv::Point2f anchorPt)
{

  cJSON *TemplatePyramid=cJSON_CreateArray();

  line2Dup::TemplatePyramid &tp=*TP;
  for(int i=0;i<tp.size();i++)
  {

    cJSON *Template=cJSON_CreateObject();
    LOGI("[%d] tl:%d,%d  wh:%d,%d ===",i,tp[i].tl_x,tp[i].tl_y,tp[i].width,tp[i].height);


    cJSON_AddNumberToObject(Template, "x", tp[i].tl_x);
    cJSON_AddNumberToObject(Template, "y", tp[i].tl_y);
    cJSON_AddNumberToObject(Template, "w", tp[i].width);
    cJSON_AddNumberToObject(Template, "h", tp[i].height);

    cJSON_AddNumberToObject(Template, "pyramid_level", tp[i].pyramid_level);
    cJSON_AddNumberToObject(Template, "angle", tp[i].angle);

    {
      cJSON *anchor=cJSON_CreateObject();
      cJSON_AddNumberToObject(anchor, "x", anchorPt.x/(1<<i));
      cJSON_AddNumberToObject(anchor, "y", anchorPt.y/(1<<i));
      cJSON_AddItemToObject(Template,"anchor", anchor);
    }

    cJSON *features=cJSON_CreateArray();
    for (auto& f : tp[i].features)
    {

      cJSON *feature=cJSON_CreateObject();

      // LOGI("label:%d xy:%d,%d   theta:%f",f.label,f.x,f.y,f.theta);

      cJSON_AddNumberToObject(feature, "label", f.label);
      cJSON_AddNumberToObject(feature, "theta", f.theta);
      cJSON_AddNumberToObject(feature, "x",f.x);
      cJSON_AddNumberToObject(feature, "y",f.y);



      cJSON_AddItemToArray(features, feature);
    }
    cJSON_AddItemToObject(Template,"features", features);

    cJSON_AddItemToArray(TemplatePyramid, Template);
  }
  return TemplatePyramid;

}

void FM_GenMatching::resetSBMIF()
{
  if(p_sbmif)delete p_sbmif;
  p_sbmif=new SBM_if(60, {4,8},30,80);
}

SBM_if *FM_GenMatching::getSBMIF()
{
  if(p_sbmif==NULL)resetSBMIF();
  return p_sbmif;
}

cJSON * FM_GenMatching::SetParam(cJSON *jsonParam)
{
  char* jsonStr = cJSON_Print(jsonParam);
  LOGI("%s..",jsonStr);
  delete jsonStr;
  // exit(-1);
  // double *tmpN;
  // if((tmpN=JFetch_NUMBER(jsonParam,"HFrom")))
  //   HFrom=(int)*tmpN;

  


  char* itype = JFetch_STRING(jsonParam,"insp_type");
  if(itype==NULL)
  {
    return NULL;
  }

  strcpy(inspType,itype);

  if (strcmp(inspType, "image_binarization") == 0)
  {    
    double *tmpN=JFetch_NUMBER(jsonParam,"thres");
    if(tmpN!=NULL)
    {
      thres=(int)*tmpN;
    }
    else
    {
      thres=-1;
    }
  }
  else if (strcmp(inspType, "extract_shape_features") == 0)
  {

    SETSPARAM_NUMBER(jsonParam,ROI[0]=(int),"ROI.x");
    SETSPARAM_NUMBER(jsonParam,ROI[1]=(int),"ROI.y");
    SETSPARAM_NUMBER(jsonParam,ROI[2]=(int),"ROI.w");
    SETSPARAM_NUMBER(jsonParam,ROI[3]=(int),"ROI.h");



    cv::Point2f Anchor((ROI[0]+ROI[2])/2,(ROI[1]+ROI[3])/2);
    SETSPARAM_NUMBER(jsonParam,Anchor.x=(float),"anchor.x");
    SETSPARAM_NUMBER(jsonParam,Anchor.y=(float),"anchor.y");
    temp_tp_anchorPt=Anchor;
    LOGI("%d %d %d %d",ROI[0],ROI[1],ROI[2],ROI[3]);

  }
  else if(strcmp(inspType, "shape_features_matching") == 0)
  {
    
    cJSON *template_pyramid = cJSON_GetObjectItem(jsonParam, "template_pyramid");
    line2Dup::TemplatePyramid temp_tp;
    loadTempPy(&temp_tp,template_pyramid);

    line2Dup::TemplatePyramid &tp=temp_tp;
    resetSBMIF();
    SBM_if &sbmif=*getSBMIF();
    // LOGI("=====temp_tp.size():%d",temp_tp.size());
    // if(temp_tp.size()<=0)
    // {
    //   return -1;
    // }
    downScale=0.5;
    templateBaseName="AAA";
    sbmif.train(templateBaseName,tp,cv::Point2f(0,0),false,downScale,0,360,360);
    sbmif.train(templateBaseName+"_f",tp,cv::Point2f(0,0),true,downScale,0,360,360);

      
    cv::Point2f cenPos(0,0);//=(cv::Point2f){ (float)(tp[0].tl_x+tp[0].width/2)      , (float)(tp[0].tl_y+tp[0].height/2)};

    SETSPARAM_NUMBER(jsonParam,cenPos.x=(float),"template_pyramid[0].anchor.x");
    SETSPARAM_NUMBER(jsonParam,cenPos.y=(float),"template_pyramid[0].anchor.y");


    cv::Point2f f0Pos (tp[0].tl_x+tp[0].features[0].x,tp[0].tl_y+tp[0].features[0].y);
    
    cv::Point2f cenOffset=(cenPos-f0Pos);


    sbmif.regTemplateOffset(templateBaseName     ,{cenOffset,false});
    cenOffset.y*=-1;
    sbmif.regTemplateOffset(templateBaseName+"_f",{cenOffset,true});




    {
      


      // for(int i=0;i<temp_tp.size();i++)
      // {
      //   temp_tp[i].tl_x=0;
      //   temp_tp[i].tl_y=0;
      // }

    }
    // line2Dup::TemplatePyramid tp;
  }

  return NULL;
}


cv::Point2f rotate2d(const cv::Point2f& inPoint, const double& angRad)
{
    cv::Point2f outPoint;
    //CW rotation
    outPoint.x = std::cos(angRad)*inPoint.x - std::sin(angRad)*inPoint.y;
    outPoint.y = std::sin(angRad)*inPoint.x + std::cos(angRad)*inPoint.y;
    return outPoint;
}

cv::Point2f rotatePoint(const cv::Point2f& inPoint, const cv::Point2f& center, const double& angRad)
{
    return rotate2d(inPoint - center, angRad) + center;
}



int FM_GenMatching::FeatureMatching(acvImage *img)
{

  ClearReport();
  cJSON *jsonRep=cJSON_CreateObject();
  
  cJSON_AddStringToObject(jsonRep, "type", GetFeatureTypeName());
  ClearReport();
  report.data.cjson_report.cjson=jsonRep;
  // LOGI("GOGOGOGOGOGGO....inspectionStage:%d",inspectionStage);

  Mat cv_img(img->GetHeight(),img->GetWidth(),CV_8UC3,img->CVector[0]);


  cJSON_AddStringToObject(jsonRep, "insp_type", inspType);
  if (strcmp(inspType, "image_binarization") == 0)
  {    
    
    if(thres>=0)
    {
      acvThreshold(img,thres);
    }
  }


  
  else if (strcmp(inspType, "extract_shape_features") == 0)
  {

    int X= ROI[0];
    int Y= ROI[1];
    int W=ROI[2];
    int H=ROI[3];

    // X=1087-100;
    // Y=231+100;
    // W=1596-1087;
    // H=666-231;

    {
      cJSON *ROI=cJSON_CreateObject();
      cJSON_AddItemToObject(jsonRep, "ROI", ROI);
      
      cJSON_AddNumberToObject(ROI, "x", X);
      cJSON_AddNumberToObject(ROI, "y", Y);
      cJSON_AddNumberToObject(ROI, "w", W);
      cJSON_AddNumberToObject(ROI, "h", H);
    }




    LOGI("X:%d  Y:%d W:%d H:%d",X,Y,W,H);
    line2Dup::TemplatePyramid tp;
    {
      
      Mat cv_mask = Mat(cv_img.size(), CV_8UC1);
      cv_mask = cv::Scalar::all(0);
      {
        cv::Rect rect(X,Y,W,H);
      // LOGI("=====");
        cv::rectangle(cv_mask, rect, cv::Scalar::all(255), cv::FILLED);
        // cv_mask(cv::Rect(X,Y,W,H))=1;
      }
      // imwrite("cv_mask.jpg", cv_mask);

      // exit(-1);
      // blur( img, img, Size(3, 3));
      // sbmif.train(cv_img,1);


      getSBMIF()->TemplateFeatureExtraction(cv_img,cv_mask,50,tp);
      // auto center = cv::Point2f(cv_img.cols*scaleN,cv_img.rows*scaleN);
      LOGI("=====");
      
      cJSON_AddItemToObject(jsonRep, "TemplatePyramid", dumpTempPy(&tp,temp_tp_anchorPt));

    }
    LOGI("%d %d %d %d",ROI[0],ROI[1],ROI[2],ROI[3]);


    cv::Rect rect(X,Y,W,H);

  }

  else if (strcmp(inspType, "shape_features_matching") == 0)
  {



    {

      // cv::Point2f ptc={(float)(temp_tp[0].width/2),(float)(temp_tp[0].height/2)};


      LOGI("=====");
      cv::Size size1 = cv_img.size();
      size1.width=((int)(size1.width*downScale))/8*8;
      size1.height=((int)(size1.height*downScale))/8*8;


      LOGI("=====");
      Mat matching_img;
      // matching_img= cv_img(cv::Rect(0,0,size1.width,size1.height));
      resize(cv_img,matching_img,size1,cv::INTER_AREA);//resize image

      // imwrite("matching_img.jpg", matching_img);

      LOGI("=====");
      std::vector<line2Dup::Match> matches = getSBMIF()->detector.match(matching_img, 85,100, {templateBaseName,templateBaseName+"_f"});
      

      LOGI("==matches.size():%d===",matches.size());

      vector<Rect> boxes;
      vector<float> scores;
      vector<int> idxs;
      int CCC=0;
      for(auto match: matches){
        Rect box;
        box.x = match.x;
        box.y = match.y;
        
        auto templ = getSBMIF()->detector.getTemplates(match.class_id,
                                            match.template_id);

        box.width = templ[0].width;
        box.height = templ[0].height;
        boxes.push_back(box);
        scores.push_back(match.similarity);
        CCC++;
      }
      LOGI("=====idxs.size():%d,%d",idxs.size(),matches.size());
      cv_dnn::NMSBoxes(boxes, scores, 0, 0.5f, idxs);

      LOGI("=====idxs.size():%d",idxs.size());


      cJSON *jmatchs=cJSON_CreateArray();
      for(auto idx: idxs){
        line2Dup::Match &match = matches[idx];
        auto templ = getSBMIF()->detector.getTemplates(match.class_id,
                                            match.template_id);

        int x =  templ[0].width/2 + match.x;
        int y = templ[0].height/2 + match.y;

        // LOGI("xy:%d,%d  wh:%d,%d angle:%f  sim:%f",x,y,templ[0].width,templ[0].height,templ[0].angle, match.similarity );

        cv::Point2f f0Pt = cv::Point2f((float)templ[0].features[0].x+match.x,(float)templ[0].features[0].y+match.y)/downScale;
        SBM_if::anchorInfo Aoffset = getSBMIF()->fetchTemplateOffset(match.class_id);
        cv::Point2f anchorPt = rotate2d(Aoffset.offset ,templ[0].angle*M_PI/180);
        anchorPt+=f0Pt;

        cJSON *jmatch=cJSON_CreateObject();

        cJSON_AddNumberToObject(jmatch, "similarity", match.similarity);
        cJSON_AddNumberToObject(jmatch, "x", anchorPt.x);
        cJSON_AddNumberToObject(jmatch, "y", anchorPt.y);

        cJSON_AddNumberToObject(jmatch, "angle", templ[0].angle);


        cJSON_AddNumberToObject(jmatch, "tl_x", templ[0].tl_x/downScale);
        cJSON_AddNumberToObject(jmatch, "tl_y", templ[0].tl_y/downScale);
        cJSON_AddNumberToObject(jmatch, "w", templ[0].width/downScale);
        cJSON_AddNumberToObject(jmatch, "h", templ[0].height/downScale);
        cJSON_AddStringToObject(jmatch, "class_id", match.class_id.c_str());

        cJSON_AddItemToArray(jmatchs, jmatch);





      }

      

      cJSON_AddItemToObject(jsonRep, "matchResults", jmatchs);


    }










  }





  return -1;
}

int FM_GenMatching::FeatureMatching1(acvImage *img)
{

  cJSON *jsonRep=report.data.cjson_report.cjson;

  return -1;
}

int FM_GenMatching::FeatureMatching0(acvImage *img)
{
  
  float cableRatio=0.074;
  float maxDiffMargin=24;


  // inspectionStage=1;

  return 0;
}
