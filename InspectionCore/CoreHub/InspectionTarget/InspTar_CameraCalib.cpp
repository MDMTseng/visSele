#include "InspTar_CameraCalib.hpp"
#include "StageInfo_Orientation.hpp"
#include "StageInfo.hpp"
#include <iostream>
using namespace cv;

using namespace std;

template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}


void InspectionTarget_CameraCalib::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{
  InspectionTarget::INIT(id,def,belongMan,local_env_path);
}

void InspectionTarget_CameraCalib::run()
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


void c_initUndistortRectifyMap(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                              const cv::Mat& R, const cv::Mat& newCameraMatrix,
                              cv::Size imageSize, int m1type, cv::Mat& map1, cv::Mat& map2, 
                              const cv::Mat& inv_perspective=cv::Mat()) {
    // Pre-allocate output maps
    map1.create(imageSize, m1type);
    map2.create(imageSize, m1type);

    // Pre-compute camera parameters
    const double fx = cameraMatrix.at<double>(0, 0);
    const double fy = cameraMatrix.at<double>(1, 1);
    const double cx = cameraMatrix.at<double>(0, 2);
    const double cy = cameraMatrix.at<double>(1, 2);

    const double newFx = newCameraMatrix.at<double>(0, 0);
    const double newFy = newCameraMatrix.at<double>(1, 1);
    const double newCx = newCameraMatrix.at<double>(0, 2);
    const double newCy = newCameraMatrix.at<double>(1, 2);

    // Pre-compute distortion coefficients
    const double k1 = distCoeffs.at<double>(0, 0);
    const double k2 = distCoeffs.at<double>(1, 0);
    const double p1 = distCoeffs.at<double>(2, 0);
    const double p2 = distCoeffs.at<double>(3, 0);
    const double k3 = distCoeffs.at<double>(4, 0);

    const int width = imageSize.width;

    // Use OpenMP for parallel processing
    #pragma omp parallel for schedule(static)
    for (int y = 0; y < imageSize.height; ++y) {
        // Thread-local vectors
        std::vector<cv::Point2f> pre_perspPoints(width);
        
        // Get direct pointer access to output maps for current row
        float* map1_row = map1.ptr<float>(y);
        float* map2_row = map2.ptr<float>(y);

        // Pre-fill perspPoints for current row
        for (int x = 0; x < width; ++x) {
            pre_perspPoints[x] = cv::Point2f(x, y);
        }

        // Handle perspective transform for entire row
        if (!inv_perspective.empty()) {
            cv::perspectiveTransform(pre_perspPoints, pre_perspPoints, inv_perspective);
        } else {
            // pre_perspPoints = perspPoints;
        }

        // Process each point in the row
        for (int x = 0; x < width; ++x) {
            // Normalize the point
            const double xNorm = (pre_perspPoints[x].x - newCx) / newFx;
            const double yNorm = (pre_perspPoints[x].y - newCy) / newFy;

            // Handle rectification
            double xDistorted, yDistorted;
            if (R.empty()) {
                xDistorted = xNorm;
                yDistorted = yNorm;
            } else {
                cv::Matx31d point(xNorm, yNorm, 1.0);
                cv::Mat rectifiedMat = R * cv::Mat(point);
                xDistorted = rectifiedMat.at<double>(0) / rectifiedMat.at<double>(2);
                yDistorted = rectifiedMat.at<double>(1) / rectifiedMat.at<double>(2);
            }

            // Apply distortion correction
            const double r2 = xDistorted * xDistorted + yDistorted * yDistorted;
            const double r4 = r2 * r2;
            const double r6 = r4 * r2;
            const double radialDistortion = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;

            const double xy_d = xDistorted * yDistorted;
            xDistorted = xDistorted * radialDistortion + 2.0 * p1 * xy_d + p2 * (r2 + 2.0 * xDistorted * xDistorted);
            yDistorted = yDistorted * radialDistortion + p1 * (r2 + 2.0 * yDistorted * yDistorted) + 2.0 * p2 * xy_d;

            // Store results directly using row pointers
            map1_row[x] = static_cast<float>(fx * xDistorted + cx);
            map2_row[x] = static_cast<float>(fy * yDistorted + cy);
        }
    }
}



