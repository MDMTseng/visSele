#include "InspTar_DimMeasure.hpp"
#include "StageInfo.hpp"
#include <iostream>
#include <opencv2/video/tracking.hpp>
#include <acvImage_BasicTool.hpp>
#include <random>
#include <numeric>
#include <circleFitting.h>
#include <InspTarUtil.hpp>
#include "polyfit.h"
#include "cJsonPP.h"
using namespace cv;

using namespace std;

template <typename Base, typename T>
inline bool instanceof (const T)
{
  return is_base_of<Base, T>::value;
}

void InspectionTarget_DimMeasure::INIT(std::string id, cJSON *def, InspectionTargetManager *belongMan, std::string local_env_path)
{
  LOGE(">>>>");
  InspectionTarget::INIT(id, def, belongMan, local_env_path);
}

// bool solveTree(vector<InspectionTarget_DimMeasure::itemInfo> &infoList,int currentIdx,vector<InspectionTarget_DimMeasure::itemInfo> &ret_infoList,int depth=0)
// {
// 		if(depth>30){
// 			throw std::runtime_error("Recursive depth is too deep");
// 		}
// 		// LOGI("currentIdx:%d",currentIdx);
//     if(currentIdx==-1)return false;
//     if(currentIdx>=infoList.size())return false;
//     if(infoList[currentIdx].processed)return true;
//     for(int i=0;i<10;i++){//
// 			if(infoList[currentIdx].refIds[i]==-1)break;//no ref
// 			for(int j=0;j<infoList.size();j++){
// 				// LOGI("infoList[%d].id:%d   infoList[%d].refIds[%d]:%d",
// 				// 	j,infoList[j].id,
// 				// 	currentIdx,i,infoList[currentIdx].refIds[i]);
// 				if(infoList[j].id==infoList[currentIdx].refIds[i]){
// 						solveTree(infoList,j,ret_infoList,depth+1);
// 				}
// 			}
//     }
//     infoList[currentIdx].processed=true;// mark as processed at current level
//     ret_infoList.push_back(infoList[currentIdx]);
//     return true;
// }

// vector<InspectionTarget_DimMeasure::itemInfo> solveDependencyTree(cJSON *featureList)
// {

//     vector<InspectionTarget_DimMeasure::itemInfo> infoList;
//     int list_size=cJSON_GetArraySize(featureList);
//     infoList.reserve(list_size);
//     for(int i=0;i<list_size;i++){
//         cJSON *item=cJSON_GetArrayItem(featureList,i);
//         InspectionTarget_DimMeasure::itemInfo info;

//         int id=JFetch_NUMBER_ex(item,"id");
//         info.id=id;

//         cJSON *refList=JFetch_ARRAY(item,"ref");

// 				info.refIds[0]=-1;
//         if(refList==NULL){
//         }else{
//             int ref_size=cJSON_GetArraySize(refList);

// 						int widx=0;
//             for(int j=0;j<ref_size;j++){
// 							cJSON *refItem=cJSON_GetArrayItem(refList,j);
// 							if(refItem==NULL)break;

// 							int refId=JFetch_NUMBER_ex(refItem,"id",-999);
// 							if(refId!=-999){
// 								info.refIds[widx++]=refId;

// 							}
// 							else
// 							{
// 								LOGI("=========refItem is not a number");
// 								break;
// 							}
//             }
//             info.refIds[widx]=-1;//close the list
//         }
//         info.processed=false;
//         infoList.push_back(info);
//     }
// 		for(int i=0;i<infoList.size();i++){

// 				LOGI("=========id:%d",infoList[i].id);
// 				for(int j=0;j<10;j++){
// 					if(infoList[i].refIds[j]==-1)break;
// 					LOGI("%d",infoList[i].refIds[j]);
// 				}

// 		}

// 		LOGI("=========");
//     vector<InspectionTarget_DimMeasure::itemInfo> ret_infoList;
//     ret_infoList.reserve(infoList.size());
//     ret_infoList.clear();
//     for(int i=0;i<list_size;i++){

//         solveTree(infoList,i,ret_infoList);
//     }
//     return ret_infoList;

// }


// InspectionTarget_DimMeasure::FeatureType getFeatureType(cJSON *featureEle)
// {
//   string type=JFetch_STRING_ex(featureEle,"type");
//   if(type=="LineFit")return InspectionTarget_DimMeasure::FeatureType::TYPE_LineFit;
//   if(type=="SearchPoint")return InspectionTarget_DimMeasure::FeatureType::TYPE_SearchPoint;
//   if(type=="ArcFit")return InspectionTarget_DimMeasure::FeatureType::TYPE_ArcFit;
//   if(type=="Measure_Distance")return InspectionTarget_DimMeasure::FeatureType::TYPE_Measure_Distance;
//   if(type=="Calc")return InspectionTarget_DimMeasure::FeatureType::TYPE_Calc;
//   return InspectionTarget_DimMeasure::FeatureType::TYPE_NA;
// }




/**
 * Polynomial fitting function
 * 
 * @param x_data Pointer to x values array
 * @param y_data Pointer to y values array
 * @param w_data Pointer to weight values array (can be NULL for equal weights)
 * @param ndata Number of data points
 * @param order Order of polynomial to fit
 * @param coeffs Output array for coefficients (size should be order + 1)
 * @param x_stride Stride for x_data array (bytes between elements)
 * @param y_stride Stride for y_data array (bytes between elements)
 * @param w_stride Stride for w_data array (bytes between elements)
 * @return 0 if successful, negative value if error
 */
int polyfit_opencv(const void* x_data, const void* y_data, const void* w_data, 
            int ndata, int order, float* coeffs,
            int x_stride = sizeof(float), 
            int y_stride = sizeof(float),
            int w_stride = sizeof(float)) {
    
    if (ndata <= order) return -1; // Not enough data points
    
    // Create matrices for least squares solving
    cv::Mat A = cv::Mat::zeros(ndata, order + 1, CV_32F);
    cv::Mat b = cv::Mat::zeros(ndata, 1, CV_32F);
    cv::Mat w;
    
    if (w_data) {
        w = cv::Mat::zeros(ndata, 1, CV_32F);
    }
    
    // Fill matrices
    for (int i = 0; i < ndata; i++) {
        float x = *(float*)((char*)x_data + i * x_stride);
        float y = *(float*)((char*)y_data + i * y_stride);
        
        float weight = 1.0f;
        if (w_data) {
            weight = *(float*)((char*)w_data + i * w_stride);
            if(weight<0)weight=0;
            w.at<float>(i) = weight;
        }
        
        // Fill one row of A matrix
        float x_power = 1.0f;
        for (int j = 0; j <= order; j++) {
            A.at<float>(i, j) = x_power * weight;
            x_power *= x;
        }
        
        b.at<float>(i) = y * weight;
    }
    
    // Solve using least squares
    cv::Mat x;
    bool solved = cv::solve(A, b, x, cv::DECOMP_SVD);
    
    if (!solved) return -2; // Failed to solve
    
    // Copy coefficients to output array
    // Note: coefficients are in ascending order (constant term first)
    for (int i = 0; i <= order; i++) {
        coeffs[i] = x.at<float>(i);
    }
    
    return 0;
}

/**
 * Calculate y value for given x using polynomial coefficients
 * 
 * @param x Input x value
 * @param coeffs Array of polynomial coefficients
 * @param order Order of polynomial (size of coeffs should be order + 1)
 * @return Calculated y value
 */
float polycalc_opencv(float x, const float* coeffs, int order) {
    float y = coeffs[0];  // constant term
    float x_power = x;
    
    for (int i = 1; i <= order; i++) {
        y += coeffs[i] * x_power;
        x_power *= x;
    }
    
    return y;
}




vector<InspectionTarget_DimMeasure::itemInfo> buildFeatureQuickList(cJSON *featureList)
{
  cJSONPP jFL(featureList,false);
  vector<InspectionTarget_DimMeasure::itemInfo> infoQList;
  int list_size = jFL.length();
  for (int i = 0; i < list_size; i++)
  {
    auto featureEle = jFL[i];
    InspectionTarget_DimMeasure::itemInfo info={.cached_feature_def=featureEle.get()};

    int id = featureEle["id"].asInt();
    info.id = id;
    info.idx=i;
    string type=featureEle["type"].asString();
    // info.feature_type=getFeatureType(featureEle);

    LOGE("=========id:%d type:%s  featureEle:%p",id,type.c_str(),featureEle.get());
    auto refList = featureEle["ref"];

    info.refIdIdx.clear();
    if (refList.isArray()==false)
    {
      LOGI("=========refList is not an array");
    }
    else
    {
      int ref_size = refList.length();

      for (int j = 0; j < ref_size; j++)
      {
        auto refItem = refList[j];
        if (refItem.isObject()==false)
          break;

        int refId = refItem["id"].asInt(-999);
        if (refId != -999)
        {
          info.refIdIdx.push_back({
            .id=refId,
            .idx=-1
          });
        }
        else
        {
          LOGI("=========refItem is not a number");
          break;
        }
      }
    }
    
    
    infoQList.push_back(info);
  }


  for (int i = 0; i < list_size; i++)
  {//find refIdx
    int id=infoQList[i].id;
    for(int j=0;j<infoQList[i].refIdIdx.size();j++)//for every ref item
    {
      int targetID=infoQList[i].refIdIdx[j].id;
      for(int k=0;k<infoQList.size();k++)//find ref item's index
      {
        if(infoQList[k].id==targetID)
        {
          infoQList[i].refIdIdx[j].idx=k;
          break;
        }
      }
    }
  }
  return infoQList;
}


vector<InspectionTarget_DimMeasure::cat_itemInfo> buildCategoryQuickList(vector<InspectionTarget_DimMeasure::itemInfo> &featureQList,cJSON *categoryList)
{
  cJSONPP jCL(categoryList,false);
  int len=jCL.length();
  vector<InspectionTarget_DimMeasure::cat_itemInfo> infoQList;
  for(int i=0;i<len;i++)
  {
    auto categoryEle=jCL[i];
    int id=categoryEle["id"].asInt(-1);
    

    LOGE("categoryEle id:%d",id);
    if(id==-1)continue;
    InspectionTarget_DimMeasure::cat_itemInfo info={.cached_def=categoryEle.get()};
    auto setupList = categoryEle["limits_setup"];
    info.id=id;
    info.idx=i;

    info.limits_setup.clear();

    
    if(setupList.isArray())
    {
      int ref_size=setupList.length();
      for(int j=0;j<ref_size;j++)
      {
        auto refItem=setupList[j];
        int refId=refItem["id"].asInt(-1);
        int featureIdx=-1;
        if(refId!=-1)
        {
          for(int k=0;k<featureQList.size();k++)
          {
            if(featureQList[k].id==refId)
            {
              featureIdx=k;
              break;
            }
          }
        }
        float low_limit=refItem["low_limit"].asDouble();
        float high_limit=refItem["high_limit"].asDouble();
        string NG_as=refItem["NG_as"].asString();

        info.limits_setup.push_back({.ref_id_idx={.id=refId,.idx=featureIdx},.low_limit=low_limit,.high_limit=high_limit,.NG_as=NG_as});
      }
    }


    infoQList.push_back(info);
    // LOGE("Push len:%d",infoQList.size());
  }
  return infoQList;
}


void InspectionTarget_DimMeasure::setInspDef(cJSON *def)
{
  //WARNING: the def pointer will be changed after setInspDef
  InspectionTarget::setInspDef(def);
  //in InspectionTarget::setInspDef it will clone the def
  //so you need to use the this->def pointer after setInspDef

  // def=this->def;
  featureQuickList.clear();
  categoryQuickList.clear();
  LOGE("=========def:%p",this->def);


  cJSONPP jdef(this->def,false);
  auto featureInfo = jdef["featureInfo"];
  if (featureInfo.isObject()==false)
    return;

  auto featureList = featureInfo["element_list"];
  if (featureList.isArray()==false)
  {
    featureInfo.w()["element_list"]=cJSONPP::newArray();
    featureList = featureInfo["element_list"];
  }

  auto categoryList = featureInfo["category_list"];
  if(categoryList.isArray()==false)
  {
    featureInfo.w()["category_list"]=cJSONPP::newArray();
    categoryList = featureInfo["category_list"];
  }
  // if (categoryList.isArray()==false)
  //   return;


  try
  {

    featureQuickList = buildFeatureQuickList(featureList.get());

    categoryQuickList = buildCategoryQuickList(featureQuickList,categoryList.get());
    for (int i = 0; i < featureQuickList.size(); i++)
    {
      LOGI("=========id:%d", featureQuickList[i].id);
    }
  }
  catch (std::runtime_error &e)
  {
    LOGE("=========%s", e.what());
  }
}

void InspectionTarget_DimMeasure::run()
{

  // acvImage cacheImage;
  while (true)
  {
    std::shared_ptr<StageInfo> curInput;
    // LOGI("<<<<<size():%d",datTransferQueue.size());
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test

    try
    {
      // LOGI("TryReadNew");
      if (input_queue.pop_blocking(curInput) == false)
      {
        LOGI("TryReadTailed");
        break;
      }

      auto ret = singleProcess(curInput,false);

      if (ret != NULL)
      {
        belongMan->dispatch(ret);
      }
    }
    catch (TS_Termination_Exception e)
    {
      LOGI("TS_Termination_Exception");
      break;
    }
  }
}

