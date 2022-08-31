
#include "InspTar_Orientation.hpp"


#include <TemplateMatching_SubPix.h>
using namespace cv;

using namespace std;


template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_Orientation_ShapeBasedMatching::InspectionTarget_Orientation_ShapeBasedMatching(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
  :InspectionTarget(id,NULL,belongMan,local_env_path)
{
  type=InspectionTarget_Orientation_ShapeBasedMatching::TYPE();


  sbm=NULL;
  
  setInspDef(def);
  // sbm=new SBM_if(60, {4,6,12},30,80);
}

bool InspectionTarget_Orientation_ShapeBasedMatching::stageInfoFilter(shared_ptr<StageInfo> sinfo)
{
  for(auto tag : sinfo->trigger_tags )
  {
    // if(tag=="_STREAM_")
    // {
    //   return false;
    // }
    if( matchTriggerTag(tag))
      return true;
  }
  return false;
}

future<int> InspectionTarget_Orientation_ShapeBasedMatching::futureInputStagePool()
{
  return async(launch::async,&InspectionTarget_Orientation_ShapeBasedMatching::processInputStagePool,this);
}

int InspectionTarget_Orientation_ShapeBasedMatching::processInputPool()
{
  int poolSize=input_pool.size();
  for(int i=0;i<poolSize;i++)
  {
    shared_ptr<StageInfo> curInput=input_pool[i];
    singleProcess(curInput);

    input_pool[i]=NULL;
  }
  input_pool.clear();


  return poolSize;//run all

}




line2Dup::Template Json2Template(cJSON* jtpTemp)
{
  line2Dup::Template tmpl;
  tmpl.angle=JFetch_NUMBER_ex(jtpTemp,"angle");
  tmpl.width=JFetch_NUMBER_ex(jtpTemp,"width");
  tmpl.height=JFetch_NUMBER_ex(jtpTemp,"height");
  tmpl.tl_x=JFetch_NUMBER_ex(jtpTemp,"tl_x");
  tmpl.tl_y=JFetch_NUMBER_ex(jtpTemp,"tl_y");
  tmpl.pyramid_level=JFetch_NUMBER_ex(jtpTemp,"pyramid_level");


  // tmpl.features.push_back();
  for(int i=0;;i++)
  {
    cJSON *jfeat=JFetch_OBJECT(jtpTemp,("features["+to_string(i)+"]").c_str());

    if(jfeat==NULL)break;
    line2Dup::Feature feat;
    feat.label=JFetch_NUMBER_ex(jfeat,"label");
    feat.theta=JFetch_NUMBER_ex(jfeat,"theta");
    feat.x=JFetch_NUMBER_ex(jfeat,"x");
    feat.y=JFetch_NUMBER_ex(jfeat,"y");
    tmpl.features.push_back(feat);
  }
  return tmpl;
}


line2Dup::TemplatePyramid Json2TemplatePyramid(cJSON* jtpArr)
{
  line2Dup::TemplatePyramid tp;
  for(int i=0;;i++)
  {
    cJSON *layerTemp=JFetch_OBJECT(jtpArr,("["+to_string(i)+"]").c_str());

    if(layerTemp==NULL)break;
    line2Dup::Template temp=Json2Template(layerTemp);
    tp.push_back(temp);
  }
  return tp;
}

void InspectionTarget_Orientation_ShapeBasedMatching::setInspDef(cJSON* def)
{
  InspectionTarget::setInspDef(def);
// featureInfo
  if(sbm)
  {
    delete(sbm);
    sbm=NULL;
  }
  refine_region_set.clear();
  matching_downScale=JFetch_NUMBER_ex(def,"matching_downScale",1);
  if(matching_downScale<0.01)matching_downScale=0.01;
  cJSON* featureInfo=JFetch_OBJECT(def,"featureInfo");
  if(featureInfo)
  {
    bool match_front_face=JFetch_TRUE(featureInfo,"match_front_face");
    bool match_back_face=JFetch_TRUE(featureInfo,"match_back_face");
    
    int num_features= JFetch_NUMBER_ex(featureInfo,"num_features",60);
    int weak_thresh= JFetch_NUMBER_ex(featureInfo,"weak_thresh",30);
    int strong_thresh= JFetch_NUMBER_ex(featureInfo,"strong_thresh",80);
    vector<int> T;
    for(int i=0;;i++)
    {
      double *t=JFetch_NUMBER(featureInfo,("T["+to_string(i)+"]").c_str());
      if(t)
        T.push_back(*t);
      else
        break;
    }
    if(T.size()==0)//default
    {
      T.push_back(4);
      T.push_back(6);
      T.push_back(12);
    }

    LOGI(">>>%d,%d,%d",num_features,weak_thresh,strong_thresh);
    sbm=new SBM_if(num_features, T,weak_thresh,strong_thresh);


    cJSON *jtemplatePyramid= JFetch_ARRAY(featureInfo,"templatePyramid");
    
    insp_tp=Json2TemplatePyramid(jtemplatePyramid);
    




    int templateCenter_x= JFetch_NUMBER_ex(featureInfo,"center.x",insp_tp[0].tl_x);
    int templateCenter_y= JFetch_NUMBER_ex(featureInfo,"center.y",insp_tp[0].tl_y);

    cv::Point2f f0Pos(insp_tp[0].tl_x+insp_tp[0].features[0].x,insp_tp[0].tl_y+insp_tp[0].features[0].y);
    cv::Point2f cenOffset=cv::Point2f(templateCenter_x,templateCenter_y)-f0Pos;
    sbm->regTemplateOffset(template_class_name     ,{cenOffset,false});
    cenOffset.y*=-1;
    sbm->regTemplateOffset(template_class_name+"_f",{cenOffset,true});

    if(match_front_face)
    {
      float AngleStart=JFetch_NUMBER_ex(featureInfo,"match_front_face_angle_range[0]",-179.999);
      float AngleEnd=JFetch_NUMBER_ex(featureInfo,"match_front_face_angle_range[1]",180);
      int AngleSegs=(int)round(JFetch_NUMBER_ex(featureInfo,"match_front_face_angle_segs",(AngleEnd-AngleStart)));

      sbm->train(template_class_name     ,insp_tp,cv::Point2f(0,0),false,
        matching_downScale,
        AngleStart,
        AngleEnd,
        AngleSegs);
    }

    if(match_back_face)
    {
      float AngleStart=JFetch_NUMBER_ex(featureInfo,"match_back_face_angle_range[0]",-179.999);
      float AngleEnd=JFetch_NUMBER_ex(featureInfo,"match_back_face_angle_range[1]",180);
      int AngleSegs=(int)round(JFetch_NUMBER_ex(featureInfo,"match_back_face_angle_segs",(AngleEnd-AngleStart)));

      sbm->train(template_class_name+"_f",insp_tp,cv::Point2f(0,0),true,
        matching_downScale,
        AngleStart,
        AngleEnd,
        AngleSegs);


    }



    cJSON*refine_match_regions=JFetch_ARRAY(featureInfo,"refine_match_regions");
    if(refine_match_regions)
    {
      string feature_ref_image_path=local_env_path+"/"+JFetch_STRING_ex(featureInfo,"feature_ref_image","FeatureRefImage.png");
      
      Mat img = imread(feature_ref_image_path, IMREAD_COLOR);
      if (img.empty()==false)
      {
    
        int reg_size=cJSON_GetArraySize(refine_match_regions);
        
        for (int i = 0 ; i <reg_size ; i++)
        {
          cJSON * regionInfo = cJSON_GetArrayItem(refine_match_regions, i);
          int x=(int)JFetch_NUMBER_ex(regionInfo, "x");
          int y=(int)JFetch_NUMBER_ex(regionInfo, "y");
          int w=(int)JFetch_NUMBER_ex(regionInfo, "w");
          int h=(int)JFetch_NUMBER_ex(regionInfo, "h");

          struct refine_region_info regInfo;
          regInfo.regionInRef=Rect2d(x,y,w,h);
          regInfo.img=img(regInfo.regionInRef).clone();

          regInfo.regionInRef.x-=templateCenter_x;
          regInfo.regionInRef.y-=templateCenter_y;

          // imwrite(local_env_path+"/"+"ddd"+std::to_string(i)+".jpg", regInfo.img);  

          LOGI("refine reg load:%d,%d,%d,%d",x,y,w,h);
          refine_region_set.push_back(regInfo);
        }
      

      }



      LOGI(">>>>>>>>>>>>>>>>>feature_ref_image_path:%s",feature_ref_image_path.c_str());
    }

    LOGI(">>>>>>>>>>>>>>>>>local_env_path:%s",local_env_path.c_str());


  }



}

cJSON* TemplateFeature2Json(line2Dup::Feature &feat)
{
  cJSON* jfeat=cJSON_CreateObject();
  cJSON_AddNumberToObject(jfeat,"label",feat.label);
  cJSON_AddNumberToObject(jfeat,"theta",feat.theta);
  cJSON_AddNumberToObject(jfeat,"x",feat.x);
  cJSON_AddNumberToObject(jfeat,"y",feat.y);

  return jfeat;
}

cJSON* Template2Json(line2Dup::Template &templ)
{
  cJSON* jtempl=cJSON_CreateObject();
  cJSON_AddNumberToObject(jtempl,"angle",templ.angle);
  cJSON_AddNumberToObject(jtempl,"tl_x",templ.tl_x);
  cJSON_AddNumberToObject(jtempl,"tl_y",templ.tl_y);
  cJSON_AddNumberToObject(jtempl,"height",templ.height);
  cJSON_AddNumberToObject(jtempl,"width",templ.width);
  cJSON_AddNumberToObject(jtempl,"pyramid_level",templ.pyramid_level);

  cJSON* jfeats=cJSON_CreateArray();
  cJSON_AddItemToObject(jtempl,"features",jfeats);
  for(int i=0;i<templ.features.size();i++)
  {
    cJSON_AddItemToArray(jfeats,TemplateFeature2Json(templ.features[i]));
  }
  return jtempl;
}

cJSON* TemplatePyramid2Json(line2Dup::TemplatePyramid &tp)
{
  cJSON* jtpArr=cJSON_CreateArray();
  for(int i=0;i<tp.size();i++)
  {
    cJSON_AddItemToArray(jtpArr,Template2Json(tp[i]));
  }

  return jtpArr;
}




bool InspectionTarget_Orientation_ShapeBasedMatching::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);//apply framework layer exchange
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");

  if(type=="extract_feature")
  {
    string path=JFetch_STRING_ex(info,"image_path");
    if(path=="")
    {
      return NULL;
    }
    Mat img = imread(path, IMREAD_COLOR);
    if (img.empty())
    {
      return NULL;
    }


    // acvImage src_acvImg(img.cols,img.rows,3);
    // for(int i=0;i<img.rows;i++)for(int j=0;j<img.cols;j++)
    // {
    //   src_acvImg.CVector[i][j*3]=0;
    //   src_acvImg.CVector[i][j*3+1]=0;
    //   src_acvImg.CVector[i][j*3+2]=0;
    // }
    int image_transfer_downsampling=(int)JFetch_NUMBER_ex(info,"image_transfer_downsampling",-1);
    if(image_transfer_downsampling>=1)
    {
      acvImage src_acvImg;
      src_acvImg.useExtBuffer((BYTE *)img.data,img.rows*img.cols*3,img.cols,img.rows);
      act.send("IM",id,&src_acvImg,image_transfer_downsampling);
    }

    int num_features= JFetch_NUMBER_ex(info,"num_features",60);

    if(num_features>0)
    {

      Mat _mask;
      cJSON *mask_regions=JFetch_ARRAY(info,"mask_regions");
      if(mask_regions)
      {

        int reg_size=cJSON_GetArraySize(mask_regions);
        if(reg_size==0)
        {
          _mask = Mat(img.size(), CV_8UC1,{255});//default all white region
        }
        else
        {
          _mask = Mat::zeros(img.rows, img.cols, CV_8UC1);//Mat(img.size(), CV_8UC1,{0});//default all black region
          for (int i = 0 ; i <reg_size ; i++)
          {
            cJSON * regionInfo = cJSON_GetArrayItem(mask_regions, i);
            bool isBlackRegion=JFetch_TRUE(regionInfo,"isBlackRegion");


            double x=JFetch_NUMBER_ex(regionInfo, "x");
            double y=JFetch_NUMBER_ex(regionInfo, "y");
            double w=JFetch_NUMBER_ex(regionInfo, "w");
            double h=JFetch_NUMBER_ex(regionInfo, "h");

            LOGI("ROI: %f,%f,%f,%f<<<<",x,y,w,h);
            if(x==x && y==y && w==w && h==h)
            {
              _mask(Rect(x,y,w,h)) = isBlackRegion?0:255;
            }
          }
        }

      }
      else
      {
        _mask = Mat(img.size(), CV_8UC1,{255});//default all white region
      }


      

      
      line2Dup::TemplatePyramid tp;
      //1087,231   1596,666

      { 
        float weak_thresh= JFetch_NUMBER_ex(info,"weak_thresh",30);
        float strong_thresh= JFetch_NUMBER_ex(info,"strong_thresh",80);
        vector<int> T;
        for(int i=0;;i++)
        {
          double *t=JFetch_NUMBER(info,("T["+to_string(i)+"]").c_str());
          if(t)
            T.push_back(*t);
          else
            break;
        }
        if(T.size()==0)//default
        {
          T.push_back(4);
          T.push_back(6);
        }
        SBM_if t_sbm(num_features, T,weak_thresh,strong_thresh);
        t_sbm.TemplateFeatureExtraction(img,_mask,num_features,tp);

      }
     






      cJSON* jtp=TemplatePyramid2Json(tp);

      act.send("RP",id,jtp);
      cJSON_Delete(jtp);jtp=NULL;
    }

    return true;

  }


  return false;
}



