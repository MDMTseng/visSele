
#include "InspTar_Orientation.hpp"

#include <TemplateMatching_SubPix.h>
#include <InspTarUtil.hpp>
using namespace cv;

using namespace std;

template <typename Base, typename T>
inline bool instanceof (const T)
{
  return is_base_of<Base, T>::value;
}

void InspectionTarget_Orientation_ShapeBasedMatching::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{

  // LOGE("InspectionTarget_Orientation_ShapeBasedMatching::INIT");
  sbm = NULL;
  recentSrcStageInfoSetIdx.RESET(20);
  recentSrcStageInfoSet.resize(recentSrcStageInfoSetIdx.space());
  InspectionTarget::INIT(id,def,belongMan,local_env_path);

}

// future<int> InspectionTarget_Orientation_ShapeBasedMatching::futureInputStagePool()
// {
//   return async(launch::async, &InspectionTarget_Orientation_ShapeBasedMatching::processInputStagePool, this);
// }

void InspectionTarget_Orientation_ShapeBasedMatching::run()
{

  // acvImage cacheImage;
  while(true)
  {
    std::shared_ptr<StageInfo> curInput;
    // LOGI("<<<<<size():%d",datTransferQueue.size());
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test
          
    try{
      // LOGI("TryReadNew");
      if(input_queue.pop_blocking(curInput)==false)
      {
        LOGI("TryReadTailed");
        break;
      }

      singleProcess(curInput);
    }
    catch(TS_Termination_Exception e)
    {
      LOGI("TS_Termination_Exception");
      break;
    }
    
  }

}

line2Dup::Template Json2Template(cJSON *jtpTemp)
{
  line2Dup::Template tmpl;
  tmpl.angle = JFetch_NUMBER_ex(jtpTemp, "angle");
  tmpl.width = JFetch_NUMBER_ex(jtpTemp, "width");
  tmpl.height = JFetch_NUMBER_ex(jtpTemp, "height");
  tmpl.tl_x = JFetch_NUMBER_ex(jtpTemp, "tl_x");
  tmpl.tl_y = JFetch_NUMBER_ex(jtpTemp, "tl_y");
  tmpl.pyramid_level = JFetch_NUMBER_ex(jtpTemp, "pyramid_level");

  // tmpl.features.push_back();
  for (int i = 0;; i++)
  {
    cJSON *jfeat = JFetch_OBJECT(jtpTemp, ("features[" + to_string(i) + "]").c_str());

    if (jfeat == NULL)
      break;
    line2Dup::Feature feat;
    feat.label = JFetch_NUMBER_ex(jfeat, "label");
    feat.theta = JFetch_NUMBER_ex(jfeat, "theta");
    feat.x = JFetch_NUMBER_ex(jfeat, "x");
    feat.y = JFetch_NUMBER_ex(jfeat, "y");
    tmpl.features.push_back(feat);
  }
  return tmpl;
}

line2Dup::TemplatePyramid Json2TemplatePyramid(cJSON *jtpArr)
{
  line2Dup::TemplatePyramid tp;
  for (int i = 0;; i++)
  {
    cJSON *layerTemp = JFetch_OBJECT(jtpArr, ("[" + to_string(i) + "]").c_str());

    if (layerTemp == NULL)
      break;
    // if(cJSON_GetArraySize(JFetch_ARRAY(jtpArr,"features"))==0)
    // {
    //   break;
    // }
    line2Dup::Template temp = Json2Template(layerTemp);
    tp.push_back(temp);
  }
  return tp;
}


void  InspectionTarget_Orientation_ShapeBasedMatching::RegisterTemplate(float scale)
{

  LOGE("origin_offset_angle:%f",origin_offset_angle*180/3.14159);


  cv::Point2f f0Pos(insp_tp[0].tl_x + insp_tp[0].features[0].x, insp_tp[0].tl_y + insp_tp[0].features[0].y);
  cv::Point2f cenOffset = templateCenter - f0Pos;
  sbm->regTemplateOffset(template_class_name, {cenOffset, false});


  sbm->detector.removeClass(template_class_name);
  if (front_face_angle_segs>0)
  {
    sbm->train(template_class_name, insp_tp, cv::Point2f(0, 0), false,
                matching_downScale*scale,
                front_face_angle_start,
                front_face_angle_end,
                front_face_angle_segs);
  }


  cenOffset.y *= -1;
  sbm->regTemplateOffset(template_class_name + "_f", {cenOffset, true});
  sbm->detector.removeClass(template_class_name + "_f");


  if (back_face_angle_segs>0)
  {
    sbm->train(template_class_name + "_f", insp_tp, cv::Point2f(0, 0), true,
                matching_downScale*scale,
                back_face_angle_start,
                back_face_angle_end,
                back_face_angle_segs);
  }

}


void InspectionTarget_Orientation_ShapeBasedMatching::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);

  template_mmpp=1;
  // featureInfo
  if (sbm)
  {
    delete (sbm);
    sbm = NULL;
  }
  refine_region_set.clear();
  matching_downScale = JFetch_NUMBER_ex(def, "matching_downScale", 1);

  matching_angle_apart=JFetch_NUMBER_ex(def,"matching_angle_apart",360);
  if (matching_downScale < 0.01)
    matching_downScale = 0.01;
  cJSON *featureInfo = JFetch_OBJECT(def, "featureInfo");
  if (featureInfo)
  {
    template_mmpp=JFetch_NUMBER_ex(featureInfo, "mmpp", 1);
    int num_features = JFetch_NUMBER_ex(featureInfo, "num_features", 60);
    int weak_thresh = JFetch_NUMBER_ex(featureInfo, "weak_thresh", 30);
    int strong_thresh = JFetch_NUMBER_ex(featureInfo, "strong_thresh", 80);
    vector<int> T;
    for (int i = 0;; i++)
    {
      double *t = JFetch_NUMBER(featureInfo, ("T[" + to_string(i) + "]").c_str());
      if (t)
        T.push_back(*t);
      else
        break;
    }
    if (T.size() == 0) // default
    {
      T.push_back(4);
      T.push_back(6);
      T.push_back(12);
    }

    LOGI(">>>%d,%d,%d", num_features, weak_thresh, strong_thresh);
    this->sbm = new SBM_if(num_features, T, weak_thresh, strong_thresh);

    cJSON *jtemplatePyramid = JFetch_ARRAY(featureInfo, "templatePyramid");

    this->insp_tp = Json2TemplatePyramid(jtemplatePyramid);

    

    while(insp_tp.size()<T.size())
    {
      T.pop_back();
    }

    LOGE("insp_tp.size():%d T.size():%d",insp_tp.size(),T.size());


    float originVecX = JFetch_NUMBER_ex(featureInfo, "origin_info.vec.x", 1);
    float originVecY = JFetch_NUMBER_ex(featureInfo, "origin_info.vec.y", 0);

    this->origin_offset_angle = atan2(originVecY, originVecX);


    int templateCenter_x = JFetch_NUMBER_ex(featureInfo, "origin_info.pt.x", insp_tp[0].tl_x);
    int templateCenter_y = JFetch_NUMBER_ex(featureInfo, "origin_info.pt.y", insp_tp[0].tl_y);
    this->templateCenter.x=templateCenter_x;
    this->templateCenter.y=templateCenter_y;


    bool match_front_face = JFetch_TRUE(featureInfo, "match_front_face");
    bool match_back_face = JFetch_TRUE(featureInfo, "match_back_face");
    this->front_face_angle_segs=0;
    this->back_face_angle_segs=0;


    if(match_front_face)
    {
      this->front_face_angle_start = JFetch_NUMBER_ex(featureInfo, "match_front_face_angle_range[0]", -179.999);
      this->front_face_angle_end = JFetch_NUMBER_ex(featureInfo, "match_front_face_angle_range[1]", 180);
      this->front_face_angle_segs = (int)round(JFetch_NUMBER_ex(featureInfo, "match_front_face_angle_segs", (front_face_angle_end - front_face_angle_start)));
    }

    if(match_back_face)
    {
      this->back_face_angle_start = JFetch_NUMBER_ex(featureInfo, "match_back_face_angle_range[0]", -179.999);
      this->back_face_angle_end = JFetch_NUMBER_ex(featureInfo, "match_back_face_angle_range[1]", 180);
      this->back_face_angle_segs = (int)round(JFetch_NUMBER_ex(featureInfo, "match_back_face_angle_segs", (back_face_angle_end - back_face_angle_start)));
    }


    // RegisterTemplate();



    cJSON *refine_match_regions = JFetch_ARRAY(featureInfo, "refine_match_regions");
    if (refine_match_regions)
    {
      string feature_ref_image_path = local_env_path + "/" + JFetch_STRING_ex(featureInfo, "feature_ref_image", "FeatureRefImage.png");

      Mat img = imread(feature_ref_image_path, IMREAD_COLOR);
      if (img.empty() == false)
      {

        int reg_size = cJSON_GetArraySize(refine_match_regions);

        for (int i = 0; i < reg_size; i++)
        {
          cJSON *regionInfo = cJSON_GetArrayItem(refine_match_regions, i);
          int x = (int)JFetch_NUMBER_ex(regionInfo, "x");
          int y = (int)JFetch_NUMBER_ex(regionInfo, "y");
          int w = (int)JFetch_NUMBER_ex(regionInfo, "w");
          int h = (int)JFetch_NUMBER_ex(regionInfo, "h");

          struct refine_region_info regInfo;
          regInfo.regionInRef = Rect2d(x, y, w, h);

          if (x + w > img.cols || y + h > img.rows)
            continue;
          regInfo.img = img(regInfo.regionInRef).clone();

          regInfo.regionInRef.x -= templateCenter_x;
          regInfo.regionInRef.y -= templateCenter_y;

          // imwrite(local_env_path+"/"+"ddd"+std::to_string(i)+".jpg", regInfo.img);

          LOGI("refine reg ori :%d,%d,%d,%d", x, y, w, h);
          LOGI("refine reg load:%f,%f,%f,%f", regInfo.regionInRef.x, regInfo.regionInRef.y, regInfo.regionInRef.width, regInfo.regionInRef.height);
          
          refine_region_set.push_back(regInfo);
        }
      }

      LOGI(">>>>>>>>>>>>>>>>>feature_ref_image_path:%s", feature_ref_image_path.c_str());
    }

    refine_angle_only = JFetch_TRUE(featureInfo, "refine_angle_only");
    LOGI(">>>>>>>>>>>>>>>>>local_env_path:%s", local_env_path.c_str());
  }
}

