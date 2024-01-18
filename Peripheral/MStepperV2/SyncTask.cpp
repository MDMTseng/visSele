
#include "main.hpp"
#include <string.h>
#include "MSteppersV2.hpp"
#include "GCodeParser.hpp"
#include "GCodeParserUtil.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include <Data_Layer_Protocol.hpp>
#include "driver/timer.h"
#include <array>


extern "C" {
#include "direct_spi.h"
}

#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)


void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc);
uint32_t g_step_trigger_edge=(1<<AXIS_IDX_X);//0xFFFFFFF;//each bits means trigger edge setting on each axis, 0 for posedge 1 for negedge
uint32_t g_dir_inv=0;//^(1<<AXIS_IDX_Z1)^(1<<AXIS_IDX_Z2)^(1<<AXIS_IDX_Z3)^(1<<AXIS_IDX_Z4);
bool doDataLog=false;
class MData_JR:public Data_JsonRaw_Layer
{
  
  public:
  MData_JR():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {
    doDataLog=false;
    return 0;
  } 
  int recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data=NULL,size_t dataL=0);
  
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  void connected(Data_Layer_IF* ch){}

  int send_data(int head_room,uint8_t *data,int len,int leg_room);
  void disconnected(Data_Layer_IF* ch){}

  int close(){return 0;}

  
  char dbgBuff[500];
  int dbg_printf(const char *fmt, ...);

  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


  GCodeParser::GCodeParser_Status insertCMD(StpGroup *stpG,const char* code);

};
MData_JR djrl;

// struct STPG_CMD_INFO{
//   int cmd_id;
//   int groupIndex;
//   char cmd[100];
// };
// RingBuf_Static <STPG_CMD_INFO,10> STPG_CMD_INFO_Buffer;

int HACK_cur_cmd_id=-1;


void G_LOG(const char* str)
{
  djrl.dbg_printf(str);
}


hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5
#define PIN_LED 2



//#define _HOMING_DBG_FLAG_ 50

#define PIN_DBG1 14
#define PIN_DBG2 27

#define PIN_DBG3 25
#define PIN_DBG4 26

#define PIN_DBG5 32
#define PIN_DBG6 33



#define PIN_PSENSOR_N 21

int pin_SH_165=17;
int pin_TRIG_595=5;

#define GPIOLS32_SET(PIN) GPIO.out_w1ts=1<<(PIN);
#define GPIOLS32_CLR(PIN) GPIO.out_w1tc=1<<(PIN);
  

#define SUBDIV (3200)
#define mm_PER_REV 95

spi_device_handle_t spi1=NULL;

enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,
  KEEP_RUN_UNTIL_ENC=10,
  KEEP_RUN_UNTIL_ENC_EARLY_STOP=11,
  HALT_UNTIL_TRACKING_SATTLED=14,

  FADE_IN_TRACKING=50,

};


enum Mstp2CommInfo_Type{
  trigInfo=1000,
  ext_log=1001,
  respFrame=1002,
};



struct Mstp2CommInfo{//TODO: rename the infoQ to be more versatile
  Mstp2CommInfo_Type type;

  //trigInfo
  char strinfo[40];
  int trig_id;
  float curFreq;
  int curReelLocation;

  //log
  // char log[40];

  //respFrame
  bool isAck;
  int resp_id;
};

RingBuf_Static<struct Mstp2CommInfo,20,uint8_t> Mstp2CommInfoQ;



struct MSTP_SegCtx_IOCTRL{
  uint32_t PORT=0,S=0;
  int32_t P=0,T=0;
};


struct MSTP_SegCtx_INPUTMON{
  uint32_t PINS,PIN_NS;
  uint32_t existField;
  bool doMonitor;
};


struct MSTP_SegCtx_OnTimeReply{
  int id;
  bool isAck;
};



struct MSTP_SegCtx_RunUntilEnc{
  int tar_ENC;
  int stepsMult;
  xVec tarVec;
};


struct MSTP_SegCtx_RunUntilEnc_EarlyStop{
  int tar_ENC;
  uint32_t axis_vec;
};




struct MSTP_SegCtx_TargetTrackingTransition{
  int tar_ENC;
  uint32_t axis_vec;
};

struct MSTP_SegCtx{
  MSTP_SegCtx(){}
  ~MSTP_SegCtx(){}

  bool isProcessed;
  MSTP_SegCtx_TYPE type;
  union {
    struct MSTP_SegCtx_IOCTRL IO_CTRL;
    struct MSTP_SegCtx_INPUTMON INPUT_MON;
    struct MSTP_SegCtx_OnTimeReply ON_TIME_REP;
    struct MSTP_SegCtx_RunUntilEnc RUN_UNTIL_ENC;
    struct MSTP_SegCtx_RunUntilEnc_EarlyStop KEEP_RUN_UNTIL_ENC_EARLY_STOP;
  };
  char TTAG[40];
  int TID;
};


const int SegCtxSize=10;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);


class GCodeParser_M:public GCodeParser{
public:
  xVec pulse_offset=(xVec){0};
  GCodeParser_M()
  {

  }
  
  JsonDocument *p_jnote;
  void putJSONNote(JsonDocument* jnote){this->p_jnote=jnote;}
  StpGroup* p_stpG;
  void putTargetStepperGroup(StpGroup* stpG)
  {
    this->p_stpG=stpG;
  }


  // inline void SetxVec_fToxVec(xVec &vec_dst,xVec_f &vec_src)
  // {
  //   for(int i=0;i<MSTP_VEC_SIZE;i++)
  //   {
  //     if(vec_src.vec[i]!=NAN)
  //       vec_dst.vec[i]=(int32_t)(vec_src.vec[i]);
  //   }
  // }

  GCodeParser::GCodeParser_Status parseCMD(char **blks, char blkCount)
  {

    
    if(blkCount==0)
      return GCodeParser::GCodeParser_Status::LINE_EMPTY;
    GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;



    if(p_stpG)
    {
      auto &stpG=*p_stpG;
      auto ret=stpG.GcodeParse(blks,blkCount);

      return (GCodeParser::GCodeParser_Status)ret;
    }




    char *cblk=blks[0];
    // int cblkL=blks[1]-blks[0];

    blks++;//move to next block
    blkCount--;


    if(cblk[0]=='G' && p_stpG!=NULL)
    {
      
    }
    else if(cblk[0]=='M')
    {

    }
    else if(cblk[0]!=';' &&cblk[0]!='('  )
    {
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }


    return retStatus;
    // return GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }
  void onError(int code)
  {
    
  }
};
 



#define _TICK2SEC_BASE_ (10*1000*1000)

extern void __digitalWrite(uint8_t pin, uint8_t val)
{
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
}





uint64_t SystemTick=0;
inline uint64_t getCurTick()
{
  return SystemTick+timerRead(timer);
}



static SemaphoreHandle_t SeqCalcTaskLock;

class PulseGenerator_M :public PulseGenerator
{
public:
  void bufferEmpty()
  {
    BaseType_t xHigherPriorityTaskWoken;
    xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(SeqCalcTaskLock,&xHigherPriorityTaskWoken);


  }
  void timerNext(uint32_t period_us)
  {
    uint64_t T=period_us*10;
    timer_set_alarm_value(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, (uint64_t)T);
  }

  uint32_t pre_f_step=0;
  uint32_t pre_f_dir=0;
  void pinInfoSet(uint32_t stp,uint32_t dir)
  {
    pre_f_step=stp;
    pre_f_dir=dir;
  }
  void pinUpdate()
  {



    digitalWrite(PIN_DBG1, pre_f_dir&(1<<3));
    digitalWrite(PIN_DBG2, pre_f_step&(1<<3));

    // digitalWrite(PIN_DBG3, pre_f_dir&(1<<1));
    // digitalWrite(PIN_DBG4, pre_f_step&(1<<1));
    
    // digitalWrite(PIN_DBG5, pre_f_dir&(1<<2));
    // digitalWrite(PIN_DBG6, pre_f_step&(1<<2));
  }
};

PulseGenerator_M PG_M;

MStpV2 mstpV2(&PG_M);
uint32_t cp0_regs[18];

void IRAM_ATTR onTimer()
{
  // enable FPU
  // xthal_set_cpenable(1);
  // // Save FPU registers
  // xthal_save_cp0(cp0_regs);
  // // uint32_t nextT=100;
  // // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);

  PG_M.nextStep();


  // Restore FPU
  // xthal_restore_cp0(cp0_regs);
  // // and turn it back off
  // xthal_set_cpenable(0);
  // 


}


StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;



StaticJsonDocument <500>doc;
StaticJsonDocument  <500>retdoc;


float txfreq=2.4;
float tyfreq=2.4;

static int CB_Count=0;

class StpGroup_RX:public StpGroup
{ 
  typedef  xnVec_f<3> RXVec;

  // static const int OUTPUT_HIST_DIV=1;
  // RingBuf_Static<RXVec,1024/OUTPUT_HIST_DIV> outputHist;

  std::array<RXVec,40> startPtBuffer;
  std::array<RXVec,40> vecBuffer;

  std::array<RXVec,40> aux_pt2;
  std::array<RXVec,40> aux_pt3;
  std::array<RXVec,40> aux_pt4;


  std::array<struct MSTP_segment,40> segsBuffer;

  MSTP_axisSetup _axisSetup[sizeof(RXVec)/sizeof(float)];

