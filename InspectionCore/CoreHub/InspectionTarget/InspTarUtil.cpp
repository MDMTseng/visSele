#include <iostream>
#include <opencv2/video/tracking.hpp>
#include <acvImage_BasicTool.hpp>
#include <random>
#include <numeric>
#include <circleFitting.h>
#include <InspTarUtil.hpp>
#include "logctrl.h"


namespace InspTarUtil {
cv::Mat opencv_rotCrop_matrix(float rotate_center_x, float rotate_center_y, float rotate_angle_rad,float scale, float post_rotate_center_x, float post_rotate_center_y) {
    // Convert radians to degrees for OpenCV
    double angle_degrees = rotate_angle_rad * 180.0 / M_PI;

    // Define the center of the region for rotation
    cv::Point2f center(rotate_center_x, rotate_center_y);

    // Get the rotation matrix with translation to the region center
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, -angle_degrees, scale);

    // Adjust the matrix to center the cropped image in the output frame
    rotationMatrix.at<double>(0, 2) += post_rotate_center_x - rotate_center_x;
    rotationMatrix.at<double>(1, 2) += post_rotate_center_y - rotate_center_y;

    return rotationMatrix;
}

cv::Mat invertAffineTransform(const cv::Mat& affineMat) {
    // Convert 2x3 affine matrix to 3x3 homogeneous matrix
    cv::Mat homogenousMat = cv::Mat::eye(3, 3, CV_64F);
    affineMat.copyTo(homogenousMat(cv::Rect(0, 0, 3, 2)));  // Copy 2x3 into 3x3

    // Invert the 3x3 matrix
    cv::Mat invHomogeneousMat;
    cv::invert(homogenousMat, invHomogeneousMat);

    // Convert back to 2x3 affine matrix
    cv::Mat invAffineMat = invHomogeneousMat(cv::Rect(0, 0, 3, 2));  // Extract first two rows

    return invAffineMat;
}

cv::Mat opencv_rotCrop(cv::Mat inputImage, float center_x, float center_y, float width, float height, float angle) {
    // Get the rotation matrix with translation to the region center
    cv::Mat rotationMatrix = opencv_rotCrop_matrix(center_x, center_y, angle, 1.0, width/2, height/2);

    // Apply the affine transformation to rotate and crop the region
    cv::Mat rotatedImage;
    cv::warpAffine(inputImage, rotatedImage, rotationMatrix, cv::Size(width, height), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));

    return rotatedImage;
}