cv::Point2f rotate2d(const cv::Point2f& inPoint, const double angRad)
{
    cv::Point2f outPoint;
    //CW rotation
    double c=std::cos(angRad);
    double s=std::sin(angRad);
    outPoint.x = c*inPoint.x - s*inPoint.y;
    outPoint.y = s*inPoint.x + c*inPoint.y;
    return outPoint;
}

cv::Point2f rotatePoint(const cv::Point2f& inPoint, const cv::Point2f& center, const double angRad)
{
    return rotate2d(inPoint - center, angRad) + center;
}


cv::Mat rotCrop(cv::Mat& srcImg,float obj_x,float obj_y,float temp_rel_x,float temp_rel_y,float temp_w,float temp_h, float angRad,int margin=5, cv::Point2f *ret_center=NULL)
{

  // LOGI("temp:: %f,%f,%f,%f  margin:%d  angRad:%f",temp_rel_x,temp_rel_y,temp_w,temp_h,margin,angRad);


  temp_rel_x-=margin;
  temp_rel_y-=margin;
  temp_w+=margin*2;
  temp_h+=margin*2;

  // LOGI(">>temp:: %f,%f,%f,%f",temp_rel_x,temp_rel_y,temp_w,temp_h);


  Point2f objPos = Point2f( obj_x, obj_y );
  Point2f temp_rel = Point2f( temp_rel_x, temp_rel_y );
  Point2f srcTri[3];      //point 2f object for input file
  srcTri[0] = Point2f( 0.f, 0.f );
  srcTri[1] = Point2f( 0  , temp_h);        //Before transformation selecting points
  srcTri[2] = Point2f( temp_w, 0);

  // LOGI("temp_rel     :%f,%f",  temp_rel.x,temp_rel.y);
  // LOGI("objPos       :%f,%f",  objPos.x,objPos.y);
  for(int i=0;i<3;i++)
  {
    // LOGI("srcTri   [%d]:%f,%f",i,srcTri[i].x,srcTri[i].y);
    srcTri[i]+=temp_rel;
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
    srcTri[i]=rotate2d(srcTri[i],angRad);
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
    srcTri[i]+=objPos;
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
  }

  if(ret_center!=NULL)
  {
    *ret_center=(srcTri[1]+srcTri[2])/2;
  }


  Point2f dstTri[3];      //point 2f object for destination file
  dstTri[0] = Point2f( 0.f, 0.f );
  dstTri[1] = Point2f( 0  , temp_h);        //Before transformation selecting points
  dstTri[2] = Point2f( temp_w, 0   );
  Mat warp_mat = getAffineTransform( srcTri, dstTri );  //apply an affine transforation to image and storing it
  Mat warp_dst = Mat::zeros( temp_h, temp_w, srcImg.type() );
  warpAffine( srcImg, warp_dst, warp_mat, warp_dst.size() );     


  return warp_dst;
}


