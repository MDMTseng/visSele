
#include "acvImage.hpp"
#include <math.h>
#include "acvImage_BasicTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "acvImage_ComponentLabelingTool.hpp"

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
            *resfront=div_round(TmpSum,(i+SizeP1));
      }
      for(;i<height-Size;i++,srcfront+=wx3,resfront+=wx3)
      {
              TmpSum-=*(srcfront-SizeX2Add1*wx3);
              TmpSum+=*srcfront;
              *resfront=div_round(TmpSum,SizeX2Add1);
      }
      for(;i<height;i++,srcfront+=wx3,resfront+=wx3)
      {
            TmpSum-=*(srcfront-SizeX2Add1*wx3);
            *resfront=div_round(TmpSum,(Size+height-i));
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

            *resfront=div_round(TmpSum,(j+SizeP1));
      }
      for(;j<width-Size;j++,srcfront+=3,resfront+=3)
      {

            TmpSum-=*(srcfront-SizeX2Add1*3);
            TmpSum+=*srcfront;
            *resfront=div_round(TmpSum,SizeX2Add1);
      }
      for(;j<width;j++,srcfront+=3,resfront+=3)
      {
            TmpSum-=*(srcfront-SizeX2Add1*3);
            *resfront=div_round(TmpSum,(Size+width-j));
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
    char k,l,Mod,MidSize;
    int i,j;
    int TmpPixelR,TmpPixelG,TmpPixelB;

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


#define DEGDim 3

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

void acvSobelFilter(acvImage *res,acvImage *src)
{
    int i,j;
    int TmpPixelH,TmpPixelV;
    BYTE *L1,*L2,*L3;
    for(i=1;i<src->GetHeight()-1;i++)
    {
      L1=&(src->CVector[i-1][0]);
      L2=&(src->CVector[i+0][0]);
      L3=&(src->CVector[i+1][0]);
      L1+=3,L2+=3,L3+=3;
      for(j=1;j<src->GetWidth()-1;j++,L1+=3,L2+=3,L3+=3)
      {


          TmpPixelH=(L1[-3]+2*L1[0]+L1[+3])-
                    (L3[-3]+2*L3[0]+L3[+3]);//Get gradient in direction ^
          TmpPixelH=div_round(TmpPixelH,8);//TmpPixelH>>3;
          if(TmpPixelH==128)TmpPixelH=127;
          res->CVector[i][3*j]=(char)TmpPixelH;

          TmpPixelV=(L1[-3]+2*L2[-3]+L3[-3])-
                    (L1[3]+2*L2[3]+L3[+3]);//Get gradient in direction <
          TmpPixelV=div_round(TmpPixelV,8);
          if(TmpPixelV==128)TmpPixelV=127;
          res->CVector[i][3*j+1]=(char)TmpPixelV;
          res->CVector[i][3*j+2]=L2[0];
      }
    }
}

void acvHarrisCornorResponse(acvImage *buff,acvImage *src)
{
  acvImage *sobelXY=buff;
  acvSobelFilter(sobelXY,src);
  for(int i=0;i<sobelXY->GetHeight();i++)
  {
    for(int j=0;j<sobelXY->GetWidth();j++)
    {
      int Ix=(char)sobelXY->CVector[i][j*3+0];
      int Iy=(char)sobelXY->CVector[i][j*3+1];
      //Ix max = 127  => Ixx=127*127 =adj> Ixx=Ixx/128
      int tmp;
      sobelXY->CVector[i][j*3+0]=128+div_round(Ix*Ix,128);//128+(0~126)
      sobelXY->CVector[i][j*3+1]=128+div_round(Iy*Iy,128);//128+(0~126)
      sobelXY->CVector[i][j*3+2]=128+div_round(Ix*Iy,128);//128+(-126~126)
    }
  }

  acvBoxFilter(src,sobelXY,2);
  acvBoxFilter(src,sobelXY,2);
  sobelXY->ChannelOffset(1);
  acvBoxFilter(src,sobelXY,2);
  acvBoxFilter(src,sobelXY,2);
  sobelXY->ChannelOffset(1);
  acvBoxFilter(src,sobelXY,2);
  acvBoxFilter(src,sobelXY,2);
  sobelXY->ChannelOffset(-2);
  //cim = (Ix2.*Iy2 - Ixy.^2)./(Ix2 + Iy2 + eps);
  for(int i=0;i<sobelXY->GetHeight();i++)
  {
    for(int j=0;j<sobelXY->GetWidth();j++)
    {
      float GoIxx=(float)sobelXY->CVector[i][j*3+0]-128;
      float GoIyy=(float)sobelXY->CVector[i][j*3+1]-128;
      float GoIxy=(float)sobelXY->CVector[i][j*3+2]-128;

      //cim = (Ix2.*Iy2 - Ixy.^2)./(Ix2 + Iy2 + eps); % Harris corner measure
      //cim = (Ix2.*Iy2 - Ixy.^2) - k*(Ix2 + Iy2).^2;
      //float Cim=(float)(GoIxx*GoIyy-GoIxy*GoIxy)/(GoIxx+GoIyy+0.01);
      float Cim=(float)(GoIxx*GoIyy-GoIxy*GoIxy)-0.1*(GoIxx+GoIyy)*(GoIxx+GoIyy);
      Cim=Cim*10;
      if(Cim<0)Cim=0;
      if(Cim>255)Cim=255;
      src->CVector[i][j*3+0]=(src->CVector[i][j*3+0]+(BYTE)Cim*7)/8;
    }
  }

}


void acvSobelFilterX(acvImage *res,acvImage *src)
{
    int i,j;
    int TmpPixelH,TmpPixelV;
    BYTE *L1,*L2,*L3;
    for(i=1;i<src->GetHeight()-1;i++)
    {
      L1=&(src->CVector[i-1][3]);
      L2=&(src->CVector[i+0][3]);
      L3=&(src->CVector[i+1][3]);
      for(j=1;j<src->GetWidth()-1;j++,L1+=3,L2+=3,L3+=3)
      {


          TmpPixelV=(L1[-3]+2*L2[-3]+L3[-3])-
                    (L1[3]+2*L2[3]+L3[+3]);
          TmpPixelV/=8;
          res->CVector[i][3*j]=(char)TmpPixelV+128;
      }
    }
}




void acvDistanceTransform_Chamfer(acvImage *src,acvDT_distRotate *distList,int distListL)
{
  int i,j;
  int TmpPixelH,TmpPixelV;
  BYTE *preL;
  BYTE *L;
  int distRotate=0;

  acvDeleteFrame(src);
  for(i=1;i<src->GetHeight()-1;i++)
  {
    preL=&(src->CVector[i-1][3]);
    L=&(src->CVector[i][3]);
    for(j=1;j<src->GetWidth()-1;j++,preL+=3,L+=3)
    {
      distRotate+=1;
      if(distRotate==distListL)distRotate=0;
      _24BitUnion *pe=(_24BitUnion*)(   L-0);
      if(pe->_2Byte.Num==0)continue;
      acvDT_distRotate *dists=distList+distRotate;
      _24BitUnion *pa=(_24BitUnion*)(preL-3);
      _24BitUnion *pb=(_24BitUnion*)(preL-0);
      _24BitUnion *pc=(_24BitUnion*)(preL+3);
      _24BitUnion *pd=(_24BitUnion*)(   L-3);
            //a|b|c
            //d|e
      uint16_t a=pa->_2Byte.Num;
      uint16_t b=pb->_2Byte.Num;
      uint16_t c=pc->_2Byte.Num;
      uint16_t d=pd->_2Byte.Num;
      if(a>c)a=c;
      if(b>d)b=d;

      if(a!=65535)a+=dists->distX;
      if(b!=65535)b+=dists->dist;
      if(a>b)a=b;
      pe->_2Byte.Num=a;
    }
  }
  for(i=src->GetHeight()-2;i>=1;i--)
  {

    preL=&(src->CVector[i+1][(src->GetWidth()-2)*3]);
    L=&(src->CVector[i][(src->GetWidth()-2)*3]);
    for(j=src->GetWidth()-2;j>=1;j--,L-=3,preL-=3)
    {
      distRotate+=1;
      if(distRotate==distListL)distRotate=0;
      _24BitUnion *pe=(_24BitUnion*)(   L-0);
      if(pe->_2Byte.Num==0)continue;
      acvDT_distRotate *dists=distList+distRotate;
      _24BitUnion *pa=(_24BitUnion*)(preL-3);
      _24BitUnion *pb=(_24BitUnion*)(preL-0);
      _24BitUnion *pc=(_24BitUnion*)(preL+3);
      _24BitUnion *pd=(_24BitUnion*)(   L+3);
            // |e|d
            //a|b|c
      uint16_t a=pa->_2Byte.Num;
      uint16_t b=pb->_2Byte.Num;
      uint16_t c=pc->_2Byte.Num;
      uint16_t d=pd->_2Byte.Num;
      if(a>c)a=c;
      if(b>d)b=d;
      if(a!=65535)a+=dists->distX;
      if(b!=65535)b+=dists->dist;
      if(a>b)a=b;
      if(pe->_2Byte.Num>a)pe->_2Byte.Num=a;
    }
  }


}

void acvDistanceTransform_Chamfer(acvImage *src,int dist,int distX)
{
  acvDT_distRotate dists[5];
  dists[0].dist=dist;
  dists[0].distX=distX;
  acvDistanceTransform_Chamfer(src,dists,1);
}

void acvDistanceTransform_ChamferX(acvImage *src)
{
  acvDT_distRotate dists[5];
  dists[0].dist=5;
  dists[0].distX=7;
  dists[1].dist=4;
  dists[1].distX=8;
  dists[2].dist=6;
  dists[2].distX=6;

  dists[3].dist=4;
  dists[3].distX=8;

  dists[4].dist=6;
  dists[4].distX=6;


  acvDistanceTransform_Chamfer(src,dists,5);
}

#define pix16b(pixB_ptr)  ( ((_24BitUnion*)&(pixB_ptr))->_2Byte.Num )
void acvDistanceTransform_Sobel(acvImage *res,acvImage *src)
{
    int i,j;
    int TmpPixelH,TmpPixelV;
    BYTE *L1,*L2,*L3;
    for(i=1;i<src->GetHeight()-1;i++)
    {
      L1=&(src->CVector[i-1][0]);
      L2=&(src->CVector[i+0][0]);
      L3=&(src->CVector[i+1][0]);
      L1+=3,L2+=3,L3+=3;
      for(j=1;j<src->GetWidth()-1;j++,L1+=3,L2+=3,L3+=3)
      {


          TmpPixelV=(pix16b(L1[-3])+2*pix16b(L2[-3])+pix16b(L3[-3]))-
                    (pix16b(L1[ 3])+2*pix16b(L2[ 3])+pix16b(L3[ 3]));//Get gradient in direction <
          res->CVector[i][3*j]=(char)(TmpPixelV);
          TmpPixelH=(pix16b(L1[-3])+2*pix16b(L1[0])+pix16b(L1[3]))-
                    (pix16b(L3[-3])+2*pix16b(L3[0])+pix16b(L3[3]));//Get gradient in direction ^
          res->CVector[i][3*j+1]=(char)(TmpPixelH);


          res->CVector[i][3*j+2]=L2[2];
      }
    }
}

acv_XY acvSignedMap2Sampling(acvImage *signedMap2,const acv_XY &XY)
{
  acv_XY sample;
  int rX=(int)(XY.X);
  int rY=(int)(XY.Y);
  float resX=XY.X-rX;
  float resY=XY.Y-rY;

  float c00,c10,c11,c01;
  c00 = (char)signedMap2->CVector[rY][rX*3];
  c01 = (char)signedMap2->CVector[rY][(rX+1)*3];
  c10 = (char)signedMap2->CVector[rY+1][rX*3];
  c11 = (char)signedMap2->CVector[rY+1][(rX+1)*3];
  c00+=resX*(c01-c00);
  c10+=resX*(c11-c10);
  c00+=resY*(c10-c00);
  sample.X=(c00);


  c00 = (char)signedMap2->CVector[rY][rX*3+1];
  c01 = (char)signedMap2->CVector[rY][(rX+1)*3+1];
  c10 = (char)signedMap2->CVector[rY+1][rX*3+1];
  c11 = (char)signedMap2->CVector[rY+1][(rX+1)*3+1];

  c00+=resX*(c01-c00);
  c10+=resX*(c11-c10);
  c00+=resY*(c10-c00);
  sample.Y=(c00);

  return sample;
}


float acvUnsignedMap1Sampling(acvImage *unsignedMap1,const acv_XY &XY)
{
  int rX=(int)(XY.X);
  int rY=(int)(XY.Y);
  float resX=XY.X-rX;
  float resY=XY.Y-rY;

  float c00,c10,c11,c01;
  c00 = unsignedMap1->CVector[rY][rX*3];
  c01 = unsignedMap1->CVector[rY][(rX+1)*3];
  c10 = unsignedMap1->CVector[rY+1][rX*3];
  c11 = unsignedMap1->CVector[rY+1][(rX+1)*3];
  c00+=resX*(c01-c00);
  c10+=resX*(c11-c10);
  c00+=resY*(c10-c00);

  return c00;
}


acv_XY acvSignedMap2Sampling_Nearest(acvImage *signedMap2,const acv_XY &XY)
{

  acv_XY sample;
  int rX=(int)round(XY.X);
  int rY=(int)round(XY.Y);
  sample.X = (char)signedMap2->CVector[rY][rX*3];
  sample.Y = (char)signedMap2->CVector[rY][rX*3+1];
  return sample;

}


float acvUnsignedMap1Sampling_Nearest(acvImage *unsignedMap1,const acv_XY &XY)
{

  int rX=(int)round(XY.X);
  int rY=(int)round(XY.Y);
  return unsignedMap1->CVector[rY][rX*3];
}
