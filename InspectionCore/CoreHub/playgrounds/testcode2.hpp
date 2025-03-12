#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <opencv2/rgbd/linemod.hpp>
using namespace cv;
using namespace std;


struct refine_region_info{
    Mat img;
    cv::Rect2d regionInRef;
};

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
    cv::imwrite("data/result"+to_string(counter)+".png",result*255);
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
    const vector<refine_region_info>& refine_region_set,
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
    for (const auto& refSegInfo : refine_region_set) {
        count++;
        // Extract template ROI

        auto roi=refSegInfo.regionInRef;

        // Calculate the transformation for this ROI
        Mat translationMat = cvM3x3::translate(-initOffset);
        Mat rotationMat = cvM3x3::rotate(-initRotation);

        Rect ex_roi=roi;

        if(!useOptFlow)
        {
            ex_roi=Rect(roi.x-searchBorder, roi.y-searchBorder, 
                                roi.width+2*searchBorder, roi.height+2*searchBorder);
        }



        Mat ROItranslationMat = cvM3x3::translate(Point2f(-ex_roi.x,-ex_roi.y));
        Mat scaleMat=cvM3x3::scale(1/scale);
        Mat warpMat = cvM3x3::mat33to23(ROItranslationMat*scaleMat *  rotationMat*translationMat);
        // Adjust the transformation matrix to account for ROI offset

        // Create warped ROI directly
        Mat warpedTargetROI;
        cv::warpAffine(targetImg, warpedTargetROI, warpMat, ex_roi.size());


        // imwrite("data/warpedTargetROI"+to_string(count)+"i"+to_string(DBG_iterCount)+".png",warpedTargetROI);
        // Extract expanded template ROI
        // Mat expandedTemplateROI = refSegInfo.img;
        
        float current_match_score=0;

        Point2f displacement;

        if(useOptFlow==false)
        {
            // Perform template matching with subpixel refinement
            Point2f matchLoc = templateMatchSubpixel(refSegInfo.img,warpedTargetROI, current_match_score);
            displacement=Point2f( matchLoc.x-searchBorder, matchLoc.y-searchBorder);
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



float ImageScale=0.7;

int mainx() {
    // Load template and target images

    Mat templateImg = imread("data/PGImg.png", IMREAD_GRAYSCALE);

    if (templateImg.empty()) {
        cerr << "Error: Could not load images." << endl;
        return -1;
    }

    Point2f trueOffset(140/1.5*ImageScale,-140/1.5*ImageScale);
    double trueRotation = 30*CV_PI / 180;

    Point2f initOffsetNoise(5,6); // Rough offset
    initOffsetNoise*=ImageScale;
    srand(time(0));
    
    double initRotationNoise=((rand()%2000)/1000.0-1)*2*CV_PI / 180; // Rough rotation in radians
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
        targetImg=targetImg*0.9+20;


        //add image noise
        Mat noiseImg=Mat::zeros(targetImg.size(),targetImg.type());
        randn(noiseImg,0,10);
        add(targetImg,noiseImg,targetImg);


        // GaussianBlur(targetImg,targetImg,Size(15,15),0);
        // GaussianBlur(templateImg,templateImg,Size(15,15),0);
    }


    int tw=60;
    int th=60;
    // Define subregions (ROIs) on the template
    vector<Rect> rois = {
        Rect(191-tw/2, 164-th/2, tw, th),
        Rect(504-tw/2, 163-th/2, tw, th),
        // Rect(506-tw/2, 387-th/2, tw, th)
    };


    int border_search_size=30;
    vector<refine_region_info> refine_region_set;
    for(const auto& roi : rois)
    {
        refine_region_info info;
        info.regionInRef = roi;
        
        // Ensure ROI is within image bounds
        Rect safeRoi = roi & Rect(0, 0, templateImg.cols, templateImg.rows);
        if (safeRoi.area() > 0 && safeRoi == roi) {
            info.img = templateImg(safeRoi).clone();  // Use clone() for deep copy
            refine_region_set.push_back(info);
        } else {
            cerr << "Warning: ROI " << roi << " is outside image bounds or partially outside. Skipping." << endl;
        }
    }

    const int MAX_ITERATIONS = 5;  // Maximum number of iterations
    const double CONVERGENCE_THRESHOLD = 0.1;  // Threshold for convergence
    
    Point2f prevOffset = initOffset;
    double prevRotation = initRotation;
    
    auto start = chrono::high_resolution_clock::now();  
    

    float confidence_threshold=0.5;

    // Iterative refinement loop
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        DBG_iterCount=iter;
        float min_confidence=refinePoseWithTemplateMatching(
            targetImg, 
            refine_region_set, 
            ImageScale,
            initOffset, 
            initRotation,
            border_search_size,
            confidence_threshold,
            true

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



// ----------------------------------------------------------
// Evaluate a polynomial of degree = coefficients.size() - 1
// coefficients in ascending order: a0 + a1*x + a2*x^2 + ...
// ----------------------------------------------------------
double evaluatePolynomial(const std::vector<double>& coeffs, double x)
{
    double result = 0.0;
    double powX   = 1.0;
    for (double c : coeffs)
    {
        result += c * powX;
        powX   *= x;
    }
    return result;
}

// ----------------------------------------------------------
// Solve linear system A * b = X using naive Gaussian elimination
// This is for demonstration only. Use a robust library in production.
// Returns false if system is singular (cannot be solved).
// ----------------------------------------------------------
bool solveLinearSystem(std::vector<std::vector<double>>& A, std::vector<double>& b)
{
    int n = static_cast<int>(A.size());

    for (int i = 0; i < n; i++)
    {
        // Find pivot (largest absolute value)
        double maxEl = std::fabs(A[i][i]);
        int pivotRow = i;
        for (int k = i + 1; k < n; k++)
        {
            if (std::fabs(A[k][i]) > maxEl)
            {
                maxEl = std::fabs(A[k][i]);
                pivotRow = k;
            }
        }

        // If pivot is nearly zero => singular
        if (std::fabs(maxEl) < 1e-12)
            return false;

        // Swap pivot row with current row
        if (pivotRow != i)
        {
            std::swap(A[i], A[pivotRow]);
            std::swap(b[i], b[pivotRow]);
        }

        // Eliminate below pivot
        for (int k = i + 1; k < n; k++)
        {
            double c = -A[k][i] / A[i][i];
            for (int j = i; j < n; j++)
            {
                if (i == j)
                {
                    A[k][j] = 0;
                }
                else
                {
                    A[k][j] += c * A[i][j];
                }
            }
            b[k] += c * b[i];
        }
    }

    // Back-substitution
    for (int i = n - 1; i >= 0; i--)
    {
        b[i] /= A[i][i];
        A[i][i] = 1.0;
        for (int k = i - 1; k >= 0; k--)
        {
            b[k] -= A[k][i] * b[i];
            A[k][i] = 0;
        }
    }

    return true;
}

// ----------------------------------------------------------
// Fit a polynomial of degree 'polyDegree' using least squares
// points: (x_i, y_i), i = 1..N
// ----------------------------------------------------------
std::vector<double> fitPolynomial(const std::vector<cv::Point2f>& points, int polyDegree)
{
    if (points.size() < static_cast<size_t>(polyDegree + 1))
        throw std::runtime_error("Not enough points to fit the requested polynomial degree.");

    const int numCoeffs = polyDegree + 1;
    const int N = static_cast<int>(points.size());

    // Normal Eq: (X^T X) * a = X^T y
    // We'll accumulate X^T X and X^T y
    std::vector<std::vector<double>> XtX(numCoeffs, std::vector<double>(numCoeffs, 0.0));
    std::vector<double> XtY(numCoeffs, 0.0);

    for (auto &p : points)
    {
        // row = [1, x, x^2, ..., x^polyDegree]
        std::vector<double> row(numCoeffs, 0.0);
        double powX = 1.0;
        for (int c = 0; c < numCoeffs; ++c)
        {
            row[c] = powX;
            powX *= p.x;
        }
        // Accumulate
        for (int i = 0; i < numCoeffs; i++)
        {
            for (int j = 0; j < numCoeffs; j++)
            {
                XtX[i][j] += row[i] * row[j];
            }
            XtY[i] += row[i] * p.y;
        }
    }

    // Solve the linear system
    bool ok = solveLinearSystem(XtX, XtY);
    if (!ok)
        throw std::runtime_error("Polynomial fit failed. System could not be solved.");

    // XtY now holds the solution vector of coefficients
    return XtY;
}

// ----------------------------------------------------------
// Robust polynomial fit with M iterations of outlier removal
// - points: the full set of data points
// - polyDegree: polynomial degree to fit
// - outlierFraction: fraction of outliers to remove each iteration (0.0 - 1.0)
// - M: number of iterations
// ----------------------------------------------------------
std::vector<double> robustPolynomialFit(
    std::vector<cv::Point2f> points,
    int polyDegree,
    double outlierFraction,
    int M)
{
    // Basic checks
    if (points.empty())
        throw std::runtime_error("No points provided for fitting.");
    if (outlierFraction < 0.0 || outlierFraction >= 1.0)
        throw std::runtime_error("outlierFraction must be in [0, 1).");
    if (M <= 0)
        throw std::runtime_error("Number of iterations M must be positive.");

    std::vector<cv::Point2f> inlierPoints = points;
    std::vector<double> coeffs;

    for (int iteration = 0; iteration < M; iteration++)
    {
        coeffs = fitPolynomial(inlierPoints, polyDegree);

        // Compute and store residuals for all points
        std::vector<std::pair<double, int>> residuals;
        residuals.reserve(inlierPoints.size());

        for (int i = 0; i < (int)inlierPoints.size(); i++)
        {
            double yPred = evaluatePolynomial(coeffs, inlierPoints[i].x);
            double r = std::fabs(inlierPoints[i].y - yPred);
            residuals.push_back({r, i});
        }

        std::sort(residuals.begin(), residuals.end(),
                  [](auto &a, auto &b){ return a.first > b.first; });

        int numToRemove = static_cast<int>(outlierFraction * inlierPoints.size());
        if (inlierPoints.size() - numToRemove < static_cast<size_t>(polyDegree + 1))
            numToRemove = static_cast<int>(inlierPoints.size()) - (polyDegree + 1);

        if (numToRemove <= 0) break;

        // Print outlier information
        std::cout << "\n=== Iteration " << iteration + 1 << " Outliers ===" << std::endl;
        for (int i = 0; i < numToRemove; i++)
        {
            int idx = residuals[i].second;
            double yPred = evaluatePolynomial(coeffs, inlierPoints[idx].x);
            std::cout << "Outlier: ("
                      << inlierPoints[idx].x << ", " 
                      << inlierPoints[idx].y << ") "
                      << "Predicted y: " << yPred
                      << " Residual: " << residuals[i].first 
                      << std::endl;
        }

        // Mark and remove outliers (existing logic)
        std::vector<bool> isOutlier(inlierPoints.size(), false);
        for (int i = 0; i < numToRemove; i++)
        {
            int idx = residuals[i].second;
            isOutlier[idx] = true;
        }

        std::vector<cv::Point2f> newInliers;
        newInliers.reserve(inlierPoints.size() - numToRemove);
        for (int i = 0; i < (int)inlierPoints.size(); i++)
        {
            if (!isOutlier[i])
                newInliers.push_back(inlierPoints[i]);
        }

        inlierPoints = std::move(newInliers);

        if (inlierPoints.size() < static_cast<size_t>(polyDegree + 1))
            break;
    }


    // Final fit
    if (!inlierPoints.empty() && inlierPoints.size() >= static_cast<size_t>(polyDegree + 1))
    {
        coeffs = fitPolynomial(inlierPoints, polyDegree);
    }


    float avg_residual=0;
    // Print final inliers
    std::cout << "\n=== Final Inliers ===" << std::endl;
    for (const auto& point : inlierPoints)
    {
        double yPred = evaluatePolynomial(coeffs, point.x);
        std::cout << "Inlier: ("
                  << point.x << ", " 
                  << point.y << ") "
                  << "Predicted y: " << yPred
                  << " Residual: " << std::fabs(point.y - yPred)
                  << std::endl;

        avg_residual+=std::fabs(point.y - yPred);
    }
    avg_residual/=inlierPoints.size();
    printf("avg_residual=%0.3f\n",avg_residual);

    float allowed_residual=avg_residual*10;
    {//put outlier points back if residual is within allowed_residual
        inlierPoints.clear();
        for(const auto& p : points)
        {
            double yPred = evaluatePolynomial(coeffs, p.x);
            if(std::fabs(p.y - yPred)<allowed_residual)
            {
                inlierPoints.push_back(p);
            }
            else
            {
                std::cout << "Outlier: (" << p.x << ", " << p.y << ") " << "Predicted y: " << yPred << " Residual: " << std::fabs(p.y - yPred) << std::endl;
            }
        }

      
    }

    if (!inlierPoints.empty() && inlierPoints.size() >= static_cast<size_t>(polyDegree + 1))
    {
        coeffs = fitPolynomial(inlierPoints, polyDegree);
    }



    std::cout << "\n=== Final Final Inliers ===" << std::endl;
    for (const auto& point : inlierPoints)
    {
        double yPred = evaluatePolynomial(coeffs, point.x);
        std::cout << "Inlier: ("
                  << point.x << ", " 
                  << point.y << ") "
                  << "Predicted y: " << yPred
                  << " Residual: " << std::fabs(point.y - yPred)
                  << std::endl;

    }

    return coeffs;
}

float randGen(float min, float max)
{
    float rand_val=(rand()%1000000)/1000000.0;//0~1
    return min + rand_val*(max - min);
}

// ----------------------------------------------------------
// Demonstration
// ----------------------------------------------------------
int mainD()
{
    // Some example data points (mostly following y = x^2)
    std::vector<cv::Point2f> allPoints;

    for(int i=0;i<100;i++)
    {
        float noise=randGen(-1,1);
        if(i%6==0)
        {
            noise=randGen(-100,100);
        }

        float x=(float)i/10.0;
        float y=(float)(x*x+noise);
        allPoints.push_back({x, y});
    }

    for(const auto& p : allPoints)
    {
        printf("(%0.3f, %0.3f)\n", p.x, p.y);
    }

    // Polynomial degree
    int polyDegree = 2;

    // Fraction of outliers to remove each iteration
    double outlierFraction = 0.20; // 20%

    // Number of robust-fit iterations
    int M = 3;

    // Perform robust fitting
    std::vector<double> finalCoeffs = robustPolynomialFit(allPoints, polyDegree, outlierFraction, M);

    // Print results
    std::cout << "Robust polynomial coefficients after " << M << " iterations:\n";
    for (size_t i = 0; i < finalCoeffs.size(); i++)
    {
        std::cout << "a" << i << " = " << finalCoeffs[i] << "\n";
    }

    // for(const auto& p : allPoints)
    // {
    //     double testY = evaluatePolynomial(finalCoeffs, p.x);
    //     printf("(%0.3f: %0.3f-> %0.3f, diff=%0.3f)\n", p.x,p.y,testY , p.y-testY);
    // }

    return 0;
}




int main_testcode2(int argc, char** argv)
{
    // Simple usage check
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " <template_image> <test_image>" << std::endl;
        return -1;
    }

    // Read images
    cv::Mat templColor = cv::imread(argv[1], cv::IMREAD_COLOR);
    cv::Mat testColor  = cv::imread(argv[2], cv::IMREAD_COLOR);

    if (templColor.empty() || testColor.empty())
    {
        std::cerr << "Error reading input images." << std::endl;
        return -1;
    }

    // Convert to grayscale (or other formats if needed)
    cv::Mat templGray, testGray;
    cv::cvtColor(templColor, templGray, cv::COLOR_BGR2GRAY);
    cv::cvtColor(testColor, testGray, cv::COLOR_BGR2GRAY);

    // Prepare LINEMOD detector
    // By default, we can use two modalities: gradient (MOD_GRADIENT) and color (MOD_COLOR).
    // In many cases, only the gradient modality is used, but let's do both here.
    std::vector<cv::Ptr<cv::linemod::Modality>> modalities;
    modalities.push_back(cv::linemod::Modality::create("Gradient"));
    modalities.push_back(cv::linemod::Modality::create("RGB"));
    
    // Create the LINEMOD detector
    cv::Ptr<cv::linemod::Detector> detector =cv::makePtr<cv::linemod::Detector>();

    // Build sources for the template image
    std::vector<cv::Mat> templSources;
    templSources.push_back(templGray);
    templSources.push_back(templColor);

    // Add template to the detector
    std::string templateName = "myObject";
    cv::Mat mask = cv::Mat(); // Empty mask means use the entire template
    int templateId = detector->addTemplate(templSources, templateName, mask);

    if (templateId < 0) {
        std::cerr << "Failed to add template to LINEMOD detector." << std::endl;
        return -1;
    }
    std::cout << "Added LINEMOD template with ID = " << templateId << std::endl;

    // Now prepare sources for the test image
    std::vector<cv::Mat> testSources;
    testSources.push_back(testGray);
    testSources.push_back(testColor);

    // Match
    std::vector<cv::linemod::Match> matches;
    detector->match(testSources, 80.0f, matches);

    std::cout << "Number of matches found: " << matches.size() << std::endl;
    for (size_t i = 0; i < matches.size(); ++i)
    {
        const cv::linemod::Match& m = matches[i];
        std::cout << "Match[" << i << "]: x=" << m.x << ", y=" << m.y 
                  << ", similarity=" << m.similarity << ", template_id=" << m.template_id 
                  << ", class_id=" << m.class_id << std::endl;
    }

    // Visualize the best match if any
    if (!matches.empty())
    {
        cv::linemod::Match bestMatch = matches[0];
        // Retrieve template info to get the bounding box or template region
        const std::vector<cv::linemod::Template>& templates = detector->getTemplates("myObject", bestMatch.template_id);
        
        // Each modality has its own set of features, but let's just take bounding box extremes
        int offsetX = 0, offsetY = 0;
        int min_x = INT_MAX, max_x = INT_MIN;
        int min_y = INT_MAX, max_y = INT_MIN;

        for (size_t m = 0; m < templates.size(); ++m)
        {
            for (size_t f = 0; f < templates[m].features.size(); ++f)
            {
                cv::linemod::Feature feat = templates[m].features[f];
                // The feature x,y are offsets within the template
                min_x = std::min(min_x, feat.x);
                max_x = std::max(max_x, feat.x);
                min_y = std::min(min_y, feat.y);
                max_y = std::max(max_y, feat.y);
            }
        }

        // Draw rectangle around detected area
        // The top-left corner in the test image is (bestMatch.x, bestMatch.y)
        // So we add the feature bounding box offsets to get a bounding region.
        cv::Rect detectedRoi(
            bestMatch.x + min_x, 
            bestMatch.y + min_y, 
            (max_x - min_x), 
            (max_y - min_y)
        );

        // Make sure ROI is valid within the image
        detectedRoi &= cv::Rect(0, 0, testColor.cols, testColor.rows);

        cv::rectangle(testColor, detectedRoi, cv::Scalar(0, 255, 0), 2);
        cv::putText(testColor, "LINEMOD Match", 
                    cv::Point(detectedRoi.x, detectedRoi.y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

        // Show the result
        cv::imshow("LINEMOD Detection", testColor);
        cv::waitKey(0);
    }
    else
    {
        std::cout << "No matches found above the given threshold." << std::endl;
    }

    return 0;
}