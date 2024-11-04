#include "InspTar_DirectionalCaliper.hpp"
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


void InspectionTarget_DirectionalCaliper::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{
  InspectionTarget::INIT(id,def,belongMan,local_env_path);
}

void InspectionTarget_DirectionalCaliper::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);
}

void InspectionTarget_DirectionalCaliper::run()
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

bool InspectionTarget_DirectionalCaliper::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
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



static int main_testEdge() {
    // Load a single image where you want to locate the object
    cv::Mat image_ori = cv::imread("testImg.png", cv::IMREAD_GRAYSCALE);

    if (image_ori.empty()) {
        std::cerr << "Could not open or find the image!" << std::endl;
        return -1;
    }


    //add noise
    if(0){

      cv::Mat noise = cv::Mat::zeros(image_ori.size(), CV_8UC1);
      //init random number generator
      cv::RNG rng(cv::getTickCount());
      rng.fill(noise, cv::RNG::NORMAL, 0, 10);
      cv::add(image_ori, noise, image_ori);
    }



    float shift_x = -0.1f;
    float shift_y = 0.0f; 
    // Fake offset
    cv::Mat shiftedImage;
    {

      cv::Mat translationMatrix = (cv::Mat_<double>(2, 3) << 1, 0, shift_x,  // Shift by 5 pixels in the x direction
                                                            0, 1, shift_y); // Shift by 6 pixels in the y direction

      
      // Apply the affine transformation (translation)
      cv::warpAffine(image_ori, shiftedImage, translationMatrix, image_ori.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    }

    auto start_time = std::chrono::high_resolution_clock::now();


    int x_seed, y_seed;

    {
      // Threshold for detecting an edge (modify as needed)
      int Edge_thres = 170;


      int ds_factor = 10;
      cv::Mat ds_image;
      cv::resize(shiftedImage, ds_image, cv::Size(shiftedImage.cols/ds_factor, shiftedImage.rows/ds_factor));

      // Apply Sobel edge detection (detect vertical edges)
      cv::Mat sobelImage;
      cv::Sobel(ds_image, sobelImage, CV_8UC1, 1, 0); // 1 for x-derivative to detect vertical edges

      // Iterate over each column (left to right)
      bool pixFound = false;
      int y_sum = 0;
      int y_weight = 0;

      int x_sum = 0;
      int x_weight = 0;

      int sobel_sum = 0;
      for (int col = sobelImage.cols-1; col >=0; col--) { 
        sobel_sum=0;
        y_sum=0;
        y_weight=0;
        x_sum=0;
        x_weight=0;
        for (int row = 0; row < sobelImage.rows; row++) {
        // Iterate over each row in the column (top to bottom)
            // Check if the pixel value exceeds the edge threshold
            int edge_val = sobelImage.at<uchar>(row, col);
            sobel_sum += edge_val;
            y_sum+=edge_val*row;
            x_sum+=edge_val*col;
            y_weight+=edge_val;
            x_weight+=edge_val;
            if (sobelImage.at<uchar>(row, col) > Edge_thres) {
                // Found the first edge pixel in this column
                pixFound = true;
                // break; // Stop after finding the first edge pixel in the current column
            }



        }
        if(pixFound) break;
      }


      x_seed = ds_factor*x_sum/x_weight;
      y_seed = ds_factor*y_sum/y_weight;

      std::cout << "First edge pixel found at (" << x_seed << ", " << y_seed << ") with value: " 
                << (int)sobelImage.at<uchar>(y_sum/y_weight, x_sum/x_weight) << std::endl;
    }

    {
      cv::Mat ROIImage;
      int ROISize = 70;
      cv::Rect ROIRect(x_seed-ROISize, y_seed-ROISize/2, 2*ROISize, 2*ROISize/2);
      {
        cv::Mat(shiftedImage, ROIRect).copyTo(ROIImage);
      }

      // Apply Gaussian blur to the image
      // cv::GaussianBlur(ROIImage, ROIImage, cv::Size(25, 25), 40);
      cv::blur(ROIImage, ROIImage, cv::Size(25, 1));

      // cv::imwrite("ROIImage.png", ROIImage);
      cv::Mat ROISobelImage;
      cv::Sobel(ROIImage, ROISobelImage, CV_32F, 1, 0); // 1 for x-derivative to detect vertical edges floating point


      float x_sum = 0;
      float y_sum = 0;
      float weight = 0;
      for(int i=0;i<ROISobelImage.rows;i++){
        for(int j=0;j<ROISobelImage.cols;j++){
          float val = ROISobelImage.at<float>(i,j);
          x_sum += j*val;
          y_sum += i*val;
          weight += val;
        }
      }

      cv::Point2f targetPoint(x_sum/weight+ROIRect.x, y_sum/weight+ROIRect.y);
      std::cout << "Center of mass: (" << targetPoint.x << ", " << targetPoint.y << ")" << std::endl;
      std::cout << "est Center of : (" << shift_x+1608.47 << std::endl;

      //draw cross
      cv::line(shiftedImage, cv::Point(targetPoint.x-10, targetPoint.y), cv::Point(targetPoint.x+10, targetPoint.y), cv::Scalar(0, 0, 255), 2);
      cv::line(shiftedImage, cv::Point(targetPoint.x, targetPoint.y-10), cv::Point(targetPoint.x, targetPoint.y+10), cv::Scalar(0, 0, 255), 2);
      //write targetPoint to image
      std::stringstream ss;
      ss << "(" << targetPoint.x << ", " << targetPoint.y << ")";
      cv::putText(shiftedImage, ss.str(), targetPoint, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);

      // cv::imwrite("shiftedImage.png", shiftedImage);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Time taken for cv::calcOpticalFlowPyrLK: " << (float)duration << " ms" << std::endl;
    // Save the result image (Sobel output)


    // cv::imwrite("edge.png", sobelImage);
    
    return 0;
}

static void trackCenterOffset_TEST(const cv::Mat& prevImg, const cv::Mat& nextImg, const cv::Point2f& prevCenter) {
    std::vector<cv::Point2f> prevPts, nextPts;
    prevPts.push_back(prevCenter);


    // Optical flow status and error
    std::vector<uchar> status;
    std::vector<float> err;

    //timer start
    auto start_time = std::chrono::high_resolution_clock::now();
    // Calculate optical flow between the two images
    cv::calcOpticalFlowPyrLK(prevImg, nextImg, prevPts, nextPts, status, err);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    std::cout << "Time taken for cv::calcOpticalFlowPyrLK: " << (float)duration << " ms" << std::endl;

    if (status[0] == 1) {
        // Print the offset between the center points
        cv::Point2f offset = nextPts[0] - prevPts[0];
        std::cout << "Center point moved by offset: (" << offset.x << ", " << offset.y << ")" << std::endl;

        // Optionally, you can draw the points and offset on the image to visualize
        cv::Mat imgWithFlow;
        cv::cvtColor(nextImg, imgWithFlow, cv::COLOR_GRAY2BGR);
        cv::circle(imgWithFlow, prevPts[0], 5, cv::Scalar(0, 255, 0), -1);  // Previous center
        cv::circle(imgWithFlow, nextPts[0], 5, cv::Scalar(0, 0, 255), -1);  // New center
        cv::arrowedLine(imgWithFlow, prevPts[0], nextPts[0], cv::Scalar(255, 0, 0), 2);
        cv::imwrite("optical_flow.png", imgWithFlow);
        // cv::imshow("Optical Flow Tracking", imgWithFlow);
        // cv::waitKey(0);
    } else {
        std::cout << "Optical flow tracking failed!" << std::endl;
    }
}


static void sDirectionalCaliper(shared_ptr<StageInfo_DirectionalCaliper>& report,cv::Mat& srcImg,cJSON* param)
{

  auto start_time = std::chrono::high_resolution_clock::now();




  float scanregion_x=JFetch_NUMBER_ex(param,"region.x");
  float scanregion_y=JFetch_NUMBER_ex(param,"region.y");
  float scanregion_w=JFetch_NUMBER_ex(param,"region.w");
  float scanregion_h=JFetch_NUMBER_ex(param,"region.h");
  float scanregion_angle_deg=JFetch_NUMBER_ex(param,"region.angle");//deg
  float scanregion_angle=scanregion_angle_deg*M_PI/180;

  float sobelLowSurpress = JFetch_NUMBER_ex(param,"sobelLowSurpress",30);
  float sobelThreshold = JFetch_NUMBER_ex(param,"sobelThreshold",70);
  int edgeType=JFetch_NUMBER_ex(param,"edge_type",1);//1 for positive edge, -1 for negative edge, 0 for both





  // Create the rotation matrix
  cv::Mat transformMatrix = InspTarUtil::opencv_rotCrop_matrix(scanregion_x, scanregion_y, scanregion_angle,1, scanregion_w/2, scanregion_h/2);
  cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);
  Mat srcImg_cropped;
  // Apply the affine transformation (rotation)
  cv::warpAffine(srcImg, srcImg_cropped, transformMatrix, cv::Size(scanregion_w,scanregion_h), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  if(0){
    float noise_level = 10;
    cv::Mat noise = cv::Mat::zeros(srcImg_cropped.size(), CV_8UC1);
    cv::RNG rng(cv::getTickCount());
    rng.fill(noise, cv::RNG::NORMAL, 0, noise_level);
    cv::add(srcImg_cropped, noise, srcImg_cropped);
  }

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
  cv::Sobel(srcImg_cropped, sobelImageX, CV_16S, 1, 0); // 1 for x-derivative to detect vertical edges

  // cv::imwrite("sobelImageX.png", sobelImageX);


  float X_center = -1;
  float Y_center = -1;
  int skip_step=1;

  int edge_hit_line_CD = 1;
  // for(int j=0;j<sobelImageX.cols;j+=skip_step){//X
  //   float Y_wsum=0;
  //   float Y_weight=0;
  //   int count = 0;
  //   for(int i=0;i<sobelImageX.rows;i+=skip_step){//Y
  //     float val = sobelImageX.at<float>(i,j);



  //     if(edgeType==1){
  //       if(val<0)continue;
  //     }else if(edgeType==-1){
  //       if(val>0)continue;
  //       val=-val;
  //     }else{
  //       if(val<0)val=-val;
  //     }


  //     val-=sobelLowSurpress+sobelThreshold;
  //     if(val<0)val=0;
  //     Y_wsum+=i*val;
  //     Y_weight+=val;

  //     if(val>0)count++;
  //     if(Y_weight>0 && edge_hit_line_CD>0)break;
  //     // if(X_weight>0)break;//break right now/ comment this to find the average center of the edge
  //   }

  //   // std::cout<<"<<<<<<<edge_hit_line_CD:"<<edge_hit_line_CD<<"Y_weight:"<<Y_weight<<std::endl;
  //   if(Y_weight>0){
  //     if(edge_hit_line_CD>0)
  //     {
  //       edge_hit_line_CD--;
  //       continue;
  //     }
  //     Y_center = Y_wsum/Y_weight;
  //     X_center = j-10;
  //     if(X_center<0)X_center=0;
  //     break;
  //   }
    
  // }

  {
    int edge_hit_line_count_limit = 3;
    int edge_hit_line_count = 0;

    float Y_wsum=0;
    float Y_weight=0;
    for(int j=0;j<sobelImageX.cols;j+=skip_step){//X
      int count = 0;
      for(int i=0;i<sobelImageX.rows;i+=skip_step){//Y
        float val = sobelImageX.at<int16_t>(i,j);



        if(edgeType==1){
          if(val<0)continue;
        }else if(edgeType==-1){
          if(val>0)continue;
          val=-val;
        }else{
          if(val<0)val=-val;
        }


        val-=sobelLowSurpress+sobelThreshold;
        if(val<0)val=0;

        val/=(edge_hit_line_count+0.5);//reduce weight of edge hit line on over scan(scan column after 1st edge hit)
        Y_wsum+=i*val;
        Y_weight+=val;

        if(val>0)count++;
      }
      if(count==0)//no edge hit,reset
      {
        edge_hit_line_count=0;
        Y_wsum=0;
        Y_weight=0;
      }
      else
      {
        edge_hit_line_count++;
        if(edge_hit_line_count>edge_hit_line_count_limit)//consecutive edge hit line count reached
        {

          Y_center = Y_wsum/Y_weight;//find refine start point of edge
          X_center = j-edge_hit_line_count_limit-3-skip_step;
          if(X_center<0)X_center=0;
          break;
        }
      }
    
      
    }
  }


  if(X_center==-1){
    LOGE("No edge detected");
    return;
  }

  std::cout << "X_center: " << X_center <<" Y_center: "<<Y_center<<std::endl;
  // int extend_edge_x_search_range = 5;

  if(1){//refine Y_center

    float X_wsum=0;
    float X_weight=0;
    bool edge_hit = false;
    int zero_edge_count = 0;
    int zero_edge_count_limit = 3;
    for(int i=X_center;i<sobelImageX.cols;i++){
      float val = sobelImageX.at<int16_t>(Y_center,i);
      if(edgeType==1){
        if(val<0)continue;
      }else if(edgeType==-1){
        if(val>0)continue;
        val=-val;
      }else{
        if(val<0)val=-val;
      }
      val-=sobelLowSurpress;
      if(val<0)val=0;


      std::cout<<"val:"<<val<<std::endl;
      if(edge_hit==false){
        if(val>0)edge_hit=true;

      }else{
        if(val)
        {
          zero_edge_count=0;
        }else{//if no edge for N times, break
          zero_edge_count++;
          if(zero_edge_count>zero_edge_count_limit)break;
        }
      }
      X_wsum+=i*val;
      X_weight+=val;
    }

    X_center = X_wsum/X_weight;
  }

  std::cout << "X_center: " << X_center <<" Y_center: "<<Y_center<<std::endl;

  cv::Point2f pt1(X_center, Y_center);
  std::vector<cv::Point2f> pts1 = {pt1}, pts2;

  // Map the point using m1
  cv::transform(pts1, pts2, invTransformMatrix);  // Use cv::transform to apply affine transformation

  std::cout << "pt1: " << pts1[0] << std::endl;
  std::cout << "pt2: " << pts2[0] << std::endl;



  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  report->location_x=pts2[0].x;
  report->location_y=pts2[0].y;
  report->category=STAGEINFO_CAT_OK;

  if(1){//save debug image




    cv::line(srcImg_cropped, cv::Point(pts1[0].x-10, pts1[0].y), cv::Point(pts1[0].x+10, pts1[0].y), cv::Scalar(0, 0, 255), 2);
    cv::line(srcImg_cropped, cv::Point(pts1[0].x, pts1[0].y-10), cv::Point(pts1[0].x, pts1[0].y+10), cv::Scalar(0, 0, 255), 2);
    // cv::imwrite("srcImg_rotated_cropped.png", srcImg_cropped);

    //draw cross on srcImg at pts2[0]
    cv::line(srcImg, cv::Point(pts2[0].x-10, pts2[0].y), cv::Point(pts2[0].x+10, pts2[0].y), cv::Scalar(0, 0, 255), 2);
    cv::line(srcImg, cv::Point(pts2[0].x, pts2[0].y-10), cv::Point(pts2[0].x, pts2[0].y+10), cv::Scalar(0, 0, 255), 2);
    //write targetPoint to image
    std::stringstream ss;
    ss << "(" << pts2[0].x << ", " << pts2[0].y << ")";
    cv::putText(srcImg, ss.str(), pts2[0], cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);


    //draw line with scanegion_center and scanregion_angle
    cv::Point2f scanegion_center(scanregion_x, scanregion_y);
    cv::Point2f scanregion_angle_point1(scanregion_x + scanregion_w * cos(-scanregion_angle), scanregion_y + scanregion_w * sin(-scanregion_angle));
    cv::Point2f scanregion_angle_point2(scanregion_x - scanregion_w * cos(-scanregion_angle), scanregion_y - scanregion_w * sin(-scanregion_angle));
    cv::line(srcImg, scanregion_angle_point1, scanregion_angle_point2, cv::Scalar(0, 255, 0), 2);

    // cv::imwrite("srcImg_cropped.png", srcImg);
  }

}


void InspectionTarget_DirectionalCaliper::singleProcess(shared_ptr<StageInfo> sinfo)
{
  LOGI("InspectionTarget_DirectionalCaliper::singleProcess");

 LOGI("InspectionTarget_ArcFitting::singleProcess");


  int64 t0 = cv::getTickCount();


  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Image *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  auto srcImg=d_sinfo->img;

  cache_latest_input = sinfo;

  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);
  LOGE("srcImg size:%d,%d",CV_srcImg.cols,CV_srcImg.rows);
  Mat CV_srcImg_gray(CV_srcImg.rows,CV_srcImg.cols,CV_8UC1);

  cvtColor(CV_srcImg, CV_srcImg_gray, COLOR_BGR2GRAY);  
  LOGE("CV_srcImg_gray size:%d,%d",CV_srcImg_gray.cols,CV_srcImg_gray.rows);
  shared_ptr<StageInfo_DirectionalCaliper> reportInfo(new StageInfo_DirectionalCaliper());

  reportInfo->category=STAGEINFO_CAT_NA;

  LOGE(">>>>>>sDirectionalCaliper");
  try{
    sDirectionalCaliper(reportInfo,CV_srcImg_gray,def);
  }catch(std::invalid_argument e)
  {
    // LOGE("sLineFitting error:%s",e.what());
    // return;
    reportInfo->error_code=1;
    reportInfo->error_msg=e.what();
  }
  
  LOGE(">>>>>>sDirectionalCaliper");


  reportInfo->img_show =
      reportInfo->img = srcImg;
  

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



InspectionTarget_DirectionalCaliper::~InspectionTarget_DirectionalCaliper()
{
 
}