shared_ptr<StageInfo_Orientation> loadOrientation(string path)
{
  shared_ptr<StageInfo_Orientation> reporn_temp(new StageInfo_Orientation());

  Mat img = imread(path + ".png");
  reporn_temp->img_show =
      reporn_temp->img = img;

  {
    // read text file
    cJSON *jfile = ReadJson((path + ".json").c_str());
    if (jfile)
    {
      // cJSON_Delete(jfile);
      if(reporn_temp->jInfo)
      {
        cJSON_Delete(reporn_temp->jInfo);
      }
      reporn_temp->jInfo=jfile;


      jfile = NULL;
    }
  }
  reporn_temp->img_prop.mmpp=reporn_temp->get_mmpp();

  LOGI("=========orientation size:%d", reporn_temp->get_report_count());

  return reporn_temp;
}

bool InspectionTarget_DimMeasure::exchangeCMD(cJSON *info, int id, exchangeCMD_ACT &act)
{
  // LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info, id, act);
  if (ret)
    return ret;

  cJSONPP jinfo(info);
  string type = jinfo["type"].asString();

  if (type == "save_cached_orientation_info")
  {
    if (cache_latest_input == NULL)
      return false;

    shared_ptr<StageInfo_Orientation> sinfo_orientation = std::static_pointer_cast<StageInfo_Orientation>(cache_latest_input);
    

    if(sinfo_orientation==NULL)
    {
      act.sendACK(id,false,"cache_latest_input is NULL, fetch a new data first...");
      return false;
    }

    LOGI(">>>");

    string file_name = jinfo["file_name"].asString();
    string folder_path = jinfo["folder_path"].asString();

    LOGI(">>>");

    {

      //remove file extension
      auto pos=file_name.find_last_of(".");
      if(pos!=string::npos)
      {
        file_name=file_name.substr(0,pos);
      }
    }

    if(file_name.length()==0)
    {
      act.sendACK(id,false,"file_name is empty...");
      return false;
    }



    LOGI(">>>");

    if (folder_path.find(":\\") == 1 || folder_path.find("/") == 0)
    { // windows absolute path or linux absolute path
    }
    else
    {
      folder_path = local_env_path + folder_path; // add local env path
    }

    if (folder_path.length() == 0)
      return false;

    auto srcImg = sinfo_orientation->img;
    if (srcImg.empty())
      return false;

    string path = folder_path + "/" + file_name;
    imwrite(path + ".png", srcImg);


    LOGI("=========save to %s", path.c_str());

    // save json
    string json_path = path + ".json"; // if exist override
    FILE *fp = fopen(json_path.c_str(), "w");
    if (fp != NULL)
    {
      char *json_str = cJSON_Print(sinfo_orientation->jInfo);
      fprintf(fp, "%s", json_str);
      fclose(fp);
      free(json_str);
    }

    return true;
  }
  if (type == "load_orientation_info")
  {

    std::__1::shared_ptr<StageInfo_Orientation> template_input_info;

    bool use_cached_input=jinfo["use_cached_input"].asBool();

    if(use_cached_input)
    {
      template_input_info=std::static_pointer_cast<StageInfo_Orientation>(cache_latest_input);
    }
    else
    {

      string file_name = jinfo["file_name"].asString("template");
      string folder_path = jinfo["folder_path"].asString();
      if (folder_path.find(":\\") == 1 || folder_path.find("/") == 0)
      { // windows absolute path or linux absolute path
      }
      else
      {
        folder_path = local_env_path + folder_path; // add local env path
      }

      if (folder_path.length() == 0)
        return false;

      string path = folder_path + "/" + file_name;
      template_input_info = loadOrientation(path);

    }


    this->template_input_info = template_input_info;

    if(template_input_info==NULL)
    {
      act.sendACK(id,false,"template_input_info is NULL");
      return false;
    }


    int imageQuality = jinfo["imageQuality"].asInt(90);
    if (imageQuality > 0 && imageQuality <= 100)
    {
      // if (imageQuality == 100)
      // {
      //   act.send(id, template_input_info->img, "png", imageQuality);
      // }
      // else
      {
        act.send(id, template_input_info->img, "jpg", imageQuality);
      }
    }





    act.send("IP", id, template_input_info->jInfo);

    // auto ret=singleProcess(this->template_input_info);
    // if(ret!=NULL)
    // {
    // 	belongMan->dispatch(ret);

    // }

    return true;
  }

  if (type == "execute_inspection")
  {
    shared_ptr<StageInfo> input;



    bool use_cached_input=jinfo["use_cached_input"].asBool();
    if(use_cached_input==false)
    {

      string file_name = jinfo["file_name"].asString("template");
      string folder_path = jinfo["folder_path"].asString();
      if (folder_path.find(":\\") == 1 || folder_path.find("/") == 0)
      { // windows absolute path or linux absolute path
      }
      else
      {
        folder_path = local_env_path + folder_path; // add local env path
      }

      if (folder_path.length() == 0)
      {

        LOGI("=========load from cache");
        input = cache_latest_input;
      }
      else
      {
        LOGI("=========load from file:%s", (folder_path + "/" + file_name).c_str());
        input = loadOrientation(folder_path + "/" + file_name);
      }
    }
    else
    {
      input=cache_latest_input;
    }

    LOGI("=========execute_inspection");
    if (input == NULL)
      return false;


    dbg_feature_id=jinfo["dbg_feature_id"].asInt(-1);

    if(dbg_feature_info!=NULL)
    {
      cJSON_Delete(dbg_feature_info);
      dbg_feature_info=NULL;
    }

    dbg_Images.clear();
    {
      auto jtmp=jinfo["dbg_feature_info"];
      if(jtmp.isObject())
      {
        dbg_feature_info=cJSON_Duplicate(jtmp.get(),true);
      }
    }
    



    
    shared_ptr<StageInfo_Orientation> input_orien = std::static_pointer_cast<StageInfo_Orientation>(input);
    if (input_orien == NULL)
    {
      return false;
    }

    if(input_orien->get_report_count()==0)
    {
      return false;
    }

    int dbg_object_idx=jinfo["dbg_object_idx"].asInt(-1);
    int count=input_orien->get_report_count();
    if(dbg_object_idx>=0 &&  (dbg_object_idx < count))//check if dbg_object_idx is valid
    {
      LOGE("=========input_orien->orientation.size()>1");
      // input_orien->orientation[0]=input_orien->orientation[dbg_object_idx];
      // input_orien-> orientation.resize(1);
    }
    auto report = singleProcess(input_orien,true);

    if(dbg_feature_info!=NULL)
    {
      cJSON_Delete(dbg_feature_info);
      dbg_feature_info=NULL;
    }



    dbg_feature_id=-1;
    int imageQuality = jinfo["imageQuality"].asInt(-1);
    if (imageQuality > 0 && imageQuality <= 100)
    {
      // if (imageQuality == 100)
      // {
      //   act.send(id, report->img, "png", imageQuality);
      // }
      // else
      // {
      //   act.send(id, report->img, "jpg", imageQuality);
      // }

      for(int i=0;i<dbg_Images.size();i++)
      {
        // if (imageQuality == 100)
        // {
        //   act.send(id, dbg_Images[i], "png", imageQuality);
        // }
        // else
        {
          act.send(id, dbg_Images[i], "jpg", imageQuality);
        }

      }
    }


    dbg_Images.clear();
    act.send("RP", id, report->jInfo);

    return true;
  }

  return false;
}

static int STAGEINFO_SCS_CAT_BASIC_reducer_(int sum_cat, int cat)
{

  switch (sum_cat)
  {
  case STAGEINFO_CAT_UNSET:
    sum_cat = cat;
    break;
  case STAGEINFO_CAT_OK:
    if (cat == STAGEINFO_CAT_NG2 || cat == STAGEINFO_CAT_NG || cat == STAGEINFO_CAT_NA)
      sum_cat = cat;

    break;
  case STAGEINFO_CAT_NG2:
    if (cat == STAGEINFO_CAT_NG || cat == STAGEINFO_CAT_NA)
      sum_cat = cat;
    break;

  case STAGEINFO_CAT_NG:
    if (cat == STAGEINFO_CAT_NA)
      sum_cat = cat;
    break;

  default:
  case STAGEINFO_CAT_NA:
  case STAGEINFO_CAT_NOT_EXIST:

    break;
  }

  return sum_cat;
}


/**
 * @brief Generate the warp polar XY map
 * 
 * @param center The center of the polar coordinate system
 * @param minRadius The minimum radius of the polar coordinate system
 * @param maxRadius The maximum radius of the polar coordinate system
 * @param startAngle_rad The start angle in radians
 * @param endAngle_rad The end angle in radians
 * @param mapX The output X map
 * @param mapY The output Y map
 * @param scale_R The scale factor for the radius
 * @param scale_ANG The scale factor for the angle
 * @param direction The direction of the polar coordinate system (1 for inner to left(outer to the right), -1 for inner to right(outer to the left))
 */
static void genWarpPolarXYMap1(const cv::Point2f& center, 
                         float minRadius, float maxRadius, 
                         float startAngle_rad, float endAngle_rad,
                         cv::Mat& mapX, cv::Mat& mapY,float scale_R,float scale_ANG,bool inner_to_left,float mmpp) {

    float angleRange_rad = (endAngle_rad - startAngle_rad);
    float radiusRange=(maxRadius-minRadius);
    // Determine the output size based on the radius and angle range
    int outputHeight = ((int)(angleRange_rad*maxRadius*scale_ANG)+1); // Width based on arc length(angle range)
    int outputWidth = (int)(radiusRange*scale_R);          // Height based on radius range
    // Prepare the mapping matrices
    mapX.create(outputHeight, outputWidth, CV_32FC1);
    mapY.create(outputHeight, outputWidth, CV_32FC1);
    // Fill the mapping matrices

    for (int y = 0; y < outputHeight; ++y) {
        // Calculate the angle in radians for this pixel
        float angle = -(startAngle_rad + (angleRange_rad*((float)y) / (outputHeight-1)));//flip angle for image coordinate (-y)

        float cos_angle=cosf(angle);
        float sin_angle=sinf(angle);
        for (int x = 0; x < outputWidth; ++x) {
            float radius = radiusRange*((float)x/(outputWidth-1)); // Current radius
            
            if(inner_to_left==false){
              radius=minRadius+radius;
            }
            else{//reverse direction
              radius=maxRadius-radius;
            }
            
            float x_cart=center.x + radius * cos_angle;
            float y_cart=center.y + radius * sin_angle;
            // Map polar coordinates to Cartesian
            mapX.at<float>(y, x) = x_cart;
            mapY.at<float>(y, x) = y_cart;
        }
    }
    
                  

}

static void genWarpPolarXYMap(const cv::Point2f& center, 
                         float minRadius, float maxRadius, 
                         float startAngle_rad, float endAngle_rad,
                         cv::Mat& mapX, cv::Mat& mapY,
                         float scale_R, float scale_ANG,
                         bool inner_to_left, float mmpp) {


    float angleRange_rad = (endAngle_rad - startAngle_rad);
    float radiusRange = (maxRadius - minRadius);
    int outputHeight = ((int)(angleRange_rad*maxRadius*scale_ANG)+1);
    int outputWidth = (int)(radiusRange*scale_R);
    
    mapX.create(outputHeight, outputWidth, CV_32FC1);
    mapY.create(outputHeight, outputWidth, CV_32FC1);

    float radiusStep = radiusRange / (outputWidth - 1);
    float angleStep = angleRange_rad / (outputHeight - 1);

    float radiusIncrement = inner_to_left ? -radiusStep : radiusStep;
    float initRadius=inner_to_left ? maxRadius : minRadius;
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < outputHeight; ++y) {
        float angle = -(startAngle_rad + angleStep * y);
        float cos_angle = cosf(angle);
        float sin_angle = sinf(angle);
        
        float* mapX_row = mapX.ptr<float>(y);
        float* mapY_row = mapY.ptr<float>(y);
        
        float radius = initRadius;

        for (int x = 0; x < outputWidth; ++x) {
            mapX_row[x] = center.x + radius * cos_angle;
            mapY_row[x] = center.y + radius * sin_angle;
            radius += radiusIncrement;
        }
    }
    
}


static cv::Mat template_from_img(StageInfo_Orientation::orient objpose,float img_scale) //coordinate transform from image to template
{

    float angle_rad=(objpose.angle);

    return cvM3x3::scale(img_scale)*cvM3x3::rotate(-angle_rad)*cvM3x3::translate(-objpose.center);
    
}

static cv::Mat line_from_template(Point2f template_pt1, Point2f template_pt2,double addtional_angle_rad) {

    float defLineAngle=atan2f((template_pt2.y-template_pt1.y),template_pt2.x-template_pt1.x);
    Point2f pointCenter=(template_pt1+template_pt2)/2;

    return cvM3x3::rotate(-defLineAngle+addtional_angle_rad)*cvM3x3::translate(-pointCenter);

}


static cv::Mat spoint_from_template(Point2f template_pt1,float spoint_angle,Point2f ref_template_pt1, Point2f ref_template_pt2,double addtional_angle_rad) {

    float defLineAngle=atan2f((ref_template_pt2.y-ref_template_pt1.y),ref_template_pt2.x-ref_template_pt1.x);
    LOGE("defLineAngle:%f,addtional_angle:%f,spoint_angle:%f",defLineAngle*180/M_PI,addtional_angle_rad*180/M_PI,spoint_angle*180/M_PI);
    return cvM3x3::rotate(-defLineAngle+addtional_angle_rad-spoint_angle)*cvM3x3::translate(-template_pt1);

}




