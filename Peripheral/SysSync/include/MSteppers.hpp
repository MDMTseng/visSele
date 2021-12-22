#pragma once

// #include <Arduino.h>

#include "RingBuf.hpp"
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);
int mainX();

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);


#ifndef MSTP_VEC_SIZE
#define MSTP_VEC_SIZE 2
#endif



struct xVec
{
  int32_t vec[MSTP_VEC_SIZE];
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

struct PulOffInfo
{
  uint32_t holdTimeStamp;
  uint32_t pin;

};



class MStp{

public:
  RingBuf <runBlock> *blocks;
  RingBuf_Static <uint32_t,10> PulOff;
  uint32_t axis_pul;
  uint32_t _axis_collectpul1;
  uint32_t _axis_collectpul2;
  uint32_t axis_collectpul;
  uint32_t axis_dir;
  uint32_t axis_RUNState;


  uint32_t TICK2SEC_BASE=1000*1000;


  uint32_t PULSE_ROUND_SHIFT=7;
  xVec curPos;
  xVec curPos_residue;
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
    minSpeed=2;
    acc=1;
    axis_pul=0;
    blocks=_blocks;
    axis_RUNState=1;
  }


  void VecTo(xVec VECTo,float speed);
  
  float delayRoundX=0;
  void BlockRunStep(runBlock &rb);

  // virtual void BlockRunEffect(uint32_t idxes)=0;
  virtual void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)=0;
  virtual void BlockDirEffect(uint32_t idxe)=0;
  virtual void blockPlayer();
  bool timerRunning;
  virtual void stopTimer(){timerRunning=false;}
  virtual void startTimer(){timerRunning=true;}





  uint32_t findNearstPulseIdx(uint32_t *ret_minResidue,int *ret_restCount);
  void delIdxResidue(uint32_t idxes);
  uint32_t taskRun();

  int pre_indexes=0;
  int tskrun_state=0;
  int tskrun_adj_debt=0;
  bool isInMidSec=false;
  int accT=0;
  int curT=0;
};

