#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "acvImage_BasicTool.hpp"
#include "common_lib.h"
#include <math.h>

#include <xmmintrin.h>
#include <pmmintrin.h>
#include <immintrin.h>

void FAST512(acvImage *src,acvImage *fast_map,int steps,int circleR, int circleR_segs,
 int (*FAST_MAP)(acvImage *img,int x,int y, std::vector <uint8_t* >&pixbuf, uint8_t* ret_PIX,void *params),
 void *params);

 

void NShift(int v,int* max, int* sec, int* trd)
{
  if(*trd<v)
  {
    *trd=v;
  }

  if(*sec<v)
  {
    *trd=*sec;
    *sec=v;
  }

  if(*max<v)
  {
    *sec=*max;
    *max=v;
  }
}

int FAST_MAP_t(acvImage *img,int x,int y, std::vector <uint8_t* >&pixbuf, uint8_t* ret_PIX,void *params)
{
  // ret_PIX[0]=x;
  // ret_PIX[1]=y;
  // ret_PIX[2]=x*y;

  uint8_t *_centerPix = &(img->CVector[y][3*x]);
  int unMod=0;
  int count=0;

  int maxCount[3]={0};
  int secCount[3]={0};
  int trdCount[3]={0};
  int initMode=-1;
  int initCount=0;
  int smargin=15;

  int centerPix=_centerPix[0];
  for(int k=0;k<pixbuf.size();k++)
  {

    int curMod=0;
    if(pixbuf[k][0]>centerPix+smargin)
    {
      curMod=1;
    }
    else if(pixbuf[k][0]<centerPix-smargin)
    {
      curMod=2;
    }

    if(unMod!=curMod)
    {
      if(initMode==-1 && maxCount[unMod]<count)
      {
        initMode=unMod;
        initCount=count;
      }
      NShift(count,maxCount+unMod, secCount+unMod,trdCount+unMod);
      unMod=curMod;
      count=1;
    }
    else
    {
      count++;
    }

    // printf("(%d,%d,%d),",pixbuf[k],unMod,count);
  }
  // printf("\n");

  if(unMod==initMode)
  {
    count+=initCount;
  }

  NShift(count,maxCount+unMod, secCount+unMod,trdCount+unMod);

  int max = (maxCount[2]>maxCount[1]?maxCount[2]:maxCount[1]);
  // max=(max>maxCount[0]?max:maxCount[0]);

  if(max<pixbuf.size()*55/100)max=0;
  // if(max>pixbuf.size()*95/100)max=0;


  ret_PIX[0]=
  ret_PIX[1]=max*255/pixbuf.size();
  ret_PIX[2]=centerPix;//maxCount[2]*255/circleR_segs;


  return -1;
}




int FAST_MAP_ts(acvImage *img,int x,int y, std::vector <uint8_t* >&pixbuf, uint8_t* ret_PIX,void *params)
{
  
  uint8_t *_centerPix = &(img->CVector[y][3*x]);

  int centerPix=_centerPix[0];

  int smargin=15;
  int sum=0;

  for(int k=0;k<pixbuf.size();k++)
  {
    int diff = pixbuf[k][0]-centerPix;
    if(diff<0)diff=-diff;
    if(diff>smargin)
      sum++;
  }

  if(sum<pixbuf.size()*3/5)sum=0;
  if(sum>pixbuf.size()*9/10)sum=0;

  ret_PIX[0]=
  ret_PIX[1]=sum*2*255/pixbuf.size();//maxCount[1]*255/circleR_segs;
  ret_PIX[2]=centerPix;//maxCount[2]*255/circleR_segs;


  return -1;
}



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
  for (int i = space; i < src->GetHeight()-space-2; i+=1)
  {
    for (int j = space; j < src->GetWidth()-space-2; j+=1)
    {
      uint8_t *dt=&dst->CVector[i][j*3];
      
      
      uint8_t *scL0=&src->CVector[i][j*3];
      uint8_t *scL1=&src->CVector[i+1][j*3];
      uint8_t *scL2=&src->CVector[i+2][j*3];

      uint8_t cod=0;
      cod =scL0[0]|scL0[3]|scL0[6];

      cod|=scL1[0]|scL1[3]|scL1[6];

      dt[1]=scL1[4];
      dt[2]=scL1[5];
      cod|=scL2[0]|scL2[3]|scL2[6];
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




uint8_t SOBEL_to_DIR8_Coding(int X,int Y)
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
      return XFlip?1<<7:1<<0;
    }
  }


  {
    const int tan45degX1024=(int)(1*1024);
    int Yt=X*tan45degX1024;
    if(Yk<Yt)
    {
      return XFlip?1<<7:1<<0;
    }
  }
  

  {
    const int tan67_5degX1024=(int)(2.41421356237*1024);
    int Yt=X*tan67_5degX1024;
    if(Yk<Yt)
    {
      return XFlip?1<<7:1<<0;
    }
  }

  return XFlip?1<<4:1<<3;
}



