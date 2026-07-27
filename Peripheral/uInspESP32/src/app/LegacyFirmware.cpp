#include "app/LegacyFirmware.hpp"
#include "main.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include "comm/Data_Layer_Protocol.hpp"
#include "config/MachineConfig.hpp"
#include "driver/timer.h"
#include <string>

extern "C" {
#include "direct_spi.h"
}

#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)


#define GPIOLS32_SET(PIN) GPIO.out_w1ts=1<<(PIN);
#define GPIOLS32_CLR(PIN) GPIO.out_w1tc=1<<(PIN);
// Direct register read, mirroring the write macros above. Arduino digitalRead()
// is not IRAM-resident and walks the pin mux table, which is dead weight in the
// step ISR. Valid for pins 0..31 only -- every input we sample is in that range.
#define GPIOLS32_GET(PIN) ((GPIO.in>>(PIN))&1)

// Single place that drives every actuator to its inactive level. Used on the
// error path, on reset, and once the plate has coasted to a stop, so a selector
// can never be left energised by a state transition that forgot one pin.
#define ALL_OUTPUTS_SAFE() \
  { \
    GPIOLS32_CLR(PIN_O_L1A); \
    GPIOLS32_CLR(PIN_O_L2A); \
    GPIOLS32_CLR(PIN_O_CAM1); \
    GPIOLS32_CLR(PIN_O_CAM2); \
    GPIOLS32_CLR(PIN_O_SEL1); \
    GPIOLS32_CLR(PIN_O_SEL2); \
    GPIOLS32_CLR(PIN_O_SEL3); \
  }


#define SARRL(SARR) (sizeof((SARR)) / sizeof(*(SARR)))

GEN_ERROR_CODE errorBuf[20];
RingBuf<typeof(*errorBuf), uint8_t> ERROR_HIST(errorBuf, SARRL(errorBuf));

SYS_INFO sysinfo = {
    .pre_state = SYS_STATE::INIT,
    .state = SYS_STATE::INIT,
    .extra_code = 0,
    .status = 0,
    .PTSyncInfo = {.state = PulseTimeSyncInfo_State::INIT},
  };



float SETUP_TAR_FREQ=0;
bool SYS_FREQ_STABLE=false;
float SYS_TAR_FREQ=1000;
float SYS_CUR_FREQ=0;
float SYS_FREQ_ADV_STEP=5;
bool SYS_STEPPER_DISABLED=false;

uint32_t SYS_MIN_PULSE_TIME_SEP_us=(1000000/15);
int SEL1_ACT_COUNTDOWN=-1;

#define _PLAT_DIAMITER_mm 350
#define _PLAT_CIRC_um (_PLAT_DIAMITER_mm*3.14159*1000)
#define _PLAT_SUB_STEP 800
#define _PLAT_PULSE_PER_TURN (_PLAT_SUB_STEP*18 *2)
#define _PLAT_DIST_um_PER_STEP ((int)(_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))

#define _PLAT_DIST_um(stepCount) ((int)(stepCount*_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))
#define _PLAT_DIST_step(dist_um) ((int)(dist_um*_PLAT_PULSE_PER_TURN/_PLAT_CIRC_um))

//disk D=350 circumference 350*Pi
//1600*9 steps per round
//0.076mm per step


typedef struct pipeLineInfo{
  uint32_t gate_pulse;
  int8_t stage;
  int32_t insp_status;
  uint32_t tid;
}pipeLineInfo;

uint64_t SEL1_Count=0;
uint64_t SEL2_Count=0;
uint64_t SEL3_Count=0;
uint64_t NA_Count=0;
uint64_t SKIP_Count=0;

// stagePulseOffset now lives in config/MachineConfig.hpp so the NVS layer can
// persist it. The values below remain the fallback for a board with no stored
// config; MachineConfig::begin() overwrites them when one exists.
stagePulseOffset STAGE_PULSE_OFFSET={
  .CAM1_on =654,
  .CAM1_off=656,
  .L1A_on =654,
  .L1A_off=666,


  .CAM2_on =654,
  .CAM2_off=656,
  .L2A_on =654,
  .L2A_off=656,


  .SWITCH =697,


  .SEL1_on=700,
  .SEL1_off=701,
  .SEL2_on=710,
  .SEL2_off=711,
  .SEL3_on=720,
  .SEL3_off=721



};

// STAGE_PULSE_OFFSET above is the main-loop-owned working copy: set_setup edits
// it field by field, MachineConfig reads/writes it as a whole, get_setup reports
// it. The step ISR must NOT read it directly -- a 15-field struct updated in
// place is torn from an interrupt's point of view, so one object could be timed
// with half the old configuration and half the new (see
// docs/CONCURRENCY_ANALYSIS.md 5.2).
//
// Instead the ISR reads through SPO_active, which always points at a snapshot
// that is never mid-write. The writer fills the OTHER snapshot with interrupts
// enabled (no step-timer stall, unlike a critical section around 15 JSON field
// extractions) and commits with a single atomic pointer swap. A read of
// SPO_active therefore sees one buffer or the other, whole, never a mix.
//
// Note this does not, and need not, make an object's whole lifecycle coherent:
// ACT_PUSH_TASK bakes gate_pulse+offset into targetPulse at registration, so
// CAM/L/SWITCH offsets are captured then, while the SEL offsets are read later
// in the SWITCH branch. A config change between those two moments gives that one
// object new SEL timing with old CAM timing -- an inherent property of reading
// SEL late, unrelated to tearing, and harmless because config only changes
// during deliberate setup.
static stagePulseOffset SPO_snap[2] = { STAGE_PULSE_OFFSET, STAGE_PULSE_OFFSET };
static volatile stagePulseOffset* volatile SPO_active = &SPO_snap[0];

// Copy the working STAGE_PULSE_OFFSET into the inactive snapshot and publish it
// to the ISR with one atomic pointer store. Call from the main loop after any
// change to STAGE_PULSE_OFFSET, before the ISR needs to act on it.
void STAGE_PULSE_OFFSET_publish()
{
  stagePulseOffset* inactive =
      (SPO_active == &SPO_snap[0]) ? &SPO_snap[1] : &SPO_snap[0];
  *inactive = STAGE_PULSE_OFFSET;
  // Ensure the copy is fully written before the pointer that exposes it moves.
  __asm__ __volatile__("" ::: "memory");
  SPO_active = inactive;   // aligned pointer store -> atomic on ESP32
}

RingBuf_Static<pipeLineInfo, PIPE_INFO_LEN, uint8_t> RBuf;



struct ACT_INFO
{
  pipeLineInfo *src;
  int info;
  uint32_t targetPulse;
};



#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info, cusCode_task) \
  {                                                                 \
    ACT_INFO *_task_;                                                 \
    _task_ = (rb).getHead();                                          \
    if (_task_)                                                       \
    {                                                               \
      _task_->targetPulse = (plinfo->gate_pulse + pulseOffset);       \
      _task_->src = plinfo;                                           \
      _task_->info = _info;                                           \
      cusCode_task                                                  \
      (rb).pushHead();                                              \
    }                                                               \
    _task_;                                                           \
  }

//EXP:
// ((0-1)>>1)+1
// ((0xFF)>>1)+1
// (0x7F)+1
// (0x80)
#define UNSIGNED_NUM_HIGHEST_BIT(num) ( (( ((typeof(num))0)-1 )>>1)+1   )


//if targetPulse-cur_pulse <=0
//=> targetPulse-cur_pulse-1 <0 (in unsigned number the highest bit is 1)
//return Yes or no
#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \ 
  {                                                   \
    ACT_INFO *task = act_rb.getTail();                \
    if (task && ((task->targetPulse-cur_pulse-1)&UNSIGNED_NUM_HIGHEST_BIT(cur_pulse)))\
    {                                                 \
      {cmd_task }                                     \
      act_rb.consumeTail();                           \
    }else  task=NULL;                                 \
    task!=NULL;                                       \
  }




string CAM1_ID;
string CAM1_Tags;
string CAM2_ID;
string CAM2_Tags;

struct ACT_SCH
{
  RingBuf_Static<ACT_INFO, PIPE_INFO_LEN>
      ACT_L1A,
      ACT_CAM1,

      ACT_L2A,
      ACT_CAM2,

