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

const uint8_t g_max_frame_rate = 40;


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

enum class SYS_STATUS_BIT
{

  SPEED_INIT = 0,
  SPEED_STABLE = 3,
  INSP_RESULT_TIME_SYNCING = 2,
  ERROR_ACTION_CLEAR_SPIN = 8,
};

enum class SYS_STATE
{
  NOP = 200,
  INIT = 0,
  WAIT_FOR_CLIENT_CONNECTION = 1,
  DATA_EXCHANGE = 2,
  WAIT_FOR_PULSE_STABLE = 3,
  PULSE_TIME_SYNCING = 4,
  READY = 100
};

enum class SYS_STATE_ACT
{
  NOP = 200,
  INIT_OK = 0,
  CLIENT_CONNECTED = 1,
  CLIENT_DISCONNECTED = 101,
  DATA_EXCHANGE_OK = 2,
  PULSE_STABLE = 3,
  PULSE_UNSTABLE = 103,
  PULSE_TIME_UNSYNC = 104,
  PULSE_TIME_SYNC = 4
};

struct PulseTimeSyncInfo
{
  PulseTimeSyncInfo_State state;
  uint32_t basePulseCount;
  uint32_t basePulse_us;
  // uint64_t us_per_1024_pulses;
  uint64_t pulses_per_1048676us; //2^10

  uint32_t pre_basePulseCount;
  uint64_t pre_basePulse_us;
};

struct SYS_INFO
{

  SYS_STATE pre_state;
  SYS_STATE state;
  int status;
  struct PulseTimeSyncInfo PTSyncInfo;
};




enum class GEN_ERROR_CODE
{
  RESET = 0,
  INSP_RESULT_HAS_NO_OBJECT = 1,
  OBJECT_HAS_NO_INSP_RESULT = 2,
  INSP_RESULT_COUNTER_ERROR = 3,
  INSP_PULSE_TIME_OUT_OF_SYNC = 4,
};

enum class ERROR_ACTION_TYPE
{
  NOP = 0,
  ALL_STOP = 1,
  FREE_SPIN_2_REV = 2,
  PULSE_TIME_RESYNC = 3,
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
  uint64_t NA;
  uint64_t OK;
  uint64_t NG;
  uint64_t ERR;
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
      ACT_CAM1,
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



#endif