  const int LOC_DIM=sizeof(RXVec)/sizeof(float);

  RXVec latestLocation={{0}};
  RXVec latestVec={{0}};


  RXVec tmpVec1={{0}};
  RXVec tmpVec2={{0}};


public:
  StpGroup_RX():StpGroup()
  {

    // memset(outputHist.buff,0,outputHist.capacity()*sizeof(RXVec));
    

    axisSetup=_axisSetup;
    for(int i=0;i<segsBuffer.size();i++)
    {
      segsBuffer[i].vec=(vecBuffer[i].vec);
      segsBuffer[i].sp=(startPtBuffer[i].vec);

      segsBuffer[i].aux_pt2=(aux_pt2[i].vec);
      segsBuffer[i].aux_pt3=(aux_pt3[i].vec);
      segsBuffer[i].aux_pt4=(aux_pt4[i].vec);


    }

    segs.RESET(segsBuffer.data(),segsBuffer.size());


    float ppmm=50;

    axisSetup[0].ppmm=ppmm;
    axisSetup[0].A_Factor=1;
    axisSetup[0].V_Factor=1;
    axisSetup[0].MaxVJump=1;
    axisSetup[0].V_Max=2000;
    axisSetup[0].A_Max=100000;



    axisSetup[1]=axisSetup[2]=axisSetup[0];


    axisSetup[1].ppmm=(4000)/(140);
    axisSetup[2].ppmm=(1000*5)/(160*2*M_PI);
    adv_info.minSpeed=5;

    design_notch_filter(5.85,1000,0.9);
    // design_lowpass_filter(10,1000);
  }

  void print(const char* str)
  {
    djrl.dbg_printf(str);
  }
  float* getLatestLocation()
  {
    return latestLocation.vec;
  }
  float* getLatestVec()
  {
    return latestVec.vec;
  }

  float preDistWent=0;
  float prePercent=0;

  uint32_t preCount=0;
  uint32_t updateCount=0;
  MSTP_segment* preSeg=0;


  RXVec latestCalcAdvLocation={{0}};
  RXVec latestAdvLocation={{0}};
  float pre_ratio=0;





  enum TrackingState{
    NOP=0,
    Syncing=1,
    InSync=2,
    Leaving=3,

  };



  int TRAJECT_ID=-1;
  float TRAJECT_params[10];
  MSTP_segment_adv_info trackingAdvInfo;
  MSTP_segment trackingMovParam;
  TrackingState trackingState=TrackingState::NOP;
  RXVec virtualTrackingCenter={{0}};




// Filter coefficients=
double b[3];
double a[3];

double x[3] = {0}; // Input signal buffer
double y[3] = {0}; // Output signal buffer

void design_notch_filter(double f0, double fs,float R=0.99) {
    double omega0 = 2 * M_PI * f0 / fs;
    
    a[0] = 1;
    a[1] = -2 * R * cos(omega0);
    a[2] = R * R;
    
    b[0] = (1 + a[2]) / 2;
    b[1] = a[1];
    b[2] = b[0];
}

void design_lowpass_filter(double f0, double fs) {
    double alpha = 2 * M_PI * f0 / fs;
    double denom = 1 + alpha;

    b[0] = alpha / denom;
    b[1] = alpha / denom;
    a[0] = 1;
    a[1] = (1 - alpha) / denom;

    b[2] = a[2]=0;
}


float iir_filter(float input) {
    // Shift old samples
    for (int i = 2; i > 0; i--) {
        x[i] = x[i-1];
        y[i] = y[i-1];
    }

    // Store current sample
    x[0] = input;

    // Compute the output
    float output = 0;
    for (int i = 0; i < 3; i++) {
        output += b[i] * x[i];
    }

    for (int i = 1; i < 3; i++) {
        output -= a[i] * y[i];
    }


    // Store current output
    y[0] = output;

    return output;
}
float alpha=0.01;
float preInput=0;
float iir_lp_filter(float input) {
    // Shift old samples
    for (int i = 2; i > 0; i--) {
        x[i] = x[i-1];
        y[i] = y[i-1];
    }

    float output=input*alpha+y[1]*(1-alpha);
    // Store current output
    y[0] = output;

    return output;
}

  RXVec cubicBezier_comp(MSTP_segment *seg,float dstanceWent)
  {
    
    float ratio=dstanceWent/(seg->Edistance);



    float cb_coeff[4];
    cubicBezier_TCoeff(ratio,cb_coeff);
    RXVec ret={{0}};
    cubicBezier_Vec(ret.vec,seg->sp,seg->aux_pt2,seg->aux_pt3,seg->aux_pt4,LOC_DIM,cb_coeff);

    return ret;
  }

