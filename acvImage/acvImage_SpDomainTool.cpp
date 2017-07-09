
#include "acvImage.hpp"
#include <math.h>



void acvbBottomEdge(acvImage *Pic)
{
    int i,j;
    int TmpPixel;

    for(i=0;i<Pic->GetHeight()-1;i++)for(j=0;j<Pic->GetWidth();j++)
    {
        TmpPixel=Pic->CVector[i][3*j]-Pic->CVector[i+1][3*j  ];

        if(TmpPixel>=0)
                Pic->CVector[i][3*j]=255;
    }

}

void acvbBottomEdge(acvImage *OutPic,acvImage *OriPic)
{
    int i,j;
    int TmpPixel;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth();j++)
    {
        TmpPixel=OriPic->CVector[i][3*j]-OriPic->CVector[i+1][3*j  ];


        if(TmpPixel<0)
                OutPic->CVector[i][3*j]=255;
        else
                OutPic->CVector[i][3*j]=0;
    }

}


void acvbEdgeDetect(acvImage *OutPic,acvImage *OriPic)
{
    int i,j;
    int TmpPixel;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {
        TmpPixel=2*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];


        if(TmpPixel!=0)
                OutPic->CVector[i][3*j]=255;
        else
                OutPic->CVector[i][3*j]=0;
    }

}

void acvBoxFilterY(acvImage *res,acvImage *src,int Size)
{
  int i,j,k,TmpSum,SizeX2Add1=Size*2+1;
  int SizeP1=Size+1;
  int width = src->GetWidth();
  int height = src->GetHeight();
  int wx3 = width*3;
  BYTE *srcfront;
  BYTE *resfront;
  for(j=0;j<width;j++)
  {
      TmpSum=0;
      srcfront=&(src->CVector[0][3*j]);
      resfront=&(res->CVector[0][3*j]);
      for(k=0;k<Size;k++,srcfront+=wx3)
         TmpSum+=*srcfront;

      for(i=0;i<SizeP1;i++,srcfront+=wx3,resfront+=wx3)
      {
            TmpSum+=*srcfront;
            *resfront=TmpSum/(j+SizeP1);
      }
      for(;i<height-Size;i++,srcfront+=wx3,resfront+=wx3)
      {
              TmpSum-=*(srcfront-SizeX2Add1*wx3);
              TmpSum+=*srcfront;
              *resfront=TmpSum/SizeX2Add1;
      }
      for(;i<height;i++,srcfront+=wx3,resfront+=wx3)
      {
            TmpSum-=*(srcfront-SizeX2Add1*wx3);
            *resfront=TmpSum/(Size+width+j);
      }

    }
}

void acvBoxFilterX(acvImage *res,acvImage *src,int Size)
{
  int i,j,k,TmpSum,SizeX2Add1=Size*2+1;
  int SizeP1=Size+1;

  int width = src->GetWidth();
  int height = src->GetHeight();
  BYTE *srcfront;
  BYTE *resfront;
  for(i=0;i<height;i++)
  {
      TmpSum=0;
      srcfront=&(src->CVector[i][0]);
      resfront=&(res->CVector[i][0]);
      for(k=0;k<Size;k++,srcfront+=3)
      {
         TmpSum+=*srcfront;
      }
      for(j=0;j<SizeP1;j++,srcfront+=3,resfront+=3)
      {
            TmpSum+=*srcfront;
            *resfront=TmpSum/(j+SizeP1);
      }
      for(;j<width-Size;j++,srcfront+=3,resfront+=3)
      {

            TmpSum-=*(srcfront-SizeX2Add1*3);
            TmpSum+=*srcfront;
            *resfront=TmpSum/SizeX2Add1;
      }
      for(;j<width;j++,srcfront+=3,resfront+=3)
      {
            TmpSum-=*(srcfront-SizeX2Add1*3);
            *resfront=TmpSum/(Size+width+j);
      }
  }
}
void acvBoxFilter(acvImage *BuffPic,acvImage *Pic,int Size)
{
    acvBoxFilterX(BuffPic,Pic,Size);
    acvBoxFilterY(Pic,BuffPic,Size);
}




