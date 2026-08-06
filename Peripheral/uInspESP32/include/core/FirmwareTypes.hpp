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
  MACROX(       INSPECTION_MODE_CAL, 102,0) \
  MACROX(    INSPECTION_MODE_SPINUP, 103,0) \
  MACROX(     INSPECTION_MODE_RECAL, 104,0) \
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
  MACROX(                          CAL_DONE,   8,0) \
  MACROX(                        SPIN_READY,   9,0) \
  MACROX(                       RECAL_START,  10,0) \
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
    MX2(A::PREPARE_TO_ENTER_INSPECTION_MODE,S::INSPECTION_MODE_CAL)\
    MX2(A::ENTER_INSPECTION_TEST_MODE,      S::INSPECTION_MODE_TEST)\
    )\
  \
  MX1(S::INSPECTION_MODE_CAL,\
    MX2(A::CAL_DONE,                        S::INSPECTION_MODE_SPINUP)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  \
  MX1(S::INSPECTION_MODE_SPINUP,\
    MX2(A::SPIN_READY,                      S::INSPECTION_MODE_READY)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  \
  MX1(S::INSPECTION_MODE_TEST,\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    )\
  \
  MX1(S::INSPECTION_MODE_READY,\
    MX2(A::RECAL_START,                     S::INSPECTION_MODE_RECAL)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  \
  MX1(S::INSPECTION_MODE_RECAL,\
    MX2(A::CAL_DONE,                        S::INSPECTION_MODE_READY)\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR,                S::INSPECTION_MODE_ERROR)\
    )\
  MX1(S::INSPECTION_MODE_ERROR,\
    MX2(A::EXIT_INSPECTION_MODE,            S::IDLE)\
    MX2(A::INSPECTION_ERROR_REDEEM,         S::INSPECTION_MODE_CAL)\
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

// Declared as an X-macro, like SYS_STATE above, so the code that has to SHOW
// these can generate its own table from the same list. The host used to keep a
// hand-copied version; it went stale, and an operator met "code 14" with no
// text at the exact moment the machine had refused to start.
//
//   MACROX(NAME, VALUE, "what the operator should understand by it")
#define GEN_ERROR_CODE_DECLARE(MACROX) \
  MACROX(NOP,                                 -1, "no error") \
  MACROX(RESET,                                0, "reset") \
  MACROX(INSP_RESULT_MATCHES_NO_OBJECT,        1, "a verdict arrived for no known object") \
  MACROX(OBJECT_HAS_NO_INSP_RESULT,            2, "object reached SWITCH with no verdict") \
  MACROX(INSP_RESULT_COUNTER_ERROR,            3, "result counter error") \
  MACROX(INSP_RESULT_PULSE_TIME_OUT_OF_SYNC,   4, "result pulse time out of sync") \
  MACROX(INSP_RESULT_HAS_NO_TIME_STAMP,        5, "result carried no timestamp") \
  MACROX(INSP_CAM_TRIG_INFO_CANNOT_BE_SENT,   10, "cam_trig could not be sent") \
  /* A malformed or stray byte on the serial link. LATCHED: one bad frame  */ \
  /* stops the machine and it does not come back on its own.               */ \
  MACROX(SERIAL_PROTOCOL_ERROR,               11, "serial protocol error (latched)") \
  MACROX(HOST_LINK_TIMEOUT,                   12, "host link timeout") \
  /* The camera clock offset no longer describes these two clocks, and the */ \
  /* machine would have to guess which object a frame belongs to. Stopping */ \
  /* is the correct answer: a wrong guess mis-sorts a part silently.       */ \
  MACROX(CAM_CLOCK_LOST,                      13, "camera clock lost") \
  /* The startup clock calibration did not converge, so the machine never  */ \
  /* had a usable offset to begin with. Refusing to start is the point.    */ \
  MACROX(CAM_CLOCK_CAL_FAILED,                14, "clock calibration did not converge") \
  /* The plate never reached its target speed. The ramp is deterministic   */ \
  /* arithmetic, so this means the machine is not doing what it was told.  */ \
  MACROX(PLATE_SPINUP_TIMEOUT,                15, "plate never reached target speed") \
  MACROX(SEL_ACT_LIMIT_REACHES,             0xff, "SEL actuation limit reached")

#define GEN_ERROR_CODE_ENUM_X(NAME,VALUE,TEXT) NAME = VALUE ,
enum class GEN_ERROR_CODE
{
  GEN_ERROR_CODE_DECLARE(GEN_ERROR_CODE_ENUM_X)
};
#undef GEN_ERROR_CODE_ENUM_X

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