  uint32_t updCounter=0;
  void update()//every system tick, update the location
  {

    updateCount=(updateCount+1)%100000;
    do{
      float T=mstpV2.updatePeriod_s;
      MSTP_segment*curSeg= segAdvance(T);//The T will be updated to the used time
      if(curSeg==NULL)
      {
        pre_ratio=0;
        // adv_info.dstanceWent=0;
        break;
      }

      RXVec newAdvLocation={{0}};
      if(curSeg->type!=MSTP_segment_type::seg_line && curSeg->type!=MSTP_segment_type::seg_arc)
      {
        bool goMove=false;
        if(curSeg->type==MSTP_segment_type::seg_wait)
        {
          if(curSeg->ctx!=NULL)
          {
            MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
            if(p_res->type==MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED )
            {
              if(trackingState==TrackingState::NOP || trackingState==TrackingState::InSync)//sattled
              {
                goMove=true;

                adv_info.dstanceWent=curSeg->distanceEnd=-1;
                T=0;//0 time is used
              }
            }






          }

          
        }
        pre_ratio=0;
        if(goMove==false)
        {
          break;
        }
      }
      else if(curSeg->type==MSTP_segment_type::seg_line)
      {
        float ratio=adv_info.dstanceWent/(curSeg->Edistance);
        for(int i=0;i<LOC_DIM;i++)
        {
          newAdvLocation.vec[i]=curSeg->vec[i]*(ratio)+curSeg->sp[i];
        }
        latestCalcAdvLocation=newAdvLocation;


      }
      else  if(curSeg->type==MSTP_segment_type::seg_arc)
      {
        latestCalcAdvLocation=newAdvLocation=cubicBezier_comp(curSeg,adv_info.dstanceWent);


      }



      




      if(T!=mstpV2.updatePeriod_s)
      {

        
        if(0){

          Mstp2CommInfo* Qhead=NULL;
          {
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
            Qhead->type=Mstp2CommInfo_Type::ext_log;
            

            uint32_t curAddr=(0xFFF&(uint32_t)curSeg);
            int strPadding=0;
            strPadding+= sprintf(Qhead->strinfo+strPadding,"EL_%d:%0.2f,%0.2f  ",curSeg->type,
              latestCalcAdvLocation.vec[0],latestCalcAdvLocation.vec[1]);

            strPadding+= sprintf(Qhead->strinfo+strPadding,"v:%0.1f:%0.1f>%0.1f  curAddr:%d",curSeg->vcur,curSeg->vcen,curSeg->vto,curAddr);


            Mstp2CommInfoQ.pushHead();

          }


          {
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
  
            int strPadding=0;

            strPadding+= sprintf(Qhead->strinfo+strPadding,"d:%0.1f/%0.1f",
            adv_info.dstanceWent,curSeg->Edistance);

            strPadding+= sprintf(Qhead->strinfo+strPadding,"a:%0.1f,%0.1f",
            curSeg->acc,
            curSeg->deacc);

            Mstp2CommInfoQ.pushHead();
          }


        }




        do{
          float leftT=mstpV2.updatePeriod_s-T;
          T=leftT;
          MSTP_segment*nxtSeg= segAdvance(T);
          if(nxtSeg==NULL || (nxtSeg->type!=MSTP_segment_type::seg_line && nxtSeg->type!=MSTP_segment_type::seg_arc))
          {
            break;
          }


          if(nxtSeg->type==MSTP_segment_type::seg_line)
          {

            float ratio=adv_info.dstanceWent/(nxtSeg->Edistance);
            for(int i=0;i<LOC_DIM;i++)
            {
              newAdvLocation.vec[i]=nxtSeg->vec[i]*(ratio)+nxtSeg->sp[i];
            }

            latestCalcAdvLocation=newAdvLocation;

          }
          else  if(nxtSeg->type==MSTP_segment_type::seg_arc)
          {
            latestCalcAdvLocation=newAdvLocation=cubicBezier_comp(nxtSeg,adv_info.dstanceWent);


          }

          if(0){

            Mstp2CommInfo* Qhead=NULL;

            {
              while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
    
              int strPadding=0;
              strPadding+= sprintf(Qhead->strinfo+strPadding,"SL_%d:%0.2f,%0.2f  ",nxtSeg->type,
                latestCalcAdvLocation.vec[0],latestCalcAdvLocation.vec[1]);

              strPadding+= sprintf(Qhead->strinfo+strPadding,"v:%0.1f:%0.1f>%0.1f",nxtSeg->vcur,nxtSeg->vcen,nxtSeg->vto);

              Mstp2CommInfoQ.pushHead();


            }
            if(1){
              while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
    
              int strPadding=0;

              strPadding+= sprintf(Qhead->strinfo+strPadding,"d:%0.1f/%0.1f",
              adv_info.dstanceWent,nxtSeg->Edistance);

              strPadding+= sprintf(Qhead->strinfo+strPadding,"a:%0.1f,%0.1f",
              nxtSeg->acc,
              nxtSeg->deacc);

              Mstp2CommInfoQ.pushHead();
            }


          }




        }while(0);

      }
      
    }while(0);
  


    latestAdvLocation=latestCalcAdvLocation;
    if(trackingState!=TrackingState::NOP)
    {
      float T = mstpV2.updatePeriod_s;
      
      float ratio;
      if(trackingState==TrackingState::InSync)
      {
        ratio=1;
      }
      else if(trackingAdvInfo.dstanceWent!=trackingMovParam.distanceEnd)
      {
        segAdvance(T,&trackingMovParam,&trackingAdvInfo);
        ratio=trackingAdvInfo.dstanceWent/trackingMovParam.distanceEnd;
      }
      else
      {
        ratio=1;
      }



      if(trackingState==TrackingState::Leaving ||trackingState==TrackingState::NOP)
      {
        if(ratio==1)
        {
          trackingState=TrackingState::NOP;
        }
        ratio=1-ratio;
      }
      else
      {

        if(ratio==1)
        {
          trackingState=TrackingState::InSync;
        }
      }

      RXVec targetLoc={0};  

      if(TRAJECT_ID==1)
      {
        float t=updateCount*0.0001;
        float w=t*2*3.1415926*TRAJECT_params[1]+TRAJECT_params[2];
        w=fmod(w, 2*3.1415926);
        targetLoc.vec[0]=TRAJECT_params[0]*sin(w)+TRAJECT_params[3];
        targetLoc.vec[1]=TRAJECT_params[0]*cos(w)+TRAJECT_params[4];
        targetLoc.vec[2]=NAN;
      }
      else if (TRAJECT_ID==2)
      {
        float t=updateCount*0.0001;
        float w=t*2*3.1415926*TRAJECT_params[1]+TRAJECT_params[2];
        w=fmod(w, 2*3.1415926);
        targetLoc.vec[0]=TRAJECT_params[0]*cos(w)+TRAJECT_params[3];
        targetLoc.vec[1]=TRAJECT_params[0]*sin(w*2)+TRAJECT_params[4];
        targetLoc.vec[2]=NAN;
      }

      for(int i=0;i<LOC_DIM;i++)
      {
        float tv=targetLoc.vec[i];


        float lcv=latestCalcAdvLocation.vec[i];
        if(tv!=tv)
        {
          latestAdvLocation.vec[i]=lcv;
          continue;
        }

        float vtcv=virtualTrackingCenter.vec[i];


        float r1v=tv+lcv-vtcv;
        float r0v=lcv;


        latestAdvLocation.vec[i]=r1v*ratio+r0v*(1-ratio);
      }
      //=virtualTrackingCenter.vec[0]+(latestCalcAdvLocation.vec[0]-virtualTrackingCenter.vec[0])*ratio;

      // float ratio=adv_info.dstanceWent/curSeg->distance;
      // for(int i=0;i<LOC_DIM;i++)
      // {
      //   newAdvLocation.vec[i]=curSeg->vec[i]*(ratio)+curSeg->sp[i];
      // }
      // latestAdvLocation=newAdvLocation;
    }
    else
    {
      latestAdvLocation=latestCalcAdvLocation;
    }



    // updCounter++;
    // if((updCounter&(OUTPUT_HIST_DIV-1))==0)
    // {
    //   if(outputHist.size()>=outputHist.capacity()-2)
    //   {
    //     outputHist.consumeTail();
    //   }
      
    //   outputHist.pushHead(latestAdvLocation);
    // }


    // auto fX_N= outputHist.getHead(1+(int)(1000/OUTPUT_HIST_DIV/(txfreq*2)));
    // auto fX_2N= outputHist.getHead(1+(int)(1000*2/OUTPUT_HIST_DIV/(txfreq*2)));



    // auto fY_N= outputHist.getHead(1+(int)(1000/OUTPUT_HIST_DIV/(tyfreq*2)));
    // auto fY_2N= outputHist.getHead(1+(int)(1000*2/OUTPUT_HIST_DIV/(tyfreq*2)));


    // if(fX_N!=NULL && fX_2N!=NULL  && fY_N!=NULL && fY_2N!=NULL)
    // {
      
    //   latestAdvLocation.vec[0]=(fX_2N->vec[0]+2*fX_N->vec[0]+latestAdvLocation.vec[0])/4;
    //   latestAdvLocation.vec[1]=(fY_2N->vec[1]+2*fY_N->vec[1]+latestAdvLocation.vec[1])/4;
    //   // latestAdvLocation.vec[3]=latestAdvLocation.vec[3];
    // }


  }

  // float* getTmpVec(int idx)
  // {
  //   if(idx==0)
  //     return tmpVec1.vec;
  //   else if(idx==1)
  //     return tmpVec2.vec;
  //   return NULL;
  // }


  void copyTo(float*dst,float*src)
  {
    memcpy(dst,src,sizeof(RXVec));
  }


  std::string vec_to_string(float*dst)
  {
    std::string ret="[";
    for(int i=0;i<LOC_DIM;i++)
    {
      ret+=std::to_string(dst[i]);

      if(i<LOC_DIM-1)
        ret+=",";
    }

    return ret+"]";
  }



  virtual void getMotMoveVec(xVec_f *mot_vec_dst)
  {
    backward(mot_vec_dst,latestAdvLocation.vec);
  }
  void backward(xVec_f *mot_vec_dst,const float* loc_vec_src){
    
    // float *loc_vec=(float*)loc_vec_src;
    
    // float x=loc_vec[0];//in mm
    // float y=loc_vec[1];
    // float z=loc_vec[2];
    // float R=62.5;
    // float RotAngle=asinf(z/R);
    // x+=R*(cosf(RotAngle));



    // mot_vec_dst->vec[1]=-x*axisSetup[0].ppmm;
    // mot_vec_dst->vec[0]=-y*axisSetup[1].ppmm;
    // mot_vec_dst->vec[2]=-RotAngle/M_PI*850;

    for(int i=0;i<LOC_DIM;i++)
    {
      mot_vec_dst->vec[i]=loc_vec_src[i]*axisSetup[i].ppmm;
    }
    
  }
  void forward(float* loc_vec_dst,const xVec_f *mot_vec_src)
  {
    for(int i=0;i<LOC_DIM;i++)
    {
      loc_vec_dst[i]=mot_vec_src->vec[i]/axisSetup[i].ppmm;
    }
  }


  // static int fadeInTracking(MSTP_segment* seg,MSTP_segment_adv_info *info)
  // {
  //   MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
  //   if(p_res==NULL)return -1;

  //   seg->ctx=NULL;
  //   sctx_pool.returnResource(p_res);
  //   return 0;
  // }
  static int instEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }
  static int IOCtrlEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=100;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    
    if(p_res->IO_CTRL.S){
      GPIOLS32_SET(p_res->IO_CTRL.P);
    }
    else
    {
      GPIOLS32_CLR(p_res->IO_CTRL.P);
    }
    
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int OnTimeRepEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=10000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;



    struct Mstp2CommInfo tinfo={
    .type=Mstp2CommInfo_Type::respFrame,
    .isAck=p_res->ON_TIME_REP.isAck,
    .resp_id=p_res->ON_TIME_REP.id
    };

