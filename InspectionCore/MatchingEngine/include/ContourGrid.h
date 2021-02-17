#ifndef CONTOURGRID_HPP
#define CONTOURGRID_HPP

#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>





class ContourFetch{
    public:
    typedef struct ptInfo
    {
      acv_XY pt;
      acv_XY sobel;
      acv_XY contourDir;
      
      acv_XY pt_img;
      float curvature;//The angle
      float edgeRsp;
      float tmp;
    };

    
    class contourMatchSec
    {
      public:
      std::vector< ptInfo > section;
      float sigma;
      float dist;
      int contourIdx;
    };

    std::vector< ptInfo > tmpXYSeq;
    private:

    public:
    std::vector< std::vector <ptInfo> > contourSections;
    ContourFetch();
    void RESET();
    void push(int group,ptInfo data);

    int dataSize();

    const ptInfo* get(int idx);

    void getContourPointsWithInCircleContour(float X,float Y,float radius,float sAngle,float eAngle,float outter_inner,
      float epsilon,std::vector<contourMatchSec> &m_sec);

    void getContourPointsWithInLineContour(acv_Line line, float epsilonX, float epsilonY,float flip_f,
      std::vector<contourMatchSec> &m_sec,float lineCurvatureMax = 0.15,float cosSim=0.9);

};



class ContourGrid:ContourFetch{
    public:
    
    typedef struct ptInfo
    {
      acv_XY pt;
      acv_XY sobel;
      acv_XY contourDir;
      float curvature;
      float edgeRsp;
      float tmp;
    };
    std::vector< ptInfo > tmpXYSeq;
    private:

    std::vector< std::vector <ptInfo> > contourSections;
    std::vector< int > intersectTestNodes;
    int gridSize;
    int dataNumber=0;
    int sectionCol;
    int sectionRow;
    int secROI_X, secROI_Y, secROI_W, secROI_H;
    public:
    ContourGrid();
    ContourGrid(int grid_size,int img_width,int img_height);

    void RESET(int grid_size,int img_width,int img_height);

    void setSecROI(int secROI_X,int secROI_Y,int secROI_W,int secROI_H);

    void resetSecROI();

    int getColumSize();

    
    void ptInfoOpt(acvImage *img);

    int getRowSize();

    int getSecIdx(int X,int Y);

    std::vector<ptInfo> &fetchBelongingSection(acv_XY data);

    void push(ptInfo data);

    int dataSize();

    const ptInfo* get(int idx);

    enum intersectTestType
    {
      intersectTestType_outer=0,
      intersectTestType_middle,
      intersectTestType_inner,
    };

    void GetSectionsWithinCircleContour(float X,float Y,float radius,float epsilon,
      std::vector<int> &intersectIdxs);

    void getContourPointsWithInCircleContour(float X,float Y,float radius,float sAngle,float eAngle,float outter_inner,
      float epsilon,
      std::vector<int> &intersectIdxs,std::vector<ptInfo> &points);

    void GetSectionsWithinLineContour(acv_Line line,float epsilonX, float epsilonY,std::vector<int> &intersectIdxs);

    void getContourPointsWithInLineContour(acv_Line line, float epsilonX, float epsilonY,float flip_f, std::vector<int> &intersectIdxs,std::vector<ptInfo> &points,float lineCurvatureMax = 0.15);

    int getGetSectionRegionDataSize(int secX,int secY,int secW,int secH);
    const acv_XY* getGetSectionRegionData(int secX,int secY,int secW,int secH,int dataIdx);
};


#endif
