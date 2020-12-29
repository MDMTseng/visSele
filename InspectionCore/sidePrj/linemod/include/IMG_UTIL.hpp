#ifndef ___sadasd__UTIL_HPP
#define ___sadasd__UTIL_HPP

#include "acvImage_BasicTool.hpp"
#include "SSEUTIL.hpp"


void FAST512(acvImage *src,acvImage *fast_map,int steps,int circleR, int circleR_segs,
 int (*FAST_MAP)(acvImage *img,int x,int y, std::vector <uint8_t* >&pixbuf, uint8_t* ret_PIX,void *params),
 void *params);


void NShift(int v,int* max, int* sec, int* trd);



class ImgPix
{
  public:
  int width,height,channel;
  uint8_t *p=NULL;
  ImgPix(int width,int height,int channel);
  ~ImgPix();
  // :width(width),height(height){}
  
};

#endif