    {
      Mstp2CommInfo* Qhead=NULL;
      while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
      *Qhead=tinfo;
      Mstp2CommInfoQ.pushHead();
    }


    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }

  static int CtxRecycleEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=1000000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    
    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }



  MSTP_segment_extra_info latestExtInfo={
    .speed=100,
    .speedOnAxisIdx=-1,
    .acc=1000,
    .deacc=-1000,
    .cornorR=0,

  };
  int GcodeParse(char **blkIdxes,int blkIdxesL)
  {

    
    if(blkIdxesL==0)
      return GCodeParser::GCodeParser_Status::LINE_EMPTY;
    GCodeParser::GCodeParser_Status retStatus=GCodeParser::GCodeParser_Status::LINE_EMPTY;




    char *cblk=blkIdxes[0];
    // int cblkL=blks[1]-blks[0];

    blkIdxes++;//move to next block
    blkIdxesL--;


    if(cblk[0]=='G')
    {
      if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))
      {
        __PRT_D_("G1 baby!!!\n");


        RXVec vec_coord=latestLocation;
        float x=NAN;
        FindFloat("X",blkIdxes,blkIdxesL,x);
        float y=NAN;
        FindFloat("Y",blkIdxes,blkIdxesL,y);
        float z=NAN;
        FindFloat("Z",blkIdxes,blkIdxesL,z);

        if(x==x)vec_coord.vec[0]=x;
        if(y==y)vec_coord.vec[1]=y;
        if(z==z)vec_coord.vec[2]=z;


        // xVec_f vec_mot=foward(vec_coord);

        MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);

        if(exinfo.acc!=exinfo.acc)exinfo.acc=latestExtInfo.acc;
        if(exinfo.deacc!=exinfo.deacc)exinfo.deacc=latestExtInfo.deacc;
        if(exinfo.speed!=exinfo.speed)exinfo.speed=latestExtInfo.speed;

        if(exinfo.cornorR!=exinfo.cornorR)exinfo.cornorR=latestExtInfo.cornorR;
        if(exinfo.speedOnAxisIdx!=exinfo.speedOnAxisIdx)exinfo.speedOnAxisIdx=latestExtInfo.speedOnAxisIdx;

        // __UPRT_I_("vec_coord:%f %f %f  exinfo:f:%f a:%f d:%f aidx:%d\n",vec_coord.vec[0],vec_coord.vec[1],vec_coord.vec[2],
        // exinfo.speed,exinfo.acc,exinfo.deacc,exinfo.speedOnAxisIdx);
        

        RXVec moveVec;
        vecSub(moveVec.vec,vec_coord.vec,latestLocation.vec,LOC_DIM);
        
        // __UPRT_I_("vec:%s",vec_to_string(moveVec.vec).c_str());
        pushInMoveVec(moveVec.vec,&exinfo,LOC_DIM,NULL,NULL,NULL);
        latestLocation=vec_coord;

        latestExtInfo=exinfo;
                // ReadGVecData(blks,blkCount,vec_f,&exinfo);

        // ConvUnitVecToPulseVec(&vec_f,&exinfo);
        
        // xVec newLoc=stpG.pulse_latestLoc;
        // SetxVec_fToxVec(newLoc,vec_f);
        // // xVec goVec=vecSub(newLoc,stpG.pulse_latestLoc);
        
        // stpG.MoveTo(newLoc,NULL,&exinfo);

        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "G4 ") || CheckHead(cblk, "G04 "))
      {

        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        if(P==P && P>=0)
        {
          pushInPause(P,NULL,NULL,NULL);
          return  GCodeParser::GCodeParser_Status::TASK_OK;
        }
        else
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
      }

      else if(CheckHead(cblk, "G_TT "))
      {

        if(trackingState!=TrackingState::NOP)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }



        RXVec vec_coord={{NAN}};
        float x=NAN;
        FindFloat("X",blkIdxes,blkIdxesL,x);
        float y=NAN;
        FindFloat("Y",blkIdxes,blkIdxesL,y);
        float z=NAN;
        FindFloat("Z",blkIdxes,blkIdxesL,z);


        if(x!=x || y!=y || z!=z)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }




        vec_coord.vec[0]=x;
        vec_coord.vec[1]=y;
        vec_coord.vec[2]=z;


        // xVec_f vec_mot=foward(vec_coord);


        TRAJECT_ID=-1;
        {
          char str[10];

          int paramLen=0;

          for(int k=0;k<sizeof(TRAJECT_params)/sizeof(TRAJECT_params[0]);k++)
          {
            sprintf(str,"PARAM%d_",k);
            float n=NAN;
            FindFloat(str,blkIdxes,blkIdxesL,n);
            TRAJECT_params[k]=n;
            if(n!=n)
            {
              break;
            }

            paramLen++;
          }

          

        }


        {

          float n=NAN;
          FindFloat("TRAJECT_ID",blkIdxes,blkIdxesL,n);

          if(n!=n)
          {
            return GCodeParser::GCodeParser_Status::TASK_FAILED;
          }

          TRAJECT_ID=n;
        }

        MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);



        virtualTrackingCenter=vec_coord;
        memset(&trackingMovParam,0,sizeof(trackingMovParam));
        trackingMovParam.acc=exinfo.acc;
        trackingMovParam.deacc=exinfo.deacc;
        trackingMovParam.vcen=exinfo.speed;
        trackingMovParam.vcur=0;
        trackingMovParam.vto=0;
        trackingMovParam.distanceStart=0;
        trackingMovParam.distanceEnd=
        trackingMovParam.Mdistance=
        trackingMovParam.Edistance=1000;
      

        trackingAdvInfo=adv_info;
        trackingAdvInfo.dstanceWent=0;
        trackingAdvInfo.inInDAcc=false;
        trackingAdvInfo.minSpeed=100;

        trackingState=TrackingState::Syncing;





        // pushInInstant(NULL,instEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      
      
      else if(CheckHead(cblk, "G_TT_UNSYNC "))
      {

        if(trackingState!=TrackingState::InSync)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }



        // xVec_f vec_mot=foward(vec_coord);

        MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);
        memset(&trackingMovParam,0,sizeof(trackingMovParam));
        trackingMovParam.acc=exinfo.acc;
        trackingMovParam.vcen=exinfo.speed;
        trackingMovParam.vcur=0;
        trackingMovParam.vto=0;
        trackingMovParam.deacc=exinfo.deacc;
        trackingMovParam.distanceStart=0;
        trackingMovParam.distanceEnd=
        trackingMovParam.Mdistance=
        trackingMovParam.Edistance=1000;
      




        trackingAdvInfo=adv_info;
        trackingAdvInfo.inInDAcc=false;
        trackingAdvInfo.dstanceWent=0;
        trackingAdvInfo.minSpeed=100;


        trackingState=TrackingState::Leaving;





        // pushInInstant(NULL,instEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      
      else if(CheckHead(cblk, "G_TT_DBG"))
      {

        // djrl.dbg_printf("ts:%d    %0.2f/%0.2f",trackingState, trackingAdvInfo.dstanceWent,trackingMovParam.distance);
        // djrl.dbg_printf("inInDAcc:%d weagle:%0.2f   %0.2f> %0.2f~%0.2f",
        //   trackingAdvInfo.inInDAcc,trackingAdvInfo.deaWeagle,
        //   trackingMovParam.vcur,
        //   trackingMovParam.vcen,
        //   trackingMovParam.vto);

        djrl.dbg_printf("sctx_pool.size:%d  CB_Count:%d",sctx_pool.size(),CB_Count);

        djrl.dbg_printf("latestAdvLocation %0.2f,%0.2f,%0.2f",latestAdvLocation.vec[0],latestAdvLocation.vec[1],latestAdvLocation.vec[2]);
        
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      
      else if(CheckHead(cblk, "G_TT_SATTLED"))
      {
        MSTP_SegCtx *p_res;

        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return GCodeParser::GCodeParser_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED;

        pushInPause(-1,NULL,CtxRecycleEndCB,(void*)p_res);

        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      
      
    }
    else if(cblk[0]=='M')
    {
      if(CheckHead(cblk, "M42 "))
      { 
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float S=NAN;
        FindFloat("S",blkIdxes,blkIdxesL,S);
        if(P!=P || S!=S)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }


        MSTP_SegCtx *p_res;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;

        p_res->IO_CTRL.P=P;
        p_res->IO_CTRL.S=S;
        pushInInstant(NULL,IOCtrlEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }


      else if(CheckHead(cblk, "M400.BLOCKING"))
      { 
        
        while(segs.size())
        {
          yield();
        }

        while( trackingState!=TrackingState::InSync && trackingState!=TrackingState::NOP) 
        {
          yield();
        }
        
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "M400"))
      { 

        MSTP_SegCtx *p_res;
        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return GCodeParser::GCodeParser_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::ON_TIME_REPLY;

        p_res->ON_TIME_REP.id=HACK_cur_cmd_id;
        p_res->ON_TIME_REP.isAck=true;
        pushInInstant(NULL,OnTimeRepEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
      }

      else if(CheckHead(cblk, "M_ZVShaper"))
      { 
        float f0=NAN;

        FindFloat("fx0=",blkIdxes,blkIdxesL,f0);
        if(f0==f0)
        {
          txfreq=f0;
        }

        FindFloat("fy0=",blkIdxes,blkIdxesL,f0);
        if(f0==f0)
        {
          tyfreq=f0;
        }


        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      else if(CheckHead(cblk, "M_NOTCH"))
      { 
        float f0=NAN;
        float R=NAN;

        FindFloat("f0=",blkIdxes,blkIdxesL,f0);
        FindFloat("R=",blkIdxes,blkIdxesL,R);

        if(f0!=f0 || R!=R)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }

        design_notch_filter(f0,1000,R);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
      else if(CheckHead(cblk, "M_LOWPASS"))
      { 
        float f0=NAN;
        FindFloat("f0=",blkIdxes,blkIdxesL,f0);

        if(f0!=f0)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
        design_lowpass_filter(f0,1000);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }
    }
    return  GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }

};

StpGroup_RX SG_RX;



class StpGroup_SIMP:public StpGroup
{ 
  typedef  xnVec_f<3> RXVec;

  // static const int OUTPUT_HIST_DIV=1;
  // RingBuf_Static<RXVec,1024/OUTPUT_HIST_DIV> outputHist;

  std::array<RXVec,40> startPtBuffer;
  std::array<RXVec,40> vecBuffer;


  std::array<struct MSTP_segment,40> segsBuffer;

  MSTP_axisSetup _axisSetup[sizeof(RXVec)/sizeof(float)];

  const int LOC_DIM=sizeof(RXVec)/sizeof(float);

  RXVec latestLocation={{0}};
  RXVec latestVec={{0}};


