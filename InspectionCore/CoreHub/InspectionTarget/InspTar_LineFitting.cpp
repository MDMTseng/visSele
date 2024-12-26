#include "InspTar_LineFitting.hpp"
#include "StageInfo_Orientation.hpp"
#include "StageInfo.hpp"
#include <iostream>
#include <opencv2/video/tracking.hpp>
#include <acvImage_BasicTool.hpp>
#include <random>
#include <numeric>
#include <circleFitting.h>
#include <InspTarUtil.hpp>
using namespace cv;

using namespace std;

template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}


void InspectionTarget_LineFitting::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{
  InspectionTarget::INIT(id,def,belongMan,local_env_path);
  LOGE("InspectionTarget_LineFitting INIT:");
  {//print def
    char *def_str=cJSON_Print(def);
    LOGE("%s",def_str);
    free(def_str);
  }

  LOGE("match_tags P:%p",match_tags);
  {//print  tags(cJSON array)

    int size=cJSON_GetArraySize(match_tags);
    for(int i=0;i<size;i++)
    {
      cJSON *tag = cJSON_GetArrayItem(match_tags,i);
      if(tag->type==cJSON_String)
        LOGE("%s",tag->valuestring);
    }
  }
}

void InspectionTarget_LineFitting::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);
}

void InspectionTarget_LineFitting::run()
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

bool InspectionTarget_LineFitting::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");
  

  return false;
}




static int STAGEINFO_SCS_CAT_BASIC_reducer_(int sum_cat,int cat)
{

  switch(sum_cat)
  {
    case STAGEINFO_CAT_UNSET:
      sum_cat=cat;
    break;
    case STAGEINFO_CAT_OK:
      if(cat==STAGEINFO_CAT_NG2 || cat==STAGEINFO_CAT_NG||cat==STAGEINFO_CAT_NA)
        sum_cat=cat;

    break;
    case STAGEINFO_CAT_NG2:
      if(cat==STAGEINFO_CAT_NG|| cat==STAGEINFO_CAT_NA)
        sum_cat=cat;
    break;

    case STAGEINFO_CAT_NG:
      if( cat==STAGEINFO_CAT_NA)
        sum_cat=cat;
    break;

    default:
    case STAGEINFO_CAT_NA:
    case STAGEINFO_CAT_NOT_EXIST:

    break;
  }

  return sum_cat;
}






