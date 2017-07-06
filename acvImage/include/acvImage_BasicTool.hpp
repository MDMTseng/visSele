#ifndef ACV_IMG_BASIC_TOOL_H
#define ACV_IMG_BASIC_TOOL_H
#include <math.h>
#include "acvImage.hpp"

void acvThreshold(acvImage *Pic,BYTE Var)
{
    BYTE* BMPLine;
    int Height=Pic->GetROIOffsetY()+ Pic->GetHeight(),
        Width =Pic->GetROIOffsetX()+Pic->GetWidth() ;
    for(int i=Pic->GetROIOffsetY();i<Height;i++)
    {
        BMPLine=Pic->CVector[i]+Pic->GetROIOffsetX()*3;
        for(int j=Pic->GetROIOffsetX();j<Width;j++)

           if(*BMPLine>Var)
           {
               *BMPLine++=255;
               *BMPLine++=255;
               *BMPLine++=255;
           }
           else
           {
               *BMPLine++=0;
               *BMPLine++=0;
               *BMPLine++=0;
           }

    }
}


void acvDeletFrame(acvImage *Pic)
{
    int Xoffset=Pic->GetROIOffsetX(),
        Yoffset=Pic->GetROIOffsetY(),
        Height=Yoffset+ Pic->GetHeight(),
        Width =Xoffset+Pic->GetWidth() ;

    for(int i=Yoffset;i<Height;i++)
    {
         Pic->CVector[i][3*Xoffset+0]=
         Pic->CVector[i][3*Xoffset+1]=
         Pic->CVector[i][3*Xoffset+2]=
         Pic->CVector[i][3*Width-1]=
         Pic->CVector[i][3*Width-2]=
         Pic->CVector[i][3*Width-3]=255;
    }
    for(int i=Xoffset;i<Width;i++)
    {
         Pic->CVector[Yoffset][3*i+1]=
         Pic->CVector[Yoffset][3*i+0]=
         Pic->CVector[Yoffset][3*i+2]=
         Pic->CVector[Height-1][3*i+1]=
         Pic->CVector[Height-1][3*i+0]=
         Pic->CVector[Height-1][3*i+2]=255;
    }
}


void acvClear(acvImage *Pic,BYTE Var)
{
    BYTE* BMPLine;
    int Xoffset=Pic->GetROIOffsetX(),
        Yoffset=Pic->GetROIOffsetY(),
        Height=Yoffset+ Pic->GetHeight(),
        Width =Xoffset+Pic->GetWidth() ;
    for(int i=Yoffset;i< Height;i++)
    {
        BMPLine=Pic->CVector[i]+Xoffset*3;
        for(int j=Xoffset;j<Width;j++)
        {
               *BMPLine++=Var;
               *BMPLine++=Var;
               *BMPLine++=Var;
        }
    }
}
void acvTurn(acvImage *Pic)
{
    BYTE* BMPLine;
    for(int i=0;i<Pic->GetHeight();i++)
    {
        BMPLine=Pic->CVector[i];
        for(int j=0;j<Pic->GetWidth();j++)
        {
               *BMPLine++=255-*BMPLine;
               *BMPLine++=255-*BMPLine;
               *BMPLine++=255-*BMPLine;
        }
    }
}
void acvFullB2W(acvImage *OriPic,acvImage *OutPic)
{

        BYTE *OutLine,*OriLine;

        for(int i=0;i<OriPic->GetHeight();i++)
        {
                OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                for(int j=0;j<OriPic->GetWidth();j++)
                {
                        if(*OriLine==255)
                        {
                                *OutLine++=255;
                                *OutLine++=255;
                                *OutLine++=255;
                                OriLine+=3;
                        }
                        else
                        {
                                *OutLine++=*OriLine++;
                                *OutLine++=*OriLine++;
                                *OutLine++=*OriLine++;
                        }

                }
        }

}

void acvClone_B2Gray(acvImage *OriPic,acvImage *OutPic)
{

        BYTE *OutLine,*OriLine;

        for(int i=0;i<OriPic->GetHeight();i++)
        {
                OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                for(int j=0;j<OriPic->GetWidth();j++)
                {
                        *OutLine++=*OriLine;
                        *OutLine++=*OriLine;
                        *OutLine++=*OriLine;
                        OriLine+=3;
                }
        }

}


#define CloneImage_FullCopy -1

#define CloneImage_B2Gray    0
#define CloneImage_G2Gray    1
#define CloneImage_R2Gray    2

 /*
#define CloneImage_B2B       3
#define CloneImage_G2G       4
#define CloneImage_R2R       5     */







