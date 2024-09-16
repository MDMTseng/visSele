#ifndef MSteppersV2_h
#define MSteppersV2_h


//include int type
#include <stdint.h>
// #include <Arduino.h>
// 

extern "C" {
#include <MStepperUtil.h>
}



template <int VEC_DIM>
struct xnVec_f
{
  float vec[VEC_DIM];
};
template <int VEC_DIM>
struct xnVec_i
{
  int32_t vec[VEC_DIM];
};


#include "RingBuf.hpp"
#include <array>
#include <vector>
float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL);

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size);
char *int2bin(uint32_t a, int digits=8);

#define MSTP_SEG_PREFIX //volatile

#define PRT_FUNC_LEN 6
#ifdef X86_PLATFORM

#else
#include <Arduino.h>
#endif

// #include <ArduinoJson.h>
float* vecAdd(float* vr,float* v1,float* v2,int dim);
float* vecSub(float* vr,float* v1,float* v2,int dim);


enum MSTP_segment_type { seg_line=0,seg_arc=1,seg_wait=100 ,seg_instant_act=150 };


template<int VEC_DIM,int SEG_BUF_SIZE>
class StpGroup;


struct MSTP_segment_adv_info;

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
  float P_PreMult;

  // float ppmm;//pulse per mm
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
  float acc;
  float deacc;
  // float cornorR_unit;
  float cornorR;

};

#define MSTP_ERR_CODE_PHY_LIMIT 1
#define MSTP_ERR_CODE_SOFT_LIMIT 2

class MStpV2;

template<int VEC_DIM,int SEG_BUF_SIZE>
class StpGroup
{
  public:



  struct MSTP_segment;

  typedef  int (*MSTP_segment_CB)(MSTP_segment*,MSTP_segment_adv_info *);


  typedef  xnVec_f<VEC_DIM> CMDVec;
  typedef  xnVec_i<VEC_DIM> CMDiVec;
  struct MSTP_segment
  {
    MSTP_segment_type type;
    float vcur;
    float vmax;
    float vto;

    float acc;
    float deacc;

    CMDVec sp;
    CMDVec vec;

    CMDVec aux_pt2;
    CMDVec aux_pt3;
    CMDVec aux_pt4;


    float distanceStart;
    float distanceEnd;
    // float Mdistance;
    float Edistance;


    
    float JunctionNormCoeff;
    float JunctionNormMaxDiff;
    float vto_JunctionMax;
    // uint32_t step_period;

    MSTP_segment_CB startCB,endCB;
    void* ctx;
    int id;

  };


  RingBuf_Static <struct MSTP_segment,SEG_BUF_SIZE> segs;


  static const int vec_dim=VEC_DIM;
  StpGroup():segs()
  {

  }
  // xVec pulse_latestRunVec={{0}};
  // xVec pulse_latestLoc={{0}};
  // xVec pulse_offset={{0}};

  // float latestFeedrate=100;
  // float latestAcc=100;
  // float latestDeacc=100;

  float main_junctionMaxSpeedJump=1;
  
  MSTP_segment_adv_info adv_info={
    .deaWeagle=1.2,// when do deacc the end time estimation might not be accurate, so we need to alter the deacc number to match 
    .magicSpace=0,// when do deacc we want to play it safe, so we allow the speed down to vto to be a little bit earlier
    .minSpeed=500,
    .inInDAcc=false,//give a running flag to 
    .dstanceWent=0
  };

  // StaticJsonDocument <500>bufferJCMD;
  // char bufferJCMD_raw[200];
  // int bufferJCMD_ID=-1;

  MSTP_axisSetup axisSetup[VEC_DIM];
  bool pushInPause(int id,uint32_t pause_ms,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx=NULL);
  bool pushInInstant(int id,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx=NULL);
  bool pushInMoveVec(int id,CMDVec vec,CMDVec startPoint,MSTP_segment_extra_info exinfo,MSTP_segment_CB startCB=NULL,MSTP_segment_CB endCB=NULL,void* ctx=NULL);

  // virtual void inverse(float *mot_vec_dst,const float* loc_vec_src)=0;
  // virtual void forward(float* loc_vec_dst,const float *mot_vec_src)=0;
  // virtual void update()=0;//update in every system tick


  void RESET();

  // virtual void segAdvance();//update in every system tick
  MSTP_segment* segAdvance(float &T);

  bool getSegLocation(MSTP_segment* seg,CMDVec &retLoc);


  enum MSTP_segment_adv_state{
    ERROR,
    ERROR_TYPE_NOT_SUPPORT,
    ADV,
    FINISH
  };
  static MSTP_segment_adv_state segAdvance(float &T,MSTP_segment* trb,MSTP_segment_adv_info *info);




  static CMDVec sub(CMDVec v1,CMDVec v2);
  static CMDVec add(CMDVec v1,CMDVec v2);
  static CMDVec lerp(const CMDVec& a, const CMDVec& b, float t);
  static float dotProduct(CMDVec v1,CMDVec v2);
  static float magnitude(CMDVec v1);
  static float distance(CMDVec v1,CMDVec v2);
};


#endif
