
#ifndef ACV_IMG_COMP_LABEL_H
#define ACV_IMG_COMP_LABEL_H
#include "acvImage_BasicTool.hpp"

typedef struct _3BYTE
{
        unsigned Num:24;
        unsigned Empty:8;
}_3BYTE;
typedef struct BYTE3
{
        unsigned Num2:8;
        unsigned Num1:8;
        unsigned Num0:8;
        unsigned Empty:8;
}BYTE3;
typedef union _24BitUnion
{
        BYTE3 Byte3;
        _3BYTE _3Byte;
}_24BitUnion;
//Num  0000_0000       0000_0000       0000_0000
//Num0 0000_0000  Num1 0000_0000  Num2 0000_0000

/*
void acvLabeledInfoAdjust(DyArray<int> * LabelInfo);//
void acvRecordLabel(BYTE *BGR,_24BitUnion NowLable,int X,int Y,int *TableData);
void acvCheck4Connect2(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * CoorTable);
void acvCheck8Connect2(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * CoorTable);
void acvComponentLabeling2(acvImage *Pic,DyArray<int>* Information);
void acvCheck4Connect(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * ProcTable);
void acvCheck8Connect(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int>* ProcTable);
void acvComponentLabeling(acvImage *Pic,DyArray<int> * Information); //0
void acvComponentLabeling_CentroidMuti(acvImage *Pic,DyArray<int> * Information); //0
void acvLabeledSignature(acvImage *LabeledPic,DyArray<int> * LabelInfo,DyArray<int> * SignInfo);
void acvLabeledHistoGenerater(acvImage *LabeledPic,acvImage *RefPic,DyArray<int> * LabelInfo,DyArray<int> * HistoInfo);
void acvWeightedCentroid(acvImage *LabeledPic,DyArray<int> * Information,acvImage *WeightPic);
int acvThresholdArea2(acvImage *LabeledPic,DyArray<int> * Information,int Valve);
int acvThresholdArea(acvImage *LabeledPic,DyArray<int> * Information,int Valve); //0
*/


void acvLabeledColorDispersion(acvImage *ColorDispersionPic,acvImage *LabeledPic,int ColorNum);//0
#endif