void acvMasking(acvImage *OutPic,acvImage *OriPic,unsigned char size,char** Mask)
{
    static char k,l,Mod,MidSize;
    static int i,j;
    static int TmpPixelR,TmpPixelG,TmpPixelB;

    MidSize=size/2;
    Mod=0;
    for(k=0;k<size;k++)for(l=0;l<size;l++)
        Mod+=Mask[k][l];
    if(Mod<=0)Mod=1;
    for(i=MidSize;i<OriPic->GetHeight()-MidSize;i++)for(j=MidSize;j<OriPic->GetWidth()-MidSize;j++)
    {
        for(k=0;k<size;k++)for(l=0;l<size;l++)
        {
                TmpPixelB+=OriPic->CVector[i-MidSize+k][3*(j-MidSize+l)]*Mask[k][l];
                TmpPixelG+=OriPic->CVector[i-MidSize+k][3*(j-MidSize+l)+1]*Mask[k][l];
                TmpPixelR+=OriPic->CVector[i-MidSize+k][3*(j-MidSize+l)+2]*Mask[k][l];
        }
        TmpPixelB/=Mod;
        TmpPixelG/=Mod;
        TmpPixelR/=Mod;
        if(TmpPixelB<0)TmpPixelB=0;
        if(TmpPixelB>255)TmpPixelB=255;
        if(TmpPixelG<0)TmpPixelG=0;
        if(TmpPixelG>255)TmpPixelG=255;
        if(TmpPixelR<0)TmpPixelR=0;
        if(TmpPixelR>255)TmpPixelR=255;

        OutPic->CVector[i-1][3*(j-1)]=TmpPixelB;
        OutPic->CVector[i-1][3*(j-1)+1]=TmpPixelG;
        OutPic->CVector[i-1][3*(j-1)+2]=TmpPixelR;
        TmpPixelR=TmpPixelG=TmpPixelB=0;
    }

}
void acvSharp(acvImage *OutPic,acvImage *OriPic,float SharpLevel)
{
    static int i,j;
    static float TmpPixel;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {
        TmpPixel=SharpLevel*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];
        TmpPixel/=SharpLevel-2;
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        if(TmpPixel>255)TmpPixel=255;
        OutPic->CVector[i][3*j]=TmpPixel;

        TmpPixel=SharpLevel*OriPic->CVector[i][3*j+1]-OriPic->CVector[i][3*(j+1)+1]
                -OriPic->CVector[i+1][3*j+1];
        TmpPixel/=SharpLevel-2;
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        if(TmpPixel>255)TmpPixel=255;
        OutPic->CVector[i][3*j+1]=TmpPixel;

        TmpPixel=SharpLevel*OriPic->CVector[i][3*j+2]-OriPic->CVector[i][3*(j+1)+2]
                -OriPic->CVector[i+1][3*j+2];
        TmpPixel/=SharpLevel-2;
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        if(TmpPixel>255)TmpPixel=255;
        OutPic->CVector[i][3*j+2]=TmpPixel;
    }

}
void acvGSharp(acvImage *OutPic,acvImage *OriPic,float SharpLevel)
{
    int i,j;
    float TmpPixel;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {
        TmpPixel=SharpLevel*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];
        if(SharpLevel!=2)
                TmpPixel/=SharpLevel-2;
        if(TmpPixel<0)
                TmpPixel=0;
        if(TmpPixel>255)TmpPixel=255;

        OutPic->CVector[i][3*j]=TmpPixel;
    }

}



void acvGFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve)
{
    int i,j;
    float TmpPixel;

    for(i=1;i<OriPic->GetHeight()-1;i++)for(j=1;j<OriPic->GetWidth()-1;j++)
    {
        if(OriPic->CVector[i][3*j]<Valve)
        {
                TmpPixel=3*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j];
                //TmpPixel/=0.6;
                if(TmpPixel<0)
                        TmpPixel=0;
                if(TmpPixel>255)
                        TmpPixel=255;

        }
        else
        {
                TmpPixel=OriPic->CVector[i][3*j]+OriPic->CVector[i][3*(j+1)]
                +OriPic->CVector[i+1][3*j]+OriPic->CVector[i+1][3*(j+1)];
                 TmpPixel/=4;
        }
                OutPic->CVector[i][3*j]=TmpPixel;
    }

}
# define DEGDim 3
void acvSFilter_De(acvImage *Pic,BYTE Valve)
{
    int i,j;
    float TmpPixel;

    for(i=1;i<Pic->GetHeight()-1;i++)for(j=1;j<Pic->GetWidth()-1;j++)
    {
         TmpPixel=(Pic->CVector[i][3*(j-1)]+Pic->CVector[i][3*(j+1)]
                +Pic->CVector[i-1][3*j]+Pic->CVector[i+1][3*j]);

        if(fabs(Pic->CVector[i][3*(j)]*4-TmpPixel)>Valve)
                Pic->CVector[i][3*j]=TmpPixel/4;
    }

}
void acvDEGFilter_Separate(acvImage *BuffPic,acvImage *Pic,BYTE Valve,int Channel)
{
    int i,j,k,l;
    float TmpPixel,a;

    for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {

                a=0;
                TmpPixel=a*Pic->CVector[i][3*(j)+ Channel];

                //TmpPixel=a=0;

                for(k=-DEGDim;k<DEGDim+1;k++)   if(i+k<0||i+k>=Pic->GetHeight())        continue;
                //if(fabs(Pic->CVector[i][3*(j)]-Pic->CVector[i+k][3*(j+l)])<=Valve)
                else if(Pic->CVector[i][3*(j)+ Channel]-Pic->CVector[i+k][3*j+ Channel]>=-Valve
                       &&Pic->CVector[i][3*j+ Channel]-Pic->CVector[i+k][3*j+ Channel]<=Valve)
                {
                       //a+=1/(k+0.1);
                       //TmpPixel+=Pic->CVector[k+i][3*j]/(k+0.1);
                       a++;
                       TmpPixel+=Pic->CVector[k+i][3*j+ Channel];

                }
                TmpPixel/=a;
                BuffPic->CVector[i][3*j+ Channel]=TmpPixel;
    }
     for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {

                a=0;
                TmpPixel=a*BuffPic->CVector[i][3*(j)+ Channel];

                //TmpPixel=a=0;

                for(l=-DEGDim;l<DEGDim+1;l++)   if(j+l<0||j+l>=Pic->GetWidth())        continue;
                //if(fabs(Pic->CVector[i][3*(j)]-Pic->CVector[i+k][3*(j+l)])<=Valve)
                else if(Pic->CVector[i][3*(j)+ Channel]-BuffPic->CVector[i][3*(j+l)+ Channel]>=-Valve
                       &&Pic->CVector[i][3*(j)+ Channel]-BuffPic->CVector[i][3*(j+l)+ Channel]<=Valve)
                {
                       //a+=1/(l+0.1);
                       //TmpPixel+=Pic->CVector[i][3*(l+j)]/(l+0.1);
                       a++;
                       TmpPixel+=BuffPic->CVector[i][3*(l+j)+ Channel];

                }
                TmpPixel/=a;
                Pic->CVector[i][3*j+ Channel]=TmpPixel;
    }

}
//#define Deviation 2
#define Rootof2PI 2.506628275
void acvDeGFilter_Gauss(acvImage *BuffPic,acvImage *Pic,int FilterSize,double Deviation,BYTE Valve)
{

    int i,j,k,CentralPixel,Diff;
    double PixelSum,a,*Gauss=new double[FilterSize+1];
    if(Deviation!=0)
        for(i=0;i<=FilterSize;i++)Gauss[i]=10000*exp(-2.0/Deviation/Deviation*(i*i))/Deviation/Rootof2PI;
    else
        for(i=0;i<=FilterSize;i++)Gauss[i]=10000;
    for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {
                a=0;
                PixelSum=a*Pic->CVector[i][3*(j)];
                CentralPixel=Pic->CVector[i][3*(j)];

                //TmpPixel=a=0;

                for(k=-FilterSize;k<=FilterSize;k++)   if(i+k<0||i+k>=Pic->GetHeight())        continue;
                else
                {
                        Diff=CentralPixel-Pic->CVector[i+k][3*j];
                        if(Diff>=-Valve&&Diff<=Valve)
                        {
                                if(k<0)
                                {
                                        a+=Gauss[-k];
                                        PixelSum+=Gauss[-k]*Pic->CVector[k+i][3*j];
                                }
                                else
                                {
                                        a+=Gauss[k];
                                        PixelSum+=Gauss[k]*Pic->CVector[k+i][3*j];
                                }
                        }
                }
                PixelSum/=a;
                BuffPic->CVector[i][3*j]=PixelSum;
    }
     for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {
                a=0;
                PixelSum=a*BuffPic->CVector[i][3*(j)];
                CentralPixel=BuffPic->CVector[i][3*(j)];

                //TmpPixel=a=0;

                for(k=-FilterSize;k<=FilterSize;k++)   if(j+k<0||j+k>=Pic->GetWidth())        continue;
                else
                {
                        Diff=CentralPixel-BuffPic->CVector[i][3*(j+k)];
                        if(Diff>=-Valve&&Diff<=Valve)
                        {
                                if(k<0)
                                {
                                        a+=Gauss[-k];
                                        PixelSum+=Gauss[-k]*BuffPic->CVector[i][3*(j+k)];
                                }
                                else
                                {
                                        a+=Gauss[k];
                                        PixelSum+=Gauss[k]*BuffPic->CVector[i][3*(j+k)];
                                }
                        }
                }
                PixelSum/=a;
                Pic->CVector[i][3*j]=PixelSum;

    }
    delete(Gauss);
}