void initDistortBackMap(const cv::Mat& cameraMatrix, const cv::Mat& distCoeffs,
                      cv::Size imageSize, cv::Mat& mapX, cv::Mat& mapY, 
                      int maxIterations = 5, double epsilon = 1e-5) {
    // Initialize the maps
    mapX.create(imageSize, CV_32FC1);
    mapY.create(imageSize, CV_32FC1);

    // Pre-compute camera matrix parameters
    const double fx = cameraMatrix.at<double>(0, 0);
    const double fy = cameraMatrix.at<double>(1, 1);
    const double cx = cameraMatrix.at<double>(0, 2);
    const double cy = cameraMatrix.at<double>(1, 2);
    const double inv_fx = 1.0 / fx;
    const double inv_fy = 1.0 / fy;

    // Pre-compute distortion coefficients
    const double k1 = distCoeffs.at<double>(0, 0);
    const double k2 = distCoeffs.at<double>(0, 1);
    const double p1 = distCoeffs.at<double>(0, 2);
    const double p2 = distCoeffs.at<double>(0, 3);
    const double k3 = distCoeffs.at<double>(0, 4);

    // Pre-compute epsilon squared for faster convergence check
    const double epsilon_squared = epsilon * epsilon;

    // Use OpenMP for parallel processing
    #pragma omp parallel for schedule(static) collapse(2)
    for (int v = 0; v < imageSize.height; v++) {
        for (int u = 0; u < imageSize.width; u++) {
            // Get direct pointer access to output maps
            float* mapX_ptr = mapX.ptr<float>(v);
            float* mapY_ptr = mapY.ptr<float>(v);
            
            // Convert undistorted pixel coordinates to normalized coordinates
            const double x_u = (u - cx) * inv_fx;
            const double y_u = (v - cy) * inv_fy;

            // Initialize distorted coordinates
            double x_d = x_u;
            double y_d = y_u;

            // Newton-Raphson iteration
            for (int iter = 0; iter < maxIterations; iter++) {
                const double x_d2 = x_d * x_d;
                const double y_d2 = y_d * y_d;
                const double r2 = x_d2 + y_d2;
                const double r4 = r2 * r2;
                const double r6 = r4 * r2;
                
                // Compute radial distortion
                const double radial_distortion = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
                
                // Compute tangential distortion
                const double xy_d = x_d * y_d;
                const double x_t = 2.0 * p1 * xy_d + p2 * (r2 + 2.0 * x_d2);
                const double y_t = p1 * (r2 + 2.0 * y_d2) + 2.0 * p2 * xy_d;

                // Calculate approximated coordinates
                const double x_approx = x_d * radial_distortion + x_t;
                const double y_approx = y_d * radial_distortion + y_t;

                // Calculate error
                const double dx = x_u - x_approx;
                const double dy = y_u - y_approx;

                // Update distorted coordinates
                x_d += dx;
                y_d += dy;

                // Check convergence using squared distance
                if ((dx * dx + dy * dy) < epsilon_squared) {
                    break;
                }
            }

            // Store results directly using pointers
            mapX_ptr[u] = static_cast<float>(fx * x_d + cx);
            mapY_ptr[u] = static_cast<float>(fy * y_d + cy);
        }
    }
}


void InspectionTarget_CameraCalib::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);


  cameraMatrix=Mat();//clear
  distCoeffs=Mat();//clear

  bool isOK=true;
  isOK=isOK&&StageInfo::cJSON2Mat(JFetch_OBJECT(def,"cameraMatrix"),cameraMatrix);
  isOK=isOK&&StageInfo::cJSON2Mat(JFetch_OBJECT(def,"distCoeffs"),distCoeffs);

  int img_width=JFetch_NUMBER_ex(def,"image_width",640);
  int img_height=JFetch_NUMBER_ex(def,"image_height",480);


  mmpp=JFetch_NUMBER_ex(def,"mmpp",1.0);

  if(!isOK)
  {
    LOGE("Update def failed: cameraMatrix or distCoeffs is not a valid matrix");
    cameraMatrix=Mat();
    distCoeffs=Mat();

    return;
  }
  StageInfo::cJSON2Mat(JFetch_OBJECT(def,"perspectiveMatrix"),perspectiveMatrix);

  String str_cameraMatrix="cameraMatrix:\n";
  str_cameraMatrix<<cameraMatrix;
  str_cameraMatrix+="\n distCoeffs:\n";
  str_cameraMatrix<<distCoeffs;

  LOGI("New def:%s",str_cameraMatrix.c_str());


  cv::Mat inv_perspective=cv::Mat();
  if(!perspectiveMatrix.empty())
    cv::invert(perspectiveMatrix, inv_perspective);


  //timer start
  auto t0=cv::getTickCount();
  c_initUndistortRectifyMap(cameraMatrix, distCoeffs, cv::Mat(),
                            cameraMatrix, Size(img_width,img_height), CV_32FC1, undistortMapX, undistortMapY,inv_perspective);
  //timer end
  auto t1=cv::getTickCount();
  LOGE("c_initUndistortRectifyMap time:%f ms",(t1-t0)*1000/cv::getTickFrequency());

  //timer start
  t0=cv::getTickCount();
  initDistortBackMap(cameraMatrix,distCoeffs,Size(img_width,img_height),distortMapX,distortMapY);
  //timer end
  t1=cv::getTickCount();
  LOGE("initDistortBackMap time:%f ms",(t1-t0)*1000/cv::getTickFrequency());


}