void sLineFitting(shared_ptr<StageInfo_LineFitting> &s_result,cv::Mat& srcImg,cJSON* param) throw(std::invalid_argument)
{

  s_result->category=STAGEINFO_CAT_UNSET;
  auto start_time = std::chrono::high_resolution_clock::now();



  float scanregion_x=JFetch_NUMBER_ex(param,"region.x");
  float scanregion_y=JFetch_NUMBER_ex(param,"region.y");
  float scanregion_w=JFetch_NUMBER_ex(param,"region.w");
  float scanregion_h=JFetch_NUMBER_ex(param,"region.h");
  float scanregion_angle=JFetch_NUMBER_ex(param,"region.angle");//deg

  float sobelLowSurpress = JFetch_NUMBER_ex(param,"sobelLowSurpress",30);
  int edgeType=JFetch_NUMBER_ex(param,"edgeType",1);//1 for positive edge, -1 for negative edge, 0 for both

  if(
    scanregion_x!=scanregion_x || 
    scanregion_y!=scanregion_y || 
    scanregion_w!=scanregion_w || 
    scanregion_h!=scanregion_h)
  {
    char err_msg[1024];
    sprintf(err_msg,"scanregion_x:%f, scanregion_y:%f, scanregion_w:%f, scanregion_h:%f",scanregion_x, scanregion_y, scanregion_w, scanregion_h);
    throw std::invalid_argument(err_msg);
  }
  
  float sideRatio_threshold = JFetch_NUMBER_ex(param,"sideRatio_threshold",0.4);

  edgeType*=-1;//invert type



  // Create the rotation matrix
  cv::Mat transformMatrix = InspTarUtil::opencv_rotCrop_matrix(scanregion_x, scanregion_y, scanregion_angle*M_PI/180,1, scanregion_w/2, scanregion_h/2);
  cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);
  Mat srcImg_cropped;
  // Apply the affine transformation (rotation)
  cv::warpAffine(srcImg, srcImg_cropped, transformMatrix, cv::Size(scanregion_w,scanregion_h), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  // if(1){
  //   float noise_level = 10;
  //   cv::Mat noise = cv::Mat::zeros(srcImg_cropped.size(), CV_8UC1);
  //   cv::RNG rng(cv::getTickCount());
  //   rng.fill(noise, cv::RNG::NORMAL, 0, noise_level);
  //   cv::add(srcImg_cropped, noise, srcImg_cropped);
  // }

  // // Define a point
  // cv::Point2f pt1(1520, 100);
  // std::vector<cv::Point2f> pts1 = {pt1}, pts2, pts3;

  // // Map the point using m1
  // cv::transform(pts1, pts2, transformMatrix);  // Use cv::transform to apply affine transformation

  // // Map back the point using m2 (inverse transform)
  // cv::transform(pts2, pts3, invTransformMatrix);  // Use cv::transform to apply affine transformation

  // // Print results
  // std::cout << "pt1: " << pts1[0] << std::endl;
  // std::cout << "pt2: " << pts2[0] << std::endl;
  // std::cout << "pt3: " << pts3[0] << std::endl;

  // int edgeSearchType = 1;



  cv::Mat sobelImageX;
  cv::Sobel(srcImg_cropped, sobelImageX, CV_32F, 1, 0); // 1 for x-derivative to detect vertical edges

  cv::Mat sobelImageY;
  cv::Sobel(srcImg_cropped, sobelImageY, CV_32F, 0, 1); // 1 for x-derivative to detect vertical edges
  // cv::imwrite("sobelImageX.png", sobelImageX);

  float X_center = -1;
  float Y_center = -1;
  int skip_step=1;

  int edge_hit_line_CD = 1;


  typedef struct ptInfo
  {
    acv_XY pt;
    float w;
  };

  {

    vector<ptInfo> edge_points;
    for(int i=0;i<sobelImageX.rows;i+=skip_step){//Y

      int noedge_count = 0;
      int noedge_count_limit = 10;
      int edge_width = 0;

      float X_wsum=0;
      float X_weight=0;
      float X_wsum_sq=0; // Sum of squared values for variance calculation

      for(int j=0;j<sobelImageX.cols;j+=skip_step){//X
        float val = sobelImageX.at<float>(i,j);
        float valY = sobelImageY.at<float>(i,j);

        if(edgeType==1){
          if(val<0)continue;
        }else if(edgeType==-1){
          if(val>0)continue;
          val=-val;
        }else{
          if(val<0)val=-val;
        }

        float sideRatio = valY/(val+0.00001);
        if(sideRatio<0)sideRatio=-sideRatio;
        if(sideRatio>0.4)//
        {
          continue;
        }


        val-=sobelLowSurpress;
        if(val<0)val=0;
        
        if(val>0)
        {
          X_wsum+=j*val;
          X_weight+=val;
          X_wsum_sq+=j*j*val; // Accumulate squared values
          edge_width++;//add width
        }
        else
        {
          if(edge_width>0)//in edge counting state
          {
            noedge_count++;
            if(noedge_count>noedge_count_limit)//no edge for N times, maybe a empty region
            {
              if(edge_width>1)break;//wide enough
              //reset
              edge_width=0;
              X_wsum=0;
              X_weight=0;
              X_wsum_sq=0; // Reset squared sum
            }
          }
        }
      }

      if(edge_width>0)
      {
        X_center = X_wsum/X_weight;
        float variance = (X_wsum_sq/X_weight) - (X_center * X_center);
        float sigma = sqrt(variance); // Calculate standard deviation

        Y_center = i;
        //draw pixel on srcImg
        // cv::circle(srcImg_cropped, cv::Point(X_center, Y_center), 1, cv::Scalar(0, 0, 255), -1);

        // Output the standard deviation
        // std::cout << "Standard deviation (sigma) for row " << i << ": " << sigma << std::endl;
        ptInfo tmp_pt;
        tmp_pt.pt.X = X_center;
        tmp_pt.pt.Y = Y_center;
        tmp_pt.w = X_weight/(1+sigma);
        edge_points.push_back(tmp_pt);
      }
    }


    if (!edge_points.empty()) {
      
      acv_Line tmp_line;
      float sigma;
      acvFitLine(
          &(edge_points[0].pt), sizeof(ptInfo),
          &(edge_points[0].w), sizeof(ptInfo), edge_points.size(), &tmp_line, &sigma);

      std::cout<<"tmp_line:"<<tmp_line.line_anchor.X<<","<<tmp_line.line_anchor.Y<<","<<tmp_line.line_vec.X<<","<<tmp_line.line_vec.Y<<std::endl;


      float t0 = -tmp_line.line_anchor.Y/tmp_line.line_vec.Y;
      float t1 = (scanregion_h-tmp_line.line_anchor.Y)/tmp_line.line_vec.Y;

      std::cout<<"XY0:"<<tmp_line.line_anchor.X+tmp_line.line_vec.X*t0<<","<<tmp_line.line_anchor.Y+tmp_line.line_vec.Y*t0<<std::endl;
      std::cout<<"XY1:"<<tmp_line.line_anchor.X+tmp_line.line_vec.X*t1<<","<<tmp_line.line_anchor.Y+tmp_line.line_vec.Y*t1<<std::endl;

      cv::Point2f pt0(tmp_line.line_anchor.X+tmp_line.line_vec.X*t0,tmp_line.line_anchor.Y+tmp_line.line_vec.Y*t0);
      cv::Point2f pt1(tmp_line.line_anchor.X+tmp_line.line_vec.X*t1,tmp_line.line_anchor.Y+tmp_line.line_vec.Y*t1);
      cv::line(srcImg_cropped, pt0, pt1, cv::Scalar(0, 0, 0), 2);

      vector<cv::Point2f> pt0_ori,pt1_ori;
      cv::transform(vector<cv::Point2f>{pt0}, pt0_ori, invTransformMatrix); 
      cv::transform(vector<cv::Point2f>{pt1}, pt1_ori, invTransformMatrix); 
      cv::line(srcImg, pt0_ori[0], pt1_ori[0], cv::Scalar(0, 0, 0), 2);

      s_result->pt1_x=pt0_ori[0].x;
      s_result->pt1_y=pt0_ori[0].y;
      s_result->pt2_x=pt1_ori[0].x;
      s_result->pt2_y=pt1_ori[0].y;

      s_result->category=STAGEINFO_CAT_OK;

    }
  }

  // cv::imwrite("srcImg_rotated_cropped.png", srcImg_cropped);
  // cv::imwrite("srcImg_rotated.png", srcImg);


}