inline Mat edgeValueTrace(Mat &x_pos_sobelImg,float centerCoeff,float spreadCoeff){
  
  spreadCoeff/=2;
  Mat FilteredImg(x_pos_sobelImg.size(),CV_32F);

  vector<float> filterBuff1(x_pos_sobelImg.cols);
  vector<float> filterBuff2(x_pos_sobelImg.cols);


  { //init filterBuff1,filterBuff2
    int16_t *row=x_pos_sobelImg.ptr<int16_t>(0);

    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {
      filterBuff1[x]=filterBuff2[x]=row[x];
    }

  }

  int filter_width=3;
  int filter_side=(filter_width-1)/2;




  float nPixCoeff=(1-centerCoeff-2*spreadCoeff);
  for(int y=0;y<x_pos_sobelImg.rows;y+=1)
  {
    int16_t *cur_row=x_pos_sobelImg.ptr<int16_t>(y);

    float* filterBuff=(y&1==0)?filterBuff1.data():filterBuff2.data();
    float* filterOutput=(y&1==0)?filterBuff1.data():filterBuff2.data();


    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {

      float f_m1=(x==0)?0:filterBuff[x-1];
      float f_c=filterBuff[x];
      float f_p1=(x>=x_pos_sobelImg.cols-1)?0:filterBuff[x+1];
      float i_c=cur_row[x];


      filterOutput[x]=nPixCoeff*i_c+centerCoeff*f_c+spreadCoeff*(f_m1+f_p1);
    }


    float *cur_frow=FilteredImg.ptr<float>(y);
    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {
      cur_frow[x]=filterOutput[x];
    }
  }


  { //init filterBuff1,filterBuff2
    int16_t *row=x_pos_sobelImg.ptr<int16_t>(x_pos_sobelImg.rows-1);

    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {
      filterBuff1[x]=filterBuff2[x]=row[x];
    }

  }

  for(int _y=0;_y<x_pos_sobelImg.rows;_y+=1)//reverse
  {
    int y=x_pos_sobelImg.rows-_y-1;
    int16_t *cur_row=x_pos_sobelImg.ptr<int16_t>(y);

    float* filterBuff=(y&1)?filterBuff1.data():filterBuff2.data();
    float* filterOutput=(y&1)?filterBuff1.data():filterBuff2.data();


    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {

      float f_m1=(x==0)?0:filterBuff[x-1];
      float f_c=filterBuff[x];
      float f_p1=(x>=x_pos_sobelImg.cols-1)?0:filterBuff[x+1];
      float i_c=cur_row[x];


      filterOutput[x]=nPixCoeff*i_c+centerCoeff*f_c+spreadCoeff*(f_m1+f_p1);
    }


    float *cur_frow=FilteredImg.ptr<float>(y);
    for(int x=0;x<x_pos_sobelImg.cols;x+=1)
    {
      cur_frow[x]+=filterOutput[x];
    }
  }

  return FilteredImg;
}




enum EdgeType{
  DARK_TO_LIGHT=1,
  LIGHT_TO_DARK=-1,
  BOTH=0,
};

enum CenterType{
  LOCAL_MAX=1,
  LOCAL_AVG=2,
  GLOBAL_MAX=3,
  GLOBAL_AVG=4,
  BEST_MATCH=5,
};

inline float getCenter_LOCAL_MEAN(int16_t *sobeled_line,int line_width,int16_t noise_surpress_threshold,EdgeType edgeType,CenterType centerType, float &w,float &sigma){
  

  float blob_w = 0;
  float blob_xw = 0;
  float blob_xw_sq = 0; // Accumulate squared values for variance calculation

  float blob_w_max = 0;
  float blob_xw_max = 0;
  float blob_sigma_max = 0;

  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = sobeled_line[x];

    if (edgeType == DARK_TO_LIGHT) {
      // Do nothing
    }
    if (edgeType == LIGHT_TO_DARK) {
      edgeRsp = -edgeRsp; // Flip the edge response
    }
    if (edgeType == BOTH) {
      if (edgeRsp < 0) edgeRsp = -edgeRsp;
      // Do nothing
    }

    edgeRsp -= noise_surpress_threshold;
    if (edgeRsp < 0) edgeRsp = 0;


    sobeled_line[x]=edgeRsp;
    if (blob_w == 0) { // Wait for edge response  to start blob
      if (edgeRsp > 0) { // Start a new blob
        // Initialize blob
      } else {
        continue;
      }
    } else {
      if (edgeRsp == 0) { // End blob
        float cur_blob_w = blob_w;
        float cur_blob_x = blob_xw / blob_w;

        // LOGE("End blob %d  w:%f,x:%f",x,cur_blob_w,cur_blob_x);
        
        if (blob_w_max < blob_w) {
          blob_w_max = blob_w;
          blob_xw_max = blob_xw;

          float cur_x=blob_xw/blob_w;
          float variance = (blob_xw_sq / blob_w) - (cur_x * cur_x);
          float cur_blob_sigma = sqrt(variance);
          blob_sigma_max = cur_blob_sigma;
        }

        // Reset blob
        blob_w = 0;
        blob_xw = 0;
        blob_xw_sq = 0;
        continue;
      } else { // Still in blob
      }
    }


    blob_w += edgeRsp;
    blob_xw += x * edgeRsp;
    blob_xw_sq += x * x * edgeRsp;
  }
  w=blob_w_max;
  sigma=blob_sigma_max;

  float x=blob_xw_max/blob_w_max;
  // LOGE("blob_xw_max:%f/blob_w_max:%f=x:%f",blob_xw_max,blob_w_max,x);
  return x;
}


void sobelRowProcess(int16_t* row,int line_width,int16_t noise_surpress_threshold,EdgeType edgeType){

  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = row[x];

    if (edgeType == DARK_TO_LIGHT) {
      // Do nothing
    }
    if (edgeType == LIGHT_TO_DARK) {
      edgeRsp = -edgeRsp; // Flip the edge response
    }
    if (edgeType == BOTH) {
      if (edgeRsp < 0) edgeRsp = -edgeRsp;
      // Do nothing
    }

    edgeRsp -= noise_surpress_threshold;
    if (edgeRsp < 0) edgeRsp = 0;

    row[x]=edgeRsp;
  }
}



void sobelProcess(Mat grayXSobel,float edge_surpress,EdgeType edgeType)
{
  for(int y=0;y<grayXSobel.rows;y+=1)
  {
    int16_t *row=grayXSobel.ptr<int16_t>(y);
    sobelRowProcess(row,grayXSobel.cols,edge_surpress,edgeType);
  }
}




// void sobelProcess_edgeSurpress(Mat grayXSobel,float edge_surpress)
// {
//   for(int y=0;y<grayXSobel.rows;y+=1)
//   {
//     int16_t *row=grayXSobel.ptr<int16_t>(y);
//     for(int x=0;x<grayXSobel.cols;x+=1)
//     {
//       int16_t edgeRsp=row[x];
//       edgeRsp-=edge_surpress;
//       if(edgeRsp<0)edgeRsp=0;
//       row[x]=edgeRsp;
//     }
//   }
// }


inline float getCenter_LOCAL_MEANd(const int16_t *sobeled_line,int line_width,CenterType centerType, float &w,float &sigma){
  

  float blob_w = 0;
  float blob_xw = 0;
  float blob_xw_sq = 0; // Accumulate squared values for variance calculation

  float blob_w_max = 0;
  float blob_xw_max = 0;
  float blob_sigma_max = 0;

  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = sobeled_line[x];

    if (blob_w == 0) { // Wait for edge response  to start blob
      if (edgeRsp > 0) { // Start a new blob
        // Initialize blob
      } else {
        continue;
      }
    } else {
      if (edgeRsp == 0) { // End blob
        float cur_blob_w = blob_w;
        float cur_blob_x = blob_xw / blob_w;

        // LOGE("End blob %d  w:%f,x:%f",x,cur_blob_w,cur_blob_x);
        
        if (blob_w_max < blob_w) {
          blob_w_max = blob_w;
          blob_xw_max = blob_xw;

          float cur_x=blob_xw/blob_w;
          float variance = (blob_xw_sq / blob_w) - (cur_x * cur_x);
          float cur_blob_sigma = sqrt(variance);
          blob_sigma_max = cur_blob_sigma;
        }

        // Reset blob
        blob_w = 0;
        blob_xw = 0;
        blob_xw_sq = 0;
        continue;
      } else { // Still in blob
      }
    }


    blob_w += edgeRsp;
    blob_xw += x * edgeRsp;
    blob_xw_sq += x * x * edgeRsp;
  }
  w=blob_w_max;
  sigma=blob_sigma_max;

  float x=blob_xw_max/blob_w_max;
  // LOGE("blob_xw_max:%f/blob_w_max:%f=x:%f",blob_xw_max,blob_w_max,x);
  return x;
}


inline float getCenter_LOCAL_MEAN(const int16_t *sobeled_line, int line_width, CenterType centerType, float &w, float &sigma) {
    float blob_w = 0;
    float blob_xw = 0;
    float blob_xw_sq = 0;
    
    float blob_w_max = 0;
    float blob_xw_max = 0;
    float blob_xw_sq_max = 0;
    
    // Pre-calculate array bounds
    const int16_t* const line_end = sobeled_line + line_width;
    const int16_t* ptr = sobeled_line;

    // Process continuous memory blocks
    while (ptr < line_end) {
        // Skip until we find start of blob
        while (ptr < line_end && *ptr <= 0) {
            ptr++;
        }
        
        // Process blob if we found one
        if (ptr < line_end) {
            blob_w = 0;
            blob_xw = 0;
            blob_xw_sq = 0;
            const int start_x = ptr - sobeled_line;
            
            // Process until end of blob or array
            while (ptr < line_end && *ptr > 0) {
                const int x = ptr - sobeled_line;
                const float edgeRsp = *ptr;
                
                // Accumulate in one pass
                blob_w += edgeRsp;
                blob_xw += x * edgeRsp;
                blob_xw_sq += x * x * edgeRsp;
                
                ptr++;
            }
            
            // Update max values if current blob is larger
            if (blob_w > blob_w_max) {
                blob_w_max = blob_w;
                blob_xw_max = blob_xw;
                blob_xw_sq_max = blob_xw_sq;
            }
        }
    }

    // Handle edge case where no blobs were found
    if (blob_w_max == 0) {
        w = 0;
        sigma = 0;
        return 0;
    }

    // Calculate final results
    const float center = blob_xw_max / blob_w_max;
    const float variance = (blob_xw_sq_max / blob_w_max) - (center * center);
    
    w = blob_w_max;
    sigma = std::sqrt(std::max(0.0f, variance));  // Protect against small negative values from floating point errors
    
    return center;
}


inline float getCenter_LOCAL_FIRST(int16_t *sobeled_line,int line_width,CenterType centerType, float &w,float &sigma){
  

  float blob_c = 0;
  float blob_w = 0;
  float blob_xw = 0;
  float blob_xw_sq = 0; // Accumulate squared values for variance calculation

  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = sobeled_line[x];
   

    if (blob_w == 0) { // Wait for edge response  to start blob
      if (edgeRsp > 0) { // Start a new blob
        // Initialize blob
      } else {
        continue;
      }
    } else {
      if (edgeRsp == 0) { // End blob
        float cur_blob_w = blob_w;
        float cur_blob_x = blob_xw / blob_w;

        float cur_x=blob_xw/blob_w;
        float variance = (blob_xw_sq / blob_w) - (cur_x * cur_x);
        float cur_blob_sigma = sqrt(variance);
        // LOGE("End blob %d  w:%f,x:%f",x,cur_blob_w,cur_blob_x);
        {
          w=cur_blob_w;
          sigma=cur_blob_sigma;
          return cur_blob_x;
        }

        // Reset blob
        blob_w = 0;
        blob_xw = 0;
        blob_xw_sq = 0;
        continue;
      } else { // Still in blob
      }
    }


    blob_w += edgeRsp;
    blob_c++;
    blob_xw += x * edgeRsp;
    blob_xw_sq += x * x * edgeRsp;
  }
  w=NAN;
  sigma=NAN;
  return NAN;
}



inline float getCenter_LOCAL_FIRSTMax(int16_t *sobeled_line,int line_width,CenterType centerType, float &w,float &sigma){
  

  int max_edgeRsp_x=0;
  int max_edgeRsp=0;

  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = sobeled_line[x];
    if(edgeRsp<=0)continue;
    if(edgeRsp>=max_edgeRsp)
    {
      max_edgeRsp=edgeRsp;
      max_edgeRsp_x=x;
    }
    else
    {
      LOGE("max_edgeRsp_x:%d,x:%d",max_edgeRsp_x,x);
      w=max_edgeRsp_x;
      sigma=3;
      if(x<2)
        return max_edgeRsp_x;

      int m1x=x-2;
      int m1w=sobeled_line[m1x];
      int max_x=x-1;
      int max_w=sobeled_line[max_x];
      int p1x=x;
      int p1w=sobeled_line[p1x];
      w=(m1w+max_w+p1w);
      sigma=2;
      return (m1x*m1w+max_x*max_w+p1x*p1w)/(m1w+max_w+p1w);

    }
  }
  w=NAN;
  sigma=NAN;
  return NAN;
}