      ACT_SWITCH,
      ACT_SEL1,
      ACT_SEL2;
};

struct ACT_SCH act_S;


void RESET_ALL_PIPELINE_QUEUE()
{
  
  RBuf.clear();
  act_S.ACT_CAM1.clear();
  act_S.ACT_CAM2.clear();
  act_S.ACT_L1A.clear();
  act_S.ACT_L2A.clear();
  act_S.ACT_SEL1.clear();
  act_S.ACT_SEL2.clear();
  act_S.ACT_SWITCH.clear();
  // RESET_GateSensing();
}


enum TaskQ2CommInfo_Type{
  trigInfo=1000,
  btrigInfo=1005,//brif trigger info
  systemInfo=1006,
  ext_log=1001,
  respFrame=1002,
};



struct TaskQ2CommInfo{//TODO: rename the infoQ to be more versatile
  TaskQ2CommInfo_Type type;

  //trigInfo
  string camera_id;
  string trig_tag;

  int btrig_idx;
  int64_t trig_time_us;
  int trig_id;
  // float curFreq;

  //log
  string log;

  //respFrame
  bool isAck;
  int resp_id;
};

// Producers: the main loop only (state transitions, recv_ERROR, trigCamPulse).
// Consumer: the main loop. The camera-trigger path used to push here too, from
// inside the timer ISR, which made this a two-producer queue -- and a locked
// counter cannot save that, because getHead()/fill/pushHead() is three steps:
// the ISR and the loop could both be handed the SAME slot, each write it, and
// one message would be lost while the next slot went out never written.
//
// Splitting the ISR onto its own queue removes the race by construction rather
// than by locking, and keeps std::string out of interrupt context: this struct
// holds three of them, so copying it in an ISR would mean malloc in an ISR.
RingBuf_Static<struct TaskQ2CommInfo,20,uint8_t> TaskQ2CommInfoQ;

// Camera-trigger announcements raised inside onTimer(). Single producer (ISR),
// single consumer (the main loop drain) -- a true SPSC ring, which the counter
// critical section in RingBuf.hpp now makes safe.
//
// POD only, deliberately: it is filled from an ISR, so nothing here may
// allocate. The main loop turns it into the bTrigInfo JSON where std::string
// is free to be used.
struct ISRTrigInfo
{
  uint64_t trig_time_us;
  uint32_t trig_id;
  uint8_t  btrig_idx;
};
RingBuf_Static<struct ISRTrigInfo,32,uint8_t> ISRTrigQ;


// --- IO trace: an on-board logic analyzer for the actuator sequence -------
// Every actuator edge in Run_ACTS records (pulse, tid, val, pin) here while
// armed, so the real-geometry timing of the L1A / CAM / SWITCH / SEL edges can
// be dumped and checked without a scope on the bench. Filled from onTimer()
// (the timer ISR), drained by the io_trace_dump command (main loop) -- SPSC,
// POD, no allocation, the same discipline as ISRTrigQ. Disarmed by default:
// the guard is a single volatile read, so a deployed machine pays nothing.
// `pin` carries the raw PIN_O_* GPIO number; the SWITCH dispatch has no pin of
// its own and is logged as pin 0 with val = the decided insp_status.
struct IOTraceEvt { uint32_t pulse; uint32_t tid; int32_t val; uint8_t pin; };
RingBuf_Static<struct IOTraceEvt,120,uint16_t> IO_TRACE;
volatile bool IO_TRACE_ARMED=false;
#define IOT_PIN_SWITCH 0
inline void IO_TRACE_LOG(uint8_t pin,int32_t val,uint32_t pulse,uint32_t tid)
{
  if(!IO_TRACE_ARMED)return;
  IOTraceEvt *e=IO_TRACE.getHead();
  if(e){ e->pulse=pulse; e->tid=tid; e->val=val; e->pin=pin; IO_TRACE.pushHead(); }
}


void ERROR_LOG_PUSH(GEN_ERROR_CODE code)
{
  GEN_ERROR_CODE *head_code = ERROR_HIST.getHead();
  if (head_code == NULL)//no space, eat tail keep the latest one
  {
    ERROR_HIST.consumeTail();
    head_code = ERROR_HIST.getHead();
  }

  if (head_code != NULL)
  {
    *head_code = code;
    ERROR_HIST.pushHead();

    // DEBUG_print("errorLOG:");
    // DEBUG_println((int)code);
    // //errorAction(sysinfo.err_act);
  }
}


// Written by the main loop on every state transition, read by newPulseEvent()
// from inside the timer ISR. volatile so the read is not hoisted or cached
// across the interrupt boundary -- see docs/CONCURRENCY_ANALYSIS.md.
volatile bool blockNewDetectedObject=false;

// Error raised inside onTimer(), waiting for firmwareLoop() to turn it into a
// real state transition. NOP means "nothing pending".
volatile GEN_ERROR_CODE PENDING_ISR_ERROR=GEN_ERROR_CODE::NOP;