void acvDeGFilter(acvImage *BuffPic,acvImage *Pic,int FilterSize,BYTE Valve)
{
    int i,j,k;
    float PixelSum,CentralPixel,Diff,a;

    for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {
                a=0;
                PixelSum=a*Pic->CVector[i][3*(j)];
                CentralPixel=Pic->CVector[i][3*(j)];

                //TmpPixel=a=0;

                for(k=-FilterSize;k<FilterSize+1;k++)   if(i+k<0||i+k>=Pic->GetHeight())        continue;
                else
                {
                        Diff=CentralPixel-Pic->CVector[i+k][3*j];
                        if(Diff>=-Valve&&Diff<=Valve)
                        {
                                a++;
                                PixelSum+=Pic->CVector[k+i][3*j];
                        }
                }
                PixelSum/=a;
                BuffPic->CVector[i][3*j]=PixelSum;
    }
     for(i=0;i<Pic->GetHeight();i++)for(j=0;j<Pic->GetWidth();j++)
    {
                a=0;
                PixelSum=a*BuffPic->CVector[i][3*(j)];
                CentralPixel=BuffPic->CVector[i][3*(j)];

                //TmpPixel=a=0;

                for(k=-FilterSize;k<FilterSize+1;k++)   if(j+k<0||j+k>=Pic->GetWidth())        continue;
                else
                {
                        Diff=CentralPixel-BuffPic->CVector[i][3*(j+k)];
                        if(Diff>=-Valve&&Diff<=Valve)
                        {
                                a++;
                                PixelSum+=BuffPic->CVector[i][3*(j+k)];
                        }
                }
                PixelSum/=a;
                Pic->CVector[i][3*j]=PixelSum;

    }

}

# define ErDim  1
# define MaskDim  ((ErDim*2+1)*(ErDim*2+1))


void acvMidianFilter(acvImage *OutPic,acvImage *OriPic)
{
    int i,j,k,l,gg;
    int p[MaskDim],temp;
    for(i=ErDim;i<OriPic->GetHeight()-ErDim;i++)for(j=ErDim;j<OriPic->GetWidth()-ErDim;j++)
    {




                for(k=-ErDim;k<ErDim+1;k++)for(l=-ErDim;l<ErDim+1;l++)
                   p[(k+ErDim)*(ErDim*2+1)+l+ErDim]=OriPic->CVector[i+k][3*(j+l)];

                for (k=0;k<MaskDim;k++)for (l=k+1;l<MaskDim;l++)
                if (p[k]<p[l])
                {
                        temp=p[k];
                        p[k]=p[l];
                        p[l]=temp;
                }
                k=0;
                /*for(k=0;p[MaskDim/2-k]==0&&k<ErDim;k++);
                for(;p[MaskDim/2-k]==255&&k>ErDim-1;k--);
                */

                OutPic->CVector[i][3*j]=p[MaskDim/2-k];




    }

}