  int homingStatus=-1;
  int homingCMDID=-1;

public:
  StpGroup_SIMP():StpGroup()
  {

    // memset(outputHist.buff,0,outputHist.capacity()*sizeof(RXVec));
    

    axisSetup=_axisSetup;
    for(int i=0;i<segsBuffer.size();i++)
    {
      segsBuffer[i].vec=(vecBuffer[i].vec);
      segsBuffer[i].sp=(startPtBuffer[i].vec);

      segsBuffer[i].aux_pt2=
      segsBuffer[i].aux_pt3=
      segsBuffer[i].aux_pt4=NULL;


    }

    segs.RESET(segsBuffer.data(),segsBuffer.size());


    float ppmm=50;

    axisSetup[0].ppmm=ppmm;
    axisSetup[0].A_Factor=1;
    axisSetup[0].V_Factor=1;
    axisSetup[0].MaxVJump=1;
    axisSetup[0].V_Max=2000;
    axisSetup[0].A_Max=100000;



    axisSetup[1]=axisSetup[2]=axisSetup[0];


    axisSetup[1].ppmm=(4000)/(140);
    axisSetup[2].ppmm=(1000*5)/(160*2*M_PI);
    adv_info.minSpeed=5;

    // design_lowpass_filter(10,1000);
  }

  void print(const char* str)
  {
    djrl.dbg_printf(str);
  }
  float* getLatestLocation()
  {
    return latestLocation.vec;
  }
  float* getLatestVec()
  {
    return latestVec.vec;
  }

  float preDistWent=0;
  float prePercent=0;

  uint32_t preCount=0;
  uint32_t updateCount=0;
  MSTP_segment* preSeg=0;


  RXVec latestCalcAdvLocation={{0}};
  RXVec latestAdvLocation={{0}};
  float pre_ratio=0;





  enum TrackingState{
    NOP=0,
    Syncing=1,
    InSync=2,
    Leaving=3,

  };



  int TRAJECT_ID=-1;
  float TRAJECT_params[10];
  MSTP_segment_adv_info trackingAdvInfo;
  MSTP_segment trackingMovParam;
  TrackingState trackingState=TrackingState::NOP;
  RXVec virtualTrackingCenter={{0}};




  int TESTCOuntdown=0;
  float homingSpeed=0;
  void homingACT()
  {

    float T=mstpV2.updatePeriod_s;
    if(homingStatus==0)
    {
      TESTCOuntdown=-1;
      homingStatus=1;
    }
    switch (homingStatus)
    {
    case 1:
      /* code */
      if(TESTCOuntdown==-1)
      {
        TESTCOuntdown=500;
        homingSpeed=0.1;
      }
      else if(TESTCOuntdown)
      {
        latestAdvLocation.vec[0]-=homingSpeed*T;
        homingSpeed+=5;
        if(homingSpeed>30)homingSpeed=30;

        if(digitalRead(PIN_PSENSOR_N)==1)
          TESTCOuntdown=0;
      }
      else
      {
        if(homingSpeed<=0.1)
        {
          TESTCOuntdown=-1;
          homingStatus=2;
        }
        else
        {
          latestAdvLocation.vec[0]-=homingSpeed*T;
          homingSpeed-=0.4;
        }

      }
      break;

    case 2:
      /* code */

      if(TESTCOuntdown==-1)
      {
        TESTCOuntdown=100;
        homingSpeed=5;
      }
      else if(TESTCOuntdown)
      {
        latestAdvLocation.vec[0]+=homingSpeed*T;
        if(digitalRead(PIN_PSENSOR_N)==0)
          TESTCOuntdown=0;
      }
      else
      {
        TESTCOuntdown=-1;
        homingStatus=3;
      }
      break;

    case 3:
      /* code */

      if(TESTCOuntdown==-1)
      {
        TESTCOuntdown=1000;
        homingSpeed=0.5;
      }
      else if(TESTCOuntdown)
      {
        latestAdvLocation.vec[0]-=homingSpeed/axisSetup[0].ppmm;
        if(digitalRead(PIN_PSENSOR_N)==1)
          TESTCOuntdown=0;
      }
      else
      {
        preMotloc.vec[0]=0;//reset
        latestAdvLocation.vec[0]=0;
        TESTCOuntdown=-1;
        homingStatus=-1;
      }
      break;


    
    default:
      homingStatus=-1;
      break;
    }

    if(homingStatus==-1)//finished
    {
      

      struct Mstp2CommInfo tinfo={
      .type=Mstp2CommInfo_Type::respFrame,
      .isAck=true,
      .resp_id=homingCMDID
      };

      {
        Mstp2CommInfo* Qhead=NULL;
        while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
        *Qhead=tinfo;
        Mstp2CommInfoQ.pushHead();
      }

    }

  }


  uint32_t updCounter=0;
  void update()//every system tick, update the location
  {
    updateCount=(updateCount+1)%100000;
    if(homingStatus!=-1)
    {
      homingACT();
      latestCalcAdvLocation=latestAdvLocation;
      return;
    }
    do{
      float T=mstpV2.updatePeriod_s;
      MSTP_segment*curSeg= segAdvance(T);//The T will be updated to the used time
      if(curSeg==NULL)
      {
        pre_ratio=0;
        // adv_info.dstanceWent=0;
        break;
      }

      RXVec newAdvLocation={{0}};
      if(curSeg->type!=MSTP_segment_type::seg_line && curSeg->type!=MSTP_segment_type::seg_arc)
      {
        bool goMove=false;
        if(curSeg->type==MSTP_segment_type::seg_wait)
        {
          if(curSeg->ctx!=NULL)
          {
            MSTP_SegCtx *p_res=(MSTP_SegCtx *)curSeg->ctx;
            if(p_res->type==MSTP_SegCtx_TYPE::HALT_UNTIL_TRACKING_SATTLED )
            {
              if(trackingState==TrackingState::NOP || trackingState==TrackingState::InSync)//sattled
              {
                goMove=true;

                adv_info.dstanceWent=curSeg->distanceEnd=-1;
                T=0;//0 time is used
              }
            }






          }

          
        }
        pre_ratio=0;
        if(goMove==false)
        {
          break;
        }
      }
      else if(curSeg->type==MSTP_segment_type::seg_line)
      {
        float ratio=adv_info.dstanceWent/(curSeg->Edistance);
        for(int i=0;i<LOC_DIM;i++)
        {
          newAdvLocation.vec[i]=curSeg->vec[i]*(ratio)+curSeg->sp[i];
        }
        latestCalcAdvLocation=newAdvLocation;


      }



      if(T!=mstpV2.updatePeriod_s)
      {

        
        if(0){

          Mstp2CommInfo* Qhead=NULL;
          {
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
            Qhead->type=Mstp2CommInfo_Type::ext_log;
            

            uint32_t curAddr=(0xFFF&(uint32_t)curSeg);
            int strPadding=0;
            strPadding+= sprintf(Qhead->strinfo+strPadding,"EL_%d:%0.2f,%0.2f  ",curSeg->type,
              latestCalcAdvLocation.vec[0],latestCalcAdvLocation.vec[1]);

            strPadding+= sprintf(Qhead->strinfo+strPadding,"v:%0.1f:%0.1f>%0.1f  curAddr:%d",curSeg->vcur,curSeg->vcen,curSeg->vto,curAddr);


            Mstp2CommInfoQ.pushHead();

          }


          {
            while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
  
            int strPadding=0;

            strPadding+= sprintf(Qhead->strinfo+strPadding,"d:%0.1f/%0.1f",
            adv_info.dstanceWent,curSeg->Edistance);

            strPadding+= sprintf(Qhead->strinfo+strPadding,"a:%0.1f,%0.1f",
            curSeg->acc,
            curSeg->deacc);

            Mstp2CommInfoQ.pushHead();
          }


        }




        do{
          float leftT=mstpV2.updatePeriod_s-T;
          T=leftT;
          MSTP_segment*nxtSeg= segAdvance(T);
          if(nxtSeg==NULL || (nxtSeg->type!=MSTP_segment_type::seg_line && nxtSeg->type!=MSTP_segment_type::seg_arc))
          {
            break;
          }


          if(nxtSeg->type==MSTP_segment_type::seg_line)
          {

            float ratio=adv_info.dstanceWent/(nxtSeg->Edistance);
            for(int i=0;i<LOC_DIM;i++)
            {
              newAdvLocation.vec[i]=nxtSeg->vec[i]*(ratio)+nxtSeg->sp[i];
            }

            latestCalcAdvLocation=newAdvLocation;

          }

          if(0){

            Mstp2CommInfo* Qhead=NULL;

            {
              while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
    
              int strPadding=0;
              strPadding+= sprintf(Qhead->strinfo+strPadding,"SL_%d:%0.2f,%0.2f  ",nxtSeg->type,
                latestCalcAdvLocation.vec[0],latestCalcAdvLocation.vec[1]);

              strPadding+= sprintf(Qhead->strinfo+strPadding,"v:%0.1f:%0.1f>%0.1f",nxtSeg->vcur,nxtSeg->vcen,nxtSeg->vto);

              Mstp2CommInfoQ.pushHead();


            }
            if(1){
              while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);Qhead->type=Mstp2CommInfo_Type::ext_log;
    
              int strPadding=0;

              strPadding+= sprintf(Qhead->strinfo+strPadding,"d:%0.1f/%0.1f",
              adv_info.dstanceWent,nxtSeg->Edistance);

              strPadding+= sprintf(Qhead->strinfo+strPadding,"a:%0.1f,%0.1f",
              nxtSeg->acc,
              nxtSeg->deacc);

              Mstp2CommInfoQ.pushHead();
            }


          }




        }while(0);

      }
      
    }while(0);
  


