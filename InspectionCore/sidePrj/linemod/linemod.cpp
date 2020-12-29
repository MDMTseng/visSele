#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "IMG_UTIL.hpp"
#include "common_lib.h"
#include <math.h>


#include <xmmintrin.h>
#include <pmmintrin.h>
#include <immintrin.h>


int DIR_Coding_downShift(acvImage *dst,acvImage *src,int downScale)
{
  dst->ReSize(src->GetWidth()/downScale,src->GetHeight()/downScale);
  int space=0;
  for (int i = 0; i < src->GetHeight(); i+=1)
  {
    for (int j = 0; j < src->GetWidth(); j+=1)
    {
      uint8_t *dt=&dst->CVector[i/downScale][(j/downScale)*3];
      
      
      uint8_t *scL0=&src->CVector[i][j*3];
      *dt|=*scL0;
    }
  }
  return 0;
}

int DIR_Coding_Spread_shift1x1(acvImage *dst,acvImage *src)
{
  int space=0;
  int CHC=src->Channel;
  for (int i = space; i < src->GetHeight()-space-2; i+=1)
  {
    for (int j = space; j < src->GetWidth()-space-2; j+=1)
    {

      uint8_t *dt=&dst->CVector[i][j*3];
      
      
      uint8_t *scL0=&src->CVector[i][j*CHC];
      uint8_t *scL1=&src->CVector[i+1][j*CHC];
      uint8_t *scL2=&src->CVector[i+2][j*CHC];

      uint8_t cod=0;
      cod =scL0[0]|scL0[CHC]|scL0[CHC*2];

      cod|=scL1[0]|scL1[CHC]|scL1[CHC*2];

      // dt[1]=scL1[4];
      // dt[2]=scL1[5];
      cod|=scL2[0]|scL2[CHC]|scL2[CHC*2];
      dt[0]=cod;
    }
  }
  return 0;
}

uint8_t SOBEL_to_DIR5_Coding(int X,int Y)
{

  if(Y<0)Y=-Y;
  bool XFlip=false;
  if(X<0)
  {
    XFlip=true;
    X=-X;
  }
  int Yk=Y*1024;
  const int tan36degX1024=(int)(0.726542528*1024);
  int Y1=X*tan36degX1024;
  if(Yk<Y1)
  {
    return XFlip?1<<4:1<<0;
  }
  
  const int tan72degX1024=(int)(3.07768353718*1024);
  int Y2=X*tan72degX1024;
  if(Yk<Y2)
  {
    return XFlip?1<<3:1<<1;
  }
  
  return 1<<2;
}


uint8_t SOBEL_to_DIR8_Sec(int X,int Y)
{
  if(Y<0)Y=-Y;
  bool XFlip=false;
  if(X<0)
  {
    XFlip=true;
    X=-X;
  }
  int Yk=Y*1024;

  {
    const int tan22_5degX1024=(int)(0.41421356237);
    int Yt=X*tan22_5degX1024;
    if(Yk<Yt)
    {
      return XFlip?7:0;
    }
  }


  {
    const int tan45degX1024=(int)(1*1024);
    int Yt=X*tan45degX1024;
    if(Yk<Yt)
    {
      return XFlip?7:0;
    }
  }
  

  {
    const int tan67_5degX1024=(int)(2.41421356237*1024);
    int Yt=X*tan67_5degX1024;
    if(Yk<Yt)
    {
      return XFlip?7:0;
    }
  }

  return XFlip?4:3;
}


uint8_t SOBEL_to_DIR8_Coding(int X,int Y)
{

  return 1<< SOBEL_to_DIR8_Sec(X,Y);
}




// uint8_t* gen_MatchingCodeLUT(int sectionCount)
// {
//   uint8_t *matching_score_LUT;
//   matching_score_LUT=new uint8_t[1<<sectionCount];
//   uint8_t lowScan=1;
//   uint8_t highScan=1<<(sectionCount-1);
//   uint8_t maxScore=255;
//   int scanCount=(sectionCount-1)/2;
//   //When sectionCount==5 => scanCount==2  1 |2/3,1/3|
//   //When sectionCount==6 => scanCount==2  1 |2/3,1/3| 0
//   //section within |...| is the loop will assign, otherwise it's the fixed data, 
//   //the first match is 1 and the last one is 0 if possible(the section might not in exact 90deg apart)