void shuffle(int *array, int size) {
  for (int i = size - 1; i > 0; i--) {
    // Generate a random index between 0 and i
    int j = rand() % (i + 1);
    
    // Swap array[i] with array[j]
    int temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}


std::vector<int> generateUniqueNumbers(int N) {

  std::vector<int> numbers(N + 1);
  
  // Initialize the array with float values from 0 to N
  for (int i = 0; i <numbers.size(); i++) {
      numbers[i] = i;
  }

  // Seed the random number generator
  srand(time(NULL));

  // Shuffle the array to randomize the order of numbers
  shuffle(numbers.data(), numbers.size());

  return numbers;
}




cv::RotatedRect circleFitting(std::vector<cv::Point3f> &pts,int *idsList,int idsList_size)
{
  static Data CircleFitData(2000);
  //CircleFitData.resize_force(0);

  if(idsList==NULL || idsList_size<0)
  {
    idsList=NULL;
    idsList_size=-1;
  }


  if(idsList_size<0)
  {
    CircleFitData.resize(pts.size());
    for(int i=0;i<pts.size();i++)
    {
      CircleFitData.X[i]=pts[i].x;
      CircleFitData.Y[i]=pts[i].y;
      CircleFitData.W[i]=pts[i].z;
    }

  }
  else
  {
    CircleFitData.resize(idsList_size);
    for(int i=0;i<idsList_size;i++)
    {
      int index=idsList[i];
      CircleFitData.X[i]=pts[index].x;
      CircleFitData.Y[i]=pts[index].y;
      CircleFitData.W[i]=pts[index].z;
    }
  }

  Circle circle;
  circle = CircleFitByHyper (CircleFitData);
  cv::RotatedRect rect;
  rect.center.x=circle.a;
  rect.center.y=circle.b;
  rect.angle=0;
  rect.size.width=rect.size.height=circle.r*2;
  rect.angle=circle.s;//just to overload the fitting sigma
  return rect;
}



// Function to compute angle between a point and the center in radians, accounting for image coordinates (flipped y-axis)
float computeAngleImageCoordinates(const cv::Point2f& center, const cv::Point2f& point) {
    return std::atan2(center.y - point.y, point.x - center.x);  // Inverted y-axis for image coordinates
}



float angleSub(float angle1,float angle2) {
  float angle = angle1 - angle2;
  angle = normalizeAngle_0_2PI(angle);
  return angle;
}

// Normalize angles to be within [0, 2*PI]

float normalizeAngle_0_2PI(float angle) {
  angle = fmod(angle, 2 * CV_PI);//-2PI~2PI
  if (angle < 0) angle += 2 * CV_PI;//0~2PI
  return angle;
}
float normalizeAngle_nPI_PI(float angle) {
  angle = fmod(angle, 2 * CV_PI);//-2PI~2PI

  if (angle < 0) angle += 2 * CV_PI;//0~2PI
  if (angle > CV_PI)
    angle -= 2 * CV_PI;//-PI~PI
  return angle;
}


// Function to compute the center, radius, and arc angles (start and end) from three points
bool findCircleFrom3PointsWithArc(const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3, 
                                  cv::Point2f& center, float& radius, float& startAngle_rad, float& endAngle_rad) {
    // Calculate midpoints of two segments (p1-p2 and p2-p3)
    cv::Point2f mid1 = (p1 + p2) * 0.5f;
    cv::Point2f mid2 = (p2 + p3) * 0.5f;

    // Slopes of p1-p2 and p2-p3
    float slope1 = (p2.y - p1.y) / (p2.x - p1.x);
    float slope2 = (p3.y - p2.y) / (p3.x - p2.x);

    // Slopes of the perpendicular bisectors
    float perpSlope1 = -1.0f / slope1;
    float perpSlope2 = -1.0f / slope2;

    // Find the center of the circle by solving for the intersection of the perpendicular bisectors
    float centerX = (perpSlope1 * mid1.x - perpSlope2 * mid2.x + mid2.y - mid1.y) / (perpSlope1 - perpSlope2);
    float centerY = perpSlope1 * (centerX - mid1.x) + mid1.y;
    center = cv::Point2f(centerX, centerY);

    // Calculate the radius
    radius = std::sqrt((center.x - p1.x) * (center.x - p1.x) + (center.y - p1.y) * (center.y - p1.y));

    // Compute the angles of the points with respect to the center (flipping y-axis for image coordinates)
    float angle1 = computeAngleImageCoordinates(center, p1);  // Start point angle
    float angle2 = computeAngleImageCoordinates(center, p2);  // Middle point angle (used to check arc direction)
    float angle3 = computeAngleImageCoordinates(center, p3);  // End point angle

    // Normalize angles to [0, 2*PI]
    angle1 = normalizeAngle_nPI_PI(angle1);
    angle2 = normalizeAngle_nPI_PI(angle2);
    angle3 = normalizeAngle_nPI_PI(angle3);




    float angle21 = angleSub(angle2,angle1);
    float angle31 = angleSub(angle3,angle1);

    if (angle31 > angle21)
    {
      startAngle_rad = angle1;
      endAngle_rad = angle3;
    }
    else
    {
      startAngle_rad = angle3;
      endAngle_rad = angle1;
    }

    if(endAngle_rad<startAngle_rad)
    {
      endAngle_rad+=2*CV_PI;
    }
    // Convert angles to degrees for easier understanding (optional)
    // startAngle = startAngle * 180.0f / CV_PI;
    // endAngle = endAngle * 180.0f / CV_PI;

    return true;
}


float point2CircleDistance(const cv::Point2f pt, const cv::Point2f& circle_c, float circle_r) {
  float centerDist = std::hypot(pt.x - circle_c.x, pt.y - circle_c.y);
  return abs(centerDist-circle_r);
}


float circleDistance(const cv::Point2f& center1, float radius1, const cv::Point2f& center2, float radius2) {
    float centerDist = std::hypot(center1.x - center2.x, center1.y - center2.y);
    return std::max(0.0f, centerDist + abs(radius1 - radius2));  // If circles overlap, distance is 0
}

// Find the closest group of circles using a threshold for "closeness"
std::vector<int> findClosestGroup(const std::vector<cv::RotatedRect>& circle, float threshold) {
    int N = circle.size();
    
    // Create a graph adjacency matrix to represent connected circles
    std::vector<std::vector<int>> adjacencyList(N);
    
    // Find pairs of circles that are "close" (distance below threshold)
    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            float dist = circleDistance(circle[i].center, circle[i].size.width/2, circle[j].center, circle[j].size.width/2);
            // std::cout<<"dist:"<<dist<<std::endl;
            if (dist < threshold) {
                // If the circles are close, mark them as connected
                adjacencyList[i].push_back(j);
                adjacencyList[j].push_back(i);
            }
        }
    }

    // Function to perform DFS and collect a group of connected circles
    std::function<void(int, std::vector<bool>&, std::vector<int>&)> dfs =
        [&](int node, std::vector<bool>& visited, std::vector<int>& group) {
            visited[node] = true;
            group.push_back(node);
            for (int neighbor : adjacencyList[node]) {
                if (!visited[neighbor]) {
                    dfs(neighbor, visited, group);
                }
            }
        };

    // Find all groups of connected circles using DFS
    std::vector<bool> visited(N, false);
    std::vector<std::vector<int>> groups;
    
    for (int i = 0; i < N; ++i) {
        if (!visited[i]) {
            std::vector<int> group;
            dfs(i, visited, group);
            groups.push_back(group);
        }
    }

    // Find the group with the smallest average pairwise distance
    float minDistance = std::numeric_limits<float>::max();
    std::vector<int> closestGroup;
    
    for (const auto& group : groups) {
        float totalDistance = 0.0f;
        int count = 0;
        
        // Compute the sum of pairwise distances within the group
        for (size_t i = 0; i < group.size(); ++i) {
            for (size_t j = i + 1; j < group.size(); ++j) {
              int idx1=group[i];  
              int idx2=group[j];
                totalDistance += circleDistance(circle[idx1].center, circle[idx1].size.width/2, circle[idx2].center, circle[idx2].size.width/2);
                ++count;
            }
        }
        
        // Find the group with the smallest average distance
        if (count > 0) {
            float avgDistance = totalDistance / count;
            if (avgDistance < minDistance) {
                minDistance = avgDistance;
                closestGroup = group;
            }
        }
    }

    return closestGroup;  // Return the indices of the closest group
}





