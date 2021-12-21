#pragma once

// #include <Arduino.h>

#include "RingBuf.hpp"
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);
int mainX();

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);



#define VEC_SIZE 3

struct xVec
{
  int32_t vec[VEC_SIZE];
};


struct runBlock
{
  xVec from;
  xVec to;
  xVec vec;
  uint32_t steps;
  uint32_t cur_step;
  float vcur;
  float vcen;
  float vto;

};



class MStp{

public:
  RingBuf <runBlock> *blocks;
  uint32_t axis_pul;
  uint32_t axis_dir;
  uint32_t axis_RUNState;


  uint32_t TICK2SEC_BASE=10000;

  xVec curPos;
  xVec lastTarLoc;
  xVec preVec;
  float acc;
  float minSpeed;

  uint32_t T_next=0;
  uint32_t T_lapsed=0;

  MStp(RingBuf<runBlock> *_blocks)
  {


    curPos=lastTarLoc=preVec=(xVec){0};
    T_next=T_lapsed=0;
    minSpeed=20;
    acc=500;
    axis_pul=0;
    blocks=_blocks;
    axis_RUNState=1;
  }


  void VecTo(xVec VECTo,float speed);
  
  float delayRoundX=0;
  void BlockRunStep(runBlock &rb);

  virtual void BlockRunEffect();
  virtual void timerTask();
  bool timerRunning;
  virtual void stopTimer(){timerRunning=false;}
  virtual void startTimer(){timerRunning=true;}
};