//   for(int i=0;i<sizeof(matching_score_LUT);i++)
//   {
//     if(i&1)
//     {
//       matching_score_LUT[i]=maxScore;
//       continue;
//     }
//     uint8_t _lS=lowScan;
//     uint8_t _hS=highScan;
//     matching_score_LUT[i]=0;
//     for(int j=0;j<scanCount;j++)
//     {
//       if(i&(_lS|_hS))
//       {
//         matching_score_LUT[i]=maxScore*(scanCount-i)/(scanCount+1);
//         break;
//       }
//       _lS<<=1;
//       _hS>>=1;
//     }
//   }
// }

class LM_DirRspInfo
{
  public:
  uint8_t *scoreLUT=NULL;
  int scoreLUTMax=255;
  int tarDir=-1;
  int skipN;
  int sectionCount=5;
  std::vector<uint8_t*> respSet;
  int bytesPerPix=2;
  int cW,cH;
  int dW,dH;
  LM_DirRspInfo(acvImage *tarImg, int skipN ,int dir)
  {
    
    RESET(tarImg,  skipN  , dir);
  }
  
  static void *aligned_malloc(size_t required_bytes, size_t alignment) {
    void *p1;
    void **p2;
    int offset=alignment-1+sizeof(void*);
    p1 = malloc(required_bytes + offset);               // the line you are missing
    p2=(void**)(((size_t)(p1)+offset)&~(alignment-1));  //line 5
    p2[-1]=p1; //line 6
    return p2;
  }

  static void aligned_free( void* p ) {
      void* p1 = ((void**)p)[-1];         // get the pointer to the buffer we allocated
      free( p1 );
  }


  static int pos_mod(int a, int b) { return (a % b + b) % b; }

  static uint32_t MatchingCode2Score(uint32_t sectionCount,uint32_t maxScore,uint32_t sectionCoding, uint32_t secCenIdx=0)
  {
    if(secCenIdx<0)return -1;
    sectionCoding=sectionCoding&((1<<sectionCount)-1);
    uint32_t secCode=sectionCoding;
    if(secCenIdx!=0)
    {
      secCenIdx=pos_mod(secCenIdx,sectionCount);
      uint32_t LSec=sectionCoding&((1<<secCenIdx)-1);
      uint32_t HSec=sectionCoding>>(secCenIdx);
      secCode=(LSec<<(sectionCount-secCenIdx))|HSec;
    }



    if(secCode&1)
    {
      return maxScore;
    }
    
    int scanCount=(sectionCount-1)/2;
    uint32_t _lS=2;
    uint32_t _hS=1<<(sectionCount-1);
    int retScore=0;
    for(int j=0;j<scanCount;j++)
    {
      if(secCode&(_lS|_hS))
      {
        return maxScore*(scanCount-j)/(scanCount+1);;
      }
      _lS<<=1;
      _hS>>=1;
    }

    return retScore;
  }
  
  void CLEANUP()
  {
    for(int i=0;i<respSet.size();i++)
    {
      aligned_free(respSet[i]);
    }
    respSet.resize(0);
    if(scoreLUT)
    {
      delete scoreLUT;
      scoreLUT=NULL;
    }
  }


  void RESET(acvImage *tarImg, int skipN  ,int dir)
  {
    CLEANUP();
    if(this->tarDir!=dir)
    {
      this->tarDir==dir;
    }

    cW=tarImg->GetWidth();
    cH=tarImg->GetHeight();
    dW=cW/skipN;
    dH=cH/skipN;
    
    cW=dW*skipN;
    cH=dH*skipN;
    this->skipN=skipN;
    for(int i=0;i<skipN*skipN;i++)
    {
      respSet.push_back((uint8_t*)aligned_malloc(dW*dH*bytesPerPix,16));
    }

    scoreLUT=new uint8_t[1<<sectionCount];


    for(int i=0;i<(1<<sectionCount);i++)
    {
      scoreLUT[i]=MatchingCode2Score(sectionCount,scoreLUTMax,i,dir);
    }
  }