int SOBEL_CODING_RGB(acvImage *img,acvImage *sobel,int edgeDiffMin=5,int sobelSaveRec=2)
{
  int space=1;
  int gS=1;
  int edgeStrengthMin=edgeDiffMin*(4+4)*1.5;
  // edgeStrengthMin=0;
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
      uint8_t *sc=sobel->CVector[i]+(j)*3;

      sc[0]=(mag<edgeStrengthMin)?0: (SOBEL_to_DIR5_Coding(sobelX[maxIdx],sobelY[maxIdx]));

      // int sX=sobelX[maxIdx]/sobelSaveRec;//X
      // if(sX>127)sX=127;
      // if(sX<-127)sX=-127;

      // int sY=sobelY[maxIdx]/sobelSaveRec;
      // if(sY>127)sY=127;
      // if(sY<-127)sY=-127;

      // sc[1]=sX+128;
      // sc[2]=sY+128;
      // // sc[1]=sc[2]=0;

      
      sc[1]=mag>>8;
      sc[2]=mag;

      // int16_t *scMag=(int16_t *)(&sc[1]);
      // *scMag=(int16_t)mag;
    }
  }

  acvDeleteFrame(sobel,1,0);
  return 0;
}


void SSE_Test()
{

  __m128 vector1 = _mm_set_ps(4.0, 3.0, 2.0, 1.0); // high element first, opposite of C array order.  Use _mm_setr_ps if you want "little endian" element order in the source.
  __m128 vector2 = _mm_set_ps(7.0, 8.0, 9.0, 0.0);

  __m128 sum = _mm_add_ps(vector1, vector2); // result = vector1 + vector 2

  vector1 = _mm_shuffle_ps(vector1, vector1, _MM_SHUFFLE(0,1,2,3));
  // vector1 is now (1, 2, 3, 4) (above shuffle reversed it)



  __m256i hello;
  // Construction from scalars or literals.
  __m256d a = _mm256_set_pd(1.0, 2.0, 3.0, 4.0);

  // Does GCC generate the correct mov, or (better yet) elide the copy
  // and pass two of the same register into the add? Let's look at the assembly.
  __m256d b = a;

  // Add the two vectors, interpreting the bits as 4 double-precision
  // floats.
  __m256d c = _mm256_add_pd(a, b);

  // Do we ever touch DRAM or will these four be registers?
  __attribute__ ((aligned (32))) double output[4];
  _mm256_store_pd(output, c);

  printf("%f %f %f %f\n",
         output[0], output[1], output[2], output[3]);
  return ;
}

int main(int argc, char** argv)
{
  SSE_Test();
  return 0;
  acvImage img;
  LoadIMGFile(&img,"data/template.png");

  if(1)
  {
    
    acvImage sobelMap;
    sobelMap.ReSize(&img);

    SOBEL_CODING_RGB(&img,&sobelMap);

    DIR_Coding_Spread_shift1x1(&sobelMap,&sobelMap);
    SaveIMGFile("data/sobelMap.png",&sobelMap);
  }
  if(0)
  {
    acvImage fast_map;
    int downSamp=3;
    int rad=8;
    fast_map.ReSize(img.GetWidth()/downSamp,img.GetHeight()/downSamp);
    FAST512(&img,&fast_map,downSamp,rad, rad*4,FAST_MAP_t,NULL);
    img.RGBToGray();

    SaveIMGFile("data/fast_map.png",&fast_map);
  }
  printf("END\n");
  return 0;
}


const int TABLE_SIN_512_LENGTH=512;
#define IDXWarp_SIN512(idx) (idx&(TABLE_SIN_512_LENGTH-1))

