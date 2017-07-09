
#include <math.h>
#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"

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
float acvFastArcTan(float x)
{
  float absx = fabs(x);
  return x*(M_PI_4-(absx - 1)*(0.2447 + 0.0663*absx));
}


float acvFastArcTan2( float y, float x )
{

  if(x==0)return y < 0 ? -M_PI_2 : M_PI_2;
  if(y==0)return 0;
	float atan;
  float absx, absy;
  absy = y < 0 ? -y : y;
  absx = x < 0 ? -x : x;
  float val;
	if ( absx>absy )
	{
		atan = acvFastArcTan(y/x);
		if ( x < 0.0f )
		{
			if ( y < 0.0f ) return atan - M_PI;
			return atan + M_PI;
		}
    return atan;
	}
	else
	{
		atan = M_PI_2 - acvFastArcTan(x/y);
		if ( y < 0.0f ) return atan - M_PI;
    return atan;
	}
	return atan;
}

void acvDeletFrame(acvImage *Pic,int line_width)
{
    int Xoffset=Pic->GetROIOffsetX(),
        Yoffset=Pic->GetROIOffsetY(),
        Height=Yoffset+ Pic->GetHeight(),
        Width =Xoffset+Pic->GetWidth() ;
    for(int k=0;k<line_width;k++)
    {
      for(int i=Yoffset;i<Height;i++)
      {
           Pic->CVector[i][3*(Xoffset+k)+0]=
           Pic->CVector[i][3*(Xoffset+k)+1]=
           Pic->CVector[i][3*(Xoffset+k)+2]=
           Pic->CVector[i][3*(Width-k)-1]=
           Pic->CVector[i][3*(Width-k)-2]=
           Pic->CVector[i][3*(Width-k)-3]=255;
      }
      for(int i=Xoffset+line_width;i<Width-line_width;i++)
      {
           Pic->CVector[Yoffset+k][3*i+1]=
           Pic->CVector[Yoffset+k][3*i+0]=
           Pic->CVector[Yoffset+k][3*i+2]=
           Pic->CVector[Height-k-1][3*i+1]=
           Pic->CVector[Height-k-1][3*i+0]=
           Pic->CVector[Height-k-1][3*i+2]=255;
      }
    }
}
void acvDeletFrame(acvImage *Pic)
{
    acvDeletFrame(Pic,1);
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
                case 1:
                case 2:
                for(int i=0;i<OriPic->GetHeight();i++)
                {
                        OutLine=OutPic->CVector[i];OriLine=OriPic->CVector[i];
                        for(int j=0;j<OriPic->GetWidth();j++)
                        {
                            BYTE tmp = OriLine[Mode];
                                *OutLine++=tmp;
                                *OutLine++=tmp;
                                *OutLine++=tmp;
                                OriLine+=3;
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
        return (int)round((float)Num);
}






char *PrintHexArr_buff(char *strBuff,int strBuffL,char *data, int dataL)
{
    char *buffptr = strBuff;

    if (data == NULL||
            dataL*3>strBuffL) {
        return NULL;

    }
    int i;
    int plen=0;
    for (i = 0; i < dataL; i++)
    {
        plen = snprintf(buffptr, strBuffL, "%02x", (unsigned char) data[i]);
        if( plen < 0 )
        {
            break;
        }
        buffptr += plen;
        strBuffL -= plen;
        if( (i&3) == 3 )
        {
            plen = snprintf(buffptr, strBuffL, " ");
            if( plen < 0 )
            {
                break;
            }
            buffptr += plen;
            strBuffL -= plen;
        }
    }
    return strBuff;
}

char *PrintHexArr(char *data, int dataL)
{
    static char bufferStr[500];

    return PrintHexArr_buff(bufferStr,sizeof(bufferStr),data, dataL);
}

unsigned char *LoadBitmapFile(char *filename, BITMAPINFOHEADER *bitmapInfoHeader)
{
    FILE *filePtr; //our file pointer
    BITMAPFILEHEADER bitmapFileHeader; //our bitmap file header
    unsigned char *bitmapImage;  //store image data
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
    fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER),1,filePtr); // small edit. forgot to add the closing bracket at sizeof

    int bmpRawSize = bitmapFileHeader.bfSize - bitmapFileHeader.bOffBits;

    //allocate enough memory for the bitmap image data
    bitmapImage = new unsigned char[bmpRawSize];

    //verify memory allocation
    if (!bitmapImage)
    {
        delete(bitmapImage);
        fclose(filePtr);
        return NULL;
    }

    //move file point to the begging of bitmap data
    fseek(filePtr, bitmapFileHeader.bOffBits, SEEK_SET);
    //read in the bitmap image data
    int rL = fread(bitmapImage,1,bmpRawSize,filePtr);

    //make sure bitmap image data was read
    if (rL != bmpRawSize)
    {
        fclose(filePtr);
        return NULL;
    }
    //close file and return bitmap iamge data
    fclose(filePtr);
    return bitmapImage;
}

