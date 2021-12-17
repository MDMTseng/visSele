#include "FM_GenMatching.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>
#include <opencv2/calib3d.hpp>
// #include <acvImage_.hpp>
#include <rotCaliper.h>
#include <UTIL.hpp>

#include <TemplateMatching_SubPix.h>
#include <algorithm>


// using namespace cv;

// Mat bg = Mat(800, 800, CV_8UC3, {0, 0, 0});

FM_GenMatching::FM_GenMatching(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
  reload(json_str);
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

  int inspSetRet=SetInspInfo(jsonParam);
  char* param_type = JFetch_STRING(jsonParam,"param_type");

  if(param_type==NULL)return NULL;
  
  if (strcmp(param_type, "load_template_image") == 0)
  {   
    char* image_src_path = JFetch_STRING(jsonParam,"image_src_path");
    if(image_src_path!=NULL)
    {
      LOGE("image_src_path:%s", image_src_path);
      int ret = LoadIMGFile(&DefTemplate, image_src_path);
      if (ret)
      {
        // LOGE("LoadBMP failed: ret:%d", ret);
        return NULL;
      }
    }
  }


}



int FM_GenMatching::SetInspInfo(cJSON *jsonParam)
{

  char* itype = JFetch_STRING(jsonParam,"insp_type");
  if(itype==NULL)
  {
    return -1;
  }

  strcpy(inspType,itype);

  if (strcmp(inspType, "image_binarization") == 0)
  {    

    thres=-1;
    SETSPARAM_NUMBER(jsonParam,thres=(int),"thres");
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

  else if (strcmp(inspType, "extract_local_pixels") == 0){


    SETSPARAM_NUMBER(jsonParam,ROI[0]=(int),"ROI.x");
    SETSPARAM_NUMBER(jsonParam,ROI[1]=(int),"ROI.y");
    SETSPARAM_NUMBER(jsonParam,ROI[2]=(int),"ROI.w");
    SETSPARAM_NUMBER(jsonParam,ROI[3]=(int),"ROI.h");
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

    // thres=-1;
    // SETSPARAM_NUMBER(jsonParam,thres=(int),"thres");

    cv::Point2f f0Pos (tp[0].tl_x+tp[0].features[0].x,tp[0].tl_y+tp[0].features[0].y);
    
    cv::Point2f cenOffset=(cenPos-f0Pos);


    sbmif.regTemplateOffset(templateBaseName     ,{cenOffset,false});
    cenOffset.y*=-1;
    sbmif.regTemplateOffset(templateBaseName+"_f",{cenOffset,true});

    locatingBlockImg.resize(0);
    locatingBlocks.resize(0);
    cJSON *blocks = cJSON_GetObjectItem(jsonParam, "locatingBlocks");
    if(blocks!=NULL)
    {
      
      int blockSize = cJSON_GetArraySize(blocks);
      for(int i=0;i<blockSize;i++)
      {
        cJSON *block = cJSON_GetArrayItem(blocks, i);
        FM_GenMatching::region reg;

        SETSPARAM_NUMBER(block,reg.x=(int),"x");
        SETSPARAM_NUMBER(block,reg.y=(int),"y");
        SETSPARAM_NUMBER(block,reg.w=(int),"w");
        SETSPARAM_NUMBER(block,reg.h=(int),"h");
        reg.rel_x=reg.x-cenPos.x;//+reg.w/2;;
        reg.rel_y=reg.y-cenPos.y;//+reg.h/2;
        locatingBlocks.push_back(reg);
      }

    }



  }

  return 0;
}


cv::Point2f rotate2d(const cv::Point2f& inPoint, const double angRad)
{
    cv::Point2f outPoint;
    //CW rotation
    outPoint.x = std::cos(angRad)*inPoint.x - std::sin(angRad)*inPoint.y;
    outPoint.y = std::sin(angRad)*inPoint.x + std::cos(angRad)*inPoint.y;
    return outPoint;
}

cv::Point2f rotatePoint(const cv::Point2f& inPoint, const cv::Point2f& center, const double angRad)
{
    return rotate2d(inPoint - center, angRad) + center;
}




cv::Mat rotCrop(cv::Mat& srcImg,float obj_x,float obj_y,int temp_rel_x,int temp_rel_y,int temp_w,int temp_h, float angRad,int margin=5, cv::Point2f *ret_center=NULL)
{
  temp_rel_x-=margin;
  temp_rel_y-=margin;
  temp_w+=margin*2;
  temp_h+=margin*2;


  Point2f objPos = Point2f( obj_x, obj_y );
  Point2f temp_rel = Point2f( temp_rel_x, temp_rel_y );
  Point2f srcTri[3];      //point 2f object for input file
  srcTri[0] = Point2f( 0.f, 0.f );
  srcTri[1] = Point2f( 0  , temp_w);        //Before transformation selecting points
  srcTri[2] = Point2f( temp_h  , 0);

  for(int i=0;i<3;i++)
  {
    srcTri[i]=rotate2d(srcTri[i]+temp_rel,angRad)+objPos;
  }

  if(ret_center!=NULL)
  {
    *ret_center=(srcTri[1]+srcTri[2])/2;
  }


  Point2f dstTri[3];      //point 2f object for destination file
  dstTri[0] = Point2f( 0.f, 0.f );
  dstTri[1] = Point2f( 0  , temp_w   );        //Before transformation selecting points
  dstTri[2] = Point2f( temp_h  , 0   );
  Mat warp_mat = getAffineTransform( srcTri, dstTri );  //apply an affine transforation to image and storing it
  Mat warp_dst = Mat::zeros( temp_h, temp_w, srcImg.type() );
  warpAffine( srcImg, warp_dst, warp_mat, warp_dst.size() );     


  return warp_dst;
}


bool sort_dec(float a, float b) {
    return a > b; //decreasing => 10,9,5,2,1
}

bool sort_inc(float a, float b) {
    return a < b; //decreasing => 10,9,5,2,1
}


float PoseRefine(cv::Mat &srcImg,std::vector<FM_GenMatching::region> &regions,std::vector<cv::Mat> &regionTempImgs,float marginFactor,Point2f &anchorPt,float &angleRad,float minAcceptedScore=0.2,int *ret_acceptedRegionCount=NULL)
{

  if(ret_acceptedRegionCount)*ret_acceptedRegionCount=0;
  LOGI("====anchorPt:%f %f======",anchorPt.x,anchorPt.y);

  vector<Point2f> initPts;
  vector<Point2f> updatedPts;
  vector<float> matchingPoints;
  float minAccScore=1;
  for(int i=0;i<regions.size();i++)
  {
    
    FM_GenMatching::region reg = regions[i];
    int margin=(int)(marginFactor);
    cv::Point2f crop_center;
    cv::Mat mat = rotCrop(
      srcImg,
      anchorPt.x,
      anchorPt.y,
      reg.rel_x,
      reg.rel_y,
      reg.w,
      reg.h, 
      angleRad,
      margin,
      &crop_center);


    Mat result;
    bool isResForMax;
    Point2f levelXPt = TemplateMatching_SubPix(mat,regionTempImgs[i],result,isResForMax,TM_CCOEFF_NORMED);
    float matchResult = result.at<float>((int)round(levelXPt.x), (int)round(levelXPt.y));


    // if(true){//export template diff image for debugging
    //   string path_p="data/TMP/"+std::to_string(count);
    //   Mat mat_ROI=mat(cv::Rect(round(levelXPt.x),round(levelXPt.y),regionTempImgs[i].cols, regionTempImgs[i].rows));
    //   // locatingBlockImg[i].copyTo(mat_ROI);

    //   float diffAmp=2;
    //   addWeighted( regionTempImgs[i], -0.5*diffAmp, mat_ROI, 0.5*diffAmp, 128.0, mat_ROI);
    //   imwrite(path_p+"mat_final"+std::to_string(i)+".png", mat);
    // }


    float offsetThres=margin-3;
    levelXPt.x-=margin;
    levelXPt.y-=margin;
    LOGI("matchResult:%f offset:%f,%f",matchResult,levelXPt.x,levelXPt.y);
    if(matchResult!=matchResult || matchResult<minAcceptedScore ||levelXPt.x<-offsetThres || levelXPt.x>offsetThres || levelXPt.y<-offsetThres || levelXPt.y>offsetThres)
    {
      levelXPt.x=levelXPt.y=NAN;
    }
    else
    {
      levelXPt=rotate2d(levelXPt,angleRad);
      initPts.push_back(crop_center);
      updatedPts.push_back(crop_center+levelXPt);
      if(minAccScore>matchResult)
      {
        minAccScore=matchResult;
      }

    }


    // printf("matchLoc_subPix:%f %f\n",levelXPt.x,levelXPt.y);
    // small_image.copyTo(big_image(cv::Rect(x,y,small_image.cols, small_image.rows)));

    // Mat cimg=def_temp_img(cv::Rect(block.x,block.y,block.w,block.h));
    

    // LOGI("block relXY:%f %f",block.rel_x,block.rel_y);
    // locatingBlockImg.push_back(cimg);


  }
  if(updatedPts.size()>=3 )
  {
    cv::Mat R = estimateAffine2D(initPts,updatedPts);

    if(R.cols != 0)
    {
      std::vector<cv::Point2f> inputV;
      inputV.push_back(anchorPt);
      inputV.push_back((cv::Point2f){anchorPt.x+1,anchorPt.y});//get offset
      // inputV.push_back((cv::Point2f){1,0});//get rotation

      std::vector<cv::Point2f> outputV;

      transform(inputV,outputV, R);

      Point2f new_anchorPt = outputV[0];
      LOGI("a:%f,%f  na:%f,%f",anchorPt.x,anchorPt.y,new_anchorPt.x,new_anchorPt.y);
      anchorPt=new_anchorPt;

      Point2f rotation = outputV[1]-new_anchorPt;
      float angle=atan2(rotation.y,rotation.x);
      LOGI("theta:%f",angle*180/M_PI);
      angleRad+=angle;

      if(ret_acceptedRegionCount)*ret_acceptedRegionCount=updatedPts.size();
      return minAccScore;
    }
  }

  if(updatedPts.size()==2 )
  {
    cv::Mat R = estimateAffine2D(initPts,updatedPts);

    if(R.cols != 0)
    {
      std::vector<cv::Point2f> inputV;
      inputV.push_back(anchorPt);
      inputV.push_back((cv::Point2f){anchorPt.x+1,anchorPt.y});//get offset
      // inputV.push_back((cv::Point2f){1,0});//get rotation

      std::vector<cv::Point2f> outputV;

      transform(inputV,outputV, R);

      Point2f new_anchorPt = outputV[0];
      LOGI("a:%f,%f  na:%f,%f",anchorPt.x,anchorPt.y,new_anchorPt.x,new_anchorPt.y);
      anchorPt=new_anchorPt;

      Point2f rotation = outputV[1]-new_anchorPt;
      float angle=atan2(rotation.y,rotation.x);
      LOGI("theta:%f",angle*180/M_PI);
      angleRad+=angle;
      return minAccScore;
    }
  }


  return 0;


  // ofsSum/=ofsCount;
  // anchorPt+=ofsSum;


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
      
      cJSON_AddItemToObject(jsonRep, "template_pyramid", dumpTempPy(&tp,temp_tp_anchorPt));

    }
    LOGI("%d %d %d %d",ROI[0],ROI[1],ROI[2],ROI[3]);


    cv::Rect rect(X,Y,W,H);

  }

  else if (strcmp(inspType, "extract_local_pixels") == 0){


    int X= ROI[0];
    int Y= ROI[1];
    int W=ROI[2];
    int H=ROI[3];




    cJSON_AddNumberToObject(jsonRep, "x", X);
    cJSON_AddNumberToObject(jsonRep, "y", Y);
    cJSON_AddNumberToObject(jsonRep, "w", W);
    cJSON_AddNumberToObject(jsonRep, "h", H);


    cJSON *pixels=cJSON_CreateArray();

    if(Y<0 || X<0 || X+W>img->GetWidth() || Y+H>img->GetHeight())
    {
      //out of bound

      cJSON_AddStringToObject(jsonRep, "error","ROI is out of bound");

    }
    else
    {

      for(int i=Y;i<Y+H;i++)for(int j=X;j<X+W;j++)
      {
        cJSON *n = cJSON_CreateNumber(img->CVector[i][j*3]);
        cJSON_AddItemToArray(pixels, n);
      }
      cJSON_AddItemToObject(jsonRep, "pixels", pixels);
    }





  }
  else if (strcmp(inspType, "shape_features_matching") == 0)
  {

    if(locatingBlockImg.size()!=locatingBlocks.size())
    {
      locatingBlockImg.resize(0);
      if(DefTemplate.GetWidth()*DefTemplate.GetHeight()>10)
      {
        Mat def_temp_img(DefTemplate.GetHeight(),DefTemplate.GetWidth(),CV_8UC3,DefTemplate.CVector[0]);

        for (auto & block : locatingBlocks) {
          Mat cimg;//=def_temp_img(cv::Rect(block.x,block.y,block.w,block.h));//the ROI method might slow down the process
          def_temp_img(cv::Rect(block.x,block.y,block.w,block.h)).copyTo(cimg);
          LOGI("block relXY:%f %f",block.rel_x,block.rel_y);
          locatingBlockImg.push_back(cimg);
        }
      }
      else
      {
      }
      
    }


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
      std::vector<line2Dup::Match> matches = getSBMIF()->detector.match(matching_img, 85,100, {templateBaseName});
      

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
      int count=0;
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


        float refine_score=0;
        int refine_block_count=0;
        {
          int margin=(int)(20/downScale);
          cv::Point2f tmp_anchorPt=anchorPt;
          float angleRad = templ[0].angle*M_PI/180;
          refine_score = PoseRefine(cv_img,locatingBlocks,locatingBlockImg,margin,tmp_anchorPt,angleRad,0.2);
          refine_score = PoseRefine(cv_img,locatingBlocks,locatingBlockImg,margin,tmp_anchorPt,angleRad,0.8,&refine_block_count);

          if(refine_score>0.8)
          {//accept the refinement
            templ[0].angle=angleRad*180/M_PI;
            anchorPt=tmp_anchorPt;
          }
        }
        

        cJSON *jmatch=cJSON_CreateObject();

        cJSON_AddNumberToObject(jmatch, "similarity", match.similarity);
        cJSON_AddNumberToObject(jmatch, "x", anchorPt.x);
        cJSON_AddNumberToObject(jmatch, "y", anchorPt.y);

        cJSON_AddNumberToObject(jmatch, "angle", templ[0].angle);


        cJSON_AddNumberToObject(jmatch, "tl_x", templ[0].tl_x/downScale);
        cJSON_AddNumberToObject(jmatch, "tl_y", templ[0].tl_y/downScale);
        cJSON_AddNumberToObject(jmatch, "w", templ[0].width/downScale);
        cJSON_AddNumberToObject(jmatch, "h", templ[0].height/downScale);

        cJSON_AddNumberToObject(jmatch, "refine_score", refine_score);
        cJSON_AddNumberToObject(jmatch, "refine_block_count", refine_block_count);


        cJSON_AddStringToObject(jmatch, "class_id", match.class_id.c_str());


        cJSON_AddItemToArray(jmatchs, jmatch);
        count++;


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
