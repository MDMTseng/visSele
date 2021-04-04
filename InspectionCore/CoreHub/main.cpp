#include <stdio.h>
#include <unistd.h>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"
#include "acvImage_BasicTool.hpp"
#include "SBM_if.hpp"
#include <UTIL.hpp>

using namespace std;
using namespace cv;
SBM_if sbmif;

float randomGen(float from=0,float to=1)
{
  float r01=(rand()%1000000)/1000000.0;
  return r01*(to-from)+from;
}

int_fast32_t testPolyFit()
{
  srand(time(NULL));
  // These inputs should result in the following approximate coefficients:
  //         0.5           2.5           1.0        3.0
  //    y = (0.5 * x^3) + (2.5 * x^2) + (1.0 * x) + 3.0
  const int dataL=100;
  struct DATA_XY
  {
    float x;
    float y;
  }Data[dataL];


  float coeff[]={1.0001,2.5,-3,4,5};
  const int order = sizeof(coeff)/sizeof(coeff[0])-1;
  for(int i=0;i<dataL;i++)
  {
    Data[i].x=(i-dataL/2)*0.1;
    Data[i].y=polycalc(Data[i].x, coeff,order+1);//+randomGen(-1,1);
  }

  float resCoeff[order+1]={0}; // resulting array of coefs

  // Perform the polyfit
  int result = polyfit(&(Data[0].x),
                       &(Data[0].y),
                       NULL,
                       dataL,
                       order,
                       resCoeff,
                       sizeof(Data[0]),
                       sizeof(Data[0])
                       );




  printf("Original coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",coeff[order-i]);
  printf("\nNew coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",resCoeff[order-i]);
  printf("\n===========\n");

  
  for(int i=0;i<dataL;i++)
  {
    float pred_yData=polycalc(Data[i].x, resCoeff,order+1);

    LOGI("[%d]: x:%.5f   y:%.5f   _y:%.5f  diff:%.5f",i,Data[i].x,Data[i].y,pred_yData,pred_yData-Data[i].y);
  }

  return 0;
}

int testDistance()
{
  float errorSum=0;
  for(int i=0;i<100;i++)
  {
    
    acv_Line line={
      .line_vec=(acv_XY){randomGen(-100,100),randomGen(-100,100)},
      .line_anchor=(acv_XY){randomGen(-100,100),randomGen(-100,100)},
    };

    float tarDist=randomGen(-100,100);
    float tarSlide=randomGen(-100,100);

    acv_XY vec = acvVecNormalize(line.line_vec);
    acv_XY nvec =acvVecNormal(vec);

    acv_XY pt1=acvVecAdd(acvVecAdd(line.line_anchor,acvVecMult(vec,tarSlide)),acvVecMult(nvec,tarDist));

    float dist =acvDistance_Signed(line, pt1);
    errorSum+=abs(dist-tarDist);
    LOGI("Test[%d]: tarDist:%f dist:%f diff:%f",i,tarDist,dist,dist-tarDist);
  }
  
  LOGI("errorSum:%f",errorSum);
  return 0;
}



static std::string prefix = "./";

void sdsd()
{
    // std::vector<std::string> ids;
    // string class_id = "test";
    // ids.push_back(class_id);

    SBM_if sbmif;
    {
        Mat img = imread(prefix+"case1/train.png");
        assert(!img.empty() && "check your img path");
        sbmif.train(img,1);
    }

    Mat test_img = imread(prefix+"case1/test.png", cv::IMREAD_GRAYSCALE);
    assert(!test_img.empty() && "check your img path");

    Timer timer;
    auto matches = sbmif.test(test_img);
    for(int i=1;i<10;i++)
    {
      auto matches_ = sbmif.test(test_img);
    }
    timer.out("MATCH::====================");

    if(test_img.channels() == 1) cvtColor(test_img, test_img, COLOR_GRAY2BGR);

    std::cout << "matches.size(): " << matches.size() << std::endl;
    size_t top5 = 500;
    if(top5>matches.size()) top5=matches.size();

    vector<Rect> boxes;
    vector<float> scores;
    vector<int> idxs;
    for(auto match: matches){
        Rect box;
        box.x = match.x;
        box.y = match.y;
        
        // printf("template_id:%d\n",match.template_id);
        auto templ = sbmif.detector.getTemplates(match.class_id,
                                            match.template_id);

        box.width = templ[0].width;
        box.height = templ[0].height;
        boxes.push_back(box);
        scores.push_back(match.similarity);
    }
    cv_dnn::NMSBoxes(boxes, scores, 0, 0.5f, idxs);

    for(auto idx: idxs){
    //for(int idx=0;idx<matches.size(); idx++){
        auto match = matches[idx];

        auto templ = sbmif.detector.getTemplates(match.class_id,
                                        match.template_id);
        int x =  templ[0].width + match.x;
        int y = templ[0].height + match.y;
        int r = templ[0].width/2;


        cv::Vec3b randColor;
        randColor[0] = rand()%(255/3) + 255/3;
        randColor[1] = rand()%(255/3) + 255/3; 
        randColor[2] = rand()%(255/3) + 255/3;

        for(int i=0; i<templ[0].features.size(); i++){
            auto feat = templ[0].features[i];
            cv::circle(test_img, {feat.x+match.x, feat.y+match.y}, 5, randColor, -1);
        }

        cv::putText(test_img, to_string(int(round(match.similarity))),
                    Point(match.x+r-10, match.y-3), FONT_HERSHEY_PLAIN, 2, randColor);

        printf("xy:%d,%d  wh:%d,%d\n",x,y,templ[0].width,templ[0].height);
        cv::RotatedRect rotatedRectangle(
          {(float)(x+match.x)/2, (float)(y+match.y)/2}, 
          {(float)templ[0].width, (float)templ[0].height}, 
          -templ[0].angle);

        cv::Point2f vertices[4];
        rotatedRectangle.points(vertices);
        for(int i=0; i<4; i++){
            int next = (i+1)%4;
            cv::line(test_img, vertices[i], vertices[next], randColor, 2);
        }





        std::cout << "\nmatch.template_id: " << match.template_id << std::endl;
        std::cout << "match.similarity: " << match.similarity << std::endl;
    }

    imshow("img", test_img);
    waitKey(0);

    std::cout << "test end" << std::endl << std::endl;

}



int main(int argc, char **argv)
{
  // return testDistance();
  // return testPolyFit();
  // tmpMain();
  // printf(">>>");
  return cp_main(argc, argv);
}