  void linearize(acvImage *tarImg)
  {
    for(int i=0;i<respSet.size();i++)
    {
      int railX=i%skipN;
      int railY=i/skipN;
      int imgPixOffset=(railY*tarImg->GetWidth()+railX);
      for(int j=0;j<dH;j++)
      {
        uint8_t *imgLine=tarImg->CVector[0]+(imgPixOffset+j*skipN*tarImg->GetWidth())*tarImg->Channel;
        // printf("%d:%d\n",i,j);
        for(int k=0;k<dW;k++)
        {
          uint32_t code=(*imgLine)&((1<<sectionCount)-1);
          respSet[i][(j*dW+k)*bytesPerPix]=scoreLUT[code];
          imgLine+=tarImg->Channel*skipN;
          
          // printf("    k:%d\n",k);
        }
      }
      // break;
    }
  }

  void* fetchLinearMemLine(int x,int y,int *ret_availLen,int *ret_dataSize)
  {
    if(x<0 || y<0)return NULL;

    int rspY=y/skipN;
    int rspX=x/skipN;
    // printf("rspX:%d dW:%d\n",rspX,dW);
    if(rspX>=dW || rspY>=dH)return NULL;

    // printf("OK\n",rspX,dW);
    
    int setIdx=(y%skipN)*skipN;
    int respIdx=setIdx+x%skipN;
    int respOnIdx = (rspY*dW+rspX);
    if(ret_availLen)
    {
      *ret_availLen=(dW*dH)-respOnIdx;
    }
    if(ret_dataSize)
      *ret_dataSize=bytesPerPix;
    return &respSet[respIdx][0];
  }


  void linearizeX(acvImage *tarImg)
  {
    for(int i=0;i<cH;i++)
    {
      int setIdx=(i%skipN)*skipN;
      int onSetIdx = i/skipN;
      for(int j=0;j<cW;j++)
      {
        int respIdx=setIdx+j%skipN;
        int respOnIdx = (onSetIdx*dW+j/skipN)*bytesPerPix;
        // printf("rIdx:%d rOIdx:%d\n",respIdx,respOnIdx);
        uint32_t code=(tarImg->CVector[i][tarImg->Channel*j])&((1<<sectionCount)-1);
        respSet[respIdx][respOnIdx]=scoreLUT[code];

      }
    }
  }
};
class LM_LinearMemStorage
{
  public:
  std::vector<LM_DirRspInfo*> dirSet;
  int skipN;
  LM_LinearMemStorage(acvImage *tarImg, int skipN  ,int dirCount)
  {
    dirSet.resize(0);
    RESET(tarImg,skipN ,dirCount);
  }


  void CLEANUP()
  {
    for(int i=0;i<dirSet.size();i++)
    {
      delete dirSet[i];
    }
    dirSet.resize(0);
  }

  void RESET(acvImage *tarImg, int skipN ,int dirCount)
  {
    if(dirCount!=dirSet.size())
    {
      CLEANUP();
      for(int i=0;i<dirCount;i++)
      {
        dirSet.push_back(new LM_DirRspInfo(tarImg,skipN,i));
      }
      return;
    }
    
    for(int i=0;i<dirCount;i++)
    {
      dirSet[i]->RESET(tarImg,skipN,i);
    }
  }


  void linearize(acvImage *tarImg)
  {

    for(int i=0;i<dirSet.size();i++)
    {
      dirSet[i]->linearize(tarImg);
    }
  }

  void* fetchLinearMemLine(int x,int y, int dir,int *ret_availLen,int *ret_dataSize)
  {
    if(dir<0 || dir>dirSet.size()-1 )return NULL;
    return dirSet[dir]->fetchLinearMemLine(x,y,ret_availLen,ret_dataSize);
  }
};