static vector<Point3f> getLocalMeanEdgePoints(const Mat& grayXSobel, CenterType centerType) {
    vector<Point3f> edge_points(grayXSobel.rows);

    #pragma omp parallel for schedule(static)
    for (int y = 0; y < grayXSobel.rows; y++) {
        float edge_w, edge_sigma;
        const int16_t* row = grayXSobel.ptr<int16_t>(y);
        

        float edge_loc = getCenter_LOCAL_MEAN(row, grayXSobel.cols, centerType, edge_w, edge_sigma);
        
        if (edge_w > 0 && edge_loc >= 2 && edge_loc<grayXSobel.cols-2) {
            edge_points[y] = {edge_loc, (float)y,edge_w/(edge_sigma+1)};
        }
        else
        {
            edge_points[y].z=0;
        }
    }

    //remove zero weight points
    int valid_count=0;
    for(int i=0;i<edge_points.size();i++)
    {
        if(edge_points[i].z>0)
        {
            edge_points[valid_count]=edge_points[i];
            valid_count++;
        }
    }
    edge_points.resize(valid_count);
    return edge_points;
}




vector<Point3f> EdgeLocExtraction(string name,Mat &featureCoordImg,cJSON *def,vector<cv::Mat> &dbg_Images)
{


  Mat featureCoordImg_gray;
  if(featureCoordImg.channels()==3)
    cv::cvtColor(featureCoordImg, featureCoordImg_gray, cv::COLOR_BGR2GRAY);
  else if(featureCoordImg.channels()==1)//is gray
    featureCoordImg_gray=featureCoordImg;
  else 
  {
    LOGE("featureCoordImg is not gray or BGR");
    return {};
  }

  dbg_Images.push_back(featureCoordImg_gray);
  int blur_size=JFetch_NUMBER_ex(def,"blur_size",3);
  blur_size=(blur_size+1)/2*2;
  
  Mat featureCoordImg_YBlur;
  if(blur_size>0)
    cv::blur(featureCoordImg_gray, featureCoordImg_YBlur, cv::Size(1, blur_size));
  else
    featureCoordImg_YBlur=featureCoordImg_gray;

  Mat XSobel;
  cv::Sobel(featureCoordImg_YBlur, XSobel, CV_16S, 1, 0); // 1 for x-derivative to detect vertical edges


  EdgeType edgeType=DARK_TO_LIGHT;
  {

    string str_edgeType=JFetch_STRING_ex(def,"edge_type","DARK_TO_LIGHT");
    LOGI("str_edgeType:%s",str_edgeType.c_str());
    if(str_edgeType=="LIGHT_TO_DARK")
      edgeType=LIGHT_TO_DARK;
    else if(str_edgeType=="DARK_TO_LIGHT")
      edgeType=DARK_TO_LIGHT;
    else if(str_edgeType=="BOTH")
      edgeType=BOTH;
  }


  float alpha1=JFetch_NUMBER_ex(def,"alpha1",0);
  float alpha2=JFetch_NUMBER_ex(def,"alpha2",0);
  float edge_surpress=JFetch_NUMBER_ex(def,"edge_surpress",10);

  Mat edgeValueTraceMat;
  if(alpha1>0||alpha2>0)
  {
    sobelProcess(XSobel,0,edgeType);
    edgeValueTraceMat=edgeValueTrace(XSobel,alpha1,alpha2);
    //convert to 16S
    edgeValueTraceMat.convertTo(XSobel,CV_16S);
    edgeType=DARK_TO_LIGHT;//the edge value(signess) is already flipped, so just use DARK_TO_LIGHT

  }
  else
  {
  }
  sobelProcess(XSobel,edge_surpress,edgeType);
  dbg_Images.push_back(XSobel);

  // cv::imwrite("data/"+name+"_polarSegment_gray.png", polarSegment_gray);
  vector<Point3f> edgeLoc=getLocalMeanEdgePoints(XSobel,LOCAL_AVG);



  // if(0)
  // {//edge filtering

  //   int coeffOrder=3;
  //   vector<float> resCoeff(coeffOrder+1,0);
  //   int dataJumpSize=sizeof(edgeLoc[0]);


  //   float diviate_threshold=5;
  //   int iterration_count=5;
  //   for(int iter=0;iter<iterration_count;iter++)
  //   {

  //     int result = polyfit_opencv(&(edgeLoc[0].pt.Y),
  //                         &(edgeLoc[0].pt.X),
  //                         &(edgeLoc[0].w),
  //                         edgeLoc.size(),
  //                         coeffOrder,
  //                         resCoeff.data(),
  //                         dataJumpSize,
  //                         dataJumpSize,
  //                         dataJumpSize
  //                         );
      
  //     // LOGE("result:%d",result);
  //     // for(int i=0;i<coeffOrder+1;i++)
  //     // {
  //     //   LOGE("resCoeff[%d]:%f",i,resCoeff[i]);
  //     // }
  //     for(int i=0;i<edgeLoc.size();i++)
  //     {
  //       float output=polycalc_opencv(edgeLoc[i].pt.Y,resCoeff.data(),coeffOrder);

  //       float diviate=abs(output-edgeLoc[i].pt.X);
  //       if(diviate<diviate_threshold)
  //       {
  //         if(edgeLoc[i].w<0)
  //           edgeLoc[i].w*=-1;//consider this point for next iteration
  //         // if(iter==iterration_count-1)
  //         //   edgeLoc[i].pt.X=output;
  //       }
  //       else
  //       {
  //         if(edgeLoc[i].w>0)
  //           edgeLoc[i].w*=-1;//ignore this point
  //       }
  //     }


  //     diviate_threshold*0.8;

  //     // for(int i=0;i<edgeLoc.size();i++)
  //     // {
  //     //   float output=polycalc_opencv(edgeLoc[i].pt.Y,resCoeff.data(),coeffOrder);
  //     //   LOGE("Y:%f => X:%f output:%f",edgeLoc[i].pt.Y,edgeLoc[i].pt.X,output);

  //     //   edgeLoc[i].pt.X=output;
  //     // }

  //   }


  // }

  return edgeLoc;
}

inline float sobelRowValueFilter(int16_t *sobeled_line,int line_width,int16_t noise_surpress_threshold,EdgeType edgeType){
  


  for (int x = 0; x < line_width; x++) {
    int16_t edgeRsp = sobeled_line[x];

    if (edgeType == DARK_TO_LIGHT) {
      // Do nothing
    }
    if (edgeType == LIGHT_TO_DARK) {
      edgeRsp = -edgeRsp; // Flip the edge response
    }
    if (edgeType == BOTH) {
      if (edgeRsp < 0) edgeRsp = -edgeRsp;
      // Do nothing
    }

    edgeRsp -= noise_surpress_threshold;
    if (edgeRsp < 0) edgeRsp = 0;


    sobeled_line[x]=edgeRsp;
  }
  return 0;
}


// inline float getCenter_LOCAL_MEAN(int16_t *sobeled_line,int line_width,int16_t noise_surpress_threshold,EdgeType edgeType,CenterType centerType, vector<ptInfo> &pushInedgeVec){
  

//   float blob_w = 0;
//   float blob_xw = 0;
//   float blob_xw_sq = 0; // Accumulate squared values for variance calculation

//   int count=0;
//   for (int x = 0; x < line_width; x++) {
//     int16_t edgeRsp = sobeled_line[x];

//     if (edgeType == DARK_TO_LIGHT) {
//       // Do nothing
//     }
//     if (edgeType == LIGHT_TO_DARK) {
//       edgeRsp = -edgeRsp; // Flip the edge response
//     }
//     if (edgeType == BOTH) {
//       if (edgeRsp < 0) edgeRsp = -edgeRsp;
//       // Do nothing
//     }

//     edgeRsp -= noise_surpress_threshold;
//     if (edgeRsp < 0) edgeRsp = 0;


//     sobeled_line[x]=edgeRsp;
//     if (blob_w == 0) { // Wait for edge response  to start blob
//       if (edgeRsp > 0) { // Start a new blob
//         // Initialize blob
//       } else {
//         continue;
//       }
//     } else {
//       if (edgeRsp == 0) { // End blob
//         float cur_blob_w = blob_w;
//         float cur_blob_x = blob_xw / blob_w;

//         float cur_x=blob_xw/blob_w;
//         float variance = (blob_xw_sq / blob_w) - (cur_x * cur_x);
//         float cur_blob_sigma = sqrt(variance);
//         // LOGE("End blob %d  w:%f,x:%f",x,cur_blob_w,cur_blob_x);

//         ptInfo pt;
//         pt.pt.X=cur_blob_x;
//         pt.pt.Y=x;
//         pt.w=cur_blob_w/(cur_blob_sigma+1);
//         pushInedgeVec.push_back(pt);

//         // Reset blob
//         blob_w = 0;
//         blob_xw = 0;
//         blob_xw_sq = 0;
//         continue;
//       } else { // Still in blob
//       }
//     }


//     blob_w += edgeRsp;
//     blob_xw += x * edgeRsp;
//     blob_xw_sq += x * x * edgeRsp;
//   }
//   return x;
// }




