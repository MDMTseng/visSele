#include <IMG_UTIL.hpp>







ImgPix::ImgPix(int width,int height,int channel)
{
  this->width=width;
  this->height=height;
  this->channel=channel;
  p=(uint8_t *)aligned_malloc(width*height*channel, 16);
}
ImgPix::~ImgPix()
{
  aligned_free(p);
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