class LM_Template
{
  public:
  typedef struct feature
  {
    int X,Y;
    int8_t gNX,gNY;
    int8_t dir;
    uint8_t *memLine;
  };
  std::vector<feature> features;
  int mX=9999999,MX=0;
  int mY=9999999,MY=0;
  void updateInfo()
  {
    mX=9999999;MX=0;
    mY=9999999;MY=0;
    for(int i=0;i<features.size();i++)
    {
      if(mX>features[i].X)
      {
        mX=features[i].X;
      }
      if(MX<features[i].X)
      {
        MX=features[i].X;
      }

      if(mY>features[i].Y)
      {
        mY=features[i].Y;
      }
      if(MY<features[i].Y)
      {
        MY=features[i].Y;
      }
    }

    for(int i=0;i<features.size();i++)
    {
      if(mX>features[i].X)
      {
        mX=features[i].X;
      }
      if(MX<features[i].X)
      {
        MX=features[i].X;
      }

      if(mY>features[i].Y)
      {
        mY=features[i].Y;
      }
      if(MY<features[i].Y)
      {
        MY=features[i].Y;
      }
    }

  }
  
};

int SOBEL_RGB(acvImage *img,acvImage *sobel,int edgeDiffMin=5)
{
  int space=1;
  int gS=1;
  int edgeStrengthMin=edgeDiffMin*(4+4)*1.5;
  int edgeStrengthMin_sq=edgeStrengthMin*edgeStrengthMin;
  for (int i = space; i < img->GetHeight()-space; i+=1)
  {
    for (int j = space; j < img->GetWidth()-space; j+=1)
    {
      uint8_t *p0=img->CVector[i-gS]+(j-gS)*3;
      uint8_t *p1=img->CVector[i-gS]+(j- 0)*3;
      uint8_t *p2=img->CVector[i-gS]+(j+gS)*3;

      uint8_t *p3=img->CVector[i- 0]+(j+gS)*3;

      uint8_t *p4=img->CVector[i+gS]+(j+gS)*3;
      uint8_t *p5=img->CVector[i+gS]+(j- 0)*3;
      uint8_t *p6=img->CVector[i+gS]+(j-gS)*3;

      uint8_t *p7=img->CVector[i- 0]+(j-gS)*3;

      int sobelX[3]={0};
      int sobelY[3]={0};

      /*
        -x-y   -2y   +x-y
        [0]    [1]    [2]
        -2x           +2x
        [7]           [3]
        -x+y    2y   +x+y
        [6]    [5]    [4]
      */


      sobelX[0]+=-p0[0]        +p2[0];
      sobelX[1]+=-p0[1]        +p2[1];
      sobelX[2]+=-p0[2]        +p2[2];
      sobelY[0]+=-p0[0]-2*p1[0]-p2[0];
      sobelY[1]+=-p0[1]-2*p1[1]-p2[1];
      sobelY[2]+=-p0[2]-2*p1[2]-p2[2];


      sobelX[0]+=-2*p7[0]+2*p3[0];
      sobelX[1]+=-2*p7[1]+2*p3[1];
      sobelX[2]+=-2*p7[2]+2*p3[2];



      sobelX[0]+=-p6[0]        +p4[0];
      sobelX[1]+=-p6[1]        +p4[1];
      sobelX[2]+=-p6[2]        +p4[2];
      sobelY[0]+=+p6[0]+2*p5[0]+p4[0];
      sobelY[1]+=+p6[1]+2*p5[1]+p4[1];
      sobelY[2]+=+p6[2]+2*p5[2]+p4[2];

      //ABS
      // if(sobelX[0<0)sobelX[0=-sobelX[0;
      // if(sobelX[1<0)sobelX[1=-sobelX[1;
      // if(sobelX[2<0)sobelX[2=-sobelX[2;
      // if(sobelX[0<0)sobelX[0=-sobelX[0;
      // if(sobelX[1<0)sobelX[1=-sobelX[1;
      // if(sobelX[2<0)sobelX[2=-sobelX[2;
      int mag_sq[3]={
        sobelX[0]*sobelX[0]+sobelY[0]*sobelY[0],
        sobelX[1]*sobelX[1]+sobelY[1]*sobelY[1],
        sobelX[2]*sobelX[2]+sobelY[2]*sobelY[2]};

      int maxIdx=0;
      if(mag_sq[maxIdx]<mag_sq[1])
      {
        maxIdx=1;
      }
      if(mag_sq[maxIdx]<mag_sq[2])
      {
        maxIdx=2;
      }
      int16_t mag=(int16_t)sqrt(mag_sq[maxIdx]);
      uint8_t *sc=(uint8_t*)sobel->CVector[i]+(j)*sobel->Channel;

      if(mag<edgeStrengthMin)
      {
        sc[0]=sc[1]=0;
      }
      else
      {
        int SX=sobelX[maxIdx];
        int SY=sobelY[maxIdx];
        SX=SX*127/mag;
        SY=SY*127/mag;
        sc[0]=(int8_t)SX;
        sc[1]=(int8_t)SY;
      }
    }
  }

  // acvDeleteFrame(sobel,1,0);
  return 0;
}


