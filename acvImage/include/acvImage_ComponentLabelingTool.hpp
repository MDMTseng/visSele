
#ifndef ACV_IMG_COMP_LABEL_H
#define ACV_IMG_COMP_LABEL_H
#include "acvImage_BasicTool.hpp"
#include<vector>

//Num  0000_0000       0000_0000       0000_0000
//Num0 0000_0000  Num1 0000_0000  Num2 0000_0000

typedef struct acv_XY
{
  float X,Y;
}acv_XY;

typedef struct acv_LabeledData
{
  acv_XY LTBound;
  acv_XY RBBound;
  acv_XY Center;
  int area;
  int misc;
}acv_LabeledData;

void acvComponentLabeling(acvImage *Pic);
int acvLabeledRegionExtraction(acvImage  *LabeledPic,std::vector<acv_LabeledData> *list);
int acvRemoveRegionLessThan(acvImage  *LabeledPic,std::vector<acv_LabeledData> *list,int threshold);
void acvLabeledColorDispersion(acvImage *ColorDispersionPic,acvImage *LabeledPic,int ColorNum);//0


#endif
