#ifndef UTIL_H
#define UTIL_H

#include <opencv2/core.hpp>
#include <vector>

namespace InspTarUtil {

// Function declarations
cv::Mat opencv_rotCrop_matrix(float rotate_center_x, float rotate_center_y, float rotate_angle_rad, float scale, float post_rotate_center_x, float post_rotate_center_y);

cv::Mat invertAffineTransform(const cv::Mat& affineMat);

cv::Mat opencv_rotCrop(cv::Mat inputImage, float center_x, float center_y, float width, float height, float angle);

void shuffle(int *array, int size);

std::vector<int> generateUniqueNumbers(int N);

cv::RotatedRect circleFitting(std::vector<cv::Point3f> &pts, int *idsList = nullptr, int idsList_size = -1);

float computeAngleImageCoordinates(const cv::Point2f& center, const cv::Point2f& point);

float angleSub(float angle1, float angle2);

float normalizeAngle_0_2PI(float angle);

float normalizeAngle_nPI_PI(float angle);

bool findCircleFrom3PointsWithArc(const cv::Point2f& p1, const cv::Point2f& p2, const cv::Point2f& p3, 
                                  cv::Point2f& center, float& radius, float& startAngle, float& endAngle);

float point2CircleDistance(const cv::Point2f pt, const cv::Point2f& circle_c, float circle_r);

float circleDistance(const cv::Point2f& center1, float radius1, const cv::Point2f& center2, float radius2);

std::vector<int> findClosestGroup(const std::vector<cv::RotatedRect>& circle, float threshold);

cv::RotatedRect fitCircleByRansac(std::vector<cv::Point3f>& points, int tryCount = 20, int batch_size = 100);



cv::Point2f rotate2d(const cv::Point2f inPoint, float sin, float cos);
cv::Point2f rotate2d(const cv::Point2f inPoint, const double angRad);

} // namespace InspTarUTIL

#endif // UTIL_H