int SOBEL_CODING_RGB(acvImage *img,acvImage *sobel,int edgeDiffMin=5,int sobelSaveRec=2)
{
  int gS=2;
  int space=gS;
  int edgeStrengthMin=edgeDiffMin*(4+4)*1.5;
  int edgeStrengthMin_sq=edgeStrengthMin*edgeStrengthMin;
  for (int i = space; i < img->GetHeight()-space; i+=1)
  {
    for (int j = space; j < img->GetWidth()-space; j+=1)
    {
      uint8_t *p0=img->CVector[i-gS]+(j-gS)*3;
      uint8_t *p1=img->CVector[i-gS]+(j- 0)*3;
      uint8_t *p2=img->CVector[i-gS]+(j+gS)*3;

      uint8_t *p3=img->CVector[i- 0]+(j+gS)*3;

      uint8_t *p4=img->CVector[i+gS]+(j+gS)*3;
      uint8_t *p5=img->CVector[i+gS]+(j- 0)*3;
      uint8_t *p6=img->CVector[i+gS]+(j-gS)*3;

      uint8_t *p7=img->CVector[i- 0]+(j-gS)*3;

      int sobelX[3]={0};
      int sobelY[3]={0};

      /*
        -x-y   -2y   +x-y
        [0]    [1]    [2]
        -2x           +2x
        [7]           [3]
        -x+y    2y   +x+y
        [6]    [5]    [4]
      */


      sobelX[0]+=-p0[0]        +p2[0];
      sobelX[1]+=-p0[1]        +p2[1];
      sobelX[2]+=-p0[2]        +p2[2];
      sobelY[0]+=-p0[0]-2*p1[0]-p2[0];
      sobelY[1]+=-p0[1]-2*p1[1]-p2[1];
      sobelY[2]+=-p0[2]-2*p1[2]-p2[2];


      sobelX[0]+=-2*p7[0]+2*p3[0];
      sobelX[1]+=-2*p7[1]+2*p3[1];
      sobelX[2]+=-2*p7[2]+2*p3[2];



      sobelX[0]+=-p6[0]        +p4[0];
      sobelX[1]+=-p6[1]        +p4[1];
      sobelX[2]+=-p6[2]        +p4[2];
      sobelY[0]+=+p6[0]+2*p5[0]+p4[0];
      sobelY[1]+=+p6[1]+2*p5[1]+p4[1];
      sobelY[2]+=+p6[2]+2*p5[2]+p4[2];

      //ABS
      // if(sobelX[0<0)sobelX[0=-sobelX[0;
      // if(sobelX[1<0)sobelX[1=-sobelX[1;
      // if(sobelX[2<0)sobelX[2=-sobelX[2;
      // if(sobelX[0<0)sobelX[0=-sobelX[0;
      // if(sobelX[1<0)sobelX[1=-sobelX[1;
      // if(sobelX[2<0)sobelX[2=-sobelX[2;
      int mag_sq[3]={
        sobelX[0]*sobelX[0]+sobelY[0]*sobelY[0],
        sobelX[1]*sobelX[1]+sobelY[1]*sobelY[1],
        sobelX[2]*sobelX[2]+sobelY[2]*sobelY[2]};

      int maxIdx=0;
      if(mag_sq[maxIdx]<mag_sq[1])
      {
        maxIdx=1;
      }
      if(mag_sq[maxIdx]<mag_sq[2])
      {
        maxIdx=2;
      }
      // int16_t mag=(int16_t)sqrt(mag_sq[maxIdx]);
      uint8_t *sc=(uint8_t*)sobel->CVector[i]+(j)*sobel->Channel;

      // if(mag<edgeStrengthMin)
      // {
      //   sc[0]=sc[1]=0;
      // }
      // else
      // {
      //   int SX=sobelX[maxIdx];
      //   int SY=sobelY[maxIdx];
      //   SX=SX*127/mag;
      //   SY=SY*127/mag;
      //   sc[0]=(int8_t)SX;
      //   sc[1]=(int8_t)SY;
      // }

      *(uint8_t*)sc=(mag_sq[maxIdx]<edgeStrengthMin_sq)?0: (SOBEL_to_DIR8_Coding(sobelX[maxIdx],sobelY[maxIdx]));

      // // int sX=sobelX[maxIdx]/sobelSaveRec;//X
      // // if(sX>127)sX=127;
      // // if(sX<-127)sX=-127;

      // // int sY=sobelY[maxIdx]/sobelSaveRec;
      // // if(sY>127)sY=127;
      // // if(sY<-127)sY=-127;

      // // sc[1]=sX+128;
      // // sc[2]=sY+128;
      // // // sc[1]=sc[2]=0;

      
      // sc[1]=mag>>8;
      // sc[2]=mag;

      // int16_t *scMag=(int16_t *)(&sc[1]);
      // *scMag=(int16_t)mag;
    }
  }

  // acvDeleteFrame(sobel,1,0);
  return 0;
}

