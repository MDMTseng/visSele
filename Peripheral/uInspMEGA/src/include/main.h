#ifndef _MAIN_CPP_
#define _MAIN_CPP_

#include "Websocket.hpp"
#include "websocket_FI.hpp"
#include "RingBuf.hpp"
#include <avr/wdt.h>
#include <inttypes.h>
#include "Arduino.h"





const uint32_t subPulseSkipCount = 16;                                     //We don't do task processing for every hardware pulse, so we can save computing power for other things
const uint32_t perRevPulseCount_HW = (uint32_t)2400 * 16;                  //the real hardware pulse count per rev
const uint32_t perRevPulseCount = perRevPulseCount_HW / subPulseSkipCount; // the software pulse count that processor really care

struct sharedInfo
{
  uint16_t skippedPulse;
};


struct sharedInfo* get_SharedInfo();


void RESET_GateSensing();


#define TIMER_SET_ISR(TN,PRE_SCALER) \
  void timer##TN##_HZ(int HZ){\
    TIMSK##TN=(HZ==0)?0:(1 << OCIE##TN##A);\
    uint16_t OCR =  16000000 / PRE_SCALER / HZ;\
    OCR##TN##A = OCR;\
    if(TCNT##TN>OCR){\
      TCNT##TN=0;\
    }\
  } \
  void timer##TN##Setup(int HZ)\
  {\
    noInterrupts();\
    TCCR##TN##A = 0;\
    TCCR##TN##B = 0;\
    TCCR##TN##B |= (1 << WGM##TN##2);\
    if(PRE_SCALER==1)TCCR##TN##B        |= (1 << CS##TN##0);\
    else if(PRE_SCALER==8)TCCR##TN##B   |= (1 << CS##TN##1);\
    else if(PRE_SCALER==64)TCCR##TN##B  |= (1 << CS##TN##1) | (1 << CS##TN##0);\
    else if(PRE_SCALER==256)TCCR##TN##B |= (1 << CS##TN##2);\
    else if(PRE_SCALER==1024)TCCR##TN##B|= (1 << CS##TN##2) | (1 << CS##TN##0);\
    TIMSK##TN |= (1 << OCIE##TN##A);\
    interrupts();\
    timer##TN##_HZ(HZ);\
  }


enum class PulseTimeSyncInfo_State
{

  INIT = 0,
  SETUP_preBaseTime = 1,
  SETUP_preBasePulse = 2,
  SETUP_BaseTime = 3,
  SETUP_BasePulse = 4,
  SETUP_DATA_CALC = 5,
  SETUP_Verify = 6,
  READY = 100,
};

// enum class SYS_STATUS_BIT
// {

//   SPEED_INIT = 0,
//   SPEED_STABLE = 3,
//   INSP_RESULT_TIME_SYNCING = 2,
//   ERROR_ACTION_CLEAR_SPIN = 8,
// };


#define SMM_STATE_DECLARE(MACROX) \
  MACROX(INIT                      ,   0,0) \
  MACROX(WAIT_FOR_CLIENT_CONNECTION,   1,0) \
  MACROX(             DATA_EXCHANGE,   2,0) \
  MACROX(     WAIT_FOR_PULSE_STABLE,   3,0) \
  MACROX(        PULSE_TIME_SYNCING,   4,0) \
  MACROX(     INSPECTION_MODE_TEST , 140,0) \
  MACROX(     INSPECTION_MODE_READY, 101,0) \
  MACROX(     INSPECTION_MODE_ERROR, 112,0) \
  MACROX(     INSPECTION_MODE_FATAL, 113,0) \
  MACROX(                      IDLE, 100,0) \
  MACROX(                       NOP, 200,0) \


#define SMM_STATE_ACT_DECLARE(MACROX) \
  MACROX(                               NOP, 200,0) \
  MACROX(                           INIT_OK,   0,0) \
  MACROX(                  CLIENT_CONNECTED,   1,0) \
  MACROX(               CLIENT_DISCONNECTED, 101,0) \
  MACROX(                  DATA_EXCHANGE_OK,   2,0) \
  MACROX(        ENTER_INSPECTION_TEST_MODE,  50,0) \
  MACROX(  PREPARE_TO_ENTER_INSPECTION_MODE,   5,0) \
  MACROX(              EXIT_INSPECTION_MODE, 105,0) \
  MACROX(                  INSPECTION_ERROR,   6,0) \
  MACROX(           INSPECTION_ERROR_REDEEM, 106,0) \
  MACROX(                  INSPECTION_FATAL,   7,0) \
  MACROX(           INSPECTION_FATAL_REDEEM, 107,0) \
  MACROX(                      PULSE_STABLE,   3,0) \
  MACROX(                    PULSE_UNSTABLE, 103,0) \
  MACROX(                 PULSE_TIME_UNSYNC, 104,0) \
  MACROX(                   PULSE_TIME_SYNC,   4,0) \


#define SMM_GEN_ENUM_X(NAME,VALUE,X) NAME = VALUE ,
#define SMM_GEN_ENUM(ENUM_NAME,DECLARE_X) \
  enum class ENUM_NAME {\
    DECLARE_X(SMM_GEN_ENUM_X) \
  };\

SMM_GEN_ENUM(SYS_STATE,SMM_STATE_DECLARE)


SMM_GEN_ENUM(SYS_STATE_ACT,SMM_STATE_ACT_DECLARE)