cv::RotatedRect fitCircleByRansac(std::vector<cv::Point3f>& points,int tryCount,int batch_size)
{
  cv::RotatedRect best_circle;

  std::vector<int> indexes=generateUniqueNumbers(points.size()-1);

  LOGE("<<<< tryCount:%d batch_size:%d",tryCount,batch_size);
  std::vector<cv::RotatedRect> fit_circles;
  for(int i=0;i<tryCount;i++)
  {
    cv::RotatedRect circle= circleFitting(points,&(indexes[0])+i*batch_size,batch_size);
    fit_circles.push_back(circle);
    
    std::cout<<"try:"<<i<<" circle:"<<circle.center.x<<","<<circle.center.y<<",R"<<circle.size.width/2<<std::endl;
  }

  LOGE("<<<< tryCount:%d batch_size:%d",tryCount,batch_size);
  
  std::vector<int> closest_group;
  
  if(tryCount>1)
  {
    closest_group=findClosestGroup(fit_circles,50);
  }
  else
  {
    closest_group.push_back(0);
  }

  LOGE("<<<<");
  
  // for(int i=0;i<closest_group.size();i++)
  // {
  //   std::cout<<"closest_group:"<<closest_group[i]<<std::endl;

  //   std::cout<<"try:"<<closest_group[i]<<" circle:"<<fit_circles[closest_group[i]].center.x<<","<<fit_circles[closest_group[i]].center.y<<",R"<<fit_circles[closest_group[i]].size.width/2<<std::endl;
  // }

  best_circle=fit_circles[closest_group[0]];


  LOGE("<<<<");
  
  // std::cout<<"best_circle_init:"<<best_circle.center.x<<" "<<best_circle.center.y<<" s:"<<best_circle.angle<<std::endl;
  indexes.clear();
  indexes.reserve(points.size());
  for(int i=0;i<points.size();i++)//add good fit points
  {
    //point to circle distance 
    float dist=point2CircleDistance({points[i].x,points[i].y},best_circle.center,best_circle.size.width/2);
    if(dist>10)continue;
    indexes.push_back(i);
  }
  std::cout<<"indexes.size:"<<indexes.size()<<std::endl;


  LOGE("<<<<");
  
  best_circle= circleFitting(points,&(indexes[0]),indexes.size());
  // std::cout<<"best_circle_refine:"<<best_circle.center.x<<" "<<best_circle.center.y<<" s:"<<best_circle.angle<<std::endl;


  LOGE("<<<<");
  
  return best_circle;
}



void drawSmoothCircle(cv::Mat& img, cv::Point2f center, float radius, int segments, cv::Scalar color, int thickness = 1) {
    std::vector<cv::Point> points;

    // Generate points along the circle's perimeter
    for (int i = 0; i < segments; ++i) {
        float theta = 2.0f * CV_PI * i / segments;  // Angle in radians
        int x = static_cast<int>(center.x + radius * std::cos(theta));
        int y = static_cast<int>(center.y + radius * std::sin(theta));
        points.emplace_back(x, y);
    }

    // Draw the polygonal approximation of the circle
    for (int i = 0; i < segments; ++i) {
        cv::line(img, points[i], points[(i + 1) % segments], color, thickness);
    }
}




cv::Point2f rotate2d(const cv::Point2f inPoint, float sin, float cos)
{
    cv::Point2f outPoint;
    //CW rotation
    outPoint.x = cos*inPoint.x - sin*inPoint.y;
    outPoint.y = sin*inPoint.x + cos*inPoint.y;
    return outPoint;
}
cv::Point2f rotate2d(const cv::Point2f inPoint, const double angRad)
{
    return rotate2d(inPoint,sin(angRad),cos(angRad));
}






} // namespace InspTarUTIL