int SimpEdge_CODING_RGB(acvImage *img,acvImage *sobel,int edgeDiffMin=5,int sobelSaveRec=2)
{
  int gS=1;
  int space=gS;
  int edgeStrengthMin=edgeDiffMin*(1+1)*1.5;
  int edgeStrengthMin_sq=edgeStrengthMin*edgeStrengthMin;
  for (int i = space; i < img->GetHeight()-space; i+=1)
  {
    for (int j = space; j < img->GetWidth()-space; j+=1)
    {
      
      uint8_t *pc=img->CVector[i-  0]+(j   )*3;
      uint8_t *px=img->CVector[i+  0]+(j+gS)*3;
      uint8_t *py=img->CVector[i+ gS]+(j   )*3;


      int sobelX[3]={0};
      int sobelY[3]={0};


      sobelX[0]=px[0]-pc[0];
      sobelX[1]=px[1]-pc[1];
      sobelX[2]=px[2]-pc[2];

      sobelY[0]=py[0]-pc[0];
      sobelY[1]=py[1]-pc[1];
      sobelY[2]=py[2]-pc[2];

      int mag_sq[3]={
        sobelX[0]*sobelX[0]+sobelY[0]*sobelY[0],
        sobelX[1]*sobelX[1]+sobelY[1]*sobelY[1],
        sobelX[2]*sobelX[2]+sobelY[2]*sobelY[2]};

      int maxIdx=0;
      if(mag_sq[maxIdx]<mag_sq[1])
      {
        maxIdx=1;
      }
      if(mag_sq[maxIdx]<mag_sq[2])
      {
        maxIdx=2;
      }
      // int16_t mag=(int16_t)sqrt(mag_sq[maxIdx]);
      uint8_t *sc=(uint8_t*)sobel->CVector[i]+(j)*sobel->Channel;

      *(uint8_t*)sc=(mag_sq[maxIdx]<edgeStrengthMin_sq)?0: (SOBEL_to_DIR5_Coding(sobelX[maxIdx],sobelY[maxIdx]));

    }
  }

  // acvDeleteFrame(sobel,1,0);
  return 0;
}