// namespace fs = std::filesystem;
// static bool CalibProcessFolder(std::vector<Mat> imgs, Size boardSize, float squareSize,Mat& ret_cameraMatrix,Mat& ret_distCoeffs,float &ret_rms) {

//     // Parameters for calibration
//     vector<vector<Point2f>> imagePoints;  // Points in 2D image space
//     vector<vector<Point3f>> objectPoints; // Points in 3D object space

//     // Generate the 3D object points for the chessboard pattern
//     vector<Point3f> objP;
//     for (int i = 0; i < boardSize.height; i++) {
//         for (int j = 0; j < boardSize.width; j++) {
//             objP.push_back(Point3f(j * squareSize, i * squareSize, 0));
//         }
//     }

//     namespace fs = std::__fs::filesystem; 
//     // Read images from the folder
//     for (const auto& img : imgs) {
//         // Find chessboard corners
//         vector<Point2f> corners;
//         bool found = findChessboardCorners(img, boardSize, corners,
//                                            CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_NORMALIZE_IMAGE);

//         LOGI("found:%d corners:%d",found,corners.size());
//         if(corners.size()>0)
//         cornerSubPix(img, corners, Size(11, 11), Size(-1, -1),
//                          TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 30, 0.1));

//         // Draw and show the corners
//         // drawChessboardCorners(image, boardSize, Mat(corners), found);
//         //   fs::path outPath = entry.path();
//         // outPath=outPath.replace_extension("_cross.png");
//         // cv::imwrite(outPath,image);


//         if (found) {
//             // Refine corner positions for higher accuracy
            
//             // Store detected points
//             imagePoints.push_back(corners);
//             objectPoints.push_back(objP);

//             // imshow("Corners", image);
//             // waitKey(100);  // Display each detected pattern briefly
//         } else {
//             LOGI("Chessboard corners not found in image");
//         }
//     }

//     if (imagePoints.size() < 1) {
//         LOGI("Not enough valid calibration images found. Need at least 10.");
//         return false;
//     }

//     // Camera matrix and distortion coefficients
//     ret_cameraMatrix = Mat::eye(3, 3, CV_64F);
//     ret_distCoeffs = Mat::zeros(8, 1, CV_64F);
//     vector<Mat> rvecs, tvecs;

//     // Calibrate the camera
//     double rms = calibrateCamera(objectPoints, imagePoints, boardSize, ret_cameraMatrix, ret_distCoeffs, rvecs, tvecs,
//                                  CALIB_FIX_K3 | CALIB_FIX_K4 | CALIB_FIX_K5);

//     LOGI("Calibration RMS error: %f",rms);
//     ret_rms=(float)rms;

//     String str_cameraMatrix_str=" camMat:";
//     str_cameraMatrix_str<<ret_cameraMatrix;
//     str_cameraMatrix_str+="\n distCoeffs:";
//     str_cameraMatrix_str<<ret_distCoeffs;
//     LOGI("%s",str_cameraMatrix_str.c_str());
//     // LOGI("Camera Matrix: \n%s",cv::format(ret_cameraMatrix, cv::Formatter::FMT_DEFAULT).str().c_str());
//     // LOGI("Distortion Coefficients: \n%s",cv::format(ret_distCoeffs).str().c_str());

//     // Save the results
//     // FileStorage fs("calibration_result.xml", FileStorage::WRITE);
//     // fs << "CameraMatrix" << cameraMatrix;
//     // fs << "DistCoeffs" << distCoeffs;
//     // fs.release();
//     // destroyAllWindows();

//     return true;
// }

vector<int> findBlobSizes(const vector<int>& circular_binary) {
    int n = circular_binary.size();
    vector<int> blob_sizes;
    int current_blob_size = 1;

    // Iterate over the vector to find each blob's size
    for (int i = 1; i < n; ++i) {
        if (circular_binary[i] == circular_binary[i - 1]) {
            current_blob_size++;
        } else {
            blob_sizes.push_back(current_blob_size);
            current_blob_size = 1;  // Reset for the next blob
        }
    }

    // Handle the circular connection between end and start
    if (circular_binary[0] == circular_binary[n - 1]) {
      if(blob_sizes.size()>0)
        blob_sizes[0] += current_blob_size;  // Merge the last blob with the first
    } else {
        blob_sizes.push_back(current_blob_size);  // Add the last blob as it is
    }

    return blob_sizes;
}




// Function to calculate the centroid of the points
cv::Point2f calculateCentroid(const std::vector<cv::Point2f>& points) {
    cv::Point2f centroid(0, 0);
    for (const auto& point : points) {
        centroid += point;
    }
    centroid.x /= points.size();
    centroid.y /= points.size();
    return centroid;
}