#define IDXWarp_COS512(idx) (IDXWarp_SIN512(idx+TABLE_SIN_512_LENGTH/4))
int TABLE_SIN_512[]=
{
   0,   6,  12,  18,  25,  31,  37,  43,  50,  56,  62,  68,  75,  81,  87,  93,
  99, 106, 112, 118, 124, 130, 136, 142, 148, 154, 160, 166, 172, 178, 184, 190,
 195, 201, 207, 213, 218, 224, 230, 235, 241, 246, 252, 257, 263, 268, 273, 279,
 284, 289, 294, 299, 304, 310, 314, 319, 324, 329, 334, 339, 343, 348, 353, 357,
 362, 366, 370, 375, 379, 383, 387, 391, 395, 399, 403, 407, 411, 414, 418, 422,
 425, 429, 432, 435, 439, 442, 445, 448, 451, 454, 457, 460, 462, 465, 468, 470,
 473, 475, 477, 479, 482, 484, 486, 488, 489, 491, 493, 495, 496, 498, 499, 500,
 502, 503, 504, 505, 506, 507, 508, 508, 509, 510, 510, 511, 511, 511, 511, 511,
 512, 511, 511, 511, 511, 511, 510, 510, 509, 508, 508, 507, 506, 505, 504, 503,
 502, 500, 499, 498, 496, 495, 493, 491, 489, 488, 486, 484, 482, 479, 477, 475,
 473, 470, 468, 465, 462, 460, 457, 454, 451, 448, 445, 442, 439, 435, 432, 429,
 425, 422, 418, 414, 411, 407, 403, 399, 395, 391, 387, 383, 379, 375, 370, 366,
 362, 357, 353, 348, 343, 339, 334, 329, 324, 319, 314, 310, 304, 299, 294, 289,
 284, 279, 273, 268, 263, 257, 252, 246, 241, 235, 230, 224, 218, 213, 207, 201,
 195, 190, 184, 178, 172, 166, 160, 154, 148, 142, 136, 130, 124, 118, 112, 106,
  99,  93,  87,  81,  75,  68,  62,  56,  50,  43,  37,  31,  25,  18,  12,   6,
   0,  -6, -12, -18, -25, -31, -37, -43, -50, -56, -62, -68, -75, -81, -87, -93,
 -99,-106,-112,-118,-124,-130,-136,-142,-148,-154,-160,-166,-172,-178,-184,-190,
-195,-201,-207,-213,-218,-224,-230,-235,-241,-246,-252,-257,-263,-268,-273,-279,
-284,-289,-294,-299,-304,-310,-314,-319,-324,-329,-334,-339,-343,-348,-353,-357,
-362,-366,-370,-375,-379,-383,-387,-391,-395,-399,-403,-407,-411,-414,-418,-422,
-425,-429,-432,-435,-439,-442,-445,-448,-451,-454,-457,-460,-462,-465,-468,-470,
-473,-475,-477,-479,-482,-484,-486,-488,-489,-491,-493,-495,-496,-498,-499,-500,
-502,-503,-504,-505,-506,-507,-508,-508,-509,-510,-510,-511,-511,-511,-511,-511,
-512,-511,-511,-511,-511,-511,-510,-510,-509,-508,-508,-507,-506,-505,-504,-503,
-502,-500,-499,-498,-496,-495,-493,-491,-489,-488,-486,-484,-482,-479,-477,-475,
-473,-470,-468,-465,-462,-460,-457,-454,-451,-448,-445,-442,-439,-435,-432,-429,
-425,-422,-418,-414,-411,-407,-403,-399,-395,-391,-387,-383,-379,-375,-370,-366,
-362,-357,-353,-348,-343,-339,-334,-329,-324,-319,-314,-310,-304,-299,-294,-289,
-284,-279,-273,-268,-263,-257,-252,-246,-241,-235,-230,-224,-218,-213,-207,-201,
-195,-190,-184,-178,-172,-166,-160,-154,-148,-142,-136,-130,-124,-118,-112,-106,
 -99, -93, -87, -81, -75, -68, -62, -56, -50, -43, -37, -31, -25, -18, -12,  -6,
};

/* 
*/

void FAST512(acvImage *src,acvImage *fast_map,int steps,int circleR, int circleR_segs,
 int (*FAST_MAP)(acvImage *img,int x,int y, std::vector <uint8_t* >&pixbuf, uint8_t* ret_PIX,void *params),
 void *params)
{
  if(circleR_segs>512)circleR_segs=512;

  int segs_div_4=circleR_segs/4;
  std::vector <uint8_t* >pixbuf(circleR_segs);
  int space=circleR+2;
  for (int i = space; i < src->GetHeight()-space; i+=steps)
  {
    for (int j = space; j < src->GetWidth()-space; j+=steps)
    {
      uint8_t *_centerPix = &(src->CVector[i][3*j]);
      uint8_t *fastMapPix =  &(fast_map->CVector[i/steps][(j/steps)*3]);
      int centerPix=_centerPix[0];

      for(int k=0;k<circleR_segs;k++)
      {
        int idx = k*TABLE_SIN_512_LENGTH/circleR_segs;
        int sin512=TABLE_SIN_512[IDXWarp_SIN512(idx)]*circleR/512;
        int cos512=TABLE_SIN_512[IDXWarp_COS512(idx)]*circleR/512;

        uint8_t *circPix = &(src->CVector[sin512+i][3*(cos512+j)]);
        pixbuf[k]=circPix;
      }
     
      FAST_MAP(src,j,i,pixbuf,fastMapPix,params);
      // fastMapPix[0]=(maxCount[2]>maxCount[1]?maxCount[2]:maxCount[1]);
      // fastMapPix[2]=0;//maxCount[1]*255/circleR_segs;
      // fastMapPix[1]=0;//maxCount[2]*255/circleR_segs;

    }
  }
}