void acvVMidianFilter(acvImage *OutPic,acvImage *OriPic)
{
    int i,j,k,l,gg,Vk,Vl;
    int p[MaskDim],temp;
    for(i=ErDim;i<OriPic->GetHeight()-ErDim;i++)for(j=ErDim;j<OriPic->GetWidth()-ErDim;j++)
    {
                Vk=Vl=0;
                for(k=0;k<DEGDim+1;k++)
                {
                           if(i+k<OriPic->GetHeight())
                           Vk+=(OriPic->CVector[i][3*(j)]-OriPic->CVector[i+k][3*j])/(k+1);
                           if(j+k<OriPic->GetWidth())
                           Vl+=(OriPic->CVector[i][3*(j)]-OriPic->CVector[i][3*(j+k)])/(k+1);

                           if(i-k>=0)
                           Vk-=(OriPic->CVector[i][3*(j)]-OriPic->CVector[i-k][3*j])/(k+1);
                           if(j-k>=0)
                           Vl-=(OriPic->CVector[i][3*(j)]-OriPic->CVector[i][3*(j-k)])/(k+1);

                }
                if(hypot(Vk,Vl)<10)
                        OutPic->CVector[i][3*j]=OriPic->CVector[i][3*(j)];
                else

                {
                for(k=-ErDim;k<ErDim+1;k++)for(l=-ErDim;l<ErDim+1;l++)
                   p[(k+ErDim)*(ErDim*2+1)+l+ErDim]=OriPic->CVector[i+k][3*(j+l)];

                for (k=0;k<MaskDim;k++)for (l=k+1;l<MaskDim;l++)
                if (p[k]<p[l])
                {
                        temp=p[k];
                        p[k]=p[l];
                        p[l]=temp;
                }
                for(k=0;p[MaskDim/2-k]==0&&k<ErDim;k++);
                for(;p[MaskDim/2-k]==255&&k>ErDim-1;k--);


                OutPic->CVector[i][3*j]=p[MaskDim/2-k];
                }


    }

}
void acvWExtremeFilter(acvImage *OutPic,acvImage *OriPic)
{
    int i,j,R,iw,jw;
    BYTE WheelType=0,SampDot,TmpPixel;
    BYTE  Max=0,Min=255;

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
    {
         if(OriPic->CVector[i][3*(j)]>Max)Max=OriPic->CVector[i][3*(j)];
         else if(OriPic->CVector[i][3*(j)]<Min)Min=OriPic->CVector[i][3*(j)];
    }
    Max--;Min++;


    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
    {
                if(OriPic->CVector[i][3*(j)]>=Max||OriPic->CVector[i][3*(j)]<=Min)
                {
                        iw=i;jw=j;R=0;WheelType=0;
                        while(1)
                        {
                                switch(WheelType)
                                {
                                        case 0:if(++jw>=R+j)WheelType++;break;

                                        case 1:if(++iw>=R+i)WheelType++;break;

                                        case 2:if(--jw<=j-R)WheelType++;break;

                                        case 3:if(--iw<=i-R){WheelType=0;R++;}break;

                                }
                                if(
                                iw>0 &&
                                jw>0 &&
                                iw<OriPic->GetHeight()-1 &&
                                jw<OriPic->GetWidth()-1 )
                                {
                                       if(OriPic->CVector[iw][3*(jw)]<Max&&OriPic->CVector[iw][3*(jw)]>Min)
                                       {
                                        TmpPixel=OriPic->CVector[iw][3*(jw)];

                                        break;
                                       }
                                }



                                if(R>50)
                                {
                                        TmpPixel=OriPic->CVector[i][3*(j)];
                                        break;
                                }


                        }



                }
                else
                      TmpPixel=OriPic->CVector[i][3*(j)];

                OutPic->CVector[i][3*j]=TmpPixel;



    }

}

void acvDeSingularFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve)
{
    int i,j,k,l,MinErr,MinErrPixel,TmpErr;

    for(i=1;i<OriPic->GetHeight()-1;i++)for(j=1;j<OriPic->GetWidth()-1;j++)
    {
                MinErr=999;
                for(k=-1;k<2;k++)for(l=-1;l<2;l++)if(k|l)//k,l!=0
                {
                        TmpErr=OriPic->CVector[i][3*j]-OriPic->CVector[i+k][3*(j+l)];
                        if(TmpErr<0)
                        {
                                if(MinErr>-TmpErr)
                                {
                                        MinErr=-TmpErr;
                                        MinErrPixel= OriPic->CVector[i+k][3*(j+l)];
                                }
                        }
                        else
                        {
                                if(MinErr>TmpErr)
                                {
                                        MinErr=TmpErr;
                                        MinErrPixel=OriPic->CVector[i+k][3*(j+l)];
                                }
                        }
                }
                if(MinErr>Valve)
                        OutPic->CVector[i][3*j]=MinErrPixel;
                else

                        OutPic->CVector[i][3*j]=OriPic->CVector[i][3*j];



    }

}
void acvErrorFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve)
{
    int i,j,k,l,ErrorNum;

    for(i=ErDim;i<OriPic->GetHeight()-ErDim;i++)for(j=ErDim;j<OriPic->GetWidth()-ErDim;j++)
    {
                ErrorNum=0;
                for(k=-ErDim;k<ErDim+1;k++)for(l=-ErDim;l<ErDim+1;l++)
                if(fabs(OriPic->CVector[i][3*(j)]-OriPic->CVector[i+k][3*(j+l)])>Valve)
                {ErrorNum++;
                        if(ErrorNum>MaskDim-ErDim-5)
                        goto Out2Loops;
                }
                Out2Loops:;

                if(ErrorNum>MaskDim-ErDim-5)
                      OutPic->CVector[i][3*j]=OriPic->CVector[i+k][3*(j+l)];
                else
                      OutPic->CVector[i][3*j]=OriPic->CVector[i][3*(j)];



    }

}
void acvSDeSpikeFilter(acvImage *OutPic,acvImage *OriPic)
{
     int i,j,k,l,gg;
    int p[9],temp;
    for(i=ErDim;i<OriPic->GetHeight()-1;i++)for(j=ErDim;j<OriPic->GetWidth()-1;j++)
    {




                for(k=-1;k<2;k++)for(l=-1;l<2;l++)
                   p[(k+1)*3+l+1]=OriPic->CVector[i+k][3*(j+l)];

                for (k=0;k<9;k++)for (l=k+1;l<9;l++)
                if (p[k]<p[l])
                {
                        temp=p[k];
                        p[k]=p[l];
                        p[l]=temp;
                }



                if(p[1]<=OriPic->CVector[i][3*(j)]||p[7]>=OriPic->CVector[i][3*(j)])
                {

                                OutPic->CVector[i][3*j]=p[4];
                }
                else

                OutPic->CVector[i][3*j]=OriPic->CVector[i][3*(j)];



    }

}
void acvDeSpikeFilter(acvImage *OutPic,acvImage *OriPic,BYTE Valve)
{
     int i,j,k,l,gg;
    int p[9],temp;
    for(i=ErDim;i<OriPic->GetHeight()-1;i++)for(j=ErDim;j<OriPic->GetWidth()-1;j++)
    {




                for(k=-1;k<2;k++)for(l=-1;l<2;l++)
                   p[(k+1)*3+l+1]=OriPic->CVector[i+k][3*(j+l)];

                for (k=0;k<9;k++)for (l=k+1;l<9;l++)
                if (p[k]<p[l])
                {
                        temp=p[k];
                        p[k]=p[l];
                        p[l]=temp;
                }



                if(p[0]==OriPic->CVector[i][3*(j)])
                {
                       if(p[0]-p[1]>Valve)
                                OutPic->CVector[i][3*j]=p[4];
                }
                else if(p[8]==OriPic->CVector[i][3*(j)])
                {
                        if(p[7]-p[8]>Valve)
                                OutPic->CVector[i][3*j]=p[4];

                }
                else

                OutPic->CVector[i][3*j]=OriPic->CVector[i][3*(j)];



    }

}

