
#ifndef ACV_IMG_COMP_LABEL_H
#define ACV_IMG_COMP_LABEL_H
#include "acvImage_BasicTool.hpp"
#include<vector>

//Num  0000_0000       0000_0000       0000_0000
//Num0 0000_0000  Num1 0000_0000  Num2 0000_0000


typedef struct acv_LabeledData
{
    acv_XY LTBound;
    acv_XY RBBound;
    acv_XY Center;
    int area;
    int misc;
} acv_LabeledData;


typedef struct _3BYTE
{
    unsigned Num:24;
} _3BYTE;
typedef struct _2BYTE
{
    uint16_t Num;
    uint8_t Empty;
} _2BYTE;
typedef struct BYTE3
{
    uint8_t Num2;
    uint8_t Num1;
    uint8_t Num0;
} BYTE3;
typedef union _24BitUnion
{
    BYTE3 Byte3;
    _3BYTE _3Byte;
    _2BYTE _2Byte;
} _24BitUnion;

BYTE* acvContourWalk(acvImage  *Pic,int *X_io,int *Y_io,int *dir_io,int dirinc);
void acvComponentLabeling(acvImage *Pic);
int acvLabeledRegionInfo(acvImage  *LabeledPic,std::vector<acv_LabeledData> *list);
int acvRemoveRegionLessThan(acvImage  *LabeledPic,std::vector<acv_LabeledData> *list,int threshold);
void acvLabeledColorDispersion(acvImage *ColorDispersionPic,acvImage *LabeledPic,int ColorNum);//0


#endif