void SYS_STATE_LIFECYCLE(SYS_STATE pre_sate, SYS_STATE new_state)
{
  SYS_STATE states[3] = {SYS_STATE::NOP};//0: enter, 1:loop, 2:exit
  int i_from, i_to;
  if (pre_sate == new_state)
  {
    i_from = 1;
    i_to = 1;
    states[1] = new_state;
  }
  else
  {
    i_from = 2;
    i_to = 0;
    states[0] = new_state;
    states[2] = pre_sate;
  }


  for (int i = i_from; i >= i_to; i--)//2(exit) -> 1(loop) -> 0(enter) the reversed order is to make sure exit(from old state) run first then run enter(to new state) block
  {

    SYS_STATE state = states[i];
    switch (state)
    {
    case SYS_STATE::INIT:
      if (i == 2)
      {//For INIT state "EXIT"(i==2) is the first and the last action it would run
        blockNewDetectedObject=true;
        SYS_TAR_FREQ=0;

        pinMode(FEEDER_PIN, OUTPUT);
      } //exit
      break;
      
    case SYS_STATE::IDLE:
      if (i == 0)
      {
        blockNewDetectedObject=true;//Accept pulse to trigger camera
        //but in this state will not handle other event
        RESET_ALL_PIPELINE_QUEUE();
      } //enter
      else if (i == 1)
      {
        SYS_TAR_FREQ=SETUP_TAR_FREQ;
        // SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
        // SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);//the event sould be issued by remote
      } //loop
      else
      {
      } //exit
      break;



    case SYS_STATE::INSPECTION_MODE_TEST:
      if (i == 0)
      {
        blockNewDetectedObject=false;
      } //enter
      else if (i == 1)
      {
        SYS_TAR_FREQ=SETUP_TAR_FREQ;
      } //loop
      else
      {
      } //exit
      break;
    break;

    case SYS_STATE::INSPECTION_MODE_READY:
    {
      static uint8_t loopDivCounter=0;// just to make the buzy loop not so buzy
       
      if (i == 0)//enter
      {
        blockNewDetectedObject=false;
        digitalWrite(FEEDER_PIN, 0);

        // 
      } 
      else if (i == 1)
      {
        SYS_TAR_FREQ=SETUP_TAR_FREQ;
      } //loop
      else
      {
        blockNewDetectedObject=true;
        digitalWrite(FEEDER_PIN, 1);
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_ERROR:
    {
      static uint32_t targetPulse=0;
      if (i == 0)
      {
        SYS_TAR_FREQ=0;
        blockNewDetectedObject=true;

        // Drop every actuator immediately. The plate still has to ramp down, so
        // waiting for the freq-stable path below would leave a selector held on
        // for the whole deceleration.
        ALL_OUTPUTS_SAFE();

        RESET_ALL_PIPELINE_QUEUE();
        // DEBUG_printf(">>ENTER ERROR(%d)>>>\n",sysinfo.extra_code);

        RESET_ALL_PIPELINE_QUEUE();

        // digitalWrite(AIR_BLOW_OK_PIN, 0);
        // digitalWrite(AIR_BLOW_NG_PIN, 0);
        // digitalWrite(BACK_LIGHT_PIN, 1);
        // targetPulse=get_Stepper_pulse_count()+perRevPulseCount/3;//in jail for a bit
        ERROR_LOG_PUSH((GEN_ERROR_CODE)sysinfo.extra_code);
      } //enter
      else if (i == 1)
      {
        // int32_t diff=get_Stepper_pulse_count()-targetPulse;
        
        // if(diff>0)//times up
        // {
        //   SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);
        // }
      }
      else
      {
        // digitalWrite(BACK_LIGHT_PIN, 0);
      }
    }


    case SYS_STATE::NOP:
    default:
    break;


    
    }
  }
}




void SYS_STATE_Transfer(SYS_STATE_ACT act,int extraCode=0)
{
  SYS_STATE state = sysinfo.state;
  

#define _MX1(CASE_STATE,CONTENT) \
  case CASE_STATE :{\
    switch(act){\
      CONTENT\
    }\
    break;\
  }
#define _MX2(CASE_ACT,NEW_STATE) \
  case CASE_ACT :{\
    state = NEW_STATE;\
    break;\
  }\

  switch(state)
  {
    SMM_STATE_TRANSFER_DECLARE(
      _MX1,
      _MX2,
      SYS_STATE,SYS_STATE_ACT
    )
  }
  #undef _MX1
  #undef _MX2
  
  if (sysinfo.state != state)
  { //state changed


    {
      TaskQ2CommInfo *commInfo = TaskQ2CommInfoQ.getHead();
      if(commInfo){
        commInfo->type=TaskQ2CommInfo_Type::systemInfo;
        char numberStr[100];  // Assuming the number will fit within 10 characters
        sprintf(numberStr, "State changed from  %d to %d",sysinfo.state,state);
        commInfo->log=numberStr;
        TaskQ2CommInfoQ.pushHead();
      }
    }

    sysinfo.pre_state = sysinfo.state;
    sysinfo.state = state;
    sysinfo.extra_code=extraCode;
    // DEBUG_printf("=========s:%d=>%d\n",sysinfo.pre_state,sysinfo.state);
    SYS_STATE_LIFECYCLE(sysinfo.pre_state, sysinfo.state );

  }
  else
  {
    SYS_STATE_LIFECYCLE(sysinfo.state, sysinfo.state );
  }
}








int ActRegister_pipeLineInfo(pipeLineInfo *pli);


uint32_t _prePulse=0;
uint64_t _preTime=0;
int newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  static uint32_t acc_tid=1;
  uint32_t _prePulse_BK=_prePulse;
  _prePulse=middle_pulse;
  if(middle_pulse-_prePulse_BK<(_PLAT_DIST_step(3500)))return -9;
  uint64_t curTime = esp_timer_get_time();
  if(curTime-_preTime<SYS_MIN_PULSE_TIME_SEP_us)return -8;
  _preTime=curTime;


  if(blockNewDetectedObject)return -1;
  pipeLineInfo *head = RBuf.getHead();
  if (head == NULL)
    return -1;

  //get a new object and find a space to log it
  // TCount++;
  // head->s_pulse = start_pulse;
  // head->e_pulse = end_pulse;
  // head->pulse_width = pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status = insp_status_UNSET;
  head->tid=acc_tid;
  if (ActRegister_pipeLineInfo(head) != 0)
  { //register failed....
    return -2;
  }
  RBuf.pushHead();
  acc_tid++;
  return 0;
}
int ActRegister_pipeLineInfo(pipeLineInfo *pli)
{


  if (act_S.ACT_L1A.space() >= 2 && act_S.ACT_L2A.space() >= 2 &&
      act_S.ACT_CAM1.space() >= 2 && act_S.ACT_CAM2.space() >= 2 && act_S.ACT_SWITCH.space() >= 1)
  {
    // DEBUG_printf(">>>>src:%p gate_pulse:%d ",pli,pli->gate_pulse);
    // DEBUG_printf("s:%d ",pli->s_pulse);
    // DEBUG_printf("e:%d ",pli->e_pulse);
    // DEBUG_printf("cur:%d\n",logicPulseCount);

    // One coherent snapshot for this object's registration (see SPO_active).
    volatile stagePulseOffset* spo = SPO_active;
    ACT_PUSH_TASK(act_S.ACT_L1A, pli, spo->L1A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L1A, pli, spo->L1A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, spo->CAM1_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, spo->CAM1_off, 0, );


    ACT_PUSH_TASK(act_S.ACT_L2A, pli, spo->L2A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L2A, pli, spo->L2A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, spo->CAM2_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, spo->CAM2_off, 0, );

    ACT_PUSH_TASK(act_S.ACT_SWITCH, pli,spo->SWITCH, 0, );
    return 0;
    // pli->insp_status=insp_status_OK;
  }
  return -1;
}





int Run_ACTS(uint32_t cur_pulse)
{
  bool time_us_fetched=false;
  uint64_t time_us=0;
  struct ACT_SCH *acts= &act_S;
  // static uint32_t pre_pulse=0;

  // uint32_t diff = cur_pulse-pre_pulse;
  // if(diff!=1)
  // {
  //   DEBUG_printf("pre_pulse:%d ",pre_pulse);
  //   DEBUG_printf("cur_pulse:%d \n",cur_pulse);
  // }
  // pre_pulse=cur_pulse;

  GEN_ERROR_CODE ecode=GEN_ERROR_CODE::NOP;

  ACT_TRY_RUN_TASK(acts->ACT_L1A, cur_pulse,
                   if(task->info)
                   {
                    GPIOLS32_SET(PIN_O_L1A);
                    IO_TRACE_LOG(PIN_O_L1A,1,cur_pulse,task->src->tid);
                   }
                   else
                   {
                    GPIOLS32_CLR(PIN_O_L1A);
                    IO_TRACE_LOG(PIN_O_L1A,0,cur_pulse,task->src->tid);
                   }


                   );






  ACT_TRY_RUN_TASK(acts->ACT_CAM1, cur_pulse,

                  if(task->info)
                  {

                    GPIOLS32_SET(PIN_O_CAM1);
                    IO_TRACE_LOG(PIN_O_CAM1,1,cur_pulse,task->src->tid);
                    ISRTrigInfo *commInfo = ISRTrigQ.getHead();
                    if(commInfo){
                      if(time_us_fetched==false)
                      {
                        time_us=esp_timer_get_time();
                        time_us_fetched=true;
                      }
                      commInfo->trig_time_us=time_us;
                      commInfo->btrig_idx=1;
                      commInfo->trig_id=task->src->tid;
                      ISRTrigQ.pushHead();
                    }
                    else
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    GPIOLS32_CLR(PIN_O_CAM1);
                    IO_TRACE_LOG(PIN_O_CAM1,0,cur_pulse,task->src->tid);
                  }


                   );


  ACT_TRY_RUN_TASK(acts->ACT_L2A, cur_pulse,
                   if(task->info)
                   {
                    GPIOLS32_SET(PIN_O_L2A);
                    IO_TRACE_LOG(PIN_O_L2A,1,cur_pulse,task->src->tid);
                   }
                   else
                   {
                    GPIOLS32_CLR(PIN_O_L2A);
                    IO_TRACE_LOG(PIN_O_L2A,0,cur_pulse,task->src->tid);
                   }
                    );


  ACT_TRY_RUN_TASK(acts->ACT_CAM2, cur_pulse,

                  if(task->info)
                  {

                    GPIOLS32_SET(PIN_O_CAM2);
                    IO_TRACE_LOG(PIN_O_CAM2,1,cur_pulse,task->src->tid);
                    ISRTrigInfo *commInfo = ISRTrigQ.getHead();
                    if(commInfo){
                      if(time_us_fetched==false)
                      {
                        time_us=esp_timer_get_time();
                        time_us_fetched=true;
                      }
                      commInfo->trig_time_us=time_us;
                      commInfo->btrig_idx=2;
                      commInfo->trig_id=task->src->tid;
                      ISRTrigQ.pushHead();
                    }
                    else
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    GPIOLS32_CLR(PIN_O_CAM2);
                    IO_TRACE_LOG(PIN_O_CAM2,0,cur_pulse,task->src->tid);
                  }




                   );



  ACT_TRY_RUN_TASK(
      acts->ACT_SWITCH, cur_pulse,

      // DEBUG_printf("SW src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

      pipeLineInfo *pli = task->src;
      // DEBUG_print("insp_status:");
      // DEBUG_println(pli->insp_status);

      IO_TRACE_LOG(IOT_PIN_SWITCH,pli->insp_status,cur_pulse,pli->tid);

      volatile stagePulseOffset* spo = SPO_active;
      switch (pli->insp_status)
      {
        case 1:
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, spo->SEL1_on, 1, _task_->src =NULL;);//the src will be cleaned up right after
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, spo->SEL1_off, 0, _task_->src =NULL; );
          break;
        case 2:
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, spo->SEL2_on, 1, _task_->src =NULL; );
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, spo->SEL2_off, 0, _task_->src =NULL; );
          break;
        case 3:
          SEL3_Count++;
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_on, 1, _task_->src =NULL; );
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_off, 0, _task_->src =NULL; );
          break;
        case 0xFFFF:
          NA_Count++;
          // inspResCount.NA++;
          break;

        case insp_status_SKIP: 
          SKIP_Count++;
          break;
        case insp_status_DEL: //ERROR
          break;

        case insp_status_UNSET:
        default:
          ecode=GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT;
          
          break;
      }
      //
      
      {
        // task->src->insp_status = insp_status_DEL;
        task->src->insp_status = insp_status_DEL;
        task->src = NULL;
        // RBuf.consumeTail();
      }
  );



  ACT_TRY_RUN_TASK(acts->ACT_SEL1, cur_pulse,
                   if(task->info)
                   {

                    if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false && SEL1_ACT_COUNTDOWN)
                    {
                      if(SEL1_ACT_COUNTDOWN>0)SEL1_ACT_COUNTDOWN--;
                      SEL1_Count++;
                      GPIOLS32_SET(PIN_O_SEL1);
                      IO_TRACE_LOG(PIN_O_SEL1,1,cur_pulse,0);
                    }
                   }
                   else
                   {
                    GPIOLS32_CLR(PIN_O_SEL1);
                    IO_TRACE_LOG(PIN_O_SEL1,0,cur_pulse,0);
                   }
                  );


  ACT_TRY_RUN_TASK(acts->ACT_SEL2, cur_pulse,
                  
                  if(task->info)
                  {

                  if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false)
                  {
                    SEL2_Count++;
                    GPIOLS32_SET(PIN_O_SEL2);
                    IO_TRACE_LOG(PIN_O_SEL2,1,cur_pulse,0);
                  }
                  }
                  else
                  {
                    GPIOLS32_CLR(PIN_O_SEL2);
                    IO_TRACE_LOG(PIN_O_SEL2,0,cur_pulse,0);
                  }
                
                  
                  );


  if(ecode!=GEN_ERROR_CODE::NOP)
  {
    // Run_ACTS() executes inside onTimer(); SYS_STATE_Transfer() walks into
    // SYS_STATE_LIFECYCLE(), which calls pinMode()/digitalWrite() -- neither is
    // IRAM-resident and neither is safe from an ISR. Do the parts that must not
    // wait right here (register writes and a bool are ISR-safe), and hand the
    // state transition itself to firmwareLoop().
    ALL_OUTPUTS_SAFE();
    blockNewDetectedObject=true;

    if(PENDING_ISR_ERROR==GEN_ERROR_CODE::NOP)
      PENDING_ISR_ERROR=ecode;//keep the first error, it is the one that explains the rest
  }

  return 0;
}