#define SMM_STATE_TRANSFER_DECLARE(MX1,MX2,S,A) \
  MX1(S::INIT,\
    MX2(A::INIT_OK,                         S::WAIT_FOR_CLIENT_CONNECTION)\
    )\
  MX1(S::WAIT_FOR_CLIENT_CONNECTION,\
    MX2(A::CLIENT_CONNECTED,                S::DATA_EXCHANGE)\
    )\
  MX1(S::DATA_EXCHANGE,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::DATA_EXCHANGE_OK,                S::IDLE)\
    )\
  MX1(S::IDLE,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::PREPARE_TO_ENTER_INSPECTION_MODE,S::WAIT_FOR_PULSE_STABLE)\
    MX2(A::ENTER_INSPECTION_TEST_MODE,      S::INSPECTION_MODE_TEST)\
    )\
  \
  \ 
  MX1(S::INSPECTION_MODE_TEST,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::PULSE_STABLE,                    S::PULSE_TIME_SYNCING)\
    )\
  \
  \
  MX1(S::WAIT_FOR_PULSE_STABLE,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::PULSE_STABLE,                    S::PULSE_TIME_SYNCING)\
    )\
  MX1(S::PULSE_TIME_SYNCING,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::PULSE_UNSTABLE,                  S::WAIT_FOR_PULSE_STABLE)\
    MX2(A::PULSE_TIME_SYNC,                 S::INSPECTION_MODE_READY)\
    )\
  MX1(S::INSPECTION_MODE_READY,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::PULSE_UNSTABLE,                  S::WAIT_FOR_PULSE_STABLE)\
    MX2(A::PULSE_TIME_UNSYNC,               S::WAIT_FOR_PULSE_STABLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  MX1(S::INSPECTION_MODE_ERROR,\
    MX2(A::CLIENT_DISCONNECTED,             S::WAIT_FOR_CLIENT_CONNECTION)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR_REDEEM,         S::WAIT_FOR_PULSE_STABLE)\
    )\



#define PULSE_TIME_SYNC_USSHIFT 25
struct PulseTimeSyncInfo
{
  PulseTimeSyncInfo_State state;
  uint32_t basePulseCount;
  uint64_t basePulse_us;
  // uint64_t us_per_1024_pulses;
  uint64_t pulses_per_1shiftXus;

  uint32_t pre_basePulseCount;
  uint64_t pre_basePulse_us;
};



enum class GEN_ERROR_CODE
{
  RESET = 0,
  INSP_RESULT_HAS_NO_OBJECT = 1,
  OBJECT_HAS_NO_INSP_RESULT = 2,
  INSP_RESULT_COUNTER_ERROR = 3,
  INSP_RESULT_PULSE_TIME_OUT_OF_SYNC = 4,
  INSP_RESULT_HAS_NO_TIME_STAMP = 5,
};

struct SYS_INFO
{

  SYS_STATE pre_state;
  SYS_STATE state;
  int extra_code;
  int status;

  struct PulseTimeSyncInfo PTSyncInfo;
};




typedef struct run_mode_info
{
  enum RUN_MODE
  {
    INIT,
    NORMAL,
    TEST
  } mode;

  int misc_info;
  int misc_var;
  int misc_var2;
} run_mode_info;




#define insp_status_UNSET -2000 //mark the inspection result is yet arrive
#define insp_status_DEL 0xFE    // mark the object can be deleted

#define insp_status_NA -128 //insp_status_NA is just for unknown insp result
#define insp_status_OK 0
#define insp_status_NG -1

typedef struct InspResCount
{
  int64_t NA;
  int64_t OK;
  int64_t NG;
  int64_t ERR;
} InspResCount;



#define SARRL(SARR) (sizeof((SARR)) / sizeof(*(SARR)))




#define PIPE_INFO_LEN 30

#define STEPPER_PLS_PIN 22
#define STEPPER_DIR_PIN 23

#define LED_PIN 13
#define CAMERA_PIN 16
#define FEEDER_PIN 14
#define BACK_LIGHT_PIN 28
#define AIR_BLOW_OK_PIN 24
#define AIR_BLOW_NG_PIN 25
#define GATE_PIN 30

#define FAKE_GATE_PIN 31



struct ACT_INFO
{
  pipeLineInfo *src;
  int info;
  uint32_t targetPulse;
};



#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info, cusCode_task) \
  {                                                                 \
    ACT_INFO *task;                                                 \
    task = (rb).getHead();                                          \
    if (task)                                                       \
    {                                                               \
      task->targetPulse = (plinfo->gate_pulse + pulseOffset);       \
      task->src = plinfo;                                           \
      task->info = _info;                                           \
      cusCode_task(rb).pushHead();                                  \
    }                                                               \
  }



struct ACT_SCH
{
  RingBuf_Static<ACT_INFO, PIPE_INFO_LEN>
      ACT_BACKLight1H,
      ACT_BACKLight1L,
      ACT_CAM1H,
      ACT_CAM1L,
      ACT_SWITCH,
      ACT_SEL1H,
      ACT_SEL1L,
      ACT_SEL2H,
      ACT_SEL2L;
};
int Run_ACTS(uint32_t cur_pulse);

void setup_Stepper();

uint32_t loop_Stepper(uint32_t tar_pulseHZ,uint32_t pulseHZ_step);

uint32_t get_Stepper_pulse_count();

int task_newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width);

int EV_Axis0_Origin(uint32_t revCount);
void ETH_RESET();

bool* getSenseInvPtr();
#endif