cv::Point2f multiplyPerspectiveMatrixWithPoint(const cv::Mat& perspectiveMatrix, const cv::Point2f& point) {
    // Convert the Point2f to a 3x1 matrix (homogeneous coordinates)
    cv::Mat pointMat = (cv::Mat_<double>(3, 1) << point.x, point.y, 1.0);

    // Perform the matrix multiplication
    cv::Mat resultMat = perspectiveMatrix * pointMat;

    // Convert back to Point2f by dividing by the third coordinate
    double w = resultMat.at<double>(2, 0);
    cv::Point2f resultPoint(resultMat.at<double>(0, 0) / w, resultMat.at<double>(1, 0) / w);

    return resultPoint;
}


bool InspectionTarget_CameraCalib::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");
  

  if(type=="save_latest_image")
  {
   
    if(cache_latest_input==NULL)
    {
      LOGE("cache_latest_input is NULL");
      return false;
    }

    auto current_ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    string img_saved_path = JFetch_STRING_ex(info,"path",to_string(current_ms)+".png");
    imwrite(img_saved_path,cache_latest_input->img);
  }
  if(type=="delete_saved_images")
  {
    string img_saved_path = JFetch_STRING_ex(info,"path","");
    if(img_saved_path=="")
    {
      LOGE("img_saved_path is empty");
      return false;
    }
    #ifdef _WIN32
    _wremove(img_saved_path.c_str());
    #else
    remove(img_saved_path.c_str());
    #endif
    return true;
  }

  if(type=="calc_calib_params_from_mark_info_list")
  {
    cJSON* jmark_info_list=JFetch_ARRAY(info,"mark_info_list");
    if(jmark_info_list==NULL || cJSON_GetArraySize(jmark_info_list)==0)
    {
      LOGE("mark_info_list is empty");
      return false;
    }

    float image_width=JFetch_NUMBER_ex(info,"image_width");
    float image_height=JFetch_NUMBER_ex(info,"image_height");
    if(image_width!=image_width || image_height!=image_height || image_width<=0||image_height<=0)
    {
      LOGE("image_width or image_height is not valid");
      return false;
    }
    Size imgSize(round(image_width),round(image_height));

    vector<vector<Point2f>> mark_locs_list;  // Points in 2D image space
    vector<vector<Point3f>> mark_coords_list; // Points in 3D object space

    int listSize=cJSON_GetArraySize(jmark_info_list);
    for(int i=0;i<listSize;i++)
    {
      cJSON* jmark_info=cJSON_GetArrayItem(jmark_info_list,i);
      cJSON* jmark_locs=JFetch_ARRAY(jmark_info,"mark_locs");
      cJSON* jmark_coords=JFetch_ARRAY(jmark_info,"mark_coords");
      vector<Point2f> mark_locs;
      StageInfo::cJSONArray2Point2fArray(jmark_locs,mark_locs);
      vector<Point3f> mark_coords;
      StageInfo::cJSONArray2Point3fArray(jmark_coords,mark_coords);

      //add image_width and image_height to jmark_info
      mark_locs_list.push_back(mark_locs);
      mark_coords_list.push_back(mark_coords);

    }


    // Camera matrix and distortion coefficients
    Mat cameraMatrix = Mat::eye(3, 3, CV_64F);
    Mat distCoeffs = Mat::zeros(8, 1, CV_64F);
    vector<Mat> rvecs, tvecs;

    // Calibrate the camera
    double rms = calibrateCamera(mark_coords_list, mark_locs_list, imgSize, cameraMatrix, distCoeffs, rvecs, tvecs,
                                 CALIB_FIX_K3 | CALIB_FIX_K4 | CALIB_FIX_K5);


    {
      cJSON* jrep=cJSON_CreateObject();
      cJSON* jcameraMatrix=cJSON_CreateObject();
      cJSON* jdistCoeffs=cJSON_CreateObject();
      StageInfo::Mat2cJson(jcameraMatrix,cameraMatrix);
      StageInfo::Mat2cJson(jdistCoeffs,distCoeffs);
      cJSON_AddItemToObject(jrep,"cameraMatrix",jcameraMatrix);
      cJSON_AddItemToObject(jrep,"distCoeffs",jdistCoeffs);

      cJSON_AddNumberToObject(jrep,"image_width",image_width);
      cJSON_AddNumberToObject(jrep,"image_height",image_height);

      act.send("RP",id,jrep);
    }
    return true;
    
  }
  if(type=="extract_pattern_points")
  {

    string img_saved_path = JFetch_STRING_ex(info,"path","");
    if(img_saved_path=="")
    {
      LOGE("img_saved_path is empty");
      return false;
    }

    Mat img=imread(img_saved_path,IMREAD_GRAYSCALE);
    if(img.empty())
    {
      LOGE("img is empty,path:%s",img_saved_path.c_str());
      return false;
    }

    Mat img_smooth;
    {//smooth
      // GaussianBlur(img, img_smooth, Size(11, 11), 6);


      int d = JFetch_NUMBER_ex(info,"bilateralFilter.d",0); 
      double sigmaColor = JFetch_NUMBER_ex(info,"bilateralFilter.sigmaColor",75);
      double sigmaSpace = JFetch_NUMBER_ex(info,"bilateralFilter.sigmaSpace",75);

      //bilateral filter
      if(d>0)
        bilateralFilter(img, img_smooth, d, sigmaColor, sigmaSpace);
      else
        img_smooth=img;
    }



    // {//use morphology to filter out noise in white background
    //   Mat kernel=getStructuringElement(MORPH_ELLIPSE,Size(13,13));
    //   morphologyEx(img_smooth,img_smooth,MORPH_CLOSE,kernel);
    // }

    vector<Point2f> corners;
    {

      int maxCorners = JFetch_NUMBER_ex(info,"maxCorners",100); 
      float qualityLevel = JFetch_NUMBER_ex(info,"qualityLevel",0.01);
      float minDistance = JFetch_NUMBER_ex(info,"minDistance",10.0);
      bool useHarrisDetector = JFetch_TRUE(info,"useHarrisDetector");
      double k = JFetch_NUMBER_ex(info,"k",0.04);
      int blockSize = JFetch_NUMBER_ex(info,"blockSize",3);
      Mat mask;
      goodFeaturesToTrack(img_smooth, corners, maxCorners, qualityLevel, minDistance, mask, blockSize, useHarrisDetector, k);


    }

    LOGE("corners:%d",corners.size());
    int chessboard_filter_N=JFetch_NUMBER_ex(info,"chessboard_filter.N",0);
    if(chessboard_filter_N>0)
    {
      //foreach cornor points find surrounding(circular) N points with radius r to find if it's a chessboard corner
      int N=chessboard_filter_N;
      float r=JFetch_NUMBER_ex(info,"chessboard_filter.r",10.0);
      int blob_size_threshold=JFetch_NUMBER_ex(info,"chessboard_filter.blob_size_threshold",N/5);
      int HL_diff_threshold=JFetch_NUMBER_ex(info,"chessboard_filter.HL_diff_threshold",100);

      vector<Point2f> surround_points_offsets;
      for(int i=0;i<N;i++)
      {
        float angle=2*M_PI*i/N;
        surround_points_offsets.push_back(Point2f(r*cos(angle),r*sin(angle)));
      }
      
      vector<int> pixel_values(surround_points_offsets.size());

      vector<Point2f> corners_filtered;

      int dfff=0;
      for(auto& corner:corners)
      {
        if(corner.x<r||corner.x>=img.cols-r||corner.y<r||corner.y>=img.rows-r)continue;
        dfff++;
        //find N points with radius r
        int idx=-1;
        int pixel_mid=0;
        int pixel_H=0;
        int pixel_L=255;
        for(auto& offset:surround_points_offsets)
        {
          idx++;
          Point2f surround_point=corner+offset;
          // if(surround_point.x<0||surround_point.x>=img.cols||surround_point.y<0||surround_point.y>=img.rows)
          //   continue;
          pixel_values[idx]=img_smooth.at<uint8_t>(surround_point);
          pixel_mid+=pixel_values[idx];
          pixel_H=max(pixel_H,pixel_values[idx]);
          pixel_L=min(pixel_L,pixel_values[idx]);
        }
        pixel_mid/=N;//threshold

        int ones_count=0;
        for(int i=0;i<N;i++)
        {
          if(pixel_values[i]>pixel_mid)
          {
            pixel_values[i]=1;
            ones_count++;
          }
          else
            pixel_values[i]=0;
        }

        vector<int> blob_sizes=findBlobSizes(pixel_values);
        //sort blob_sizes by size
        std::sort(blob_sizes.begin(),blob_sizes.end());

        if(blob_sizes.size()==4 && blob_sizes[blob_sizes.size()-1]-blob_sizes[0]<=blob_size_threshold && pixel_H-pixel_L>=HL_diff_threshold)
        {
          corners_filtered.push_back(corner);
        }

      }

      corners=corners_filtered;

    }


    int window_size = JFetch_NUMBER_ex(info,"refine.window_size",31);
    int zero_zone_size = JFetch_NUMBER_ex(info,"refine.zero_zone_size",5);
    if(corners.size()>0)
      cornerSubPix(img, corners, Size(window_size,window_size), Size(zero_zone_size, zero_zone_size),
                      TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 100, 0.01));

    {

      cJSON* jrep=cJSON_CreateObject();
      cJSON* jcorners=cJSON_CreateArray();
      StageInfo::Point2fArray2cJSONArray(jcorners,corners);

      cJSON_AddItemToObject(jrep,"corners",jcorners);
      act.send("RP",id,jrep);
      cJSON_Delete(jrep);jrep=NULL;
    }

    act.send(id,img_smooth,"jpg",90);


    return true;
  }

  if(type=="enable_image_accumulator")
  {
    bool enable=JFetch_TRUE(info,"enable");
    if(enable)
    {

      cache_latest_undistorted_image_accumulator_32F=Mat();
      cache_latest_undistorted_image_accumulator_count=0;
    }
    else
    {
      cache_latest_undistorted_image_accumulator_count=-1;
    }
    return true;
  }
  if(type=="get_refine_points_from_latest_undistorted_image" )
  {
    if(cache_latest_undistorted_image.empty())
    {
      LOGE("cache_latest_undistorted_image is empty");
      return false;
    }

    vector<Point2f> points;
    {
      cJSON* jpoints=JFetch_ARRAY(info,"points");
      if(jpoints==NULL)
      {
        LOGE("points is NULL");
        return false;
      }
      StageInfo::cJSONArray2Point2fArray(jpoints,points);
    }
    float window_size=JFetch_NUMBER_ex(info,"window_size",11);
    float zero_zone_size=JFetch_NUMBER_ex(info,"zero_zone_size",-1);


    cornerSubPix(cache_latest_undistorted_image, points, Size(window_size,window_size), Size(zero_zone_size, zero_zone_size),
                      TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 100, 0.01));

    {
      cJSON* jrep=cJSON_CreateObject();
      cJSON* jpoints=cJSON_CreateArray();
      StageInfo::Point2fArray2cJSONArray(jpoints,points);
      cJSON_AddItemToObject(jrep,"points",jpoints);
      act.send("RP",id,jrep);
      cJSON_Delete(jrep);jrep=NULL;
    }
    return true;
  }


  if(type=="calc_perspective_transform_from_4points_set")
  {

    cJSON* j4_points_set=JFetch_ARRAY(info,"4_points_set");
    if(j4_points_set==NULL || cJSON_GetArraySize(j4_points_set)!=4)
    {
      LOGE("mark_info is empty");
      return false;
    }

    // float image_width=JFetch_NUMBER_ex(info,"image_width");
    // float image_height=JFetch_NUMBER_ex(info,"image_height");
    // if(image_width!=image_width || image_height!=image_height || image_width<=0||image_height<=0)
    // {
    //   LOGE("image_width or image_height is not valid");
    //   return false;
    // }
    // Size imgSize(round(image_width),round(image_height));

    vector<Point2f> marks;
    vector<Point2f> coords;
    for(int i=0;i<4;i++)
    {
      cJSON* jpoint=cJSON_GetArrayItem(j4_points_set,i);
      if(jpoint==NULL)
      {
        LOGE("point %d is NULL",i);
        return false;
      }

      Point2f mark;
      Point2f coord;
      cJSON* jmark=JFetch_ARRAY(jpoint,"mark");
      cJSON* jcoord=JFetch_ARRAY(jpoint,"coord");
      if(jmark==NULL || cJSON_GetArraySize(jmark)!=2 || jcoord==NULL || cJSON_GetArraySize(jcoord)!=2)
      {
        LOGE("point %d is not a 2D point",i);
        return false;
      }
      mark.x=JFetch_NUMBER_ex(jmark,"[0]");
      mark.y=JFetch_NUMBER_ex(jmark,"[1]");
      coord.x=JFetch_NUMBER_ex(jcoord,"[0]");
      coord.y=JFetch_NUMBER_ex(jcoord,"[1]");


      //print
      LOGI("mark:%f,%f,coord:%f,%f",mark.x,mark.y,coord.x,coord.y);
      marks.push_back(mark);
      coords.push_back(coord);


    }


    vector<Point2f> marks_undistorted;
    undistortPoints(marks,marks_undistorted,cameraMatrix,distCoeffs, noArray(), cameraMatrix);

    for(auto& mark:marks_undistorted)
    {
      LOGI("mark_undistorted:%f,%f",mark.x,mark.y);
    }
    LOGI("size::  marks:%d,marks_undistorted:%d,coords:%d",marks.size(),marks_undistorted.size(),coords.size());
    cv::Mat perspectiveMatrix = cv::getPerspectiveTransform(marks_undistorted, coords);
    //coord=perspectiveMatrix*marks_undistorted




    Point2f center_undistorted_mark=calculateCentroid(marks_undistorted);
    Point2f center_coord=multiplyPerspectiveMatrixWithPoint(perspectiveMatrix,center_undistorted_mark);
    float scale=NAN;
    float rotation=NAN;
    Point2f translation(NAN,NAN);
    {

      float extendDist=100;
      Point2f center_undistorted_mark_xp1(center_undistorted_mark.x+extendDist,center_undistorted_mark.y);
      //apply perspectiveMatrix on center_undistorted_mark
      Point2f center_coord_xp1=multiplyPerspectiveMatrixWithPoint(perspectiveMatrix,center_undistorted_mark_xp1);



      float dist_undistorted=norm(center_undistorted_mark_xp1-center_undistorted_mark);
      float dist_distorted=norm(center_coord_xp1-center_coord);

      scale=1/(dist_distorted/dist_undistorted);
      rotation=atan2(center_coord_xp1.y-center_coord.y,center_coord_xp1.x-center_coord.x);
      translation=center_coord-center_undistorted_mark;

      LOGI("scale:%f,rotation:%f,translation:%f,%f",scale,rotation*180/M_PI,translation.x,translation.y);
    }


    {
      Point2f apply_translation=-center_coord;

      //Step1 offset to origin (0,0)
      Mat translationMatrix = (Mat_<double>(3, 3) << 1, 0, apply_translation.x, 0, 1, apply_translation.y, 0, 0, 1);
      perspectiveMatrix =   translationMatrix*perspectiveMatrix;

      //Step2 apply scaling
      float apply_scaling = scale;
      Mat scalingMatrix = (Mat_<double>(3, 3) << apply_scaling, 0, 0, 0, apply_scaling, 0, 0, 0, 1);
      perspectiveMatrix =scalingMatrix*perspectiveMatrix;

      //Step3 apply rotation
      float apply_rotation_rad = -rotation;
      Mat rotationMatrix = (Mat_<double>(3, 3) << cos(apply_rotation_rad), -sin(apply_rotation_rad), 0, sin(apply_rotation_rad), cos(apply_rotation_rad), 0, 0, 0, 1);
      perspectiveMatrix =rotationMatrix* perspectiveMatrix ;

      //Step4 apply translation to the center of the mark center
      Mat translationMatrix2 = (Mat_<double>(3, 3) << 1, 0, center_undistorted_mark.x, 0, 1, center_undistorted_mark.y, 0, 0, 1);

      perspectiveMatrix =translationMatrix2*perspectiveMatrix;

      Point2f result1=multiplyPerspectiveMatrixWithPoint(perspectiveMatrix,center_undistorted_mark);
      Point2f result2=multiplyPerspectiveMatrixWithPoint(perspectiveMatrix,marks_undistorted[0]);

      LOGI("center_undistorted_mark:%f,%f, result1:%f,%f",
        center_undistorted_mark.x,center_undistorted_mark.y,
        result1.x,result1.y);
      LOGI("marks_undistorted[0]:%f,%f, result2:%f,%f",marks_undistorted[0].x,marks_undistorted[0].y,result2.x,result2.y);



    }


    LOGI("getPerspectiveTransform::");
    cout << perspectiveMatrix << endl;
    {
      cJSON* jrep=cJSON_CreateObject();
      cJSON* jperspectiveMatrix=cJSON_CreateObject();
      StageInfo::Mat2cJson(jperspectiveMatrix,perspectiveMatrix);
      cJSON_AddItemToObject(jrep,"perspectiveMatrix",jperspectiveMatrix);
      act.send("RP",id,jrep);
    }

    return true;
  }

  // if(type=="calibrate_from_images")
  // {
  //   //chessboard style
  //   vector<string> img_paths=JFetch_STRING_ARRAY_ex(info,"img_paths");
  //   if(img_paths.size()==0)
  //   {
  //     LOGE("img_paths is empty");
  //     return false;
  //   }

  //   vector<Mat> imgs;
  //   for(auto& path:img_paths)
  //   {
  //     Mat img=imread(path,IMREAD_GRAYSCALE);
  //     if(img.empty())
  //     {
  //       LOGE("img is empty,path:%s",path.c_str());
  //       return false;
  //     }
  //     imgs.push_back(img);
  //   }

  //   if(imgs.size()==0)
  //   {
  //     LOGE("Not enough valid calibration images found. available image count:%d",imgs.size());
  //     return false;
  //   }

  //   cv::Mat cameraMatrix_tmp;
  //   cv::Mat distCoeffs_tmp;
  //   float rms;
  //   CalibProcessFolder(imgs,Size(42, 35),1,cameraMatrix_tmp,distCoeffs_tmp,rms);
    
  //   {

  //     cJSON* jrep=cJSON_CreateObject();
  //     cJSON* cameraMatrix_j=cJSON_CreateObject();
  //     cJSON* distCoeffs_j=cJSON_CreateObject();
  //     StageInfo::Mat2cJson(cameraMatrix_j,cameraMatrix_tmp);
  //     StageInfo::Mat2cJson(distCoeffs_j,distCoeffs_tmp);
  //     cJSON_AddItemToObject(jrep,"cameraMatrix",cameraMatrix_j);
  //     cJSON_AddItemToObject(jrep,"distCoeffs",distCoeffs_j);
  //     cJSON_AddNumberToObject(jrep,"image_width",imgs[0].cols);
  //     cJSON_AddNumberToObject(jrep,"image_height",imgs[0].rows);
      

  //     act.send("RP",id,jrep);
  //     cJSON_Delete(jrep);jrep=NULL;
  //   }
  //   return true;
  // }

  if(type=="enable_calibration")
  {
    enable_calibration=!(JFetch_FALSE(info,"enable"));

    return true;
  }
  

  if(type=="undistort_image")
  {

    string img_saved_path = JFetch_STRING_ex(info,"path","");
    if(img_saved_path=="")
    {
      LOGE("img_saved_path is empty");
      return false;
    }

    Mat img=imread(img_saved_path,IMREAD_GRAYSCALE);
    if(img.empty())
    {
      LOGE("img is empty,path:%s",img_saved_path.c_str());
      return false;
    }



    Mat img_undistorted;
    // undistort(img, img_undistorted, cameraMatrix, distCoeffs);

    int64 t0 = cv::getTickCount();
    
    remap(img, img_undistorted, undistortMapX, undistortMapY, INTER_LINEAR);

    int64 t1 = cv::getTickCount();
    LOGI("undistort time: %f ms", (t1 - t0) * 1000 / cv::getTickFrequency());
    cache_latest_undistorted_image=img_undistorted;

    {
      string path_tmp=img_saved_path;
      int strLen=path_tmp.length();
      int extDotLoc=path_tmp.find_last_of(".");
      if(strLen-extDotLoc>7)//it might be something weird, just give original path
      {
        cache_latest_undistorted_image_path=img_saved_path;
      }
      else
      {
        cache_latest_undistorted_image_path=path_tmp.substr(0,extDotLoc);
      }

    }


    // Mat img_distorted_back;
    // remap(img_undistorted, img_distorted_back, distortMapX, distortMapY, INTER_LINEAR);

    act.send(id,img_undistorted,"jpg",90);
    // act.send(id,img_distorted_back,"jpg",90);

    // imwrite("undistorted.png",img_undistorted);
    // imwrite("distorted_back.png",img_distorted_back);
    return true;
  }
  
  if(type=="save_latest_undistorted_image")
  {
    if(cache_latest_undistorted_image.empty())
    {
      LOGE("cache_latest_undistorted_image is empty");
      return false;
    }

    string img_saved_path = JFetch_STRING_ex(info,"path",cache_latest_undistorted_image_path+"_undistorted.png");
    if(img_saved_path=="")
    {
      LOGE("img_saved_path is empty");
      return false;
    }
    //test if the path is available to save image

    imwrite(img_saved_path,cache_latest_undistorted_image);
    return true;
  }
  
  return false;
}