inline float mm2Pulse_conv(int axisIdx,float dist);

void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc);
bool doDataLog=false;
class MData_JR:public Data_JsonRaw_Layer
{
  bool commsErrorLatched=false;
  void handleResetCommand();
  
  public:
  MData_JR():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {
    handleResetCommand();
    return msg_printf("RESET_OK","");
  } 
  int recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data=NULL,size_t dataL=0);
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  void connected(Data_Layer_IF* ch){}

  int send_data(int head_room,uint8_t *data,int len,int leg_room);
  void disconnected(Data_Layer_IF* ch){}

  int close(){}

  
  char dbgBuff[500];
  int dbg_printf(const char *fmt, ...);

  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


};
MData_JR djrl;


int HACK_cur_cmd_id=-1;


void G_LOG(char* str)
{
  djrl.dbg_printf(str);
}


hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5
#define PIN_LED 2



//#define _HOMING_DBG_FLAG_ 50


int pin_SH_165=17;
int pin_TRIG_595=5;

#define SUBDIV (3200)
#define mm_PER_REV 95

spi_device_handle_t spi1=NULL;

enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,

};





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

struct MSTP_SegCtx{
  MSTP_SegCtx(){}
  ~MSTP_SegCtx(){}

  bool isProcessed;
  MSTP_SegCtx_TYPE type;
  union {
    struct MSTP_SegCtx_IOCTRL IO_CTRL;
    struct MSTP_SegCtx_INPUTMON INPUT_MON;
    struct MSTP_SegCtx_OnTimeReply ON_TIME_REP;
  }; 
  string CID;
  string TTAG;
  int TID;
};


const int SegCtxSize=40;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);





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





uint32_t SYS_STEP_COUNT=0;


typedef struct GateInfo {
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint16_t debunce;
  uint8_t cur_Sense;


} GateInfo;



//uint32_t logicPulseCount = 0;

GateInfo gateInfo={0};




extern int DEBOUNCE_H_THRES;   // defined below; seeds the debounce counter
void RESET_GateSensing()
{
  GateInfo ngateInfo = {0};
  ngateInfo.cur_Sense=0;
  ngateInfo.start_pulse=~0;
  ngateInfo.end_pulse=~0;
  // Idle is LOW (INPUT_PULLUP + _senseInv_), so the first edge to confirm is a
  // rising one; seed the counter with the rising threshold rather than 0.
  ngateInfo.debunce=DEBOUNCE_H_THRES;
  gateInfo=ngateInfo;
}


bool _senseInv_=true;

int  minWidth = 0;
int  maxWidth = 1000;//1+40000/_PLAT_DIST_um_PER_STEP;

// Gate debounce, in gate-sample ticks (the timer runs at 2*plateFreq, so one
// tick is ~1 step ~= _PLAT_DIST_um_PER_STEP of travel -- 38um on the 350mm
// plate). A value of N means an edge is accepted only after the new level has
// held for N consecutive samples, so any glitch shorter than N ticks is
// rejected. Runtime-settable (gate_debounce_rise / gate_debounce_fall) like
// minWidth/maxWidth; not persisted, so a reboot returns to these defaults.
//
//   DEBOUNCE_H_THRES  rising edge  (object arriving): reject short HIGH blips
//                                  -- noise that would otherwise become a
//                                  phantom object with a real tid.
//   DEBOUNCE_L_THRES  falling edge (object leaving):  tolerate short LOW dips
//                                  -- a seam/hole in one object that would
//                                  otherwise split it into two detections.
//
// Default 2/2 = reject a single-sample glitch on either edge, the minimum that
// is unambiguously correct. The old 20um expression rounded to 1 (< one step),
// i.e. no debounce at all. Real parts are hundreds of steps wide, so 2 costs
// only a 1-step leading-edge shift (a constant offset, harmless downstream).
int  DEBOUNCE_L_THRES = 2;
int  DEBOUNCE_H_THRES = 2;


