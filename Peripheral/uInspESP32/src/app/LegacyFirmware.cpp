#include "app/LegacyFirmware.hpp"
#include "main.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include "comm/Data_Layer_Protocol.hpp"
#include "config/MachineConfig.hpp"
#include "driver/timer.h"
#include <esp_task_wdt.h>
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"      // rtc_gpio_hold_en -- IO0 low across a reset
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

// Per-output ON polarity. A driver board with common-anode (common +5V) opto
// inputs is energised by the GPIO sinking LOW, so "ON" is a logical notion:
// every firmware write goes through IO_ON/IO_OFF/io_drive below and IO_INV_MASK
// (bit set = ON is LOW) maps it to the wire. Configured per machine via
// set_setup {"io_on_level":{...}} and persisted with the rest of the config.
enum IO_IDX { IOI_L1A=0, IOI_CAM1, IOI_L2A, IOI_CAM2,
              IOI_SEL1, IOI_SEL2, IOI_SEL3, IOI_FEEDER, IOI_COUNT };
// FEEDER's legacy convention was already ON=LOW (run pulls the pin low); the
// default mask preserves that while everything else stays ON=HIGH.
volatile uint32_t IO_INV_MASK = (1u<<IOI_FEEDER);
#define IO_IS_INV(IDX) ((IO_INV_MASK>>(IDX))&1u)
// Register-level writes valid for any pin (out1 bank covers >=32), IRAM-safe.
#define GPIO_ANY_SET(PIN) do{ if((PIN)<32) {GPIO.out_w1ts=1u<<(PIN);} else {GPIO.out1_w1ts.val=1u<<((PIN)-32);} }while(0)
#define GPIO_ANY_CLR(PIN) do{ if((PIN)<32) {GPIO.out_w1tc=1u<<(PIN);} else {GPIO.out1_w1tc.val=1u<<((PIN)-32);} }while(0)
#define IO_ON(PIN,IDX)  do{ if(IO_IS_INV(IDX)) GPIO_ANY_CLR(PIN); else GPIO_ANY_SET(PIN); }while(0)
#define IO_OFF(PIN,IDX) do{ if(IO_IS_INV(IDX)) GPIO_ANY_SET(PIN); else GPIO_ANY_CLR(PIN); }while(0)
// Main-loop counterpart (digitalWrite is fine outside the ISR). idx<0 = pin not
// under polarity control (e.g. a custom trig_cam_pulse pin): logical==physical.
static inline void io_drive(int pin,int idx,bool on)
{
  bool inv = (idx>=0) && IO_IS_INV(idx);
  digitalWrite(pin, (on!=inv) ? HIGH : LOW);
}
// Logical feeder state, so a polarity change can re-drive the pin correctly.
bool FEEDER_ON=false;

// Manual backlight hold, for camera setup (exposure/gain/focus need a steady
// image, and the stage machine only ever flashes the light for ~600us).
//
// Auto-off is not optional politeness: a backlight sized for a 600us strobe is
// not necessarily rated for continuous duty, so a hold that outlives the person
// who set it is a thermal risk. LIGHT_HOLD_deadline_ms==0 means nothing is
// held; a hold is refused outside IDLE because there the stage tasks own these
// pins and would fight it.
volatile uint32_t LIGHT_HOLD_deadline_ms=0;
volatile int LIGHT_HOLD_pin=-1;
volatile int LIGHT_HOLD_idx=-1;
static const uint32_t LIGHT_HOLD_MAX_MS=300000;      // 5 min ceiling
static const uint32_t LIGHT_HOLD_DEFAULT_MS=60000;   // 1 min unless asked otherwise
// Name<->pin<->mask-bit table for the io_on_level JSON config.
static const struct { const char *name; int pin; int idx; } IO_POL_TAB[] = {
  {"L1A",   PIN_O_L1A,  IOI_L1A},
  {"CAM1",  PIN_O_CAM1, IOI_CAM1},
  {"L2A",   PIN_O_L2A,  IOI_L2A},
  {"CAM2",  PIN_O_CAM2, IOI_CAM2},
  {"SEL1",  PIN_O_SEL1, IOI_SEL1},
  {"SEL2",  PIN_O_SEL2, IOI_SEL2},
  {"SEL3",  PIN_O_SEL3, IOI_SEL3},
  {"FEEDER",FEEDER_PIN, IOI_FEEDER},
};