void InspectionTarget_CameraCalib::singleProcess(shared_ptr<StageInfo> sinfo)
{


  int64 t0 = cv::getTickCount();


  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Image *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  cache_latest_input = sinfo;
  





  shared_ptr<StageInfo_Image> reportInfo(new StageInfo_Image());


  reportInfo->source_id=id;
  reportInfo->source=this;
  reportInfo->img_prop=sinfo->img_prop;
  reportInfo->trigger_id=sinfo->trigger_id;


  Mat img_undistorted; 
  if(enable_calibration) 
  {
    remap(sinfo->img, img_undistorted, undistortMapX, undistortMapY, INTER_LINEAR);
  }
  else
  {
    img_undistorted=sinfo->img;
  }
  cache_latest_undistorted_image=img_undistorted;

  if(cache_latest_undistorted_image_accumulator_count>=0)
  {
    if(
      cache_latest_undistorted_image_accumulator_32F.empty() || 
      cache_latest_undistorted_image_accumulator_32F.size()!=img_undistorted.size() ||
      cache_latest_undistorted_image_accumulator_32F.channels()!=img_undistorted.channels()
    )
    {
      //reset
      int img_undistorted_channelcount=img_undistorted.channels();
      cache_latest_undistorted_image_accumulator_32F=Mat::zeros(img_undistorted.size(),CV_32FC(img_undistorted_channelcount));
      cache_latest_undistorted_image_accumulator_count=0;
    }
    cache_latest_undistorted_image_accumulator_32F+=img_undistorted;

    cache_latest_undistorted_image_accumulator_count++;

    Mat avg_img=cache_latest_undistorted_image_accumulator_32F/cache_latest_undistorted_image_accumulator_count;
    //convert to img_undistorted pixel type
    avg_img.convertTo(img_undistorted,img_undistorted.type());

  }




  reportInfo->img=img_undistorted;
  reportInfo->img_show=img_undistorted;


  reportInfo->img_prop.distCoeffs=
  reportInfo->img_prop.cameraMatrix=this->cameraMatrix;
  reportInfo->img_prop.mmpp=this->mmpp;
  reportInfo->process_time_us=(cv::getTickCount()-t0)*1000000/cv::getTickFrequency();
  reportInfo->create_time_sysTick=t0;
  reportInfo->error_code=0;
  reportInfo->error_msg="";


  StageInfoFillDefault(reportInfo.get(),sinfo.get());

  reportInfo->genJsonRepTojInfo();

  // cache_latest_result = reportInfo;
  belongMan->dispatch(reportInfo);
}


InspectionTarget_CameraCalib::~InspectionTarget_CameraCalib()
{
 
}