cJSON *TemplateFeature2Json(line2Dup::Feature &feat)
{
  cJSON *jfeat = cJSON_CreateObject();
  cJSON_AddNumberToObject(jfeat, "label", feat.label);
  cJSON_AddNumberToObject(jfeat, "theta", feat.theta);
  cJSON_AddNumberToObject(jfeat, "x", feat.x);
  cJSON_AddNumberToObject(jfeat, "y", feat.y);

  return jfeat;
}

cJSON *Template2Json(line2Dup::Template &templ)
{
  cJSON *jtempl = cJSON_CreateObject();
  cJSON_AddNumberToObject(jtempl, "angle", templ.angle);
  cJSON_AddNumberToObject(jtempl, "tl_x", templ.tl_x);
  cJSON_AddNumberToObject(jtempl, "tl_y", templ.tl_y);
  cJSON_AddNumberToObject(jtempl, "height", templ.height);
  cJSON_AddNumberToObject(jtempl, "width", templ.width);
  cJSON_AddNumberToObject(jtempl, "pyramid_level", templ.pyramid_level);

  cJSON *jfeats = cJSON_CreateArray();
  cJSON_AddItemToObject(jtempl, "features", jfeats);
  for (int i = 0; i < templ.features.size(); i++)
  {
    cJSON_AddItemToArray(jfeats, TemplateFeature2Json(templ.features[i]));
  }
  return jtempl;
}

cJSON *TemplatePyramid2Json(line2Dup::TemplatePyramid &tp)
{
  cJSON *jtpArr = cJSON_CreateArray();
  for (int i = 0; i < tp.size(); i++)
  {
    cJSON_AddItemToArray(jtpArr, Template2Json(tp[i]));
  }

  return jtpArr;
}


static void XYWH_clipping(int &X,int &Y,int &W,int &H, int MX,int MY,int MW,int MH)
{
  if(X<MX)
  {
    W-=MX-X;
    X=MX;
  }

  if(Y<MY)
  {
    H-=MY-Y;
    Y=MY;
  }


  if(X>MX+MW)
  {
    X=MX+MW-1;
  }
  if(Y>MY+MH)
  {
    Y=MY+MH-1;
  }

  if(X+W>MX+MW)
  {
    W=MX+MW-X;
  }

  if(Y+H>MY+MH)
  {
    H=MY+MH-Y;
  }
}

struct PointFeature {
    cv::Point2f point;
    float a1;
    cv::Vec2f v1;
    float a2;
    cv::Vec2f v2;
};

struct AlignmentResult {
    cv::Mat transformationMatrix;
    float error;
};

float computeError(const std::vector<PointFeature>& features, const cv::Mat& transform) {
    float totalError = 0.0f;

    for (const auto& feature : features) {
        cv::Point2f transformedPoint;
        cv::Mat pointMat = (cv::Mat_<float>(3, 1) << feature.point.x, feature.point.y, 1.0f);
        cv::Mat transformedPointMat = transform * pointMat;
        transformedPoint.x = transformedPointMat.at<float>(0, 0);
        transformedPoint.y = transformedPointMat.at<float>(1, 0);

        cv::Vec2f diff = transformedPoint - feature.point;
        float error = feature.a1 * diff.dot(feature.v1) + feature.a2 * diff.dot(feature.v2);
        totalError += error * error;
    }

    return totalError;
}

AlignmentResult alignPoints(const std::vector<PointFeature>& features) {
    AlignmentResult result;
    result.transformationMatrix = cv::Mat::eye(3, 3, CV_32F);
    result.error = std::numeric_limits<float>::max();

    // Parameters for gradient descent
    float learningRate = 0.01f;
    int maxIterations = 1000;
    float tolerance = 1e-6f;

    cv::Mat currentTransform = cv::Mat::eye(3, 3, CV_32F);
    float currentError = computeError(features, currentTransform);

    for (int iter = 0; iter < maxIterations; ++iter) {
        cv::Mat gradient = cv::Mat::zeros(3, 3, CV_32F);

        // Compute gradient of the error with respect to the transformation matrix
        // (using numerical differentiation for simplicity)
        float epsilon = 1e-6f;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                cv::Mat perturbedTransform = currentTransform.clone();
                perturbedTransform.at<float>(i, j) += epsilon;
                float perturbedError = computeError(features, perturbedTransform);
                gradient.at<float>(i, j) = (perturbedError - currentError) / epsilon;
            }
        }

        // Update the transformation matrix using gradient descent
        currentTransform -= learningRate * gradient;

        // Recompute the error
        float newError = computeError(features, currentTransform);

        // Check for convergence
        if (std::abs(currentError - newError) < tolerance) {
            break;
        }

        currentError = newError;
    }

    result.transformationMatrix = currentTransform;
    result.error = currentError;
    return result;
}