void matchingXXXX(acvImage *res,acvImage *src,LM_Template *temp)
{
  // printf("matchingXXXX:CH2:%d,%d\n",res->Channel,src->Channel);
  if(res->Channel!=2 || src->Channel!=2)return;
  int Width=src->GetWidth()-temp->MX;
  int Height=src->GetHeight()-temp->MY;

  
  int adv=128/8/res->Channel;

  // printf("matchingXXXX:WH:%d,%d\n",Width,Height);


    int skipIdx;
    int offset_on_skipIdx;
    // LM_Template::offset(src->GetWidth(),src->GetHeight(),1,temp->features[k].X,temp->features[k].Y,&skipIdx,&offset_on_skipIdx);
    for(int i=0;i<Height;i++)
    {
      uint8_t* pixLine=src->CVector[i];
      uint8_t* resLine=res->CVector[i];

      for(int j=0;j<Width;j+=adv)
      {  
        for(int k=0;k<temp->features.size();k++)
        {

          // printf("pixLine:%p resLine:%p\n",pixLine,resLine);
          // AA = _mm_mulhrs_epi16(AA, half );
          volatile __m128i result = _mm_hadds_epi16(*(__m128i*) resLine,*(__m128i*) pixLine);

          _mm_store_si128((__m128i*) resLine,result);
          // printf("res->Channel:%d\n",res->Channel);
          resLine+=res->Channel*adv;
          pixLine+=src->Channel*adv;
        }
      }
      // printf("sdsd\n");
    }
}

void matchingXXXX2(acvImage *res,LM_Template *temp,LM_LinearMemStorage *lnMem)
{
  if(res->Channel!=2)return;
  int Width=res->GetWidth()-temp->MX;
  int Height=res->GetHeight()-temp->MY;
  int shortestLen=INT_MAX;
  int dataSize=-1;
  for(int i=0;i<temp->features.size();i++)
  {
    int dir=SOBEL_to_DIR8_Sec(temp->features[i].gNX,temp->features[i].gNY);
    int availLen=-1;
    int _dataSize=-1;
    temp->features[i].memLine=(uint8_t*)lnMem->fetchLinearMemLine(temp->features[i].X,temp->features[i].Y,dir,&availLen,&_dataSize);
    
    if(shortestLen>availLen)
    {
      shortestLen=availLen;
    }
    if(dataSize==-1)
    {
      dataSize=_dataSize;
    }
    else
    {
      
      assert( dataSize==_dataSize);
      if(dataSize!=_dataSize)
      {
        //ERROR !!!
      }
    }
  }

  
  int adv=128/8;
  // lnMem->fetchLinearMemLine
  int byteCount = shortestLen*dataSize;
  // printf("matchLen:%d\n",matchLen);
  for(int i=0;i<temp->features.size();i++)
  {
    uint8_t *resLine = res->CVector[0];
    uint8_t *line=temp->features[i].memLine;
    // printf("i:%d byteCount:%d adv:%d dataSize:%d \n",i,byteCount,adv,dataSize);
    for(int j=0;j<byteCount;j+=adv)//pixel count
    {
      // if((j&0xFF)==0)
      // printf("j:%d resLine:%p line:%p\n",j,resLine,line);
      // printf("j:%d,  %d,%d,%d,%d,%d,%d\n",j,line[0],line[1],line[2],line[3],line[4],line[5]);

      if(1)
      {
        *(__m128i*) (resLine) = _mm_hadds_epi16(*(__m128i*) (resLine), *(__m128i*) line );
        line+=adv;
        resLine+=adv;

      }
      else
      {

        for(int d=0;d<adv;d+=2)
        {
          *(uint16_t*)resLine+=*(uint16_t*)line;
          line+=2;
          resLine+=2;
        }
      }
    }
  }

}