// Single place that drives every actuator to its inactive level. Used on the
// error path, on reset, and once the plate has coasted to a stop, so a selector
// can never be left energised by a state transition that forgot one pin.
#define ALL_OUTPUTS_SAFE() \
  { \
    IO_OFF(PIN_O_L1A,IOI_L1A); \
    IO_OFF(PIN_O_L2A,IOI_L2A); \
    IO_OFF(PIN_O_CAM1,IOI_CAM1); \
    IO_OFF(PIN_O_CAM2,IOI_CAM2); \
    IO_OFF(PIN_O_SEL1,IOI_SEL1); \
    IO_OFF(PIN_O_SEL2,IOI_SEL2); \
    IO_OFF(PIN_O_SEL3,IOI_SEL3); \
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



float PLATE_FREQ_SETPOINT=0;
bool SYS_FREQ_STABLE=false;

// Ignore the real gate sensor while leaving trig_phantom_pulse working.
//
// The two are deliberately separated: this is checked on the SENSOR path only,
// so injected pulses still register. blockNewDetectedObject lives inside
// newPulseEvent and blocks everything, which is right for IDLE/ERROR but wrong
// here -- the whole point is to inject while real parts are ignored.
//
// What it buys: a clean lane for calibration. Disable, wait for the plate to
// reach constant speed, fire a couple of known pulses to measure the host's
// clock offset against, then re-enable. The measurement is then taken on
// pulses whose timing we chose, with no real part able to land in the middle
// of it. Parts on the plate are not lost by this -- they simply ride round
// again, which is what they do for any other rejection.
volatile bool GATE_DISABLED=false;
float PLATE_FREQ_TARGET=1000;
float PLATE_FREQ_CURRENT=0;
// Plate spin-up/down ramp, in Hz of plate_freq per second of wall time. The old
// scheme stepped a fixed 5 Hz every 256th loop pass, so the real acceleration
// depended on loop speed; this is deterministic and per-machine configurable
// (set_setup "plate_accel", persisted). <=0 means jump instantly.
float SYS_FREQ_ACCEL=2000;
bool SYS_STEPPER_DISABLED=false;

// 15/s was the old machine's rate; production runs ~30/s and bursts higher, so
// a 66ms floor silently merged adjacent parts. 4ms still rejects the double
// counting a single part can cause, while clearing 250 parts/s.
// Dry run: the stage machine advances, the plate does not.
//
// The rig used to fake this with stepper_disable + plate_freq 15000, which is
// the worst available combination: dropping ENABLE takes away the motor's
// holding torque while StepGo keeps clocking the driver, so the plate is both
// free and being pulsed -- and parts get shaken off it.
//
// Muting the pulse instead leaves the driver energised, so the plate is held
// still by its own holding torque and the logical clock runs at whatever
// plate_freq asks for. Physical speed is zero by construction rather than by
// trusting the driver to honour an enable pin.
volatile bool DRY_RUN=false;

uint32_t SYS_MIN_PULSE_TIME_SEP_us=4000;

// ---------------------------------------------------------------------------
// Automatic trigger rate
// ---------------------------------------------------------------------------
// SKIP is the machine telling us it admitted more parts than it could judge.
// Backing the gate off when that happens, and easing it forward when it does
// not, keeps the machine at the fastest rate it can actually sustain instead of
// at whatever rate somebody typed once.
//
// AIMD, the congestion-control shape, because the problem has the same
// structure: the cost of being slightly too slow is small and linear, the cost
// of being too fast is parts passing unjudged. So retreat fast, return slowly.
//
// SYS_MIN_PULSE_TIME_SEP_us stays the CONFIGURED value -- the fastest the
// operator is willing to run and the thing save_setup persists. The loop only
// ever moves GATE_SEP_EFF_us, between that and a floor. Without this split a
// save_setup during a backoff would write a transient into NVS and the machine
// would come back permanently slow.
uint32_t GATE_SEP_EFF_us      = 4000;    // what newPulseEvent actually enforces
uint32_t AUTO_RATE_FLOOR_us   = 200000;  // slowest it may go (5/s)
volatile bool AUTO_RATE       = false;
uint32_t AUTO_RATE_OK_RUN     = 0;       // consecutive judged parts with no SKIP
uint32_t AUTO_RATE_RECOVER_N  = 50;      // clean parts before easing forward
uint32_t AUTO_RATE_BACKOFFS   = 0, AUTO_RATE_RECOVERS = 0;

// Called from the report handler when a part is swept into SKIP.
static inline void autoRateBackoff()
{
  if(!AUTO_RATE) return;
  AUTO_RATE_OK_RUN=0;
  uint32_t s = GATE_SEP_EFF_us + (GATE_SEP_EFF_us>>3);   // +12.5%
  if(s>AUTO_RATE_FLOOR_us) s=AUTO_RATE_FLOOR_us;
  if(s!=GATE_SEP_EFF_us){ GATE_SEP_EFF_us=s; AUTO_RATE_BACKOFFS++; }
}

// Called when a part is judged normally.
static inline void autoRateOk()
{
  if(!AUTO_RATE) return;
  if(++AUTO_RATE_OK_RUN < AUTO_RATE_RECOVER_N) return;
  AUTO_RATE_OK_RUN=0;
  uint32_t s = GATE_SEP_EFF_us - (GATE_SEP_EFF_us>>5);   // -3%
  if(s<SYS_MIN_PULSE_TIME_SEP_us) s=SYS_MIN_PULSE_TIME_SEP_us;
  if(s!=GATE_SEP_EFF_us){ GATE_SEP_EFF_us=s; AUTO_RATE_RECOVERS++; }
}

// Promote the camera-timestamp match from observer to decider. Default off: the
// first flash must behave exactly as before, and the agree/disagree counters in
// get_running_stat are what justify turning it on.
bool REPORT_MATCH_TS=false;
int SEL1_ACT_COUNTDOWN=-1;

// Plate geometry for the distance gate. These were the OLD machine's numbers
// (350mm plate, 28800 pulses/turn), which made every mm<->pulse conversion here
// off by a factor of ~2: 3.5mm resolved to 91 pulses instead of 278.
#define _PLAT_DIAMITER_mm 240
#define _PLAT_CIRC_um (_PLAT_DIAMITER_mm*3.14159*1000)
#define _PLAT_PULSE_PER_TURN 60000
#define _PLAT_DIST_um_PER_STEP ((int)(_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))

#define _PLAT_DIST_um(stepCount) ((int)(stepCount*_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))
#define _PLAT_DIST_step(dist_um) ((int)(dist_um*_PLAT_PULSE_PER_TURN/_PLAT_CIRC_um))

//disk D=350 circumference 350*Pi
//1600*9 steps per round
//0.076mm per step


typedef struct pipeLineInfo{
  uint32_t gate_pulse;
  int8_t stage;
  // The verdict handoff: written by report (main loop) and by newPulseEvent /
  // the SWITCH branch (ISR), read by the SWITCH branch (ISR) and the cleanup
  // sweep (main loop). Aligned int32 so each access is atomic; volatile so
  // neither side caches the other's write. The object is reached through a
  // pointer into RBuf, never copied by value, so a volatile member is free.
  volatile int32_t insp_status;
  uint32_t tid;
  // Registration wall time (lower 32 of esp_timer_get_time), for the
  // gate->report latency stat. Wraps every ~71 min; a single latency sample
  // never spans that, so unsigned subtraction stays correct.
  uint32_t trig_us;
  // Device clock at the instant THIS object's camera trigger fired. Full 64
  // bits, unlike trig_us: the offset arithmetic against the camera's own clock
  // would otherwise have to handle a 71-minute wrap, and 8 bytes x 100 objects
  // is 800 bytes out of 270KB free.
  //
  // Written from the CAM ISR branch, read by the report handler in the main
  // loop. A 64-bit store is not atomic on a 32-bit core, so this relies on the
  // pipeline order rather than on atomicity: an object cannot be reported
  // against until its camera stage has already fired, which is what wrote this.
  // The write and the read are separated by the whole inspection round trip.
  uint64_t cam_us;
  // Set on objects the device emitted purely to measure the clock offset, at a
  // moment when nothing else was outstanding. Only these teach CamClockSync --
  // see the sync-pulse note there.
  uint8_t sync;
}pipeLineInfo;

// ---------------------------------------------------------------------------
// Camera clock <-> device clock
// ---------------------------------------------------------------------------
// Which frame belongs to which object is a question this board can answer
// directly: it fired the trigger, so it knows the object and the instant. The
// host cannot, and had been reconstructing it from arrival order and a clock
// offset it estimated itself -- which needed bootstrap, drift tracking, a
// staleness sweep, a resync path and an idle heartbeat, all of it compensation
// for knowledge that lives here.
//
// So the host now reports what it actually knows -- "the frame taken at camera
// time T is verdict C" -- and the matching happens where the ground truth is.
//
// The two clocks share no epoch but their difference is near constant: only
// crystal drift moves it, ~83us per second measured on this pair, against a
// tolerance of several ms. Estimate it once, track it slowly, and every report
// becomes self-addressing.
//
// Integer throughout on purpose: this runs in the report handler beside ISR
// work, and an EWMA is a shift.
struct CamClockSync
{
  static const int64_t TOL_US   = 5000;   // match window; drift needs ~60s to cross it
  static const int     BOOT_N   = 8;      // samples before the estimate is trusted
  static const int     EWMA_SH  = 4;      // 1/16 per update
  // Consecutive out-of-window samples before the estimate is abandoned. Long
  // enough that a burst of mismatches cannot trigger it, short enough that a
  // genuinely moved offset is re-learned in a second or two of traffic.
  static const int     LOST_N   = 16;

  bool     valid = false;
  int64_t  offset_us = 0;                 // cam_ts - cam_us
  int64_t  boot[BOOT_N];
  uint8_t  boot_n = 0;
  int64_t  last_resid_us = 0;
  int64_t  max_resid_us = 0;
  uint32_t agree = 0, disagree = 0, learned = 0;
  // Samples refused as outliers, and how often the model was abandoned and
  // rebuilt. Both are diagnostics for the failure this guard exists to stop:
  // rejected climbing while resid stays small is the guard working; rebuilds
  // climbing means the offset really is moving.
  uint32_t rejected = 0, rebuilds = 0;
  uint16_t consec_reject = 0;

  void reset()
  {
    valid=false; offset_us=0; boot_n=0;
    last_resid_us=0; max_resid_us=0;
    agree=disagree=learned=rejected=rebuilds=0; consec_reject=0;
  }

  // A report whose object is already known (matched by tid) is a free
  // measurement of the offset. Real parts therefore calibrate the clock as a
  // side effect of ordinary running -- no injected pulses required.
  void observe(uint64_t cam_ts, uint64_t cam_us)
  {
    if(cam_ts==0 || cam_us==0) return;
    int64_t sample = (int64_t)cam_ts - (int64_t)cam_us;
    learned++;
    if(!valid)
    {
      if(boot_n < BOOT_N) boot[boot_n++] = sample;
      if(boot_n < BOOT_N) return;
      // Median, then require a real majority around it. Disagreement means the
      // samples are not measuring one constant -- better to stay unconverged and
      // say so than to publish an average of unrelated numbers.
      int64_t srt[BOOT_N];
      for(int i=0;i<BOOT_N;i++) srt[i]=boot[i];
      for(int i=1;i<BOOT_N;i++){int64_t k=srt[i];int j=i-1;while(j>=0&&srt[j]>k){srt[j+1]=srt[j];j--;}srt[j+1]=k;}
      int64_t med = srt[BOOT_N/2];
      int ok=0; for(int i=0;i<BOOT_N;i++) if(llabs(srt[i]-med)<=TOL_US) ok++;
      if(ok*2 > BOOT_N){ offset_us=med; valid=true; }
      else { for(int i=0;i<BOOT_N/2;i++) boot[i]=boot[i+BOOT_N/2]; boot_n=BOOT_N/2; }
      return;
    }
    int64_t resid = sample - offset_us;
    last_resid_us = resid;
    if(llabs(resid) > llabs(max_resid_us)) max_resid_us = resid;

    // Refuse samples from outside the match window before they touch the
    // estimate.
    //
    // Without this every sample was folded in unconditionally, which made the
    // estimator amplify its own mistakes: one pair that is wrong by 400ms moves
    // the offset by 400ms/16 = 25ms in a single step, against a 5000us match
    // window. The next frame then matches the wrong object, which produces
    // another bad pair, and so on. Measured under induced frame loss: -1381us
    // residual with a 381ms maximum in timestamp mode, and -274ms in positional
    // mode, where nearly every pair is wrong by construction.
    //
    // A sample further from the estimate than the window either belongs to a
    // different object or the estimate is stale -- and neither case is improved
    // by averaging it in. Crystals drift; they do not jump.
    if(llabs(resid) > TOL_US)
    {
      rejected++;
      if(++consec_reject >= LOST_N)
      {
        // Sustained rejection is the other case: the offset has genuinely moved
        // (a long idle, a device reboot) and no longer describes these clocks.
        // Rebuild it from scratch rather than creep toward it a sixteenth at a
        // time, which would spend the whole approach mismatching.
        reset_estimate();
        rebuilds++;
      }
      return;
    }
    consec_reject = 0;
    offset_us += resid >> EWMA_SH;   // crystals drift, they do not jump
  }

  // Drop the estimate but keep the counters -- this is a recovery, not a
  // measurement boundary.
  void reset_estimate() { valid=false; boot_n=0; consec_reject=0; }

  // Where a frame taken at cam_ts should sit on the device clock.
  int64_t expectedCamUs(uint64_t cam_ts) const { return (int64_t)cam_ts - offset_us; }
};
CamClockSync CAM_SYNC;

// Gate->report latency, updated by the report handler (main loop only),
// reported by get_running_stat, zeroed by reset_running_stat.
uint32_t REP_LAT_N=0;
uint64_t REP_LAT_SUM_US=0;
uint32_t REP_LAT_MAX_US=0;

// Incremented from the ISR (Run_ACTS' SWITCH branch), read and zeroed from the
// main loop (get_running_stat / reset_running_stat). Were uint64_t, which on a
// 32-bit core is two loads -> get_running_stat could read a half-updated value
// straddling a carry. 32-bit is a single aligned access, so the read is atomic;
// 4.29e9 parts is ~800 days at 60/s and they are resettable stats anyway.
// The reset store still races the ISR's ++ and may drop one count at the exact
// moment of reset -- harmless, and display-only regardless.
// Context of the most recent pipeline error, captured in the ISR at the moment
// the fault is detected so the state-change log can say WHICH object failed
// and how late it was, instead of a bare code.
// Deliberate-crash request (CRASH_TEST command); executed in firmwareLoop().
volatile int CRASH_REQ=0;
// Deliberate task-WDT starvation request (wdt_test command).
volatile int WDT_TEST_REQ=0;

// Fail-to-reject policy (docs/RELIABILITY_ROADMAP.md, layer 2). 0 = legacy:
// any unanswered part at SWITCH stops the line. 1 = force-NA: the part is
// left unactuated (recirculates via the NA path), counted, and only
// UNANSWERED_STOP_AFTER *consecutive* unanswered parts stop the line --
// quality uncertainty is rejected, tracking-integrity faults still stop.
volatile int UNANSWERED_POLICY=0;
volatile int UNANSWERED_STOP_AFTER=5;
volatile uint32_t UNANSWERED_Count=0;
volatile uint32_t CONSEC_UNANSWERED=0;

// Monotonic sequence stamped on every async event (cam_trig / system_info)
// so the host can DETECT event loss on the unacked path -- events, unlike
// replies, have no id to miss.
uint32_t EVENT_SEQ=0;

// Host-link watchdog: if >0 and the machine is in READY, going this long
// without any valid inbound frame is treated as host death -> fail-safe stop
// (a hung vision process must stop the line without host cooperation).
volatile int host_timeout_ms=0;

// Health high-water marks (reset with reset_running_stat).
volatile uint32_t ISR_GAP_MAX_CY=0;   // max inter-tick gap, CPU cycles
volatile uint32_t RBUF_PEAK=0;        // max pipeline depth seen

// Why a detection was turned away at the gate. Incremented from the timer ISR
// (single writer) and read by the main loop for get_running_stat.
//
// Until now newPulseEvent's return code was discarded at both call sites, so a
// rejected object was indistinguishable from an object that was never there.
// That matters most for the rate limit: it is the knob that protects the camera
// from being asked for frames faster than it can produce them, and without a
// count there is no way to tell whether it is doing nothing or throwing away
// half the parts.
volatile uint32_t GATE_REJ_RATE=0;    // faster than min_detect_sep_us
volatile uint32_t GATE_REJ_DIST=0;    // closer than the 2mm plate-distance gate
volatile uint32_t GATE_REJ_BUSY=0;    // no room in RBuf / the ACT schedules
volatile uint32_t GATE_ACCEPT=0;      // registered, for a rejection ratio

volatile uint32_t ERR_CTX_TID=0;
volatile int32_t  ERR_CTX_STATUS=0;
volatile uint32_t ERR_CTX_GATE_PULSE=0;
volatile uint32_t ERR_CTX_CUR_PULSE=0;

volatile uint32_t SEL1_Count=0;
volatile uint32_t SEL2_Count=0;
volatile uint32_t SEL3_Count=0;
volatile uint32_t NA_Count=0;
volatile uint32_t SKIP_Count=0;

// Stepper polarity (see config/MachineConfig.hpp). Compiled fallback keeps the
// original direct-wired behaviour; a common-anode 5V driver input flips these
// via set_setup {"stepper_en_active":..,"stepper_dir":..} + save_setup.
int stepper_en_active = STEPPER_EN_ACTIVATION;
int stepper_dir_level = 0;

// Passive machine metadata (see config/MachineConfig.hpp): reported and
// persisted so hosts can convert physical units, never used in firmware math.
// These are the REAL machine's numbers, not a placeholder: a config-version
// bump discards the stored blob and falls back here, which happens on every
// firmware change, so wrong defaults silently re-arrive after each flash.
uint32_t pulses_per_rev = 60000;      // measured on the machine
float plate_diameter_mm = 240.0f;     // glass plate diameter

// stagePulseOffset now lives in config/MachineConfig.hpp so the NVS layer can
// persist it. The values below remain the fallback for a board with no stored
// config; MachineConfig::begin() overwrites them when one exists.
// Measured/confirmed on this machine (see docs and tools/cam_grab.py pulsetest):
//   - 18 ticks = 600us of light at production plate_freq 15000, twice the
//     exposure-covering width the camera needs; its trigger floor is ~100us, so
//     the old 2-tick CAM window (66us) could not have fired a camera at all.
//   - the blow station sits 30000 pulses after the gate; SWITCH (the verdict
//     deadline) must fall before it, leaving the whole transit as answer budget.
//   - 1500 ticks = 50ms of air at production speed; the old 1-tick default was
//     33us, far too short to eject anything.
stagePulseOffset STAGE_PULSE_OFFSET={
  .CAM1_on =654,
  .CAM1_off=672,
  .L1A_on =654,
  .L1A_off=672,


  .CAM2_on =654,
  .CAM2_off=672,
  .L2A_on =654,
  .L2A_off=672,


  .SWITCH =29900,


  .SEL1_on=30000,
  .SEL1_off=31500,
  .SEL2_on=30010,
  .SEL2_off=31510,
  .SEL3_on=30020,
  .SEL3_off=31520



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
  system_info=1006,
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
  uint32_t gate_pulse;
  // float curFreq;

  //log
  string log;

  //respFrame
  bool isAck;
  int resp_id;
};

// Producers: the main loop only (state transitions, recv_ERROR, trig_cam_pulse).
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
// allocate. The main loop turns it into the cam_trig JSON where std::string
// is free to be used.
struct ISRTrigInfo
{
  uint64_t trig_time_us;
  uint32_t trig_id;
  uint32_t gate_pulse;
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
        PLATE_FREQ_TARGET=0;

        pinMode(FEEDER_PIN, OUTPUT);
        io_drive(FEEDER_PIN, IOI_FEEDER, false);
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
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;
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
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;
      } //loop
      else
      {
      } //exit
      break;
    break;

    case SYS_STATE::INSPECTION_MODE_READY:
    {
      if (i == 0)//enter
      {
        blockNewDetectedObject=false;
        FEEDER_ON=true;
        io_drive(FEEDER_PIN, IOI_FEEDER, true);

        //
      }
      else if (i == 1)
      {
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;
      } //loop
      else
      {
        blockNewDetectedObject=true;
        FEEDER_ON=false;
        io_drive(FEEDER_PIN, IOI_FEEDER, false);
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_ERROR:
    {
      if (i == 0)
      {
        PLATE_FREQ_TARGET=0;
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
        commInfo->type=TaskQ2CommInfo_Type::system_info;
        char numberStr[200];
        if(state==SYS_STATE::INSPECTION_MODE_ERROR
           && extraCode==(int)GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT)
        {
          // Say WHICH object failed and how far past its deadline it was --
          // "err=2 tid=7 ... late=123" reads directly as "tid 7 was never
          // answered and the SWITCH point passed 123 pulses ago".
          sprintf(numberStr,
                  "State changed from  %d to %d err=%d tid=%u status=%ld"
                  " gate_pulse=%lu cur_pulse=%lu late_pulses=%ld",
                  sysinfo.state,state,extraCode,
                  (unsigned)ERR_CTX_TID,(long)ERR_CTX_STATUS,
                  (unsigned long)ERR_CTX_GATE_PULSE,
                  (unsigned long)ERR_CTX_CUR_PULSE,
                  (long)(ERR_CTX_CUR_PULSE-ERR_CTX_GATE_PULSE
                         -STAGE_PULSE_OFFSET.SWITCH));
        }
        else if(state==SYS_STATE::INSPECTION_MODE_ERROR)
        {
          sprintf(numberStr, "State changed from  %d to %d err=%d",
                  sysinfo.state,state,extraCode);
        }
        else
        {
          sprintf(numberStr, "State changed from  %d to %d",sysinfo.state,state);
        }
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
// Consumed by the next newPulseEvent: marks that object as a clock-sync pulse.
// Only ever set by syncPulseService, which fires with the pipeline empty.
static uint8_t SYNC_MARK_NEXT = 0;

int newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  static uint32_t tid_counter=1;
  uint32_t _prePulse_BK=_prePulse;
  _prePulse=middle_pulse;
  // 2mm, not 3.5mm: parts are specified 3mm apart, and with the plate geometry
  // finally correct a 3.5mm gate would reject conforming production parts.
  if(middle_pulse-_prePulse_BK<(_PLAT_DIST_step(2000))){GATE_REJ_DIST++;return -9;}
  uint64_t curTime = esp_timer_get_time();
  // The fire-rate limit. Rejecting here is the cheapest possible outcome: the
  // object never gets a tid, never gets a camera trigger and never gets a
  // SWITCH task, so it simply recirculates for another pass. Letting it through
  // instead would ask the camera for a frame it cannot deliver, and a trigger
  // with no frame poisons the host's pairing (see CORE0_1_CAVEATS J7/J9).
  if(curTime-_preTime<GATE_SEP_EFF_us){GATE_REJ_RATE++;return -8;}
  _preTime=curTime;


  if(blockNewDetectedObject)return -1;
  pipeLineInfo *head = RBuf.getHead();
  if (head == NULL)
  {
    GATE_REJ_BUSY++;
    return -1;
  }

  //get a new object and find a space to log it
  // TCount++;
  // head->s_pulse = start_pulse;
  // head->e_pulse = end_pulse;
  // head->pulse_width = pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status = insp_status_UNSET;
  head->tid=tid_counter;
  head->trig_us=(uint32_t)esp_timer_get_time();
  // Clear the previous occupant's camera time. RBuf is a ring, so without this
  // an object whose CAM stage has not fired yet still carries a plausible-
  // looking cam_us from whoever held the slot last -- and the timestamp matcher
  // only skips zeroes, so it would happily match a frame against it.
  head->cam_us = 0;
  head->sync = SYNC_MARK_NEXT;
  SYNC_MARK_NEXT = 0;
  if (ActRegister_pipeLineInfo(head) != 0)
  { //register failed....
    GATE_REJ_BUSY++;
    return -2;
  }
  RBuf.pushHead();
  GATE_ACCEPT++;
  {
    uint32_t sz=RBuf.size();
    if(sz>RBUF_PEAK) RBUF_PEAK=sz;
  }
  tid_counter++;
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
                    IO_ON(PIN_O_L1A,IOI_L1A);
                    IO_TRACE_LOG(PIN_O_L1A,1,cur_pulse,task->src->tid);
                   }
                   else
                   {
                    IO_OFF(PIN_O_L1A,IOI_L1A);
                    IO_TRACE_LOG(PIN_O_L1A,0,cur_pulse,task->src->tid);
                   }


                   );






  ACT_TRY_RUN_TASK(acts->ACT_CAM1, cur_pulse,

                  if(task->info)
                  {

                    IO_ON(PIN_O_CAM1,IOI_CAM1);
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
                      commInfo->gate_pulse=task->src->gate_pulse;
                      ISRTrigQ.pushHead();
                      // Keep it on the object too. Until now this timestamp was
                      // announced and then forgotten, which is why the host had
                      // to reconstruct the frame<->object mapping from clocks it
                      // could only observe indirectly.
                      task->src->cam_us = time_us;
                    }
                    else
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    IO_OFF(PIN_O_CAM1,IOI_CAM1);
                    IO_TRACE_LOG(PIN_O_CAM1,0,cur_pulse,task->src->tid);
                  }


                   );


  ACT_TRY_RUN_TASK(acts->ACT_L2A, cur_pulse,
                   if(task->info)
                   {
                    IO_ON(PIN_O_L2A,IOI_L2A);
                    IO_TRACE_LOG(PIN_O_L2A,1,cur_pulse,task->src->tid);
                   }
                   else
                   {
                    IO_OFF(PIN_O_L2A,IOI_L2A);
                    IO_TRACE_LOG(PIN_O_L2A,0,cur_pulse,task->src->tid);
                   }
                    );


  ACT_TRY_RUN_TASK(acts->ACT_CAM2, cur_pulse,

                  if(task->info)
                  {

                    IO_ON(PIN_O_CAM2,IOI_CAM2);
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
                      commInfo->gate_pulse=task->src->gate_pulse;
                      ISRTrigQ.pushHead();
                    }
                    else
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    IO_OFF(PIN_O_CAM2,IOI_CAM2);
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
          CONSEC_UNANSWERED=0;
          autoRateOk();   // a part was judged
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, spo->SEL1_on, 1, _task_->src =NULL;);//the src will be cleaned up right after
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, spo->SEL1_off, 0, _task_->src =NULL; );
          break;
        case 2:
          CONSEC_UNANSWERED=0;
          autoRateOk();   // a part was judged
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, spo->SEL2_on, 1, _task_->src =NULL; );
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, spo->SEL2_off, 0, _task_->src =NULL; );
          break;
        case 3:
          CONSEC_UNANSWERED=0;
          autoRateOk();   // a part was judged
          SEL3_Count++;
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_on, 1, _task_->src =NULL; );
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_off, 0, _task_->src =NULL; );
          break;
        case 0xFFFF:
          CONSEC_UNANSWERED=0;
          autoRateOk();   // a part was judged
          NA_Count++;
          // inspResCount.NA++;
          break;

        // --- unjudged: the part goes round again, and the run is on notice ---
        //
        // SKIP and UNSET differ only in WHY nobody judged this part -- swept by
        // a later report, or never spoken for at all -- so they are counted
        // apart but escalate together. Neither is an answer, so neither resets
        // the consecutive counter. That reset was the bug: SKIP is the common
        // case once reports can arrive out of order, so zeroing the counter
        // there quietly disabled the only guard against a machine that has
        // stopped judging anything.
        //
        // One unjudged part is normal loss -- it recirculates and gets another
        // pass, costing a lap. Several in a row is not loss, it is a system
        // that has stopped working, and that is what UNANSWERED_STOP_AFTER is
        // for. The threshold is the whole safety argument for not faulting on
        // the first one.
        case insp_status_SKIP:
          SKIP_Count++;
          // The machine just told us it admitted a part it could not judge.
          autoRateBackoff();
          CONSEC_UNANSWERED++;
          if(UNANSWERED_POLICY==1 && CONSEC_UNANSWERED < (uint32_t)UNANSWERED_STOP_AFTER)
            break;   // fail-to-reject: no actuation -> part recirculates
          if(UNANSWERED_POLICY!=1) break;   // policy 0: SKIP alone never faults
          ecode=GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT;
          ERR_CTX_TID=pli->tid;
          ERR_CTX_STATUS=pli->insp_status;
          ERR_CTX_GATE_PULSE=pli->gate_pulse;
          ERR_CTX_CUR_PULSE=cur_pulse;
          break;
        case insp_status_DEL: //ERROR
          break;

        case insp_status_UNSET:
        default:
          UNANSWERED_Count++;
          CONSEC_UNANSWERED++;
          if(UNANSWERED_POLICY==1 && CONSEC_UNANSWERED < (uint32_t)UNANSWERED_STOP_AFTER)
            break;   // fail-to-reject: no actuation -> part recirculates
          ecode=GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT;
          ERR_CTX_TID=pli->tid;
          ERR_CTX_STATUS=pli->insp_status;
          ERR_CTX_GATE_PULSE=pli->gate_pulse;
          ERR_CTX_CUR_PULSE=cur_pulse;
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

                    if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false && DRY_RUN==false && SEL1_ACT_COUNTDOWN)
                    {
                      if(SEL1_ACT_COUNTDOWN>0)SEL1_ACT_COUNTDOWN--;
                      SEL1_Count++;
                      IO_ON(PIN_O_SEL1,IOI_SEL1);
                      IO_TRACE_LOG(PIN_O_SEL1,1,cur_pulse,0);
                    }
                   }
                   else
                   {
                    IO_OFF(PIN_O_SEL1,IOI_SEL1);
                    IO_TRACE_LOG(PIN_O_SEL1,0,cur_pulse,0);
                   }
                  );


  ACT_TRY_RUN_TASK(acts->ACT_SEL2, cur_pulse,
                  
                  if(task->info)
                  {

                  if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false && DRY_RUN==false)
                  {
                    SEL2_Count++;
                    IO_ON(PIN_O_SEL2,IOI_SEL2);
                    IO_TRACE_LOG(PIN_O_SEL2,1,cur_pulse,0);
                  }
                  }
                  else
                  {
                    IO_OFF(PIN_O_SEL2,IOI_SEL2);
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
void setMachineSetup(JsonDocument &jdoc, bool apply_hw);
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





// Advanced by the ISR (onTimer), read by the main loop (get_setup report,
// bench's at-speed wait). Aligned 32-bit so each access is atomic; volatile so
// the main loop re-reads it rather than caching a stale copy, and the ISR is
// not free to hoist it. Single writer (the ISR), so its ++ needs no lock.
volatile uint32_t SYS_STEP_COUNT=0;


typedef struct GateInfo {
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint16_t debounce;
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
  ngateInfo.debounce=DEBOUNCE_H_THRES;
  gateInfo=ngateInfo;
}


bool _senseInv_=true;

// Written by set_setup (main loop), read by GateSensing (ISR). Aligned int so
// the access is atomic; volatile so the ISR does not cache a stale threshold.
volatile int  minWidth = 0;
volatile int  maxWidth = 1000;//1+40000/_PLAT_DIST_um_PER_STEP;

// Gate debounce, in gate-sample ticks (the timer runs at 2*plate_freq, so one
// tick is ~1 step ~= _PLAT_DIST_um_PER_STEP of travel -- 12.6um on the 240mm
// plate). A value of N means an edge is accepted only after the new level has
// held for N consecutive samples, so any glitch shorter than N ticks is
// rejected. Runtime-settable (gate_debounce_rise / gate_debounce_fall) like
// minWidth/maxWidth, and persisted with the config blob since v6.
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
      gateInfo.debounce--;
      if(gateInfo.debounce==0)
      {
        onSenseEdge=true;
        gateInfo.debounce=DEBOUNCE_H_THRES;
      }
    }
    else
    {//still HIGH: absorb the dip, keep the trailing edge current
      gateInfo.debounce=DEBOUNCE_L_THRES;
      gateInfo.end_pulse=SYS_STEP_COUNT;
    }
  }
  else
  {//currently LOW -- no object
    if(new_Sense)
    {//reading HIGH: count toward a rising edge
      gateInfo.debounce--;
      if(gateInfo.debounce==0)
      {
        onSenseEdge=true;
        gateInfo.debounce=DEBOUNCE_L_THRES;
      }
    }
    else
    {//still LOW: reject the blip, keep the leading edge current
      gateInfo.debounce=DEBOUNCE_H_THRES;
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
        if(SYS_STEPPER_DISABLED==false && SYS_FREQ_STABLE && GATE_DISABLED==false && DRY_RUN==false)
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




void StepGo()
{
  // Held at whatever level it had; a static pin is not a step.
  if(DRY_RUN) return;

  if((SYS_STEP_COUNT&1)==0)
  {
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
  



  {
    // Inter-tick gap high-water: field evidence of ISR jitter/stalls without
    // a scope. A gap over 1s is a timer stop/start seam, not jitter.
    static uint32_t last_cc=0;
    uint32_t cc=XTHAL_GET_CCOUNT();
    if(last_cc){
      uint32_t d=cc-last_cc;
      if(d<240000000u && d>ISR_GAP_MAX_CY) ISR_GAP_MAX_CY=d;
    }
    last_cc=cc;
  }

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
StaticJsonDocument<3072> recv_doc;
StaticJsonDocument<3072> ret_doc;


StaticJsonDocument <3072>doc;
StaticJsonDocument <3072>retdoc;



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
      commInfo->type=TaskQ2CommInfo_Type::system_info;
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

// An NVS commit erases/writes flash, which disables the instruction cache. The
// timer ISR (onTimer, IRAM) calls into non-IRAM code (Run_ACTS et al.), so a
// save with the timer live risks a stall/reset. Permit it only with the plate
// fully stopped -- PLATE_FREQ_CURRENT==0 is exactly when the timer alarm has been
// disabled (PLATE_FREQ_CURRENT==0 -> timerAlarmDisable) -- and held stopped
// (PLATE_FREQ_SETPOINT==0, so it will not spin back up), in a settle-able state
// (IDLE or a stopped READY, not ERROR/TEST/FATAL). Returns NULL if a save is
// allowed, else a human-readable reason so the caller knows what to fix.
// Phantom object train, emitted from the main loop.
//
// The host can already inject objects one `trig_phantom_pulse` at a time, but
// each one is a serial round trip, so the interval carries that jitter: a
// 33.3ms target measured 25.9-39.8ms, stdev 1.76ms, which is visible as an
// unsteady strobe and means the rig cannot present a known instantaneous load.
//
// trig_cam_burst is exact, but it blocks the machine loop and drives the pins
// directly -- so it produces frames with no pipeline object behind them, and
// while it runs no report can be serviced. Neither half is usable here.
//
// This schedules against the absolute clock (never `now + period`, which
// accumulates every iteration's cost and drifts the train slow) and emits at
// most one object per loop pass, so reports keep flowing the whole time.
static int32_t  PH_TRAIN_LEFT     = 0;
static int64_t  PH_TRAIN_NEXT_US  = 0;
static int32_t  PH_TRAIN_PERIOD_US= 0;
static uint32_t PH_TRAIN_EMITTED  = 0;
static int64_t  PH_TRAIN_PREV_US  = 0;
static int32_t  PH_TRAIN_MIN_US   = 0, PH_TRAIN_MAX_US = 0;

// Where trig_phantom_pulse puts an object: one L1A offset back, plus enough
// plate distance that it has somewhere to travel from.
static inline void phantomEmitOne()
{
  uint32_t tatPulse = SYS_STEP_COUNT - STAGE_PULSE_OFFSET.L1A_on + _PLAT_DIST_step(3000);
  newPulseEvent(tatPulse-10, tatPulse+10, tatPulse, 20);
}

// Clock-sync pulses: break the circularity in the offset estimate.
//
// Learning the offset from ordinary reports is circular. A (cam_ts, cam_us)
// pair is only a valid sample if the pairing that produced it was correct, and
// the pairing needs the offset. One bad pair moves the estimate, a moved
// estimate produces more bad pairs, and the loop closes on itself -- which is
// exactly what was measured before outlier rejection went in (-274ms in
// positional mode). Rejection contains that, but it cannot break it: deciding
// what is an outlier assumes the current estimate is roughly right, so a
// genuinely wrong estimate has to time out and rebuild, sampling blind
// throughout.
//
// A pulse fired when nothing else is outstanding has no such problem. There is
// exactly one candidate object, so the frame that comes back belongs to it by
// construction, with no clock knowledge required. Those samples are
// unambiguous, so they -- and only they -- teach the estimate. Ordinary reports
// consume the offset and never modify it, which means a lost frame or a
// mispaired part can no longer corrupt the clock at all.
//
// This is the piece the migration was missing: moving the estimator onto the
// device was done, but its samples still came from mixed production traffic.
static int64_t  SYNC_LAST_MS = 0;
static uint32_t SYNC_EMITTED = 0;

static void syncPulseService()
{
  if(sysinfo.state != SYS_STATE::INSPECTION_MODE_READY) return;

  // Cold, the estimate does not exist and the first real part would be paired
  // on a guess -- so beat fast until it does. Warm, the only job is to outpace
  // drift (~24-83us/s against a 5000us window), and 10s is ample.
  const int64_t now_ms = (int64_t)(esp_timer_get_time()/1000);
  const int64_t due_ms = CAM_SYNC.valid ? 10000 : 300;
  if(SYNC_LAST_MS!=0 && (now_ms-SYNC_LAST_MS) < due_ms) return;

  // The whole point: nothing else may be outstanding, or the returning frame
  // would have more than one candidate and the sample would need the very
  // estimate it is meant to produce.
  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    if(p->insp_status==insp_status_UNSET) return;
  }

  SYNC_MARK_NEXT = 1;
  uint32_t tatPulse = SYS_STEP_COUNT - STAGE_PULSE_OFFSET.L1A_on + _PLAT_DIST_step(3000);
  if(newPulseEvent(tatPulse-10, tatPulse+10, tatPulse, 20) != 0)
    SYNC_MARK_NEXT = 0;            // refused (gate, busy) -- try again later
  else
    SYNC_EMITTED++;
  SYNC_LAST_MS = now_ms;
}

static void phantomTrainService()
{
  if(PH_TRAIN_LEFT<=0) return;
  const int64_t now=esp_timer_get_time();
  if(now < PH_TRAIN_NEXT_US) return;

  phantomEmitOne();
  PH_TRAIN_EMITTED++;
  if(PH_TRAIN_PREV_US!=0)
  {
    const int32_t d=(int32_t)(now-PH_TRAIN_PREV_US);
    if(PH_TRAIN_MIN_US==0 || d<PH_TRAIN_MIN_US) PH_TRAIN_MIN_US=d;
    if(d>PH_TRAIN_MAX_US) PH_TRAIN_MAX_US=d;
  }
  PH_TRAIN_PREV_US=now;

  PH_TRAIN_NEXT_US += PH_TRAIN_PERIOD_US;
  // If the loop was held up longer than a whole period, do not try to catch up
  // by firing back-to-back -- that would hand the pipeline a burst it never
  // asked for. Give up the missed slots and stay on the original phase.
  if(PH_TRAIN_NEXT_US < now) PH_TRAIN_NEXT_US = now + PH_TRAIN_PERIOD_US;
  PH_TRAIN_LEFT--;
}

static const char* cfgPersistDeny()
{
  if(sysinfo.state!=SYS_STATE::IDLE &&
     sysinfo.state!=SYS_STATE::INSPECTION_MODE_READY)
    return "must be in IDLE or INSPECTION_MODE_READY";
  if(PLATE_FREQ_SETPOINT!=0)
    return "set plate_freq to 0 first";
  if(PLATE_FREQ_CURRENT!=0)
    return "plate still moving; wait until SYS_STEP_COUNT stops";
  return NULL;
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
  else if(strcmp(type,"get_version")==0)
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


  else if(strcmp(type,"trig_cam_pulse")==0)
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
        commInfo->gate_pulse=0;   // no pipeline object behind a manual pulse
        TaskQ2CommInfoQ.pushHead();
      }
    }


    // Custom cpin/lpin fall outside polarity control (idx -1: logical==physical).
    int cam_idx = (cam_PIN==PIN_O_CAM1)?IOI_CAM1 : (cam_PIN==PIN_O_CAM2)?IOI_CAM2 : -1;
    int light_idx = (light_PIN==PIN_O_L1A)?IOI_L1A : (light_PIN==PIN_O_L2A)?IOI_L2A : -1;
    io_drive(cam_PIN,cam_idx,true);
    delayMicroseconds(Light_Delay);
    io_drive(light_PIN,light_idx,true);
    delayMicroseconds(Light_Duration);
    io_drive(light_PIN,light_idx,false);
    io_drive(cam_PIN,cam_idx,false);





    doRsp=rspAck=true;
  }
  // Emit a whole pulse train from here instead of one command per pulse.
  //
  // The panel used to loop in python, one trig_cam_pulse per pulse, each one
  // contending for the same lock the status poller holds across a blocking
  // round trip. Measured: ask for 60 Hz, get 21 Hz, with intervals ranging
  // 22-140 ms. That is six times the jitter of the thing we are trying to
  // measure, so no camera rate ceiling above ~21 Hz could ever be established.
  //
  // Here a plain busy-wait on esp_timer_get_time() gets the jitter down to
  // interrupt latency. Two things this loop MUST do, because it runs inside
  // firmwareLoop() and hogs it for the whole train:
  //   1. feed the task WDT itself -- firmwareLoop's own esp_task_wdt_reset()
  //      at the top is out of reach, and esp_task_wdt_init(5,true) panics.
  //   2. NOT push a TaskQ2CommInfo per pulse -- that queue is 20 deep and its
  //      only consumer is the part of firmwareLoop we are blocking, so a train
  //      of any useful length would overflow it. The train's timing is reported
  //      back measured instead, which is better evidence than an echo anyway.
  else if(strcmp(type,"trig_cam_burst")==0)
  {
    retdoc["type"]="trig_cam_burst";
    doRsp=true;

    // Blocking the machine loop for seconds is only safe with the plate
    // stopped -- otherwise parts keep arriving at the selector with nobody
    // answering for them. Same condition the NVS save uses.
    const char* deny=cfgPersistDeny();
    if(deny!=NULL)
    {
      retdoc["burst_err"]=deny;
      retdoc["state"]=(int)sysinfo.state;
      rspAck=false;
    }
    else
    {
      int cam_PIN   = doc["cpin"].is<int>()          ? (int)doc["cpin"]           : PIN_O_CAM1;
      int light_PIN = doc["lpin"].is<int>()          ? (int)doc["lpin"]           : PIN_O_L1A;
      int Light_Delay    = doc["light_delay"].is<int>()    ? (int)doc["light_delay"]    : 100;
      int Light_Duration = doc["light_duration"].is<int>() ? (int)doc["light_duration"] : 100;
      int count     = doc["count"].is<int>()         ? (int)doc["count"]          : 10;
      int period_us = doc["period_us"].is<int>()     ? (int)doc["period_us"]      : 0;
      if(period_us<=0 && doc["hz"].is<float>() && (float)doc["hz"]>0.0f)
        period_us=(int)(1000000.0f/(float)doc["hz"]);
      if(period_us<=0) period_us=200000;   // 5 Hz

      // Bound the stall. 5000 pulses and 30 s are both far past any real
      // experiment, and either one running away would look like a hang.
      if(count<1) count=1;
      if(count>5000) count=5000;
      const int64_t max_total_us=30LL*1000000LL;
      if((int64_t)count*(int64_t)period_us > max_total_us)
        count=(int)(max_total_us/(int64_t)period_us);

      // A pulse cannot be shorter than the light it gates.
      const int pulse_us=Light_Delay+Light_Duration;
      if(period_us < pulse_us+50) period_us = pulse_us+50;

      const int cam_idx   = (cam_PIN==PIN_O_CAM1)?IOI_CAM1 : (cam_PIN==PIN_O_CAM2)?IOI_CAM2 : -1;
      const int light_idx = (light_PIN==PIN_O_L1A)?IOI_L1A : (light_PIN==PIN_O_L2A)?IOI_L2A : -1;

      // Optional: splice a tight cluster of extra pulses into the middle of an
      // otherwise regular train. The regular train keeps its ORIGINAL absolute
      // schedule -- the extras go in between two of its pulses rather than
      // pushing the rest of it late -- so the emitted pattern is exactly "N Hz
      // with a couple of intruders", which is what an unexpected trigger on the
      // line looks like to the pipeline.
      int ins_at = doc["insert_after"].is<int>()      ? (int)doc["insert_after"]      : -1;
      int ins_n  = doc["insert_count"].is<int>()      ? (int)doc["insert_count"]      : 0;
      int ins_p  = doc["insert_period_us"].is<int>()  ? (int)doc["insert_period_us"]  : period_us/4;
      if(ins_n<0) ins_n=0;
      if(ins_p<pulse_us+50) ins_p=pulse_us+50;
      // The cluster has to fit in one gap of the regular train, or it would
      // collide with the next scheduled pulse.
      if(ins_n>0 && (int64_t)(ins_n+1)*(int64_t)ins_p >= (int64_t)period_us)
        ins_n=(int)((int64_t)period_us/(int64_t)ins_p)-1;
      if(ins_n<0) ins_n=0;
      if(ins_at<1 || ins_at>=count) { ins_at=-1; ins_n=0; }

      // Per-pulse emission times, so the host can line them up against frame
      // timestamps. Only for short trains -- the reply serialises into 2048 B.
      const int total=count+ins_n;
      const bool want_offsets = (total<=120);
      JsonArray offs = want_offsets ? retdoc.createNestedArray("offsets_us") : JsonArray();

      int64_t main_next=esp_timer_get_time();
      const int64_t t_first=main_next;
      int64_t prev_rise=0, sum_us=0, min_us=INT64_MAX, max_us=0;
      int emitted=0;

      // One pulse, scheduled against the absolute clock. Never `now + period`:
      // that accumulates the cost of every pulse and drifts the train slow.
      auto emit_at=[&](int64_t when)
      {
        while(esp_timer_get_time() < when)
          esp_task_wdt_reset();

        const int64_t rise=esp_timer_get_time();
        io_drive(cam_PIN,cam_idx,true);
        delayMicroseconds(Light_Delay);
        io_drive(light_PIN,light_idx,true);
        delayMicroseconds(Light_Duration);
        io_drive(light_PIN,light_idx,false);
        io_drive(cam_PIN,cam_idx,false);
        emitted++;

        if(prev_rise!=0)
        {
          const int64_t d=rise-prev_rise;
          sum_us+=d;
          if(d<min_us) min_us=d;
          if(d>max_us) max_us=d;
        }
        prev_rise=rise;
        if(want_offsets) offs.add((int32_t)(rise-t_first));
        esp_task_wdt_reset();
      };

      for(int i=0;i<count;i++)
      {
        emit_at(main_next);
        if(ins_n>0 && (i+1)==ins_at)
        {
          int64_t ins_t=main_next+ins_p;
          for(int j=0;j<ins_n;j++) { emit_at(ins_t); ins_t+=ins_p; }
        }
        main_next+=period_us;
      }

      const int gaps=emitted-1;
      retdoc["emitted"]=emitted;
      retdoc["period_us"]=period_us;
      retdoc["span_us"]=(int32_t)(prev_rise-t_first);
      retdoc["mean_us"]=gaps>0?(int32_t)(sum_us/gaps):0;
      retdoc["min_us"] =gaps>0?(int32_t)min_us:0;
      retdoc["max_us"] =gaps>0?(int32_t)max_us:0;
      // With a cluster spliced in, min/max span the whole pattern by design --
      // jitter_us is only a jitter figure for a uniform train.
      retdoc["jitter_us"]=gaps>0?(int32_t)(max_us-min_us):0;
      if(ins_n>0) { retdoc["insert_at"]=ins_at; retdoc["insert_n"]=ins_n;
                    retdoc["insert_period_us"]=ins_p; }
      rspAck=true;
    }
  }
  else if(strcmp(type,"pin_mode")==0)
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


  // Both casings on purpose. The uInspMEGA-era WebUI peripheral base class
  // sends {"type":"PING"} (script.jsx triggerPing), while this firmware and the
  // bring-up panel use lowercase everywhere else. strcmp is case-sensitive, so
  // the uppercase form went unanswered -- and the WebUI treats 3 missed pings
  // (3s apart) as a dead link, tearing the peripheral channel down and
  // reopening the serial port every 9s, forever. Accepting both is the
  // additive fix; it costs one comparison and leaves the old WebUI untouched.
  else if(strcmp(type,"ping")==0 || strcmp(type,"PING")==0)
  {
    retdoc["type"]="pong";
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"get_setup")==0)
  {
    retdoc["ver"]="0.0.0 Alpha";
    retdoc["name"]="uInspESP32";
    
    genMachineSetup(retdoc);
    // Canonical hash of the full active config: host compares at connect and
    // refuses to run on mismatch (config-drift guard, RELIABILITY_ROADMAP L3).
    // Added here rather than inside genMachineSetup, which hash() itself calls.
    retdoc["cfg_crc"]=MachineConfig::hash();

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"set_setup")==0)
  {
    retdoc["type"]="set_setup";

    setMachineSetup(doc, true);

    // Opt-in commit. Without "persist":true this behaves exactly as before --
    // RAM only, gone at power-off -- so probing/jogging during setup doesn't
    // burn flash write cycles. Refused (NAK) unless the plate is stopped (see
    // cfgPersistDeny): a flash write with the timer ISR live is unsafe. The RAM
    // update above still applied; only the save is withheld.
    bool persistAck=true;
    if(doc["persist"].is<bool>() && doc["persist"].as<bool>())
    {
      const char* deny=cfgPersistDeny();
      if(deny==NULL)
        retdoc["persisted"]=MachineConfig::save();
      else
      {
        retdoc["persisted"]=false;
        retdoc["persist_err"]=deny;
        retdoc["state"]=(int)sysinfo.state;
        persistAck=false;
      }
    }

    doRsp=true;
    rspAck=persistAck;

  }
  else if(strcmp(type,"save_setup")==0)
  {
    retdoc["type"]="save_setup";
    const char* deny=cfgPersistDeny();
    if(deny==NULL)
    {
      retdoc["persisted"]=MachineConfig::save();
      doRsp=rspAck=true;
    }
    else
    {
      retdoc["persisted"]=false;
      retdoc["persist_err"]=deny;
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true;
      rspAck=false;
    }
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
    SKIP_Count=0;
    UNANSWERED_Count=0;
    CONSEC_UNANSWERED=0;
    ISR_GAP_MAX_CY=0;
    RBUF_PEAK=0;
    GATE_ACCEPT=GATE_REJ_RATE=GATE_REJ_DIST=GATE_REJ_BUSY=0;
    // The clock model too. Leaving it out made every segmented experiment
    // read the previous segment's numbers: an A/B control appeared to show 12
    // disagreements that were entirely leftovers, which nearly produced the
    // wrong conclusion about which matching mode is safe.
    CAM_SYNC.reset();
    REP_LAT_N=0;
    REP_LAT_SUM_US=0;
    REP_LAT_MAX_US=0;

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"get_running_stat")==0)
  {

    {
      JsonArray jERROR_HIST = retdoc.createNestedArray("error_hist");

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
    jCountInfo["UNANSWERED"]=UNANSWERED_Count;
    // Counted since the first build and reported by nothing until now, which
    // hid the real cost of out-of-order reports. Reporting tid N marks every
    // OLDER object still UNSET as SKIP; if that object's own verdict then
    // arrives before SWITCH it overwrites the SKIP and all is well, but if it
    // does not, the object passes the selector unjudged and -- unlike UNSET --
    // SKIP raises no error. So SKIP is the honest count of parts that went
    // through without a verdict, and err=2 systematically under-reports it.
    jCountInfo["SKIP"]=SKIP_Count;

    //current state
    retdoc["state"]=(int)sysinfo.state;

    retdoc["plate_freq"]=PLATE_FREQ_TARGET;//PLATE_FREQ_CURRENT;
    // if(SEL1_ACT_COUNTDOWN>=0)
    // {
    // }
    retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;

    // Pipeline snapshot: what is currently registered on the plate and where
    // each object is headed. Walked concurrently with the ISR that consumes
    // objects, so counts are a display-only approximation (each field read is
    // a single aligned access -- never garbage, just possibly one tick stale).
    {
      int reg=0,waiting=0,h1=0,h2=0,h3=0,hna=0,hskip=0;
      for(int i=0;i<RBuf.size();i++)
      {
        pipeLineInfo *p=RBuf.getTail(i);
        if(p==NULL)break;
        int32_t st=p->insp_status;
        if(st==insp_status_DEL)continue;   // consumed, awaiting cleanup sweep
        reg++;
        if(st==insp_status_UNSET)waiting++;
        else if(st==1)h1++;
        else if(st==2)h2++;
        else if(st==3)h3++;
        else if(st==0xFFFF)hna++;
        else if(st==insp_status_SKIP)hskip++;
      }
      JsonObject jP=retdoc.createNestedObject("pipe");
      jP["registered"]=reg;
      jP["waiting"]=waiting;
      JsonObject jH=jP.createNestedObject("heading");
      jH["SEL1"]=h1;
      jH["SEL2"]=h2;
      jH["SEL3"]=h3;
      jH["NA"]=hna;
      jH["SKIP"]=hskip;
    }
    {
      JsonObject jHl=retdoc.createNestedObject("health");
      jHl["min_heap"]=esp_get_minimum_free_heap_size();
      jHl["max_block"]=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      jHl["stack_hwm"]=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
      jHl["isr_gap_max_us"]=ISR_GAP_MAX_CY/240;   // 240MHz CPU
      jHl["rbuf_peak"]=RBUF_PEAK;
      jHl["uptime_s"]=(uint32_t)(esp_timer_get_time()/1000000ULL);
      jHl["consec_unanswered"]=CONSEC_UNANSWERED;
      jHl["rx_frames"]=djrl.rx_frames;
      jHl["rx_crc_ok"]=djrl.rx_crc_ok;
      jHl["rx_crc_fail"]=djrl.rx_crc_fail;
    }
    {
      // Gate admission. rate>0 means the fire-rate limit is actually engaging:
      // parts are arriving faster than min_detect_sep_us allows and are being
      // left on the plate to come round again, which is the intended behaviour
      // but needs to be visible -- otherwise "the machine is missing parts" and
      // "the machine is deliberately skipping parts" look identical.
      JsonObject jG=retdoc.createNestedObject("gate");
      jG["accept"]=GATE_ACCEPT;
      jG["rej_rate"]=GATE_REJ_RATE;
      jG["rej_dist"]=GATE_REJ_DIST;
      jG["rej_busy"]=GATE_REJ_BUSY;
      jG["disabled"]=(bool)GATE_DISABLED;
      // The host needs this to know when a calibration pulse is meaningful:
      // before the plate is at constant speed the stage offsets do not line up.
      jG["freq_stable"]=SYS_FREQ_STABLE;
      // Stage clock running with the plate held still -- test rig only.
      jG["dry_run"]=(bool)DRY_RUN;
      jG["min_sep_us"]=SYS_MIN_PULSE_TIME_SEP_us;      // configured
      jG["max_hz"]=SYS_MIN_PULSE_TIME_SEP_us ?
                     (uint32_t)(1000000UL/SYS_MIN_PULSE_TIME_SEP_us) : 0;
      // What the gate is enforcing right now. Equal to the configured value
      // unless the auto-rate loop has backed off.
      jG["eff_sep_us"]=GATE_SEP_EFF_us;
      jG["eff_hz"]=GATE_SEP_EFF_us ? (uint32_t)(1000000UL/GATE_SEP_EFF_us) : 0;
      jG["auto_rate"]=(bool)AUTO_RATE;
      jG["auto_floor_hz"]=AUTO_RATE_FLOOR_us ?
                     (uint32_t)(1000000UL/AUTO_RATE_FLOOR_us) : 0;
      jG["auto_backoffs"]=AUTO_RATE_BACKOFFS;
      jG["auto_recovers"]=AUTO_RATE_RECOVERS;
    }
    {
      // The migration's evidence. agree/disagree is the whole argument for
      // promoting the timestamp match: the new mechanism checked against the
      // old one on real traffic, continuously, at no cost. resid says whether
      // the clock model is healthy -- tens of us is right, drifting toward
      // TOL_US means the EWMA is losing the crystals.
      JsonObject jS=retdoc.createNestedObject("cam_sync");
      jS["valid"]=CAM_SYNC.valid;
      jS["authoritative"]=REPORT_MATCH_TS;
      jS["offset_us"]=(double)CAM_SYNC.offset_us;
      jS["resid_us"]=(int32_t)CAM_SYNC.last_resid_us;
      jS["resid_max_us"]=(int32_t)CAM_SYNC.max_resid_us;
      jS["learned"]=CAM_SYNC.learned;
      jS["agree"]=CAM_SYNC.agree;
      jS["disagree"]=CAM_SYNC.disagree;
      // rejected up while resid stays small = the outlier guard doing its job.
      // rebuilds up = the offset genuinely moved and was re-learned.
      jS["rejected"]=CAM_SYNC.rejected;
      jS["rebuilds"]=CAM_SYNC.rebuilds;
      // Unambiguous samples emitted. `learned` should now equal this: any
      // excess means something other than a sync pulse taught the estimate.
      jS["sync_pulses"]=SYNC_EMITTED;
    }
    {
      JsonObject jL=retdoc.createNestedObject("report_latency");
      jL["n"]=REP_LAT_N;
      jL["avg_us"]=REP_LAT_N ? (uint32_t)(REP_LAT_SUM_US/REP_LAT_N) : 0;
      jL["max_us"]=REP_LAT_MAX_US;
    }

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"report")==0)
  {
    int tid=(doc["tid"].is<int>()==true)?doc["tid"]:-1;
    int cat=(doc["cat"].is<int>()==true)?doc["cat"]:-1;
    // The camera's own timestamp for the frame this verdict came from. The host
    // knows this without knowing anything else, which is the point: it stops
    // having to work out WHICH object it inspected.
    uint64_t cam_ts = 0;
    if(doc["cam_ts"].is<uint64_t>()==true) cam_ts=doc["cam_ts"];
    else if(doc["cam_ts"].is<double>()==true) cam_ts=(uint64_t)(double)doc["cam_ts"];

    pipeLineInfo *tarP=NULL;

    // --- find the object, both ways, before touching anything ------------
    //
    // Separated from the SKIP sweep below on purpose. The old code found and
    // swept in one pass, which meant the sweep depended on which object it
    // happened to find first -- fine when there was only one way to look, wrong
    // now that there are two and they can disagree.
    pipeLineInfo *byTid=NULL;
    pipeLineInfo *byTs=NULL;
    int64_t bestDelta=0;
    if(cat!=-1)
    {
      int64_t want = CAM_SYNC.valid ? CAM_SYNC.expectedCamUs(cam_ts) : 0;
      for (int i = 0; i < RBuf.size(); i++)
      {
        pipeLineInfo *pipe = RBuf.getTail(i);
        if (pipe == NULL) break;
        if(tid!=-1 && pipe->tid==(uint32_t)tid) byTid=pipe;
        if(cam_ts!=0 && CAM_SYNC.valid && pipe->cam_us!=0)
        {
          int64_t d = (int64_t)pipe->cam_us - want; if(d<0) d=-d;
          if(d<=CamClockSync::TOL_US && (byTs==NULL || d<bestDelta)){ byTs=pipe; bestDelta=d; }
        }
      }
    }

    // --- cross-check, and learn ------------------------------------------
    //
    // While both are available the tid stays authoritative and the timestamp
    // match is only watched. That is what makes this migration free of risk:
    // the new mechanism has to agree with the old one, continuously and on real
    // production traffic, before anyone has to trust it. A disagreement is not
    // a tie to break -- it is a defect, reported as one.
    // Only sync pulses teach. An ordinary report is a sample of the offset
    // ONLY IF its pairing was right, which is the thing the offset is for --
    // so learning from them is circular and self-poisoning. Sync pulses are
    // fired with nothing else outstanding, so their pairing is certain.
    if(byTid!=NULL && cam_ts!=0 && byTid->sync) CAM_SYNC.observe(cam_ts, byTid->cam_us);
    if(byTid!=NULL && byTs!=NULL)
    {
      if(byTid==byTs) CAM_SYNC.agree++;
      else
      {
        CAM_SYNC.disagree++;
        djrl.dbg_printf("CAMSYNC MISMATCH tid=%u ts_tid=%u d=%lld",
                        (unsigned)byTid->tid,(unsigned)byTs->tid,(long long)bestDelta);
      }
    }

    // tid wins while it is present. REPORT_MATCH_TS promotes the timestamp to
    // authoritative once the agreement count says it has earned it.
    tarP = (REPORT_MATCH_TS && byTs!=NULL) ? byTs : (byTid!=NULL ? byTid : byTs);

    // --- sweep everything older than the chosen object --------------------
    if(tarP!=NULL)
    {
      for (int i = 0; i < RBuf.size(); i++)
      {
        pipeLineInfo *pipe = RBuf.getTail(i);
        if (pipe == NULL || pipe==tarP) break;
        if(pipe->insp_status==insp_status_UNSET)
          pipe->insp_status=insp_status_SKIP;
      }
    }

    if(tarP)
    {
      uint32_t pressure=tarP->gate_pulse+STAGE_PULSE_OFFSET.SWITCH-SYS_STEP_COUNT;
      // if(pressure<1000)
      // {
      //   PLATE_FREQ_SETPOINT=PLATE_FREQ_SETPOINT*19/20;
      // }
      retdoc["tr"]=pressure;
      if(tarP->insp_status==insp_status_UNSET)
      {
        uint32_t lat=(uint32_t)esp_timer_get_time()-tarP->trig_us;
        REP_LAT_N++;
        REP_LAT_SUM_US+=lat;
        if(lat>REP_LAT_MAX_US)REP_LAT_MAX_US=lat;
      }
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

  else if(strcmp(type,"pin_on")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      digitalWrite(pin,HIGH);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"pin_off")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];

      digitalWrite(pin,LOW);
    }
    doRsp=rspAck=true;
  }
  // Polarity-aware light hold. pin_on/pin_off above are RAW digitalWrite, and
  // with io_on_level.L1A=0 (ON is LOW, the current machine's config) they do
  // the opposite of what their names suggest -- not something to put behind an
  // operator's button. This one goes through io_drive() so "on" means lit
  // whatever the wiring polarity is.
  //   {"type":"light","ch":"L1A"|"L2A","on":true,"timeout_ms":60000}
  else if(strcmp(type,"light")==0)
  {
    doRsp=true;
    const char* ch = doc["ch"].is<const char*>() ? doc["ch"].as<const char*>() : "L1A";
    int pin=-1, idx=-1;
    if(strcmp(ch,"L1A")==0)      { pin=PIN_O_L1A; idx=IOI_L1A; }
    else if(strcmp(ch,"L2A")==0) { pin=PIN_O_L2A; idx=IOI_L2A; }
    else { retdoc["err"]="ch must be L1A or L2A"; rspAck=false; }

    if(pin>=0)
    {
      bool on = doc["on"].is<bool>() ? doc["on"].as<bool>() : true;
      if(on)
      {
        const char* deny=cfgPersistDeny();
        if(deny!=NULL)
        {
          // In INSPECTION the stage tasks drive these pins every part; a manual
          // hold would be stomped within milliseconds and read as a fault.
          retdoc["err"]=deny;
          retdoc["state"]=(int)sysinfo.state;
          rspAck=false;
        }
        else
        {
          uint32_t to = doc["timeout_ms"].is<uint32_t>()
                      ? (uint32_t)doc["timeout_ms"] : LIGHT_HOLD_DEFAULT_MS;
          if(to>LIGHT_HOLD_MAX_MS) to=LIGHT_HOLD_MAX_MS;
          io_drive(pin,idx,true);
          LIGHT_HOLD_pin=pin; LIGHT_HOLD_idx=idx;
          LIGHT_HOLD_deadline_ms=millis()+to;
          retdoc["on"]=true; retdoc["timeout_ms"]=to;
          rspAck=true;
        }
      }
      else
      {
        io_drive(pin,idx,false);
        LIGHT_HOLD_deadline_ms=0; LIGHT_HOLD_pin=-1; LIGHT_HOLD_idx=-1;
        retdoc["on"]=false;
        rspAck=true;
      }
    }
  }
  else if(strcmp(type,"pin_read")==0)
  {
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      retdoc["pin"]=pin;
      retdoc["val"]=digitalRead(pin);
      rspAck=true;
    }
    else if(doc["pins"].is<JsonArray>()==true)
    {
      JsonArray pins=doc["pins"];
      JsonArray vals=retdoc.createNestedArray("vals");
      for(JsonVariant p : pins)
      {
        vals.add(digitalRead(p.as<int>()));
      }
      retdoc["pins"]=pins;
      rspAck=true;
    }
    else
    {
      rspAck=false;
    }
    doRsp=true;
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
  else if(strcmp(type,"reboot_bootloader")==0)
  {
    // Enter the ROM serial bootloader without a physical BOOT press.
    //
    // This board's auto-reset circuit is only half wired: DTR->EN resets the
    // chip fine, but RTS->IO0 does nothing, so esptool always finds boot:0x1
    // (SPI boot) instead of download mode no matter how long IO0 is held --
    // measured at 0.05s, 0.3s and 0.8s, all identical. No timing setting can
    // fix a missing connection.
    //
    // The obvious software route does not exist on this chip. An earlier
    // version wrote RTC_CNTL_FORCE_DOWNLOAD_BOOT, which the ESP32-S2/S3/C3 ROM
    // honours -- but the original ESP32 has no such register. It is absent from
    // every header in this SDK, and writing the address anyway produced exactly
    // what you would expect: the command acked, the chip restarted
    // (rst:0xc SW_CPU_RESET), and it came straight back up as
    // boot:0x13 SPI_FAST_FLASH_BOOT.
    //
    // DOES NOT WORK EITHER, and is kept only so the next person does not spend
    // the afternoon rediscovering it. The idea was to drive the strapping pin
    // and make the level outlive the reset: GPIO0 is an RTC GPIO, and the RTC
    // domain is not cleared by a software reset, so rtc_gpio_hold_en ought to
    // keep the pad low while the ROM samples it.
    //
    // Measured, both ways round:
    //   hold + esp_restart()          -> rst:0xc SW_CPU_RESET,  boot:0x13
    //   hold + RTC_CNTL_SW_SYS_RST    -> rst:0x3 SW_RESET,      boot:0x13
    // 0x13 is SPI_FAST_FLASH_BOOT; download mode is 0x03, and the bit that
    // differs is IO0. So the system reset half is right -- the reset reason
    // changed exactly as intended -- and the pad half is not: this chip's RTC
    // hold is a deep-sleep facility and the pad is not held low at the moment
    // the straps latch.
    //
    // There is no software route on the original ESP32, and none is needed
    // anymore: the actual fix was a 4k resistor from IO0 to GND. The board's
    // RTS->IO0 leg was present but too weak to pull below VIL against the
    // pull-up; 4k to ground moves the operating point far enough that RTS now
    // crosses the threshold, while the resting level still boots normally
    // (verified 28/28 resets into the application, 3/3 uploads with no button).
    //
    // So this command is not the flashing path -- `pio run -t upload` is. What
    // survives here is a plain software reboot, which is occasionally useful on
    // its own. Nothing depends on it.
    //
    // Reply first -- after the restart there is nobody left to answer.
    retdoc["type"]="reboot_bootloader";
    retdoc["ack"]=true;
    {
      int slen=serializeJson(retdoc,(char*)dataBuff,sizeof(dataBuff));
      djrl.send_json_string(0,dataBuff,slen,0);
    }
    Serial.flush();
    delay(80);
    rtc_gpio_hold_dis((gpio_num_t)0);      // in case a previous attempt left it
    rtc_gpio_init((gpio_num_t)0);
    rtc_gpio_set_direction((gpio_num_t)0, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)0, 0);
    rtc_gpio_hold_en((gpio_num_t)0);
    delay(10);
    // A SYSTEM reset, not esp_restart().
    //
    // esp_restart() ends in esp_cpu_reset(), which is a SW_CPU_RESET (rst:0xc)
    // -- it restarts the CPU but does not re-latch GPIO_STRAP. The ROM then
    // reads the strapping captured at the last system reset, i.e. IO0 as it was
    // at power-on, and holding the pad low now changes nothing. That is exactly
    // what was observed: pad held, and still boot:0x13 SPI_FAST_FLASH_BOOT.
    //
    // RTC_CNTL_SW_SYS_RST re-latches the straps, and leaves the RTC domain --
    // and therefore the pad hold -- alone, which is the combination this needs.
    SET_PERI_REG_MASK(RTC_CNTL_OPTIONS0_REG, RTC_CNTL_SW_SYS_RST);
    while(1){}   // not reached
    doRsp=false;
  }

  else if(strcmp(type,"set_gate_disable")==0)
  {
    if(doc["on"].is<bool>()==true)
      GATE_DISABLED = (bool)doc["on"];
    retdoc["gate_disabled"]=(bool)GATE_DISABLED;
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"trig_phantom_pulse")==0)
  {
    phantomEmitOne();
    doRsp=rspAck=true;
  }

  // A train of phantom objects at an exact interval, emitted by the device.
  // {count, hz|period_us}; count:0 cancels a running train. Reply carries the
  // measured min/max interval of the PREVIOUS train, so the rig can state the
  // load it actually applied rather than the one it asked for.
  else if(strcmp(type,"trig_phantom_train")==0)
  {
    retdoc["type"]="trig_phantom_train";
    retdoc["prev_emitted"]=PH_TRAIN_EMITTED;
    retdoc["prev_min_us"]=PH_TRAIN_MIN_US;
    retdoc["prev_max_us"]=PH_TRAIN_MAX_US;

    int count=doc["count"].is<int>() ? (int)doc["count"] : 0;
    int period_us=doc["period_us"].is<int>() ? (int)doc["period_us"] : 0;
    if(period_us<=0 && doc["hz"].is<float>() && (float)doc["hz"]>0.0f)
      period_us=(int)(1000000.0f/(float)doc["hz"]);
    if(period_us<=0) period_us=100000;      // 10 Hz
    if(period_us<2000) period_us=2000;      // 500 Hz is already absurd here
    if(count<0) count=0;
    if(count>20000) count=20000;

    PH_TRAIN_EMITTED=0; PH_TRAIN_PREV_US=0;
    PH_TRAIN_MIN_US=0;  PH_TRAIN_MAX_US=0;
    PH_TRAIN_PERIOD_US=period_us;
    PH_TRAIN_NEXT_US=esp_timer_get_time();
    PH_TRAIN_LEFT=count;

    retdoc["count"]=count;
    retdoc["period_us"]=period_us;
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"set_sel1_cd")==0)
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

  else if(strcmp(type,"set_dry_run")==0)
  {
    // Only from a standstill, in both directions. Muting mid-spin is an abrupt
    // stop with the driver still energised; unmuting mid-spin would start
    // stepping a plate the caller believes is stopped. Either way the answer is
    // "bring it to rest first".
    bool on = doc["on"].is<bool>() ? (bool)doc["on"] : true;
    retdoc["type"]="set_dry_run";
    if(PLATE_FREQ_CURRENT!=0)
    {
      retdoc["dry_run_err"]="plate must be at rest (plate_freq 0) to change dry_run";
      retdoc["plate_freq_current"]=PLATE_FREQ_CURRENT;
      doRsp=true; rspAck=false;
    }
    else
    {
      DRY_RUN=on;
      if(on)
      {
        // Holding the plate still is the whole point, and that needs the driver
        // energised. Leaving ENABLE off would put us back in the state this
        // replaces: a free plate that shakes its parts off.
        digitalWrite(STEPPER_EN_PIN,stepper_en_active);
        SYS_STEPPER_DISABLED=false;
      }
      retdoc["dry_run"]=DRY_RUN;
      retdoc["stepper_disabled"]=SYS_STEPPER_DISABLED;
      doRsp=rspAck=true;
    }
  }
  else if(strcmp(type,"stepper_enable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,stepper_en_active);
    SYS_STEPPER_DISABLED=false;
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"stepper_disable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,!stepper_en_active);
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
      io_drive(PIN_O_SEL1,IOI_SEL1,true);
      delay(delay_ms);
      io_drive(PIN_O_SEL1,IOI_SEL1,false);
      rspAck=true;
      break;
      case 2:
      io_drive(PIN_O_SEL2,IOI_SEL2,true);
      delay(delay_ms);
      io_drive(PIN_O_SEL2,IOI_SEL2,false);
      rspAck=true;
      break;
      case 3:
      io_drive(PIN_O_SEL3,IOI_SEL3,true);
      delay(delay_ms);
      io_drive(PIN_O_SEL3,IOI_SEL3,false);
      rspAck=true;
      break;
    }
    doRsp=true;
  }
  
  else if(strcmp(type,"wdt_test")==0)
  {
    // Starve the task watchdog on purpose -- drills the WDT->panic->forensics
    // chain the same way crash_test drills a null-deref panic.
    if(doc["confirm"].is<bool>() && doc["confirm"].as<bool>())
    {
      WDT_TEST_REQ=1;
      retdoc["starving"]=true;
      rspAck=true;
    }
    else
    {
      retdoc["err"]="needs confirm:true";
      rspAck=false;
    }
    doRsp=true;
  }
  else if(strcmp(type,"crash_test")==0)
  {
    // Deliberately kill the firmware to exercise the host's crash forensics
    // (NOISE capture of the panic dump + reset_reason on the next boot).
    // Refused without confirm:true so a stray command can't stop a machine.
    if(doc["confirm"].is<bool>() && doc["confirm"].as<bool>())
    {
      CRASH_REQ = (doc["mode"].is<const char*>()
                   && strcmp(doc["mode"].as<const char*>(),"abort")==0) ? 2 : 1;
      retdoc["crashing"]=true;
      rspAck=true;
    }
    else
    {
      retdoc["err"]="needs confirm:true";
      rspAck=false;
    }
    doRsp=true;
  }
  else if(strcmp(type,"bye")==0)
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
    
    uint8_t buff[2048];
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
  if(strcmp(type,"aux_test")==0)
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
  // setRxBufferSize MUST precede begin(): HardwareSerial::setRxBufferSize()
  // bails out with "RX Buffer can't be resized when Serial is already running"
  // the moment _uart exists, so the old begin()-then-resize order silently left
  // the buffer at the 256-byte default. Anything longer than that -- a full
  // set_setup with stage_pulse_offset and io_on_level is ~500-670 bytes --
  // overran the FIFO and arrived corrupted mid-frame ("'plate_<garbage>:2000"),
  // which the parser reported as JSON_FORMAT_ERROR/INIT_CHAR_ERROR. The stream
  // then desynced and every later command, PING included, went unanswered, so
  // the host declared the link dead and reconnected every 9s forever.
  // 2048 matches Data_Layer_Protocol's dataBuff, so a frame the protocol layer
  // can hold is a frame the UART can hold.
  Serial.setRxBufferSize(2048);
  Serial.begin(115200);//230400);
  // Serial.begin(460800);
  // Serial.setHwFlowCtrlMode(0);
  // // setup_comm();

  // Must run before the timer is armed: the pulse offsets and plate frequency
  // it restores are what everything below derives its timing from.
  // XTAL autodetect guard: this board's 40MHz crystal is occasionally
  // misdetected as 26MHz on noisy boots (EN bounce), which shifts the whole
  // clock tree by 26/40 -- UART 115200 becomes an unintelligible 74880 and
  // every timing constant is silently wrong. Detect and re-boot until the
  // detection lands right (bounded by an RTC-memory counter so a genuinely
  // 26MHz board could never brick).
  {
    static RTC_NOINIT_ATTR uint32_t xtal_retry;
    if(esp_reset_reason()==ESP_RST_POWERON) xtal_retry=0;
    int mhz=(int)rtc_clk_xtal_freq_get();
    if(mhz!=40 && xtal_retry<5)
    {
      xtal_retry++;
      esp_restart();
    }
  }

  MachineConfig::begin();
  // The gate enforces GATE_SEP_EFF_us, not the configured value, so it has to be
  // seeded from whatever begin() just restored. Without this a reboot left it at
  // its 4000us initialiser -- a wide-open 250/s gate on a machine configured for
  // 35/s, which is precisely the overload the rate limit exists to prevent.
  GATE_SEP_EFF_us = SYS_MIN_PULSE_TIME_SEP_us;

  // Task watchdog on the main loop: a wedged parser/deadlock must reboot
  // (and leave TASK_WDT in reset_reason), not spin the ISR blind forever.
  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);
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

  digitalWrite(STEPPER_DIR_PIN,stepper_dir_level);
  digitalWrite(STEPPER_EN_PIN,stepper_en_active);
  SYS_STEPPER_DISABLED=false;
  



  pinMode(PIN_O_L1A, OUTPUT);
  pinMode(PIN_O_CAM1, OUTPUT);

  pinMode(PIN_O_L2A, OUTPUT);
  pinMode(PIN_O_CAM2, OUTPUT);


  pinMode(PIN_O_SEL1, OUTPUT);
  pinMode(PIN_O_SEL2, OUTPUT);
  pinMode(PIN_O_SEL3, OUTPUT);

  // Rest every actuator at its logical OFF level -- with an active-low output
  // the reset-default LOW would otherwise mean "energised" until first use.
  io_drive(PIN_O_L1A,IOI_L1A,false);
  io_drive(PIN_O_CAM1,IOI_CAM1,false);
  io_drive(PIN_O_L2A,IOI_L2A,false);
  io_drive(PIN_O_CAM2,IOI_CAM2,false);
  io_drive(PIN_O_SEL1,IOI_SEL1,false);
  io_drive(PIN_O_SEL2,IOI_SEL2,false);
  io_drive(PIN_O_SEL3,IOI_SEL3,false);

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
  esp_task_wdt_reset();
  syncPulseService();
  phantomTrainService();
  // Drop a manual light hold when it expires, or the moment the machine leaves
  // IDLE -- entering inspection hands these pins back to the stage tasks.
  if(LIGHT_HOLD_deadline_ms!=0)
  {
    bool expired = (int32_t)(millis()-LIGHT_HOLD_deadline_ms) >= 0;
    bool notIdle = (sysinfo.state!=SYS_STATE::IDLE &&
                    sysinfo.state!=SYS_STATE::INSPECTION_MODE_READY);
    if(expired || notIdle)
    {
      if(LIGHT_HOLD_pin>=0) io_drive(LIGHT_HOLD_pin,LIGHT_HOLD_idx,false);
      LIGHT_HOLD_deadline_ms=0; LIGHT_HOLD_pin=-1; LIGHT_HOLD_idx=-1;
    }
  }
  if(host_timeout_ms>0 && sysinfo.state==SYS_STATE::INSPECTION_MODE_READY)
  {
    uint32_t last=djrl.last_rx_ms;
    if(last!=0 && (millis()-last) > (uint32_t)host_timeout_ms)
    {
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                         (int)GEN_ERROR_CODE::HOST_LINK_TIMEOUT);
    }
  }
  if(WDT_TEST_REQ)
  {
    delay(200);              // let the ack out
    for(;;){}                // starve the task WDT -> panic -> TASK_WDT reboot
  }
  if(CRASH_REQ)
  {
    delay(200);              // let the ack reach the wire first
    if(CRASH_REQ==2)
    {
      abort();               // panic via abort(): SW reset, abort backtrace
    }
    *(volatile int*)0 = 42;  // LoadProhibited/StoreProhibited Guru Meditation
  }

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
        retdoc["type"]="cam_trig";
        retdoc["q"]=EVENT_SEQ++;
        retdoc["tid"]=trig.trig_id;
        retdoc["cam"]=trig.btrig_idx;
        retdoc["t_us"]=trig.trig_time_us;
        retdoc["gate_pulse"]=trig.gate_pulse;
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
          retdoc["type"]="cam_trig_tagged"; 
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
          retdoc["type"]="cam_trig";
          retdoc["q"]=EVENT_SEQ++;
          retdoc["tid"]=info.trig_id;
          retdoc["cam"]=info.btrig_idx;
          retdoc["t_us"]=(uint64_t)info.trig_time_us;
          retdoc["gate_pulse"]=info.gate_pulse;
          retdoc["Qs"]=RBuf.size();
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case TaskQ2CommInfo_Type::system_info :
        {
          retdoc["type"]="system_info";
          retdoc["q"]=EVENT_SEQ++;

          retdoc["state"]=(int)sysinfo.state;
          

          {
            JsonArray jERROR_HIST = retdoc.createNestedArray("error_hist");

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
  static int64_t lastRampUs=0;
  do{//timer freq ctrl
    subDiv=(subDiv+1)&(0xFF);
    if(subDiv!=0)break;
    int64_t nowUs=esp_timer_get_time();
    float dt=(nowUs-lastRampUs)*1e-6f;
    lastRampUs=nowUs;
    if(PLATE_FREQ_CURRENT==PLATE_FREQ_TARGET)
    {
      if(PLATE_FREQ_TARGET==0 && SYS_FREQ_STABLE==false)//just stable
      {
        ALL_OUTPUTS_SAFE();
      }
      SYS_FREQ_STABLE=true;
      break;
    }
    SYS_FREQ_STABLE=false;
    bool TimerNeedsStart=false;
    if(PLATE_FREQ_CURRENT==0)
    {
      TimerNeedsStart=true;
    }
    // Wall-time ramp: accel is Hz/s regardless of loop speed. dt is clamped so
    // a stall (long serial burst, NVS write) can't turn into a frequency jump.
    if(dt<0)dt=0;
    if(dt>0.25f)dt=0.25f;
    float step=(SYS_FREQ_ACCEL>0) ? SYS_FREQ_ACCEL*dt : 3.4e38f;
    if(PLATE_FREQ_CURRENT>PLATE_FREQ_TARGET)
    {
      if(PLATE_FREQ_TARGET==0 && PLATE_FREQ_CURRENT<10)
      {
        PLATE_FREQ_CURRENT=0;
      }
      else
      {
        PLATE_FREQ_CURRENT-=step;
        if(PLATE_FREQ_CURRENT<PLATE_FREQ_TARGET)
        {
          PLATE_FREQ_CURRENT=PLATE_FREQ_TARGET;
        }
      }
    }
    else
    {
      PLATE_FREQ_CURRENT+=step;
      if(PLATE_FREQ_CURRENT>PLATE_FREQ_TARGET)
      {
        PLATE_FREQ_CURRENT=PLATE_FREQ_TARGET;
      }
    }



    if(PLATE_FREQ_CURRENT==0)
    {
      timerAlarmDisable(timer);
    }
    else
    {
      timerAlarmWrite(timer, (uint64_t)((_TICK2SEC_BASE_>>1)/PLATE_FREQ_CURRENT), true);
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

  jdoc["plate_freq"]=PLATE_FREQ_SETPOINT;
  jdoc["plate_accel"]=SYS_FREQ_ACCEL;
  jdoc["min_detect_sep_us"]=SYS_MIN_PULSE_TIME_SEP_us;
  jdoc["auto_rate"]=(bool)AUTO_RATE;
  jdoc["auto_rate_floor_us"]=AUTO_RATE_FLOOR_us;
  jdoc["auto_rate_recover_n"]=AUTO_RATE_RECOVER_N;
  jdoc["report_match_ts"]=REPORT_MATCH_TS;

  jdoc["pulse_min_width"]=minWidth;
  jdoc["pulse_max_width"]=maxWidth;

  jdoc["gate_debounce_rise"]=DEBOUNCE_H_THRES;
  jdoc["gate_debounce_fall"]=DEBOUNCE_L_THRES;

  jdoc["stepper_en_active"]=stepper_en_active;
  jdoc["stepper_dir"]=stepper_dir_level;

  jdoc["unanswered_policy"]=UNANSWERED_POLICY;
  jdoc["unanswered_stop_after"]=UNANSWERED_STOP_AFTER;
  jdoc["pulses_per_rev"]=pulses_per_rev;
  jdoc["plate_diameter_mm"]=plate_diameter_mm;

  {
    JsonObject jIO = jdoc.createNestedObject("io_on_level");
    for(size_t i=0;i<SARRL(IO_POL_TAB);i++)
      jIO[IO_POL_TAB[i].name]=IO_IS_INV(IO_POL_TAB[i].idx)?0:1;
  }

  // Lets the host tell the two machines apart and see whether what it is
  // reading came from NVS or is just the compiled fallback.
  jdoc["machine_id"]=MachineConfig::machineId();
  jdoc["cfg_from_nvs"]=MachineConfig::isLoadedFromNVS();
  // cfg_crc is NOT added here. MachineConfig::hash() fingerprints the image
  // this function produces, so calling it from inside would recurse -- and
  // each frame carries a 3KB document, so it overflows the stack rather than
  // merely being slow. The get_setup handler adds it once, at the top.
  jdoc["host_timeout_ms"]=host_timeout_ms;

  // Why the chip last booted: lets a host that finds the board freshly in
  // IDLE tell a panic/watchdog/brownout from a plain power cycle.
  {
    static const char* rr_names[]={"UNKNOWN","POWERON","EXT","SW","PANIC",
                                   "INT_WDT","TASK_WDT","WDT","DEEPSLEEP",
                                   "BROWNOUT","SDIO"};
    int rr=(int)esp_reset_reason();
    jdoc["reset_reason"]=rr;
    jdoc["reset_reason_name"]=(rr>=0 && rr<11)?rr_names[rr]:"?";
    jdoc["xtal_mhz"]=(int)rtc_clk_xtal_freq_get();
  }


  {
    JsonArray jERROR_HIST = jdoc.createNestedArray("error_hist");

    for(int i=0;i<ERROR_HIST.size();i++)
    {
      jERROR_HIST.add((int)*ERROR_HIST.getTail(i));
    }
  }


  // jdoc["PLATE_FREQ_TARGET"]=PLATE_FREQ_TARGET;
  jdoc["cur_state"]=(int)sysinfo.state;
  jdoc["step_count"]=(int)SYS_STEP_COUNT;

  
}




#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}


// apply_hw=false assigns the globals and touches nothing else.
//
// MachineConfig::begin() runs BEFORE pinMode(), so driving a pin from there
// would write to an unconfigured GPIO. It does not need to: firmwareSetup rests
// every actuator at its logical OFF level right after pinMode, and that reads
// IO_INV_MASK -- so setting the variables is enough to come up correct.
void setMachineSetup(JsonDocument &jdoc, bool apply_hw)
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

  JSON_SETIF_ABLE(PLATE_FREQ_SETPOINT,jdoc,"plate_freq");
  JSON_SETIF_ABLE(SYS_FREQ_ACCEL,jdoc,"plate_accel");
  JSON_SETIF_ABLE(SYS_MIN_PULSE_TIME_SEP_us,jdoc,"min_detect_sep_us");
  // Changing the configured rate always resets the live one: the operator asked
  // for a rate, not for whatever the loop had crept to.
  if(jdoc["min_detect_sep_us"].is<int>()) GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us;
  {
    bool ar=AUTO_RATE;
    JSON_SETIF_ABLE(ar,jdoc,"auto_rate");
    if(ar!=AUTO_RATE){ AUTO_RATE=ar; AUTO_RATE_OK_RUN=0;
                       if(!ar) GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us; }
  }
  JSON_SETIF_ABLE(AUTO_RATE_FLOOR_us,jdoc,"auto_rate_floor_us");
  JSON_SETIF_ABLE(AUTO_RATE_RECOVER_N,jdoc,"auto_rate_recover_n");
  if(AUTO_RATE_RECOVER_N<1) AUTO_RATE_RECOVER_N=1;
  if(AUTO_RATE_FLOOR_us<SYS_MIN_PULSE_TIME_SEP_us)
    AUTO_RATE_FLOOR_us=SYS_MIN_PULSE_TIME_SEP_us;
  if(GATE_SEP_EFF_us<SYS_MIN_PULSE_TIME_SEP_us)
    GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us;
  JSON_SETIF_ABLE(REPORT_MATCH_TS,jdoc,"report_match_ts");

  JSON_SETIF_ABLE(minWidth,jdoc,"pulse_min_width");
  JSON_SETIF_ABLE(maxWidth,jdoc,"pulse_max_width");
  JSON_SETIF_ABLE(DEBOUNCE_H_THRES,jdoc,"gate_debounce_rise");
  JSON_SETIF_ABLE(DEBOUNCE_L_THRES,jdoc,"gate_debounce_fall");

  if(jdoc["stepper_en_active"].is<int>() || jdoc["stepper_dir"].is<int>())
  {
    JSON_SETIF_ABLE(stepper_en_active,jdoc,"stepper_en_active");
    JSON_SETIF_ABLE(stepper_dir_level,jdoc,"stepper_dir");
    stepper_en_active = stepper_en_active ? 1 : 0;
    stepper_dir_level = stepper_dir_level ? 1 : 0;
    // Re-drive both pins so the new polarity takes effect now, preserving the
    // driver's current enabled/disabled state under the new convention.
    if(apply_hw)
    {
      digitalWrite(STEPPER_DIR_PIN, stepper_dir_level);
      digitalWrite(STEPPER_EN_PIN, SYS_STEPPER_DISABLED ? !stepper_en_active
                                                        : stepper_en_active);
    }
  }

  {
    int v=UNANSWERED_POLICY;
    if(jdoc["unanswered_policy"].is<int>()){ v=jdoc["unanswered_policy"]; UNANSWERED_POLICY=(v==1)?1:0; }
    v=UNANSWERED_STOP_AFTER;
    if(jdoc["unanswered_stop_after"].is<int>()){ v=jdoc["unanswered_stop_after"]; UNANSWERED_STOP_AFTER=(v<1)?1:v; }
  }
  JSON_SETIF_ABLE(host_timeout_ms,jdoc,"host_timeout_ms");
  JSON_SETIF_ABLE(pulses_per_rev,jdoc,"pulses_per_rev");
  JSON_SETIF_ABLE(plate_diameter_mm,jdoc,"plate_diameter_mm");

  if(jdoc["io_on_level"].is<JsonObject>())
  {
    JsonObject jIO=jdoc["io_on_level"];
    uint32_t mask=IO_INV_MASK;
    for(size_t i=0;i<SARRL(IO_POL_TAB);i++)
    {
      if(jIO[IO_POL_TAB[i].name].is<int>())
      {
        if(jIO[IO_POL_TAB[i].name].as<int>())
          mask&=~(1u<<IO_POL_TAB[i].idx);   // ON is HIGH
        else
          mask|=(1u<<IO_POL_TAB[i].idx);    // ON is LOW
      }
    }
    IO_INV_MASK=mask;
    // Re-rest every actuator at its logical OFF under the new polarity, so a
    // flipped output doesn't sit energised at its old idle level. FEEDER keeps
    // its current logical state instead -- it is level-driven, not pulsed.
    if(apply_hw)
    {
      for(size_t i=0;i<SARRL(IO_POL_TAB);i++)
      {
        if(IO_POL_TAB[i].idx==IOI_FEEDER)
          io_drive(IO_POL_TAB[i].pin,IOI_FEEDER,FEEDER_ON);
        else
          io_drive(IO_POL_TAB[i].pin,IO_POL_TAB[i].idx,false);
      }
    }
  }
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