void GateSensing()
{
  uint8_t new_Sense = GPIOLS32_GET(PIN_I_GATE);
  if(_senseInv_)new_Sense=!new_Sense;
  bool onSenseEdge=false;

  // Counter debounce: an edge is only accepted once the new level has held for
  // DEBOUNCE_*_THRES consecutive samples, so a glitch shorter than that is
  // ignored. While the level holds we reload the counter and keep tracking the
  // trailing/leading edge, so start_pulse/end_pulse still mark the TRUE edges,
  // not the (later) debounce-confirmation tick.
  if(gateInfo.cur_Sense)
  {//currently HIGH -- object present
    if(!new_Sense)
    {//reading LOW: count toward a falling edge
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        onSenseEdge=true;
        gateInfo.debunce=DEBOUNCE_H_THRES;
      }
    }
    else
    {//still HIGH: absorb the dip, keep the trailing edge current
      gateInfo.debunce=DEBOUNCE_L_THRES;
      gateInfo.end_pulse=SYS_STEP_COUNT;
    }
  }
  else
  {//currently LOW -- no object
    if(new_Sense)
    {//reading HIGH: count toward a rising edge
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        onSenseEdge=true;
        gateInfo.debunce=DEBOUNCE_L_THRES;
      }
    }
    else
    {//still LOW: reject the blip, keep the leading edge current
      gateInfo.debunce=DEBOUNCE_H_THRES;
      gateInfo.start_pulse=SYS_STEP_COUNT;
    }
  }

  if(onSenseEdge)
  {
    if(!new_Sense)
    {//a pulse is completed -- end_pulse is the last HIGH sample (true edge)
      uint32_t diff=gateInfo.end_pulse-gateInfo.start_pulse;
      if( diff>minWidth && diff<maxWidth )
      {
        uint32_t middle_pulse=gateInfo.start_pulse+(diff>>1);
        (void)middle_pulse;
        // Reference the object off its TRAILING edge (end_pulse), as the
        // pre-debounce code did, so the calibrated stage_pulse_offset values
        // still line up. (Switching to the object centre -- middle_pulse -- is
        // checklist 5.3's separate, calibration-affecting change.)
        if(SYS_STEPPER_DISABLED==false && SYS_FREQ_STABLE)
          newPulseEvent(gateInfo.start_pulse,gateInfo.end_pulse,
                        gateInfo.end_pulse,diff);
      }
      gateInfo.start_pulse=SYS_STEP_COUNT;
    }
    else
    {
      gateInfo.end_pulse=SYS_STEP_COUNT;
    }
    gateInfo.cur_Sense=new_Sense;
  }
}




int stepRun=-1;
void StepGo()
{

  if((SYS_STEP_COUNT&1)==0)
  {
    // if(stepRun)
    // {
    //   if(stepRun>0)stepRun--;
    //   GPIOLS32_SET(STEPPER_PLS_PIN);
    // }
    GPIOLS32_SET(STEPPER_PLS_PIN);
  }
  else
  {
    GPIOLS32_CLR(STEPPER_PLS_PIN);
  }


  

}



void IRAM_ATTR onTimer()
{

  // static uint32_t cp0_regs[18];
  // GPIOLS32_SET(PIN_LED);

  // enable FPU
  // xthal_set_cpenable(1);
  // // Save FPU registers
  // xthal_save_cp0(cp0_regs);
  // uint32_t nextT=100;
  // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);
  



  SYS_STEP_COUNT++;

  //Step adv
  StepGo();



  GateSensing();

  Run_ACTS(SYS_STEP_COUNT);

  //sensor detection
  //Try run task


  
  // Restore FPU
  // xthal_restore_cp0(cp0_regs);
  // // and turn it back off
  // xthal_set_cpenable(0);
  // 
  // GPIOLS32_CLR(PIN_LED);

}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;


StaticJsonDocument <1024>doc;
StaticJsonDocument <1024>retdoc;



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

  if(recv_data)
    dbg_printf("recv_ERROR:%d %s dat:%s",errorcode,dataBuff,string((char*)recv_data,0,9).c_str());
  else 
    dbg_printf("recv_ERROR:%d %s",errorcode,dataBuff);
  if(commsErrorLatched==false)
  {
    commsErrorLatched=true;

    TaskQ2CommInfo *commInfo = TaskQ2CommInfoQ.getHead();
    if(commInfo){
      commInfo->type=TaskQ2CommInfo_Type::systemInfo;
      commInfo->log="Serial protocol error detected";
      TaskQ2CommInfoQ.pushHead();
    }

    if(sysinfo.state!=SYS_STATE::INSPECTION_MODE_ERROR)
    {
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::SERIAL_PROTOCOL_ERROR);
    }
  }
  return 0;
}

