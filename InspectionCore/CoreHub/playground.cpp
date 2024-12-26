#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

using namespace cv;
using namespace std;



namespace cvM3x3{
  //just a note, opencv matrix operation is Max*Vec=newVec
  //So the matrix compose is from right(first) to left(last) 
  //ie, if you want transform order m1,m2,m3, then =>   m3*m2*m1*vec=newVec

  cv::Mat rotate(float angle_rad) 
  {
    double cos_val = cos(angle_rad);  
    double sin_val = sin(angle_rad);
    cv::Mat mat33 = cv::Mat::eye(3,3,CV_64F);
    mat33.at<double>(0,0) = cos_val;
    mat33.at<double>(0,1) = -sin_val;  // Correct
    mat33.at<double>(1,0) = sin_val;   // Correct 
    mat33.at<double>(1,1) = cos_val;
    return mat33;
  }

  cv::Mat translate(Point2f pt)
  {
    cv::Mat mat33=cv::Mat::eye(3,3,CV_64F);
    mat33.at<double>(0,2)=pt.x;
    mat33.at<double>(1,2)=pt.y;
    return mat33;
  }


  cv::Mat scale(float scale)
  {
    cv::Mat mat33=cv::Mat::eye(3,3,CV_64F);
    mat33.at<double>(0,0)=scale;
    mat33.at<double>(1,1)=scale;
    return mat33;
  }


  Mat mat23to33(const Mat& matrix23) {
      Mat matrix33 = Mat::eye(3, 3, CV_64F);
      matrix23.copyTo(matrix33(Rect(0, 0, 3, 2)));
      return matrix33;
  }