void acvGEdgeDetect(acvImage *OutPic,acvImage *OriPic)
{
    static int i,j;
    static int TmpSub1,TmpSub2;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {

        TmpSub1=OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)];

        TmpSub2=OriPic->CVector[i][3*j]-OriPic->CVector[i+1][3*j  ];

        if(TmpSub1<0)
                TmpSub1=-TmpSub1;
        if(TmpSub2<0)
                TmpSub2=-TmpSub2;

        TmpSub1=(TmpSub2>TmpSub1)? TmpSub2:TmpSub1;
        if(TmpSub2>TmpSub1) TmpSub1=TmpSub2;

        /*TmpPixel=2*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];
        if(TmpPixel<0)
                TmpPixel=0; */
        OutPic->CVector[i][3*j]=TmpSub1;
    }

}
void acvLaplace(acvImage *OutPic,acvImage *OriPic)
{
    static int i,j;
    static int TmpPixel;

    for(i=1;i<OriPic->GetHeight()-1;i++)for(j=1;j<OriPic->GetWidth()-1;j++)
    {
        /*TmpPixel=8*OriPic->CVector[i][3*j]
                -OriPic->CVector[i][3*(j-1)]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*(j-1)]-OriPic->CVector[i+1][3*(j+1)]
                -OriPic->CVector[i-1][3*j]-OriPic->CVector[i+1][3*j]
                -OriPic->CVector[i-1][3*(j-1)]-OriPic->CVector[i-1][3*(j+1)];*/
        TmpPixel=4*OriPic->CVector[i][3*j]
                -OriPic->CVector[i][3*(j-1)]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i-1][3*j]-OriPic->CVector[i+1][3*j];
        if(TmpPixel<0)
                TmpPixel=0;
        OutPic->CVector[i][3*j]=TmpPixel;
    }

}

void acvCLaplace(acvImage *OutPic,acvImage *OriPic)
{
    int i,j;
    int TmpPixel,Max;

    for(i=1;i<OriPic->GetHeight()-1;i++)for(j=1;j<OriPic->GetWidth()-1;j++)
    {

        TmpPixel=OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j-1)];
        if(TmpPixel>0)Max=TmpPixel;
        TmpPixel=OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)];
        if(TmpPixel>Max)Max=TmpPixel;
        TmpPixel=OriPic->CVector[i][3*j]-OriPic->CVector[i-1][3*j];
        if(TmpPixel>Max)Max=TmpPixel;
        TmpPixel=OriPic->CVector[i][3*j]-OriPic->CVector[i+1][3*j];
        if(TmpPixel>Max)Max=TmpPixel;

        OutPic->CVector[i][3*j]=Max;
    }

}
void acvSobel(acvImage *OutPic,acvImage *OriPic)
{
    static int i,j;
    static int TmpPixelH,TmpPixelV;

    for(i=1;i<OriPic->GetHeight()-1;i++)for(j=1;j<OriPic->GetWidth()-1;j++)
    {


        TmpPixelH=2*OriPic->CVector[i-1][3*j]
                   +OriPic->CVector[i-1][3*(j-1)]
                   +OriPic->CVector[i-1][3*(j+1)]
                 -2*OriPic->CVector[i+1][3*j]
                   -OriPic->CVector[i+1][3*(j-1)]
                   -OriPic->CVector[i+1][3*(j+1)];
        TmpPixelV=2*OriPic->CVector[i][3*(j-1)]
                   +OriPic->CVector[i-1][3*(j-1)]
                   +OriPic->CVector[i+1][3*(j-1)]
                 -2*OriPic->CVector[i][3*(j+1)]
                   -OriPic->CVector[i-1][3*(j+1)]
                   -OriPic->CVector[i+1][3*(j+1)];

        TmpPixelH=(TmpPixelH<0)?  0:TmpPixelH;
        TmpPixelV=(TmpPixelV<0)?  0:TmpPixelV;
        //TmpPixelH=(TmpPixelH>TmpPixelV)?   TmpPixelH:TmpPixelV;

        //TmpPixelH+=TmpPixelV;
        //if(TmpPixelH>255)TmpPixelH=255;


        TmpPixelH=(TmpPixelV+TmpPixelH)/2;



        OutPic->CVector[i][3*j]=TmpPixelH;
    }

}