int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode!=1)
  {
    return -1;
  }


  doc.clear();
  retdoc.clear();
  DeserializationError error = deserializeJson(doc, raw);
  bool rspAck=false;
  bool doRsp=false;

  if(error)
  {
    dbg_printf("JSON parse error:%s", error.c_str());
    enterProtocolError(ERROR_TYPE::JSON_FORMAT_ERROR,raw,rawL);
    return -1;
  }

  const char* type = doc["type"];
  if(type==NULL)
  {
    dbg_printf("JSON missing type field");
    enterProtocolError(ERROR_TYPE::JSON_FORMAT_ERROR,raw,rawL);
    return -1;
  }

  HACK_cur_cmd_id=-1;
  if(doc["id"].is<int>()==true)
  {
    HACK_cur_cmd_id=doc["id"];
  }
  if(commsErrorLatched && strcmp(type,"RESET")!=0)
  {
    retdoc.clear();
    retdoc["type"]="resp";
    if(!doc["id"].isNull())
    {
      retdoc["id"]=doc["id"];
    }
    retdoc["ack"]=false;
    retdoc["err"]="serial_error_locked";
    uint8_t buff[256];
    int slen=serializeJson(retdoc,(char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);
    return 0;
  }
  // const char* id = doc["id"];
  if(strcmp(type,"RESET")==0)
  {
    handleResetCommand();
    return msg_printf("RESET_OK","");
  }
  else if(strcmp(type,"ask_JsonRaw_version")==0)
  {
    
    const char* _version = doc["version"];
    if(_version)
    {
      strcpy(peerVERSION,_version);
    }
    return this->rsp_JsonRaw_version();
  }
  // else if(strcmp(type,"rsp_JsonRaw_version")==0)
  // {
  //   const char* _version = doc["version"];
  //   strcpy(peerVERSION,_version);
  //   return 0;
  // }


  else if(strcmp(type,"trigCamPulse")==0)
  {
    int cam_PIN=PIN_O_CAM1;
    if(doc["cpin"].is<int>()==true)
    {
      cam_PIN=doc["cpin"];
    }
    int light_PIN=PIN_O_L1A;
    if(doc["lpin"].is<int>()==true)
    {
      light_PIN=doc["lpin"];
    }
    int Light_Delay=100;
    if(doc["light_delay"].is<int>()==true)
    {
      Light_Delay=doc["light_delay"];
    }


    int Light_Duration=100;
    if(doc["light_duration"].is<int>()==true)
    {
      Light_Duration=doc["light_duration"];
    }


    int trigger_id=924949;
    if(doc["trigger_id"].is<int>()==true)
    {
      trigger_id=doc["trigger_id"];
    }
    {
      TaskQ2CommInfo *commInfo = TaskQ2CommInfoQ.getHead();
      if(commInfo){
        commInfo->type=TaskQ2CommInfo_Type::btrigInfo;
        
        uint32_t time_us=esp_timer_get_time();
        commInfo->trig_time_us=time_us;
        commInfo->btrig_idx=1;
        commInfo->trig_id=trigger_id;
        TaskQ2CommInfoQ.pushHead();
      }
    }


    digitalWrite(cam_PIN,HIGH);
    delayMicroseconds(Light_Delay);
    digitalWrite(light_PIN,HIGH);
    delayMicroseconds(Light_Duration);
    digitalWrite(light_PIN,LOW);
    digitalWrite(cam_PIN,LOW);





    doRsp=rspAck=true;
  }
  else if(strcmp(type,"PIN_MODE")==0)
  {
    doRsp=true;
    int PIN_Mode=INPUT;
    if(doc["mode"].is<String>()==true)
    {
      String mode=doc["mode"];
      if(mode=="INPUT")
        PIN_Mode=INPUT;
      else if(mode=="OUTPUT")
        PIN_Mode=OUTPUT;
      else if(mode=="PULLUP")
        PIN_Mode=PULLUP;
      else if(mode=="PULLDOWN")
        PIN_Mode=PULLDOWN;
      else if(mode=="INPUT_PULLUP")
        PIN_Mode=INPUT_PULLUP;
      else if(mode=="INPUT_PULLDOWN")
        PIN_Mode=INPUT_PULLDOWN;
      else if(mode=="OPEN_DRAIN")
        PIN_Mode=OPEN_DRAIN;
    }


    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      pinMode(pin,PIN_Mode);
      rspAck=true;
    }
    else
    {
      rspAck=false;
    }
  }


  else if(strcmp(type,"PING")==0)
  {
    retdoc["type"]="PONG"; 
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"get_setup")==0)
  {
    retdoc["ver"]="0.0.0 Alpha";
    retdoc["name"]="uInspESP32";
    
    genMachineSetup(retdoc);

    
    doRsp=rspAck=true;

  }
  else if(strcmp(type,"set_setup")==0)
  {
    retdoc["type"]="set_setup";

    setMachineSetup(doc);

    // Opt-in commit. Without "persist":true this behaves exactly as before --
    // RAM only, gone at power-off -- so probing/jogging during setup doesn't
    // burn flash write cycles.
    if(doc["persist"].is<bool>() && doc["persist"].as<bool>())
    {
      retdoc["persisted"]=MachineConfig::save();
    }

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"save_setup")==0)
  {
    retdoc["type"]="save_setup";
    retdoc["persisted"]=MachineConfig::save();
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"clear_saved_setup")==0)
  {
    // Wipes NVS only. The running values stay put, so this cannot disturb a
    // machine mid-run; the compiled defaults come back on the next boot.
    retdoc["type"]="clear_saved_setup";
    retdoc["cleared"]=MachineConfig::clear();
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"reset_running_stat")==0)
  {

    SEL1_Count=SEL2_Count=SEL3_Count=NA_Count=0;

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"get_running_stat")==0)
  {

    {
      JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");

      for(int i=0;i<ERROR_HIST.size();i++)
      {
        jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
      }
    }


    JsonObject jCountInfo  = retdoc.createNestedObject("count");
    jCountInfo["SEL1"]=SEL1_Count;
    jCountInfo["SEL2"]=SEL2_Count;
    jCountInfo["SEL3"]=SEL3_Count;
    jCountInfo["NA"]=NA_Count;

    //current state
    retdoc["state"]=(int)sysinfo.state;

    retdoc["plateFreq"]=SYS_TAR_FREQ;//SYS_CUR_FREQ;
    // if(SEL1_ACT_COUNTDOWN>=0)
    // {
    // }
    retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;

    // retdoc["plateFreq"]=NA_Count;


    doRsp=rspAck=true;

  }
  else if(strcmp(type,"report")==0)
  {
    int tid=(doc["tid"].is<int>()==true)?doc["tid"]:-1;
    int cat=(doc["cat"].is<int>()==true)?doc["cat"]:-1;

    pipeLineInfo *tarP=NULL;

    if(tid!=-1 && cat!=-1)
    for (int i = 0; i < RBuf.size(); i++)
    {
      pipeLineInfo *pipe = RBuf.getTail(i);
      if (pipe == NULL)
        break;

      if(pipe->tid==tid)
      {
        tarP=pipe;
        break;
      }
      if(pipe->insp_status==insp_status_UNSET)
      {
        pipe->insp_status=insp_status_SKIP;
      }

      // if(i>30)
      // {//only check the last 30 info in the queue
      //   break;
      // }
    }

    if(tarP)
    {
      uint32_t pressure=tarP->gate_pulse+STAGE_PULSE_OFFSET.SWITCH-SYS_STEP_COUNT;
      // if(pressure<1000)
      // {
      //   SETUP_TAR_FREQ=SETUP_TAR_FREQ*19/20;
      // }
      retdoc["tr"]=pressure;
      tarP->insp_status=cat;
      rspAck=true;
    }
    else
    {
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_MATCHES_NO_OBJECT);
      rspAck=false;
    }







    doRsp=false;

  }

  else if(strcmp(type,"clear_error")==0)
  {
    RESET_ALL_PIPELINE_QUEUE(); 

    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);

    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"clear_error_history")==0)
  {
    ERROR_HIST.clear();
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"io_trace_arm")==0)
  {
    // Disarm before clearing so an in-flight ISR event lands in the cleared
    // buffer, never after the arm. Order: off -> clear -> on.
    IO_TRACE_ARMED=false;
    IO_TRACE.clear();
    IO_TRACE_ARMED=true;
    retdoc["armed"]=true;
    retdoc["cap"]=(int)120;
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"io_trace_stop")==0)
  {
    IO_TRACE_ARMED=false;
    retdoc["n"]=(int)IO_TRACE.size();
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"io_trace_dump")==0)
  {
    // Freeze, then hand-serialize into a dedicated buffer: retdoc is only 1KB,
    // too small for a full trace, and each row is a flat [pulse,pin,val,tid].
    IO_TRACE_ARMED=false;
    int n=IO_TRACE.size();
    static char db[3072];
    int cid=(doc["id"].is<int>()==true)?(int)doc["id"]:0;
    int off=snprintf(db,sizeof(db),
        "{\"type\":\"io_trace_dump\",\"id\":%d,\"ack\":true,\"n\":%d,\"ev\":[",
        cid,n);
    int emitted=0;
    for(int i=0;i<n && off<(int)sizeof(db)-48;i++)
    {
      IOTraceEvt *e=IO_TRACE.getTail(i);
      if(!e)break;
      off+=snprintf(db+off,sizeof(db)-off,"%s[%u,%d,%d,%u]",
          emitted?",":"",(unsigned)e->pulse,(int)e->pin,
          (int)e->val,(unsigned)e->tid);
      emitted++;
    }
    off+=snprintf(db+off,sizeof(db)-off,"],\"emitted\":%d}",emitted);
    send_json_string(0,(uint8_t*)db,off,0);
    doRsp=false;   // already sent above
  }

  else if(strcmp(type,"PIN_ON")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      digitalWrite(pin,HIGH);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"PIN_OFF")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];

      digitalWrite(pin,LOW);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"enter_insp_mode")==0)
  {

    SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"exit_insp_mode")==0)
  {

    SYS_STATE_Transfer(SYS_STATE_ACT::EXIT_INSPECTION_MODE);
    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"trig_phamton_pulse")==0)
  {
    uint32_t tatPulse= SYS_STEP_COUNT-STAGE_PULSE_OFFSET.L1A_on+_PLAT_DIST_step(3000);

    newPulseEvent(tatPulse-10, tatPulse+10, tatPulse,20);
    
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"sel1_act_countdown")==0 || strcmp(type,"set_sel1_cd")==0)
  {
    
    if(doc["count"].is<int>()==true)
    {
      SEL1_ACT_COUNTDOWN=doc["count"];
    }
    else
    {
      SEL1_ACT_COUNTDOWN=0;
    }
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"get_sel1_cd")==0)
  {
    retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"stepper_enable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,STEPPER_EN_ACTIVATION);
    SYS_STEPPER_DISABLED=false;
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"stepper_disable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,!STEPPER_EN_ACTIVATION);
    SYS_STEPPER_DISABLED=true;
    doRsp=rspAck=true;
  }



  else if(strcmp(type,"sel_act")==0)
  {
    int idx=doc["idx"];
    int delay_ms=10;

    if(doc["delay"].is<int>()==true)
    {
      delay_ms=doc["delay"];
    }

    switch(idx)
    {
      case 1:
      digitalWrite(PIN_O_SEL1, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL1, 0);
      rspAck=true;
      break;
      case 2:
      digitalWrite(PIN_O_SEL2, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL2, 0);
      rspAck=true;
      break;
      case 3:
      digitalWrite(PIN_O_SEL3, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL3, 0);
      rspAck=true;
      break;
    }
    doRsp=true;
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
    retdoc["id"]=doc["id"];
    retdoc["ack"]=rspAck;
    
    uint8_t buff[1024];
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);
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
}

void MData_JR::handleResetCommand()
{
  bool wasLatched=commsErrorLatched || hasProtocolError();
  commsErrorLatched=false;
  clearProtocolError();
  doDataLog=false;

  // A RESET can arrive with the link in an unknown state; don't leave an
  // actuator held on while we sort the protocol out.
  ALL_OUTPUTS_SAFE();

  if(wasLatched && sysinfo.state==SYS_STATE::INSPECTION_MODE_ERROR)
  {
    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);
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

  return false;
}