  Mat mat33to23(const Mat& matrix33) {
      return matrix33(Rect(0, 0, 3, 2)).clone();
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
    matchTemplate(searchROI, templateROI, result, TM_CCOEFF_NORMED);
    
    // Find the maximum location first
    double minVal, maxVal;
    Point minLoc, maxLoc;
    minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    static int counter=0;
    // cv::imwrite("data/result"+to_string(counter)+".png",result*255);
    counter++;
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


// Function to refine pose using template matching
float refinePoseWithTemplateMatching(
    const Mat& templateImg,
    const Mat& targetImg,
    const vector<Rect>& rois,
    float scale,
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

    int count=0;
    vector<Point2f> allValidTemplatePoints, allValidTargetPoints;

    float min_confidence=numeric_limits<float>::max();
    for (const auto& roi : rois) {
        count++;
        // Extract template ROI

        // Calculate the transformation for this ROI
        Mat translationMat = cvM3x3::translate(-initOffset);
        Mat rotationMat = cvM3x3::rotate(-initRotation);

        Mat ROItranslationMat = cvM3x3::translate(Point2f(-roi.x,-roi.y));
        Mat scaleMat=cvM3x3::scale(1/scale);
        Mat warpMat = cvM3x3::mat33to23(ROItranslationMat*scaleMat *  rotationMat*translationMat);
        // Adjust the transformation matrix to account for ROI offset

        // Create warped ROI directly
        Mat warpedTargetROI;
        cv::warpAffine(targetImg, warpedTargetROI, warpMat, roi.size());


        imwrite("data/warpedTargetROI"+to_string(count)+"i"+to_string(DBG_iterCount)+".png",warpedTargetROI);
        // Extract expanded template ROI
        Rect expandedTemplateRect(roi.x-searchBorder, roi.y-searchBorder, 
                                roi.width+2*searchBorder, roi.height+2*searchBorder);
        Mat expandedTemplateROI = templateImg(expandedTemplateRect);
        
        float current_match_score=0;

        Point2f displacement;

        if(useOptFlow==false)
        {
            // Perform template matching with subpixel refinement
            Point2f matchLoc = templateMatchSubpixel(warpedTargetROI, expandedTemplateROI,current_match_score);
            displacement=Point2f(searchBorder - matchLoc.x, searchBorder - matchLoc.y);
        }
        else
        {
            Mat templateROI=templateImg(roi);
            vector<Point2f> templatePoints;
            templatePoints.push_back(Point2f(templateROI.cols/2, templateROI.rows/2));//center of the template
            

            vector<Point2f> targetPoints;
            vector<uchar> status;
            vector<float> err;
            calcOpticalFlowPyrLK(templateROI, warpedTargetROI, templatePoints, targetPoints, status, err);

            displacement=targetPoints[0]-templatePoints[0];

        }
        



        float score_zero_threshold=0.2;
        float current_confidence=(current_match_score-score_zero_threshold)/(1-score_zero_threshold);
        current_confidence=current_confidence>0?current_confidence:0;

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

    // Perform single affine estimation with all collected points
    if (allValidTemplatePoints.size() >= 2) {
        Mat transform = estimateAffinePartial2D(allValidTemplatePoints, allValidTargetPoints);
        
        if (!transform.empty()) {
            // Decompose transformation matrix into rotation and translation
            double dx = transform.at<double>(0, 2);
            double dy = transform.at<double>(1, 2);
            double theta = atan2(transform.at<double>(1, 0), transform.at<double>(0, 0));

            // Update the initial pose
            initOffset += Point2f(dx, dy);
            initRotation += theta;
        }
    }
    else if(allValidTemplatePoints.size()==1)
    {
        initOffset-=allValidTemplatePoints[0]-allValidTargetPoints[0];
    }
    else
    {
        return 0;//no valid points
    }

    return min_confidence;
}



float ImageScale=1;

int main() {
    // Load template and target images

    Mat templateImg = imread("data/PGImg.png", IMREAD_GRAYSCALE);

    if (templateImg.empty()) {
        cerr << "Error: Could not load images." << endl;
        return -1;
    }

    Point2f trueOffset(120/1.5*ImageScale,-120/1.5*ImageScale);
    double trueRotation = 20*CV_PI / 180;

    Point2f initOffsetNoise(5,6); // Rough offset
    initOffsetNoise*=ImageScale;
    double initRotationNoise=1*CV_PI / 180; // Rough rotation in radians
    // Initial rough pose
    Point2f initOffset=trueOffset+initOffsetNoise; // Rough offset
    double initRotation = trueRotation+initRotationNoise; // Rough rotation in radians
    


    cout << "-------------------Init---------------------" << endl;
    // Output refined pose
    cout << "Initial Offset: (" << initOffset.x << ", " << initOffset.y << ")" << endl;
    cout << "Initial Rotation (degrees): " << initRotation * 180.0 / CV_PI << endl;

    Mat targetImg=warpImage(templateImg,trueOffset,trueRotation,ImageScale);
    // imwrite("data/targetImg.png",targetImg);
    if(1){
        //reduce image contrast
        targetImg=targetImg*0.6+20;


        //add image noise
        Mat noiseImg=Mat::zeros(targetImg.size(),targetImg.type());
        randn(noiseImg,0,30);
        add(targetImg,noiseImg,targetImg);

    }


    int tw=60;
    int th=60;
    // Define subregions (ROIs) on the template
    vector<Rect> rois = {
        Rect(191-tw/2, 164-th/2, tw, th),
        Rect(504-tw/2, 163-th/2, tw, th),
        // Rect(506-tw/2, 387-th/2, tw, th)
    };

    const int MAX_ITERATIONS = 4;  // Maximum number of iterations
    const double CONVERGENCE_THRESHOLD = 0.1;  // Threshold for convergence
    
    Point2f prevOffset = initOffset;
    double prevRotation = initRotation;
    
    auto start = chrono::high_resolution_clock::now();  
    

    float confidence_threshold=0.5;

    int border_search_size=35;
    // Iterative refinement loop
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        DBG_iterCount=iter;
        float min_confidence=refinePoseWithTemplateMatching(
            templateImg, 
            targetImg, 
            rois, 
            ImageScale,
            initOffset, 
            initRotation,
            border_search_size,
            confidence_threshold,
            iter==MAX_ITERATIONS-1

        );
        if(min_confidence<confidence_threshold)
        {
            break;
        }
        // Check for convergence
        double offsetDiff = norm(prevOffset - initOffset);
        double rotationDiff = abs(prevRotation - initRotation);
        
        cout << "Iteration " << iter + 1 << ":" << endl;
        cout << "  Offset: (" << initOffset.x << ", " << initOffset.y << ")" << endl;
        cout << "  Rotation (degrees): " << initRotation * 180.0 / CV_PI << endl;
        cout << "  Min Confidence: " << min_confidence << endl;
        if (offsetDiff < CONVERGENCE_THRESHOLD && rotationDiff < CONVERGENCE_THRESHOLD) {
            cout << "Converged after " << iter + 1 << " iterations" << endl;
            break;
        }
        
        prevOffset = initOffset;
        prevRotation = initRotation;
    }
    



    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();
    cout << "Time taken: " << duration << " milliseconds" << endl;


    {
        // initOffset*=ImageScale;
        //overlap templateImg and targetImg(warp back with initOffset and initRotation)
        Mat warpBackImg=warpImage(targetImg,initOffset,initRotation,ImageScale,true);
        Mat overlapImg=templateImg.clone();
        addWeighted(overlapImg, 0.5, warpBackImg, 0.5, 0, overlapImg);
        imwrite("data/overlap.png",overlapImg);



        

    }



    cout << "-------------------Result---------------------" << endl;

    // Output refined pose
    cout << "Refined Offset: (" << initOffset.x << ", " << initOffset.y << ")" << endl;
    cout << "True Offset: (" << trueOffset.x << ", " << trueOffset.y << ")" << endl;
    cout << "Refined Rotation (degrees): " << initRotation * 180.0 / CV_PI << endl;
    cout << "True Rotation (degrees): " << trueRotation * 180.0 / CV_PI << endl;

    return 0;
}
