#pragma once

// #include <Arduino.h>
#include <MSteppers_setup.h>
#include <MStepperStruct.hpp>

extern "C" {
#include <MStepperUtil.h>
}
#include "RingBuf.hpp"
#include <array>
#include <vector>
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
#include "PulseGenerator.hpp"


float* vecAdd(float* vr,float* v1,float* v2,int dim);
float* vecSub(float* vr,float* v1,float* v2,int dim);


xVec_f vecAdd(xVec_f v1,xVec_f v2);
xVec_f vecSub(xVec_f v1,xVec_f v2);
xVec vecAdd(xVec v1,xVec v2);
xVec vecSub(xVec v1,xVec v2);

enum MSTP_segment_type { seg_line=0,seg_arc=1,seg_wait=100 ,seg_instant_act=150 };

struct MSTP_segment;
struct MSTP_segment_adv_info;
typedef int (*MSTP_segment_CB)(MSTP_segment*,MSTP_segment_adv_info *);
struct MSTP_segment
{
  MSTP_segment_type type;
  float vcur;
  float vcen;
  float vto;

  float acc;
  float deacc;

  float* vec;
  float* sp;//start point

  float* aux_pt2;
  float* aux_pt3;
  float* aux_pt4;

  int vecL;

  float distanceStart;
  float distanceEnd;
  float Mdistance;
  float Edistance;


  int main_axis_idx;
  int virtual_axis_idx;
  float JunctionNormCoeff;
  float JunctionNormMaxDiff;
  float vto_JunctionMax;
  // uint32_t step_period;

  MSTP_segment_CB startCB,endCB;
  void* ctx;

};


struct MSTP_segment_adv_info
{

  float deaWeagle;  
  float magicSpace;
  float minSpeed;

  bool inInDAcc;
  float dstanceWent;
};


struct MSTP_axisSetup
{


  float ppmm;//pulse per mm
  float V_Factor;// pause/s

  float A_Factor;// pause/s^2

  float MaxVJump;
  // float maxSpeedInc;

  float V_Max;
  float A_Max;
};



struct MSTP_segment_extra_info
{
  float speed;
  int speedOnAxisIdx;
  float acc;
  float deacc;
  // float cornorR_unit;
  float cornorR;

};
char* toStr(const MSTP_SEG_PREFIX xVec &vec);

#define MSTP_ERR_CODE_PHY_LIMIT 1
#define MSTP_ERR_CODE_SOFT_LIMIT 2

class MStpV2;


class StpGroup
{
  public:
  StpGroup();
  // xVec pulse_latestRunVec={{0}};
  // xVec pulse_latestLoc={{0}};
  // xVec pulse_offset={{0}};

  // float latestFeedrate=100;
  // float latestAcc=100;
  // float latestDeacc=100;

  float main_junctionMaxSpeedJump=1;

  char bufferGCMD[100];
  int bufferGCMD_ID=-1;
  MSTP_axisSetup *axisSetup;
  RingBuf <struct MSTP_segment> segs;
  // bool VecAdd(xVec VECTo,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL);
  bool pushInPause(uint32_t pause_ms,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx=NULL);
  bool pushInInstant(MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx=NULL);
  bool pushInMoveVec(float* vec,MSTP_segment_extra_info *exinfo,int locDim,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx);

  virtual float* getLatestLocation()=0;
  virtual float* getLatestVec()=0;
  virtual void copyTo(float*dst,float*src)=0;
  // virtual float* getTmpVec(int idx)=0;

  virtual int GcodeParse(char **blkIdxes,int blkIdxesL)=0;
  virtual void getMotMoveVec(xVec_f *mot_vec_dst)=0;
  virtual void backward(xVec_f *mot_vec_dst,const float* loc_vec_src)=0;
  virtual void forward(float* loc_vec_dst,const xVec_f *mot_vec_src)=0;
  virtual void update()=0;//update in every system tick


  virtual void print(const char*)=0;
  virtual std::string vec_to_string(float*dst)=0;
  
  MSTP_segment_adv_info adv_info={
    .deaWeagle=1.2,
    .magicSpace=0,
    .minSpeed=500,
    .inInDAcc=false,
    .dstanceWent=0
  };
  // virtual void segAdvance();//update in every system tick
  virtual MSTP_segment* segAdvance(float &T);



  enum MSTP_segment_adv_state{
    ERROR,
    ERROR_TYPE_NOT_SUPPORT,
    ADV,
    FINISH
  };
  static MSTP_segment_adv_state segAdvance(float &T,MSTP_segment* trb,MSTP_segment_adv_info *info);
};


class MStpV2{

public:

  float updatePeriod_s=0.001;
  PulseGenerator *pGen;

  // std::array<StpGroup, 5> stepperGroup;

  std::vector<StpGroup*> stepperGroup;

  void SystemClear();

  MStpV2(PulseGenerator *pGen)
  {
    this->pGen=pGen;
  }

};

