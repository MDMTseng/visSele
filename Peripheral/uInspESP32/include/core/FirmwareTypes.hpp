#pragma once

#include <cstdint>

#define PRTF_B2b_PAT "%c%c%c%c%c%c%c%c"
#define PRTF_B2b(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

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

#define SMM_STATE_DECLARE(MACROX) \
  MACROX(INIT                      ,   0,0) \
  MACROX(     INSPECTION_MODE_TEST , 140,0) \
  MACROX(     INSPECTION_MODE_READY, 101,0) \
  MACROX(     INSPECTION_MODE_ERROR, 112,0) \
  MACROX(     INSPECTION_MODE_FATAL, 113,0) \
  MACROX(                      IDLE, 100,0) \
  MACROX(                       NOP, 200,0)

#define SMM_STATE_ACT_DECLARE(MACROX) \
  MACROX(                               NOP, 200,0) \
  MACROX(                           INIT_OK,   0,0) \
  MACROX(        ENTER_INSPECTION_TEST_MODE,  50,0) \
  MACROX(  PREPARE_TO_ENTER_INSPECTION_MODE,   5,0) \
  MACROX(              EXIT_INSPECTION_MODE, 105,0) \
  MACROX(                  INSPECTION_ERROR,   6,0) \
  MACROX(           INSPECTION_ERROR_REDEEM, 106,0) \
  MACROX(                  INSPECTION_FATAL,   7,0) \
  MACROX(           INSPECTION_FATAL_REDEEM, 107,0)

#define SMM_GEN_ENUM_X(NAME,VALUE,X) NAME = VALUE ,
#define SMM_GEN_ENUM(ENUM_NAME,DECLARE_X) \
  enum class ENUM_NAME {\
    DECLARE_X(SMM_GEN_ENUM_X) \
  };

SMM_GEN_ENUM(SYS_STATE,SMM_STATE_DECLARE)
SMM_GEN_ENUM(SYS_STATE_ACT,SMM_STATE_ACT_DECLARE)

#define SMM_STATE_TRANSFER_DECLARE(MX1,MX2,S,A) \
  MX1(S::INIT,\
    MX2(A::INIT_OK,                         S::IDLE)\
    )\
  MX1(S::IDLE,\
    MX2(A::PREPARE_TO_ENTER_INSPECTION_MODE,S::INSPECTION_MODE_READY)\
    MX2(A::ENTER_INSPECTION_TEST_MODE,      S::INSPECTION_MODE_TEST)\
    )\
  \
  MX1(S::INSPECTION_MODE_TEST,\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    )\
  \
  MX1(S::INSPECTION_MODE_READY,\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  MX1(S::INSPECTION_MODE_ERROR,\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR_REDEEM,         S::INSPECTION_MODE_READY)\
    )

#define PULSE_TIME_SYNC_USSHIFT 25
struct PulseTimeSyncInfo
{
  PulseTimeSyncInfo_State state;
  uint32_t basePulseCount;
  uint64_t basePulse_us;
  uint64_t pulses_per_1shiftXus;
  uint32_t pre_basePulseCount;
  uint64_t pre_basePulse_us;
};

enum class GEN_ERROR_CODE
{
  NOP=-1,
  RESET = 0,
  INSP_RESULT_MATCHES_NO_OBJECT = 1,
  INSP_CAM_TRIG_INFO_CANNOT_BE_SENT = 10,
  SERIAL_PROTOCOL_ERROR = 11,
  OBJECT_HAS_NO_INSP_RESULT = 2,
  INSP_RESULT_COUNTER_ERROR = 3,
  INSP_RESULT_PULSE_TIME_OUT_OF_SYNC = 4,
  INSP_RESULT_HAS_NO_TIME_STAMP = 5,
  SEL_ACT_LIMIT_REACHES=0xff,
};

struct SYS_INFO
{
  SYS_STATE pre_state;
  SYS_STATE state;
  int extra_code;
  int status;
  PulseTimeSyncInfo PTSyncInfo;
};

struct run_mode_info
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
};

constexpr int insp_status_SKIP = -2100;
constexpr int insp_status_UNSET = -2000;
constexpr int insp_status_DEL = -1000;

constexpr int PIPE_INFO_LEN = 100;