bool InspectSearchPointFeature(const cv::Mat &mat_img2template,float mmpp,vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{

  auto &info=fqList[featureIdx];
  LOGE("=========SearchPoint");
  if(info.inputInfo==NULL)return false;

  StageInfo_DimMeasure::LINE_RESULT refLineInfo;

  {//extract ref info

    if(info.refIdIdx.size()==0)
    {
      LOGE("no ref");
      info.result.error_code=-1;
      info.result.error_msg="no ref";
      return false;
    }

    auto &refIdIdx=info.refIdIdx[0];//only use the first ref

    LOGE("refIdIdx.idx:%d id:%d",refIdIdx.idx,refIdIdx.id);


    


    auto &refInfo=fqList[refIdIdx.idx];

    if(refInfo.result.result_type!=StageInfo_DimMeasure::LINE)
    {
      LOGE("ref is not a line");
      info.result.error_code=-1;
      info.result.error_msg="ref is not a line";
      return false;
    }
    if(refInfo.result.error_code!=0)
    {
      LOGE("ref error:%d,msg:%s",refInfo.result.error_code,refInfo.result.error_msg.c_str());
      info.result.error_code=-1;
      info.result.error_msg="ref error";
      return false;
    }

    refLineInfo=refInfo.result.line;
    // Point2f pt1=refInfo.result.line.pt1,pt2=refInfo.result.line.pt2;
    // ref_angle=atan2f(pt2.y-pt1.y,pt2.x-pt1.x);


  }


  auto &inputInfo=info.inputInfo;
  StageInfo_Orientation::orient pose;
  inputInfo->get_report_object(info.orientation_idx,pose);
  pose.angle-=info.template_angle;
  Mat srcImg=inputInfo->img;


  cJSON *def=info.cached_feature_def;

  info.result.result_type=StageInfo_DimMeasure::POINT;
  info.result.point.pt1.x=NAN;
  info.result.point.pt1.y=NAN;
  info.result.error_code=-1;
  // info.result.error_msg="Not implemented";

  string name=JFetch_STRING_ex(def,"name");
  
  Point2f pt1;
  pt1.x=JFetch_NUMBER_ex(def,"pt1.x");
  pt1.y=JFetch_NUMBER_ex(def,"pt1.y");

  float margin=JFetch_NUMBER_ex(def,"margin");//search margin/depth
  float width=JFetch_NUMBER_ex(def,"width");//search width
  float angle=JFetch_NUMBER_ex(def,"angle");//search angle

  cv::Size cropSize(margin*2/mmpp,width/mmpp);
  cv::Mat temp2spoint_view=
      cvM3x3::translate({(float)cropSize.width/2,(float)cropSize.height/2})*
      cvM3x3::scale(1/mmpp)*
      spoint_from_template(pt1,angle,refLineInfo.pt1,refLineInfo.pt2,-M_PI/2);


  cv::Mat img2spoint_view=temp2spoint_view*mat_img2template;

  img2spoint_view=cvM3x3::mat33to23(img2spoint_view);



  {//check if the spoint region is in the image

    cv::Mat mat_template2img=mat_img2template.inv();
    cv::Mat mat_template2img_23 = cvM3x3::mat33to23(mat_template2img);
    float delta = cv::determinant(mat_template2img);

    LOGE("delta(mat_template2img):%f",delta);
    vector<cv::Point2f> pts_on_template_coord={pt1};
    vector<cv::Point2f> pts_on_line_coord;
    cv::transform(pts_on_template_coord,pts_on_line_coord, mat_template2img_23); 

    cv::Point2f pt1_img=pts_on_line_coord[0];
    float margin_img=margin/delta;
    if(pt1_img.x-margin_img<0 || pt1_img.x+margin_img>srcImg.cols ||
       pt1_img.y-margin_img<0 || pt1_img.y+margin_img>srcImg.rows)
    {
      info.result.error_code=-1;
      info.result.error_msg="spoint is out of image";
      return false;
    }
  }


  Mat transformSpointImg;
  cv::warpAffine(srcImg, transformSpointImg, img2spoint_view,cropSize, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
  
  
  vector<cv::Mat> _dbg_Images;
  vector<Point3f> edge_points=EdgeLocExtraction(name,transformSpointImg,def,_dbg_Images);
  float xPosMin=999999;
  for(int y=0;y<edge_points.size();y+=1)
  {
    Point3f &ept=edge_points[y];
    if(xPosMin>ept.x)xPosMin=ept.x;
  }

  float Wsum=0;
  float YWsum=0;
  float XWsum=0;



  float consider_range=JFetch_NUMBER_ex(def,"consider_range",1);
  float alpha_keep_range=JFetch_NUMBER_ex(def,"alpha_keep_range",0.5);

  if(alpha_keep_range>consider_range)
  {
    // LOGE("alpha_keep_range>consider_range, set to consider_range");
    alpha_keep_range=consider_range;
  }


  vector<cv::Point3f> consider_edge_points;
  for(int y=0;y<edge_points.size();y+=1)
  {
    Point3f &ept=edge_points[y];
    float dist=ept.x-xPosMin;
    if(dist>consider_range)
    {
      continue;
    }

    float alpha=1-(dist-alpha_keep_range)/(consider_range-alpha_keep_range);
    if(alpha>1)alpha=1;
    float w=ept.z*alpha;

    Wsum+=w;
    YWsum+=ept.y*w;
    XWsum+=ept.x*w;
    consider_edge_points.push_back(ept);  
  }


  float loc_offset=JFetch_NUMBER_ex(def,"loc_offset",0);
  Point2f pt;
  pt.x=XWsum/Wsum+loc_offset;
  pt.y=YWsum/Wsum;

  //drow red point on the image at the spoint
  int8_t *imgrow=transformSpointImg.ptr<int8_t>(pt.y);
  imgrow[(int)pt.x*3]=0;//BGR
  imgrow[(int)pt.x*3+1]=0;
  imgrow[(int)pt.x*3+2]=255;
  LOGE("spoint:%f,%f",pt.x,pt.y);




  vector<cv::Point2f> pts_on_spoint_coord={pt};

  vector<cv::Point2f> pts_on_template_coord;


  cv::Mat spoint_view2temp;
  cv::invert(temp2spoint_view,spoint_view2temp);
  spoint_view2temp=cvM3x3::mat33to23(spoint_view2temp);
  // cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);


  cv::transform(pts_on_spoint_coord, pts_on_template_coord, spoint_view2temp); 

  LOGE("back:pt1:%f,%f,pt2:%f,%f,pto:%f,%f",
  pts_on_template_coord[0].x,pts_on_template_coord[0].y,pts_on_template_coord[1].x,pts_on_template_coord[1].y,pts_on_template_coord[2].x,pts_on_template_coord[2].y);


  info.result.point.pt1.x=pts_on_template_coord[0].x;
  info.result.point.pt1.y=pts_on_template_coord[0].y;

  if(info.result.dbg_info!=NULL)
  {
    //there is a dbg info request
    cJSON_AddNumberToObject(info.result.dbg_info,"x",pts_on_template_coord[0].x);
    cJSON_AddNumberToObject(info.result.dbg_info,"y",pts_on_template_coord[0].y);

  }


  if(info.result.dbg_info!=NULL)
  {//attach dbg info

    
    vector<cv::Point2f> pts_on_template_coord;

    {
      vector<cv::Point2f> pts_on_line_coord;
      for(int i=0;i<edge_points.size();i++)
      {
        pts_on_line_coord.push_back({edge_points[i].x,edge_points[i].y});
      }

      cv::transform(pts_on_line_coord, pts_on_template_coord, spoint_view2temp); 

      cJSON* jpts=cJSON_CreateArray();
      cJSON_AddItemToObject(info.result.dbg_info,"edge_points",jpts);
      for(int i=0;i<pts_on_template_coord.size();i++)
      {
        cJSON* jpt=cJSON_CreateObject();
        cJSON_AddItemToArray(jpts,jpt);
        cJSON_AddNumberToObject(jpt,"x",pts_on_template_coord[i].x);
        cJSON_AddNumberToObject(jpt,"y",pts_on_template_coord[i].y);
        cJSON_AddNumberToObject(jpt,"w",edge_points[i].z);
      }
    }

    {
      vector<cv::Point2f> pts_on_line_coord;
      for(int i=0;i<consider_edge_points.size();i++)
      {
        pts_on_line_coord.push_back({consider_edge_points[i].x,consider_edge_points[i].y});
      }
      cv::transform(pts_on_line_coord, pts_on_template_coord, spoint_view2temp); 

      cJSON* jpts=cJSON_CreateArray();
      cJSON_AddItemToObject(info.result.dbg_info,"consider_edge_points",jpts);
      for(int i=0;i<pts_on_template_coord.size();i++)
      {
        cJSON* jpt=cJSON_CreateObject();
        cJSON_AddItemToArray(jpts,jpt);
        cJSON_AddNumberToObject(jpt,"x",pts_on_template_coord[i].x);
        cJSON_AddNumberToObject(jpt,"y",pts_on_template_coord[i].y);
        cJSON_AddNumberToObject(jpt,"w",consider_edge_points[i].z);
      }
    }


  }

  info.result.error_code=0;
  // cv::imwrite("data/"+name+"_spoint.png", transformSpointImg);

  return true;
}



bool InspectLineFitFeature(const cv::Mat &mat_img2template,float mmpp,vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{
  auto &info=fqList[featureIdx];
  LOGE("=========LineFit");
  if(info.inputInfo==NULL)return false;

  auto &inputInfo=info.inputInfo;
  cJSON *def=info.cached_feature_def;

  info.result.result_type=StageInfo_DimMeasure::LINE;
  info.result.line.pt1.x=NAN;
  info.result.line.pt1.y=NAN;
  info.result.line.pt2.x=NAN;
  info.result.line.pt2.y=NAN;
  info.result.error_code=-1;
  // info.result.error_msg="Not implemented";


  StageInfo_Orientation::orient pose;
  inputInfo->get_report_object(info.orientation_idx,pose);
  pose.angle-=info.template_angle;
  string name=JFetch_STRING_ex(def,"name");

  Mat srcImg=inputInfo->img;

  Point2f pt1,pt2;
  pt1.x=JFetch_NUMBER_ex(def,"pt1.x");
  pt1.y=JFetch_NUMBER_ex(def,"pt1.y");
  pt2.x=JFetch_NUMBER_ex(def,"pt2.x");
  pt2.y=JFetch_NUMBER_ex(def,"pt2.y");



  float margin=JFetch_NUMBER_ex(def,"margin");

  float edge_surpress=JFetch_NUMBER_ex(def,"edge_surpress",10);


  float cropHeight=hypotf(pt1.x-pt2.x,pt1.y-pt2.y);

  cv::Size cropSize(margin*2/mmpp,cropHeight/mmpp);





  cv::Mat temp2line_view=
      cvM3x3::translate({(float)cropSize.width/2,(float)cropSize.height/2})*
      cvM3x3::scale(1/mmpp)*
      line_from_template(pt1,pt2,-M_PI/2);


  cv::Mat img2line_view=temp2line_view*mat_img2template;

  img2line_view=cvM3x3::mat33to23(img2line_view);

  std::cout<<"cropSize:"<<cropSize<<std::endl;
  std::cout<<"img2line_view:"<<std::endl<<img2line_view<<std::endl;



  cv::Mat line_view2temp;
  cv::invert(temp2line_view,line_view2temp);
  line_view2temp=cvM3x3::mat33to23(line_view2temp);

  if(0){//check if the line is in the image

    cv::Mat mat_template2img=mat_img2template.inv();
    cv::Mat mat_template2img_23 = cvM3x3::mat33to23(mat_template2img);
    float delta = cv::determinant(mat_template2img);

    vector<cv::Point2f> pts_on_template_coord={pt1,pt2};
    vector<cv::Point2f> pts_on_line_coord;
    cv::transform(pts_on_template_coord,pts_on_line_coord, mat_template2img_23); 
    cv::Point2f pt1_img=pts_on_line_coord[0];
    cv::Point2f pt2_img=pts_on_line_coord[1];
    if(pt1_img.x-margin<0 || pt1_img.x+margin>srcImg.cols ||
       pt1_img.y-margin<0 || pt1_img.y+margin>srcImg.rows ||
       pt2_img.x-margin<0 || pt2_img.x+margin>srcImg.cols ||
       pt2_img.y-margin<0 || pt2_img.y+margin>srcImg.rows)
    {
      info.result.error_msg="line is out of image";
      LOGE("%s",info.result.error_msg.c_str());
      return false;
    }
  }

  Mat transformLineImg;
  //it would be a vertical line image(top center is pt1, bottom center is pt2)
  //scan from left to right, and find the edge point
  cv::warpAffine(srcImg, transformLineImg, img2line_view,cropSize, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  vector<cv::Mat> _dbg_Images;
  vector<Point3f> edge_points=EdgeLocExtraction(name,transformLineImg,def,_dbg_Images);

  {//TODO:edge_points filter, remove deviant points

  }
  
  if (!edge_points.empty()) {
    


    acv_Line tmp_line;
    float sigma;


    bool is_dual_apex_line=JFetch_BOOL(def,"dual_apex_line",false);


    bool go_normal_fitting_seq=is_dual_apex_line==false;

    if(is_dual_apex_line==true)
    {
      // info.result.error_code=-1;
      // info.result.error_msg="is_dual_apex_line is not implemented";

      float dual_apex_line_length_ratio_threshold=JFetch_NUMBER_ex(def,"dual_apex_line_length_ratio_threshold",0.5);

      // edge_points
      vector<Point2f> consider_points;
      for(int i=0;i<edge_points.size();i++)
      {
        if(edge_points[i].z>0)
          consider_points.push_back({edge_points[i].x,edge_points[i].y});
      }


      float extDist=10000000;
      consider_points.push_back({extDist, extDist});
      consider_points.push_back({extDist,-extDist});
      
      std::vector<cv::Point2f> hull;
      cv::convexHull(consider_points, hull);





      int lineSize=transformLineImg.rows;


      float max_dist=0;
      int max_dist_idx=-1;

      cv::Point2f pre_point=hull[hull.size()-1];
      for(int i=0;i<hull.size();i++)
      {
        cv::Point2f cur_point=hull[i];

        float dist=hypotf(pre_point.x-cur_point.x,pre_point.y-cur_point.y);
        LOGE("[%d]:line hull (%.2f,%.2f)->(%.2f,%.2f),dist:%.2f",i,pre_point.x,pre_point.y,cur_point.x,cur_point.y,dist);
        if(dist>extDist/2)//extension points involved
        {
          LOGE("line hull extension points involved");
          pre_point=cur_point;
          continue;
        }

        if(max_dist<dist)
        {
          max_dist=dist;
          max_dist_idx=i;
          LOGE("max_dist:%f,max_dist_idx:%d",max_dist,max_dist_idx);
        }
        pre_point=cur_point;
      }

      if(max_dist>lineSize*dual_apex_line_length_ratio_threshold)
      {//the distance is good enough, construct a line


        LOGE("max_dist_idx:%d",max_dist_idx);
        int max_dist_pre_idx=max_dist_idx-1;
        if(max_dist_pre_idx<0)max_dist_pre_idx=hull.size()-1;
        

        tmp_line.line_anchor.X=hull[max_dist_pre_idx].x;
        tmp_line.line_anchor.Y=hull[max_dist_pre_idx].y;
        tmp_line.line_vec.X=hull[max_dist_idx].x-hull[max_dist_pre_idx].x;
        tmp_line.line_vec.Y=hull[max_dist_idx].y-hull[max_dist_pre_idx].y;

        LOGE("line anchor:%f,%f,vec:%f,%f",tmp_line.line_anchor.X,tmp_line.line_anchor.Y,tmp_line.line_vec.X,tmp_line.line_vec.Y);

        // info.result.error_code=-1;
        // info.result.error_msg="dual_apex_line mode failed";
        // return false;
      }
      else
      {//the distance is not good enough, go to normal matching seq

        go_normal_fitting_seq=true;

      }

    }




    if(go_normal_fitting_seq==true)
    {
      acvFitLine(
          &(edge_points[0].x), sizeof(Point3f),
          &(edge_points[0].z), sizeof(Point3f), edge_points.size(), &tmp_line, &sigma);

      // std::cout<<"tmp_line:"<<tmp_line.line_anchor.X<<","<<tmp_line.line_anchor.Y<<","<<tmp_line.line_vec.X<<","<<tmp_line.line_vec.Y<<std::endl;


      if(tmp_line.line_vec.Y<0)//make sure the line is from top to bottom
      {
        tmp_line.line_vec.Y=-tmp_line.line_vec.Y;
        tmp_line.line_vec.X=-tmp_line.line_vec.X;
      }
      //draw the line on the image
      {//TODO:check if to stick to outmost edge point 

        bool align_to_apex=JFetch_TRUE(def,"align_to_apex");
        if(align_to_apex)
        {//find the 
          Point2f lineVec={tmp_line.line_vec.X,tmp_line.line_vec.Y};
          Point2f normalVec={-lineVec.y,lineVec.x};

          Point3f apex_pt;
          float max_score=-FLT_MAX;
          for(int i=0;i<edge_points.size();i++)
          {
            Point3f &ept=edge_points[i];
            if(ept.z<=0)continue;
            float score=normalVec.x*ept.x+normalVec.y*ept.y;
            if(max_score<score)
            {
              max_score=score;
              apex_pt=ept;
            }
          }

          if(max_score!=-FLT_MAX)
          {
            tmp_line.line_anchor.X=apex_pt.x;
            tmp_line.line_anchor.Y=apex_pt.y;
          }



        }

        float edge_offset=JFetch_NUMBER_ex(def,"edge_offset");
        if(edge_offset==edge_offset && edge_offset!=0)
        {
          tmp_line.line_anchor.X+=-tmp_line.line_vec.Y*edge_offset;
          tmp_line.line_anchor.Y+=tmp_line.line_vec.X*edge_offset;  
        }
      
      }
    }
    cv::Point2f pto(tmp_line.line_anchor.X,tmp_line.line_anchor.Y);
    cv::Point2f vo(tmp_line.line_vec.X,tmp_line.line_vec.Y);

    int H=transformLineImg.rows;
    cv::Point2f pt1(pto+vo*(H-pto.y)/vo.y);//to bottom

    cv::Point2f pt2(pto-vo*pto.y/vo.y);//to top


    LOGE("linec:pt1:%f,%f,pt2:%f,%f",pt1.x,pt1.y,pt2.x,pt2.y);

    vector<cv::Point2f> pts_on_line_coord={pt1,pt2,pto};

    vector<cv::Point2f> pts_on_template_coord;



    // cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);


    cv::transform(pts_on_line_coord, pts_on_template_coord, line_view2temp); 

    LOGE("back:pt1:%f,%f,pt2:%f,%f,pto:%f,%f",
    pts_on_template_coord[0].x,pts_on_template_coord[0].y,pts_on_template_coord[1].x,pts_on_template_coord[1].y,pts_on_template_coord[2].x,pts_on_template_coord[2].y);

    info.result.line.pt1.x=pts_on_template_coord[0].x;
    info.result.line.pt1.y=pts_on_template_coord[0].y;
    info.result.line.pt2.x=pts_on_template_coord[1].x;
    info.result.line.pt2.y=pts_on_template_coord[1].y;
    info.result.line.sigma=sigma;
    info.result.error_code=0;




    //attach dbg_info
    if(info.result.dbg_info!=NULL)
    {

      dbg_Images=_dbg_Images;
      vector<cv::Point2f> pts_on_line_coord;
      for(int i=0;i<edge_points.size();i++)
      {
        pts_on_line_coord.push_back({edge_points[i].x,edge_points[i].y});
      }

      vector<cv::Point2f> pts_on_template_coord;
      cv::transform(pts_on_line_coord, pts_on_template_coord, line_view2temp); 

      cJSON* jpts=cJSON_CreateArray();
      cJSON_AddItemToObject(info.result.dbg_info,"edge_points",jpts);
      for(int i=0;i<pts_on_template_coord.size();i++)
      {
        cJSON* jpt=cJSON_CreateObject();
        cJSON_AddItemToArray(jpts,jpt);
        cJSON_AddNumberToObject(jpt,"x",pts_on_template_coord[i].x);
        cJSON_AddNumberToObject(jpt,"y",pts_on_template_coord[i].y);
        cJSON_AddNumberToObject(jpt,"w",edge_points[i].z);
      }
    }


  }
  else
  {
    info.result.error_code=-1;
    info.result.error_msg="No edge points found";
    return false;
  }

  // cv::imwrite("data/"+name+"_transformLineImg.png", transformLineImg);
  // cv::imwrite("data/"+name+"_transformLineXSobel.png", transformLineXSobel);







  return true;
}



// static vector<ptInfo>  getLocalMeanEdgePoints(Mat grayXSobel,CenterType centerType)
// {


//   vector<ptInfo> edge_points;

//   for(int y=0;y<grayXSobel.rows;y+=1)
//   {
//     int16_t *row=grayXSobel.ptr<int16_t>(y);


//     float edge_w,edge_sigma;  

//     float edge_loc=getCenter_LOCAL_MEAN(
//       row,
//       grayXSobel.cols,
//       centerType,
//       edge_w,edge_sigma);


//     // float edge_loc=getCenter_LOCAL_FIRSTMax(
//     //   row,grayXSobel.cols,centerType,edge_w,edge_sigma);

//     if(edge_w==0 || edge_loc<2)continue;
//     // row[(int)edge_loc]=255;

//     // int8_t *imgrow=grayFeatureCoordImg.ptr<int8_t>(y);
//     // imgrow[(int)edge_loc*3]=255;
//     // imgrow[(int)edge_loc*3+1]=0;
//     // imgrow[(int)edge_loc*3+2]=0;

//     ptInfo pt;
//     pt.pt.X=edge_loc;
//     pt.pt.Y=y;
//     pt.w=edge_w/(edge_sigma+1);

//     edge_points.push_back(pt);
//   }

//   return edge_points;
// }


// Function to compute the mean of points
static void computeMeans(const std::vector<cv::Point3f>& points, double& meanX, double& meanY, double& sumW) {
    meanX = 0;
    meanY = 0;
    sumW = 0;
    #pragma omp parallel for reduction(+:meanX,meanY,sumW)
    for (const auto& point : points) {
        float w=point.z;
        if(w<0)continue;
        meanX += point.x * w;
        meanY += point.y * w;
        sumW += w;
    }
    meanX /= sumW;
    meanY /= sumW;
}

// RMS error function for the fitted circle
static float computeSigma(const std::vector<cv::Point3f>& points, const cv::RotatedRect& circle) {
    float sum = 0;

    float cx=circle.center.x;
    float cy=circle.center.y;
    float r=circle.size.width/2;
    for (const auto& point : points) {
        float dx = point.x - cx;
        float dy = point.y - cy;
        float distance = std::hypot(dx, dy);
        float error = distance - r; // Assuming the circle's radius is half of its width
        sum += point.z * error * error;
    }
    return std::sqrt(sum / points.size());
}
static cv::RotatedRect CircleFit_Hyper(const std::vector<cv::Point3f>& points, float* rms) {
    int iter, IterMAX = 999;
    const double Four = 4.0f, Three = 3.0f, Two = 2.0f;

    double meanX, meanY, sumW;
    computeMeans(points, meanX, meanY, sumW);

    // Moments calculation
    double Mxx = 0, Myy = 0, Mxy = 0, Mxz = 0, Myz = 0, Mzz = 0;
    for (const auto& point : points) {
        float w=point.z;
        if(w<=0)continue;
        double Xi = point.x - meanX;
        double Yi = point.y - meanY;
        double Zi = Xi * Xi + Yi * Yi;
        Mxy += Xi * Yi * w;
        Mxx += Xi * Xi * w;
        Myy += Yi * Yi * w;
        Mxz += Xi * Zi * w;
        Myz += Yi * Zi * w;
        Mzz += Zi * Zi * w;
    }
    Mxx /= sumW;
    Myy /= sumW;
    Mxy /= sumW;
    Mxz /= sumW;
    Myz /= sumW;
    Mzz /= sumW;

    // Characteristic polynomial coefficients
    double Mz = Mxx + Myy;
    double Cov_xy = Mxx * Myy - Mxy * Mxy;
    double Var_z = Mzz - Mz * Mz;

    double A2 = Four * Cov_xy - Three * Mz * Mz - Mzz;
    double A1 = Var_z * Mz + Four * Cov_xy * Mz - Mxz * Mxz - Myz * Myz;
    double A0 = Mxz * (Mxz * Myy - Myz * Mxy) + Myz * (Myz * Mxx - Mxz * Mxy) - Var_z * Cov_xy;
    double A22 = A2 + A2;

    // Newton's method to find the root of the characteristic polynomial
    double x = 0, y = A0;
    for (iter = 0; iter < IterMAX; ++iter) {
        double Dy = A1 + x * (A22 + 16.0f * x * x);
        double xnew = x - y / Dy;
        if (std::fabs(xnew - x) < std::numeric_limits<double>::epsilon()) break;
        double ynew = A0 + xnew * (A1 + xnew * (A2 + Four * xnew * xnew));
        if (std::fabs(ynew) >= std::fabs(y)) break;
        x = xnew;
        y = ynew;
    }

    // Calculate the circle parameters
    double DET = x * x - x * Mz + Cov_xy;
    double Xcenter = (Mxz * (Myy - x) - Myz * Mxy) / (DET * Two);
    double Ycenter = (Myz * (Mxx - x) - Mxz * Mxy) / (DET * Two);

    cv::RotatedRect rect;
    rect.center = cv::Point2f(Xcenter + meanX, Ycenter + meanY);
    double r=std::sqrt(Xcenter * Xcenter + Ycenter * Ycenter + Mz - x - x);
    rect.size = cv::Size2f(r*2,r*2);

    float sigma=computeSigma(points, rect); // RMS error
    rect.angle = sigma; //just to overload the fitting sigma


    if(rms)
      *rms = sigma; // RMS error

    return rect;
}




bool InspectArcFitFeature(const cv::Mat &mat_img2template,float mmpp,vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{
  auto &info=fqList[featureIdx];
  LOGE("=========ArcFit");
  if(info.inputInfo==NULL)return false;

  auto &inputInfo=info.inputInfo;
  cJSON *def=info.cached_feature_def;

  info.result.result_type=StageInfo_DimMeasure::CIRCLE;
  info.result.circle.c.x=NAN;
  info.result.circle.c.y=NAN;
  info.result.circle.r=NAN;
  info.result.error_code=-1;
  // info.result.error_msg="Not implemented";


  StageInfo_Orientation::orient pose;// = inputInfo->orientation[info.orientation_idx];
  inputInfo->get_report_object(info.orientation_idx,pose);

  pose.angle-=info.template_angle;
  string name=JFetch_STRING_ex(def,"name");

  Mat srcImg=inputInfo->img;

  Point2f pt1,pt2,pt3;
  pt1.x=JFetch_NUMBER_ex(def,"pt1.x");
  pt1.y=JFetch_NUMBER_ex(def,"pt1.y");
  pt2.x=JFetch_NUMBER_ex(def,"pt2.x");
  pt2.y=JFetch_NUMBER_ex(def,"pt2.y");
  pt3.x=JFetch_NUMBER_ex(def,"pt3.x");
  pt3.y=JFetch_NUMBER_ex(def,"pt3.y");



  float margin=JFetch_NUMBER_ex(def,"margin")/mmpp;//in image scale

  float edge_surpress=JFetch_NUMBER_ex(def,"edge_surpress",10);

  bool from_outer_margin=JFetch_TRUE(def,"from_outer_margin");
  bool is_full_circle=JFetch_TRUE(def,"is_full_circle");

  // CenterType centerType=JFetch_STRING_ex(def,"center_type","local_avg");  

  LOGI("=========execute_inspection");
  cv::Mat img2spoint_view=mat_img2template;
  cv::Mat img2spoint_view_inv=img2spoint_view.inv();
  img2spoint_view_inv=cvM3x3::mat33to23(img2spoint_view_inv);
  img2spoint_view=cvM3x3::mat33to23(img2spoint_view);
  {//transform pt1,pt2,pt3 to template_coord

    vector<cv::Point2f> pts={pt1,pt2,pt3};
    cv::transform(pts,pts,img2spoint_view_inv);
    pt1=pts[0];
    pt2=pts[1];
    pt3=pts[2];

    LOGE("pt1:%f,%f,pt2:%f,%f,pt3:%f,%f",pt1.x,pt1.y,pt2.x,pt2.y,pt3.x,pt3.y);
  }






  cv::Point2f center;
  float radius;
  float startAngle, endAngle;
  InspTarUtil::findCircleFrom3PointsWithArc(pt1,pt2,pt3,center,radius,startAngle,endAngle);
  if(is_full_circle)
  {
    endAngle=startAngle+M_PI*2;
  }
  std::cout<<"center:"<<center.x<<","<<center.y<<",R"<<radius<<",ang:"<<startAngle* 180.0f / CV_PI<<","<<endAngle* 180.0f / CV_PI<<std::endl;
  std::cout<<"*margin:"<<margin<<" mmpp:"<<mmpp<<std::endl;

  // {//check if the circle is in the image
  //   if(center.x-radius-margin<0 || center.x+radius+margin>srcImg.cols ||
  //      center.y-radius-margin<0 || center.y+radius+margin>srcImg.rows)
  //   {
  //     LOGE("circle is out of image");
  //     return false;
  //   }
  // }


  cv::Mat mapX,mapY;
  genWarpPolarXYMap(center,radius-margin,radius+margin  ,startAngle,endAngle,mapX,mapY,1,1,from_outer_margin,mmpp);

  std::cout<<"mapX:"<<mapX.size()<<std::endl;
  std::cout<<"mapY:"<<mapY.size()<<std::endl;
  cv::Mat polarSegment;
  cv::remap(srcImg  , polarSegment, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));


  vector<cv::Mat> _dbg_Images;
  vector<Point3f> edgeLoc=EdgeLocExtraction(name,polarSegment,def,_dbg_Images);


  // cv::imwrite("data/"+name+"_edgeValueTraceMat.png", edgeValueTraceMat);
  

  // cv::imwrite("data/"+name+"_polarSegment_YBlur.png", polarSegment_YBlur);
  LOGE("edgeLoc.size():%d",edgeLoc.size());


  vector<Point3f> pix_edge_points;
  if(edgeLoc.size()<10)
  {
    string emsg="edgeLoc.size()="+std::to_string(edgeLoc.size())+" <10 ,skip";
    LOGE("%s",emsg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=emsg;
  }
  else
  {



    {//convert edgeLoc(polar coordinate) to pix_edge_points(XY image coordinate)

      int edge_len=edgeLoc.size();
      for(int i=0;i<edge_len;i++){
        float w=edgeLoc[i].z;
        if(w<=0)
        {
          // LOGE("surpress edgeLoc[%d]:%f,%f",i,edgeLoc[i].x,edgeLoc[i].y);
          // continue;
          //skip this point
          // continue;
          w=0;
        }
        float x=edgeLoc[i].x;
        int y=edgeLoc[i].y;
        //find mapX,mapY
        int floor_x=floor(x);
        int ceil_x=ceil(x);
        float ratio_x=(x-floor_x);

        // LOGE("edgeLoc[%d]:%f,%f  => fc:%d,%d",i,edgeLoc[i].pt.X,edgeLoc[i].pt.Y,floor_x,ceil_x);
        Point3f edge_point;
        edge_point.x=
          mapX.at<float>(y,floor_x)*(1-ratio_x)+
          mapX.at<float>(y,ceil_x)*ratio_x;

        edge_point.y=
          mapY.at<float>(y,floor_x)*(1-ratio_x)+
          mapY.at<float>(y,ceil_x)*ratio_x;


        edge_point.z=w;
        // LOGE("edgeLoc[%d]:%f,%d  => %f,%f,w:%f",i,x,y,edge_point.x,edge_point.y,w);

        pix_edge_points.push_back(edge_point);


        
      }
    }



    LOGE("=========CircleFit_Hyper");
    float sigma=NAN;
    cv::RotatedRect res= CircleFit_Hyper(pix_edge_points,&sigma);
    Point2f ptOnRight=res.center+Point2f{res.size.width/2,0};
    vector<cv::Point2f> center_on_img={res.center,ptOnRight};
    vector<cv::Point2f> center_on_template;
    cv::transform(center_on_img, center_on_template, img2spoint_view); 
    
    info.result.circle.c.x=center_on_template[0].x;
    info.result.circle.c.y=center_on_template[0].y;
    // info.result.circle.r=res.size.width/2;//it's the radius on the image
    info.result.circle.r=norm(center_on_template[1]-center_on_template[0]);//it's the radius on the template
    // info.result.circle.sigma=sigma;
    info.result.error_code=0;
    info.result.error_msg="";


    LOGE("=========END");


  }
  
  //attach dbg_info
  if(info.result.dbg_info!=NULL)
  {

    // dbg_Images.push_back(XSobel);
    // dbg_Images.push_back(edgeValueTraceMat);
    dbg_Images=_dbg_Images;
    LOGE("=========attach dbg_info");

    if(pix_edge_points.size()>0)
    {
      LOGE("=========attach pix_edge_points");

      vector<cv::Point2f> pts_on_line_coord;
      for(int i=0;i<pix_edge_points.size();i++)
      {
        pts_on_line_coord.push_back({pix_edge_points[i].x,pix_edge_points[i].y});
      }

      vector<cv::Point2f> pts_on_template_coord;
      cv::transform(pts_on_line_coord, pts_on_template_coord, img2spoint_view); 

      cJSON* jpts=cJSON_CreateArray();
      cJSON_AddItemToObject(info.result.dbg_info,"edge_points",jpts);
      for(int i=0;i<pts_on_template_coord.size();i++)
      {
        cJSON* jpt=cJSON_CreateObject();
        cJSON_AddItemToArray(jpts,jpt);
        cJSON_AddNumberToObject(jpt,"x",pts_on_template_coord[i].x);
        cJSON_AddNumberToObject(jpt,"y",pts_on_template_coord[i].y);
        cJSON_AddNumberToObject(jpt,"w",pix_edge_points[i].z);
      }
    }
  }


  return true;
}






Point2f getCenterPt(InspectionTarget_DimMeasure::itemInfo &info)
{
  switch(info.result.result_type)
  {
    case StageInfo_DimMeasure::LINE:
      return (info.result.line.pt1+info.result.line.pt2)/2;
    case StageInfo_DimMeasure::POINT:
      return info.result.point.pt1;
    case StageInfo_DimMeasure::CIRCLE:
      return info.result.circle.c;
    default:
      return {NAN,NAN};
  }

  return {NAN,NAN};
}

int findRefIdx(cJSON *ref_array,string target_ref_type)
{
  int refSize=cJSON_GetArraySize(ref_array);
  
  for(int i=0;i<refSize;i++)
  {
    cJSON *ref_obj=cJSON_GetArrayItem(ref_array,i);
    string ref_type=JFetch_STRING_ex(ref_obj,"type");
    if(ref_type==target_ref_type)
      return i;
  }
  return -1;
}

Point2f getPoint(cJSON *jpt)
{
  if(jpt==NULL)return {NAN,NAN};
  return {(float)JFetch_NUMBER_ex(jpt,"x",NAN),(float)JFetch_NUMBER_ex(jpt,"y",NAN)};
}



Point2f rotate2d_sc(Point2f pt, float sin_v,float cos_v,int flipF=1) {
  if(flipF>=0)flipF=1;
  else flipF=-1;
  return {
    pt.x * cos_v -flipF* pt.y * sin_v,
    pt.x * sin_v +flipF* pt.y * cos_v
  };
}

Point2f rotate2d(Point2f pt, float theta,int flipF=1) {
  float sin_v = sinf(theta);
  float cos_v = cosf(theta);
  return rotate2d_sc(pt,sin_v,cos_v,flipF);
}




bool InspectMeasureDistanceFeature(vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{
  auto &info=fqList[featureIdx];
  if(info.refIdIdx.size()<3)
  {
    LOGE("ref count<3");
    info.result.error_code=-1;
    info.result.error_msg="ref count<3";
    return false;
  }
  cJSON *ref_array=JFetch_ARRAY(info.cached_feature_def,"ref");


  int distance_select=JFetch_NUMBER_ex(info.cached_feature_def,"distance_select",0);

  //inf the index of ref list
  int obj1_ref_idx=findRefIdx(ref_array,"obj1");
  int obj2_ref_idx=findRefIdx(ref_array,"obj2");
  int obj_project_ref_idx=findRefIdx(ref_array,"obj_project");

  if(obj1_ref_idx==-1 || obj2_ref_idx==-1 || obj_project_ref_idx==-1)
  {
    string error_msg=
    "ref_idx not found obj1_ref_idx:"+std::to_string(obj1_ref_idx)+
    ",obj2_ref_idx:"+std::to_string(obj2_ref_idx)+
    ",obj_project_ref_idx:"+std::to_string(obj_project_ref_idx);
    LOGE("%s",error_msg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=error_msg;
    return false;
  }


  LOGE("ref_idx:%d,%d,%d",obj1_ref_idx,obj2_ref_idx,obj_project_ref_idx);
  //info.refIdIdx[obj1_ref_idx].idx is the index of obj1 in fqList
  //get the ref info  
  InspectionTarget_DimMeasure::itemInfo &obj1_result=fqList[info.refIdIdx[obj1_ref_idx].idx];
  InspectionTarget_DimMeasure::itemInfo &obj2_result=fqList[info.refIdIdx[obj2_ref_idx].idx];
  InspectionTarget_DimMeasure::itemInfo &obj_project_result=fqList[info.refIdIdx[obj_project_ref_idx].idx];

  if(obj_project_result.result.result_type!=StageInfo_DimMeasure::LINE)
  {
    LOGE("obj_project is not a line");
    info.result.error_code=-1;
    info.result.error_msg="obj_project is not a line";
    return false;
  }

  Point2f obj1_pt1=getCenterPt(obj1_result);
  Point2f obj2_pt1=getCenterPt(obj2_result);
  Point2f obj_project_pt1=getCenterPt(obj_project_result);
  Point2f obj_project_vec=obj_project_result.result.line.pt2-obj_project_result.result.line.pt1;
  obj_project_vec=obj_project_vec/norm(obj_project_vec);//normalize

  float projectRotateTheta=JFetch_NUMBER_ex(info.cached_feature_def,"rotate",M_PI/2);
  Point2f projectVec_rotated=rotate2d(obj_project_vec,projectRotateTheta);
  Point2f projectVec_rotated_normal={-projectVec_rotated.y,projectVec_rotated.x};

  Point2f intersec1, intersec2;


  float distance=NAN;
  {
    struct Line2f {
      Point2f pt1;
      Point2f pt2;
    };

    Line2f line1 = {obj1_pt1, obj1_pt1 + projectVec_rotated_normal};
    Line2f line2 = {obj2_pt1, obj2_pt1 + projectVec_rotated_normal};
    Line2f linec = {obj_project_pt1, obj_project_pt1 + projectVec_rotated};

    bool found1 = false, found2 = false;

    // Line intersection calculation
    auto lineIntersection = [](const Line2f& line1, const Line2f& line2, 
                              Point2f& intersection) -> bool {
      float x1 = line1.pt1.x, y1 = line1.pt1.y;
      float x2 = line1.pt2.x, y2 = line1.pt2.y;
      float x3 = line2.pt1.x, y3 = line2.pt1.y;
      float x4 = line2.pt2.x, y4 = line2.pt2.y;

      float denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
      if (std::abs(denominator) < 1e-6) return false;  // Lines are parallel

      float t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denominator;
      intersection.x = x1 + t * (x2 - x1);
      intersection.y = y1 + t * (y2 - y1);
      return true;
    };

    found1 = lineIntersection(line1, linec, intersec1);
    found2 = lineIntersection(line2, linec, intersec2);


    {
      distance = norm(intersec1 - intersec2);
      int ds=distance_select;

      if(ds==1 || ds==2)
      {
        Point2f vector_to_project={intersec2.x-intersec1.x,intersec2.y-intersec1.y};
        vector_to_project=vector_to_project/norm(vector_to_project);
        float dprod = vector_to_project.x * projectVec_rotated.x + 
              vector_to_project.y * projectVec_rotated.y;
        // LOGE(">>>>>>>>>>>>>>dprod:%f",dprod);
        dprod=(dprod>0)?1:-1;
        if(ds==2)dprod*=-1;
        distance=dprod*distance;
      }
    }

    LOGE("intersec1:%f,%f,intersec2:%f,%f",intersec1.x,intersec1.y,intersec2.x,intersec2.y);
    LOGE("line1:%f,%f,%f,%f,  line2:%f,%f,%f,%f,  linec:%f,%f,%f,%f",
    line1.pt1.x,line1.pt1.y,line1.pt2.x,line1.pt2.y,
    line2.pt1.x,line2.pt1.y,line2.pt2.x,line2.pt2.y,
    linec.pt1.x,linec.pt1.y,linec.pt2.x,linec.pt2.y);

    if (!found1 || !found2) {
      info.result.error_code = -1;
      info.result.error_msg = "Failed to find intersection points";
      return false;
    }

  }

  info.result.result_type=StageInfo_DimMeasure::VALUE;
  info.result.value.value=distance;
  info.result.error_code=0;
  info.result.error_msg="";
  return true;
}



bool InspectMeasureAngleFeature(vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{
  auto &info=fqList[featureIdx];
  cJSON *ref_array=JFetch_ARRAY(info.cached_feature_def,"ref");

  //inf the index of ref list
  int obj1_ref_idx=findRefIdx(ref_array,"obj1");
  int obj2_ref_idx=findRefIdx(ref_array,"obj2");

  if(obj1_ref_idx==-1 || obj2_ref_idx==-1 )
  {
    string error_msg=
    "ref_idx not found obj1_ref_idx:"+std::to_string(obj1_ref_idx)+
    ",obj2_ref_idx:"+std::to_string(obj2_ref_idx);
    LOGE("%s",error_msg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=error_msg;
    return false;
  }


  LOGE("ref_idx:%d,%d,%d",obj1_ref_idx,obj2_ref_idx);
  //info.refIdIdx[obj1_ref_idx].idx is the index of obj1 in fqList
  //get the ref info  
  InspectionTarget_DimMeasure::itemInfo &obj1_result=fqList[info.refIdIdx[obj1_ref_idx].idx];
  InspectionTarget_DimMeasure::itemInfo &obj2_result=fqList[info.refIdIdx[obj2_ref_idx].idx];

  if(obj1_result.result.result_type!=StageInfo_DimMeasure::LINE || obj2_result.result.result_type!=StageInfo_DimMeasure::LINE)
  {
    string emsg=("obj1_result or obj2_result is not a line");
    LOGE("%s",emsg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=emsg;
    return false;
  }

  if(obj1_result.result.error_code || obj2_result.result.error_code)
  {
    string emsg=("obj1 error code:"+std::to_string(obj1_result.result.error_code)+
    ",obj2 error code:"+std::to_string(obj2_result.result.error_code));
    LOGE("%s",emsg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=emsg;
    return false;
  }

  int angle_select=JFetch_NUMBER_ex(info.cached_feature_def,"angle_select",0);
  angle_select%=8;
  if(angle_select<0)angle_select+=8;

  Point2f obj1_vec=obj1_result.result.line.pt2-obj1_result.result.line.pt1;
  Point2f obj2_vec=obj2_result.result.line.pt2-obj2_result.result.line.pt1;
  obj1_vec=obj1_vec/norm(obj1_vec);//normalize
  obj2_vec=obj2_vec/norm(obj2_vec);//normalize


  float as=atan2f(obj1_vec.y,obj1_vec.x);
  float ae=atan2f(obj2_vec.y,obj2_vec.x);

  float angle=NAN;


  {
    int spand=(angle_select/4);
    if(spand==1)
    {
      ae+=M_PI;
    }
  }


  switch(angle_select%4)
  {
    case 0:
      break;
    
    case 1://
    {
      float at=as;
      as=ae;
      ae=at;
      ae+=M_PI;
    }
      break;
    case 2://
      ae+=M_PI;
      as+=M_PI;
      break;
    case 3://
    {
      as+=M_PI;
      ae+=M_PI;

      float at=as;
      as=ae;
      ae=at;
      ae+=M_PI;
    }
      break;

      
    break;
  }


  angle=ae-as;

  {//make angle in [0,2*Math.PI] at any range
    angle = fmod(angle, M_PI*2);
    if(angle<0)
    {
      angle+=M_PI*2;
    }

    if(JFetch_TRUE(info.cached_feature_def,"is_principal_range"))
    {
      if(angle>M_PI)
      {
        angle-=M_PI*2;
      }
    }




    ae=as+angle;
    // ae=as+angle;
  }


  info.result.result_type=StageInfo_DimMeasure::VALUE;
  info.result.value.value=angle;
  if(angle!=angle)
  {
    info.result.error_code=-1;
    info.result.error_msg="angle value is NAN";
    return false;
  }
  info.result.error_code=0;
  info.result.error_msg="";
  return true;
}



bool InspectMeasureDiameterFeature(vector<InspectionTarget_DimMeasure::itemInfo> &fqList,int featureIdx,vector<cv::Mat> &dbg_Images)
{

  auto &info=fqList[featureIdx];
  cJSON *ref_array=JFetch_ARRAY(info.cached_feature_def,"ref");

  //inf the index of ref list
  int obj1_ref_idx=findRefIdx(ref_array,"obj1");
  if(obj1_ref_idx==-1)
  {
    string error_msg="obj1_ref_idx not found";
    LOGE("%s",error_msg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=error_msg;
    return false;
  }

  InspectionTarget_DimMeasure::itemInfo &obj1_result=fqList[info.refIdIdx[obj1_ref_idx].idx];

  if(obj1_result.result.result_type!=StageInfo_DimMeasure::CIRCLE)
  {
    string emsg=("obj1_result is not a ArcFit");
    LOGE("%s",emsg.c_str());
    info.result.error_code=-1;
    info.result.error_msg=emsg;
    return false;
  }


  Point2f center=obj1_result.result.circle.c;
  float radius=obj1_result.result.circle.r;

  info.result.result_type=StageInfo_DimMeasure::VALUE;

  bool is_radius=JFetch_TRUE(info.cached_feature_def,"is_radius");
  if(is_radius)
  {
    info.result.value.value=radius;
  }
  else
  {
    info.result.value.value=radius*2;
  }
  info.result.error_code=0;
  info.result.error_msg="";
  return true;
}

bool InspectionTarget_DimMeasure::executeFeature(const cv::Mat &mat_img2template,float mmpp,vector<itemInfo> &fqList,int featureIdx)
{

LOGE("wwwwwwfeatureIdx:%d,fqList.size:%d",featureIdx,fqList.size());
  if(featureIdx<0 || featureIdx>=fqList.size())
  {
    LOGE("featureIdx out of range,featureIdx:%d,fqList.size:%d",featureIdx,fqList.size());
    return false;
  }
  if(fqList[featureIdx].processed)
    return true;
  LOGE(">>>");
  auto &info=fqList[featureIdx];
  
  {//make all ref ready
    auto &refIdIdx=info.refIdIdx;

    for(int i=0;i<refIdIdx.size();i++)
    {
      if(fqList[refIdIdx[i].idx].processed==false)
      {
        executeFeature(mat_img2template,mmpp,fqList,refIdIdx[i].idx);

      }
    }

  }

  info.processed=true;
  string type=JFetch_STRING_ex(info.cached_feature_def,"type");

  info.result.error_code=-100;
  info.result.error_msg="Not implemented";


  LOGE(">>>type:%s",type.c_str());
  //timer start
  auto t0=cv::getTickCount();

  bool processed=false;
  bool procRet=false;
  if(type=="LineFit")
  {
    LOGE("=========LineFit");
    procRet=InspectLineFitFeature(mat_img2template,mmpp,fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="SearchPoint")
  {
    LOGE("=========SearchPoint");
    procRet=InspectSearchPointFeature(mat_img2template,mmpp,fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="ArcFit")
  {
    LOGE("=========ArcFit");
    procRet=InspectArcFitFeature(mat_img2template,mmpp,fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="Measure_Distance")
  {
    LOGE("=========Measure_Distance");
    procRet=InspectMeasureDistanceFeature(fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="Measure_Angle")
  {
    LOGE("=========Measure_Distance");
    procRet=InspectMeasureAngleFeature(fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="Measure_Diameter")
  {
    LOGE("=========Measure_Diameter");
    procRet=InspectMeasureDiameterFeature(fqList,featureIdx,dbg_Images);
    processed=true;
  }
  else if(type=="Calc")
  {
    LOGE("=========Calc");
  }

  //timer end
  auto t1=cv::getTickCount();
  // LOGE("TIME!! featureIdx:%d,type:%s,time:%f ms",featureIdx,type.c_str(),(t1-t0)*1000/cv::getTickFrequency()); 

  if(processed==true)
  {
    return procRet;
  }
  info.result.error_code=-9991;
  info.result.error_msg=std::string("Unknown feature type:")+type;
  //execute feature

  return false;
}

bool InspectionTarget_DimMeasure::executeCategory(const vector<itemInfo> &fqList,vector<cat_itemInfo> &catList,int categoryIdx)
{
  auto &info=catList[categoryIdx];


  info.result.sub_category.clear();
  for(int i=0;i<info.limits_setup.size();i++)
  {
    auto &limits_setup=info.limits_setup[i];


    int sub_category=0;

    do{

      if(limits_setup.ref_id_idx.idx<0 || limits_setup.ref_id_idx.idx>=fqList.size())
      {
        sub_category=0;//NA
        LOGE("ref_info has no viable index,idx:%d,id:%d",limits_setup.ref_id_idx.idx,limits_setup.ref_id_idx.id);
        break;
      }

      auto &ref_info=fqList[limits_setup.ref_id_idx.idx];
      if(ref_info.result.result_type!=StageInfo_DimMeasure::VALUE)
      {
        sub_category=0;//NA
        LOGE("ref_info is not a value  type:%d",ref_info.result.result_type);
        break;
      }

      if(ref_info.result.error_code!=0)
      {
        sub_category=0;//NA
        LOGE("ref_info has error_code:%d",ref_info.result.error_code);
        continue;
      }

      float value=ref_info.result.value.value;
      LOGE("value:%f,low_limit:%f,high_limit:%f",value,limits_setup.low_limit,limits_setup.high_limit);
      if(value==value)
      {
        if(//allow unset limits to judge
          ((limits_setup.low_limit==limits_setup.low_limit)   && (value<limits_setup.low_limit)) ||
          ((limits_setup.high_limit==limits_setup.high_limit) && (value>limits_setup.high_limit))
        )
        {//out of range
          if(limits_setup.NG_as=="NA")
            sub_category=0;
          else if(limits_setup.NG_as=="OK")
            sub_category=1;
          else
            sub_category=-1;

        }
        else
        {
          sub_category=1;
        }
      }
      else
      {
        sub_category=0;//NA
      }
    }while(0);

    info.result.sub_category.push_back(sub_category);
  }


  int red_category=999999;
  for(int i=0;i<info.result.sub_category.size();i++)
  {
    int sub_category=info.result.sub_category[i];
    if(sub_category==0 || red_category==0)//any NA => NA
    {
      red_category=0;
      break;
    }
    if(red_category>sub_category)//or choose the smallest
    {
      red_category=sub_category;
      continue;
    }
  }

  if(red_category==999999)
  {
    LOGE("no setup found");
    red_category=0;
  }

  info.result.category=red_category;

  return true;
}

shared_ptr<StageInfo_DimMeasure> InspectionTarget_DimMeasure::singleProcess(shared_ptr<StageInfo> sinfo,bool skipCache)
{
  // try to convert sinfo to StageInfo_Image
  shared_ptr<StageInfo_Orientation> sinfo_img = std::dynamic_pointer_cast<StageInfo_Orientation>(sinfo);
  if (sinfo_img == NULL)
  {
    LOGE("sinfo is not a StageInfo_Image");
    return shared_ptr<StageInfo_DimMeasure>();
  }


  // {//test dycast speed
  //   int runtimes=1000000;
  //   int64 t0;
  //   t0 = cv::getTickCount();
  //   for(int i=0;i<runtimes;i++)
  //   {
  //     std::dynamic_pointer_cast<StageInfo_Orientation>(sinfo);
  //   }
  //   LOGE("dycast speed: %f us", (cv::getTickCount() - t0)*1000000 / cv::getTickFrequency()/runtimes);


  //   t0 = cv::getTickCount();
  //   for(int i=0;i<runtimes;i++)
  //   {
  //     std::static_pointer_cast<StageInfo_Orientation>(sinfo);
  //   }
  //   LOGE("stcast speed: %f us", (cv::getTickCount() - t0)*1000000 / cv::getTickFrequency()/runtimes);




  // }


  if(skipCache==false)
  {
    cache_latest_input = sinfo_img;
  }
  cv::Mat srcImg = sinfo_img->img;

  auto t0 = cv::getTickCount();

  shared_ptr<StageInfo_DimMeasure> reportInfo(new StageInfo_DimMeasure());

  reportInfo->img_prop = sinfo_img->img_prop;
  reportInfo->set_source_id(this->id);
  reportInfo->set_trigger_id(sinfo_img->get_trigger_id());

  reportInfo->source = this;
  
  float mmpp=sinfo_img->get_mmpp();
  float template_angle=0;//JFetch_NUMBER_ex(def,"featureInfo.template_angle",0);

  LOGE("wwwwww");
  int obj_orientation_count=sinfo_img->get_report_count();
  // LOGE("sinfo_img use_count: %ld  p:%p from:%s", sinfo_img.use_count(),sinfo.get(),sinfo_img->source->name.c_str());
  LOGE("Orientation vector size: %zu", obj_orientation_count);

  for (int i = 0; i < obj_orientation_count; i++)
  {
    // std::cout<<"orientation["<<i<<"]:"<<orientation[i]<<std::endl;
    StageInfo_Orientation::orient pose;
    sinfo_img->get_report_object(i,pose);

    
    pose.angle-=template_angle;

    Mat mat_img2template=template_from_img(pose,mmpp);

// LOGE("wwwwww");
//     cv::Mat rotationMatrix = cv::getRotationMatrix2D(pose.center, pose.angle-template_angle, 1.0);
//     rotationMatrix.at<double>(0, 2) -= pose.center.x;
//     rotationMatrix.at<double>(1, 2) -= pose.center.y;
// LOGE("wwwwww");
// std::cout<<rotationMatrix<<std::endl;

//     cv::Mat object_to_template_coord_transform = rotationMatrix;//InspTarUtil::invertAffineTransform(rotationMatrix);;

// std::cout<<rotationMatrix<<std::endl;

    LOGE("wwwwww");
    // cJSON* jrep=cJSON_CreateArray();
    //reset featureQuickList processed flag
    for (int j = 0; j < featureQuickList.size(); j++)
    {
      featureQuickList[j].processed=false;
      featureQuickList[j].result.error_code=0;
      featureQuickList[j].result.error_msg="";
      featureQuickList[j].inputInfo=sinfo_img;
      featureQuickList[j].orientation_idx=i;
      featureQuickList[j].template_angle=template_angle;
      featureQuickList[j].result.dbg_info=NULL;
      if(dbg_feature_info!=NULL && featureQuickList[j].id==dbg_feature_id )
      {
        featureQuickList[j].result.dbg_info=cJSON_Duplicate(dbg_feature_info,cJSON_True);
      }
    }
    LOGE("wwwwww  featureQuickList.size():%d",featureQuickList.size());
    vector<StageInfo_DimMeasure::MeasureResultInfo> MeasureReport;
    for (int j = 0; j < featureQuickList.size(); j++)
    {
      executeFeature(mat_img2template,mmpp,featureQuickList,j);
      

      MeasureReport.push_back(featureQuickList[j].result);
      featureQuickList[j].result.dbg_info=NULL;
    }





    vector<StageInfo_DimMeasure::CategoryResultInfo> CategoryReport;
    LOGE("wwwwww  report.size():%d",MeasureReport.size());


    for (int j = 0; j < categoryQuickList.size(); j++)
    {
      categoryQuickList[j].result.category=0;
      executeCategory(featureQuickList,categoryQuickList,j);
      

      CategoryReport.push_back(categoryQuickList[j].result);
    }


    StageInfo_DimMeasure::DimMeasureResultInfo dimMeasureResultInfo;
    dimMeasureResultInfo.pose=pose;
    dimMeasureResultInfo.measureList=MeasureReport;
    dimMeasureResultInfo.categoryList=CategoryReport;



    // reportInfo->DimMeasureResultList.push_back(dimMeasureResultInfo);
    reportInfo->set_report_object(i,dimMeasureResultInfo);

  }

  for (int j = 0; j < featureQuickList.size(); j++)//clean up
  {
    featureQuickList[j].processed=false;
    featureQuickList[j].result.error_code=0;
    featureQuickList[j].result.error_msg="";
    featureQuickList[j].inputInfo=NULL;
    featureQuickList[j].orientation_idx=-1;
  }


  reportInfo->set_mmpp(sinfo_img->get_mmpp());

  reportInfo->img = sinfo_img->img;
  reportInfo->img_show = sinfo_img->img_show;

  reportInfo->set_process_time_us((cv::getTickCount() - t0) * 1000000 / cv::getTickFrequency());
  reportInfo->create_time_sysTick = t0;
  reportInfo->set_error_code(0,"");

  StageInfoFillDefault(reportInfo.get(), sinfo.get());

LOGE("wwwwww");
  // reportInfo->genJsonRepTojInfo();

LOGE("wwwwww");
  // cache_latest_result = reportInfo;

  return reportInfo;
}

InspectionTarget_DimMeasure::~InspectionTarget_DimMeasure()
{

  if(dbg_feature_info!=NULL)
  {
    cJSON_Delete(dbg_feature_info);
    dbg_feature_info=NULL;
  }
}
