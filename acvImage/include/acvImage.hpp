#ifndef ACV_IMG_H
#define ACV_IMG_H

#define _USE_MATH_DEFINES
#include <cmath>

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define S_RGB 0
#define S_HSV 1




#define HSV_V  0
#define HSV_S  1
#define HSV_H  2
#define HSV_HMax 251
#define HSV_SMax 255
#define HSV_VMax 255


#define RGB_B  0
#define RGB_G  1
#define RGB_R  2

#define NCC_g  1
#define NCC_r  2
typedef uint8_t BYTE;




class acvImage
{

private:
    unsigned char ColorType;
    BYTE*   ImLine;
    int ROIWidth,ROIHeight,RealWidth,RealHeight;
    int ROIOffsetX,ROIOffsetY;
public :
    int Channel;
    BYTE    *ImageData;
    BYTE    **CVector;
    acvImage();
    acvImage(int SetWidth,int SetHeight,int SetChannel);
    void VarInit(void);

    void ReSize(int SetWidth,int SetHeight);
    void SetROI(int SetOffsetX,int SetOffsetY,int SetWidth,int SetHeight);
    void ReSetROI();

    void ChannelOffset(int offset);
    void FreeImage();
    ~acvImage();
private:

    void RESIZE(int SetWidth,int SetHeight);

public:
    void YUY2ToYUV();
    void YUY2ToRGB();
    void YUY2ToGray();



    void RGBToHSV();

    void HSVToRGB();

    void RGBToGray();

    void RGBToEvenGray();
#define InDataR  InData[2]
#define InDataG  InData[1]
#define InDataB  InData[0]

    static void HSVFromRGB(BYTE* OutData,BYTE* InData);

#define InDataV  InData[0]
#define InDataS  InData[1]
#define InDataH  InData[2]
    /*H 0~251 S 0~255 V 0~255 ==> RGB 0~255

     i =H/42        (int)    // sector 0 to 5
     f =H-i*42      (int)    // factorial*42 to int

     p=V*(255-S)            /255
     q=V*(255*42-S*(f)  )   /255/42
     t=V*(255*42-S*(42-f))  /255/42*/
    static void RGBFromHSV(BYTE* OutData,BYTE* InData);
    int GetWidth();
    int GetHeight();

    int GetROIOffsetX();
    int GetROIOffsetY();
    int GetRealWidth();
    int GetRealHeight();


    unsigned char GetColorType();
};


#endif
