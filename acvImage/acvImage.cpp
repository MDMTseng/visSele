
#include "acvImage.hpp"
#include <stddef.h>
acvImage::acvImage()
{
    VarInit();
}
acvImage::acvImage(int SetWidth,int SetHeight,int SetChannel)
{
    VarInit();
    Channel=SetChannel;
    RESIZE(SetWidth,SetHeight);
}
void acvImage::VarInit(void)
{
    CVector=NULL;
    bufferPtr = NULL;
    ImageData=NULL;
    ColorType=S_RGB;
    ROIWidth=ROIHeight
             =RealWidth=RealHeight=
                            ROIOffsetX=ROIOffsetY=0;
    Channel=3;
}

void acvImage::ReSize(int SetWidth,int SetHeight)
{
    RESIZE(SetWidth,SetHeight);
}
int acvImage::SetROI(int SetOffsetX,int SetOffsetY,int SetWidth,int SetHeight)
{
    //if(!Bitmap)Bitmap->FreeImage();
    //RESIZE(SetWidth,SetHeight);
    if(SetOffsetX+SetWidth>RealWidth||SetOffsetY+SetHeight>RealHeight)
    {
        //ReSize(ROIOffsetX+SetWidth,ROIOffsetY+SetHeight);
        return -1;
    }

    ROIWidth=SetWidth;
    ROIHeight=SetHeight;
    ROIOffsetX=SetOffsetX;
    ROIOffsetY=SetOffsetY;

    return 0;
}
void acvImage::ReSetROI()
{
    ROIWidth=RealWidth;
    ROIHeight=RealHeight;
    ROIOffsetX=0;
    ROIOffsetY=0;


}

void acvImage::FreeImage()
{
    if( isBufferInternal && bufferPtr!=NULL )
    {
        delete(bufferPtr);
    }

    if(CVector!=NULL)
    {
        delete(CVector);
    }

    ImageData=NULL;
    CVector=NULL;
    bufferPtr=NULL;

    bufferDataLength =
    cVecLength =
    ROIWidth=RealWidth=
    ROIHeight=RealHeight=
    ROIOffsetX=
    ROIOffsetY=0;
    
    isBufferInternal=true;

}
acvImage::~acvImage()
{
    FreeImage();
}
void acvImage::RESIZE(int SetWidth,int SetHeight)
{
    if(RealWidth==SetWidth && RealHeight==SetHeight)return;
    
    ResetChannelOffset();
    
    ReSetROI();

    
    //Check if we have enough sapce
    if(bufferDataLength<SetWidth*SetHeight*Channel)
    {//If not, claim a new space
        
        FreeImage();
        bufferDataLength = SetWidth*SetHeight*Channel;

        bufferPtr = 
        ImageData=new BYTE[bufferDataLength];
        isBufferInternal=true;
    }

    
    //Need to reflow the pixel 4X4 => 5X3 ~1
    //RealHeight=bufferDataLength/Channel/RealWidth;//There might be some residual pixels.
    if(SetHeight>cVecLength)
    {
        if(CVector!=NULL)
        {
            delete(CVector);
        }
        cVecLength = SetHeight;
        CVector=new BYTE* [cVecLength];
    }

    RealWidth=SetWidth;
    RealHeight=SetHeight;
    ChannelOffset(0);
    ReSetROI();

}


int acvImage::useExtBuffer(BYTE *extBuffer,int extBufferLen,int SetWidth,int SetHeight)
{
    if(extBufferLen<SetWidth*SetHeight*Channel)return -1;
    
    FreeImage();
    ImageData = bufferPtr = extBuffer;
    bufferDataLength=extBufferLen;
    isBufferInternal=false;
    
    RESIZE(SetWidth,SetHeight);
    
    return 0;
}

void acvImage::ChannelOffset(int offset)
{
    ImageData+=offset;
    CVector[0]=ImageData;
    for(int i=1; i<RealHeight; i++)
    {
        CVector[i]=CVector[i-1]+RealWidth*Channel;
    }

}
void acvImage::ResetChannelOffset()
{
    if(ImageData==bufferPtr)return;
    ImageData = bufferPtr;
    ChannelOffset(0);
}