void TEST_InspTar_LineFitting(cv::Mat& srcImg,cJSON* param)
{

  /*
    ref json:
    {
      "region":{
        "x":269,
        "y":272,
        "w":300,
        "h":200,
        "angle":-178.1
      },
      "sobelLowSurpress":70,
      "edgeType":1,
      "sideRatio_threshold":0.4
    }
   */


  cJSON *mocking_param=cJSON_CreateObject();
  {
    //add pt1 object
    cJSON *pt1=cJSON_CreateObject();
    cJSON_AddNumberToObject(pt1,"x",269);
    cJSON_AddNumberToObject(pt1,"y",272);
    cJSON_AddNumberToObject(pt1,"w",200);
    cJSON_AddNumberToObject(pt1,"h",300);
    cJSON_AddNumberToObject(pt1,"angle",-178.1);

    cJSON_AddItemToObject(mocking_param,"region",pt1);
  }
   
  cJSON_AddNumberToObject(mocking_param,"sobelLowSurpress",70);
  cJSON_AddNumberToObject(mocking_param,"edgeType",1);

  shared_ptr<StageInfo_LineFitting> reportInfo(new StageInfo_LineFitting());
  sLineFitting(reportInfo,srcImg,mocking_param);

  cJSON_Delete(mocking_param);
}



void InspectionTarget_LineFitting::singleProcess(shared_ptr<StageInfo> sinfo)
{
  LOGI("InspectionTarget_LineFitting::singleProcess");

  int64 t0 = cv::getTickCount();


  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Image *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  cache_latest_input = sinfo;

  Mat CV_srcImg=d_sinfo->img;
  LOGE("srcImg size:%d,%d",CV_srcImg.cols,CV_srcImg.rows);
  Mat CV_srcImg_gray(CV_srcImg.rows,CV_srcImg.cols,CV_8UC1);

  cvtColor(CV_srcImg, CV_srcImg_gray, COLOR_BGR2GRAY);  
  LOGE("CV_srcImg_gray size:%d,%d",CV_srcImg_gray.cols,CV_srcImg_gray.rows);
  shared_ptr<StageInfo_LineFitting> reportInfo(new StageInfo_LineFitting());

  reportInfo->category=STAGEINFO_CAT_NA;

  LOGE(">>>>>>sLineFitting");
  try{
    sLineFitting(reportInfo,CV_srcImg_gray,def);
  }catch(std::invalid_argument e)
  {
    // LOGE("sLineFitting error:%s",e.what());
    // return;
    reportInfo->error_code=1;
    reportInfo->error_msg=e.what();
  }
  
  LOGE(">>>>>>sLineFitting");


  reportInfo->img_show =
      reportInfo->img = d_sinfo->img;
  

  StageInfoFillDefault(reportInfo.get(),sinfo.get());

  {
    int64 t1 = cv::getTickCount();
    double secs_us = 1000000 * (t1 - t0) / cv::getTickFrequency();
    reportInfo->process_time_us = secs_us;
    reportInfo->create_time_sysTick = t1;
    // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

    LOGI(">>>>>>>>process_time_us:%f", secs_us);
  }


  reportInfo->genJsonRepTojInfo();

  // cache_latest_result = reportInfo;
  belongMan->dispatch(reportInfo);

}



InspectionTarget_LineFitting::~InspectionTarget_LineFitting()
{
 
}