unsigned int LoadBitmapFile(acvImage *img,char *filename)
{

    BITMAPINFOHEADER bitmapInfoHeader;
    unsigned char * bitmap=LoadBitmapFile(filename,&bitmapInfoHeader);
    if(bitmap == NULL)
    {
      return -1;
    }
    int biBitCount=bitmapInfoHeader.biBitCount;
    if(biBitCount != 32 && biBitCount !=24)
    {
      delete(bitmap);
      return -1;
    }
    int W=bitmapInfoHeader.biWidth,H=bitmapInfoHeader.biHeight;
    W=(W<0)?-W:W;
    H=(H<0)?-H:H;
    img->ReSize(W,H);


    unsigned char * bmp_ptr = bitmap;
    int BpP = biBitCount/8;
    int padding4Skip=(4-((img->GetWidth()*BpP)%4))%4;

    for(int i=img->GetROIOffsetY();i<img->GetHeight();i++)
    {unsigned char *ImLine;
      if(bitmapInfoHeader.biHeight<0)
        ImLine=img->CVector[i];
      else
        ImLine=img->CVector[img->GetHeight()-1-i];

            for(int j=img->GetROIOffsetX();j<img->GetWidth();j++)
            {
              ImLine[0] = bmp_ptr[0];
              ImLine[1] = bmp_ptr[1];
              ImLine[2] = bmp_ptr[2];
              ImLine+=3;
              bmp_ptr+=BpP;
            }
            bmp_ptr+=padding4Skip;
    }

    delete(bitmap);

    return 0;
}

int SaveBitmapFile(char *filename,unsigned char* pixData,int width,int height)
{
  FILE *f;
  f = fopen(filename,"wb");

  int padding=(4-((width*3)%4))%4;
  BITMAPFILEHEADER bm_header={0};
  BITMAPINFOHEADER bm_info_header={0};
  bm_info_header.biSize=sizeof(bm_info_header);
  bm_info_header.biWidth=width;
  bm_info_header.biHeight=height;
  bm_info_header.biPlanes=1;
  bm_info_header.biBitCount=8*3;
  bm_info_header.biCompression=0;
  bm_info_header.biSizeImage=width*height*3+padding*height;

  bm_header.bfType=0x4D42;
  bm_header.bOffBits=sizeof(bm_header)+sizeof(bm_info_header);
  bm_header.bfSize=bm_info_header.biSizeImage+bm_header.bOffBits;

  unsigned char bmppad[3] = {0,0,0};

  fwrite(&bm_header,1,sizeof(bm_header),f);
  fwrite(&bm_info_header,1,sizeof(bm_info_header),f);
  for(int i=0; i<height; i++)
  {
      fwrite(pixData+(width*(height-i-1)*3),3,width,f);
      fwrite(bmppad,1,(4-(width*3)%4)%4,f);
  }
  fclose(f);
  return 0;
}