int downScale(acvImage &dst,acvImage &src,int downFactor)
{
  dst.ReSize(src.GetWidth()/downFactor,src.GetHeight()/downFactor);


  for(int i=0;i<dst.GetHeight();i++)
  {
    for(int j=0;j<dst.GetWidth();j++)
    {
      dst.CVector[i][j*3+0]=src.CVector[i*downFactor][j*downFactor*3+0];
      dst.CVector[i][j*3+1]=src.CVector[i*downFactor][j*downFactor*3+1];
      dst.CVector[i][j*3+2]=src.CVector[i*downFactor][j*downFactor*3+2];
    }
  }

}

void printBits(unsigned int num,int bits=8, char* spacing=NULL)
{
  unsigned int pin=1<<(bits-1);
  for(int bit=0;bit<bits; bit++)
  {
    printf("%i%s", (num & pin)?1:0,spacing==NULL?" ":spacing);
    pin = pin >> 1;
  }
}

int main(int argc, char** argv)
{

  // for(int i=0;i<256;i++)
  // {
  //   printf("%03d:",i);printBits(i,8,"");
    
  //   int score = LM_DirRspInfo::MatchingCode2Score(8,1000,i,3);
  //   printf(" s:%d",score);
  //   printf("\n");

  // }
  // return 0;
  // SSE_Test();
  // return 0;
  acvImage img;

  acvImage buffer;
  if(0)
  {
    LoadIMGFile(&buffer,"data/testImg.png");
    downScale(img,buffer,4);
  }
  else
  {
    LoadIMGFile(&img,"data/testImg.png");
  }

  if(1)
  {
    LM_Template temp;
    acvImage sobelMap;
    sobelMap.ReSize(&img,1);
    
    int channelC=2;
    acvImage resultMap;
    uint8_t *resultMap_buf=(uint8_t*)aligned_malloc(img.GetWidth()*img.GetHeight()*channelC,16);
    resultMap.useExtBuffer(resultMap_buf,img.GetWidth()*img.GetHeight()*channelC,img.GetWidth(),img.GetHeight());//2 channel

    for(int i=0;i<32;i++)
    {
      LM_Template::feature f;
      f.X=rand()%100;
      f.Y=rand()%100;
      f.gNX=rand()%255;
      f.gNY=rand()%255;
      // uint8_t SOBEL_to_DIR5_Coding(int X,int Y);
      temp.features.push_back(f);
    }
    temp.updateInfo();
    // sobelMap.ReSize(&img);


    printf("START  temp.features.size():%d  resultMap:%p\n",temp.features.size(),&resultMap.CVector[0][0]);
    LM_LinearMemStorage LM(&sobelMap,8,8);
    

    clock_t t = clock();
    int CompCount=50;
    
    SOBEL_CODING_RGB(&img,&sobelMap);
    DIR_Coding_Spread_shift1x1(&sobelMap,&sobelMap);
    LM.linearize(&sobelMap);
    for(int i=0;i<CompCount;i++)
    {

      for(int k=0;k<360;k++)
        matchingXXXX2(&resultMap,&temp,&LM);
    }
    // for(int i=0;i<10;i++)
    // {

      
    //   for(int i=0;i<360;i++)
    //     matchingXXXX(&resultMap,&sobelMap,&temp);
    //     // printf("[%d]--  W:%d H:%d  skip:%d  fX:%d,fY:%d, skipIdx:%d  offset_on_skipIdx:%d\n",i,sobelMap.GetWidth(),sobelMap.GetHeight(),5,f.X,f.Y,skipIdx,offset_on_skipIdx);

    // }

    printf("SOBELX   :%fms per cycle\n", (double)(clock() - t) / CLOCKS_PER_SEC * 1000/CompCount);
    aligned_free(resultMap_buf);
    // SaveIMGFile("data/sobelMap.png",&sobelMap);
  }
  // if(0)
  // {
  //   acvImage fast_map;
  //   int downSamp=3;
  //   int rad=8;
  //   fast_map.ReSize(img.GetWidth()/downSamp,img.GetHeight()/downSamp);
  //   FAST512(&img,&fast_map,downSamp,rad, rad*4,FAST_MAP_t,NULL);
  //   img.RGBToGray();

  //   SaveIMGFile("data/fast_map.png",&fast_map);
  // }
  printf("END\n");
  return 0;
}