    latestAdvLocation=latestCalcAdvLocation;


  }

  // float* getTmpVec(int idx)
  // {
  //   return NULL;
  // }


  void copyTo(float*dst,float*src)
  {
    memcpy(dst,src,sizeof(RXVec));
  }


  std::string vec_to_string(float*dst)
  {
    std::string ret="[";
    for(int i=0;i<LOC_DIM;i++)
    {
      ret+=std::to_string(dst[i]);

      if(i<LOC_DIM-1)
        ret+=",";
    }

    return ret+"]";
  }


  xVec preMotloc={0};
  virtual void getMotMoveVec(xVec_f *mot_vec_dst)
  {
    xVec_f curMotLoc={0};
    backward(&curMotLoc,latestAdvLocation.vec);
    mot_vec_dst->vec[3]=(int32_t)curMotLoc.vec[0]-preMotloc.vec[0];
    preMotloc.vec[0]=curMotLoc.vec[0];
  }
  void backward(xVec_f *mot_vec_dst,const float* loc_vec_src){
    for(int i=0;i<LOC_DIM;i++)
    {
      mot_vec_dst->vec[i]=loc_vec_src[i]*axisSetup[i].ppmm;
    }
  }
  void forward(float* loc_vec_dst,const xVec_f *mot_vec_src)
  {
    for(int i=0;i<LOC_DIM;i++)
    {
      loc_vec_dst[i]=mot_vec_src->vec[i]/axisSetup[i].ppmm;
    }
  }


  // static int fadeInTracking(MSTP_segment* seg,MSTP_segment_adv_info *info)
  // {
  //   MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
  //   if(p_res==NULL)return -1;

  //   seg->ctx=NULL;
  //   sctx_pool.returnResource(p_res);
  //   return 0;
  // }



  static int instEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=1;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;

    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }
  static int IOCtrlEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=100;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    
    if(p_res->IO_CTRL.S){
      GPIOLS32_SET(p_res->IO_CTRL.P);
    }
    else
    {
      GPIOLS32_CLR(p_res->IO_CTRL.P);
    }
    
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }


  static int OnTimeRepEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {
    CB_Count+=10000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;



    struct Mstp2CommInfo tinfo={
    .type=Mstp2CommInfo_Type::respFrame,
    .isAck=p_res->ON_TIME_REP.isAck,
    .resp_id=p_res->ON_TIME_REP.id
    };

    {
      Mstp2CommInfo* Qhead=NULL;
      while( (Qhead=Mstp2CommInfoQ.getHead()) ==NULL);
      *Qhead=tinfo;
      Mstp2CommInfoQ.pushHead();
    }


    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }

  static int CtxRecycleEndCB(MSTP_segment* seg,MSTP_segment_adv_info *info)
  {

    CB_Count+=1000000;
    MSTP_SegCtx *p_res=(MSTP_SegCtx *)seg->ctx;
    if(p_res==NULL)return -1;
    
    p_res->isProcessed=true;
    seg->ctx=NULL;
    sctx_pool.returnResource(p_res);
    return 0;
  }



  MSTP_segment_extra_info latestExtInfo={
    .speed=100,
    .speedOnAxisIdx=-1,
    .acc=1000,
    .deacc=-1000,
    .cornorR=0,

  };
  int GcodeParse(char **blkIdxes,int blkIdxesL)
  {

    
    if(blkIdxesL==0)
      return GCodeParser::GCodeParser_Status::LINE_EMPTY;
    GCodeParser::GCodeParser_Status retStatus=GCodeParser::GCodeParser_Status::LINE_EMPTY;




    char *cblk=blkIdxes[0];
    // int cblkL=blks[1]-blks[0];

    blkIdxes++;//move to next block
    blkIdxesL--;


    if(cblk[0]=='G')
    {
      if(CheckHead(cblk, "G28"))//G28 GO HOME!!!:
      {
        homingStatus=0;
        homingCMDID=HACK_cur_cmd_id;

        return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
      }
      else if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))
      {
        __PRT_D_("G1 baby!!!\n");


        RXVec vec_coord=latestLocation;
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float R1_=NAN;
        FindFloat("R1_",blkIdxes,blkIdxesL,R1_);

        if(P==P)vec_coord.vec[0]=P;
        if(R1_==R1_)vec_coord.vec[1]=R1_;
        // if(z==z)vec_coord.vec[2]=z;


        // xVec_f vec_mot=foward(vec_coord);

        MSTP_segment_extra_info exinfo = ReadSegment_extra_info(blkIdxes,blkIdxesL);

        if(exinfo.acc!=exinfo.acc)exinfo.acc=latestExtInfo.acc;
        if(exinfo.deacc!=exinfo.deacc)exinfo.deacc=latestExtInfo.deacc;
        if(exinfo.speed!=exinfo.speed)exinfo.speed=latestExtInfo.speed;

        if(exinfo.cornorR!=exinfo.cornorR)exinfo.cornorR=latestExtInfo.cornorR;
        if(exinfo.speedOnAxisIdx!=exinfo.speedOnAxisIdx)exinfo.speedOnAxisIdx=latestExtInfo.speedOnAxisIdx;

        // __UPRT_I_("vec_coord:%f %f %f  exinfo:f:%f a:%f d:%f aidx:%d\n",vec_coord.vec[0],vec_coord.vec[1],vec_coord.vec[2],
        // exinfo.speed,exinfo.acc,exinfo.deacc,exinfo.speedOnAxisIdx);
        

        RXVec moveVec;
        vecSub(moveVec.vec,vec_coord.vec,latestLocation.vec,LOC_DIM);
        
        // __UPRT_I_("vec:%s",vec_to_string(moveVec.vec).c_str());
        pushInMoveVec(moveVec.vec,&exinfo,LOC_DIM,NULL,NULL,NULL);
        latestLocation=vec_coord;

        latestExtInfo=exinfo;
                // ReadGVecData(blks,blkCount,vec_f,&exinfo);

        // ConvUnitVecToPulseVec(&vec_f,&exinfo);
        
        // xVec newLoc=stpG.pulse_latestLoc;
        // SetxVec_fToxVec(newLoc,vec_f);
        // // xVec goVec=vecSub(newLoc,stpG.pulse_latestLoc);
        
        // stpG.MoveTo(newLoc,NULL,&exinfo);

        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "G4 ") || CheckHead(cblk, "G04 "))//pause
      {

        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        if(P==P && P>=0)
        {
          pushInPause(P,NULL,NULL,NULL);
          return  GCodeParser::GCodeParser_Status::TASK_OK;
        }
        else
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
      }
    }
    else if(cblk[0]=='M')
    {
      if(CheckHead(cblk, "M42 "))//IO ctrl
      { 
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float S=NAN;
        FindFloat("S",blkIdxes,blkIdxesL,S);
        if(P!=P || S!=S)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }


        MSTP_SegCtx *p_res;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;

        p_res->IO_CTRL.P=P;
        p_res->IO_CTRL.S=S;
        pushInInstant(NULL,IOCtrlEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }


      else if(CheckHead(cblk, "M42.MODE "))//IO ctrl
      { 
        float P=NAN;
        FindFloat("P",blkIdxes,blkIdxesL,P);
        float M=NAN;
        FindFloat("M",blkIdxes,blkIdxesL,M);
        if(P!=P || M!=M)
        {
          return GCodeParser::GCodeParser_Status::TASK_FAILED;
        }
        pinMode(P,M);
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "M400.BLOCKING"))
      { 
        
        while(segs.size())
        {
          yield();
        }

        while( trackingState!=TrackingState::InSync && trackingState!=TrackingState::NOP) 
        {
          yield();
        }
        
        return  GCodeParser::GCodeParser_Status::TASK_OK;
      }

      else if(CheckHead(cblk, "M400"))
      { 

        MSTP_SegCtx *p_res;
        int retryCount=0;
        while((p_res=sctx_pool.applyResource())==NULL)
        {
          if(retryCount++>100)
            return GCodeParser::GCodeParser_Status::TASK_FAILED;
          yield();
        }
        p_res->type=MSTP_SegCtx_TYPE::ON_TIME_REPLY;

        p_res->ON_TIME_REP.id=HACK_cur_cmd_id;
        p_res->ON_TIME_REP.isAck=true;
        pushInInstant(NULL,OnTimeRepEndCB,(void*)p_res);
        return  GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP;
      }
    }
    return  GCodeParser::GCodeParser_Status::TASK_UNSUPPORTED;
  }

};

StpGroup_SIMP SG_SIMP;


GCodeParser_M gcpm;

bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK);

int MData_JR::recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data,size_t dataL)
{
  for(int i=0;i<buffIdx;i++)
  {
    if(dataBuff[i]=='"')
      dataBuff[i]='\'';
  }  
  dataBuff[buffIdx]='\0';
  // doDataLog=true;

  // if(recv_data)
  //   dbg_printf("recv_ERROR:%d %s dat:%s",errorcode,dataBuff,string((char*)recv_data,0,9).c_str());
  // else 
  //   dbg_printf("recv_ERROR:%d %s",errorcode,dataBuff);

  return 0;
}


int GCMD_Magic_SafeSpace=3;