RingBuf_Static<struct TaskQ2CommInfo,20,uint8_t> AUX2CommInfoQ;
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
          // case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_ENC :
          //   while(mstp.EncV<info.wait_enc.value)
          //   {
          //     vTaskDelay(1 / portTICK_RATE_MS);
          //   }

          // break;

          case AUX_TASK_INFO_TYPE::AUX_IO_CTRL :

            if(info.ioCtrl.CID[0])
            {
              //send camera idx 
              struct TaskQ2CommInfo tinfo={
                .type=TaskQ2CommInfo_Type::trigInfo,
                // .camera_id=string(info.ioCtrl.CID),
                // .trig_tag=string(info.ioCtrl.TTAG),
                .trig_id=info.ioCtrl.TID};



              xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);//LOCK
              TaskQ2CommInfo* Qhead=NULL;
              while( (Qhead=AUX2CommInfoQ.getHead()) ==NULL)
              {
                yield();
              }
              *Qhead=tinfo;
              AUX2CommInfoQ.pushHead();
              xSemaphoreGive(AUX2Comm_Lock);//UNLOCK
            

            }
            // if(info.ioCtrl.state==1)
            // {
            //   mstp.static_Pin_info|=(uint32_t)1<<info.ioCtrl.pin;
            // }
            // if(info.ioCtrl.state==0)
            // {
            //   mstp.static_Pin_info&=~(((uint32_t)1)<<info.ioCtrl.pin);
            // }
          break;


          case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH :

              struct TaskQ2CommInfo tinfo={
              .type=TaskQ2CommInfo_Type::respFrame,
              .isAck=true,
              .resp_id=info.wait_fin.cmd_id
              };

              xSemaphoreTake(AUX2Comm_Lock, portMAX_DELAY);//LOCK
              TaskQ2CommInfo* Qhead=NULL;
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


int rzERROR=0;
void firmwareSetup()
{
  
  // noInterrupts();
  Serial.begin(115200);//230400);
  // Serial.begin(460800);
  Serial.setRxBufferSize(500);
  // Serial.setHwFlowCtrlMode(0);
  // // setup_comm();

  // Must run before the timer is armed: the pulse offsets and plate frequency
  // it restores are what everything below derives its timing from.
  MachineConfig::begin();
  // Publish whatever begin() loaded (or the compiled defaults) to the ISR
  // snapshot before onTimer can read it.
  STAGE_PULSE_OFFSET_publish();

  timer = timerBegin(0, 80*1000*1000/_TICK2SEC_BASE_, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 7000, true);
  // timerAlarmEnable(timer);
  timerAlarmDisable(timer);


  AUX2Comm_Lock = xSemaphoreCreateMutex();
  for(int i=0;i<AUX_COUNT;i++)
  {
    AUXTaskQueue[i] = xQueueCreate(20 /* Number of queue slots */, sizeof(AUX_TASK_INFO));
    xTaskCreatePinnedToCore(&AUX_task, "AUX_task", 2048, (void*)&AUXTaskQueue[i], 1, NULL, 0);

  }

  pinMode(PIN_LED, OUTPUT);


  pinMode(STEPPER_PLS_PIN, OUTPUT);
  pinMode(STEPPER_DIR_PIN, OUTPUT);
  pinMode(STEPPER_EN_PIN, OUTPUT);

  digitalWrite(STEPPER_EN_PIN,STEPPER_EN_ACTIVATION);
  SYS_STEPPER_DISABLED=false;
  



  pinMode(PIN_O_L1A, OUTPUT);
  pinMode(PIN_O_CAM1, OUTPUT);

  pinMode(PIN_O_L2A, OUTPUT);
  pinMode(PIN_O_CAM2, OUTPUT);


  pinMode(PIN_O_SEL1, OUTPUT);
  pinMode(PIN_O_SEL2, OUTPUT);
  pinMode(PIN_O_SEL3, OUTPUT);

  pinMode(PIN_I_GATE, INPUT_PULLUP);

  // CameraIDList[0]="ABC";
  // CameraIDList[1]="DEF";



  SYS_STATE_Transfer(SYS_STATE_ACT::INIT_OK);
}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    yield();
  }
}

MSTP_SegCtx ctx[10];


string toFixed(float num,int powNum=100)
{
  int ipnum=round(num*powNum);
  int inum=ipnum/powNum;

  string istr=std::to_string(inum);
  int pnum=(ipnum%powNum);

  string pstr=std::to_string(pnum+powNum);

  string resStr=istr+pstr;
  resStr[istr.length()]='.';
  return resStr;
}


bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