void acvImage::YUY2ToYUV()
{
    int i,j;
    int hWidth=ROIWidth>>1;

    for(i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(j=ROIOffsetX; j<hWidth; j++)
        {
            ImLine[5]=ImLine[1];
            ImLine[2]=ImLine[4];
            ImLine+=6;

        }
    }

}
void acvImage::YUY2ToRGB()
{
    int i,j,RUV,GUV,BUV,TmpR,TmpG,TmpB;
    BYTE Y1,Y2;
    int U,V;
    int hWidth=ROIHeight>>1;

    for(i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(j=ROIOffsetX; j<hWidth; j++)
        {
            Y1=ImLine[0];

            U =(int)ImLine[1]-128;

            Y2=ImLine[3];

            V =(int)ImLine[4]-128;

            /*
            RUV=V*13/10;
            GUV=-(U*3+V*7)/10;
            BUV=U*17/10; */
            RUV=V*1.370705;
            GUV=-(U*0.337633+V*0.698);
            BUV=U*1.732446;


            TmpR=Y1+RUV;
            TmpG=Y1+GUV;
            TmpB=Y1+BUV;

            if(TmpR&0x800)TmpR=0;
            else if(TmpR&0x300)TmpR=0xff;

            if(TmpG&0x800)TmpG=0;
            else if(TmpG&0x300)TmpG=0xff;

            if(TmpB&0x800)TmpB=0;
            else if(TmpB&0x300)TmpB=0xff;



            *(ImLine++)=TmpB;
            *(ImLine++)=TmpG;
            *(ImLine++)=TmpR;

            TmpR=Y2+RUV;
            TmpG=Y2+GUV;
            TmpB=Y2+BUV;

            if(TmpR&0x800)TmpR=0;
            else if(TmpR&0x300)TmpR=0xff;

            if(TmpG&0x800)TmpG=0;
            else if(TmpG&0x300)TmpG=0xff;

            if(TmpB&0x800)TmpB=0;
            else if(TmpB&0x300)TmpB=0xff;


            *(ImLine++)=TmpB;
            *(ImLine++)=TmpG;
            *(ImLine++)=TmpR;
        }
    }
}
void acvImage::YUY2ToGray()
{
    for(int i=ROIOffsetY; i<ROIHeight; i++)
    {   ImLine=CVector[i];
        for(int j=ROIOffsetX; j<ROIWidth; j++)
        {
            ImLine[1]=ImLine[2]=ImLine[0];
            ImLine+=3;
        }
    }
}



void acvImage::RGBToHSV()
{
    ColorType=S_HSV;
    for(int i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(int j=ROIOffsetX; j<ROIWidth; j++)
        {
            HSVFromRGB(ImLine,ImLine);
            ImLine+=3;
        }
    }
}

void acvImage::HSVToRGB()
{
    ColorType=S_RGB;
    for(int i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(int j=ROIOffsetX; j<ROIWidth; j++)
        {
            RGBFromHSV(ImLine,ImLine);
            ImLine+=3;
        }
    }
}

void acvImage::RGBToGray()
{
    for(int i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(int j=ROIOffsetX; j<ROIWidth; j++)
        {

            ImLine[0]=ImLine[1]=ImLine[2]=
                                    (ImLine[0]+2*ImLine[1]+ImLine[2])/4;
            ImLine+=3;
        }
    }
}

void acvImage::RGBToEvenGray()
{
    for(int i=ROIOffsetY; i<ROIHeight; i++)
    {
        ImLine=CVector[i];
        for(int j=ROIOffsetX; j<ROIWidth; j++)
        {
            ImLine[0]=ImLine[1]=ImLine[2]=
                                    (ImLine[0]+ImLine[1]+ImLine[2])/3;
            ImLine+=3;
        }
    }
}


#define InDataR  InData[2]
#define InDataG  InData[1]
#define InDataB  InData[0]

void acvImage::HSVFromRGB(BYTE* OutData,BYTE* InData)
{
    //0 V ~255
    //1 S ~255
    //2 H ~251
    BYTE Mod,Max,Min,D1,D2;
    Max=Min=InDataR;
    D1=InDataG;
    D2=InDataB;
    Mod=6;
    if(InDataG>Max)
    {
        Max=InDataG;
        Mod=2;       //
        D1=InDataB;
        D2=InDataR;
    }
    else
    {
        Min=InDataG;

    }

    if(InDataB>Max)
    {
        Max=InDataB;
        Mod=4;
        D1=InDataR;
        D2=InDataG;
    }
    else if(InDataB<Min)
    {
        Min=InDataB;

    }

    OutData[0]=Max;
    if(Max==0)
    {
        OutData[1]=
            OutData[2]=0;
        goto Exit;
    }
    else
        OutData[1]=255-Min*255/Max;
    Max-=Min;
    if(Max)
    {
        OutData[2]=(Mod*(Max)+D1-D2)*42/(Max);
        if(OutData[2]<42||OutData[2]>=252)OutData[2]+=4;
    }
    else
        OutData[2]=0;
Exit:
    ;
}