bool InspectionTarget_Orientation_ShapeBasedMatching::exchangeCMD(cJSON *info, int id, exchangeCMD_ACT &act)
{

  std::lock_guard<std::mutex> lock_(process_lock);
  LOGI(">>>>>>>LOCK>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info, id, act); // apply framework layer exchange
  if (ret)
    return ret;
  string type = JFetch_STRING_ex(info, "type");

  if (type == "extract_feature")
  {
    string path = JFetch_STRING_ex(info, "image_path");
    if (path == "")
    {
      return false;
    }
    Mat img = imread(path, IMREAD_COLOR);
    if (img.empty())
    {
      return false;
    }

    // acvImage src_acvImg(img.cols,img.rows,3);
    // for(int i=0;i<img.rows;i++)for(int j=0;j<img.cols;j++)
    // {
    //   src_acvImg.CVector[i][j*3]=0;
    //   src_acvImg.CVector[i][j*3+1]=0;
    //   src_acvImg.CVector[i][j*3+2]=0;
    // }
    int image_scale = (int)JFetch_NUMBER_ex(info, "image_scale", 1);
    if (image_scale > 0)
    {
      act.send(id, img, "jpg", 90);
    }

    int num_features = JFetch_NUMBER_ex(info, "num_features", 60);

    if (num_features > 0)
    {

      Mat _mask;
      cJSON *mask_regions = JFetch_ARRAY(info, "mask_regions");
      if (mask_regions)
      {

        int reg_size = cJSON_GetArraySize(mask_regions);
        if (reg_size == 0)
        {
          _mask = Mat(img.size(), CV_8UC1, {255}); // default all white region
        }
        else
        {
          _mask = Mat::zeros(img.rows, img.cols, CV_8UC1); // Mat(img.size(), CV_8UC1,{0});//default all black region
          for (int i = 0; i < reg_size; i++)
          {
            cJSON *regionInfo = cJSON_GetArrayItem(mask_regions, i);
            bool isBlackRegion = JFetch_TRUE(regionInfo, "isBlackRegion");

            int x = JFetch_NUMBER_ex(regionInfo, "x",-1);
            int y = JFetch_NUMBER_ex(regionInfo, "y",-1);
            int w = JFetch_NUMBER_ex(regionInfo, "w",-1);
            int h = JFetch_NUMBER_ex(regionInfo, "h",-1);

            XYWH_clipping(x,y,w,h, 0,0,_mask.cols,_mask.rows);
            LOGI("ROI: %d,%d,%d,%d<<<<", x, y, w, h);
            if (x>0 && y >0 && w >0 && h >0)
            {
              _mask(Rect(x, y, w, h)) = isBlackRegion ? 0 : 255;
            }
          }
        }
      }
      else
      {
        _mask = Mat(img.size(), CV_8UC1, {255}); // default all white region
      }

      line2Dup::TemplatePyramid tp;
      // 1087,231   1596,666

      {
        float weak_thresh = JFetch_NUMBER_ex(info, "weak_thresh", 30);
        float strong_thresh = JFetch_NUMBER_ex(info, "strong_thresh", 80);


        int feature_min_space = JFetch_NUMBER_ex(info, "feature_min_space", 5);

        LOGE("feature_min_space:%d",feature_min_space);
        vector<int> T;
        for (int i = 0;; i++)
        {
          double *t = JFetch_NUMBER(info, ("T[" + to_string(i) + "]").c_str());
          if (t)
            T.push_back(*t);
          else
            break;
        }
        if (T.size() == 0) // default
        {
          T.push_back(2);
          T.push_back(4);
        }
        SBM_if t_sbm(num_features, T, weak_thresh, strong_thresh);
        t_sbm.TemplateFeatureExtraction(img, _mask, num_features,feature_min_space, tp);
      }

      cJSON *jtp = TemplatePyramid2Json(tp);

      act.send("RP", id, jtp);
      cJSON_Delete(jtp);
      jtp = NULL;
    }

    return true;
  }


  if(type=="GetFetchSrcTIDList")
  {
      
    std::lock_guard<std::mutex> lock(recentSrcLock); 
    cJSON* arr= cJSON_CreateArray();

    for(int i=0;i<recentSrcStageInfoSetIdx.size();i++)
    {
      int idx = recentSrcStageInfoSetIdx.getHead(i+1);
      // cJSON_AddItemToArray(arr,cJSON_CreateNumber(recentSrcStageInfoSet[idx]->trigger_id));

      cJSON_AddItemToArray(arr,cJSON_CreateNumber(i));

    }

    act.send("RP",id,arr);
    cJSON_Delete(arr);arr=NULL;
    return true;
  }


  if(type=="FetchCountDown")
  {
      
    // FetchCountDown_OK=JFetch_NUMBER_ex(info,"count_OK",FetchCountDown_OK);
    // FetchCountDown_NG=JFetch_NUMBER_ex(info,"count_NG",FetchCountDown_NG);
    // FetchCountDown_NG2=JFetch_NUMBER_ex(info,"count_NG2",FetchCountDown_NG2);
    // FetchCountDown_NG3=JFetch_NUMBER_ex(info,"count_NG3",FetchCountDown_NG3);
    // FetchCountDown_NA=JFetch_NUMBER_ex(info,"count_NA",FetchCountDown_NA);
    return true;
  }




  if(type=="TriggerFetchSrc")
  {
    int targetIdx=-1;


    float index=JFetch_NUMBER_ex(info,"index");
    if(index==index)
    {
        targetIdx = recentSrcStageInfoSetIdx.getHead(index);
    }
    else
    {
      
      std::lock_guard<std::mutex> lock(recentSrcLock); 
      

      float ftargetTID=JFetch_NUMBER_ex(info,"trigger_id");
      if(ftargetTID!=ftargetTID)return false;
      int targetTID=ftargetTID;
          // LOGI("<<<<targetTID:%d>>>>",targetTID);
      for(int i=0;i<recentSrcStageInfoSetIdx.size();i++)
      {
        int idx = recentSrcStageInfoSetIdx.getTail(i);
        auto &infoSet=recentSrcStageInfoSet[idx];

          // LOGI("<<<<infoSet tid:%d  size:%d>>>>",infoSet[0]->trigger_id,infoSet.size());
        if(infoSet->get_trigger_id()!=targetTID)continue;
        targetIdx=i;
        break;
      }

    }

    if(targetIdx>=0 && targetIdx<recentSrcStageInfoSetIdx.size())
    {
      auto &infoSet=recentSrcStageInfoSet[targetIdx];

      auto *src = dynamic_cast<StageInfo*>(infoSet.get());

      if(src)
      {



        
        LOGI("SEND....");
        shared_ptr<StageInfo> pkt(new StageInfo());
        pkt->img=src->img;
        pkt->img_prop=src->img_prop;
        pkt->img_show=src->img_show;
        pkt->set_process_time_us(src->get_process_time_us());

        pkt->source=src->source;
        pkt->set_source_id(src->get_source_id());


        {
          vector<string> tags;
          src->get_trigger_tags(tags);
          tags.push_back("s_uInspCache_");
          pkt->set_trigger_tags(tags);
        }
        pkt->set_trigger_id(-src->get_trigger_id());
        belongMan->dispatch(pkt);





      }
      
      return true;
    }

        LOGI("END....");
    return true;
  }


  if(type=="ClearFetchSrc")
  {
    recentSrcStageInfoSetIdx.clear();
    return true;
  }


  // if(type=="test")
  // {
  //   //info={
  //   //  "type":"test",
  //   //  "srcPoints":[[0,0],[1,0],[0,1],[2,2]],
  //   //  "dstPoints":[[1,1],[2,1],[1,2],[3,3]],
  //   //  "accuracyVectors":[[1,1],[0.707,0.707],[1,1],[0.866,0.5]]}

  //   cJSON *jsrcPoints = JFetch_ARRAY(info, "srcPoints");
  //   cJSON *jdstPoints = JFetch_ARRAY(info, "dstPoints");
  //   cJSON *jaccuracyVectors = JFetch_ARRAY(info, "accuracyVectors");
  //   std::vector<cv::Point2f> srcPoints;
  //   std::vector<cv::Point2f> dstPoints;
  //   std::vector<cv::Vec2f> accuracyVectors;
  //   if (jsrcPoints && jdstPoints && jaccuracyVectors && cJSON_GetArraySize(jsrcPoints) == cJSON_GetArraySize(jdstPoints) && cJSON_GetArraySize(jsrcPoints) == cJSON_GetArraySize(jaccuracyVectors)) {


  //     for (int i = 0; i < cJSON_GetArraySize(jsrcPoints); ++i) {
  //         cJSON *srcPoint = cJSON_GetArrayItem(jsrcPoints, i);
  //         cJSON *dstPoint = cJSON_GetArrayItem(jdstPoints, i);
  //         cJSON *accuracyVector = cJSON_GetArrayItem(jaccuracyVectors, i);

  //         srcPoints.push_back(cv::Point2f(JFetch_NUMBER_ex(srcPoint, "[0]"), JFetch_NUMBER_ex(srcPoint, "[1]")));
  //         dstPoints.push_back(cv::Point2f(JFetch_NUMBER_ex(dstPoint, "[0]"), JFetch_NUMBER_ex(dstPoint, "[1]")));
  //         accuracyVectors.push_back(cv::Vec2f(JFetch_NUMBER_ex(accuracyVector, "[0]"), JFetch_NUMBER_ex(accuracyVector, "[1]")));
  //     }

  //   }
  //   else
  //   {
  //     return false;
  //   }

  //   // Convert points to cv::Mat
  //   cv::Mat srcMat(srcPoints);
  //   cv::Mat dstMat(dstPoints);

  //   // Create weight matrix
  //   cv::Mat W = createWeightMatrix(accuracyVectors);

  //   // Formulate the weighted least squares problem
  //   cv::Mat A = cv::Mat::zeros(2 * srcPoints.size(), 6, CV_64F);
  //   cv::Mat b = cv::Mat::zeros(2 * srcPoints.size(), 1, CV_64F);

  //   for (size_t i = 0; i < srcPoints.size(); ++i) {
  //       A.at<double>(2 * i, 0) = srcPoints[i].x;
  //       A.at<double>(2 * i, 1) = srcPoints[i].y;
  //       A.at<double>(2 * i, 2) = 1;
  //       A.at<double>(2 * i + 1, 3) = srcPoints[i].x;
  //       A.at<double>(2 * i + 1, 4) = srcPoints[i].y;
  //       A.at<double>(2 * i + 1, 5) = 1;

  //       b.at<double>(2 * i, 0) = dstPoints[i].x;
  //       b.at<double>(2 * i + 1, 0) = dstPoints[i].y;
  //   }

  //   // Apply the weight matrix
  //   cv::Mat WtW = W.t() * W;
  //   cv::Mat WtA = W.t() * A;
  //   cv::Mat Wtb = W.t() * b;

  //   // Solve for the affine transformation parameters
  //   cv::Mat affineParams;
  //   cv::solve(WtA, Wtb, affineParams, cv::DECOMP_SVD);

  //   // Reshape the solution into a 2x3 affine transformation matrix
  //   cv::Mat affineMatrix = (cv::Mat_<double>(2, 3) << affineParams.at<double>(0), affineParams.at<double>(1), affineParams.at<double>(2),
  //                                                    affineParams.at<double>(3), affineParams.at<double>(4), affineParams.at<double>(5));

  //   // Output the transformation matrix
  //   std::cout << "Affine Transformation Matrix:\n" << affineMatrix << std::endl;

  //   // Extract rotation and translation information
  //   double angle = atan2(affineMatrix.at<double>(1, 0), affineMatrix.at<double>(0, 0)) * 180.0 / CV_PI;
  //   cv::Point2f translation(affineMatrix.at<double>(0, 2), affineMatrix.at<double>(1, 2));

  //   std::cout << "Rotation Angle: " << angle << " degrees" << std::endl;
  //   std::cout << "Translation: (" << translation.x << ", " << translation.y << ")" << std::endl;

  //   return true;
  // }
  

  if(type=="test2")
  {
    std::vector<PointFeature> features = {
        {{0.0f, 0.0f}, 1.0f, {1.0f, 0.0f}, 1.0f, {0.0f, 1.0f}},
        {{1.0f, 1.0f}, 1.0f, {0.414f, -0.414f}, 0.0f, {0.414f, -0.414f}},
        // Add more features as needed
    };

    AlignmentResult result = alignPoints(features);

    std::cout << "Transformation Matrix:\n" << result.transformationMatrix << std::endl;
    std::cout << "Alignment Error: " << result.error << std::endl;

    return true;
  }
  return false;
}

cv::Point2f rotate2d(const cv::Point2f &inPoint, const double angRad)
{
  cv::Point2f outPoint;
  // CW rotation
  double c = std::cos(angRad);
  double s = std::sin(angRad);
  outPoint.x = c * inPoint.x - s * inPoint.y;
  outPoint.y = s * inPoint.x + c * inPoint.y;
  return outPoint;
}

cv::Point2f rotatePoint(const cv::Point2f &inPoint, const cv::Point2f &center, const double angRad)
{
  return rotate2d(inPoint - center, angRad) + center;
}

cv::Mat rotCrop(cv::Mat &srcImg, float obj_x, float obj_y, bool y_flip, float temp_rel_x, float temp_rel_y, float temp_w, float temp_h, float angRad, int margin = 5, int downSamp = 1, cv::Point2f *ret_center = NULL)
{

  // LOGI("temp:: %f,%f,%f,%f  margin:%d  angRad:%f",temp_rel_x,temp_rel_y,temp_w,temp_h,margin,angRad);

  temp_rel_x -= margin;
  temp_rel_y -= margin;
  temp_w += margin * 2;
  temp_h += margin * 2;

  int yMult = y_flip ? -1 : 1;
  // LOGI(">>temp:: %f,%f,%f,%f",temp_rel_x,temp_rel_y,temp_w,temp_h);

  Point2f objPos = Point2f(obj_x, obj_y);
  if (y_flip)
  {
    temp_rel_y = -temp_rel_y; //+temp_h;
  }
  Point2f temp_rel = Point2f(temp_rel_x, temp_rel_y);
  Point2f srcTri[3]; // point 2f object for input file
  srcTri[0] = Point2f(0.f, 0.f);
  srcTri[1] = Point2f(0, temp_h); // Before transformation selecting points
  srcTri[2] = Point2f(temp_w, 0);

  // LOGI("temp_rel     :%f,%f",  temp_rel.x,temp_rel.y);
  // LOGI("objPos       :%f,%f",  objPos.x,objPos.y);
  for (int i = 0; i < 3; i++)
  {
    // LOGI("srcTri   [%d]:%f,%f",i,srcTri[i].x,srcTri[i].y);
    srcTri[i] += temp_rel;
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
    srcTri[i] = rotate2d(srcTri[i], angRad);
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
    srcTri[i] += objPos;
    // LOGI("      >>>>>>>:%f,%f",  srcTri[i].x,srcTri[i].y);
  }

  if (ret_center != NULL)
  {
    *ret_center = (srcTri[1] + srcTri[2]) / 2;
  }

  Point2f dstTri[3]; // point 2f object for destination file
  dstTri[0] = Point2f(0.f, 0.f);
  dstTri[1] = Point2f(0, yMult * temp_h / downSamp); // Before transformation selecting points
  dstTri[2] = Point2f(temp_w / downSamp, 0);
  Mat warp_mat = getAffineTransform(srcTri, dstTri); // apply an affine transforation to image and storing it
  Mat warp_dst = Mat::zeros(temp_h / downSamp, temp_w / downSamp, srcImg.type());
  warpAffine(srcImg, warp_dst, warp_mat, warp_dst.size());

  return warp_dst;
}




bool hasEnding(std::string const &fullString, std::string const &ending)
{
  if (fullString.length() >= ending.length())
  {
    return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
  }
  else
  {
    return false;
  }
}




// Function to warp the target image based on initial pose
Mat warpImage(const Mat& img, const Point2f& offset, double rotation,float scale=1,bool reverse=false) {

    Mat warpMat;
    if(reverse==false)
    {

        Mat scaleMat=cvM3x3::scale(scale);
        Mat rotationMat=cvM3x3::rotate(rotation);
        Mat translationMat=cvM3x3::translate(offset);
        warpMat=cvM3x3::mat33to23(translationMat*rotationMat*scaleMat);
    }
    else
    {//an inverse mat of above basically
        Mat scaleMat=cvM3x3::scale(1/scale);
        Mat rotationMat=cvM3x3::rotate(-rotation);
        Mat translationMat=cvM3x3::translate(-offset);
        warpMat=cvM3x3::mat33to23(scaleMat*rotationMat*translationMat);
    }

    Mat warpedImg;
    cv::warpAffine(img, warpedImg, warpMat, img.size());
    return warpedImg;
}



int DBG_iterCount=0;
// Function to perform subpixel template matching
Point2f templateMatchSubpixel(const Mat& templateROI, const Mat& searchROI,float &ret_confidence) {
       Mat result;

    Mat _searchROI;

    if(searchROI.channels()!=templateROI.channels())
    {
      if(templateROI.channels()==1)
      {
        cvtColor(searchROI, _searchROI, COLOR_BGR2GRAY);
      }
      else if(templateROI.channels()==3)
      {
        cvtColor(searchROI, _searchROI, COLOR_BGR2GRAY);
      }
      else
      {
        _searchROI=searchROI;
      }
    }
    else
    {
        _searchROI=searchROI;
    }

    matchTemplate(_searchROI, templateROI, result, TM_CCOEFF_NORMED);
    
    // Find the maximum location first
    double minVal, maxVal;
    Point minLoc, maxLoc;
    minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    LOGE("--------maxVal:%f",maxVal);
    // static int counter=0;
    // cv::imwrite("data/result"+to_string(counter)+".png",result*255);
    // counter++;
    Point2f subPixelLoc = maxLoc;

    {
        
    }
    if (maxLoc.x > 0 && maxLoc.x < result.cols-1 && 
        maxLoc.y > 0 && maxLoc.y < result.rows-1) {
        
        // Get neighboring values
        float x0 = result.at<float>(maxLoc.y, maxLoc.x-1);
        float x1 = result.at<float>(maxLoc.y, maxLoc.x);
        float x2 = result.at<float>(maxLoc.y, maxLoc.x+1);
        float y0 = result.at<float>(maxLoc.y-1, maxLoc.x);
        float y1 = result.at<float>(maxLoc.y, maxLoc.x);
        float y2 = result.at<float>(maxLoc.y+1, maxLoc.x);
        
        // Quadratic interpolation for x and y independently
        float deltaX = (x2 - x0) / (2 * (2*x1 - x2 - x0));
        float deltaY = (y2 - y0) / (2 * (2*y1 - y2 - y0));
        
        // Update location with subpixel refinement
        if (isfinite(deltaX) && abs(deltaX) < 1)
            subPixelLoc.x += deltaX;
        if (isfinite(deltaY) && abs(deltaY) < 1)
            subPixelLoc.y += deltaY;
    }

    ret_confidence=maxVal;
    if(maxVal<0.2)
    {
        return subPixelLoc;
    }


    if(0){

        float concentration=0;
        // Analyze correlation peak shape using PCA
        vector<Point3f> points;
        float threshold = 0.1;  // Adjust threshold to capture peak shape

        // Collect points for PCA
        for(int y = 0; y < result.rows; y++) {
            for(int x = 0; x < result.cols; x++) {
                float val = result.at<float>(y, x);
                if(val > threshold) {

                    // Normalize weights to be between 0 and 1
                    float normalized_weight = (val - threshold) / (maxVal - threshold);
                    points.push_back(Point3f(x, y, normalized_weight));
                }
            }
        }

        // Only proceed if we have enough points
        if(points.size() >= 3) {
            // Compute weighted mean
            Point2f mean(0, 0);
            float totalWeight = 0;
            for(const auto& p : points) {
                mean += Point2f(p.x, p.y) * p.z;
                totalWeight += p.z;
            }
            mean = mean * (1.0f/totalWeight);

            // Compute weighted covariance matrix
            float cxx = 0, cyy = 0, cxy = 0;
            for(const auto& p : points) {
                float dx = p.x - mean.x;
                float dy = p.y - mean.y;
                float w = p.z/totalWeight;
                cxx += dx * dx * w;
                cyy += dy * dy * w;
                cxy += dx * dy * w;
            }

            // Compute eigenvalues
            float trace = cxx + cyy;
            float det = cxx * cyy - cxy * cxy;
            float lambda1 = trace/2 + sqrt((trace*trace/4) - det);  // larger eigenvalue
            float lambda2 = trace/2 - sqrt((trace*trace/4) - det);  // smaller eigenvalue
            
            // Calculate sigmas and direction
            float sigma1 = sqrt(lambda1);
            float sigma2 = sqrt(lambda2);

            float normalized_sigma1, normalized_sigma2;


            float max_theoretical_sigma = (searchROI.cols)/sqrt(12.0f);  // where N is your template width
            float threshold_sigma=max_theoretical_sigma*0.7;
            {


                // Normalized sigmas using 1/(1-x) for continuous transition to infinity
                float ratio = sigma1/threshold_sigma;
                if(ratio>0.99999)ratio=0.99999;
                normalized_sigma1 = ratio / (1.0f - ratio);  // approaches inf as ratio approaches 1

                ratio = sigma2/threshold_sigma;
                if(ratio>0.99999)ratio=0.99999;
                normalized_sigma2 = ratio / (1.0f - ratio);  // approaches inf as ratio approaches 1

            }

            std::cout << "Normalized Sigmas:" << endl;
            std::cout << "  Major: " << normalized_sigma1 << endl;
            std::cout << "  Minor: " << normalized_sigma2 << endl;
            std::cout << "  threshold_sigma: " << threshold_sigma << endl;

            float principal_direction = atan2(lambda1 - cxx, cxy) * 180/CV_PI;
            concentration = 1/(normalized_sigma2>normalized_sigma1?normalized_sigma2:normalized_sigma1);  // Lower value means more concentrated

            // std::cout << "Match Quality Metrics:" << endl;
            // std::cout << "  Peak Value: " << maxVal << endl;
            // std::cout << "  Concentration (σ₂/σ₁): " << concentration 
            //     << " (closer to 0 means more concentrated)" << endl;
            // std::cout << "  Major Sigma: " << sigma1 << endl;
            // std::cout << "  Minor Sigma: " << sigma2 << endl;
            // std::cout << "  Principal Direction: " << principal_direction << "°" << endl;
        }
        std::cout << "=====concentration:"<<concentration << endl;
        concentration*=50;
        if(concentration>1)concentration=1;
        maxVal*=concentration;
        
        static int counter=0;
        // cv::imwrite("data/result"+to_string(counter)+".png",result*255);
        counter++;
        // exit(0);
    }

    ret_confidence=maxVal;

    return subPixelLoc;
}


Mat Local_Contrast_Normalization(const Mat& input) {
    Mat processed;
    
    // 1. Convert to floating point
    input.convertTo(processed, CV_32F, 1.0/255.0);
    
    // 2. Apply local contrast normalization
    Mat mean, stddev;
    int ksize = 21; // Adjust kernel size as needed
    GaussianBlur(processed, mean, Size(ksize, ksize), 0);
    
    Mat squared;
    multiply(processed, processed, squared);
    GaussianBlur(squared, stddev, Size(ksize, ksize), 0);
    subtract(stddev, mean.mul(mean), stddev);
    sqrt(stddev, stddev);
    
    // Avoid division by zero
    stddev += 1e-5;
    
    // Normalize
    subtract(processed, mean, processed);
    divide(processed, stddev, processed);
    
    // 3. Scale back to original range (optional)
    processed = (processed + 3) * (255.0/6.0); // Adjust scaling factors as needed
    processed.convertTo(processed, CV_8U);
    
    return processed;
}


// Function to refine pose using template matching
float refinePoseWithTemplateMatching(
    const Mat& targetImg,
    const vector<InspectionTarget_Orientation_ShapeBasedMatching::refine_region_info>& refine_region_set,
    float rec_scale,//incoming shape how much larger than the template
    Point2f& initOffset,
    double& initRotation,
    int searchBorder=25,
    float confidence_threshold=0.7,
    bool useOptFlow=false
)
{
    // Refined offset and rotation
    Point2f refinedOffset(0, 0);
    double refinedRotation = 0;
    int numPoints = 0;

    vector<Point2f> allValidTemplatePoints, allValidTargetPoints;

    float min_confidence=numeric_limits<float>::max();
    for (const auto& refSegInfo : refine_region_set) {
        // Extract template ROI

        auto roi=refSegInfo.regionInRef;

        Rect ex_roi=roi;

        // if(!useOptFlow)
        // {
        //     ex_roi=Rect(roi.x-searchBorder, roi.y-searchBorder, 
        //                         roi.width+2*searchBorder, roi.height+2*searchBorder);
        // }

        if(!useOptFlow)
        {
            ex_roi=Rect(roi.x+searchBorder, roi.y+searchBorder, 
                                roi.width-2*searchBorder, roi.height-2*searchBorder);
        }




        // Calculate the transformation for this ROI
        Mat translationMat = cvM3x3::translate(-initOffset);
        Mat rotationMat = cvM3x3::rotate(-initRotation);

        Mat ROItranslationMat = cvM3x3::translate(Point2f(-ex_roi.x,-ex_roi.y));
        Mat scaleMat=cvM3x3::scale(1/rec_scale);//scale the incoming shape to the template size
        Mat warpMat3x3=ROItranslationMat*scaleMat *  rotationMat*translationMat;//map source image to template orientation
        Mat warpMat = cvM3x3::mat33to23(warpMat3x3);//map source image to template orientation
        // Adjust the transformation matrix to account for ROI offset

        // Create warped ROI directly
        Mat warpedTargetROI;
        cv::warpAffine(targetImg, warpedTargetROI, warpMat, ex_roi.size());


        // static int count=0;
        // imwrite("data/tempRef"+to_string(count)+".png",refSegInfo.img);
        // imwrite("data/imgeWrp"+to_string(count)+".png",warpedTargetROI);
        // count++;
        // Extract expanded template ROI
        // Mat expandedTemplateROI = refSegInfo.img;
        
        float current_match_score=0;

        Point2f displacement;

        if(useOptFlow==false)
        {
            // Perform template matching with subpixel refinement
            Point2f matchLoc = templateMatchSubpixel(warpedTargetROI, refSegInfo.img,current_match_score);

            cout<<"matchLoc:"<<matchLoc<<endl;
            displacement=-Point2f( matchLoc.x-searchBorder, matchLoc.y-searchBorder);
        }
        else 
        {
            Mat templateROI = refSegInfo.img;
            vector<Point2f> templatePoints;
            // Add multiple points instead of just the center
            // for(int y = templateROI.rows/4; y < templateROI.rows*3/4; y += templateROI.rows/4) {
            //     for(int x = templateROI.cols/4; x < templateROI.cols*3/4; x += templateROI.cols/4) {
            //         templatePoints.push_back(Point2f(x, y));
            //     }
            // }

            templatePoints.push_back(Point2f(templateROI.cols/2, templateROI.rows/2));//center of the template
            
            vector<Point2f> targetPoints;
            vector<uchar> status;
            vector<float> err;
            
            // Increase window size for better accuracy
            int winSize = std::min(templateROI.cols, templateROI.rows) / 2;
            winSize = std::max(21, winSize); // Larger minimum window
            winSize = winSize | 1; // Make sure it's odd
            
            // Mat preprocessedTemplate = Local_Contrast_Normalization(templateROI);
            // Mat preprocessedTarget = Local_Contrast_Normalization(warpedTargetROI);


            // Mat preprocessedTemplate;
            // equalizeHist(templateROI, preprocessedTemplate);
            // Mat preprocessedTarget;
            // equalizeHist(warpedTargetROI, preprocessedTarget);
            Mat preprocessedTemplate=templateROI;
            Mat preprocessedTarget=warpedTargetROI;

            // imwrite("data/preprocessedTemplate"+to_string(count)+".png",preprocessedTemplate);
            // imwrite("data/preprocessedTarget"+to_string(count)+".png",preprocessedTarget);  

            calcOpticalFlowPyrLK(preprocessedTemplate, preprocessedTarget, templatePoints, targetPoints, status, err,
                Size(winSize, winSize), 
                5, // Increase pyramid levels (was 3)
                TermCriteria(TermCriteria::COUNT+TermCriteria::EPS, 1000, 0.01), // More iterations, tighter epsilon
                OPTFLOW_LK_GET_MIN_EIGENVALS, // Add this flag for better feature tracking
                1e-6); // Smaller min eigenvalue threshold for better accuracy
            // Average all valid points for more robust matching
            Point2f avgDisplacement(0, 0);
            float totalWeight = 0;
            int validPoints = 0;
            
            for(size_t i = 0; i < templatePoints.size(); i++) {
                if(status[i]) {
                    float weight = 1.0f / (err[i] + 1e-6); // Weight by inverse error
                    avgDisplacement += (targetPoints[i] - templatePoints[i]) * weight;
                    totalWeight += weight;
                    validPoints++;
                }
            }
            
            if(validPoints > 0) {
                displacement = avgDisplacement * (1.0f / totalWeight);
                // Adjust confidence based on number of valid points and average error
                current_match_score = (float)validPoints / templatePoints.size();
                current_match_score *= 2.0f; // Scale up confidence (adjust as needed)
            } else {
                current_match_score = 0.0f;
            }
        }
        
        cout<<"displacement:"<<displacement<<endl;
        cout<<"current_match_score:"<<current_match_score<<endl;


        // float score_zero_threshold=0.2;
        // float current_confidence=(current_match_score-score_zero_threshold)/(1-score_zero_threshold);
        // current_confidence=current_confidence>0?current_confidence:0;
        float current_confidence=current_match_score;
        if(current_confidence<confidence_threshold)
        {
            printf("conf:%f SKIP\n",current_confidence);
            continue;//skip this roi
        }
        // Convert match location to relative displacement
        
        // Add points for affine estimation
        Point2f templatePoint(roi.x + roi.width/2.0f, roi.y + roi.height/2.0f);
        Point2f targetPoint = templatePoint + displacement;
        
        
        {
            if(min_confidence>current_confidence)
            {
                min_confidence=current_confidence;
            }
            allValidTemplatePoints.push_back(templatePoint);
            allValidTargetPoints.push_back(targetPoint);

        }

    }

    // the offset result is in template coordinate

    //So we need to map back to the target object coordinate in incoming image 


    // Perform single affine estimation with all collected points
    if (allValidTemplatePoints.size() >= 2) {
        Mat transform = estimateAffinePartial2D(allValidTemplatePoints, allValidTargetPoints);
        
        if (!transform.empty()) {
            // Decompose transformation matrix into rotation and translation
            double dx = transform.at<double>(0, 2);
            double dy = transform.at<double>(1, 2);
            double theta = atan2(transform.at<double>(1, 0), transform.at<double>(0, 0));

            initRotation += theta;

            Point2f approach_offset=Point2f(dx, dy)*rec_scale;//scale the offset by the rec_scale
            //rotate approach_offset by theta
            approach_offset=rotate2d(approach_offset,initRotation);//
            // Update the initial pose
            initOffset += approach_offset;


        }
    }
    else if(allValidTemplatePoints.size()==1)
    {
        initRotation+=0;//no rotation adjustment
        Point2f approach_offset=(allValidTargetPoints[0]-allValidTemplatePoints[0])*rec_scale;
        approach_offset=rotate2d(approach_offset,initRotation);
        initOffset+=approach_offset;
    }
    else
    {
        return 0;//no valid points
    }

    return min_confidence;
}

vector<StageInfo_Orientation::orient> MatchPoseRefine(
  cv::Mat &CV_srcImg,
  std::vector<line2Dup::Match> &matches,
  SBM_if *sbm,vector<int> &idxs,
  vector<InspectionTarget_Orientation_ShapeBasedMatching::refine_region_info> &refine_region_set,
  float matching_downScale,
  float recScale,//incoming shape how much larger than the template
  float refine_score_thres,
  float origin_offset_angle,
  bool  refine_angle_only,
  bool  must_refine_result,
  bool remove_refine_failed_result)
{
  vector<StageInfo_Orientation::orient> retOrient;
  for (int i = 0; i < idxs.size(); i++)
  {
    auto idx = idxs[i];
    line2Dup::Match match = matches[idx];
    if (match.similarity == 0) // for regional search, that needs to give a yes/no answer
    {
      continue;
    }

    auto templ = sbm->detector.getTemplates(match.class_id, match.template_id);

    // calc the position relative to the first point
    cv::Point2f f0Pt = cv::Point2f((float)templ[0].features[0].x + match.x, (float)templ[0].features[0].y + match.y) / matching_downScale;
    SBM_if::anchorInfo Aoffset = sbm->fetchTemplateOffset(match.class_id);
    // LOGI("[%d]>>>ang:%f <<id:%s",i,templ[0].angle,match.class_id.c_str());
    cv::Point2f anchorPt = rotate2d(Aoffset.offset, templ[0].angle * M_PI / 180);

    float offset = (2 / matching_downScale);
    f0Pt += cv::Point2f(offset, offset);
    anchorPt += f0Pt;

    float refine_score = 0;
    float refinedAngleRad = templ[0].angle * M_PI / 180;

    std::string DBG_STR;
    // LOGI("refine_score_thres:%f must_refine_result:%d",refine_score_thres,must_refine_result);
    // LOGE(">>matching_downScale:%f",matching_downScale);




    int refineCount = 3;

    {
      bool y_flip = hasEnding(match.class_id, "_f");
      int border_search_size = (int)(25 + (2 / matching_downScale))*1;
      bool allowMatchingOnSearchRegionEdge = refine_angle_only;


      double initRotation = refinedAngleRad;
      cv::Point2f initOffset = anchorPt;

      
      float confidence_threshold=0.2;
      // Iterative refinement loop

      const double CONVERGENCE_THRESHOLD = 0.1; 

      Point2f updatedOffset = initOffset;
      double updatedRotation = initRotation;
      LOGE("refine_region_set.size():%d",refine_region_set.size());
      if(refine_region_set.size()>0)
      {
        Point2f prevOffset = initOffset;
        double prevRotation = initRotation;
        for (int iter = 0; iter < refineCount+1; iter++) {
            DBG_iterCount=iter;
            float min_confidence=refinePoseWithTemplateMatching(
                CV_srcImg, 
                refine_region_set, 
                recScale,
                updatedOffset, 
                updatedRotation,
                border_search_size,
                confidence_threshold,
                false

            );
            if(min_confidence<confidence_threshold)
            {
                break;
            }


            refine_score=min_confidence;
            // Check for convergence
            double offsetDiff = norm(prevOffset - updatedOffset);
            double rotationDiff = abs(prevRotation - updatedRotation);
            
            cout << "Iteration " << iter + 1 << ":" << endl;
            cout << "  Offset: (" << updatedOffset.x << ", " << updatedOffset.y << ")" << endl;
            cout << "  Rotation (degrees): " << updatedRotation * 180.0 / CV_PI << endl;
            cout << "  Min Confidence: " << min_confidence << endl;
            if (offsetDiff < CONVERGENCE_THRESHOLD && rotationDiff < CONVERGENCE_THRESHOLD) {
                cout << "Converged after " << iter + 1 << " iterations" << endl;
                break;
            }
            
            prevOffset = updatedOffset;
            prevRotation = updatedRotation;
        }
        




        anchorPt=updatedOffset;
        refinedAngleRad=updatedRotation;


        if (refine_angle_only)
          anchorPt = initOffset; // ignore the refined location if needed


      }
      // LOGI(" %f =>  %f",refinedAngleRad*180/M_PI,tmpAngle*180/M_PI);
      // LOGI(" %f,%f, a:%f  =>  %f,%f a:%f ",anchorPt.x,anchorPt.y,refinedAngleRad*180/M_PI,tmp_anchorPt.x,tmp_anchorPt.y,tmpAngle*180/M_PI);

      if (refine_score > refine_score_thres || refine_region_set.size()==0)
      { // accept the refinement
      }
      else if (must_refine_result == true) // for regional search, that needs to give yes/no answer
      {
        if (remove_refine_failed_result == true)
          continue;
        StageInfo_Orientation::orient orie;

        orie.angle = refinedAngleRad;
        orie.flip = hasEnding(match.class_id, "_f");;
        orie.center = anchorPt;
        orie.confidence = 0;

        retOrient.push_back(orie);//push confidence 0 result
        continue;

        // refinedAngleRad=tmpAngle;
        // anchorPt=tmp_anchorPt;
        // match.similarity=0.1;
      }








    }










    if (refine_score > 0.999)//
      refine_score = 0.999;

    // LOGI("[%d]----------refine_score:%f  must_refine_result:%d",i,refine_score,must_refine_result);
    StageInfo_Orientation::orient orie;

    orie.angle = refinedAngleRad + origin_offset_angle;

    // LOGE("refAng:%f + offset:%f = res:%f",refinedAngleRad*180/3.14159,origin_offset_angle*180/3.14159,orie.angle*180/3.14159);
    orie.flip = hasEnding(match.class_id, "_f");
    orie.center = {anchorPt.x, anchorPt.y};
    orie.confidence = round(match.similarity) + refine_score; // HACK to store refine info

    retOrient.push_back(orie);
  }

  return retOrient;

}








template<typename _Tp> static inline
static double  jaccardDistance__(const Rect_<_Tp>& a, const Rect_<_Tp>& b) {
    _Tp Aa = a.area();
    _Tp Ab = b.area();

    if ((Aa + Ab) <= std::numeric_limits<_Tp>::epsilon()) {
        // jaccard_index = 1 -> distance = 0
        return 0.0;
    }

    float Aab = (a & b).area();
    float dist=1.0f - Aab / (Aa + Ab - Aab);

    return dist;
}




template <typename T>
inline float rectOverlap_angle(const T& a, const T& b,void* ctx)
{
  float *angle_diff_thres = (float*)ctx;
  if(a.isFlip!=b.isFlip)
  {
    return 0.f;
  }
  if(*angle_diff_thres>0)
  {
    float angle_diff = fmod(fabs(a.angle_deg - b.angle_deg), 360.0f);
    if (angle_diff > 180.0f) {
        angle_diff = 360.0f - angle_diff;
    }
    if(angle_diff>*angle_diff_thres)//large angle diff, consider as no overlap
    {
        return 0.f;
    }
  }




    
    return 1.f - static_cast<float>(jaccardDistance__(a.rect, b.rect));
}
void CloseMatchFilter(std::vector<line2Dup::Match> &matches,SBM_if *sbm,vector<int> &idxs,float matching_angle_apart)
{

  vector<cv_dnn::NMSBoxesStruct> boxes;
  vector<float> scores;

  for (auto match : matches)
  {
    cv_dnn::NMSBoxesStruct box;
    box.rect.x = match.x;
    box.rect.y = match.y;
    box.isFlip = hasEnding(match.class_id, "_f");
    auto templ = sbm->detector.getTemplates(match.class_id,
                                            match.template_id);

    
    // LOGE(">>>>>match.class_id:%s  wh:%d,%d",match.class_id.c_str(),templ[0].width,templ[0].height);
    box.rect.width = templ[0].width;
    box.rect.height = templ[0].height;
    box.angle_deg = templ[0].angle;
    boxes.push_back(box);
    scores.push_back(match.similarity);
  }

  // cv_dnn::NMSBoxes(boxes, scores, 0, 0.7f, idxs,0.8);
  float angle_diff_thres = matching_angle_apart;
  cv_dnn::NMSFast_(boxes, scores, 0, 0.7f,0.8,0, idxs,rectOverlap_angle,&angle_diff_thres);




  std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
    int sa=matches[a].y*10+matches[a].x;//tilt the score a bit, assume the arrangment is like a grid
    int sb=matches[b].y*10+matches[b].x;
    return sa < sb;
  });


}