void acvSGEdgeDetect(acvImage *OutPic,acvImage *OriPic)
{
    static int i,j;
    static int TmpPixel;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {
        TmpPixel=2*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];
        //if(OriPic->CVector[i][3*j]==0)OriPic->CVector[i][3*j]=1;

        //TmpPixel=TmpPixel*200/OriPic->CVector[i][3*j];

        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        if(TmpPixel>255)TmpPixel=255;
        OutPic->CVector[i][3*j]=TmpPixel;
    }

}
void acvCEdgeDetect(acvImage *OutPic,acvImage *OriPic)
{
    static int i,j;
    static int TmpPixel,DirSum;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++,DirSum=0)
    {
        TmpPixel=2*OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)]
                -OriPic->CVector[i+1][3*j  ];
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        DirSum+=TmpPixel;



        TmpPixel=2*OriPic->CVector[i][3*j+1]-OriPic->CVector[i][3*(j+1)+1]
                -OriPic->CVector[i+1][3*j+1];
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        DirSum+=TmpPixel;

        TmpPixel=2*OriPic->CVector[i][3*j+2]-OriPic->CVector[i][3*(j+1)+2]
                -OriPic->CVector[i+1][3*j+2];
        if(TmpPixel<0)
                TmpPixel=-TmpPixel;
        DirSum+=TmpPixel;

        if(DirSum>255)DirSum=255;
        OutPic->CVector[i][3*j]=DirSum;
    }

}
void acvCEdgeDetect2(acvImage *OutPic,acvImage *OriPic)
{
    int i,j;
    int TmpSub1,TmpSub2,DirSum;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++,DirSum=0)
    {
        TmpSub1=OriPic->CVector[i][3*j]-OriPic->CVector[i][3*(j+1)];

        TmpSub2=OriPic->CVector[i][3*j]-OriPic->CVector[i+1][3*j  ];

        if(TmpSub1<0)
                TmpSub1=-TmpSub1;
        if(TmpSub2<0)
                TmpSub2=-TmpSub2;

        TmpSub1=(TmpSub2>TmpSub1)? TmpSub2:TmpSub1;
        DirSum=TmpSub1;



        TmpSub1=OriPic->CVector[i][3*j+1]-OriPic->CVector[i][3*(j+1)+1];

        TmpSub2=OriPic->CVector[i][3*j+1]-OriPic->CVector[i+1][3*j  +1];

        if(TmpSub1<0)
                TmpSub1=-TmpSub1;
        if(TmpSub2<0)
                TmpSub2=-TmpSub2;

        TmpSub1=(TmpSub2>TmpSub1)? TmpSub2:TmpSub1;
        if(DirSum<TmpSub1)
        DirSum=TmpSub1;

        TmpSub1=OriPic->CVector[i][3*j+2]-OriPic->CVector[i][3*(j+1)+2];

        TmpSub2=OriPic->CVector[i][3*j+2]-OriPic->CVector[i+1][3*j  +2];

        if(TmpSub1<0)
                TmpSub1=-TmpSub1;
        if(TmpSub2<0)
                TmpSub2=-TmpSub2;

        TmpSub1=(TmpSub2>TmpSub1)? TmpSub2:TmpSub1;
        if(DirSum<TmpSub1)
        DirSum=TmpSub1;

        //if(DirSum>255)DirSum=255;
        OutPic->CVector[i][3*j]=DirSum;
    }

}



void acvHEdgeDetect(acvImage *OutPic,acvImage *OriPic)    //Hue 000~126 127~251
{
    static int i,j;
    static int MaxSub,Sub;

    for(i=0;i<OriPic->GetHeight()-1;i++)for(j=0;j<OriPic->GetWidth()-1;j++)
    {
        Sub=OriPic->CVector[i][3*j+2]-OriPic->CVector[i][3*(j+1)+2];
        if(Sub<0)Sub=-Sub;
        if(Sub>127)Sub=251-Sub;

        MaxSub=Sub;

        Sub=OriPic->CVector[i][3*j+2]-OriPic->CVector[i+1][3*(j)+2];
        if(Sub<0)Sub=-Sub;
        if(Sub>127)Sub=251-Sub;
        if(Sub>MaxSub)
                MaxSub=Sub;

        OutPic->CVector[i][3*j]=MaxSub;
    }

}
