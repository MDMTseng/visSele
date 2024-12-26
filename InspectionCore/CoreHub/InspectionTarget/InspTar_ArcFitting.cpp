#include "InspTar_ArcFitting.hpp"
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


void InspectionTarget_ArcFitting::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{
  InspectionTarget::INIT(id,def,belongMan,local_env_path);
}

void InspectionTarget_ArcFitting::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);
}

void InspectionTarget_ArcFitting::run()
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

bool InspectionTarget_ArcFitting::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
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





static void sssimgProcess_TEST_EllipseFitting(cv::Mat& srcImg,cJSON* param)
{

  auto start_time = std::chrono::high_resolution_clock::now();

  float scanegion_center_x=934;
  float scanegion_center_y=2305;

  float scanegion_w=300;
  float scanegion_h=500;

  float scanegion_angle=-135*M_PI/180;

  // Create the rotation matrix
  cv::Mat transformMatrix = InspTarUtil::opencv_rotCrop_matrix(scanegion_center_x, scanegion_center_y, scanegion_angle,1, scanegion_w/2, scanegion_h/2);
  cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);
  Mat srcImg_cropped;
  // Apply the affine transformation (rotation)
  cv::warpAffine(srcImg, srcImg_cropped, transformMatrix, cv::Size(scanegion_w,scanegion_h), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  if(1){
    float noise_level = 10;
    cv::Mat noise = cv::Mat::zeros(srcImg_cropped.size(), CV_8UC1);
    cv::RNG rng(cv::getTickCount());
    rng.fill(noise, cv::RNG::NORMAL, 0, noise_level);
    cv::add(srcImg_cropped, noise, srcImg_cropped);
  }

  cv::Mat sobelImageX;
  cv::Sobel(srcImg_cropped, sobelImageX, CV_32F, 1, 0); // 1 for x-derivative to detect vertical edges

  // cv::imwrite("sobelImageX.png", sobelImageX);

  float sobelLowSurpress = 70;
  float sobelThreshold = 70;

  float X_center = -1;
  float Y_center = -1;
  int skip_step=1;

  int edgeType=-1;//1 for positive edge, -1 for negative edge, 0 for both

  int edge_hit_line_CD = 1;

  {

    vector<cv::Point2f> edge_points;
    for(int i=0;i<sobelImageX.rows;i+=skip_step){//Y

      int noedge_count = 0;
      int noedge_count_limit = 10;
      int edge_width = 0;

      float X_wsum=0;
      float X_weight=0;
      float X_wsum_sq=0; // Sum of squared values for variance calculation

      for(int j=0;j<sobelImageX.cols;j+=skip_step){//X
        float val = sobelImageX.at<float>(i,j);

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
        //draw pixel on srcImg do not use circle
        cv::circle(srcImg_cropped, cv::Point(X_center, Y_center), 1, cv::Scalar(0, 0, 255), -1);

        // Output the standard deviation
        // std::cout << "Standard deviation (sigma) for row " << i << ": " << sigma << std::endl;

        edge_points.push_back(Point2f(X_center,Y_center));
      }
    }


    if (!edge_points.empty()) {
      
      cv::RotatedRect tmp_rect = cv::fitEllipse(edge_points);
      cv::ellipse(srcImg_cropped, tmp_rect, cv::Scalar(0, 0, 0), 2);
      
    }
  }

  // cv::imwrite("srcImg_rotated_cropped.png", srcImg_cropped);

  

}



// void drawSmoothCircle(cv::Mat& img, cv::Point2f center, float radius, int segments, cv::Scalar color, int thickness = 1) {
//     std::vector<cv::Point> points;

//     // Generate points along the circle's perimeter
//     for (int i = 0; i < segments; ++i) {
//         float theta = 2.0f * CV_PI * i / segments;  // Angle in radians
//         int x = static_cast<int>(center.x + radius * std::cos(theta));
//         int y = static_cast<int>(center.y + radius * std::sin(theta));
//         points.emplace_back(x, y);
//     }

//     // Draw the polygonal approximation of the circle
//     for (int i = 0; i < segments; ++i) {
//         cv::line(img, points[i], points[(i + 1) % segments], color, thickness);
//     }
// }