GCodeParser::GCodeParser_Status MData_JR::insertCMD(StpGroup *stpG,const char* code)
{
  gcpm.putJSONNote(&retdoc);
  gcpm.putTargetStepperGroup(stpG);
  auto grep=gcpm.runLine(code);
  // if(grep==GCodeParser::GCodeParser_Status::TASK_OK)
  // {
  //   rspAck=true;
  // }
  // else if(grep==GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP)
  // {
  //   rspAck=true;
  //   doRsp=false;
  // }
  // else
  // {
  //   rspAck=false;
  // }
  gcpm.putJSONNote(NULL);
  gcpm.putTargetStepperGroup(NULL);

  return grep;
}


int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    doc.clear();
    retdoc.clear();
    DeserializationError error = deserializeJson(doc, raw);
    bool rspAck=false;
    bool doRsp=false;

    const char* type = doc["type"];
    // const char* id = doc["id"];

    if(strcmp(type,"G")==0)
    {
      HACK_cur_cmd_id=-1;
      doRsp=true;
      
      do
      {

        if(doc["id"].is<int>()==false)break;
        if(doc["group"].is<int>()==false)break;


        if(doc["code"].is<const char*>()==false)break;

        int groupIdx=doc["group"];

        if(groupIdx<0)break;
        if(groupIdx>=mstpV2.stepperGroup.size())break;

        auto &stpG=*(mstpV2.stepperGroup[groupIdx]);

        if(stpG.bufferGCMD_ID>=0)
        {
          rspAck=false;
          break;
        }


        int cmd_id=HACK_cur_cmd_id=doc["id"];
        const char* code = doc["code"];


        if(stpG.segs.space()>GCMD_Magic_SafeSpace)//gives a magic safe space
        {

          doRsp=true;


          auto retSt=insertCMD(&stpG,code);
          if(retSt==GCodeParser::GCodeParser_Status::TASK_OK)
          {
            rspAck=true;
          }
          else if(retSt==GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP)
          {
            rspAck=true;
            doRsp=false;
          }
          else
          {
            rspAck=false;
          }
        }
        else
        {
          
          doRsp=false;
          strcpy(stpG.bufferGCMD,code);
          stpG.bufferGCMD_ID=cmd_id;

        }





        // rspAck=true;
      } while (false);
      

    }
    
    if(strcmp(type,"TTTT")==0)
    {
      doRsp=true;
      if(doc["vec"].is<JsonArray>() && doc["seg_time_us"].is<int>())
      {
        xVec vec={0};
        JsonArray array = doc["vec"].as<JsonArray>();
        int i=0;
        for(JsonVariant v : array) {
          vec.vec[i++]=v.as<int>();
        }

        while(PG_M.segSteps_next!=0)//wait until the buffer is empty
        {
          yield();
        }
        PG_M.loadNext(doc["seg_time_us"],vec);
        rspAck=true;

      }
    }
    else if(strcmp(type,"get_running_stat")==0)
    {

      // {
      //   JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");

      //   for(int i=0;i<ERROR_HIST.size();i++)
      //   {
      //     jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
      //   }
      // }


      // JsonObject jCountInfo  = retdoc.createNestedObject("count");
      // jCountInfo["SEL1"]=SEL1_Count;
      // jCountInfo["SEL2"]=SEL2_Count;
      // jCountInfo["SEL3"]=SEL3_Count;
      // jCountInfo["NA"]=NA_Count;

      //current state
      // retdoc["state"]=(int)sysinfo.state;

      // retdoc["plateFreq"]=SYS_TAR_FREQ;//SYS_CUR_FREQ;


      // retdoc["plateFreq"]=NA_Count;


      doRsp=rspAck=true;

    }
    else if(strcmp(type,"RESET")==0)
    {
      return msg_printf("RESET_OK","");
    }
    else if(strcmp(type,"ask_JsonRaw_version")==0)
    {
      
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return this->rsp_JsonRaw_version();
    }
    else if(strcmp(type,"rsp_JsonRaw_version")==0)
    {
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return 0;
    }
    else if(strcmp(type,"PING")==0)
    {
      retdoc["type"]="PONG"; 
      doRsp=rspAck=true;
    }
    else if(strcmp(type,"get_setup")==0)
    {

      retdoc["type"]="get_setup";
      retdoc["ver"]="0.5.0";
      retdoc["name"]="CNC_1";






      
      genMachineSetup(retdoc);

      
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"set_setup")==0)
    {
      retdoc["type"]="set_setup";
      
      setMachineSetup(doc);
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"PIN_CONF")==0)
    {
      
      if(doc["pin"].is<int>())
      {
        int pinNo = doc["pin"];

      
        if(doc["mode"].is<int>())
        {
          int mode= doc["mode"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(mode)
          {
            case 0:pinMode(pinNo, INPUT);break;
            case 1:pinMode(pinNo, OUTPUT);break;
            case 2:pinMode(pinNo, INPUT_PULLUP);break;
            case 3:pinMode(pinNo, INPUT_PULLDOWN);break;
          }
        }
        else if(doc["output"].is<int>())
        {
          int output= doc["output"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(output)
          {
            case -2://analog
            {
              int value=analogRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;

              break;
            }
            case -1://digital
            {
              int value=digitalRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;
              break;
            }
            case 0:digitalWrite(pinNo, LOW);break;
            case 1:digitalWrite(pinNo, HIGH);break;
          }
        }
      }
      else
      {

      }
      


    
      // retdoc["type"]="DBG_PRT";
      // // retdoc["msg"]=doc;
      // retdoc["error"]=error.code();
      // retdoc["id"]=doc["id"];
      // uint8_t buff[300];
      // int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      // send_json_string(0,buff,slen,0);
    }
    else if(strcmp(type,"BYE")==0)
    {
      doRsp=rspAck=true;

    }      
    
    
    else if(AUX_Task_Try_Read(doc,type,retdoc,doRsp,rspAck))
    {
    }


    if(doRsp)
    {
      retdoc["id"]=(int)doc["id"];
      retdoc["ack"]=rspAck;
      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      send_json_string(0,buff,slen,0);

    }
  }


  return 0;


}
int MData_JR::send_data(int head_room,uint8_t *data,int len,int leg_room){
  Serial.write(data,len);
  return 0;
}

int MData_JR::dbg_printf(const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"dbg\":\"");
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

int MData_JR::msg_printf(const char *type,const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"type\":\"%s\",\"data\":\"",type);
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

void MData_JR::loop()
{
  for(int i=0;i<mstpV2.stepperGroup.size();i++)
  {//check for buffered G cmd
    if(mstpV2.stepperGroup[i]->bufferGCMD_ID<0)continue;
    if(mstpV2.stepperGroup[i]->segs.space()<GCMD_Magic_SafeSpace)continue;

    bool rspAck=false;
    bool doRsp=true;

    int cmd_id=mstpV2.stepperGroup[i]->bufferGCMD_ID;
    const char* code = mstpV2.stepperGroup[i]->bufferGCMD;


    auto retSt=insertCMD(mstpV2.stepperGroup[i],code);
    if(retSt==GCodeParser::GCodeParser_Status::TASK_OK)
    {
      rspAck=true;
    }
    else if(retSt==GCodeParser::GCodeParser_Status::TASK_OK_HOLD_RSP)
    {
      rspAck=true;
      doRsp=false;
    }
    else
    {
      rspAck=false;
    }


    mstpV2.stepperGroup[i]->bufferGCMD_ID=-1;

    if(doRsp)
    {
      retdoc["id"]=(int)doc["id"];
      retdoc["ack"]=rspAck;
      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      send_json_string(0,buff,slen,0);

    }
  }
}

#define AUX_COUNT 5

enum AUX_TASK_INFO_TYPE{
  AUX_DELAY=1,
  AUX_IO_CTRL=2,
  AUX_WAIT_FOR_ENC=3,
  AUX_WAIT_FOR_FINISH=1000,

};


struct AUX_TASK_INFO_WAIT_FOR_FINISH{
  int cmd_id;

};

struct AUX_TASK_INFO_WAIT_FOR_ENC{
  int value;

};
struct AUX_TASK_INFO_DELAY{
  int time;

};
struct AUX_TASK_INFO_IO_CTRL{
  
  int pin;
  int state;

  char CID[50];
  char TTAG[100];
  int TID;


};

struct AUX_TASK_INFO {
  AUX_TASK_INFO(){}
  ~AUX_TASK_INFO(){}
  AUX_TASK_INFO_TYPE type;
  


  union {
    AUX_TASK_INFO_DELAY delayInfo;
    AUX_TASK_INFO_IO_CTRL ioCtrl;
    AUX_TASK_INFO_WAIT_FOR_ENC wait_enc;
    AUX_TASK_INFO_WAIT_FOR_FINISH wait_fin;
  }; 

  //Just for ioCtrl
  // string CID;
  // string TTAG;
};

// static QueueHandle_t SeqCalcTaskQueue;
static QueueHandle_t AUXTaskQueue[AUX_COUNT];

bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK)
{
  int AUX_THREAD_ID=(doc["aid"].is<int>())?doc["aid"]:0;
  if(AUX_THREAD_ID>=AUX_COUNT)
  {
    return false;
  }
  if(strcmp(type,"AUX_TEST")==0)
  {
    ret_doc["msg"]="Try more";
    doRsp=true;
    isACK=false;
    return true;
  }


  if(strcmp(type,"AUX_DELAY")==0)
  {


    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_DELAY;


    task.delayInfo.time=(doc["P"].is<int>())?doc["P"]:1000;

    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    doRsp=true;
    isACK=true;
    return true;
  }
  if(strcmp(type,"AUX_WAIT_FOR_ENC")==0)
  {

    doRsp=true;



    if(doc["value"].is<int>()==false)
    {
      isACK=false;
      return true;
    }
    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_ENC;
    task.wait_enc.value=doc["value"];

    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    isACK=true;
    return true;
  }
  if(strcmp(type,"AUX_WAIT_FOR_FINISH")==0)
  {



    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH;
    task.wait_fin.cmd_id=doc["id"];


    xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
    doRsp=false;
    isACK=true;
    return true;
  }



  if(strcmp(type,"AUX_IO_CTRL")==0)
  {


    AUX_TASK_INFO task;
    task.type = AUX_TASK_INFO_TYPE::AUX_IO_CTRL;


    task.ioCtrl.pin=(doc["pin"].is<int>())?doc["pin"]:-1;
    task.ioCtrl.state=(doc["state"].is<int>())?doc["state"]:-1;

    if(task.ioCtrl.pin==-1 || task.ioCtrl.state==-1)
    {
      isACK=false;

    }
    else
    {
      task.ioCtrl.CID[0]='\0';
      task.ioCtrl.TTAG[0]='\0';
      task.ioCtrl.TID=-1;
      if(doc["CID"].is<const char*>()  )
      { 
        strncpy(task.ioCtrl.CID,(const char*)doc["CID"],sizeof(task.ioCtrl.CID));

        if(doc["TTAG"].is<const char*>() )
        {
          strncpy(task.ioCtrl.TTAG,(const char*)doc["TTAG"],sizeof(task.ioCtrl.TTAG));
        }


        task.ioCtrl.TID=(doc["TID"].is<int>() )?doc["TID"]:-1;

      }

      


      xQueueSend(AUXTaskQueue[AUX_THREAD_ID], (void*)&task, 10 / portTICK_PERIOD_MS /* timeout */);
      isACK=true;
    }
    doRsp=true;
    return true;
  }

  return false;
}



RingBuf_Static<struct Mstp2CommInfo,20,uint8_t> AUX2CommInfoQ;
static SemaphoreHandle_t AUX2Comm_Lock;

void AUX_task(void *pvParameter)
{
  QueueHandle_t &Q=*(QueueHandle_t *)pvParameter;
    while(1) {
      AUX_TASK_INFO info; 
      if (xQueueReceive(Q, (void *)&info, portMAX_DELAY) == pdTRUE) {

        switch(info.type)
        {

          case AUX_TASK_INFO_TYPE::AUX_DELAY:
            vTaskDelay(info.delayInfo.time / portTICK_RATE_MS);
            // G_LOG(">>>>");
          break;

          case AUX_TASK_INFO_TYPE::AUX_IO_CTRL :

          break;


          case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH :

              struct Mstp2CommInfo tinfo={
              .type=Mstp2CommInfo_Type::respFrame,
              .isAck=true,
              .resp_id=info.wait_fin.cmd_id
              };

              xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);//LOCK
              Mstp2CommInfo* Qhead=NULL;
              while( (Qhead=AUX2CommInfoQ.getHead()) ==NULL);
              *Qhead=tinfo;
              AUX2CommInfoQ.pushHead();
              xSemaphoreGive(AUX2Comm_Lock);//UNLOCK
          break;
        }
      }
    }
}

//float 100 add,sub 5.5us
//float 100 mult,div 24us
//float 100 sin 48us

//int 100 add,mult,div 5.5us
//int 100 div 5.8us


void SEG_CALC_task(void *pvParameter)
{

  // xVec pre_mot_iloc={0};
  while(1)
  { 
    xSemaphoreTake(SeqCalcTaskLock, portMAX_DELAY);//LOCK
    xVec_f mot_loc={0};
    for(int i=0;i<mstpV2.stepperGroup.size();i++)//calc next step
    {
      mstpV2.stepperGroup[i]->update();

      mstpV2.stepperGroup[i]->getMotMoveVec(&mot_loc);
    }

    xVec mot_ivec={0};
    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      mot_ivec.vec[i]=(uint32_t)mot_loc.vec[i];//-pre_mot_iloc.vec[i]);

      // pre_mot_iloc.vec[i]=(uint32_t)mot_loc.vec[i];
    }

 
    PG_M.loadNext(mstpV2.updatePeriod_s*1000000,mot_ivec);

  }

}



int rzERROR=0;
void setup()
{
  SeqCalcTaskLock = xSemaphoreCreateBinary();

  
  mstpV2.stepperGroup.push_back(&SG_RX);
  mstpV2.stepperGroup.push_back(&SG_SIMP);

  
  // noInterrupts();
  // Serial.begin(921600);//230400);
  Serial.begin(115200);//230400);
  // Serial.begin(460800);
  Serial.setRxBufferSize(500);
  // // setup_comm();
  timer = timerBegin(0, 80*1000*1000/_TICK2SEC_BASE_, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);


  AUX2Comm_Lock = xSemaphoreCreateMutex();
  for(int i=0;i<AUX_COUNT;i++)
  {
    AUXTaskQueue[i] = xQueueCreate(20 /* Number of queue slots */, sizeof(AUX_TASK_INFO));
    xTaskCreatePinnedToCore(&AUX_task, "AUX_task", 2048, (void*)&AUXTaskQueue[i], 1, NULL, 0);

  }

  // SeqCalcTaskQueue= xQueueCreate(2, sizeof(uint32_t));
  xTaskCreatePinnedToCore(&SEG_CALC_task, "SEGCalcTask", 1024,NULL, configMAX_PRIORITIES-1, NULL, 0);

  pinMode(PIN_DBG1, OUTPUT);
  pinMode(PIN_DBG2, OUTPUT);
  pinMode(PIN_DBG3, OUTPUT);
  pinMode(PIN_DBG4, OUTPUT);
  pinMode(PIN_DBG5, OUTPUT);
  pinMode(PIN_DBG6, OUTPUT);
  pinMode(pin_TRIG_595, OUTPUT);
  pinMode(pin_SH_165, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_PSENSOR_N, INPUT_PULLUP);


  

  // CameraIDList[0]="ABC";
  // CameraIDList[1]="DEF";
}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    yield();
  }
}

// MSTP_SegCtx ctx[10];


// string toFixed(float num,int powNum=100)
// {
//   int ipnum=round(num*powNum);
//   int inum=ipnum/powNum;

//   string istr=to_string(inum);
//   int pnum=(ipnum%powNum);

//   string pstr=to_string(pnum+powNum);

//   string resStr=istr+pstr;
//   resStr[istr.length()]='.';
//   return resStr;
// }


bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


static uint8_t recvBuf[20];
void loop()
{
  djrl.loop();
  {
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.read(recvBuf,sizeof(recvBuf-1));
      //
      // djrl.dbg_printf("recvLen:%d",recvLen);
      djrl.recv_data((uint8_t*)recvBuf,recvLen);
      if(doDataLog)
      {
        for(int i=0;i<recvLen;i++) 
        {
          if(recvBuf[i]=='"')
            recvBuf[i]='\'';
        }     
        recvBuf[recvLen]='\0';
        djrl.dbg_printf(">%s",recvBuf);
      }

    }
    if(recvF)
    {
      // djrl.dbg_printf("recv DONE");
    }
  }


  {



    uint8_t buff[700];
    while(1)
    {
      bool hasNewInfo=false;
      Mstp2CommInfo info;
      if(hasNewInfo ==false && 0!=(Mstp2CommInfoQ.size()))
      {
        info=*Mstp2CommInfoQ.getTail();
        Mstp2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }


      xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);
      if(hasNewInfo ==false && 0!=(AUX2CommInfoQ.size()))
      {
        info=*AUX2CommInfoQ.getTail();
        AUX2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }
      xSemaphoreGive(AUX2Comm_Lock);




      if(hasNewInfo==false)break;

      retdoc.clear();
      // retdoc["tag"]="s_Step_"+std::to_string((int)info.step);
      // retdoc["trigger_id"]=info.step;
      switch (info.type)
      {
        case Mstp2CommInfo_Type::ext_log :
        {

          djrl.dbg_printf("%s",info.strinfo);

          break;
        }
      
        case Mstp2CommInfo_Type::respFrame :
        {


          retdoc["id"]=info.resp_id;
          retdoc["ack"]=info.isAck;
          
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }
      }
    }
  }


  // static unsigned long startMillis=0; 
  // unsigned long currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  // if (currentMillis - startMillis >= 100)  //test whether the period has elapsed
  // {
  //   startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.

  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>24));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>16));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>8));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins));
  //   Serial.printf("\n");
  // }

}





int intArrayContent_ToJson(char *jbuff, uint32_t jbuffL, int16_t *intarray, int intarrayL)
{
  uint32_t MessageL = 0;

  for (int i = 0; i < intarrayL; i++)
    MessageL += sprintf((char *)jbuff + MessageL, "%d,", intarray[i]);
  MessageL--; //remove the last comma',';

  return MessageL;
}


void genMachineSetup(JsonDocument &jdoc)
{

  jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  // auto obj=jdoc.createNestedObject("obj");
}

#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}

void setMachineSetup(JsonDocument &jdoc)
{
  // JSON_SETIF_ABLE(g_cam_trig_delay,jdoc,"cam_trig_delay");
}





