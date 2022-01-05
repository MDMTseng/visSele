#pragma once

// #include <Arduino.h>
#include <MSteppers_setup.h>
#include "RingBuf.hpp"
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);
int mainX();

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);



struct xVec
{
  int32_t vec[MSTP_VEC_SIZE];
};


struct runBlock
{
  xVec from;
  xVec to;
  xVec runvec;
  xVec posvec;
  uint32_t steps;
  uint32_t cur_step;
  float JunctionNormCoeff;
  float JunctionNormMaxDiff;
  bool isInDeAccState;
  float vcur;
  float vcen;
  float vto;
  float vto_JunctionMax;
  float vto_StopMax;

};

struct PulOffInfo
{
  uint32_t holdTimeStamp;
  uint32_t pin;

};



class MStp{

public:
  RingBuf <runBlock> *blocks;
  // RingBuf_Static <uint32_t,10> PulOff;

  //preset cannot be touched
  MSTP_setup* axisSetup;
  uint32_t TICK2SEC_BASE=1000*1000;
  uint32_t _PULSE_ROUND_SHIFT_=7;
  float acc;
  float minSpeed;
  float junctionMaxSpeedJump;
  float maxSpeedInc;






  uint32_t axis_pul;
  uint32_t axis_collectpul;
  uint32_t axis_dir;

  xVec curPos_c;
  xVec curPos_mod;
  xVec curPos_residue;
  xVec lastTarLoc;
  xVec preVec;

  uint32_t T_next=0;
  uint32_t T_lapsed=0;

  void SystemClear();



  void printBLKInfo();
  void StepperForceStop();
  MStp(RingBuf<runBlock> *_blocks, MSTP_setup *_axisSetup);
  runBlock *curBlk;


  void VecAdd(xVec VECAdd,float speed);
  void VecTo(xVec VECTo,float speed);
  float calcMajorSpeed(runBlock &rb);
  
  float delayRoundX=0;
  void BlockRunStep(runBlock &rb);

  // virtual void BlockRunEffect(uint32_t idxes)=0;
  virtual void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)=0;
  virtual void BlockDirEffect(uint32_t idxe)=0;
  virtual void blockPlayer();
  bool timerRunning;
  virtual void stopTimer(){timerRunning=false;}
  virtual void startTimer(){timerRunning=true;}





  uint32_t findMidIdx(uint32_t from_idxes,uint32_t totSteps);
  uint32_t findNearstPulseIdx(uint32_t *ret_minResidue,int *ret_restCount);
  void delIdxResidue(uint32_t idxes);
  uint32_t taskRun();

  int pre_indexes=0;
  int tskrun_state=0;
  int tskrun_adj_debt=0;
  bool isMidPulTrig=false;

  uint32_t _axis_collectpul1;
  uint32_t _axis_collectpul2;
  int accT=0;
  int curT=0;

  uint32_t save_pre_indexes=0;
  uint32_t save_mT=0;


};

