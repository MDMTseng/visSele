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
  float minv;
  float minv2;
  float acc;
  float deacc;
  
  MSTP_segment_type type;
  xVec from;
  xVec to;
  xVec runvec;
  int main_axis_idx;
  int virtual_axis_idx;
  uint32_t steps;
  uint32_t steps_real;
  uint32_t cur_step;
  
  uint32_t dir_bit;
  xVec runvec_abs;

  float JunctionNormCoeff;
  float JunctionNormMaxDiff;
  float vto_JunctionMax;
  uint32_t step_period;

};


struct MSTP_axisSetup
{
  
//VirtualStep is to addon the main axis selection logic and feed speed adjustment
//Every axis's "effect" on every step may vary, Note that the MSTP is pulse frequency based system.

//for example:
// vector virtual step -> is to compensate every axis "effects" speed difference 
// (the pulse speed will alter according to virtual step number)
// virtual step  [2  1 3]
// vector To     [50 60 3], speed 100   => the axis [1] is the physical main axis, if there is no virtual step
// virtual effect[100 60 9]  => the axis [0] is the  virtual main axis  so the speed should act on it(main axis selects the largest effect axis)
// phy speed on axis[0] is 100/2,  speed on axis[1] is 100/2*60/50

//Other case
// virtual step  [2  1  3]
// vector To     [20 60 3], speed 100   => the axis [1] is the physical main axis
// virtual effect[40 60 9]  => the axis [1] is the  virtual main axis  so the speed should act on it
// phy speed on axis[0] is 100/1*20/60 speed on axis[1] is 100

//In short higher(go more virtual steps), go slower, and under the hood this will affects which axis is the main axis;
  float VirtualStep;//it's the effect multiplier that 


//A bit like what VirtualStep does but for acceleration,
//Some axis allows higher acceleration .....
  float AccW;


//value of the MAX "pulse" speed(freq) jump on this axis
  float MaxSpeedJumpW;
  // float maxSpeedInc;

  float MaxSpeed;
};



struct MSTP_segment_extra_info
{
  int speedOnAxisIdx;
  float acc;
  float deacc;
};
char* toStr(const MSTP_SEG_PREFIX xVec &vec);

#define MSTP_ERR_CODE_PHY_LIMIT 1
#define MSTP_ERR_CODE_SOFT_LIMIT 2



class MStp{

public:
  MSTP_SEG_PREFIX int segBufL;
  MSTP_SEG_PREFIX int segBufHeadIdx;
  MSTP_SEG_PREFIX int segBufTailIdx;
  MSTP_SEG_PREFIX MSTP_segment *segBuf;
  MSTP_SEG_PREFIX MSTP_segment *p_runSeg;
  // SemaphoreHandle_t motionFinishMutex;
  // runBlock runBlk;

  // RingBuf_Static <uint32_t,10> PulOff;

  uint32_t latest_input_pins=0;
  bool endStopDetection=false;
  volatile bool endStopHitLock=false;
  volatile bool MTP_INIT_Lock=true;

  uint32_t endstopPins=0;//0xFF;//first 8 bits input are for end stop
  uint32_t endstopPins_normalState=0;//0xFF; //first 8 bits input(end stops ) are 1111 1111 at notmal state
  uint32_t endstopPins_hit=0;//0xFF; //first 8 bits input(end stops ) are 1111 1111 at notmal state

  
  //preset cannot be touched
  uint32_t TICK2SEC_BASE=1000*1000;
  float main_acc;
  // float SYS_MINSpeed;
  float main_junctionMaxSpeedJump;
  MSTP_axisSetup axisInfo[MSTP_VEC_SIZE];

  int fatalErrorCode;
  void _FatalError(int errorCode,const char* errorText);
  virtual void FatalError(int errorCode,const char* errorText)=0;
  bool doCheckSoftLimit=false;
  xVec limit1,limit2;

  xVec vec_abs;
  xVec curPos_c;
  xVec curPos_mod;
  // xVec curPos_residue;
  xVec lastTarLoc;


  // uint32_t axis_pul_1st;
  // uint32_t axis_pul_2nd;
  uint32_t axis_pul;
  uint32_t axis_dir;
  float delayResidue=0;

  void SystemClear();

  bool MT_SegQ_Clear_Flag=false;
  int SegQ_Size() MSTP_SEG_PREFIX;
  int SegQ_Space() MSTP_SEG_PREFIX;
  int SegQ_Capacity() MSTP_SEG_PREFIX;
  bool SegQ_IsEmpty() MSTP_SEG_PREFIX;
  bool SegQ_IsFull() MSTP_SEG_PREFIX;
protected:
  void SegQ_Clear() MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX MSTP_segment* SegQ_Head(int idx=0) MSTP_SEG_PREFIX;
  bool SegQ_Head_Push() MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX MSTP_segment* SegQ_Tail(int idx=0) MSTP_SEG_PREFIX;
  MSTP_SEG_PREFIX bool SegQ_Tail_Pop() MSTP_SEG_PREFIX;
public:

  void printSEGInfo();
  virtual void MT_StepperForceStop();
  void MT_SegQ_Clear();
  virtual void IT_StepperForceStop();

  MStp(MSTP_segment *buffer, int bufferL);
  bool AddWait(uint32_t period,int times=1, void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  bool VecAdd(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  bool VecTo(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  uint32_t T_next=0;
  void CalcNextStep(MSTP_SEG_PREFIX MSTP_segment *curSeg) MSTP_SEG_PREFIX;

  // virtual void BlockRunEffect(uint32_t idxes)=0;
  virtual void BlockPulEffect(MSTP_SEG_PREFIX MSTP_segment* seg,uint32_t idxes_T,uint32_t idxes_R)=0;
  virtual void BlockDirEffect(MSTP_SEG_PREFIX MSTP_segment* seg,uint32_t idxes)=0;

  virtual void BlockPinInfoUpdate(MSTP_SEG_PREFIX MSTP_segment* seg,uint32_t dir,uint32_t idxes_T,uint32_t idxes_R)=0;



  virtual void BlockInitEffect(MSTP_SEG_PREFIX MSTP_segment* blk)=0;
  virtual void BlockEndEffect(MSTP_SEG_PREFIX MSTP_segment* seg,MSTP_SEG_PREFIX MSTP_segment* n_seg)=0;

  virtual void BlockCtxReturn(MSTP_SEG_PREFIX MSTP_segment* seg)=0;
  virtual int MachZeroRet(uint32_t axis_index,uint32_t sensor_pin,int distance,int speed,void* context)=0;
  
  bool timerRunning=false;
  virtual void stopTimer() MSTP_SEG_PREFIX {timerRunning=false;}
  virtual void startTimer() MSTP_SEG_PREFIX{timerRunning=true;}
  virtual void setTimer(uint64_t) =0;

  int tskrun_state=0;
  virtual uint32_t taskRun();


  uint32_t findMidIdx(uint32_t from_idxes,uint32_t totSteps);


};