void acvCloneImage(acvImage *OriPic,acvImage *OutPic,int Mode)
{

        BYTE *OutLine,*OriLine;

        switch(Mode)
        {
                case -1:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                                *OutLine++=*OriLine++;
                                *OutLine++=*OriLine++;
                                *OutLine++=*OriLine++;
                        }
                }break;
                case 0:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine;
                                OriLine+=3;
                        }
                }break;
                case 1:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                                OriLine++;
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine;
                                OriLine+=2;
                        }
                }break;
                case 2:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                                OriLine+=2;
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine;
                                *OutLine++=*OriLine++;
                        }
                }break;
                defalut:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                                *OutLine++=0;
                                *OutLine++=0;
                                *OutLine++=0;
                        }
                }


        }

}



int DoubleRoundInt(double Num)
{

        return (Num>0)? ((Num-(int)Num)<0.5? Num:Num+1):(((int)Num-Num)>0.5? Num-1:Num);
}






template <typename QUICKSORTType>

#define SWAP(x,y) {t = x; x = y; y = t;}
int partitionB2S(QUICKSORTType number[], int left, int right)
{
    int i, j;
    QUICKSORTType s, t;

    s = number[right];
    i = left - 1;

    for(j = left; j < right; j++) {
        if(number[j] > s) {
            i++;
            SWAP(number[i], number[j]);
        }
    }

    SWAP(number[i+1], number[right]);
    return i+1;
}

template <typename QUICKSORTType>
void quicksortB2S(QUICKSORTType number[], int left, int right)
{
    QUICKSORTType q;

    if(left < right) {
        q = partitionB2S(number, left, right);
        quicksortB2S(number, left, q-1);
        quicksortB2S(number, q+1, right);
    }
}

template <typename QUICKSORTType>
int partitionS2B(QUICKSORTType number[], int left, int right)
{
    int i, j;
    QUICKSORTType s, t;

    s = number[right];
    i = left - 1;

    for(j = left; j < right; j++)
    {
        if(number[j] < s)
        {
            i++;
            SWAP(number[i], number[j]);
        }
    }

    SWAP(number[i+1], number[right]);
    return i+1;
}

template <typename QUICKSORTType>
void quicksortS2B(QUICKSORTType number[], int left, int right)
{
    int q;

    if(left < right) {
        q = partitionS2B(number, left, right);
        quicksortS2B(number, left, q-1);
        quicksortS2B(number, q+1, right);
    }
}


template <typename QUICKSORTType>
void quicksort(QUICKSORTType number[], int left, int right,int S2BorB2S_)
{
    int q;

    if(S2BorB2S_)
    {
        quicksortS2B(number, left, right);
    }
    else
    {
        quicksortB2S(number, left, right);
    }
}


template <typename QUICKSORTType>

#define SWAP(x,y) {t = x; x = y; y = t;}
int partitionDataGroupB2S(QUICKSORTType number[],int DataL,int SortKey, int left, int right)
{
    int i, j,k;
    QUICKSORTType s, t;

    s = number[right+SortKey];
    i = left - DataL;

    for(j = left; j < right; j+=DataL) {
        if(number[j+SortKey] > s) {
            i+=DataL;
            for(k=0;k<DataL;k++)
                SWAP(number[i+k], number[j+k]);
        }
    }
    i+=DataL;
    for(k=0;k<DataL;k++)
        SWAP(number[i+k], number[right+k]);
    return i;
}

template <typename QUICKSORTType>
void quicksortDataGroupB2S(QUICKSORTType number[],int DataL,int SortKey, int left, int right)
{
    int q;

    if(left < right) {
        q = partitionDataGroupB2S(number,DataL,SortKey, left, right);
        quicksortDataGroupB2S(number,DataL,SortKey, left, q-DataL );
        quicksortDataGroupB2S(number,DataL,SortKey, q+DataL, right);
    }
}

template <typename QUICKSORTType>
int partitionDataGroupS2B(QUICKSORTType number[],int DataL,int SortKey, int left, int right)
{
    int i, j,k;
    QUICKSORTType s, t;

    s = number[SortKey+right];
    i = left - DataL;

    for(j = left; j < right; j+=DataL)
    {
        if(number[j+SortKey] < s)
        {
            i+=DataL;
            for(k=0;k<DataL;k++)
                SWAP(number[i+k], number[j+k]);
        }
    }

    i+=DataL;
    for(k=0;k<DataL;k++)
        SWAP(number[i+k], number[right+k]);
    return i;
}

template <typename QUICKSORTType>
void quicksortDataGroupS2B(QUICKSORTType number[],int DataL,int SortKey, int left, int right)
{
    int q;

    if(left < right) {
        q = partitionDataGroupS2B(number,DataL,SortKey, left, right);
        quicksortDataGroupS2B(number,DataL,SortKey, left, q-DataL);
        quicksortDataGroupS2B(number,DataL,SortKey, q+DataL, right);
    }
}