int DBG_C=0;

float PoseRefine(cv::Mat &srcImg,std::vector<InspectionTarget_Orientation_ShapeBasedMatching::refine_region_info> &regions_n_TempImgs,float marginFactor,Point2f &anchorPt,float &angleRad,float minAcceptedScore=0.2,int *ret_acceptedRegionCount=NULL)
{

  if(ret_acceptedRegionCount)*ret_acceptedRegionCount=0;
  // anchorPt.x+=5;//test shift
  // anchorPt.y+=5;
  // angleRad=12.000000*M_PI/180;
  // LOGI("original theta:%f",angleRad*180/M_PI);

  vector<Point2f> initPts;
  vector<Point2f> initPts_temp;
  vector<Point2f> updatedPts;
  vector<float> matchingPoints;
  float minAccScore=1;
  for(int i=0;i<regions_n_TempImgs.size();i++)
  {
    
    auto reg = regions_n_TempImgs[i].regionInRef;
    int margin=(int)(marginFactor);
    cv::Point2f crop_center;

    // LOGI("====anchorPt:%f %f======",anchorPt.x,anchorPt.y);
    // LOGI("====reg:xy:%f,%f  wh:%f,%f======",reg.x,reg.y,reg.width,reg.height);
    cv::Mat mat = rotCrop(
      srcImg,
      anchorPt.x,
      anchorPt.y,
      reg.x,
      reg.y,
      reg.width,
      reg.height, 
      angleRad,
      margin,
      &crop_center);

    Mat result;
    bool isResForMax;
    cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
    equalizeHist( mat, mat );

    Mat temp_gray;
    cv::cvtColor(regions_n_TempImgs[i].img, temp_gray, cv::COLOR_BGR2GRAY);
    equalizeHist( temp_gray, temp_gray );


    Point2f levelXPt = TemplateMatching_SubPix(mat,temp_gray,result,isResForMax,TM_CCOEFF_NORMED);


    float matchResult = result.at<float>((int)round(levelXPt.x), (int)round(levelXPt.y));

    std::string prefix="S"+to_string(DBG_C)+"_";
    if(0)
    {
      // imwrite("data/ZZZ/"+prefix+"OOP_"+std::to_string(i)+".jpg",mat);  

      cv::Mat mat_cp =mat.clone();

      auto rect=Rect(levelXPt.x,levelXPt.y,temp_gray.cols, temp_gray.rows);
      temp_gray.copyTo(mat_cp(rect));
      cv::rectangle(mat_cp, rect, cv::Scalar(255, 255, 255));
      cv::rectangle(mat_cp, Rect(mat_cp.cols/2-1,mat_cp.rows/2-1,2,2), cv::Scalar(200, 200, 200));
      imwrite("data/ZZZ/"+prefix+"_TOP_"+std::to_string(i)+".jpg",mat_cp);  


      // result*=200;
      // imwrite("data/TEMP_"+std::to_string(i)+".jpg", temp_gray);  
      // imwrite("data/ZZZ/"+prefix+"RES_"+std::to_string(i)+".jpg",result);  
    }
    // // if(ret_acceptedRegionCount)//JUST for DBG
    // std::string prefix="S"+to_string(DBG_C)+"_";
    // if(1)
    // {
    //   // imwrite("data/ZZZ/"+prefix+"OOP_"+std::to_string(i)+".jpg",mat);  

    //   cv::Mat mat_cp =mat.clone();

    //   auto rect=Rect(levelXPt.x,levelXPt.y,temp_gray.cols, temp_gray.rows);
    //   temp_gray.copyTo(mat_cp(rect));
    //   cv::rectangle(mat_cp, rect, cv::Scalar(255, 255, 255));
    //   cv::rectangle(mat_cp, Rect(mat_cp.cols/2-1,mat_cp.rows/2-1,2,2), cv::Scalar(200, 200, 200));
    //   imwrite("data/ZZZ/"+prefix+"_TOP_"+std::to_string(i)+".jpg",mat_cp);  


    //   // result*=200;
    //   // imwrite("data/TEMP_"+std::to_string(i)+".jpg", temp_gray);  
    //   // imwrite("data/ZZZ/"+prefix+"RES_"+std::to_string(i)+".jpg",result);  
    // }
    // if(true){//export template diff image for debugging
    //   string path_p="data/TMP/"+std::to_string(count);
    //   Mat mat_ROI=mat(cv::Rect(round(levelXPt.x),round(levelXPt.y),regionTempImgs[i].cols, regionTempImgs[i].rows));
    //   // locatingBlockImg[i].copyTo(mat_ROI);

    //   float diffAmp=2;
    //   addWeighted( regionTempImgs[i], -0.5*diffAmp, mat_ROI, 0.5*diffAmp, 128.0, mat_ROI);
    //   imwrite(path_p+"mat_final"+std::to_string(i)+".png", mat);
    // }


    float offsetThres=margin-1;
    levelXPt.x-=margin;
    levelXPt.y-=margin;
    LOGI("[%d]:matchResult:%f offset:%f,%f",i,matchResult,levelXPt.x,levelXPt.y);
    if(matchResult!=matchResult || matchResult<minAcceptedScore ||levelXPt.x<-offsetThres || levelXPt.x>offsetThres || levelXPt.y<-offsetThres || levelXPt.y>offsetThres)
    {
      levelXPt.x=levelXPt.y=NAN;
    }
    else
    {


      // if(ret_acceptedRegionCount)//JUST for DBG

      // cv::cvtColor(mat, mat, cv::COLOR_GRAY2BGR);
      // mat.copyTo(srcImg(Rect(crop_center.x-mat.cols/2,crop_center.y-mat.rows/2,mat.cols, mat.rows)));

      
      // LOGI("O levelXPt %f,%f",levelXPt.x,levelXPt.y);
      levelXPt=rotate2d(levelXPt,angleRad);
      // LOGI("> levelXPt  %f,%f",levelXPt.x,levelXPt.y);
      // LOGI("initPts     %f,%f",crop_center.x,crop_center.y);
      // LOGI("crop_center %f,%f",crop_center.x+levelXPt.x,crop_center.y+levelXPt.y);

      cv::Point2f updatedPt=crop_center+levelXPt;
      cv::Point2f tempPt=cv::Point2f(reg.x+reg.width/2,reg.y+reg.height/2);
      // LOGI("temp        %f,%f",tempPt.x,tempPt.y);
      // if(i==0)
      // {
      //   updatedPt=cv::Point2f(794,230);
      // }
      // else if(i==1)
      // {
      //   updatedPt=cv::Point2f(799,442);
      // }
      // else if(i==2)
      // {
      //   updatedPt=cv::Point2f(872,443.5);
      // }


      // LOGI("DBG_CENTER %f,%f",updatedPt.x,updatedPt.y);

      initPts_temp.push_back(tempPt);
      initPts.push_back(crop_center);
      updatedPts.push_back(updatedPt);
      // LOGI("x:%f y:%f",crop_center.x+levelXPt.x,crop_center.y+levelXPt.y);

      // regions_n_TempImgs[i].img.copyTo(srcImg(Rect(crop_center.x+levelXPt.x-reg.width/2,crop_center.y+levelXPt.y-reg.height/2,temp_gray.cols, temp_gray.rows)));
      
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
  // exit(0);



  if(true && updatedPts.size()>=3 )
  {
    cv::Mat R = estimateAffine2D(initPts_temp,updatedPts);

    cout << "M = " << endl << " "  << R << endl << endl;



    double v_cos=(R.at<double>(0,0)+R.at<double>(1,1))/2;
    double v_sin=(R.at<double>(1,0)-R.at<double>(0,1))/2;



    // std::vector<cv::Point2f> inputV;
    // std::vector<cv::Point2f> outputV;

    // cv::Point2f pt0={0};
    // for(int i=0;i<regions_n_TempImgs.size();i++)
    // {
    //   pt0.x+=regions_n_TempImgs[i].regionInRef.x;
    //   pt0.y+=regions_n_TempImgs[i].regionInRef.y;
    // }
    // pt0/=(float)regions_n_TempImgs.size();
    // pt0+=anchorPt;

    // inputV.push_back(pt0);
    // inputV.push_back((cv::Point2f){pt0.x+1,pt0.y});//get offset
    // transform(inputV,outputV, R);


    cv::Point2f translate=cv::Point2f(R.at<double>(0,2),R.at<double>(1,2));

    float angle=atan2(v_sin,v_cos);
    LOGI("theta:%f",angle*180/M_PI);
    angleRad=angle;

    anchorPt=translate;
    

    if(ret_acceptedRegionCount)*ret_acceptedRegionCount=updatedPts.size();
    return minAccScore;
  }


  if(false && updatedPts.size()>=3 )
  {
    for(int i=0;i<updatedPts.size();i++)
    {
      LOGI("[%d] %.2f,%.2f => %.2f,%.2f",i,initPts[i].x,initPts[i].y,updatedPts[i].x,updatedPts[i].y);
      // LOGI("crop_center %f,%f",crop_center.x+levelXPt.x,crop_center.y+levelXPt.y);
    }
    cv::Mat R = estimateAffine2D(initPts,updatedPts);

    if(R.cols != 0)
    {
      std::vector<cv::Point2f> inputV;
      std::vector<cv::Point2f> outputV;

      inputV.clear();
      outputV.clear();

      cv::Point2f pt0={0};
      for(int i=0;i<regions_n_TempImgs.size();i++)
      {
        pt0.x+=regions_n_TempImgs[i].regionInRef.x;
        pt0.y+=regions_n_TempImgs[i].regionInRef.y;
      }
      pt0/=(float)regions_n_TempImgs.size();
      pt0+=anchorPt;

      inputV.push_back(pt0);
      inputV.push_back((cv::Point2f){pt0.x+1,pt0.y});//get offset
      // inputV.push_back((cv::Point2f){1,0});//get rotation

      transform(inputV,outputV, R);
      for(int i=0;i<inputV.size();i++)
      {
        LOGI("..[%d] %.2f,%.2f => %.2f,%.2f",i,inputV[i].x,inputV[i].y,outputV[i].x,outputV[i].y);
        // LOGI("crop_center %f,%f",crop_center.x+levelXPt.x,crop_center.y+levelXPt.y);
      }

      // LOGI("a:%f,%f  na:%f,%f",anchorPt.x,anchorPt.y,new_anchorPt.x,new_anchorPt.y);
      

      Point2f rotation = outputV[1]-outputV[0];
      float angle=atan2(rotation.y,rotation.x);
      LOGI("theta:%f",angle*180/M_PI);
      angleRad+=1*angle-0.0;


      {
        LOGI("%f,%f >>>>> %f,%f",inputV[0].x,inputV[0].y,outputV[0].x,outputV[0].y);
        // Point2f new_anchorPt = outputV[0];
        anchorPt+=(outputV[0]-inputV[0]);
      }

      if(ret_acceptedRegionCount)*ret_acceptedRegionCount=updatedPts.size();
      return minAccScore;
    }
  }

  if(false && updatedPts.size()==2 )
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



cJSON* InspectionTarget_Orientation_ShapeBasedMatching::genITIOInfo()
{


  
  cJSON* arr= cJSON_CreateArray();

  {
    cJSON* opt= cJSON_CreateObject();
    cJSON_AddItemToArray(arr,opt);

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "i",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Image::stypeName().c_str() ));
    }

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "o",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Orientation::stypeName().c_str() ));
    }




  }

  return arr;

}




bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}


void InspectionTarget_Orientation_ShapeBasedMatching::singleProcess(shared_ptr<StageInfo> sinfo)
{

  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  int64 t0 = cv::getTickCount();
  cache_stage_info=sinfo;
  LOGI(">>>>>>>>InspectionTarget_Orientation_ShapeBasedMatching>>>>>>>>");
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  
  auto srcImg=sinfo->img;

  LOGI(">>>>>>>>");

  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);

  LOGI(">>>>>>>>");


  cv::Size size1 = CV_srcImg.size();
  size1.width=((int)(size1.width*matching_downScale))/8*8;
  size1.height=((int)(size1.height*matching_downScale))/8*8;

  Mat CV_srcImg_ds(size1,CV_8UC3);
  resize(CV_srcImg,CV_srcImg_ds,size1,cv::INTER_AREA);
  
  cv::cvtColor(CV_srcImg_ds, CV_srcImg_ds, cv::COLOR_BGR2GRAY);
  LOGI(">>>>>>>>");

  float magThres_eq_alpha=0.3;
  float magnitude_thres=JFetch_NUMBER_ex(def,"magnitude_thres",20)/(magThres_eq_alpha+(1-magThres_eq_alpha)*matching_downScale);
  if(magnitude_thres>128)magnitude_thres=128;



  std::vector<line2Dup::Match> matches;
  

  vector<int> idxs;
  {
    bool doMatchFilter=false;


    cJSON * search_regions=JFetch_ARRAY(def,"search_regions");
    float similarity_thres=JFetch_NUMBER_ex(def,"similarity_thres",60);
    if(search_regions && cJSON_GetArraySize(search_regions))
    {
      int arrL=cJSON_GetArraySize(search_regions);
      for(int i=0;i<arrL;i++)
      {

        cJSON * region_info = cJSON_GetArrayItem(search_regions, i);

        int x = (int)(JFetch_NUMBER_ex(region_info,"x",0)*matching_downScale);
        int y = (int)(JFetch_NUMBER_ex(region_info,"y",0)*matching_downScale);
        int w = (int)(JFetch_NUMBER_ex(region_info,"w",0)*matching_downScale);
        int h = (int)(JFetch_NUMBER_ex(region_info,"h",0)*matching_downScale);
        
        {
          Mat &m=CV_srcImg_ds;

          if(x<0)
          {
            w+=x;
            x=0;
          }
          if(y<0)
          {
            h+=y;
            y=0;
          }
          if(x>=m.cols || y>=m.rows)continue;

          if(x+w>m.cols)
          {
            w=m.cols-x;
          }
          if(y+h>m.rows)
          {
            h=m.rows-y;
          }

          int margin=10;
          if(w<=margin || h<=margin)continue;


        }
        
        Mat CV_srcImg_region = CV_srcImg_ds(Rect(x,y,w,h));

        std::vector<line2Dup::Match> sub_matches= sbm->detector.match(
        CV_srcImg_region, 
        similarity_thres,
        magnitude_thres,
        {template_class_name,template_class_name+"_f"});

        line2Dup::Match maxMatch;
        maxMatch.similarity=0;
        float maxSim=0;
        for(auto &sub_match: sub_matches){
          if(maxSim<sub_match.similarity)
          {
            maxSim=sub_match.similarity;
            maxMatch=sub_match;
            maxMatch.x+=x;
            maxMatch.y+=y;
          }
        }

        LOGI(">>>maxMatch sim:%f",maxMatch.similarity);
        idxs.push_back(matches.size());
        matches.push_back(maxMatch);
      }
      doMatchFilter=false;
    }
    else
    {
      matches= sbm->detector.match(
      CV_srcImg_ds, 
      similarity_thres,
      magnitude_thres,
      {template_class_name,template_class_name+"_f"});
      LOGI(">>>>>>>>");
      doMatchFilter=true;


    }


    if(doMatchFilter)
    {

      vector<Rect> boxes;
      vector<float> scores;

      for(auto match: matches){
        Rect box;
        box.x = match.x;
        box.y = match.y;
        
        auto templ = sbm->detector.getTemplates(match.class_id,
                                            match.template_id);

        box.width = templ[0].width;
        box.height = templ[0].height;
        boxes.push_back(box);
        scores.push_back(match.similarity);

      }

      cv_dnn::NMSBoxes(boxes, scores, 0, 0.5f, idxs);
    }

  }




  LOGI("=====idxs.size():%d",idxs.size());


  // std::cout << "matches.size(): " << matches.size() << std::endl; 

  LOGI("matches.size():%d",matches.size());
  shared_ptr<StageInfo_Orientation> reportInfo(new StageInfo_Orientation());


  for(int i=0;i<idxs.size();i++)
  {
    auto idx=idxs[i];
    line2Dup::Match match = matches[idx];
    if(match.similarity==0)//for regional search, that needs to give yes/no answer
    {

      StageInfo_Orientation::orient orie;

      orie.angle=0;
      orie.flip=false;
      orie.center={0,0};
      orie.confidence=0;

      reportInfo->orientation.push_back(orie);
      continue;
    }
    
    auto templ = sbm->detector.getTemplates(match.class_id,match.template_id);

    //calc the position relative to the first point
    cv::Point2f f0Pt = cv::Point2f((float)templ[0].features[0].x+match.x,(float)templ[0].features[0].y+match.y)/matching_downScale;
    SBM_if::anchorInfo Aoffset = sbm->fetchTemplateOffset(match.class_id);
    LOGI("[%d]>>>ang:%f <<id:%s",i,templ[0].angle,match.class_id.c_str());
    cv::Point2f anchorPt = rotate2d(Aoffset.offset ,templ[0].angle*M_PI/180);

    float offset = (1/matching_downScale-1);// /2;
    f0Pt+=cv::Point2f(offset,offset);
    anchorPt+=f0Pt;


    float refine_score=0;
    float refinedAngleRad=templ[0].angle*M_PI/180;

    double refine_score_thres=JFetch_NUMBER_ex(def,"refine_score_thres",0.5);
    bool must_refine_result=JFetch_TRUE(def,"must_refine_result");

    
    LOGI("refine_score_thres:%f must_refine_result:%d",refine_score_thres,must_refine_result);
    if(refine_region_set.size()>0 && refine_score_thres>0)
    {
      float tmpAngle=refinedAngleRad;
      cv::Point2f tmp_anchorPt=anchorPt;
      int margin=(int)(25);
      DBG_C=0;
      refine_score = PoseRefine(CV_srcImg,refine_region_set,margin,tmp_anchorPt,tmpAngle,0.2);
      
      // if(refine_score>0.3)
      if(1)//further refine
      {
        LOGI("[%d]-----refine_score:%f",i,refine_score);
        auto tmp_anchorPt2=tmp_anchorPt;
        auto tmpAngle2=tmpAngle;
        int refine_block_count=0;
        float refine_score2;

        for(int i=1;i<3;i++)
        {
          DBG_C=i;
          refine_score2 = PoseRefine(CV_srcImg,refine_region_set,margin,tmp_anchorPt2,tmpAngle2,0.2,&refine_block_count);

          LOGI("[%d]-----refine_score2:%f",i,refine_score2);
          if(refine_score<=refine_score2)
          {
            refine_score=refine_score2;
            tmpAngle=tmpAngle2;
            tmp_anchorPt=tmp_anchorPt2;
          }
          else
          {
            break;//early stop
          }
        }

      }

      LOGI("[%d]----------refine_score:%f  must_refine_result:%d",i,refine_score,must_refine_result);
      LOGI(" %f =>  %f",refinedAngleRad*180/M_PI,tmpAngle*180/M_PI);
      LOGI(" %f,%f  =>  %f,%f",anchorPt.x,anchorPt.y,tmp_anchorPt.x,tmp_anchorPt.y);

      if(refine_score>refine_score_thres)
      {//accept the refinement
        refinedAngleRad=tmpAngle;
        anchorPt=tmp_anchorPt;
      }
      else if(must_refine_result==true)//for regional search, that needs to give yes/no answer
      {

        StageInfo_Orientation::orient orie;

        orie.angle=0;
        orie.flip=false;
        orie.center={0,0};
        orie.confidence=0;

        reportInfo->orientation.push_back(orie);
        continue;

        // refinedAngleRad=tmpAngle;
        // anchorPt=tmp_anchorPt;
        // match.similarity=0.1;
      }
    }
    if(refine_score>0.999)refine_score=0.999;


    StageInfo_Orientation::orient orie;

    orie.angle=refinedAngleRad;
    orie.flip=hasEnding(match.class_id,"_f");
    orie.center={anchorPt.x,anchorPt.y};
    orie.confidence=round(match.similarity)+refine_score;//HACK to store refine info

    reportInfo->orientation.push_back(orie);

  }

  LOGI(">>>>>>>>");
  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->img=srcImg;
  
  reportInfo->trigger_id=sinfo->trigger_id;

  reportInfo->sharedInfo.push_back(sinfo);
  reportInfo->trigger_tags.push_back(id);
  insertInputTagsWPrefix(reportInfo->trigger_tags,sinfo->trigger_tags,"s_");


  LOGI(">>>>>>>>");
  

  reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->img_prop.StreamInfo.downsample=1;//JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",4);
  LOGI("CHID:%d",reportInfo->img_prop.StreamInfo.channel_id);

  {
    int64 t1 = cv::getTickCount();
    double secs_us = 1000000*(t1-t0)/cv::getTickFrequency();
    reportInfo->process_time_us=secs_us;
    reportInfo->create_time_sysTick=t1;
    // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

    LOGI(">>>>>>>>process_time_us:%f",secs_us);
  }

  reportInfo->genJsonRepTojInfo();


  belongMan->dispatch(reportInfo);

  
}

InspectionTarget_Orientation_ShapeBasedMatching::~InspectionTarget_Orientation_ShapeBasedMatching()
{
  if(sbm)
    delete sbm;
}