/*

 H 0~360 S 0~1 V 0~1   ==> RGB 0~1
 Hi=H/60       (float)

 i =floor(Hi)  (int)      // sector 0 to 5
 f =Hi-i        (float)    // factorial part of h

 p=V*(1-S)
 q=V*(1-S*(f))
 t=V*(1-S*(1-f))




 H 0~252 S 0~255 V 0~255 ==> RGB 0~255
 Hi=H/42        (float)

 i =floor(Hi)   (int)      // sector 0 to 5
 f =Hi-i        (float)    // factorial part of h

 p=V*(255-S)        /255
 q=V*(255-S*(f)  )  /255
 t=V*(255-S*(1-f))  /255



 H 0~252 S 0~255 V 0~255 ==> RGB 0~255

 i =H/42        (int)    // sector 0 to 5
 f =H-i*42      (int)    // factorial*42 to int

 p=V*(255-S)            /255
 q=V*(255*42-S*(f)  )   /255/42
 t=V*(255*42-S*(42-f))  /255/42

        RGB
 0      Vtp
 1      qVp
 2      PVt
 3      pqV
 4      tpV
 5      Vpq



*/



#define InDataV  InData[0]
#define InDataS  InData[1]
#define InDataH  InData[2]
/*H 0~251 S 0~255 V 0~255 ==> RGB 0~255

 i =H/42        (int)    // sector 0 to 5
 f =H-i*42      (int)    // factorial*42 to int

 p=V*(255-S)            /255
 q=V*(255*42-S*(f)  )   /255/42
 t=V*(255*42-S*(42-f))  /255/42*/
void acvImage::RGBFromHSV(BYTE* OutData,BYTE* InData)
{
    int i,f,R,G,B;
    i=InDataH /42;
    f=InDataH -i*42;

    /*
            p=InDataV *(255-InDataS )/255
            q=InDataV *(10710-InDataS *f)/10710
            t=InDataV *(10710-InDataS *(42-f))/10710
    */
    switch( i )
    {
    case 1:
        B = InDataV *(255-InDataS )/255;
        G = InDataV ;
        R = InDataV *(10710-InDataS *f)/10710;
        break;
    case 2:
        B = InDataV *(10710-InDataS *(42-f))/10710;
        G = InDataV ;
        R = InDataV *(255-InDataS )/255;
        break;
    case 3:
        B = InDataV ;
        G = InDataV *(10710-InDataS *f)/10710;
        R = InDataV *(255-InDataS )/255;
        break;
    case 4:
        B = InDataV ;
        G = InDataV *(255-InDataS )/255;
        R = InDataV *(6120-InDataS *(42-f))/6120;
        break;
    case 5:
        B = InDataV *(6120-InDataS *f)/6120;
        G = InDataV *(255-InDataS )/255;
        R = InDataV ;
        break;
    default: // case 0||6:
        B = InDataV *(255-InDataS )/255;
        G = InDataV *(6120-InDataS *(42-f))/6120;
        R = InDataV ;
        break;
    }

    if(R&0x800)R=0;
    else if(R&0x300)R=0xff;

    if(G&0x800)G=0;
    else if(G&0x300)G=0xff;

    if(B&0x800)B=0;
    else if(B&0x300)B=0xff;

    *OutData++ = B;
    *OutData++ = G;
    *OutData++ = R;

}


int acvImage::GetWidth() {
    return ROIWidth;
}
int acvImage::GetHeight() {
    return ROIHeight;
}

int acvImage::GetROIOffsetX() {
    return ROIOffsetX;
}
int acvImage::GetROIOffsetY() {
    return ROIOffsetY;
}
int acvImage::GetRealWidth() {
    return RealWidth;
}
int acvImage::GetRealHeight() {
    return RealHeight;
}


unsigned char acvImage::GetColorType() {
    return ColorType;
}
