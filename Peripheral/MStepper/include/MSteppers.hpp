#pragma once

// #include <Arduino.h>
#include <MSteppers_setup.h>
#include "RingBuf.hpp"
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);
int mainX();

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);



#define PRT_FUNC_LEN 6
#ifdef X86_PLATFORM

#define __PRT_D_(fmt,...) printf("%04d %.*s:d " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)
#define __PRT_I_(fmt,...) printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)


#else
#include <Arduino.h>
#define __PRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __PRT_I_(fmt,...) //Serial.printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)
#endif


struct xVec
{
  int32_t vec[MSTP_VEC_SIZE];
};

enum blockType { blk_line=0,blk_wait=1 };

struct runBlock
{
  void* ctx;
  float vcur;
  float vcen;
  float vto;
  blockType type;
  xVec from;
  xVec to;
  xVec runvec;
  uint32_t steps;
  uint32_t cur_step;
  float JunctionNormCoeff;
  float JunctionNormMaxDiff;
  float vto_JunctionMax;

};

struct PulOffInfo
{
  uint32_t holdTimeStamp;
  uint32_t pin;

};



class MStp{

public:
  RingBuf <runBlock> *blocks;
  // runBlock runBlk;
  runBlock *p_runBlk;

  // RingBuf_Static <uint32_t,10> PulOff;

  //preset cannot be touched
  MSTP_setup* axisSetup;
  uint32_t TICK2SEC_BASE=1000*1000;
  float acc;
  float minSpeed;
  float junctionMaxSpeedJump;
  float maxSpeedInc;






  uint32_t axis_pul;
  uint32_t axis_collectpul;
  uint32_t axis_dir;

  xVec posvec;
  xVec curPos_c;
  xVec curPos_mod;
  xVec curPos_residue;
  xVec lastTarLoc;

  uint32_t T_next=0;
  float delayResidue=0;

  void SystemClear();



  bool isQueueEmpty();
  void printBLKInfo();
  void StepperForceStop();
  MStp(RingBuf<runBlock> *_blocks, MSTP_setup *_axisSetup);


  bool VecAdd(xVec VECAdd,float speed,void* ctx=NULL);
  bool VecTo(xVec VECTo,float speed,void* ctx=NULL);

  void Delay(int interval,int intervalCount=1);
  
  void BlockRunStep(runBlock &rb);

  // virtual void BlockRunEffect(uint32_t idxes)=0;
  virtual void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)=0;
  virtual void BlockDirEffect(uint32_t idxes)=0;
  virtual void BlockInitEffect(runBlock* blk)=0;

  virtual void BlockEndEffect(runBlock* blk)=0;
  
  virtual void blockPlayer();
  bool timerRunning=false;
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

