#ifndef MORPHENGINE_HPP
#define MORPHENGINE_HPP

#include "acvImage_ToolBox.hpp"

#include "acvImage_BasicDrawTool.hpp"
#include <cstdlib>
#include <unistd.h>


class MorphEngine{

    int sectionCol;
    int sectionRow;

    typedef struct {
      acv_XY buf_node;
      acv_XY v;

    }morphOptimizer;
    std::vector< morphOptimizer > morphNodes_optimizer;
    float gridSize;
    public:
    std::vector< acv_XY > morphNodes;
    void RESET(int grid_size,int img_width,int img_height);
    int getSecIdx(acv_XY from);

    int Mapping_adjust(acv_XY pt, acv_XY vec, float *distGainTbl,const int TblL);
    int Mapping(acv_XY from,acv_XY *ret_to);
    int regularization(float alpha);
    int reset_optimization();
    int optimization(float alpha);
    int grid_adjust(int X,int Y, acv_XY vec);
    int Mapping_adjust_Global(acv_XY offset);
    int Mapping_adjust(acv_XY pt, acv_XY vec);
};


#endif
