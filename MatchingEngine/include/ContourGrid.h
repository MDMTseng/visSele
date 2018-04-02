#ifndef CONTOURGRID_HPP
#define CONTOURGRID_HPP

#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>


class ContourGrid{
    std::vector< std::vector <acv_XY> > contourSections;
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

    int getRowSize();

    int getSecIdx(int X,int Y);

    std::vector<acv_XY> &fetchBelongingSection(acv_XY data);

    void push(acv_XY data);

    int dataSize();

    const acv_XY* get(int idx);

    enum intersectTestType
    {
      intersectTestType_outer=0,
      intersectTestType_middle,
      intersectTestType_inner,
    };

    void GetSectionsWithinCircleContour(float X,float Y,float radius,float epsilon,
      std::vector<int> &intersectIdxs);

    void getContourPointsWithInCircleContour(float X,float Y,float radius,float epsilon,
      std::vector<int> &intersectIdxs,std::vector<acv_XY> &points);

    void GetSectionsWithinLineContour(acv_Line line,float epsilon,std::vector<int> &intersectIdxs);

    void getContourPointsWithInLineContour(acv_Line line,float epsilon,std::vector<int> &intersectIdxs,std::vector<acv_XY> &points);

    int getGetSectionRegionDataSize(int secX,int secY,int secW,int secH);
    const acv_XY* getGetSectionRegionData(int secX,int secY,int secW,int secH,int dataIdx);
};


#endif
