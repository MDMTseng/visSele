
#include "InspTar_Orientation.hpp"


using namespace cv;

using namespace std;


template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_Orientation_ShapeBasedMatching::InspectionTarget_Orientation_ShapeBasedMatching(string id,cJSON* def,InspectionTargetManager* belongMan)
  :InspectionTarget(id,NULL,belongMan)
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
    if( matchTriggerTag(tag,def))
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
    sbm->regTemplateOffset(template_name     ,{cenOffset,false});
    cenOffset.y*=-1;
    sbm->regTemplateOffset(template_name+"_f",{cenOffset,true});

    if(match_front_face)
    {
      sbm->train(template_name     ,insp_tp,cv::Point2f(0,0),false,matching_downScale,0,360,360);
    }

    if(match_back_face)
    {
      sbm->train(template_name+"_f",insp_tp,cv::Point2f(0,0),true ,matching_downScale,0,360,360);
    }



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
      act.send("IM",id,&src_acvImg,10);
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
      sbm->TemplateFeatureExtraction(img,_mask,num_features,tp);


      cJSON* jtp=TemplatePyramid2Json(tp);

      act.send("RP",id,jtp);
      cJSON_Delete(jtp);jtp=NULL;
    }

    return true;

  }


  
  if(type=="cache_image_save")
  {
    if(cache_stage_info==NULL)return false;
    string folder_path=JFetch_STRING_ex(info,"folder_path");
    if(folder_path.length()==0)return false;

    auto srcImg=cache_stage_info->imgSets["img"];
    if(srcImg==NULL)return false;

    Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);


    string image_name=JFetch_STRING_ex(info,"image_name","test.png");
    imwrite(folder_path+"/"+image_name, CV_srcImg);  



    // cache_stage_info



  }

  return false;
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


void InspectionTarget_Orientation_ShapeBasedMatching::singleProcess(shared_ptr<StageInfo> sinfo)
{

  cache_stage_info=sinfo;
  LOGI(">>>>>>>>InspectionTarget_Orientation_ShapeBasedMatching>>>>>>>>");
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  
  auto srcImg=sinfo->imgSets["img"];

  LOGI(">>>>>>>>");

  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);


  cv::Size size1 = CV_srcImg.size();
  size1.width=((int)(size1.width*matching_downScale))/8*8;
  size1.height=((int)(size1.height*matching_downScale))/8*8;

  Mat CV_srcImg_ds(size1,CV_8UC3);
  resize(CV_srcImg,CV_srcImg_ds,size1,cv::INTER_AREA);
  

  float magThres_eq_alpha=0.3;
  float magnitude_thres=JFetch_NUMBER_ex(def,"magnitude_thres",20)/(magThres_eq_alpha+(1-magThres_eq_alpha)*matching_downScale);
  if(magnitude_thres>128)magnitude_thres=128;
  std::vector<line2Dup::Match> matches = sbm->detector.match(CV_srcImg_ds, 
    JFetch_NUMBER_ex(def,"similarity_thres",60),
    magnitude_thres,
    {template_name,template_name+"_f"});








  vector<Rect> boxes;
  vector<float> scores;
  vector<int> idxs;
  int CCC=0;
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
    CCC++;
  }
  cv_dnn::NMSBoxes(boxes, scores, 0, 0.5f, idxs);



  LOGI("=====idxs.size():%d",idxs.size());


  // std::cout << "matches.size(): " << matches.size() << std::endl; 

  LOGI("matches.size():%d",matches.size());
  shared_ptr<StageInfo_Orientation> reportInfo(new StageInfo_Orientation());


  cJSON* rep_regionInfo=cJSON_CreateArray();

  for(auto idx: idxs)
  {
       
    line2Dup::Match match = matches[idx];
    auto templ = sbm->detector.getTemplates(match.class_id,match.template_id);

    //calc the position relative to the first point
    cv::Point2f f0Pt = cv::Point2f((float)templ[0].features[0].x+match.x,(float)templ[0].features[0].y+match.y)/matching_downScale;
    SBM_if::anchorInfo Aoffset = sbm->fetchTemplateOffset(match.class_id);
    LOGI(">>>ang:%f <<id:%s",templ[0].angle,match.class_id.c_str());
    cv::Point2f anchorPt = rotate2d(Aoffset.offset ,templ[0].angle*M_PI/180);
    anchorPt+=f0Pt;


    cJSON *region_report=cJSON_CreateObject();
    cJSON_AddItemToArray(rep_regionInfo,region_report);


    cJSON_AddNumberToObject(region_report,"x",anchorPt.x);
    cJSON_AddNumberToObject(region_report,"y",anchorPt.y);
    cJSON_AddNumberToObject(region_report,"angle",templ[0].angle/180*M_PI);

    cJSON_AddNumberToObject(region_report,"template_id",match.template_id);
    cJSON_AddNumberToObject(region_report,"similarity",match.similarity);
    cJSON_AddStringToObject(region_report,"class_id",match.class_id.c_str());


  }


  LOGI(">>>>>>>>");
  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->imgSets["img"]=srcImg;//shared_ptr<acvImage>(retImage);
  
  reportInfo->trigger_id=sinfo->trigger_id;
  // reportInfo->trigger_tags.push_back("InfoStream2UI");
  // reportInfo->trigger_tags.push_back("ToTestRule");
  reportInfo->trigger_tags.push_back("ImTran");

  
  reportInfo->trigger_tags.push_back(id);

  LOGI(">>>>>>>>");
  

  reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",4);
  LOGI("CHID:%d",reportInfo->img_prop.StreamInfo.channel_id);

  reportInfo->jInfo=rep_regionInfo;

  // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

  LOGI(">>>>>>>>");
  belongMan->dispatch(reportInfo);

  
}

InspectionTarget_Orientation_ShapeBasedMatching::~InspectionTarget_Orientation_ShapeBasedMatching()
{
  if(sbm)
    delete sbm;
}