static uint8_t recvBuf[20];
void firmwareLoop()
{
  // Drain any error the step ISR raised. Its outputs are already safe and new
  // object detection is already blocked; what is left is the state transition,
  // which has to run out here where Arduino GPIO calls are legal.
  {
    GEN_ERROR_CODE isr_ecode=(GEN_ERROR_CODE)PENDING_ISR_ERROR;
    if(isr_ecode!=GEN_ERROR_CODE::NOP)
    {
      PENDING_ISR_ERROR=GEN_ERROR_CODE::NOP;
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)isr_ecode);
    }
  }

  SYS_STATE_Transfer(SYS_STATE_ACT::NOP);
  djrl.loop();
  
  // Periodic system time debug print (1 second interval)
  {
    static unsigned long lastPrintTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastPrintTime >= 1000) {
      djrl.dbg_printf("SYSTIME: %lu ms", currentTime);
      lastPrintTime = currentTime;
    }
  }
  {
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      size_t recvLen = Serial.read(recvBuf,sizeof(recvBuf));
      //
      if(recvLen==0)continue;
      // djrl.dbg_printf("recvLen:%d",recvLen);
      djrl.recv_data((uint8_t*)recvBuf,(int)recvLen);
      if(doDataLog)
      {
        size_t printableLen = (recvLen < sizeof(recvBuf)-1)?recvLen:(sizeof(recvBuf)-1);
        recvBuf[printableLen]='\0';
        for(size_t i=0;i<printableLen;i++) 
        {
          if(recvBuf[i]=='"')
            recvBuf[i]='\'';
        }     
        
        djrl.dbg_printf(">%s",(char*)recvBuf);
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
      // Camera triggers first: they are what the host is waiting on to give a
      // verdict before the part reaches the selector, so they must not queue
      // behind chattier diagnostics.
      if(0!=(ISRTrigQ.size()))
      {
        ISRTrigInfo trig=*ISRTrigQ.getTail();
        ISRTrigQ.consumeTail();

        retdoc.clear();
        retdoc["type"]="bTrigInfo";
        retdoc["tidx"]=trig.btrig_idx;
        retdoc["usH"]=(uint32_t)(trig.trig_time_us>>32);
        retdoc["usL"]=(uint32_t)(trig.trig_time_us&((uint32_t)0-1));
        retdoc["tid"]=trig.trig_id;
        retdoc["Qs"]=RBuf.size();
        int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
        djrl.send_json_string(0,buff,slen,0);
        continue;
      }

      bool hasNewInfo=false;
      TaskQ2CommInfo info;
      if(hasNewInfo ==false && 0!=(TaskQ2CommInfoQ.size()))
      {
        info=*TaskQ2CommInfoQ.getTail();
        TaskQ2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }


      if(hasNewInfo ==false && 0!=(AUX2CommInfoQ.size()))
      {
        info=*AUX2CommInfoQ.getTail();
        AUX2CommInfoQ.consumeTail();
        hasNewInfo=true;
      }




      if(hasNewInfo==false)break;




      retdoc.clear();
      // retdoc["tag"]="s_Step_"+std::to_string((int)info.step);
      // retdoc["trigger_id"]=info.step;
      switch (info.type)
      {
        case TaskQ2CommInfo_Type::trigInfo :
        {
          retdoc["type"]="TriggerInfo"; 
          retdoc["camera_id"]=info.camera_id;


          string tag = info.trig_tag;
          // if(info.curFreq==info.curFreq)
          //   replace(tag,"$s_PFQ", "s_PFQ="+toFixed(info.curFreq,100));

          retdoc["tag"]=tag;
          retdoc["trigger_id"]=info.trig_id;



          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }
        
        case TaskQ2CommInfo_Type::btrigInfo :
        {
          retdoc["type"]="bTrigInfo"; 
          retdoc["tidx"]=info.btrig_idx;
          retdoc["usH"]=info.trig_time_us>>32;
          retdoc["usL"]=info.trig_time_us&((uint32_t)0-1);

          retdoc["tid"]=info.trig_id;
          retdoc["Qs"]=RBuf.size();
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case TaskQ2CommInfo_Type::systemInfo :
        {
          retdoc["type"]="systemInfo"; 

          retdoc["state"]=(int)sysinfo.state;
          

          {
            JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");

            for(int i=0;i<ERROR_HIST.size();i++)
            {
              jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
            }
          }


          retdoc["log"]=info.log;


          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case TaskQ2CommInfo_Type::ext_log :
        {

          djrl.dbg_printf("%s",info.log.c_str());

          break;
        }
      
        case TaskQ2CommInfo_Type::respFrame :
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



  static int subDiv=0;
  do{//timer freq ctrl
    subDiv=(subDiv+1)&(0xFF);
    if(subDiv!=0)break;
    if(SYS_CUR_FREQ==SYS_TAR_FREQ)
    {
      if(SYS_TAR_FREQ==0 && SYS_FREQ_STABLE==false)//just stable
      {
        ALL_OUTPUTS_SAFE();
      }
      SYS_FREQ_STABLE=true;
      break;
    }
    SYS_FREQ_STABLE=false;
    bool TimerNeedsStart=false;
    if(SYS_CUR_FREQ==0)
    {
      TimerNeedsStart=true;
    }
    if(SYS_CUR_FREQ>SYS_TAR_FREQ)
    {
      if(SYS_TAR_FREQ==0 && SYS_CUR_FREQ<10)
      {
        SYS_CUR_FREQ=0;
      }
      else
      {
        SYS_CUR_FREQ-=SYS_FREQ_ADV_STEP;
        if(SYS_CUR_FREQ<SYS_TAR_FREQ)
        {
          SYS_CUR_FREQ=SYS_TAR_FREQ;
        }
      }
    }
    else
    {
      SYS_CUR_FREQ+=SYS_FREQ_ADV_STEP;
      if(SYS_CUR_FREQ>SYS_TAR_FREQ)
      {
        SYS_CUR_FREQ=SYS_TAR_FREQ;
      }
    }



    if(SYS_CUR_FREQ==0)
    {
      timerAlarmDisable(timer);
    }
    else
    {
      timerAlarmWrite(timer, (uint64_t)((_TICK2SEC_BASE_>>1)/SYS_CUR_FREQ), true);
    }

    if(TimerNeedsStart)
    {
      timerAlarmEnable(timer);
    }


  }while(0);
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



  {//clean up finished 
    pipeLineInfo * tail;
    while (tail=RBuf.getTail())
    {
      // task->src->insp_status = insp_status_DEL;
      if(tail->insp_status == insp_status_DEL)
      {
        RBuf.consumeTail();
      }
      else
      {
        break;
      }
    }
  }

  // {
  //   if(SEL1_ACT_COUNTDOWN==0)
  //   {
      
  //     SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::SEL_ACT_LIMIT_REACHES);
  //   }
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

  // jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  JsonObject jSPO  = jdoc.createNestedObject("stage_pulse_offset");
  jSPO["L1A_on"]=STAGE_PULSE_OFFSET.L1A_on;
  jSPO["L1A_off"]=STAGE_PULSE_OFFSET.L1A_off;
  jSPO["CAM1_on"]=STAGE_PULSE_OFFSET.CAM1_on;
  jSPO["CAM1_off"]=STAGE_PULSE_OFFSET.CAM1_off;

  jSPO["L2A_on"]=STAGE_PULSE_OFFSET.L2A_on;
  jSPO["L2A_off"]=STAGE_PULSE_OFFSET.L2A_off;
  jSPO["CAM2_on"]=STAGE_PULSE_OFFSET.CAM2_on;
  jSPO["CAM2_off"]=STAGE_PULSE_OFFSET.CAM2_off;

  jSPO["SWITCH"]=STAGE_PULSE_OFFSET.SWITCH;
  
  jSPO["SEL1_on"]=STAGE_PULSE_OFFSET.SEL1_on;
  jSPO["SEL1_off"]=STAGE_PULSE_OFFSET.SEL1_off;
  jSPO["SEL2_on"]=STAGE_PULSE_OFFSET.SEL2_on;
  jSPO["SEL2_off"]=STAGE_PULSE_OFFSET.SEL2_off;
  jSPO["SEL3_on"]=STAGE_PULSE_OFFSET.SEL3_on;
  jSPO["SEL3_off"]=STAGE_PULSE_OFFSET.SEL3_off;



  // auto obj=jdoc.createNestedObject("obj");

  jdoc["plateFreq"]=SETUP_TAR_FREQ;
  jdoc["minDetectTimeSep_us"]=SYS_MIN_PULSE_TIME_SEP_us;

  jdoc["pulse_minWidth"]=minWidth;
  jdoc["pulse_maxWidth"]=maxWidth;

  jdoc["gate_debounce_rise"]=DEBOUNCE_H_THRES;
  jdoc["gate_debounce_fall"]=DEBOUNCE_L_THRES;

  // Lets the host tell the two machines apart and see whether what it is
  // reading came from NVS or is just the compiled fallback.
  jdoc["machine_id"]=MachineConfig::machineId();
  jdoc["cfg_from_nvs"]=MachineConfig::isLoadedFromNVS();


  {
    JsonArray jERROR_HIST = jdoc.createNestedArray("ERROR_HIST");

    for(int i=0;i<ERROR_HIST.size();i++)
    {
      jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
    }
  }


  // jdoc["SYS_TAR_FREQ"]=SYS_TAR_FREQ;
  jdoc["curState"]=(int)sysinfo.state;
  jdoc["SYS_STEP_COUNT"]=(int)SYS_STEP_COUNT;

  
}




#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}


void setMachineSetup(JsonDocument &jdoc)
{
  if(jdoc["CAM1_ID"].is<const char*>()  )
  { 
    CAM1_ID=jdoc["CAM1_ID"].as<const char*>();
  }

  if(jdoc["CAM2_ID"].is<const char*>()  )
  { 
    CAM2_ID=jdoc["CAM2_ID"].as<const char*>();
  }

  if(jdoc["CAM1_Tags"].is<const char*>()  )
  { 
    CAM1_Tags=jdoc["CAM1_Tags"].as<const char*>();
  }

  if(jdoc["CAM2_Tags"].is<const char*>()  )
  { 
    CAM2_Tags=jdoc["CAM2_Tags"].as<const char*>();
  }
  

  if(jdoc["machine_id"].is<const char*>())
  {
    MachineConfig::setMachineId(jdoc["machine_id"].as<const char*>());
  }

  JSON_SETIF_ABLE(SETUP_TAR_FREQ,jdoc,"plateFreq");
  JSON_SETIF_ABLE(SYS_MIN_PULSE_TIME_SEP_us,jdoc,"minDetectTimeSep_us");
  JSON_SETIF_ABLE(stepRun,jdoc,"stepRun");

  JSON_SETIF_ABLE(minWidth,jdoc,"pulse_minWidth");
  JSON_SETIF_ABLE(maxWidth,jdoc,"pulse_maxWidth");
  JSON_SETIF_ABLE(DEBOUNCE_H_THRES,jdoc,"gate_debounce_rise");
  JSON_SETIF_ABLE(DEBOUNCE_L_THRES,jdoc,"gate_debounce_fall");
  // A threshold of 0 would underflow the down-counter into a ~65k-sample stall;
  // 1 means "no debounce" (accept on the first sample), the sane floor.
  if(DEBOUNCE_H_THRES<1)DEBOUNCE_H_THRES=1;
  if(DEBOUNCE_L_THRES<1)DEBOUNCE_L_THRES=1;
  

  if (jdoc.containsKey("stage_pulse_offset")) {
    JsonObject jSPO  = jdoc["stage_pulse_offset"];
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L1A_on,jSPO,"L1A_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L1A_off,jSPO,"L1A_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM1_on,jSPO,"CAM1_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM1_off,jSPO,"CAM1_off");


    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L2A_on,jSPO,"L2A_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L2A_off,jSPO,"L2A_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM2_on,jSPO,"CAM2_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM2_off,jSPO,"CAM2_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SWITCH,jSPO,"SWITCH");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL1_on,jSPO,"SEL1_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL1_off,jSPO,"SEL1_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL2_on,jSPO,"SEL2_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL2_off,jSPO,"SEL2_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL3_on,jSPO,"SEL3_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL3_off,jSPO,"SEL3_off");

    // Hand the freshly edited offsets to the ISR as one atomic snapshot. The
    // field-by-field writes above ran with interrupts enabled, so the ISR may
    // have registered objects against the old snapshot until this point -- that
    // is fine, and far cheaper than masking the step timer through 15 writes.
    STAGE_PULSE_OFFSET_publish();
  }




}