static void sssimgProcess_TEST_CircleFitting_Simple(cv::Mat& srcImg,cJSON* param)
{

  auto start_time = std::chrono::high_resolution_clock::now();

  float scanegion_center_x=2000;
  float scanegion_center_y=942;

  float scanegion_w=300;
  float scanegion_h=500;

  float scanegion_angle=-135*M_PI/180;

  // Create the rotation matrix
  cv::Mat transformMatrix = InspTarUtil::opencv_rotCrop_matrix(scanegion_center_x, scanegion_center_y, scanegion_angle,1, scanegion_w/2, scanegion_h/2);
  cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);
  Mat srcImg_cropped;
  // Apply the affine transformation (rotation)
  cv::warpAffine(srcImg, srcImg_cropped, transformMatrix, cv::Size(scanegion_w,scanegion_h), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

  if(1){
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
  cv::Sobel(srcImg_cropped, sobelImageX, CV_32F, 1, 0); // 1 for x-derivative to detect vertical edges

  // cv::imwrite("sobelImageX.png", sobelImageX);

  float sobelLowSurpress = 70;
  float sobelThreshold = 70;

  float X_center = -1;
  float Y_center = -1;
  int skip_step=1;

  int edgeType=-1;//1 for positive edge, -1 for negative edge, 0 for both

  int edge_hit_line_CD = 1;


  {

    vector<Point3f> edge_points;
    for(int i=0;i<sobelImageX.rows;i+=skip_step){//Y

      int noedge_count = 0;
      int noedge_count_limit = 10;
      int edge_width = 0;

      float X_wsum=0;
      float X_weight=0;
      float X_wsum_sq=0; // Sum of squared values for variance calculation

      for(int j=0;j<sobelImageX.cols;j+=skip_step){//X
        float val = sobelImageX.at<float>(i,j);

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
        float x = X_center;
        float y = Y_center;
        float w = X_weight/(1+sigma);
        edge_points.push_back(cv::Point3f(x,y,w));
        
      }
    }


    if (!edge_points.empty()) {
      cv::RotatedRect tmp_rect = InspTarUtil::circleFitting(edge_points);
      
      std::cout<<"tmp_rect:"<<tmp_rect.center.x<<","<<tmp_rect.center.y<<","<<tmp_rect.size.width<<","<<tmp_rect.size.height<<std::endl;
      
      float center_x = tmp_rect.center.x;
      float center_y = tmp_rect.center.y;
      float radius = tmp_rect.size.width/2;
      // cv::circle(srcImg_cropped, cv::Point(center_x, center_y), radius, cv::Scalar(0, 0, 0), 2);
      // InspTarUtil::drawSmoothCircle(srcImg_cropped, cv::Point(center_x, center_y), radius, 100, cv::Scalar(0, 0, 0), 2);
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;

  // cv::imwrite("srcImg_rotated_cropped.png", srcImg_cropped);

  

}

static void genWarpPolarXYMap(const cv::Point2f& center, 
                         float minRadius, float maxRadius, 
                         float startAngle_deg, float endAngle_deg,
                         cv::Mat& mapX, cv::Mat& mapY,float scale_R = 1,float scale_ANG = 1,int direction = 1 ) {
    float angleRange_rad = (endAngle_deg - startAngle_deg) * CV_PI / 180.0;
    float radiusRange=maxRadius-minRadius;
    // Determine the output size based on the radius and angle range
    int outputHeight = ((int)(angleRange_rad*maxRadius*scale_ANG)+1); // Width based on arc length(angle range)
    int outputWidth = (int)(radiusRange*scale_R);          // Height based on radius range
    // Prepare the mapping matrices
    mapX.create(outputHeight, outputWidth, CV_32FC1);
    mapY.create(outputHeight, outputWidth, CV_32FC1);
    // Fill the mapping matrices

    float startAngle_rad=startAngle_deg*CV_PI/180.0;
    for (int y = 0; y < outputHeight; ++y) {
        // Calculate the angle in radians for this pixel
        float angle = -(startAngle_rad + (angleRange_rad*((float)y) / (outputHeight-1)));//flip angle for image coordinate (-y)

        float cos_angle=cosf(angle);
        float sin_angle=sinf(angle);
        for (int x = 0; x < outputWidth; ++x) {
            float radius = radiusRange*((float)x/(outputWidth-1)); // Current radius
            if(direction==1){
              radius=minRadius+radius;
            }
            else if(direction==-1){//reverse direction
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



// static cv::Mat warpPolarSegment(const cv::Mat& src, const cv::Point2f& center, 
//                          double minRadius, double maxRadius, 
//                          double startAngle_deg, double endAngle_deg) {
//     cv::Mat mapX, mapY;
//     warpPolarWarpXYMap(center, minRadius, maxRadius, startAngle_deg, endAngle_deg, mapX, mapY);
//     // Remap the source image to polar coordinates
//     cv::Mat polarSegment;
//     cv::remap(src, polarSegment, mapX, mapY, cv::INTER_CUBIC, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

//     return polarSegment;
// }

static void sArcFitting(shared_ptr<StageInfo_ArcFitting>& report,cv::Mat& srcImg,cJSON* param)
{
  //add noise to srcImg

  StageInfo_ArcFitting s_result;
  s_result.category=STAGEINFO_CAT_UNSET;
  auto start_time = std::chrono::high_resolution_clock::now();

  float pt1_x=JFetch_NUMBER_ex(param,"pt1.x");
  float pt1_y=JFetch_NUMBER_ex(param,"pt1.y");
  Point2f circle_pt1(pt1_x,pt1_y);

  float pt2_x=JFetch_NUMBER_ex(param,"pt2.x");
  float pt2_y=JFetch_NUMBER_ex(param,"pt2.y");
  Point2f circle_pt2(pt2_x,pt2_y);

  float pt3_x=JFetch_NUMBER_ex(param,"pt3.x");
  float pt3_y=JFetch_NUMBER_ex(param,"pt3.y");
  Point2f circle_pt3(pt3_x,pt3_y);

  if(pt1_x!=pt1_x || pt1_y!=pt1_y || pt2_x!=pt2_x || pt2_y!=pt2_y || pt3_x!=pt3_x || pt3_y!=pt3_y)
  {
    LOGE("pt1 or pt2 or pt3 is not a number");
    return;
  }

  float outerMargin=JFetch_NUMBER_ex(param,"outerMargin",100);
  float innerMargin=JFetch_NUMBER_ex(param,"innerMargin",100);

  float noise_threshold=JFetch_NUMBER_ex(param,"noise_threshold",3);
  int edge_type=JFetch_NUMBER_ex(param,"edge_type",1);
  float blur_sigma=JFetch_NUMBER_ex(param,"blur_sigma",3);
  int blur_size=JFetch_NUMBER_ex(param,"blur_size",5);


  float noise_level=JFetch_NUMBER_ex(param,"noise_level",0);

  if(noise_level>0){
    cv::Mat noise = cv::Mat::zeros(srcImg.size(), CV_8UC1);
    cv::RNG rng(cv::getTickCount());
    rng.fill(noise, cv::RNG::NORMAL, 0, noise_level);
    cv::add(srcImg, noise, srcImg);
  }


  //outer pixel minus inner pixel as edge strength
  //1 for positive edge, -1 for negative edge, 0 for both




  cv::Point2f center;
  float radius;
  float startAngle, endAngle;
  InspTarUtil::findCircleFrom3PointsWithArc(circle_pt1,circle_pt2,circle_pt3,center,radius,startAngle,endAngle);
  std::cout<<"center:"<<center.x<<","<<center.y<<",R"<<radius<<",ang:"<<startAngle* 180.0f / CV_PI<<","<<endAngle* 180.0f / CV_PI<<std::endl;
  

  float open_angle=InspTarUtil::normalizeAngle_nPI_PI((InspTarUtil::angleSub(startAngle,endAngle))/2+endAngle);
  std::cout<<"open_angle:"<<open_angle* 180.0f / CV_PI<<std::endl;
  
  {

    //start time
    auto start_time = std::chrono::high_resolution_clock::now();

    float a1=InspTarUtil::angleSub(startAngle,open_angle);
    std::cout<<"a1:"<<a1* 180.0f / CV_PI<<std::endl;
    bool overHalf=a1>CV_PI/2;

    float outer_radius=radius+outerMargin;
    float inner_radius=radius-innerMargin;


    cv::Point2f boxSize(
      outer_radius+(overHalf?inner_radius:outer_radius)*cos(a1),
      (overHalf)?(outer_radius)*2*sin(a1):2*(outer_radius));

    float boxW_shrink=2*(outer_radius)-boxSize.x;

    float boxCenterRetract=boxW_shrink/2;


    std::cout<<"boxCenterRetract:"<<boxCenterRetract<<std::endl;

    cv::Point2f boxCenter(center.x-boxCenterRetract*cos(-open_angle),center.y-boxCenterRetract*sin(-open_angle));

    float close_angle=InspTarUtil::normalizeAngle_nPI_PI(open_angle+CV_PI);



    // boxSize.x=boxSize.x+130;//expand W
    // boxSize.y=boxSize.y+130;//expand H

    cv::Mat transformMatrix = InspTarUtil::opencv_rotCrop_matrix(boxCenter.x, boxCenter.y, open_angle,1, boxSize.x/2, boxSize.y/2);
    cv::Mat invTransformMatrix = InspTarUtil::invertAffineTransform(transformMatrix);
    Mat srcImg_cropped;
    // Apply the affine transformation (rotation)


    cv::warpAffine(srcImg, srcImg_cropped, transformMatrix, cv::Size(boxSize.x,boxSize.y), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>warpAffine Time taken: " << duration.count() << " milliseconds" << std::endl;
    }


    if(blur_sigma>0 && blur_size>0)
    {
      //blur
      cv::GaussianBlur(srcImg_cropped, srcImg_cropped, cv::Size(blur_size,blur_size), blur_sigma);
    }
    


    std::vector<cv::Point2f> pcenter = {center}, pcenter_onCrop;

    // Map the point using m1
    cv::transform(pcenter, pcenter_onCrop, transformMatrix);  // Use cv::transform to apply affine transformation

    //draw arc on pcenter_onCrop
    // cv::ellipse(srcImg_cropped, pcenter_onCrop[0], cv::Size(radius-10,radius-10), 0, 0, 360, cv::Scalar(0, 0, 255), 5);
    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>pre-sobel Time taken: " << duration.count() << " milliseconds" << std::endl;
    }
    //sobel on srcImg_cropped
    cv::Mat sobelImageX;
    cv::Mat sobelImageY;
    cv::Sobel(srcImg_cropped, sobelImageX, CV_16S, 1, 0); // 1 for x-derivative to detect vertical edges
    cv::Sobel(srcImg_cropped, sobelImageY, CV_16S, 0, 1); // 1 for x-derivative to detect vertical edges


    //merge sobelImageX and sobelImageY into 2 channels
    



    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>sobel Time taken: " << duration.count() << " milliseconds" << std::endl;
    }

    vector<Point3f> edge_points;

    for(int i=0;i<sobelImageX.rows;i++){
      for(int j=0;j<sobelImageX.cols;j++){
        int16_t sobel_x=sobelImageX.at<int16_t>(i,j);
        int16_t sobel_y=sobelImageY.at<int16_t>(i,j);
        // float mag= hypot(sobel_xy[0],sobel_xy[1]);
        cv::Vec2f normal_vec=cv::Vec2f(j-pcenter_onCrop[0].x,i-pcenter_onCrop[0].y);//point to center

        normal_vec[1]=-normal_vec[1];//flip y
        float magnitude=hypot(normal_vec[0],normal_vec[1]);

        if(magnitude>radius+outerMargin || magnitude<radius-innerMargin)
        {
          sobelImageX.at<int16_t>(i,j)=0;
          continue;
        }

        //normalize
        normal_vec=normal_vec/magnitude;
        //rotate sobel_xy  by normal_vec
        cv::Vec2f rotated_sobel_xy=cv::Vec2f(sobel_x*normal_vec[0]-sobel_y*normal_vec[1],sobel_x*normal_vec[1]+sobel_y*normal_vec[0]);

        float normal_mag=rotated_sobel_xy[0];
        if(edge_type==1)
        {
          if(normal_mag<0)normal_mag=0;
        }
        else if(edge_type==-1)
        {
          if(normal_mag>0)normal_mag=0;
          normal_mag=-normal_mag;
        }
        else
        {
          if(normal_mag<0)normal_mag=-normal_mag;
        }




        sobelImageX.at<int16_t>(i,j)=0;
        normal_mag-=noise_threshold;
        if(normal_mag<0)normal_mag=0;

        if(normal_mag==0)continue;
        float tengential_mag=rotated_sobel_xy[1];
        int slentedRatio=tengential_mag/normal_mag;
        if(slentedRatio<0)slentedRatio=-slentedRatio;
        if(slentedRatio>0.3)continue;//ignore too slented edge


        edge_points.push_back(cv::Point3f(j,i,normal_mag));
        sobelImageX.at<int16_t>(i,j)=(int16_t)(normal_mag);

      }
    }

    std::cout<<"edge_points.size():"<<edge_points.size()<<std::endl;

    if(1){
      int tryCount=20;
      int batch_size=200;
      if(edge_points.size()<batch_size*tryCount)
      {
        tryCount=1;
        batch_size=edge_points.size();
      }
      cv::RotatedRect circle= InspTarUtil::fitCircleByRansac(edge_points,tryCount,batch_size);
      std::cout<<"circle:"<<circle.center.x<<","<<circle.center.y<<",R"<<circle.size.width/2<<std::endl;
      cv::ellipse(srcImg_cropped, circle.center, cv::Size(circle.size.width/2,circle.size.height/2), 0, 0, 360, cv::Scalar(200, 0, 255), 5);


      std::vector<cv::Point2f> pcenter = {circle.center}, pcenter_onOrigin;

      // Map the point using m1
      cv::transform(pcenter, pcenter_onOrigin, invTransformMatrix);  // Use cv::transform to apply affine transformation


      report->center_x=pcenter_onOrigin[0].x+0.5;
      report->center_y=pcenter_onOrigin[0].y+0.5;
      report->radius=circle.size.width/2;
      report->category=STAGEINFO_CAT_OK;
      std::cout<<"circle:"<<pcenter_onOrigin[0].x+0.5<<","<<pcenter_onOrigin[0].y+0.5<<",R"<<circle.size.width/2<<std::endl;

    }

    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    }

    // std::cout<<"edge_points.size():"<<edge_points.size()<<std::endl;
    //save
    // cv::imwrite("sobelImageMag.png", sobelImageX);

    // std::cout<<"pcenter_onCrop:"<<pcenter_onCrop[0].x<<","<<pcenter_onCrop[0].y<<std::endl;

    // cv::imwrite("srcImg_cropped222.png", srcImg_cropped);

    // std::cout<<"boxSize:"<<boxSize.x<<","<<boxSize.y<<std::endl;

    // std::cout<<"center:"<<center.x<<","<<center.y<<std::endl;
    // std::cout<<"boxCenter:"<<boxCenter.x<<","<<boxCenter.y<<std::endl;
  }
  

  //calculate the 3 point on edge to find circle center and radius
  return;


  

}



// static Mat cameraMatrix = (Mat_<double>(3, 3) << 11775.33981935765, 0, 1300.919341080545,
//                                           0, 11764.9763003154, 988.9958350064916,
//                                           0, 0, 1);

// // // Define the distortion coefficients 
// static Mat distCoeffs = (Mat_<double>(5, 1) << -0.370482288257829, -2.57889743589971,
//                                           -0.0004007172176641259, 0.001292566228390161, 0);



// Point2f pixUndistortion(Point2f pix)
// {
//   vector<Point2f> originalPoints = { pix }, undistortedPoints;
//   undistortPoints(originalPoints, undistortedPoints, cameraMatrix, distCoeffs, noArray(), cameraMatrix);
//   return undistortedPoints[0];
// }



// Point2f distortPoint(const Point2f& undistortedPoint, const Mat& cameraMatrix, const Mat& distCoeffs, int iterations = 5) {
//     // Camera matrix parameters
//     double fx = cameraMatrix.at<double>(0, 0);
//     double fy = cameraMatrix.at<double>(1, 1);
//     double cx = cameraMatrix.at<double>(0, 2);
//     double cy = cameraMatrix.at<double>(1, 2);

//     // Distortion coefficients
//     double k1 = distCoeffs.at<double>(0, 0);
//     double k2 = distCoeffs.at<double>(1, 0);
//     double p1 = distCoeffs.at<double>(2, 0);
//     double p2 = distCoeffs.at<double>(3, 0);
//     double k3 = distCoeffs.at<double>(4, 0);

//     // Start with the undistorted point in normalized coordinates
//     double x = (undistortedPoint.x - cx) / fx;
//     double y = (undistortedPoint.y - cy) / fy;

//     // Iteratively apply distortion
//     for (int i = 0; i < iterations; ++i) {
//         double r2 = x * x + y * y;
//         double radialDistortion = 1 + k1 * r2 + k2 * r2 * r2 + k3 * r2 * r2 * r2;

//         double xDistorted = x * radialDistortion + 2 * p1 * x * y + p2 * (r2 + 2 * x * x);
//         double yDistorted = y * radialDistortion + p1 * (r2 + 2 * y * y) + 2 * p2 * x * y;

//         // Update x and y with the distorted values
//         x = xDistorted;
//         y = yDistorted;
//     }

//     // Convert back to pixel coordinates
//     Point2f distortedPoint;
//     distortedPoint.x = fx * x + cx;
//     distortedPoint.y = fy * y + cy;
//     return distortedPoint;
// }

// Point2f pixDistortion(Point2f pix)
// {
//   return distortPoint(pix,cameraMatrix,distCoeffs);
// }



// Function to compute the mean of points
void computeMeans(const std::vector<cv::Point3f>& points, double& meanX, double& meanY, double& sumW) {
    meanX = 0;
    meanY = 0;
    sumW = 0;

    for (const auto& point : points) {
        meanX += point.x * point.z;
        meanY += point.y * point.z;
        sumW += point.z;
    }
    meanX /= sumW;
    meanY /= sumW;
}

// RMS error function for the fitted circle
float computeSigma(const std::vector<cv::Point3f>& points, const cv::RotatedRect& circle) {
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
cv::RotatedRect CircleFit_Hyper(const std::vector<cv::Point3f>& points, float* rms) {
    int iter, IterMAX = 999;
    const double Four = 4.0f, Three = 3.0f, Two = 2.0f;

    double meanX, meanY, sumW;
    computeMeans(points, meanX, meanY, sumW);

    // Moments calculation
    double Mxx = 0, Myy = 0, Mxy = 0, Mxz = 0, Myz = 0, Mzz = 0;
    for (const auto& point : points) {
        double Xi = point.x - meanX;
        double Yi = point.y - meanY;
        double Zi = Xi * Xi + Yi * Yi;

        Mxy += Xi * Yi * point.z;
        Mxx += Xi * Xi * point.z;
        Myy += Yi * Yi * point.z;
        Mxz += Xi * Zi * point.z;
        Myz += Yi * Zi * point.z;
        Mzz += Zi * Zi * point.z;
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


static Point3f sArcFitting_direct(shared_ptr<StageInfo_ArcFitting>& report,cv::Mat& srcImg,cv::Mat& sendImg,cJSON* param,Point3f newCenterRadius=Point3f(NAN,NAN,NAN),bool undistort=false,float scale_R=1,float scale_ANG=1)
{
  //add noise to srcImg

  StageInfo_ArcFitting s_result;
  s_result.category=STAGEINFO_CAT_UNSET;
  auto start_time = std::chrono::high_resolution_clock::now();

  string id=JFetch_STRING_ex(param,"id","");
  float pt1_x=JFetch_NUMBER_ex(param,"pt1.x");
  float pt1_y=JFetch_NUMBER_ex(param,"pt1.y");
  Point2f circle_pt1(pt1_x,pt1_y);

  float pt2_x=JFetch_NUMBER_ex(param,"pt2.x");
  float pt2_y=JFetch_NUMBER_ex(param,"pt2.y");
  Point2f circle_pt2(pt2_x,pt2_y);

  float pt3_x=JFetch_NUMBER_ex(param,"pt3.x");
  float pt3_y=JFetch_NUMBER_ex(param,"pt3.y");
  Point2f circle_pt3(pt3_x,pt3_y);

  if(pt1_x!=pt1_x || pt1_y!=pt1_y || pt2_x!=pt2_x || pt2_y!=pt2_y || pt3_x!=pt3_x || pt3_y!=pt3_y)
  {
    LOGE("pt1 or pt2 or pt3 is not a number");
    return Point3f(NAN,NAN,NAN);
  }

  float outerMargin=JFetch_NUMBER_ex(param,"outerMargin",100);
  float innerMargin=JFetch_NUMBER_ex(param,"innerMargin",100);

  float noise_threshold=JFetch_NUMBER_ex(param,"noise_threshold",3);
  int edge_type=JFetch_NUMBER_ex(param,"edge_type",1);


  //outer pixel minus inner pixel as edge strength
  //1 for positive edge, -1 for negative edge, 0 for both




  cv::Point2f center;
  float radius;
  float startAngle, endAngle;
  InspTarUtil::findCircleFrom3PointsWithArc(circle_pt1,circle_pt2,circle_pt3,center,radius,startAngle,endAngle);
  if(newCenterRadius.x==newCenterRadius.x && newCenterRadius.y==newCenterRadius.y)
  {
    center.x=newCenterRadius.x;
    center.y=newCenterRadius.y;
  }
   
  if(newCenterRadius.z==newCenterRadius.z)
    radius=newCenterRadius.z;
  std::cout<<"center:"<<center.x<<","<<center.y<<",R"<<radius<<",ang:"<<startAngle* 180.0f / CV_PI<<","<<endAngle* 180.0f / CV_PI<<std::endl;
  

  float open_angle=InspTarUtil::normalizeAngle_nPI_PI((InspTarUtil::angleSub(startAngle,endAngle))/2+endAngle);
  std::cout<<"open_angle:"<<open_angle* 180.0f / CV_PI<<std::endl;
  
  {

    //start time
    auto start_time = std::chrono::high_resolution_clock::now();

    //draw arc on pcenter_onCrop
    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>pre-sobel Time taken: " << duration.count() << " milliseconds" << std::endl;
    }
    //sobel on srcImg_cropped
    cv::Mat sobelImageX;
    cv::Mat sobelImageY;
    cv::Sobel(srcImg, sobelImageX, CV_16S, 1, 0); // 1 for x-derivative to detect vertical edges
    cv::Sobel(srcImg, sobelImageY, CV_16S, 0, 1); // 1 for x-derivative to detect vertical edges


    //merge sobelImageX and sobelImageY into 2 channels
    



    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>sobel Time taken: " << duration.count() << " milliseconds" << std::endl;
    }

    vector<Point3f> edge_points;
    

    for(int i=0;i<sobelImageX.rows;i++){
      for(int j=0;j<sobelImageX.cols;j++){
        int16_t sobel_x=sobelImageX.at<int16_t>(i,j);
        int16_t sobel_y=sobelImageY.at<int16_t>(i,j);
        // float mag= hypot(sobel_xy[0],sobel_xy[1]);
        cv::Vec2f normal_vec=cv::Vec2f(j-center.x,i-center.y);//point to center

        normal_vec[1]=-normal_vec[1];//flip y
        float magnitude=hypot(normal_vec[0],normal_vec[1]);

        if(magnitude>radius+outerMargin || magnitude<radius-innerMargin)
        {
          sobelImageX.at<int16_t>(i,j)=0;
          continue;
        }

        float theta=atan2(normal_vec[1],normal_vec[0]);

        if(theta<startAngle || theta>endAngle)
        {
          sobelImageX.at<int16_t>(i,j)=0;
          continue;
        }

        if(!sendImg.empty())
          sendImg.at<cv::Vec3b>(i,j)=cv::Vec3b(100,0,100);
        //normalize
        normal_vec=normal_vec/magnitude;
        //rotate sobel_xy  by normal_vec
        cv::Vec2f rotated_sobel_xy=cv::Vec2f(sobel_x*normal_vec[0]-sobel_y*normal_vec[1],sobel_x*normal_vec[1]+sobel_y*normal_vec[0]);

        float normal_mag=rotated_sobel_xy[0];
        if(edge_type==1)
        {
          if(normal_mag<0)normal_mag=0;
        }
        else if(edge_type==-1)
        {
          if(normal_mag>0)normal_mag=0;
          normal_mag=-normal_mag;
        }
        else
        {
          if(normal_mag<0)normal_mag=-normal_mag;
        }




        sobelImageX.at<int16_t>(i,j)=0;
        normal_mag-=noise_threshold;
        if(normal_mag<0)normal_mag=0;

        if(normal_mag==0)continue;
        float tengential_mag=rotated_sobel_xy[1];
        int slentedRatio=tengential_mag/normal_mag;
        if(slentedRatio<0)slentedRatio=-slentedRatio;
        if(slentedRatio>0.3)continue;//ignore too slented edge

        // Point2f undistorted_pix=undistort?pixUndistortion(Point2f(j,i)):Point2f(j,i);
        Point2f undistorted_pix=Point2f(j,i);
        edge_points.push_back(cv::Point3f(undistorted_pix.x,undistorted_pix.y,normal_mag));
        // sobelImageX.at<int16_t>(i,j)=(int16_t)(normal_mag);
        if(!sendImg.empty())
        {
          normal_mag/=2;
          if(normal_mag<0)normal_mag=0;
          if(normal_mag>255)normal_mag=255;
          uint8_t normal_mag_u8=normal_mag;
          sendImg.at<cv::Vec3b>(i,j)=cv::Vec3b(normal_mag_u8,normal_mag_u8,normal_mag_u8);
        }

      }
    }

    std::cout<<"edge_points.size():"<<edge_points.size()<<std::endl;

    if(1){
      int tryCount=20;
      int batch_size=200;
      if(edge_points.size()<batch_size*tryCount)
      {
        tryCount=1;
        batch_size=edge_points.size();
      }
      // cv::RotatedRect circle= InspTarUtil::fitCircleByRansac(edge_points,tryCount,batch_size);

      cv::RotatedRect circle= InspTarUtil::circleFitting(edge_points);

      // if(undistort)circle.center=pixDistortion(circle.center);
      std::cout<<"circle:"<<circle.center.x<<","<<circle.center.y<<",R"<<circle.size.width/2<<std::endl;

      report->center_x=circle.center.x+0.5;
      report->center_y=circle.center.y+0.5;
      report->radius=circle.size.width/2;
      report->sigma=circle.angle;//hack to set sigma to angle 
      report->category=STAGEINFO_CAT_OK;
      std::cout<<"circle:"<<report->center_x<<","<<report->center_y<<",R"<<report->radius<<",s:"<<report->sigma<<std::endl;

    }

    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    }

    // std::cout<<"edge_points.size():"<<edge_points.size()<<std::endl;
    //save
    // if(templateOffset.x!=0 && templateOffset.y!=0)
    //   cv::imwrite(std::string("sobelImageX_")+id+".png", sobelImageX);

    // std::cout<<"pcenter_onCrop:"<<pcenter_onCrop[0].x<<","<<pcenter_onCrop[0].y<<std::endl;

    // cv::imwrite("srcImg_cropped222.png", srcImg_cropped);

    // std::cout<<"boxSize:"<<boxSize.x<<","<<boxSize.y<<std::endl;

    // std::cout<<"center:"<<center.x<<","<<center.y<<std::endl;
    // std::cout<<"boxCenter:"<<boxCenter.x<<","<<boxCenter.y<<std::endl;
  }
  

  //calculate the 3 point on edge to find circle center and radius
  return Point3f(report->center_x,report->center_y,report->radius);


  

}






static Point3f  sArcFitting_polar(shared_ptr<StageInfo_ArcFitting>& report,vector<Point3f> &extracted_edge_points,cv::Mat& srcImg,cv::Mat& sendImg,cJSON* param,Point3f newCenterRadius=Point3f(NAN,NAN,NAN),bool undistort=false,float scale_R=1,float scale_ANG=1)
{
  //add noise to srcImg

  StageInfo_ArcFitting s_result;
  s_result.category=STAGEINFO_CAT_UNSET;
  auto start_time = std::chrono::high_resolution_clock::now();

  float pt1_x=JFetch_NUMBER_ex(param,"pt1.x");
  float pt1_y=JFetch_NUMBER_ex(param,"pt1.y");
  Point2f circle_pt1(pt1_x,pt1_y);

  float pt2_x=JFetch_NUMBER_ex(param,"pt2.x");
  float pt2_y=JFetch_NUMBER_ex(param,"pt2.y");
  Point2f circle_pt2(pt2_x,pt2_y);

  float pt3_x=JFetch_NUMBER_ex(param,"pt3.x");
  float pt3_y=JFetch_NUMBER_ex(param,"pt3.y");
  Point2f circle_pt3(pt3_x,pt3_y);

  if(pt1_x!=pt1_x || pt1_y!=pt1_y || pt2_x!=pt2_x || pt2_y!=pt2_y || pt3_x!=pt3_x || pt3_y!=pt3_y)
  {
    LOGE("pt1 or pt2 or pt3 is not a number");
    return Point3f(NAN,NAN,NAN);
  }




  cv::Point2f center;
  float radius;
  float startAngle, endAngle;
  InspTarUtil::findCircleFrom3PointsWithArc(circle_pt1,circle_pt2,circle_pt3,center,radius,startAngle,endAngle);
  std::cout<<"center:"<<center.x<<","<<center.y<<",R"<<radius<<",ang:"<<startAngle* 180.0f / CV_PI<<","<<endAngle* 180.0f / CV_PI<<std::endl;



  float outerMargin=JFetch_NUMBER_ex(param,"outerMargin",100);
  float innerMargin=JFetch_NUMBER_ex(param,"innerMargin",100);

  float noise_threshold=JFetch_NUMBER_ex(param,"noise_threshold",3);
  int edge_type=JFetch_NUMBER_ex(param,"edge_type",1);
  float blur_sigma=JFetch_NUMBER_ex(param,"blur_sigma",3);
  int blur_size=JFetch_NUMBER_ex(param,"blur_size",5);
  if(blur_size<=0)blur_size=0;
  else
  {
    blur_size=blur_size/2*2+1;//make sure it is odd
  }


  int w_drop_border_ratio=JFetch_NUMBER_ex(param,"w_drop_border_ratio",0);

  noise_threshold/=scale_R;
  //outer pixel minus inner pixel as edge strength
  //1 for positive edge, -1 for negative edge, 0 for both

  
  if(newCenterRadius.x==newCenterRadius.x && newCenterRadius.y==newCenterRadius.y)
  {
    center.x=newCenterRadius.x;
    center.y=newCenterRadius.y;
  }
   
  if(newCenterRadius.z==newCenterRadius.z)
    radius=newCenterRadius.z;

  vector<Point3f> edge_points;
  {
    double minRadius = radius-innerMargin;        // Minimum radius
    if(minRadius<1)minRadius=1;
    double maxRadius = radius+outerMargin;        // Maximum radius
    double startAngle_deg = startAngle*180/CV_PI;       // Start angle in degrees
    double endAngle_deg = endAngle*180/CV_PI;         // End angle in degrees


    // LOGI("c:%f,%f,minR:%f,maxR:%f,start:%f,end:%f",center.x,center.y,minRadius,maxRadius,startAngle_deg,endAngle_deg);

    
    cv::Mat mapX, mapY;
    int map_r_direction=edge_type;
    genWarpPolarXYMap(center, minRadius, maxRadius, startAngle_deg, endAngle_deg, mapX, mapY,scale_R,scale_ANG,map_r_direction);
    // Remap the source image to polar coordinates
    cv::Mat polarSegment;
    cv::remap(srcImg  , polarSegment, mapX, mapY, cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));


    
    int searchShrink=0;
    if(blur_sigma>0 && blur_size>0)
    {
      //blur
      cv::GaussianBlur(polarSegment, polarSegment, cv::Size(1,blur_size), blur_sigma);
      searchShrink+=blur_size;
    }
    

    cv::Mat sobelX;

    
    cv::Sobel(polarSegment, sobelX, CV_16S, 1, 0); // 1 for x-derivative to detect vertical edges

    vector<Point2f> edgeLoc(sobelX.rows);
    vector<float> edgeValueCache(sobelX.cols);
    //find X max edge for every row
    for(int i=blur_size;i<sobelX.rows-blur_size;i++){
      int W=sobelX.cols;
      for(int j=0;j<W;j++){//filter
        int16_t val=sobelX.at<int16_t>(i,j);

        // if(edge_type>0)
        // {
        //   if(val<0)val=0;
        // }
        // else if(edge_type<0)
        // {
        //   if(val>0)val=0;
        //   val=-val;
        // }
        // else
        // {
        //   if(val<0)val=-val;
        // }


        val-=noise_threshold;

        if(val<0)val=0;


        sobelX.at<int16_t>(i,j)=(int16_t)val;//filter

        edgeValueCache[j]=val;

      }


      //float find max blob location in edgeValueCache
      float max_blob_start=-1;
      float max_blob_len=0;
      float max_xEST=0;
      float max_xEST_w=0;

      float cur_blob_start=-1;
      float cur_blob_len=0;
      float cur_xEST=0;
      float cur_xEST_w=0;

      cur_blob_start=max_blob_start=0;

      // for(int j=0;j<W;j++){
      //   if(edgeValueCache[j]>0){
      //     if(cur_blob_start==-1)cur_blob_start=j;
      //     cur_blob_len++;
      //     cur_xEST+=j*edgeValueCache[j];
      //     cur_xEST_w+=edgeValueCache[j];
      //   }else{
      //     if(cur_xEST_w>max_xEST_w){
      //       max_blob_len=cur_blob_len;
      //       max_blob_start=cur_blob_start;
      //       max_xEST=cur_xEST/cur_xEST_w;
      //       max_xEST_w=cur_xEST_w;
      //     }

      //     cur_blob_len=0;
      //     cur_blob_start=-1;
      //   }
      // }


      // for(int j=0;j<W;j++){
      //   cur_blob_len++;
      //   cur_xEST+=j*edgeValueCache[j];
      //   cur_xEST_w+=edgeValueCache[j];
      // }



      if(1)
      {//first blob and mean

        for(int j=0;j<W;j++){
          if(edgeValueCache[j]>0){
            if(cur_blob_start==-1)cur_blob_start=j;
            cur_blob_len++;
            cur_xEST+=j*edgeValueCache[j];
            cur_xEST_w+=edgeValueCache[j];
          }else{
            if(cur_blob_len!=0)break;
            if(cur_xEST_w>max_xEST_w){
              max_blob_len=cur_blob_len;
              max_blob_start=cur_blob_start;
              max_xEST=cur_xEST/cur_xEST_w;
              max_xEST_w=cur_xEST_w;
            }

            cur_blob_len=0;
            cur_blob_start=-1;
          }
        }



        if(cur_xEST_w>max_xEST_w){
          max_blob_len=cur_blob_len;
          max_blob_start=cur_blob_start;
          max_xEST=cur_xEST/cur_xEST_w;
          max_xEST_w=cur_xEST_w;
        }



    
        if(max_blob_start!=-1){
          edgeLoc[i].x=max_xEST;
          edgeLoc[i].y=max_xEST_w;
          //set pixel to 0
          polarSegment.at<uint8_t>(i,(int)max_xEST)=0;
        }
        else
        {
          edgeLoc[i].x=-1;
          edgeLoc[i].y=0;//weight
        }


        // if(i%100==0){
        //   std::cout<<"row:"<<i<<",max_blob_start:"<<max_blob_start<<",max_blob_len:"<<max_blob_len<<",edgeLoc:"<<edgeLoc[i]<<std::endl;
        // }
      
      }
      else
      {//find first peak(local max) location

        float max_value=0;
        float max_idx=-1;

        int avgRange=2;
        float drop_thres=5;
        for(int j=avgRange;j<W-avgRange;j++){
          if(edgeValueCache[j]<max_value-drop_thres){
            break;
          }
          else
          {
            if(edgeValueCache[j]>max_value){
              max_value=edgeValueCache[j];
              max_idx=j;
            }
          }
        }


        if(max_idx==-1){//ignore
          edgeLoc[i].x=-1;
          edgeLoc[i].y=0;
          continue;
        }

        float idxSum=0;
        float wSum=0;

        for(int i=max_idx-avgRange;i<=max_idx+avgRange;i++){

          wSum+=edgeValueCache[i];
          idxSum+=i*edgeValueCache[i];

          
        }


        edgeLoc[i].x=idxSum/wSum;
        edgeLoc[i].y=wSum;
      }






    }




    float LP_diff_thres=1;
    int boxFilterSize=5;
    if(boxFilterSize%2==1)//make sure it is oddx`
    {//lowpass filter for edge_points

      int boxFilterSingleSideSize=boxFilterSize/2;
      {//surpreess head and tail
        for(int i=0;i<boxFilterSingleSideSize;i++){
          edgeLoc[i].y=0;
          edgeLoc[edgeLoc.size()-1-i].y=0;
        }

      }
      // vector<int> edge_points_Loc_LP;

      // int target_size=edgeLoc.size()-boxFilterSingleSideSize*2;
      // edge_points_Loc_LP.reserve(target_size);

      // {//pass 1: to find anomaly points

      //   edge_points_Loc_LP.clear();
      //   int sum_x=0;
      //   for(int i=0;i<boxFilterSize;i++){
      //     sum_x+=(int)(edgeLoc[i].x*1000);
      //   }
      //   edge_points_Loc_LP.push_back(sum_x);
      //   for(int i=1;i<target_size;i++){
      //     int ei=i+boxFilterSingleSideSize;
      //     sum_x-=(int)(edgeLoc[ei-boxFilterSingleSideSize-1].x*1000);
      //     sum_x+=(int)(edgeLoc[ei+boxFilterSingleSideSize].x*1000);
      //     edge_points_Loc_LP.push_back(sum_x);
      //   }

      //   {//compare edge_points_Loc_LP and edgeLoc and surpress edgeLoc if the difference is too large
      //     for(int i=0;i<edge_points_Loc_LP.size();i++){

      //       int ei=i+boxFilterSingleSideSize;
      //       float LPx=(float)edge_points_Loc_LP[i]/1000/boxFilterSize;
      //       float diff=abs(LPx-edgeLoc[ei].x);
      //       if(diff>LP_diff_thres){
      //         LOGE("surpress edgeLoc[%d]:%f,%f,diff:%f",ei,edgeLoc[ei].x,edgeLoc[ei].y,diff);
      //         edgeLoc[ei].y*=-1;//preserve the value, but mark it is invalid
      //         edgeLoc[ei].y=0;
      //       }
      //     }
      //   }



      // }



      {//pass 2: to find valid points

        float IIR_alpha=0.1;
        float IIR_value=NAN;
        for(int i=0;i<edgeLoc.size();i++){//foward pass
          if(edgeLoc[i].y<=0)continue;
          float nIIR_value=IIR_value*(1-IIR_alpha)+edgeLoc[i].x*IIR_alpha;
          if(IIR_value!=IIR_value){//NAN
            IIR_value=edgeLoc[i].x;
            continue;
          }
          float diff=abs(IIR_value-edgeLoc[i].x);
          if(diff>LP_diff_thres){
            LOGE("surpress1 >>edgeLoc[%d]:%f,%f,diff:%f",i,edgeLoc[i].x,edgeLoc[i].y,diff);
            edgeLoc[i].y=0;
          }
          else
          {
            IIR_value=nIIR_value;
          }
        }


        IIR_value=NAN;
        for(int i=edgeLoc.size()-1;i>=0;i--){//backward pass
          if(edgeLoc[i].y<=0)continue;
          float nIIR_value=IIR_value*(1-IIR_alpha)+edgeLoc[i].x*IIR_alpha;
          if(IIR_value!=IIR_value){//NAN
            IIR_value=edgeLoc[i].x;
            continue;
          }
          float diff=abs(IIR_value-edgeLoc[i].x);
          if(diff>LP_diff_thres){
            LOGE("surpress2 <<edgeLoc[%d]:%f,%f,diff:%f",i,edgeLoc[i].x,edgeLoc[i].y,diff);
            edgeLoc[i].y=0;
          }
          else
          {
            IIR_value=nIIR_value;
          }
        }


      }



    }

    {//convert edgeLoc(polar coordinate) to edge_points(XY image coordinate)
      edge_points.reserve(edgeLoc.size());
      edge_points.clear();
      int edge_len=edgeLoc.size();
      for(int i=0;i<edge_len;i++){
        float w=edgeLoc[i].y;
        if(w==0)
        {
          // LOGE("surpress edgeLoc[%d]:%f,%f",i,edgeLoc[i].x,edgeLoc[i].y);
          // continue;
        }
        float x=edgeLoc[i].x;
        int y=i;
        //find mapX,mapY
        int floor_x=floor(x);
        int ceil_x=ceil(x);
        float ratio_x=(x-floor_x);
        Point3f edge_point;
        edge_point.x=
          mapX.at<float>(y,floor_x)*(1-ratio_x)+
          mapX.at<float>(y,ceil_x)*ratio_x;

        edge_point.y=
          mapY.at<float>(y,floor_x)*(1-ratio_x)+
          mapY.at<float>(y,ceil_x)*ratio_x;


        float progress_rate=(float)i/(edge_len-1);


        // LOGI("%d,%f,%f,%f",i,edge_point.x,edge_point.y,edgeLoc[i].y);
        float ratio=1;
        if(progress_rate<w_drop_border_ratio)
          ratio=progress_rate/w_drop_border_ratio;
        else if(progress_rate>1-w_drop_border_ratio)
          ratio=(1-progress_rate)/(1-w_drop_border_ratio);
        edge_point.z=edgeLoc[i].y*ratio;//weight
        // edge_point.z=1;

        // if(undistort)
        // {
        //   Point2f undistorted_pix=pixUndistortion(Point2f(edge_point.x,edge_point.y));
        //   edge_point.x=undistorted_pix.x;
        //   edge_point.y=undistorted_pix.y;
        // }
        edge_points.push_back(edge_point);


        
      }
    }

    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << ">>warpPolarSegment Time taken: " << duration.count() << " milliseconds" << std::endl;
    }


    // sobelX
    if(0){
      // scale sobelX to 0-255
      //find min and max
      // double minVal, maxVal;
      // cv::minMaxIdx(sobelX, &minVal, &maxVal);
      // sobelX=(sobelX-minVal)/(maxVal-minVal)*255;

      uint64_t current_ms=cv::getTickCount();
      string filename_prefix=std::to_string(current_ms);
      cv::imwrite(filename_prefix+"_sobelX.png", sobelX);
      cv::imwrite(filename_prefix+"_polarSegment.png", polarSegment);

    }
    
  }

  {
    if(1){
      // int tryCount=20;
      // int batch_size=200;
      // if(edge_points.size()<batch_size*tryCount)
      // {
      //   tryCount=1;
      //   batch_size=edge_points.size();
      // }
      // cv::RotatedRect circle= InspTarUtil::fitCircleByRansac(edge_points,tryCount,batch_size);

      // cv::RotatedRect circle= InspTarUtil::circleFitting(edge_points);

      // extracted_edge_points=edge_points;
      // report->center_x=circle.center.x;
      // report->center_y=circle.center.y;
      // report->radius=circle.size.width/2;
      // report->category=STAGEINFO_CAT_OK;
      cv::RotatedRect res= CircleFit_Hyper(edge_points,&report->sigma);
      extracted_edge_points=edge_points;
      report->center_x=res.center.x;
      report->center_y=res.center.y;
      report->radius=res.size.width/2;
      report->category=STAGEINFO_CAT_OK;
      std::cout<<"circle:"<<report->center_x<<","<<report->center_y<<",R"<<report->radius<<std::endl;

    }

    {

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      std::cout << "Time taken: " << duration.count() << " milliseconds" << std::endl;
    }

    // std::cout<<"edge_points.size():"<<edge_points.size()<<std::endl;
    //save
    // cv::imwrite("sobelImageMag.png", sobelImageX);

    // std::cout<<"pcenter_onCrop:"<<pcenter_onCrop[0].x<<","<<pcenter_onCrop[0].y<<std::endl;

    // cv::imwrite("srcImg_cropped222.png", srcImg_cropped);

    // std::cout<<"boxSize:"<<boxSize.x<<","<<boxSize.y<<std::endl;

    // std::cout<<"center:"<<center.x<<","<<center.y<<std::endl;
    // std::cout<<"boxCenter:"<<boxCenter.x<<","<<boxCenter.y<<std::endl;
  }
  

  //calculate the 3 point on edge to find circle center and radius
  return Point3f(report->center_x,report->center_y,report->radius);


  

}





void sssimgProcess_TEST(cv::Mat& srcImg,cJSON* param)
{

  cJSON *mocking_param=cJSON_CreateObject();
  {
    //add pt1 object
    cJSON *pt1=cJSON_CreateObject();
    cJSON_AddNumberToObject(pt1,"x",44);
    cJSON_AddNumberToObject(pt1,"y",1924);
    cJSON_AddItemToObject(mocking_param,"pt1",pt1);

    //add pt2 object
    cJSON *pt2=cJSON_CreateObject();
    cJSON_AddNumberToObject(pt2,"x",850);
    cJSON_AddNumberToObject(pt2,"y",2280);
    cJSON_AddItemToObject(mocking_param,"pt2",pt2);
    //add pt3 object
    cJSON *pt3=cJSON_CreateObject();
    cJSON_AddNumberToObject(pt3,"x",1074);
    cJSON_AddNumberToObject(pt3,"y",2938);
    cJSON_AddItemToObject(mocking_param,"pt3",pt3);
  }
  //add outerMargin
  cJSON_AddNumberToObject(mocking_param,"outerMargin",100);
  //add innerMargin
  cJSON_AddNumberToObject(mocking_param,"innerMargin",100);
  //add noise_threshold
  cJSON_AddNumberToObject(mocking_param,"noise_threshold",3);
  //add edge_type
  cJSON_AddNumberToObject(mocking_param,"edge_type",-1);

  /*
  sample json:
  {
    "pt1":{
      "x":44,
      "y":1924
     },
    "pt2":{
      "x":850,
      "y":2280
    },
    "pt3":{
      "x":1074,
      "y":2938
    },
    "outerMargin":100,
    "innerMargin":100,
    "noise_threshold":3,
    "edge_type":-1
  }
  */


  shared_ptr<StageInfo_ArcFitting> reportInfo(new StageInfo_ArcFitting());
  Point2f templateOffset(0,0);
  // templateOffset=sArcFitting_direct(reportInfo,srcImg,mocking_param,templateOffset);
  // LOGI("templateOffse1t:%f,%f",templateOffset.x,templateOffset.y);
  // templateOffset=sArcFitting_direct(reportInfo,srcImg,mocking_param,templateOffset);
  // LOGI("templateOffset2:%f,%f",templateOffset.x,templateOffset.y);

  cJSON_Delete(mocking_param);
}



void InspectionTarget_ArcFitting::singleProcess(shared_ptr<StageInfo> sinfo)
{
  LOGI("InspectionTarget_ArcFitting::singleProcess");


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


  shared_ptr<StageInfo_ArcFitting> reportInfo(new StageInfo_ArcFitting());

  reportInfo->category=STAGEINFO_CAT_NA;


  Mat sendImg(CV_srcImg.rows,CV_srcImg.cols,CV_8UC3);



  float blur_sigma=JFetch_NUMBER_ex(def,"blur_sigma",3);
  int blur_size=JFetch_NUMBER_ex(def,"blur_size",5);

  if(blur_size>0)
    blur_size=(blur_size/2)*2+1;//make it odd
  float noise_level=JFetch_NUMBER_ex(def,"noise_level",0);


  if(noise_level>0){
    cv::Mat noise = cv::Mat::zeros(CV_srcImg_gray.size(), CV_8UC1);
    cv::RNG rng(cv::getTickCount());
    rng.fill(noise, cv::RNG::NORMAL, 0, noise_level);
    cv::add(CV_srcImg_gray, noise, CV_srcImg_gray);
  }
  LOGE(">>>>>>sArcFitting");


  vector<Point3f> extracted_edge_points;

  float scale_R=JFetch_NUMBER_ex(def,"scale_R",1);
  float scale_ANG=JFetch_NUMBER_ex(def,"scale_ANG",1);
  try{



    bool do_undistort=!JFetch_FALSE(def,"do_undistort");//default is true

    Point3f newCenterRadius(NAN,NAN,NAN);

    cv::Mat EmptyImg;
    int op_mode=(int)JFetch_NUMBER_ex(def,"op_mode",0);
    if(op_mode==0)//using polar coordinate to find edge and fit circle
    {
      newCenterRadius=sArcFitting_polar(reportInfo,extracted_edge_points,CV_srcImg_gray,EmptyImg,def,newCenterRadius,do_undistort,1,1);
      // LOGI("templateOffse1t:%f,%f",templateOffset.x,templateOffset.y);
      memset(sendImg.data,0,3*sendImg.rows*sendImg.cols);
      newCenterRadius=sArcFitting_polar(reportInfo,extracted_edge_points,CV_srcImg_gray,sendImg,def,newCenterRadius,do_undistort,scale_R,scale_ANG);
    }
    else 
    {
      if(blur_size>0){
        GaussianBlur(CV_srcImg_gray, CV_srcImg_gray, Size(blur_size, blur_size), blur_sigma);
      }

      newCenterRadius=sArcFitting_direct(reportInfo,CV_srcImg_gray,EmptyImg,def,newCenterRadius,do_undistort,1,1);
      // LOGI("templateOffse1t:%f,%f",templateOffset.x,templateOffset.y);
      memset(sendImg.data,0,3*sendImg.rows*sendImg.cols);
      newCenterRadius=sArcFitting_direct(reportInfo,CV_srcImg_gray,sendImg,def,newCenterRadius,do_undistort,scale_R,scale_ANG);

    }


    LOGI("fitted circle:%f,%f, R:%f, s:%f op_mode:%d",reportInfo->center_x,reportInfo->center_y,reportInfo->radius,reportInfo->sigma,op_mode);

  }catch(std::invalid_argument e)
  {
    // LOGE("sLineFitting error:%s",e.what());
    // return;
    reportInfo->error_code=1;
    reportInfo->error_msg=e.what();
  }
  
  LOGE(">>>>>>sArcFitting");


  // {
  //   Mat undistortedImage;
  //   undistort(CV_srcImg_gray, undistortedImage, cameraMatrix, distCoeffs);

  //   cvtColor(undistortedImage, sendImg, COLOR_GRAY2BGR);
  // }
  reportInfo->img_show =d_sinfo->img;
  

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

  if(reportInfo->jInfo)
  {//extracted_edge_points
    cJSON* jpoints=cJSON_CreateArray();
    StageInfo::Point3fArray2cJSONArray(jpoints,extracted_edge_points);
    cJSON_AddItemToObject(reportInfo->jInfo,"extracted_edge_points",jpoints);
  
  }

  // cache_latest_result = reportInfo;
  belongMan->dispatch(reportInfo);

}



InspectionTarget_ArcFitting::~InspectionTarget_ArcFitting()
{
 
}
