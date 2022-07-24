#pragma once

// #include <Arduino.h>
#include <MSteppers_setup.h>
#include "RingBuf.hpp"
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);
int mainX();

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);

#define MSTP_SEG_PREFIX //volatile

#define PRT_FUNC_LEN 6
#ifdef X86_PLATFORM

#else
#include <Arduino.h>
#endif

struct xVec
{
  int32_t vec[MSTP_VEC_SIZE];
};
 xVec vecAdd(xVec v1,xVec v2);
 xVec vecSub(xVec v1,xVec v2);

enum MSTP_segment_type { seg_line=0,seg_wait=1 };

struct MSTP_segment
{
  void* ctx;
  float vcur;
  float vcen;
  float vto;
  float acc;
  float deacc;
  
  MSTP_segment_type type;
  xVec from;
  xVec to;
  xVec runvec;
  int main_axis_idx;
  int virtual_axis_idx;
  uint32_t steps;
  uint32_t cur_step;
  float JunctionNormCoeff;
  float JunctionNormMaxDiff;
  float vto_JunctionMax;
  uint32_t step_period;

};


struct MSTP_axisSetup
{
  float VirtualStep;//it's the effect multiplier that 
  float AccW;
  float MaxSpeedJumpW;
  // float maxSpeedInc;
};



struct MSTP_segment_extra_info
{
  float acc;
  float deacc;
};
char* toStr(const MSTP_SEG_PREFIX xVec &vec);

class MStp{

public:
  MSTP_SEG_PREFIX int segBufL;
  MSTP_SEG_PREFIX int segBufHeadIdx;
  MSTP_SEG_PREFIX int segBufTailIdx;
  MSTP_SEG_PREFIX MSTP_segment *segBuf;
  MSTP_SEG_PREFIX MSTP_segment *p_runSeg;
  // runBlock runBlk;

  // RingBuf_Static <uint32_t,10> PulOff;

  //preset cannot be touched
  uint32_t TICK2SEC_BASE=1000*1000;
  float main_acc;
  float minSpeed;
  float main_junctionMaxSpeedJump;
  float maxSpeedInc;
  MSTP_axisSetup axisInfo[MSTP_VEC_SIZE];

  int fatalErrorCode;
  void _FatalError(int errorCode,const char* errorText);
  virtual void FatalError(int errorCode,const char* errorText)=0;
  bool doCheckHardLimit=false;
  xVec limit1,limit2;

  xVec posvec;
  xVec curPos_c;
  xVec curPos_mod;
  // xVec curPos_residue;
  xVec lastTarLoc;


  uint32_t axis_pul_1st;
  uint32_t axis_pul_2nd;
  uint32_t axis_collectpul;
  uint32_t _axis_collectpul1;
  uint32_t axis_dir;
  float delayResidue=0;

  void SystemClear();


  void SegQ_Clear() MSTP_SEG_PREFIX;
  bool SegQ_IsEmpty() MSTP_SEG_PREFIX;
  bool SegQ_IsFull() MSTP_SEG_PREFIX;
  int SegQ_Size() MSTP_SEG_PREFIX;
  int SegQ_Space() MSTP_SEG_PREFIX;
  int SegQ_Capacity() MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX MSTP_segment* SegQ_Head(int idx=0) MSTP_SEG_PREFIX;
  bool SegQ_Head_Push() MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX MSTP_segment* SegQ_Tail(int idx=0) MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX bool SegQ_Tail_Pop() MSTP_SEG_PREFIX;


  void printSEGInfo();
  void StepperForceStop();

  MStp(MSTP_segment *buffer, int bufferL);
  bool AddWait(uint32_t period,int times=1, void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  bool VecAdd(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  bool VecTo(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  uint32_t T_next=0;
  void BlockRunStep(MSTP_SEG_PREFIX MSTP_segment *curSeg) MSTP_SEG_PREFIX;

  // virtual void BlockRunEffect(uint32_t idxes)=0;
  virtual void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)=0;
  virtual void BlockDirEffect(uint32_t idxes)=0;

  virtual void BlockPinInfoUpdate(uint32_t dir,uint32_t idxes_T,uint32_t idxes_R)=0;



  virtual void BlockInitEffect(MSTP_SEG_PREFIX MSTP_segment* blk)=0;
  virtual void BlockEndEffect(MSTP_SEG_PREFIX MSTP_segment* seg,MSTP_SEG_PREFIX MSTP_segment* n_seg)=0;
  virtual int MachZeroRet(uint32_t axis_index,uint32_t sensor_pin,int distance,int speed,void* context)=0;
  
  bool timerRunning=false;
  virtual void stopTimer() MSTP_SEG_PREFIX {timerRunning=false;}
  virtual void startTimer() MSTP_SEG_PREFIX{timerRunning=true;}

  int pre_indexes=0;
  int tskrun_state=0;
  bool isMidPulTrig=false;
  uint32_t taskRun();


  uint32_t findMidIdx(uint32_t from_idxes,uint32_t totSteps);


};