template <typename QUICKSORTType>
void quicksortDataGroup(QUICKSORTType number[],int DataL,int SortKey, int left, int right,int S2BorB2S_)
{
    int q;

    if(S2BorB2S_)
    {
        quicksortDataGroupS2B(number,DataL,SortKey, left, right);
    }
    else
    {
        quicksortDataGroupB2S(number,DataL,SortKey,  left, right);
    }
}


template <typename SORTType>
void bubblesortTopGroup(SORTType *Data,int GroupL,int GroupKey,int TopNum,SORTType* From,SORTType* To)
{
    SORTType *DataIter,*MaxIter;
    int MaxNum;
    TopNum*=GroupL;
    for(int i=0;i<TopNum;i+=GroupL)
    {
            DataIter=From+i;
            MaxIter=DataIter;
            MaxNum=MaxIter[GroupKey];
            DataIter+=GroupL;
            for(;DataIter<=To;DataIter+=GroupL)
            {
                  if(MaxNum<DataIter[GroupKey])
                  {
                        MaxIter=DataIter;
                        MaxNum=MaxIter[GroupKey];
                  }
            }
            DataIter=From+i;
            for(int j=0;j<GroupL;j++)
            {
                  MaxNum=MaxIter[j];
                  MaxIter[j]=DataIter[j];
                  DataIter[j]=MaxNum;
            }

    }
}


#pragma pack(push, 1)

typedef struct tagBITMAPFILEHEADER
{
    __int16_t bfType;  //specifies the file type
    __int32_t bfSize;  //specifies the size in bytes of the bitmap file
    __int32_t bfReserved;  //reserved; must be 0
    __int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
}BITMAPFILEHEADER;
typedef struct tagBITMAPINFOHEADER
{
    __int32_t biSize;  //specifies the number of bytes required by the struct
    __int32_t biWidth;  //specifies width in pixels
    __int32_t biHeight;  //species height in pixels
    __int16_t biPlanes; //specifies the number of color planes, must be 1
    __int16_t biBitCount; //specifies the number of bit per pixel
    __int32_t biCompression;//spcifies the type of compression
    __int32_t biSizeImage;  //size of image in bytes
    __int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
    __int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
    __int32_t biClrUsed;  //number of colors used by th ebitmap
    __int32_t biClrImportant;  //number of colors that are important
}BITMAPINFOHEADER;

#pragma pack(pop)


unsigned int LoadBitmapFile(acvImage *img,char *filename, BITMAPINFOHEADER *bitmapInfoHeader)
{
    FILE *filePtr; //our file pointer
    BITMAPFILEHEADER bitmapFileHeader; //our bitmap file header
    int imageIdx=0;  //image index counter
    unsigned char tempRGB;  //our swap variable

    //open filename in read binary mode
    filePtr = fopen(filename,"rb");
    if (filePtr == NULL)
        return -1;

    //read the bitmap file header
    fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER),1,filePtr);

    //verify that this is a bmp file by check bitmap id
    if (bitmapFileHeader.bfType !=0x4D42)
    {
        fclose(filePtr);
        return -1;
    }

    //read the bitmap info header
    fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr); // small edit. forgot to add the closing bracket at sizeof

    //move file point to the begging of bitmap data
    fseek(filePtr, bitmapFileHeader.bOffBits, SEEK_SET);


    img->ReSize(bitmapInfoHeader->biWidth,bitmapInfoHeader->biHeight);
    //read in the bitmap image data
    fread(img->ImageData,bitmapInfoHeader->biSizeImage,1,filePtr);



    //close file and return bitmap iamge data
    fclose(filePtr);
    return 0;
}

/*
unsigned char *LoadBitmapFile(acvImage *img, char *filename, )
{
    FILE *filePtr; //our file pointer
    BITMAPFILEHEADER bitmapFileHeader; //our bitmap file header
    BITMAPINFOHEADER bitmapInfoHeader;
    int imageIdx=0;  //image index counter
    unsigned char tempRGB;  //our swap variable

    //open filename in read binary mode
    filePtr = fopen(filename,"rb");
    if (filePtr == NULL)
        return NULL;

    //read the bitmap file header
    fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER),1,filePtr);

    //verify that this is a bmp file by check bitmap id
    if (bitmapFileHeader.bfType !=0x4D42)
    {
        fclose(filePtr);
        return NULL;
    }

    //read the bitmap info header
    fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr); // small edit. forgot to add the closing bracket at sizeof

    img->ReSize(bitmapInfoHeader.biWidth,bitmapInfoHeader.biHeight);
    //move file point to the begging of bitmap data
    fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);



    //read in the bitmap image data
    fread(img->ImageData,bitmapInfoHeader->biSizeImage,filePtr);

    //close file and return bitmap iamge data
    fclose(filePtr);
    return bitmapImage;
}*/

#endif