void InspectionTarget_Orientation_ShapeBasedMatching::singleProcess(shared_ptr<StageInfo> sinfo)
{

  std::lock_guard<std::mutex> lock_(process_lock);
  LOGI(">>>>>>>LOCK>>>>>");

  if(sinfo->get_trigger_id()>0)
  {

    
    // LOGI("recentSrcStageInfoSet:%d  recentSrcStageInfoSetIdx:%d ",recentSrcStageInfoSet.size(),recentSrcStageInfoSetIdx.space());
    // LOGI(">>>>fetch save>>>>sinfo->trigger_id:%d",sinfo->trigger_id);
    std::lock_guard<std::mutex> lock(recentSrcLock); 

  

    if(recentSrcStageInfoSetIdx.space()==0)
    {//if full, wipe tail
      int tail_idx = recentSrcStageInfoSetIdx.getTail();
      recentSrcStageInfoSetIdx.consumeTail();
    }
    
  

    {//push new info in head
      int head_idx = recentSrcStageInfoSetIdx.getHead();
  
      recentSrcStageInfoSet[head_idx]=sinfo;
  
      recentSrcStageInfoSetIdx.pushHead();

    }
  }

  


  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  int64 t0 = cv::getTickCount();
  cache_latest_input = sinfo;
  // LOGI(">>>>>>>>InspectionTarget_Orientation_ShapeBasedMatching>>>>>>>>");
  // LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());

  Mat _CV_srcImg=sinfo->img;

  //crop CV_srcImg to make width and height multiples of 8

  float im_scale=this->template_mmpp/sinfo->img_prop.mmpp;//incoming image Zoom in scale, big im_scale means the target shape is bigger than the template

  LOGE("im_scale>>>>:%f  mmpp:%f  pixel_size_mm:%f",im_scale,sinfo->img_prop.mmpp,sinfo->img_prop.fi.pixel_size_mm);

  RegisterTemplate(im_scale);
  

  cv::Size size_origin = _CV_srcImg.size();






  cv::Size size_sd;
  size_sd.width = ((int)(size_origin.width * matching_downScale)) / 8 * 8;
  size_sd.height = ((int)(size_origin.height * matching_downScale)) / 8 * 8;




  cv::Size size_crop;

  size_crop.width = (int)(size_sd.width/matching_downScale);
  size_crop.height = (int)(size_sd.height/matching_downScale);



  Mat CV_srcImg=_CV_srcImg(Rect(0,0,size_crop.width,size_crop.height));

  Mat CV_srcImg_ds;

  if(matching_downScale==1)
  {
    CV_srcImg_ds=CV_srcImg;
  }
  else
  {
    resize(CV_srcImg, CV_srcImg_ds, size_sd, cv::INTER_AREA);
  }

  // cv::cvtColor(CV_srcImg_color, CV_srcImg_ds, cv::COLOR_BGR2GRAY);
  

  float magThres_eq_alpha = 0.3;
  float magnitude_thres = JFetch_NUMBER_ex(def, "magnitude_thres", 20) / (magThres_eq_alpha + (1 - magThres_eq_alpha) * matching_downScale);
  if (magnitude_thres > 128)
    magnitude_thres = 128;

  
  std::vector<line2Dup::Match> matches;

  bool regional_most_similar_match = JFetch_TRUE(def, "regional_most_similar_match");

  
  double refine_score_thres = JFetch_NUMBER_ex(def, "refine_score_thres", 0.5);
  bool must_refine_result = JFetch_TRUE(def, "must_refine_result");
  bool remove_refine_failed_result = JFetch_TRUE(def, "remove_refine_failed_result");

  LOGI("refine_score_thres:%f must_refine_result:%d  remove_refine_failed_result:%d",refine_score_thres,must_refine_result,remove_refine_failed_result);
  vector<int> idxs;

  
  shared_ptr<StageInfo_Orientation> reportInfo(new StageInfo_Orientation());
  {
    reportInfo->clear_report();
    cJSON *search_regions = JFetch_ARRAY(def, "search_regions");
    float similarity_thres = JFetch_NUMBER_ex(def, "similarity_thres", 60);



    bool mask_enable=JFetch_TRUE(def,"mask.enable");

          
    cJSON* gval=belongMan->getNLockGlobalValue();
    double l_h=DFetch_NUMBER_ex(def,"mask.lh",0,gval);
    double l_s=DFetch_NUMBER_ex(def,"mask.ls",0,gval);
    double l_v=DFetch_NUMBER_ex(def,"mask.lv",0,gval);

    double h_h=DFetch_NUMBER_ex(def,"mask.hh",180,gval);
    double h_s=DFetch_NUMBER_ex(def,"mask.hs",255,gval);
    double h_v=DFetch_NUMBER_ex(def,"mask.hv",255,gval);



    int blur1_size=DFetch_NUMBER_ex(def,"mask.blur1_size",25,gval);
    double thres1_l=DFetch_NUMBER_ex(def,"mask.thres1_l",130,gval);
    double thres1_h=DFetch_NUMBER_ex(def,"mask.thres1_h",255,gval);
    int blur2_size=DFetch_NUMBER_ex(def,"mask.blur2_size",25,gval);
    double thres2_l=DFetch_NUMBER_ex(def,"mask.thres2_l",1,gval);
    double thres2_h=DFetch_NUMBER_ex(def,"mask.thres2_h",255,gval);
    belongMan->unLockGlobalValue();

   
    if (search_regions && cJSON_GetArraySize(search_regions))
    {

      int arrL = cJSON_GetArraySize(search_regions);
      for (int i = 0; i < arrL; i++)
      {

        cJSON *region_info = cJSON_GetArrayItem(search_regions, i);

        int x = (int)(JFetch_NUMBER_ex(region_info, "x", 0) * matching_downScale);
        int y = (int)(JFetch_NUMBER_ex(region_info, "y", 0) * matching_downScale);
        int w = (int)(JFetch_NUMBER_ex(region_info, "w", 0) * matching_downScale);
        int h = (int)(JFetch_NUMBER_ex(region_info, "h", 0) * matching_downScale);

        {
          Mat &m = CV_srcImg_ds;

          if (x < 0)
          {
            w += x;
            x = 0;
          }
          if (y < 0)
          {
            h += y;
            y = 0;
          }
          if (x >= m.cols || y >= m.rows)
            continue;

          if (x + w > m.cols)
          {
            w = m.cols - x;
          }
          if (y + h > m.rows)
          {
            h = m.rows - y;
          }

          int margin = 10;
          if (w <= margin || h <= margin)
            continue;
        }

        Mat CV_srcImg_region = CV_srcImg_ds(Rect(x, y, w, h));


   
        //apply color based(HSV) mask first to avoid background noise.... kind of
        if(mask_enable)
        {
          // make a HSV filtered mask to remove the background
          Mat CV_srcImg_region_hsv;
          cv::cvtColor(CV_srcImg_region, CV_srcImg_region_hsv, cv::COLOR_BGR2HSV);

   
   
          Mat img_HSV_threshold;
          {



            Scalar rangeH=Scalar(h_h,h_s,h_v);
            Scalar rangeL=Scalar(l_h,l_s,l_v);

            Mat img_HSV_range;
            inRange(CV_srcImg_region_hsv, rangeL, rangeH, img_HSV_range);


   
            cv::blur(img_HSV_range,img_HSV_range,cv::Size(blur1_size,blur1_size));
            threshold(img_HSV_range, img_HSV_range, thres1_l , thres1_h, THRESH_BINARY);


            // cv::imwrite("data/img_HSV_range_"+id+"_"+std::to_string(i)+".jpg",img_HSV_range);

            cv::blur(img_HSV_range,img_HSV_range,cv::Size(blur2_size,blur2_size));


   

            threshold(img_HSV_range, img_HSV_threshold, thres2_l, thres2_h, THRESH_BINARY);

          }

          //and the mask to the image

          Mat CV_srcImg_region_masked;
          CV_srcImg_region.copyTo(CV_srcImg_region_masked,img_HSV_threshold);

          //copy CV_srcImg_region_masked to CV_srcImg_region
          CV_srcImg_region=CV_srcImg_region_masked;

          //save the mask for debugging
          // if(1)
          // {
          //   cv::imwrite("data/mask_"+id+"_"+std::to_string(i)+".jpg",img_HSV_threshold);
          //   cv::imwrite("data/masked_"+id+"_"+std::to_string(i)+".jpg",CV_srcImg_region);
          // }


        }

   


  LOGI(">>>>>i:%d>>>",i);
        std::vector<line2Dup::Match> sub_matches = sbm->detector.match(
            CV_srcImg_region,
            similarity_thres,
            magnitude_thres,
            {template_class_name, template_class_name + "_f"});

  LOGI(">>>>>i:%d>>>",i);
        LOGI("sub_matches.size():%d", sub_matches.size());
        vector<int> SubIdxs;
        if(1)
        {
          CloseMatchFilter(sub_matches,sbm,SubIdxs,matching_angle_apart);
        }
        else
        {
          for (int i = 0; i < sub_matches.size(); i++)
          {
            SubIdxs.push_back(i);
          }
        }

        LOGI("SubIdxs.size():%d", SubIdxs.size());

        for(auto &index:SubIdxs)
        {
          sub_matches[index].x+=x;
          sub_matches[index].y+=y;
        }
        vector<StageInfo_Orientation::orient> orientList;
        orientList =MatchPoseRefine(
          CV_srcImg,
          sub_matches,
          sbm,
          SubIdxs,
          refine_region_set,
          matching_downScale,
          im_scale,
          refine_score_thres,
          origin_offset_angle,
          false,
          must_refine_result,
          remove_refine_failed_result);

        if(regional_most_similar_match)
        {//keep the most similar(confident) one
          if(orientList.size()>0)
          {
            float maxScore=orientList[0].confidence;
            int maxIdx=0;
            for(int i=1;i<orientList.size();i++)
            {
              if(orientList[i].confidence>maxScore)
              {
                maxScore=orientList[i].confidence;
                maxIdx=i;
              }
            }
            reportInfo->push_report_object(orientList[maxIdx]);
          }
          else
          {
            //construct a place holder result
            StageInfo_Orientation::orient orie;
                      
            orie.angle = 0;
            orie.flip = false;
            orie.center = {0, 0};
            orie.confidence = -1;
            reportInfo->push_report_object(orie);
          }





        }
        else
        {

          for(auto &orie:orientList)
          {
            reportInfo->push_report_object(orie);
          }
        }

      }
    
  
    }
    else
    {
      auto t0=cv::getTickCount();
      matches = sbm->detector.match(
          CV_srcImg_ds,
          similarity_thres,
          magnitude_thres,
          {template_class_name, template_class_name + "_f"});

      int64 t1=cv::getTickCount();
      double secs_us = 1000000 * (t1 - t0) / cv::getTickFrequency();
      LOGI(">>>>sbm->detector.match>>>>process_time_us:%f", secs_us);

      
      vector<int> SubIdxs;
      CloseMatchFilter(matches,sbm,SubIdxs,matching_angle_apart);
      vector<StageInfo_Orientation::orient> orientList =MatchPoseRefine(
        CV_srcImg,
        matches,
        sbm,
        SubIdxs,
        refine_region_set,
        matching_downScale,
        im_scale,
        refine_score_thres,
        origin_offset_angle,
        false,
        must_refine_result,
        remove_refine_failed_result);

  

      if(regional_most_similar_match)
      {//keep the most similar(confident) one
        if(orientList.size()>0)
        {
          float maxScore=orientList[0].confidence;
          int maxIdx=0;
          for(int i=1;i<orientList.size();i++)
          {
            if(orientList[i].confidence>maxScore)
            {
              maxScore=orientList[i].confidence;
              maxIdx=i;
            }
          }
          reportInfo->push_report_object(orientList[maxIdx]);
        }
      }
      else
      {
        for(auto &orie:orientList)
        {
          reportInfo->push_report_object(orie);
        }
      }
    }

  }

  {
    LOGI(">>>>>>>>process_time_us:%f", 1000000 * (cv::getTickCount() - t0) / cv::getTickFrequency());
  }




  reportInfo->set_mmpp(sinfo->img_prop.mmpp);
  reportInfo->source = this;
  reportInfo->set_source_id(id);
  reportInfo->img_show =
      reportInfo->img = sinfo->img;
  reportInfo->set_trigger_id(sinfo->get_trigger_id());

  vector<string> tags;
  tags.push_back(id);
  insertInputTagsWPrefix(tags, sinfo->cached_trigger_tags, "s_");
  reportInfo->set_trigger_tags(tags);

  

  reportInfo->img_prop = sinfo->img_prop;
  reportInfo->img_prop.StreamInfo.channel_id = JFetch_NUMBER_ex(additionalInfo, "stream_info.stream_id", 0);
  reportInfo->img_prop.StreamInfo.downsample = JFetch_NUMBER_ex(additionalInfo, "stream_info.downsample", 10);
  LOGI("id:%s   reportInfo->orientation.size():%d  p:%p", id.c_str(), reportInfo->get_report_count(),reportInfo.get());

  {
    int64 t1 = cv::getTickCount();
    double secs_us = 1000000 * (t1 - t0) / cv::getTickFrequency();
    reportInfo->set_process_time_us(secs_us);
    reportInfo->create_time_sysTick = t1;
    // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

    LOGI(">>>>>>>>process_time_us:%f", secs_us);
  }

  // reportInfo->genJsonRepTojInfo();

  cache_latest_result = reportInfo;
  belongMan->dispatch(reportInfo);
}

InspectionTarget_Orientation_ShapeBasedMatching::~InspectionTarget_Orientation_ShapeBasedMatching()
{
  if (sbm)
    delete sbm;
}