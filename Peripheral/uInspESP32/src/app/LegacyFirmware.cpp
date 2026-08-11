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

// 30000us = 33/s. This was 4000us (250/s), which is faster than ANY camera
// configuration measured on this machine -- 5420us (184.5 fps) at the
// production crop, 28425us (~35 fps) at full frame. A compiled default that
// exceeds the frame floor admits a trigger density the camera cannot take, and
// what that produces is not a dropped part: it is triggers with no frames, so
// the host's pairing walks off by one and STAYS off.
//
// Defaulted slow rather than fast because the two errors are not symmetric.
// Too slow costs throughput, which is visible on the panel and fixed by typing
// a number. Too fast mis-sorts, and looks healthy while doing it. 33/s sits
// under both measured floors, so a board that comes up with no NVS config is
// safe whatever ROI it is pointed at -- and slow enough that nobody ships it
// by accident.
uint32_t SYS_MIN_PULSE_TIME_SEP_us=30000;

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
// (double) first: dist_um is uint32 and the multiply by 60000 wraps above
// 71582 um, so min_detect_dist_um 100000 silently resolved to 2261 ticks
// (28.4mm) instead of 7958 (100mm) -- a gate an order of magnitude looser
// than configured, with nothing saying so.
#define _PLAT_DIST_step(dist_um) ((int)((double)(dist_um)*_PLAT_PULSE_PER_TURN/_PLAT_CIRC_um))

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
  // Retirement, owned by whoever finished with the object and never written by
  // the report path. The drain used to key off insp_status==DEL, which put the
  // ONE field the main loop writes freely in charge of whether a slot can ever
  // be freed:
  //
  //   main loop reads insp_status (not DEL) -> ISR runs SWITCH and writes DEL
  //   -> main loop stores cat over it. Nothing sets DEL again; that object's
  //   SWITCH task is spent. The drain stops at the first non-DEL tail, so RBuf
  //   fills to 100, newPulseEvent starts returning GATE_REJ_BUSY for every
  //   part, no camera fires, no SWITCH runs, CONSEC_UNANSWERED never advances
  //   -- the machine sits in READY with the plate turning and the feeder on,
  //   inspecting nothing, and raises no error at all.
  //
  // A host verdict reached the same state by a second route: cat below
  // insp_status_DEL (-1000) passes the worst-wins comparison and overwrites it.
  //
  // Separating the flag removes both. The ISR and the sync sweep set it; the
  // report path never touches it; the drain reads only it. No lock, because
  // after this there is nothing to contend over.
  volatile uint8_t retired;
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
  // How wide the gate sensor was blocked, in plate steps. The width filter
  // (pulse_min_width / pulse_max_width) is the only thing separating a part
  // from anything else that breaks the beam, and it could not be tuned because
  // the measurement was thrown away the moment it was tested against. Reported
  // in cam_trig so the two populations can actually be compared.
  uint32_t w;
  // The board's own count of camera trigger pulses at the instant THIS
  // object's trigger fired -- the pulse-count twin of cam_us.
  //
  // The camera keeps the same count independently (ExtTriggerCount, decoded
  // out of the frame watermark and sent back in the report as `pcnt`), so the
  // two differ by a constant that is fixed at power-on and learned once. That
  // makes it a second, entirely separate way to say which frame belongs to
  // which object: cam_us pairs by WHEN, cam_pcnt pairs by HOW MANY.
  //
  // Its value is that it fails differently. A clock mispairing is a
  // continuous error -- drift, latency, a window slightly too wide -- and can
  // be off by one object while still looking reasonable. A count mispairing is
  // discrete: it is either exactly right or off by a whole pulse, and a pulse
  // the camera refused shifts it permanently and visibly. Neither can hide the
  // other's failure, which is the whole point of running both.
  //
  // 0 means "this object's camera stage has not fired", same convention as
  // cam_us, and the matcher skips zeroes for the same reason.
  uint32_t cam_pcnt;
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
  // Match window, set_setup "cam_match_window_us". Not a constant because the
  // right value follows the object spacing, which is a property of the line and
  // not of this code: it has to be well inside the spacing (so a frame cannot be
  // within one window of two objects) and well outside the worst delta actually
  // observed. Both of those numbers are in get_running_stat next to this one --
  // gate.eff_sep_us and cam_sync.delta_max_us -- so the setting can be checked
  // against measurement rather than guessed.
  // int32, like every other setting here: JSON_SETIF_ABLE gates on
  // is<typeof(var)>(), and ArduinoJson's long-long support is conditional, so an
  // int64 target silently never matches and the setting can never be applied.
  // Microseconds in an int32 reach 35 minutes; there is nothing to gain from 64.
  static int32_t TOL_US;
  static const int     BOOT_N   = 8;      // samples to establish the first offset
  // Consecutive frames whose nearest object is outside the window before the
  // machine is stopped. Two, not sixteen: one is a lost frame or a stray, two
  // in a row is the clock, and there is nothing to be gained by letting a
  // machine that cannot place its frames keep sorting parts.
  static const int     LOST_N   = 2;

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
  // Set when the clock has been lost and the machine must stop. Raised here,
  // acted on at the call site, which is where SYS_STATE_Transfer lives.
  bool     fault_pending = false;
  // When the current offset was measured, so its age -- and therefore how much
  // drift it has accumulated -- is readable. resid/age is the drift rate.
  uint64_t est_cam_us = 0;
  uint32_t established = 0;
  // The worst nearest-delta that was still accepted -- i.e. how much of the
  // window the machine actually needed. This is the number the window should be
  // set against: if it stays at hundreds of us and the window is thousands,
  // there is real margin; if it creeps toward the window, there is not.
  int64_t  delta_max_us = 0;
  // Accepted-residual distribution, log2 buckets from 32us. See gate().
  static const int DELTA_BUCKETS = 8;
  uint32_t delta_hist[DELTA_BUCKETS] = {0};
  int64_t  delta_last_us = 0;
  // The deltas of frames that were REFUSED. delta_max_us only records what was
  // accepted, so without these a halt says "two frames were outside the window"
  // and nothing about whether they were 6ms out or 400ms out -- and those two
  // have completely different causes.
  int64_t  miss_delta_last_us = 0;
  int64_t  miss_delta_max_us = 0;
  // Device time since the previous measurement. Without it last_resid_us cannot
  // be read at all: the offset is re-measured every report, so the residual is
  // drift accrued over THIS gap -- the same -211us means a healthy machine over
  // a 6s idle gap and a broken one over 100ms. resid/gap is the drift rate,
  // which is the number that is actually comparable between runs.
  int64_t  last_gap_us = 0;
  // Drift rate, parts per billion, signed. Opt-in (cam_drift_comp).
  //
  // The offset is re-measured every report and so carries no lag, but between
  // measurements it is a CONSTANT while the truth is a RAMP. Estimating the
  // ramp's slope lets the prediction follow it, which is what turns idle drift
  // from "error accumulating at 35us/s" into "error accumulating at whatever is
  // left after correction".
  //
  // Filtering the SLOPE is safe where filtering the offset was not. A filter's
  // lag is proportional to the rate of change of what it tracks; the offset
  // ramps continuously, so a filter on it lags forever (that was the -3430us
  // EWMA). The slope is near-constant -- it moves with temperature over
  // minutes -- so a slow filter on it settles and stays settled.
  //
  // ppb keeps this integer: 35us/s is 35000ppb, and slope_ppb * elapsed_us
  // stays inside int64 for any elapsed worth correcting.
  //
  // TODO (deferred 2026-08-05, do not enable this before doing it): offset and
  // slope should be estimated JOINTLY with a learning rate, not separately as
  // they are here -- an alpha-beta filter, where a residual corrects both terms
  // at once with alpha on the offset and beta on the slope. Estimating them
  // independently, as this does, means the slope is fitted to residuals that
  // the offset has already partly absorbed, and the two can chase each other.
  // The A/B that shipped with this showed the slope estimated correctly
  // (-17.5us/s against -20.6 actual) while delta_max did not improve, which is
  // consistent with exactly that.
  //
  // Watch the arithmetic too. inst_ppb = resid * 1e9 / gap, and resid has 1us
  // granularity, so at the 1s minimum gap one count of resid noise is 1000ppb
  // = 1us/s of slope noise; only the long gaps measure the slope well. Any
  // joint estimator needs more headroom than int32 ppb before it is trusted.
  //
  // Also note the experiment that produced "no improvement" was run over burst
  // gaps of tens of ms, where there is nothing for a slope to correct. The case
  // this is FOR is a slow line -- parts minutes apart -- and it was never tested
  // there. Retest with a known long idle before concluding anything.
  int32_t  slope_ppb = 0;
  uint32_t slope_n = 0;
  // Why the bootstrap is not converging is not answerable from counters: 690
  // samples with valid=false says only "no 8 of them agreed". Keep the raw
  // sample and how many windows were thrown out, so the shape of the
  // disagreement (constant / drifting / noise) is readable from the status.
  int64_t  last_sample_us = 0;
  uint32_t boot_fail = 0;

  // Drop the estimate, keep the evidence.
  //
  // A mid-run RECAL must not zero the counters: agree/disagree/rejected/
  // delta_max are the record of how the machine has been behaving, and wiping
  // them at every idle top-up means a reading of "disagree 0" says only "none
  // since the last top-up", which is a far weaker claim than it looks. Observed
  // exactly that -- a burst run reported agree=0 delta_max=0 because a RECAL
  // had landed near the end.
  void resetEstimate()
  {
    valid=false; boot_n=0; consec_reject=0; est_cam_us=0;
  }

  void reset()
  {
    valid=false; offset_us=0; boot_n=0;
    last_resid_us=0; max_resid_us=0;
    agree=disagree=learned=rejected=rebuilds=0; consec_reject=0;
    last_sample_us=0; boot_fail=0;
    fault_pending=false; est_cam_us=0; established=0;
    delta_max_us=0; delta_last_us=0;
    for(int i=0;i<DELTA_BUCKETS;i++) delta_hist[i]=0;
    miss_delta_last_us=0; miss_delta_max_us=0; last_gap_us=0;
    slope_ppb=0; slope_n=0;
  }

  // Establish the first offset from sync pulses. After that, every report
  // maintains it directly -- see gate().
  void observe(uint64_t cam_ts, uint64_t cam_us)
  {
    if(cam_ts==0 || cam_us==0) return;
    int64_t sample = (int64_t)cam_ts - (int64_t)cam_us;
    learned++;
    last_sample_us = sample;

    if(valid)
    {
      // Diagnostic only -- this never corrects anything. Divided by the age of
      // the measurement it is the drift rate, which is what sizes the window.
      last_resid_us = sample - offset_us;
      if(llabs(last_resid_us) > llabs(max_resid_us)) max_resid_us = last_resid_us;
    }

    if(boot_n < BOOT_N) boot[boot_n++] = sample;
    if(boot_n < BOOT_N) return;

    // Median, then require a real majority around it. Disagreement means the
    // samples are not measuring one constant -- better to keep the previous
    // measurement (or stay unconverged) and say so than to publish an average
    // of unrelated numbers.
    int64_t srt[BOOT_N];
    for(int i=0;i<BOOT_N;i++) srt[i]=boot[i];
    for(int i=1;i<BOOT_N;i++){int64_t k=srt[i];int j=i-1;while(j>=0&&srt[j]>k){srt[j+1]=srt[j];j--;}srt[j+1]=k;}
    int64_t med = srt[BOOT_N/2];
    int ok=0; for(int i=0;i<BOOT_N;i++) if(llabs(srt[i]-med)<=TOL_US) ok++;
    if(ok*2 > BOOT_N)
    {
      offset_us = med;                 // replace outright; do not blend
      est_cam_us = cam_us;
      valid = true;
      established++;
      boot_n = 0;
      consec_reject = 0;
    }
    else
    {
      boot_fail++;
      for(int i=0;i<BOOT_N/2;i++) boot[i]=boot[i+BOOT_N/2];
      boot_n=BOOT_N/2;
    }
  }

  // Every report, once the offset exists: take the nearest pending object, and
  // decide. Inside the window it is a match and the offset is re-measured from
  // it outright. Outside, twice running, the machine stops.
  //
  // This replaced an EWMA that folded each accepted sample in at 1/16. A
  // first-order filter following a ramp keeps a permanent lag proportional to
  // the drift rate, and the drift here is real: measured on the plate at
  // -35us/s (35ppm between the camera crystal and the ESP32's). The lag settled
  // at -3430us against a 5000us window -- 69% of the match margin spent on an
  // error that was constant, predictable, and entirely self-inflicted.
  //
  // Freezing the offset instead is no better: 35us/s crosses a 5000us window in
  // 143 seconds of uninterrupted running, so the machine would fault every
  // couple of minutes.
  //
  // Updating from every report removes the problem rather than managing it. The
  // offset is never more than one report old (~55ms at 18 parts/s), so the
  // drift it can accumulate is 55ms * 35us/s = 2us -- three orders of magnitude
  // inside the window, and it no longer matters how long the line runs.
  //
  // What makes taking the nearest object safe is that the window is far smaller
  // than the spacing between objects: 5ms against 55ms at 18 parts/s. A frame
  // cannot be within 5ms of two different objects, so a match inside the window
  // is the right object or there is no right object. That is also why this
  // cannot quietly lock onto a wrong offset the way blending could -- a wrong
  // offset puts frames outside the window, and outside the window it stops
  // instead of guessing.
  void gate(uint64_t cam_ts, uint64_t nearest_cam_us, int64_t nearest_delta)
  {
    if(!valid) return;
    if(nearest_delta > TOL_US)
    {
      rejected++;
      miss_delta_last_us = nearest_delta;
      if(nearest_delta > miss_delta_max_us) miss_delta_max_us = nearest_delta;
      if(++consec_reject >= LOST_N)
      {
        valid = false;
        boot_n = 0;
        consec_reject = 0;
        rebuilds++;            // kept as the "how often did this happen" counter
        fault_pending = true;
      }
      return;
    }
    consec_reject = 0;
    delta_last_us = nearest_delta;
    if(nearest_delta > delta_max_us) delta_max_us = nearest_delta;
    // Distribution, not just the high-water mark.
    //
    // delta_max over a four-minute run says nothing about the tail a machine
    // running all day will meet, and the tail is the whole question: the match
    // window has to sit above the worst residual that ever legitimately occurs,
    // or good parts get refused. A max cannot distinguish "one outlier at 240us
    // and everything else under 60" from "routinely near 240", and those two
    // want very different windows.
    //
    // Log2 buckets from 32us, so 8 counters cover 32us..4ms and the tail is
    // readable without shipping a histogram of hundreds of bins over 115200
    // baud. Bucket i holds [32<<i, 32<<(i+1)); bucket 0 also absorbs
    // everything below 32us, which is already under the ~50us noise floor.
    {
      uint32_t d = (uint32_t)(nearest_delta < 0 ? -nearest_delta : nearest_delta);
      int b = 0;
      while(b < DELTA_BUCKETS-1 && d >= (uint32_t)(32u << (b+1))) b++;
      delta_hist[b]++;
    }
    last_resid_us = (int64_t)cam_ts - (int64_t)nearest_cam_us - offset_us;
    if(llabs(last_resid_us) > llabs(max_resid_us)) max_resid_us = last_resid_us;
    last_gap_us = est_cam_us ? ((int64_t)nearest_cam_us - (int64_t)est_cam_us) : 0;

    // Learn the slope, but only from samples that already passed the window.
    //
    // That ordering is the whole safety argument: a sample far enough out to be
    // wrong is refused above and never reaches here, so the slope cannot be
    // taught by a mispaired frame. Feeding bad data into a slope is strictly
    // worse than feeding it into an offset -- an offset error is constant,
    // while a slope error grows without bound until the next measurement.
    //
    // Short gaps are skipped: resid/gap at 55ms is dominated by the ~1us
    // sample noise (1us/55ms = 18000ppb of pure noise), and the slope does not
    // need them -- it is constant, so the long gaps measure it far better.
    if(last_gap_us >= 1000000 && llabs(last_resid_us) <= TOL_US)
    {
      int64_t inst_ppb = (int64_t)last_resid_us * 1000000000LL / last_gap_us;
      // Two crystals cannot differ by more than a few hundred ppm. Anything
      // past this is not a clock, and clamping keeps one strange sample from
      // steering the prediction into the weeds.
      if(inst_ppb >  200000) inst_ppb =  200000;
      if(inst_ppb < -200000) inst_ppb = -200000;
      if(slope_n == 0) slope_ppb = (int32_t)inst_ppb;
      else             slope_ppb += (int32_t)((inst_ppb - slope_ppb) >> 3);
      slope_n++;
    }
    offset_us  = (int64_t)cam_ts - (int64_t)nearest_cam_us;   // measured, not blended
    est_cam_us = nearest_cam_us;
    established++;
  }

  // Where a frame taken at cam_ts should sit on the device clock.
  //
  // With drift compensation on, the offset is projected forward by the measured
  // slope over the time since it was measured -- so an idle line no longer
  // accumulates error at the full crystal rate, only at whatever the slope
  // estimate gets wrong.
  int64_t expectedCamUs(uint64_t cam_ts) const
  {
    int64_t off = offset_us;
    if(DRIFT_COMP && slope_n && est_cam_us)
    {
      int64_t elapsed_us = (int64_t)esp_timer_get_time() - (int64_t)est_cam_us;
      if(elapsed_us > 0)
        off += (int64_t)slope_ppb * elapsed_us / 1000000000LL;
    }
    return (int64_t)cam_ts - off;
  }

  static bool DRIFT_COMP;
};
int32_t CamClockSync::TOL_US = 5000;
bool    CamClockSync::DRIFT_COMP = false;   // opt-in, set_setup cam_drift_comp
CamClockSync CAM_SYNC;

// ---------------------------------------------------------------------------
// Camera trigger count <-> device pulse count
// ---------------------------------------------------------------------------
// The second pairing mechanism, and deliberately not a variation on the first.
//
// CamClockSync answers "which object was in front of the camera at time T",
// which is a question about two free-running crystals and therefore about
// drift, latency and a tolerance window. This answers "which object was the
// Nth trigger", which involves no clock at all: the board counts the pulses it
// puts on the CAM1 line, the camera counts the pulses it accepts on the same
// wire (ExtTriggerCount, written into the frame watermark and returned in the
// report as `pcnt`), and the difference between the two counters is a constant
// fixed at the camera's power-on.
//
// So the match is exact arithmetic -- cam_pcnt == pcnt - offset -- with no
// tolerance to size and nothing to re-measure. What it CANNOT do is survive a
// pulse the camera refuses: a refused trigger advances this board's counter
// and not the camera's, so the offset moves by one and every subsequent frame
// pairs to the wrong object. That is not a weakness to be papered over, it is
// the mechanism's whole diagnostic value -- a refusal is exactly the event a
// clock-only pairing absorbs silently (the frame simply is not there, the next
// one is a few ms off and still inside the window). Here it is a permanent,
// unmistakable step.
//
// Hence: learn the offset ONCE, from a calibration pulse fired with nothing
// else outstanding -- the same unambiguous sample CamClockSync bootstraps from
// -- and never quietly re-learn it. A sample that disagrees with the
// established offset is counted as a slip and left visible; if pulse-count
// matching is authoritative the machine then stops, which is the correct
// outcome for a sorter that no longer knows which part it is holding.
struct CamPulseSync
{
  bool     valid = false;
  int64_t  offset = 0;          // pcnt - cam_pcnt
  uint32_t learned = 0;         // unambiguous samples seen
  uint32_t slip = 0;            // samples that disagreed with the offset
  uint32_t hit = 0, miss = 0;   // reports this named / could not name an object
  // WHICH side failed, split out because `miss` alone cannot say -- and the
  // answer is the whole diagnosis. A halt on CAM_PAIRING_DISAGREE has three
  // distinct causes and they call for opposite responses:
  //
  //   ts_blind    the timestamp could not place the frame, the count could.
  //               The report arrived outside the match window -- latency, not
  //               pairing. Widen the window or make the report faster.
  //   pcnt_blind  the count could not, the timestamp could. A trigger the
  //               camera did not count, i.e. the refusal case.
  //   mismatch    both placed it, on DIFFERENT objects. The serious one:
  //               two independent mechanisms actively contradicting.
  //
  // The board already prints this at the halt, but a printed line has to be
  // caught as it goes past and the log ring has blocked that four times. A
  // counter is readable afterwards, which is when the question gets asked.
  uint32_t ts_blind = 0, pcnt_blind = 0, mismatch = 0;
  int64_t  last_sample = 0;     // most recent pcnt - cam_pcnt
  int64_t  last_pcnt = 0;

  void reset(){ valid=false; offset=0; learned=slip=hit=miss=0;
                ts_blind=pcnt_blind=mismatch=0;
                last_sample=0; last_pcnt=0; }

  void observe(int64_t pcnt, uint32_t cam_pcnt)
  {
    if(pcnt<0 || cam_pcnt==0) return;
    const int64_t s = pcnt - (int64_t)cam_pcnt;
    learned++;
    last_sample = s;
    if(!valid){ offset=s; valid=true; }
    else if(s!=offset) slip++;      // do not adopt -- see the note above
  }
};
CamPulseSync CAM_PCNT;

// The board's own count of camera trigger pulses, since boot.
//
// Incremented wherever this firmware drives the CAM1 line high, which is three
// places and has to be all three: the CAM stage in the ISR, calFireNow (the
// calibration pulse), and the trig_cam_* commands. The camera counts every
// edge it accepts regardless of which of them produced it, so a path that
// fired a pulse without counting it would shift the offset exactly as a
// refused trigger does -- and it would look identical in the counters.
static volatile uint32_t CAM_PULSE_N = 0;
// Pairing by pulse count, set_setup "report_match_pcnt". With
// report_match_ts also on, BOTH must name the same object or the machine
// stops; with only one on, that one decides. Off by default: the offset needs
// the frame watermark enabled on the host side (INSP_CAM_TRIG_WATERMARK), and
// without `pcnt` in the report this can never learn anything.
bool REPORT_MATCH_PCNT=false;

// Gate->report latency, updated by the report handler (main loop only),
// reported by get_running_stat, zeroed by reset_running_stat.
// Repeated reports for an object that already had a verdict. See the
// worst-wins block in the report handler: N counts them, DIFF counts the ones
// that disagreed with what was already there, WORSE counts the ones severe
// enough to replace it. DIFF>0 with WORSE==0 means later frames were kinder
// than earlier ones, which under last-wins would have released those parts.
uint32_t REP_REPEAT_N=0, REP_REPEAT_DIFF_N=0, REP_REPEAT_WORSE_N=0;
uint32_t REP_LAT_N=0;
uint64_t REP_LAT_SUM_US=0;
uint32_t REP_LAT_MAX_US=0;

// The SAME statistic measured from the camera trigger instead of from the gate.
//
// REP_LAT_* runs from trig_us, which is stamped when the gate SEES the part, so
// it contains the part's travel from the gate to the camera. Measured 924 / 479
// / 276 ms at plate 5000 / 10000 / 20000 -- it scales with the plate, because
// most of it is the part walking. That number was compared against the
// CAM->SWITCH budget twice on 2026-08-09, and both comparisons were wrong,
// because the budget starts where the camera fires and this starts where the
// gate fires.
//
// cam_us has been carried at full 64 bits all along and was never used for a
// latency figure. From it:
//   REP_CAMLAT   = camera trigger -> verdict processed  (the electronics)
//   REP_LAT - REP_CAMLAT = gate -> camera               (the travel)
// The second one is also the empirical answer to V-31: compare it against
// CAM1_on ticks and the 2x tick ambiguity resolves itself, instead of every
// budget staying [derived] forever.
uint32_t REP_CAMLAT_N=0;
uint64_t REP_CAMLAT_SUM_US=0;
uint32_t REP_CAMLAT_MAX_US=0;

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
// Step-ISR DURATION, the other half of the tick budget. See onTimer.
volatile uint32_t ISR_DUR_MAX_CY=0, ISR_DUR_LAST_CY=0, ISR_OVERRUN_N=0, ISR_DUR_N=0;
volatile uint64_t ISR_DUR_SUM_CY=0;
// The tick the step ISR must fit inside, in CPU cycles. Computed in the ramp
// service (main loop) because the ISR must not touch the FPU -- see onTimer.
volatile uint32_t ISR_BUDGET_CY=0;
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
// Every place a real part can vanish between the sensor edge and an object.
//
// rej_dist / rej_rate / rej_busy were counted; these four were not, and they
// sit EARLIER in the chain -- so `accept` could never be traced back to what
// the sensor actually saw. The width filter in particular drops parts in
// total silence, which is the worst way to lose them: nothing changes except
// the output.
//
// edges is the honest denominator: pulses that survived debounce. Everything
// else is attrition on the way to an object.
volatile uint32_t GATE_EDGES=0, GATE_REJ_WIDTH=0, GATE_REJ_UNSTABLE=0,
                  GATE_REJ_BLOCKED=0;
// One counter per reason, because merging them reverses cause and effect.
//
// These four used to share GATE_REJ_UNSTABLE, whose name and comment claimed
// "the plate is not at speed". A halt sets PLATE_FREQ_TARGET=0, so the plate
// then decelerates for ~16s at accel 2000 with the sensor still seeing parts,
// and every one of those was counted as "unstable" -- 375 of them in one
// measured halt. The counter therefore reads as the CAUSE of a stop when it is
// its AFTER-EFFECT, and on 2026-08-08 that produced a whole afternoon's wrong
// conclusion ("three soaks failed from plate-speed instability").
volatile uint32_t GATE_REJ_STEPPER_OFF=0,   // driver disabled
                  GATE_REJ_GATE_OFF=0,      // set_gate_disable, or a calibration
                  GATE_REJ_DRYRUN=0;        // stage clock without the plate
// The minimum travel between two parts, in microns. Was hardcoded 2000 inside
// newPulseEvent; settable so a measurement run can open it and see what the
// gate would otherwise have hidden.
volatile uint32_t GATE_MIN_DIST_um = 2000;


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
// SEL late, unrelated to tearing.
//
// It is harmless because config only changes during deliberate setup, and that
// property is now LOad-BEARING rather than incidental.
//
// It was briefly given up: STAGE_PULSE_WIDTH_apply() was moved into the ramp
// service so the derived *_off fields would follow the live speed, which made
// publishes continuous instead of rare. Paired with admitting parts during a
// ramp, the machine hung on the first speed change -- twelve seconds of UART
// silence, cleared only by a DTR reset. Both were reverted the same day.
//
// So: readers are Run_ACTS and ActRegister_pipeLineInfo, both inside onTimer(),
// holding the pointer for a handful of instructions. With publishes confined to
// set_setup a reader cannot span two of them and meet its own buffer being
// rewritten. Anything that republishes while the pipeline is live puts that
// back in play, and the machine has already shown what that costs.
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

// Set when a SEL width came out wider than half the part spacing. A FLAG, not a
// message, because this function runs above djrl and printf() on this board
// writes straight to UART0 -- which is the protocol link. Raw text there is a
// stray byte to the device's parser: INIT_CHAR_ERROR, err 11, latched, machine
// stopped. That is not hypothetical; it is what happened when this warning was
// first written as a printf, ten minutes after the same failure was diagnosed
// and fixed elsewhere in this file. NOTHING may printf here.
bool STAGE_WIDTH_SEL_WARN = false;

// All zero = "no width configured", i.e. the *_off offsets above stay in charge.
// Deliberately not seeded with the tick-derived equivalents: a machine that has
// never been told a width in microseconds must behave EXACTLY as it did before
// this existed, and silently converting its stored offsets would be a behaviour
// change disguised as a default.
stagePulseWidthUs STAGE_PULSE_WIDTH_US = {0,0,0,0,0,0,0};

// All zero = "the offsets are window STARTS", i.e. today's forward-only shape.
// Opt-in per station, for the same reason as the widths above: reinterpreting a
// stored offset as a centre would move every deployed blow earlier by half its
// width -- 750 ticks on this machine -- which is a behaviour change wearing a
// feature's clothes.
stagePulseCenter STAGE_PULSE_CENTER = {0,0,0,0,0,0,0};

// How far the plate may drift from its setpoint before the machine stops
// inspecting, as a percentage.
//
// The station windows are converted ONCE, for the setpoint, and a tick is a
// fixed distance -- so a window is a fixed arc and the TIME it takes scales
// with 1/speed. Inside this band a 50ms blow is 50ms +/- this percentage, and
// that bound is the whole safety argument, because what dislodges a
// neighbouring part is impulse and impulse follows dwell.
//
// Two kinds of speed change, one rule:
//
//   small   the closed-loop controller trimming the rate, or an operator
//           nudging it. Stays inside the band, so the arcs are still right to
//           within the bound and the line keeps running. This is the case that
//           matters: a controller that adjusts constantly would otherwise stop
//           production every time it moved.
//   large   a deliberate change of operating point. Leaves the band, so the
//           gate shuts, the plate ramps, the windows are re-derived once for
//           the new setpoint, and inspection resumes when it arrives.
//
// This replaced continuously re-deriving the windows from the ramp service.
// That tracked the speed exactly but turned STAGE_PULSE_OFFSET_publish() from
// a rare event into a constant one, and the machine hung the first time parts
// were admitted while it was happening (2026-08-11). Bounding the error is
// cheaper than tracking it, and it removes the interaction rather than
// narrowing it.
uint32_t SPEED_BAND_PCT = 10;

// True when the plate is close enough to its setpoint for the derived windows
// to still mean what they say.
static inline bool plateInSpeedBand()
{
  const float sp = PLATE_FREQ_SETPOINT;
  if(!(sp > 0.0f)) return false;
  const float tol = sp * (float)SPEED_BAND_PCT * 0.01f;
  const float dc = PLATE_FREQ_CURRENT - sp;
  const float dt = PLATE_FREQ_TARGET  - sp;
  // BOTH, not just current: heading out of the band is as disqualifying as
  // being out of it, or a large change would keep admitting parts for the
  // whole first tenth of its ramp.
  return (dc <= tol && dc >= -tol) && (dt <= tol && dt >= -tol);
}

// A configured centre sat closer to the gate than half its own window, so the
// leading edge was clamped to 0 instead of wrapping a uint32_t. Reported the
// same way the SEL width warning is: said out loud, not silently obeyed.
bool STAGE_CENTER_CLAMP_WARN = false;

// Turn the microsecond widths into tick offsets and publish them.
//
// Done here, in the main loop, rather than in the ISR: the step ISR keeps
// reading exactly one thing (SPO_active) and nothing about its timing changes.
// This just writes the *_off fields it was going to read anyway.
//
// Rounds UP, and converts against PLATE_FREQ_SETPOINT rather than the current
// speed, because the two error directions are not symmetric:
//
//   too short -> the camera misses the trigger, or the light is out during
//                exposure, or the part is not blown off. Every one of those
//                loses a part or a verdict.
//   too long  -> more LED duty, more air. Nothing is lost.
//
// So during SPINUP, when CURRENT is below SETPOINT, the pulse comes out longer
// than asked for -- which is the safe side. (SETPOINT / TARGET / CURRENT are
// three different variables here; using the wrong one is a mistake this
// codebase has already made once, in the host panel.)
// pf is the speed the pulse will actually be traversed at, NOT the setpoint.
//
// It used to be PLATE_FREQ_SETPOINT, converted once per set_setup, which makes
// every window a fixed ARC -- and an arc is a fixed distance, so the TIME it
// takes scales with 1/speed. That is invisible while speed only ever changes
// through set_setup (which re-runs this), and wrong the moment anything else
// moves the plate: at half speed the blow lasts twice as long.
//
// "Twice as long" is not the benign direction the width comment above assumes.
// It is benign for the camera and the light -- more exposure, more LED duty.
// It is not benign for a nozzle: what dislodges a neighbouring part is
// IMPULSE, force times dwell, and dwell scales with 1/speed. Dislodging is a
// threshold, not a proportion, so a neighbour that sits still at full speed
// can be ejected at reduced speed. A blow has to be a fixed TIME.
//
// The answer is NOT to make the arc follow the speed. That was tried, from the
// ramp service, and it worked -- SEL1 held 50.0ms against an asked 50.0 at
// 9750, 10500 and 10750 -- but it turned STAGE_PULSE_OFFSET_publish() from a
// rare event into a continuous one, and the machine hung the first time parts
// were admitted while it was running.
//
// Instead the error is BOUNDED and the arc left alone: converted once, here,
// for the speed the plate is being sent to, while SPEED_BAND_PCT keeps the
// plate near it whenever parts are moving. Inside a 10% band a 50ms blow is
// 50ms +/-10%, which is under any impulse threshold worth worrying about, and
// nothing republishes a shared snapshot while the pipeline is live.
//
// A large speed change leaves the band, so the gate shuts, this runs once for
// the new setpoint, and inspection resumes at the new operating point.
void STAGE_PULSE_WIDTH_apply(float pf)
{
  if(!(pf > 0)) return;                 // no speed, no conversion
  // ticks = us * (2*pf) / 1e6, rounded up, never zero.
  auto us2t = [pf](uint32_t us) -> uint32_t {
    if(us == 0) return 0;
    double t = ((double)us * 2.0 * (double)pf) / 1000000.0;
    uint32_t r = (uint32_t)(t + 0.999999);
    return r ? r : 1;
  };

  // The SEL blow is the one width that must ALSO respect distance: a fixed time
  // covers more plate as speed rises, and a blow wide enough to still be open
  // when the next part arrives ejects two. Cap at half the admission spacing --
  // the same rule the match window follows, for the same reason.
  uint32_t sel_cap = us2t(SYS_MIN_PULSE_TIME_SEP_us / 2);

  struct { uint32_t us; uint32_t ctr; uint32_t *on; uint32_t *off; bool is_sel; } M[] = {
    { STAGE_PULSE_WIDTH_US.CAM1, STAGE_PULSE_CENTER.CAM1, &STAGE_PULSE_OFFSET.CAM1_on, &STAGE_PULSE_OFFSET.CAM1_off, false },
    { STAGE_PULSE_WIDTH_US.L1A,  STAGE_PULSE_CENTER.L1A,  &STAGE_PULSE_OFFSET.L1A_on,  &STAGE_PULSE_OFFSET.L1A_off,  false },
    { STAGE_PULSE_WIDTH_US.CAM2, STAGE_PULSE_CENTER.CAM2, &STAGE_PULSE_OFFSET.CAM2_on, &STAGE_PULSE_OFFSET.CAM2_off, false },
    { STAGE_PULSE_WIDTH_US.L2A,  STAGE_PULSE_CENTER.L2A,  &STAGE_PULSE_OFFSET.L2A_on,  &STAGE_PULSE_OFFSET.L2A_off,  false },
    { STAGE_PULSE_WIDTH_US.SEL1, STAGE_PULSE_CENTER.SEL1, &STAGE_PULSE_OFFSET.SEL1_on, &STAGE_PULSE_OFFSET.SEL1_off, true  },
    { STAGE_PULSE_WIDTH_US.SEL2, STAGE_PULSE_CENTER.SEL2, &STAGE_PULSE_OFFSET.SEL2_on, &STAGE_PULSE_OFFSET.SEL2_off, true  },
    { STAGE_PULSE_WIDTH_US.SEL3, STAGE_PULSE_CENTER.SEL3, &STAGE_PULSE_OFFSET.SEL3_on, &STAGE_PULSE_OFFSET.SEL3_off, true  },
  };
  for(auto &m : M)
  {
    if(m.us == 0) continue;             // not configured -> leave *_off alone
    uint32_t t = us2t(m.us);
    // WARN, do not clamp.
    //
    // A blow wider than half the admission spacing is still open when the next
    // part arrives, which can eject two. But clamping it would silently shorten
    // a blow someone tuned by watching parts actually leave the plate -- and
    // the machine already runs that way: the shipped 1500-tick SEL width is
    // 75ms at pf 10000 against a 28571us spacing, i.e. well over the "cap"
    // and evidently fine in practice. Quietly halving it on the first day
    // someone sets a width in microseconds would be a behaviour change wearing
    // a bug fix's clothes. Say it and let the operator decide.
    if(m.is_sel && sel_cap && t > sel_cap)
      STAGE_WIDTH_SEL_WARN = true;
    // A centre, when one is configured, replaces the position rather than
    // adjusting it: BOTH edges are derived, so *_on becomes as derived as
    // *_off already is and nothing accumulates across repeated applies.
    //
    // off is on+t rather than centre+half so the window is exactly t ticks
    // wide whichever way the halving rounds; the centre is then honoured to
    // within one tick, which is 0.0126mm of plate.
    if(m.ctr)
    {
      const uint32_t half = t/2;
      // A centre closer to the gate than half the window would underflow a
      // uint32_t into ~4 billion ticks -- a pulse that never fires, from a
      // number that looks merely small. Clamp and say so.
      if(m.ctr > half) *m.on = m.ctr - half;
      else             { *m.on = 0; STAGE_CENTER_CLAMP_WARN = true; }
    }
    *m.off = *m.on + t;
  }
  STAGE_PULSE_OFFSET_publish();
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
  // SWITCH first: it is the stage that PUSHES into ACT_SEL1/ACT_SEL2, so
  // clearing it last left a window where a tick between the SEL clears and
  // the SWITCH clear queued a fresh blow into a queue that had just been
  // emptied -- an actuation surviving the flush and firing later at an
  // arbitrary plate position with no part behind it.
  act_S.ACT_SWITCH.clear();
  act_S.ACT_CAM1.clear();
  act_S.ACT_CAM2.clear();
  act_S.ACT_L1A.clear();
  act_S.ACT_L2A.clear();
  act_S.ACT_SEL1.clear();
  act_S.ACT_SEL2.clear();
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
  // Calibration object or real part. Without it a host cannot tell the two
  // apart on the wire, so every count it keeps is contaminated by whatever
  // syncPulseService happened to fire -- bench B.5 read "fired=10 objects=16"
  // that way, with the six extras being the machine's own sync pulses.
  uint8_t  sync;
};
RingBuf_Static<struct ISRTrigInfo,32,uint8_t> ISRTrigQ;

// How deep this queue ever got, and how often it was found full.
//
// It is 32 entries and every object pushes TWO (CAM1 and CAM2), so there are
// about 16 objects of headroom between the ISR that fills it and the main loop
// that drains it one entry per iteration over the serial link. When it
// overflows the machine stops with INSP_CAM_TRIG_INFO_CANNOT_BE_SENT -- which
// has been appearing under churn at object rates nowhere near any limit, and
// was impossible to chase because NOTHING PUBLISHED THIS DEPTH. The `Qs` field
// on every cam_trig is RBuf, a different queue with a different size, and the
// bench check that reads it has been reporting "firmware queue stayed bounded"
// every run while looking at the queue that does not overflow.
//
// Written from the timer ISR, so: plain integers in DRAM, no allocation, no
// flash. Read from the main loop; a torn read costs a wrong diagnostic number,
// never behaviour.
extern volatile uint32_t LOOP_N;        // defined beside firmwareLoop()
extern volatile uint32_t LOOP_MAX_US;
extern volatile uint32_t SEG_SVC_US, SEG_ST_US, SEG_RX_US, SEG_TX_US;
volatile uint8_t  ISRTRIGQ_HWM = 0;    // deepest seen since the last reset
volatile uint32_t ISRTRIGQ_OVF = 0;    // pushes that found it full

// Pushes made inside ONE onTimer() call, worst case.
//
// The depth instrument said the queue is empty 146 samples out of 149 and yet
// its high-water mark is 19: the 19 arrives and clears between samples. Two
// shapes fit that and they need opposite fixes -- either one ISR call pushes
// a clump (objects reaching the camera stage on the same tick, a timing
// problem), or the pushes are spread over time and the drain was blocked for
// long enough to let them pile up (a scheduling problem). This counter tells
// them apart: a burst of 19 in one tick pins the first, a max of 2 pins the
// second. Everything else was ruled out by measurement first -- link
// saturation, single perturbations, ISR catch-up, the ring counter race.
volatile uint8_t  ISRTRIGQ_BURST = 0;  // most pushes in a single ISR call
volatile uint8_t  ISRTRIGQ_THIS = 0;   // pushes so far in the current call

// A witness for the moment the queue was deepest: which objects were in it.
//
// The depth alone said "28, in the first three seconds, never again". Knowing
// WHICH parts those were is the difference between reading the answer and
// inferring it. Recorded when a push sets a new high-water mark:
//   tid_new   the object being announced right then
//   tid_old   the oldest one still waiting -- so tid_new-tid_old is the span
//             of the batch that arrived together
//   gate_new/gate_old  their gate pulses: equal means they were admitted at
//             the same plate position, which is what a stopped plate does
//   step, state  where the machine was
volatile uint32_t HWM_TID_NEW=0, HWM_TID_OLD=0;
volatile uint32_t HWM_GATE_NEW=0, HWM_GATE_OLD=0;
volatile uint32_t HWM_STEP=0;
volatile int      HWM_STATE=0;
// How long the OLDEST entry had been waiting when the queue was at its
// deepest. This is the whole question: 0.4s means the drain stopped running,
// ~1ms means the batch arrived together and the drain never had a chance.
// The two have nothing in common except the symptom.
volatile uint32_t HWM_AGE_US=0;
// How late a CAM task was when it finally fired: cur_pulse - targetPulse, in
// plate ticks, worst case. Run_ACTS runs every tick and fires one task per
// stage per tick, so a task cannot fall behind by more than a tick or two --
// UNLESS its target was already in the past when it was queued. This number
// separates "the dispatcher fell behind" from "the task was born late", and
// arithmetic on the witness data already points at the second: the oldest
// entry at the peak was 7550 ticks (0.63s) past due.
volatile uint32_t ACT_LATE_MAX=0;
// Who it was. The stage queues are FIFO, so if targets are ever written out of
// order the head parks everything behind it and releases the lot at one per
// tick. LATE_PREV_TARGET is the target of the CAM1 task that fired just
// before: if it is GREATER than this one's, the queue is out of order and the
// question becomes how a later-registered object got an earlier target.
extern volatile uint32_t SYS_STEP_COUNT;   // defined below, beside the ISR
volatile uint32_t LATE_TID=0, LATE_GATE=0, LATE_TARGET=0, LATE_CUR=0;
volatile uint32_t LATE_QDEPTH=0, LATE_PREV_TARGET=0, LATE_LAST_TARGET=0;
// Which KIND of object it was. Two paths register objects -- the sensor/gate
// path and the calibration sync path -- and the inversion is almost exactly
// one CAM1_on, which is the kind of constant one path would carry and the
// other would not. w is the detected pulse width: 20 ticks is a phantom, a
// real part is hundreds.
volatile uint8_t  LATE_SYNC=0;
volatile uint32_t LATE_W=0;
volatile uint8_t  LATE_PREV_SYNC=0, LATE_LAST_SYNC=0;

// A trace of the last 24 CAM1 ON-task REGISTRATIONS, in push order.
//
// The inversion is created at push time, not at fire time, so looking at the
// queue's two ends can only prove it happened. This records every push --
// which object, its gate_pulse, the target written, and where the plate was
// at that instant -- and freezes the moment a task fires badly late, so the
// window around the event survives to be read out.
#define PUSHLOG_N 24
struct PushLogEnt { uint32_t tid, gate, target, at; };
PushLogEnt PUSHLOG[PUSHLOG_N];
volatile uint8_t  PUSHLOG_I=0;
volatile uint8_t  PUSHLOG_FROZEN=0;
volatile uint32_t PUSHLOG_SEEN=0;
static inline void pushLog(ACT_INFO *t)
{
  if(PUSHLOG_FROZEN || t==NULL) return;
  PushLogEnt &e = PUSHLOG[PUSHLOG_I];
  e.tid    = t->src->tid;
  e.gate   = t->src->gate_pulse;
  e.target = t->targetPulse;
  e.at     = SYS_STEP_COUNT;
  PUSHLOG_I = (uint8_t)((PUSHLOG_I+1) % PUSHLOG_N);
  PUSHLOG_SEEN++;
}
extern volatile uint32_t SYS_STEP_COUNT;   // defined below, beside the ISR

static inline void hwmWitness(uint32_t tid, uint32_t gate_pulse)
{
  HWM_TID_NEW=tid;  HWM_GATE_NEW=gate_pulse;
  ISRTrigInfo *oldest = ISRTrigQ.size() ? ISRTrigQ.getTail() : NULL;
  HWM_TID_OLD  = oldest ? oldest->trig_id    : 0;
  HWM_GATE_OLD = oldest ? oldest->gate_pulse : 0;
  HWM_AGE_US   = oldest ? ((uint32_t)esp_timer_get_time() - oldest->trig_time_us) : 0;
  HWM_STEP     = SYS_STEP_COUNT;
  HWM_STATE    = (int)sysinfo.state;
}

static inline void isrTrigQMark(uint32_t tid=0, uint32_t gate_pulse=0)
{
  uint8_t d = ISRTrigQ.size();
  if(d > ISRTRIGQ_HWM) { ISRTRIGQ_HWM = d; hwmWitness(tid, gate_pulse); }
  if(++ISRTRIGQ_THIS > ISRTRIGQ_BURST) ISRTRIGQ_BURST = ISRTRIGQ_THIS;
}

// The calibration path pushes from the MAIN LOOP (syncPulseService), not from
// onTimer. Counting those into the per-ISR-call counter made it read 7 during
// CAL while the queue itself never went above 1: nothing resets the counter
// between main-loop pushes, so they simply accumulate until the next tick.
// Depth still counts -- same queue -- but the burst figure has to stay
// ISR-only or it answers a question nobody asked.
static inline void calTrigQMark(uint32_t tid=0, uint32_t gate_pulse=0)
{
  uint8_t d = ISRTrigQ.size();
  if(d > ISRTRIGQ_HWM) { ISRTRIGQ_HWM = d; hwmWitness(tid, gate_pulse); }
}


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


// The calibration phase is documented where it is implemented, far below; the
// state machine that drives it lives up here.
static void calibrationBegin(bool full);
void STAGE_PULSE_WIDTH_apply(float pf);
// One speed, converted once: the speed the plate is being SENT to.
//
// Both stations use it, so the early/late split is gone. That split only
// existed to make live tracking safe, and live tracking is gone -- see
// SPEED_BAND_PCT for what replaced it.
static inline float stageWidthRefFreq()
{
  return (PLATE_FREQ_SETPOINT > 0.0f) ? PLATE_FREQ_SETPOINT : PLATE_FREQ_CURRENT;
}
static void calibrationCleanup();
static void spinupBegin();
// Verdict trace: the last N (object, verdict) pairs, in application order.
// Read back with get_verdict_log. Kept out of get_running_stat deliberately --
// that response is already close to its buffer and silently overflowing it once
// cost an afternoon.
struct VerdRec { uint32_t tid; int32_t cat; };
static const int VERD_LOG_N = 64;
static VerdRec   VERD_LOG[VERD_LOG_N];
static uint16_t  VERD_W = 0;
static uint16_t  VERD_N = 0;

static bool CAL_GATE_PREV = false;   // GATE_DISABLED before calibration shut it
// RECAL has been asked for, but the old estimate is still in use until the
// pipeline empties. See calibrationBegin.
static bool CAL_RESET_PENDING = false;
// Recals abandoned because the machine never emptied within the phase timeout.
// Harmless -- the previous offset is untouched -- but a rising count means the
// top-up is not actually happening.
static uint32_t CAL_RESET_SKIPPED = 0;

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
        // Outputs first, then the queues -- and both, which IDLE did not do.
        //
        // A SEL blow is an ON task and an OFF task in the same queue. Clearing
        // between them discards the OFF and leaves the valve energised, and
        // IDLE's loop body then drives the plate at the setpoint, so unless the
        // stop sequence also wrote plate_freq:0 the air stays on indefinitely
        // with no error. exit_insp_mode landing inside a ~50ms blow is not a
        // corner case at 39/s with 10% rejects.
        //
        // ERROR entry has the same pair in the opposite order (safe then
        // clear), which leaves its own window: the ISR can fire a pending SEL
        // ON between them and have its OFF cleared underneath it. Fixed there
        // too.
        ALL_OUTPUTS_SAFE();
        RESET_ALL_PIPELINE_QUEUE();
        ALL_OUTPUTS_SAFE();
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

    case SYS_STATE::INSPECTION_MODE_CAL:
    {
      if (i == 0)//enter
      {
        // Objects must flow through the pipeline (the phantoms are objects),
        // but nothing real may enter: calibrationBegin shuts the gate and holds
        // the feeder off, so the only things in flight are our own pulses.
        //
        // This used to CLEAR the block, which said the opposite of the comment
        // -- it was the only way calFireNow would fire, since that checked the
        // same flag. calFireNow no longer does, so the sensor can stay blocked
        // for real.
        blockNewDetectedObject=true;
        calibrationBegin(true);
      }
      else if (i == 1)
      {
        // Held still. calFireNow drives the camera directly, so calibration no
        // longer needs the plate turning -- and measuring a clock is no reason
        // to move a machine.
        PLATE_FREQ_TARGET=0;
      } //loop
      else
      {
        calibrationCleanup();
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_RECAL:
    {
      // Re-measure without stopping the line.
      //
      // The offset is only maintained while parts are being reported, so an
      // idle line lets it go stale at the crystal drift rate. This is the
      // scheduled top-up, and unlike CAL it must not touch the plate: CAL holds
      // it at zero because at startup nothing has spun up yet, but here it is
      // already at speed and correct. Spinning down and back up would cost
      // ~7.5s each way at accel 2000 -- around 19s of downtime to fix an error
      // of 0.038mm, which is the wrong trade by a wide margin.
      //
      // calFireNow does not care whether the plate turns, so keeping speed
      // costs nothing. Shut the gate, let the pipeline drain (syncPulseService
      // will not fire while anything is outstanding), take fresh samples,
      // reopen.
      if (i == 0)//enter
      {
        blockNewDetectedObject=true;   // sensor shut; calFireNow is unaffected
        calibrationBegin(false);
      }
      else if (i == 1)
      {
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;   // stay at speed
      } //loop
      else
      {
        calibrationCleanup();
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_SPINUP:
    {
      // Reaching speed is its own phase.
      //
      // The gate has always required SYS_FREQ_STABLE, so parts could never be
      // registered while the plate was still ramping. That protection was real
      // but invisible: from outside, a machine accelerating with the feeder off
      // and a machine that had simply stopped detecting looked identical. As a
      // state it is reportable, and the sequence IDLE -> CAL -> SPINUP -> READY
      // says plainly what the machine is doing before it starts judging parts.
      // The decision itself is in spinupService(), beside syncPulseService():
      // both need djrl and SYS_STATE_Transfer, which are defined below this
      // state machine.
      if (i == 0)//enter
      {
        blockNewDetectedObject=true;
        // Start the ramp on entry rather than waiting for the first loop pass,
        // so the state's own service call cannot observe a pre-ramp plate.
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;
        spinupBegin();
      }
      else if (i == 1)
      {
        PLATE_FREQ_TARGET=PLATE_FREQ_SETPOINT;
      } //loop
      else
      {
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_READY:
    {
      if (i == 0)//enter
      {
        blockNewDetectedObject=false;
        FEEDER_ON=true;
        io_drive(FEEDER_PIN, IOI_FEEDER, true);
        // The unanswered budget belongs to the run, not to the boot. It is
        // cleared by a judged part and by reset_running_stat, and by nothing
        // else -- so a run that ended at 8 of stop_after 10 left the next one
        // halting after TWO unjudged parts, which reads as "it keeps stopping
        // right after restart" and points nowhere near the cause.
        CONSEC_UNANSWERED=0;

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

        RESET_ALL_PIPELINE_QUEUE();
        // Again, after the queues are empty. Safe-then-clear leaves a window:
        // the ISR can execute a pending SEL ON between the two, and its OFF is
        // then thrown away by the clear -- valve energised with nothing left to
        // release it. The double RESET narrows that but cannot close it, since
        // the tick can land in the first one. Dropping the outputs once more at
        // the end is what actually closes it, and it costs nothing.
        ALL_OUTPUTS_SAFE();

        // targetPulse=get_Stepper_pulse_count()+perRevPulseCount/3;//in jail for a bit
        ERROR_LOG_PUSH((GEN_ERROR_CODE)sysinfo.extra_code);
      } //enter
      else if (i == 1)
      {
        
        // if(diff>0)//times up
        // {
        //   SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);
        // }
      }
      else
      {
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
// When a REAL part was last registered. Sync pulses must stay out of the way
// of production -- see syncPulseService.
static int64_t REAL_ACCEPT_MS = 0;

int newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  static uint32_t tid_counter=1;
  uint32_t _prePulse_BK=_prePulse;
  _prePulse=middle_pulse;
  // 2mm, not 3.5mm: parts are specified 3mm apart, and with the plate geometry
  // finally correct a 3.5mm gate would reject conforming production parts.
  if(GATE_MIN_DIST_um &&
     middle_pulse-_prePulse_BK<(_PLAT_DIST_step(GATE_MIN_DIST_um))){GATE_REJ_DIST++;return -9;}
  uint64_t curTime = esp_timer_get_time();
  // The fire-rate limit. Rejecting here is the cheapest possible outcome: the
  // object never gets a tid, never gets a camera trigger and never gets a
  // SWITCH task, so it simply recirculates for another pass. Letting it through
  // instead would ask the camera for a frame it cannot deliver, and a trigger
  // with no frame poisons the host's pairing (see CORE0_1_CAVEATS J7/J9).
  if(curTime-_preTime<GATE_SEP_EFF_us){GATE_REJ_RATE++;return -8;}
  _preTime=curTime;


  if(blockNewDetectedObject){ GATE_REJ_BLOCKED++; return -1; }
  pipeLineInfo *head = RBuf.getHead();
  if (head == NULL)
  {
    GATE_REJ_BUSY++;
    return -1;
  }

  //get a new object and find a space to log it
  head->w = pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status = insp_status_UNSET;
  head->tid=tid_counter;
  head->trig_us=(uint32_t)esp_timer_get_time();
  // Clear the previous occupant's camera time. RBuf is a ring, so without this
  // an object whose CAM stage has not fired yet still carries a plausible-
  // looking cam_us from whoever held the slot last -- and the timestamp matcher
  // only skips zeroes, so it would happily match a frame against it.
  head->cam_us = 0;
  head->cam_pcnt = 0;           // same ring-reuse hazard as cam_us
  // Same reason cam_us is cleared: RBuf is a ring, so without this a recycled
  // slot arrives already retired and the drain frees it before it has lived.
  head->retired = 0;
  head->sync = SYNC_MARK_NEXT;
  if(SYNC_MARK_NEXT==0) REAL_ACCEPT_MS = (int64_t)(esp_timer_get_time()/1000);
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

    // One coherent snapshot for this object's registration (see SPO_active).
    volatile stagePulseOffset* spo = SPO_active;
    ACT_PUSH_TASK(act_S.ACT_L1A, pli, spo->L1A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L1A, pli, spo->L1A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, spo->CAM1_on, 1, pushLog(_task_););
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, spo->CAM1_off, 0, );


    ACT_PUSH_TASK(act_S.ACT_L2A, pli, spo->L2A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L2A, pli, spo->L2A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, spo->CAM2_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, spo->CAM2_off, 0, );

    ACT_PUSH_TASK(act_S.ACT_SWITCH, pli,spo->SWITCH, 0, );
    return 0;
  }
  return -1;
}





int Run_ACTS(uint32_t cur_pulse)
{
  bool time_us_fetched=false;
  uint64_t time_us=0;
  struct ACT_SCH *acts= &act_S;

  // if(diff!=1)
  // {
  // }

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
                    // Count the edge, and stamp the object with the count --
                    // outside the ISRTrigQ block on purpose. The announcement
                    // can be suppressed (trig_report) or the queue can
                    // overflow; the pulse still went out and the camera still
                    // counted it, so this must not be conditional on either.
                    task->src->cam_pcnt = ++CAM_PULSE_N;
                    {
                      uint32_t late = cur_pulse - task->targetPulse;
                      if(late < 0x80000000u && late > ACT_LATE_MAX)
                      {
                        ACT_LATE_MAX  = late;
                        LATE_TID      = task->src->tid;
                        LATE_GATE     = task->src->gate_pulse;
                        LATE_TARGET   = task->targetPulse;
                        LATE_CUR      = cur_pulse;
                        LATE_QDEPTH   = acts->ACT_CAM1.size();
                        LATE_PREV_TARGET = LATE_LAST_TARGET;
                        LATE_SYNC     = task->src->sync;
                        LATE_W        = task->src->w;
                        LATE_PREV_SYNC= LATE_LAST_SYNC;
                        if(late>1000) PUSHLOG_FROZEN=1;   // keep the window
                      }
                      LATE_LAST_TARGET = task->targetPulse;
                      LATE_LAST_SYNC   = task->src->sync;
                    }
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
                      commInfo->sync=task->src->sync;
                      ISRTrigQ.pushHead();
                      isrTrigQMark(task->src->tid, task->src->gate_pulse);
                      // Keep it on the object too. Until now this timestamp was
                      // announced and then forgotten, which is why the host had
                      // to reconstruct the frame<->object mapping from clocks it
                      // could only observe indirectly.
                      task->src->cam_us = time_us;
                    }
                    else
                    {
                      ISRTRIGQ_OVF++;
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
                      commInfo->sync=task->src->sync;
                      ISRTrigQ.pushHead();
                      isrTrigQMark(task->src->tid, task->src->gate_pulse);
                    }
                    else
                    {
                      ISRTRIGQ_OVF++;
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


      pipeLineInfo *pli = task->src;

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
          // SEL3 has no actuator. These were SEL2's tasks copy-pasted, which
          // would have ejected every OK part into the NG chute; left commented
          // rather than deleted, which is why cat=3 is documented as a trap.
          break;
        case 0xFFFF:
          CONSEC_UNANSWERED=0;
          autoRateOk();   // a part was judged
          NA_Count++;
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
          // Policy 0 means do not stop -- for THIS arm too.
          //
          // The comment above says SKIP and UNSET "escalate together", and the
          // SKIP arm has an explicit `if(UNANSWERED_POLICY!=1) break;`. This one
          // did not, so it fell straight through and faulted on the FIRST
          // unjudged part no matter what the policy said. skip_policy mode
          // "none" therefore did not mean none: a 5-hour soak ran 51,161 objects
          // at 36.5/s with zero disagreements and stopped on object 51,162.
          //
          // Not faulting is the safe outcome, not the lenient one: an unjudged
          // part is never actuated, so it recirculates and gets another pass.
          // Production runs policy 1 (slow_and_stop) and is unchanged -- it
          // still stops after UNANSWERED_STOP_AFTER consecutive.
          if(UNANSWERED_POLICY!=1) break;
          ecode=GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT;
          ERR_CTX_TID=pli->tid;
          ERR_CTX_STATUS=pli->insp_status;
          ERR_CTX_GATE_PULSE=pli->gate_pulse;
          ERR_CTX_CUR_PULSE=cur_pulse;
          break;
      }
      //
      
      {
        task->src->insp_status = insp_status_DEL;
        task->src->retired     = 1;   // the drain keys off this, not the status
        task->src = NULL;
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
// The plate speed the machine is ACTUALLY running at, differentiated from the
// step counter in the main loop.
//
// Everything that reported "plate_freq" reported PLATE_FREQ_TARGET -- the
// commanded value -- and line 3688 even carries the measured one commented out
// beside it. So a host that wrote plate_freq:0 and read plate_freq:0 had
// confirmed the COMMAND, not the plate: a halt sets TARGET=0 while the plate
// keeps turning for ~16s at accel 2000. Every "the plate is stopped" line any
// tool has ever printed, including this session's, was that.
//
// Differentiating SYS_STEP_COUNT is the only measurement available -- there is
// no encoder -- but it is a real one: the ISR advances it, so if the ISR has
// stopped this reads zero, which is exactly the case the setpoint hides.
volatile float PLATE_FREQ_MEAS=0.0f;


typedef struct GateInfo {
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint16_t debounce;
  uint8_t cur_Sense;


} GateInfo;




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
      GATE_EDGES++;
      if( !(diff>minWidth && diff<maxWidth) ) GATE_REJ_WIDTH++;
      else
      {
        uint32_t middle_pulse=gateInfo.start_pulse+(diff>>1);
        (void)middle_pulse;
        // Reference the object off its TRAILING edge (end_pulse), as the
        // pre-debounce code did, so the calibrated stage_pulse_offset values
        // still line up. (Switching to the object centre -- middle_pulse -- is
        // checklist 5.3's separate, calibration-affecting change.)
        // Changing speed used to stop the machine detecting.
        //
        // SYS_FREQ_STABLE is only "CURRENT == TARGET", so ANY speed change --
        // including a deliberate one on a running machine -- shut the gate
        // until the ramp finished, and every part that passed meanwhile was
        // lost. That is a nuisance with a manual speed knob and fatal to a
        // closed-loop one: an AIMD controller adjusts constantly, so the
        // machine would spend its life in the ramp with the gate shut.
        //
        // Admitting during a ramp is safe when the machine is ALREADY running
        // and heading somewhere, and the argument is about bounds rather than
        // about the ramp being harmless. Every speed the part will experience
        // lies between CURRENT and TARGET, so it is bounded by the larger of
        // the two -- and TARGET is the speed the operator is deliberately
        // running at, whose margins (SWITCH deadline against host latency,
        // blow width against part spacing) are the ones already being met.
        // Nothing on the way there is more demanding than the destination.
        //
        // The other half of the argument is that the station windows now
        // follow the live speed (STAGE_PULSE_WIDTH_apply, called from the ramp
        // service). Without that this would be wrong in the dangerous
        // direction: an arc derived at one speed and crossed at a higher one
        // is a blow that is too short and a light that is out during exposure.
        // This relaxation depends on that fix and must not outlive it.
        //
        // Three cases stay blocked, deliberately:
        //   TARGET == 0   stopping. A part admitted now never reaches a
        //                 station; it just sits on the plate.
        //   not READY     spin-up and CAL. Those are not "a running machine
        //                 changing speed", and their protection is untouched.
        //   the rest      stepper off / gate off / dry run, as before.
        // Keep running through a SMALL speed change; stop for a large one.
        //
        // Requiring SYS_FREQ_STABLE -- literally CURRENT == TARGET -- shut the
        // gate for any speed change at all, and every part that passed
        // meanwhile was lost. Tolerable with a manual knob, fatal to a
        // closed-loop one, which adjusts continuously and would spend its life
        // ramping with the gate shut.
        //
        // The first attempt at this admitted parts through ANY ramp, paired
        // with windows re-derived continuously to match. It hung the machine on
        // the first speed change (2026-08-11): twelve seconds of UART silence,
        // cleared only by a DTR reset. Admission had started crossing
        // STAGE_PULSE_OFFSET_publish() exactly when that publish became
        // constant, and ActRegister_pipeLineInfo reads the snapshot it swaps.
        //
        // So the windows are static again -- derived once, for the setpoint --
        // and this admits parts only while the plate is close enough to that
        // setpoint for them to still be right, within SPEED_BAND_PCT. Nothing
        // republishes while parts are moving, which is the property the hang
        // took away.
        // REVERTED TWICE. Do not re-enable without reading this.
        //
        // Attempt 1 admitted parts through any ramp, with the station windows
        // re-derived continuously from the ramp service to match. The machine
        // hung on the first speed change: twelve seconds of complete UART
        // silence, no boot banner, cleared only by a DTR reset. That was
        // blamed on STAGE_PULSE_OFFSET_publish() having become continuous
        // while parts crossed it.
        //
        // Attempt 2 removed the continuous publishing entirely -- windows
        // converted once for the setpoint, error bounded by SPEED_BAND_PCT
        // instead of tracked -- and admitted parts only inside that band. It
        // hung the same way. So the publishing hypothesis is DISPROVEN, and
        // what survives both attempts is the one thing they share: parts
        // entering the pipeline while CURRENT != TARGET.
        //
        // Current suspicion, untested: the ramp rewrites the step timer's
        // alarm period as it goes, and Run_ACTS executes inside that timer's
        // ISR. Until now a ramp always had an EMPTY task queue, because this
        // gate is what kept parts out -- so the expensive path through
        // Run_ACTS and a shrinking alarm period never coincided. An ISR that
        // outruns its own alarm starves the main loop, and a starved main loop
        // is exactly a machine that stops answering its UART without ever
        // rebooting.
        //
        // If that is right, the fix has nothing to do with pulse widths, and
        // testing it needs a way that does not involve wedging the production
        // machine a third time.
        const bool speed_ok = SYS_FREQ_STABLE;
        if(SYS_STEPPER_DISABLED==false && speed_ok && GATE_DISABLED==false && DRY_RUN==false)
          newPulseEvent(gateInfo.start_pulse,gateInfo.end_pulse,
                        gateInfo.end_pulse,diff);
        else
          // Not an error, but not free either: a spin-up, a CAL, or a stop
          // holds this closed, and the parts on the plate during it are simply
          // gone. Counted so that loss has a size.
          // Attributed, not lumped. Order is deliberate: the most specific
          // and most deliberate reasons first, so "unstable" keeps only the
          // meaning its name claims.
          if(SYS_STEPPER_DISABLED)   GATE_REJ_STEPPER_OFF++;
          else if(GATE_DISABLED)     GATE_REJ_GATE_OFF++;
          else if(DRY_RUN)           GATE_REJ_DRYRUN++;
          else                       GATE_REJ_UNSTABLE++;
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


// --- phantom emission: requested by the main loop, PERFORMED by the step ISR --
//
// newPulseEvent had two producers. The sensor path reaches it from the step ISR
// (GateSensing inside onTimer); the rig path reached it from the main loop
// (phantomEmitOne, from trig_phantom_pulse and the train service). Nothing
// serialised them, and everything they share was unprotected:
//
//   tid_counter        a static ++, so two objects could take the same tid --
//                      and byTid is AUTHORITATIVE in the report handler, so a
//                      duplicate lands a verdict on the wrong part. That is the
//                      one path that gets round the 2*TOL uniqueness invariant
//                      from the side rather than through it.
//   _prePulse/_preTime  the distance and rate gates' references, read-modify-write
//   RBuf.getHead()      does NOT reserve the slot. getHead -> write 8 fields ->
//                       pushHead is not atomic, so both producers could be handed
//                       the SAME slot, both write it, both push: the ring advances
//                       twice for one object and the second entry is whatever the
//                       previous lap left there.
//
// It has been survivable only by convention -- the phantom suites set
// set_gate_disable first, so the ISR does not call newPulseEvent while they run.
// Convention is not enough for what this is about to be used for: a phantom
// clock top-up during production makes a rare race a continuous one.
//
// Fixed by removing the second producer rather than by locking around it. The
// main loop only asks; the ISR emits, at most one per tick, and every piece of
// shared state above goes back to being touched from exactly one context. No
// critical section, so nothing here can delay a step pulse -- which locking
// would have done, on the very path whose timing everything else is measured
// against.
//
// It is also more correct: gate_pulse is a step count, and taking it inside the
// ISR means it cannot be read while SYS_STEP_COUNT is advancing underneath.
//
// Single-producer/single-consumer counters: the main loop only ever writes REQ,
// the ISR only ever writes DONE and DROP. Unsigned wraparound makes the
// comparison safe without a lock.
// Announce every camera trigger to the host, or not.
//
// The host used to NEED these: it paired frame<->object itself and would wait
// up to TRIG_WAIT_MAX_MS for a late announcement. Measured 2026-08-10 with the
// inspection removed so nothing else could be blamed, that wait WAS the entire
// remaining latency tail -- trig_wait max 86.15ms against insp_off max 86.35ms,
// with placement perfect (rx 2024, matched 2024, no_candidate 0). The cost was
// never mis-pairing; it was waiting for an uplink that arrives after the frame.
//
// With the host placing reports by cam_ts instead, these become pure uplink
// traffic: ~105 bytes each at 36.5/s is 3.8 KB/s of a 23 KB/s link, and every
// one of them is an ISR-queued event that can overflow into
// INSP_CAM_TRIG_INFO_CANNOT_BE_SENT.
//
// Runtime, not compile time, so turning it back on does not need a reflash --
// and the counter keeps a suppressed run from looking like a dead link.
static volatile bool     TRIG_REPORT_ON = true;
static volatile uint32_t TRIG_REPORT_SUPPRESSED = 0;

static volatile uint32_t PHANTOM_REQ_N  = 0;   // main loop writes
static volatile uint32_t PHANTOM_DONE_N = 0;   // ISR writes
static volatile uint32_t PHANTOM_DROP_N = 0;   // ISR writes: the gate refused it

// Virtual objects scheduled in the TICK domain, not against esp_timer.
//
// Every phantom source until now paced itself in time, and that is the one
// thing a real part never does: a part is registered at the plate position the
// sensor saw it (gate_pulse = SYS_STEP_COUNT), and camera, SWITCH and the
// ejectors are all reckoned in ticks from there. A time-paced train therefore
// exercises a path no real part takes -- its spacing is a duration, so it drifts
// against the plate the moment the speed moves, and every "spacing" conclusion
// drawn from it silently assumes time and position are interchangeable.
//
// Spacing here is a DISTANCE. It is exact regardless of plate speed and speed
// jitter, and the objects run the same tick arithmetic real parts do.
//
// jitter_ticks is not decoration. Perfectly even virtual spacing is the one
// case where a persistent off-by-N pairing can hide: every object sits at the
// same offset from its neighbour, so a slip equal to that period looks
// identical to a correct match. Real parts arrive irregularly and that
// irregularity is itself the anti-slip mechanism -- see the slip appendix in
// docs/MACHINE_FLOW.md. A virtual train that wants to stand in for real traffic
// has to reproduce it.
//
// Written by the main loop, read by the ISR. ISR-only state stays in the ISR.
static volatile uint32_t VIRT_PERIOD_TICKS = 0;   // 0 = off
static volatile uint32_t VIRT_JITTER_TICKS = 0;   // +/- this many, uniform
static volatile uint32_t VIRT_EMIT_N       = 0;   // ISR writes
static volatile uint32_t VIRT_DROP_N       = 0;   // ISR writes: gate refused it

static inline void phantomServiceISR()
{
  // A host request wins the tick; the virtual train takes the next one. Same
  // discipline as the sensor-vs-phantom ordering above, and it keeps the "at
  // most one newPulseEvent per tick" property that T-7 restored.
  if(PHANTOM_DONE_N != PHANTOM_REQ_N)
  {
    PHANTOM_DONE_N++;
    // Same shape a real detection presents: a narrow pulse centred on now. The
    // width is nominal -- the gate's width filter is what it has to satisfy.
    const uint32_t at = SYS_STEP_COUNT;
    if(newPulseEvent(at-10, at+10, at, 20) != 0) PHANTOM_DROP_N++;
    return;
  }

  static uint32_t next_tick = 0;
  static uint32_t rng = 0x9E3779B9u;

  const uint32_t period = VIRT_PERIOD_TICKS;
  if(period == 0)
  {
    // Disarm, so re-enabling starts a fresh interval instead of firing
    // immediately on a schedule left over from the previous run.
    next_tick = 0;
    return;
  }

  const uint32_t now = SYS_STEP_COUNT;
  // Unsigned difference: correct across the 32-bit wrap, unlike (now >= next).
  if(next_tick == 0 || (uint32_t)(now - next_tick) >= 0x80000000u) 
  {
    if(next_tick == 0) next_tick = now + period;   // first arming
    return;
  }

  // Width has to satisfy the gate that is actually configured, not a constant.
  //
  // This used to emit a fixed 20-tick pulse. Production runs with
  // pulse_min_width 120, so the gate refused EVERY virtual object as too
  // narrow -- which is the 88-97% rejection recorded on 2026-08-09 and read
  // there as "the injector's scheduling is wrong". The scheduling was fine; the
  // stimulus did not meet the filter. A test fixture that cannot pass the
  // machine's own entry test measures the entry test, not the machine.
  //
  // Aim at the middle of the configured window so the rig is not sitting on
  // either edge, and keep the pulse symmetric about `now` so the timestamp the
  // pairing sees is still the centre.
  // Aim at a physically plausible part (200 ticks ~ 1.3mm of plate travel) and
  // only move off it to clear the configured limits. Splitting the window
  // instead -- lo + (hi-lo)/4 -- makes the pulse track the CONFIG rather than
  // the part, so opening pulse_max_width to 100000 for a throughput run would
  // emit a pulse two thirds of a revolution wide.
  int w = 200;
  {
    const int lo = (minWidth > 0) ? minWidth : 1;
    const int hi = (maxWidth > lo) ? maxWidth : (lo * 4);
    if(w <= lo) w = lo + (lo / 4 ? lo / 4 : 1);
    if(w >= hi) w = hi - 1;
    if(w < 2)   w = 2;
  }
  const uint32_t at = now;
  if(newPulseEvent(at - w/2, at + w/2, at, w) != 0) VIRT_DROP_N++;
  else VIRT_EMIT_N++;

  uint32_t step = period;
  const uint32_t j = VIRT_JITTER_TICKS;
  if(j)
  {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    step = period + (rng % (2u*j + 1u)) - j;
    if((int32_t)step < 1) step = 1;
  }
  next_tick = now + step;
}


void IRAM_ATTR onTimer()
{


  // enable FPU
  // // Save FPU registers
  



  // Entry stamp, kept for the DURATION measurement at the bottom. The gap
  // high-water below wants the same read, so take it once.
  const uint32_t _isr_cc0 = XTHAL_GET_CCOUNT();
  {
    // Inter-tick gap high-water: field evidence of ISR jitter/stalls without
    // a scope. A gap over 1s is a timer stop/start seam, not jitter.
    static uint32_t last_cc=0;
    if(last_cc){
      uint32_t d=_isr_cc0-last_cc;
      if(d<240000000u && d>ISR_GAP_MAX_CY) ISR_GAP_MAX_CY=d;
    }
    last_cc=_isr_cc0;
  }

  ISRTRIGQ_THIS=0;      // per-call push counter; see ISRTRIGQ_BURST
  SYS_STEP_COUNT++;

  //Step adv
  StepGo();



  GateSensing();

  // After the sensor, so a real detection always wins the tick it arrived on
  // and an injected one takes the next. At most one per tick, the same
  // discipline ACT_TRY_RUN_TASK uses.
  phantomServiceISR();

  Run_ACTS(SYS_STEP_COUNT);

  // How much of its own tick this ISR just spent.
  //
  // ISR_GAP_MAX_CY above measures the gap BETWEEN ticks, which is the timer
  // doing its job; this measures the work INSIDE one, which is the thing that
  // can outgrow it. The two together are the budget: at plate_freq f the tick
  // is 1e6/(2f) us, so at 10500 there are 47.6us to fit in.
  //
  // Added to test one hypothesis. Admitting parts while the plate was ramping
  // hung the machine twice (2026-08-11), and the surviving suspicion is that a
  // ramp had never before coincided with a NON-EMPTY act queue -- the gate's
  // stability check is exactly what kept parts out -- so Run_ACTS's expensive
  // path and a shrinking alarm period never met. An ISR that outruns its alarm
  // starves the main loop, and a starved main loop is a board that stops
  // answering its UART without ever rebooting. Which is the symptom, twice.
  //
  // Measured rather than argued, and measured FIRST with the gate still
  // closed: if the steady-state ISR already fills most of its tick, the
  // hypothesis needs no reproduction.
  {
    const uint32_t d = XTHAL_GET_CCOUNT() - _isr_cc0;
    if(d < 240000000u)                      // ignore a counter wrap
    {
      ISR_DUR_LAST_CY = d;
      if(d > ISR_DUR_MAX_CY) ISR_DUR_MAX_CY = d;
      ISR_DUR_SUM_CY += d;
      ISR_DUR_N++;
      // INTEGER compare against a budget computed elsewhere.
      //
      // The first version of this read PLATE_FREQ_CURRENT (a float) and did
      // 240000000.0f/(2.0f*f) right here. The board went silent the instant
      // the plate was told to turn -- which is exactly when this ISR starts
      // running -- and stayed silent until a reset. Floating point in this ISR
      // needs the FPU registers saved and restored around it, or it corrupts
      // whatever the interrupted code had in them -- that is what the
      // "Restore FPU / and turn it back off" note at the bottom of onTimer is
      // about. Saving them is correct but costs cycles in an ISR that already
      // does not fit its tick, so the division lives in the ramp service and
      // this only compares two uint32s.
      const uint32_t b = ISR_BUDGET_CY;
      if(b && d >= b) ISR_OVERRUN_N++;
    }
  }

  //sensor detection
  //Try run task


  
  // Restore FPU
  // // and turn it back off
  // 

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
// Nominal pitch plus +-jitter, from a seeded LCG so a failing train replays.
static int32_t  PH_TRAIN_JITTER_US= 0;
static uint32_t PH_TRAIN_RNG      = 1;

// Where trig_phantom_pulse puts an object: one L1A offset back, plus enough
// plate distance that it has somewhere to travel from.
static inline void phantomEmitOne()
{
  // A phantom registers at the plate position it is fired at, exactly like a
  // part the sensor just saw.
  //
  // It used to back-date gate_pulse by a whole L1A_on
  // (SYS_STEP_COUNT - L1A_on + _PLAT_DIST_step(3000), = now-9076 here) so the
  // injected object reached the light and camera stages at once instead of
  // after a lap. That was worth it when calibration had no path of its own;
  // calFireNow drives the camera directly now and registers no stage tasks, so
  // nothing but the rig commands still call this.
  //
  // The back-date cost more than the wait it saved. ACT_* queues are FIFO in
  // registration order and ACT_TRY_RUN_TASK only looks at the tail, so a
  // phantom's near-immediate target and a real part's lap-away target in the
  // same queue are an inversion: one real part parks every phantom behind it
  // until its own target comes due, and the whole overdue batch then leaves at
  // one to two per tick and buries the 32-entry ISRTrigQ. That is the
  // INSP_CAM_TRIG_INFO_CANNOT_BE_SENT that chaos has been hitting for months
  // at object rates nowhere near any limit -- measured, UINSP_CAVEATS.
  //
  // Cost of the change: an injected object now takes CAM1_on/(2*plate_freq) to
  // announce, like everything else. The suites already wait for arrival rather
  // than a fixed delay, and objects pipeline, so a run pays one transit, not
  // one per pulse.
  // Ask; do not emit. See phantomServiceISR above for why the second producer
  // on newPulseEvent had to go rather than be locked around. The pulse lands on
  // the next timer tick -- 15.6us at plate_freq 32000, and quantised to a tick
  // rather than to whenever the main loop got here, which makes the train's
  // interval MORE exact than it was, not less.
  //
  // With the timer alarm off (PLATE_FREQ_CURRENT==0) the request simply waits.
  // Nothing is lost: a phantom's stage tasks are scheduled at future step
  // counts, so on a stationary plate it could never have reached its camera
  // either -- that is exactly why calFireNow exists. `ph_pend` in poll makes a
  // waiting request visible instead of leaving it to be discovered.
  PHANTOM_REQ_N++;
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

// Clock calibration is a phase, not a background activity.
//
// It used to be neither: sync pulses were fired opportunistically alongside
// real work, guarded by "has anything real happened in the last 3 seconds".
// That guard existed because the two compete -- a sync pulse takes a gate slot,
// a camera trigger and an inspection. Measured on the real plate: 690 of 715
// registered objects were sync pulses, 151 real parts were refused by the rate
// gate because the phantoms had spent the budget, and from the operator's side
// the machine "flashes on its own and ignores the parts on the plate".
//
// Blocking instead removes the competition rather than refereeing it. While
// calibrating, the gate is shut and the feeder is off, so there are no real
// objects at all -- which means every phantom pulse is the only thing in the
// pipeline BY CONSTRUCTION, not by a timing guard that can be wrong. Once the
// offset exists the phantoms stop for good; ordinary reports maintain it from
// then on (CamClockSync::gate), and if it is ever lost the machine stops and
// the operator re-enters inspection mode, which recalibrates.
//
// It is a real state (INSPECTION_MODE_CAL, 102) rather than a flag riding
// alongside READY, so it is one state machine instead of two: the host and the
// operator see "calibrating" instead of an unexplained pause before parts move,
// and IDLE -> CAL -> READY makes "running without an offset" unreachable rather
// than merely unlikely. A redeem from INSPECTION_MODE_ERROR goes back to CAL,
// not to READY -- if the clock is why the machine stopped, resuming on the same
// stale offset is the one thing that must not happen.
static int64_t  CAL_DEADLINE_MS = 0;
// Calibration objects get their own tid space so they are never confused with
// gate-registered parts, in a log or in the core.
//
// 0x40000000 and not 0xC0000000: the report handler parses tid as a signed int
// (`doc["tid"].is<int>()`), so anything above INT32_MAX fails that test, comes
// back as -1, matches no object and faults the machine with
// INSP_RESULT_MATCHES_NO_OBJECT. Measured exactly that on the first run.
static uint32_t CAL_TID_NEXT = 0x40000001;
// The ramp is deterministic arithmetic and always converges, so this only fires
// if the machine is not doing what it was told -- but a silent hang between
// calibration and running is exactly the kind of stall nobody would diagnose.
static int64_t  SPINUP_DEADLINE_MS = 0;
// Per-pulse deadline. A calibration round trip is one camera frame plus one
// inspection -- tens of ms on this machine -- so a second and a half is already
// far past "late" and firmly into "not coming".
static int64_t  CAL_PULSE_MS = 0;
static uint32_t CAL_PULSE_LOST = 0;
static const int64_t CAL_PULSE_TIMEOUT_MS = 1500;
// Idle before the offset is re-measured, set_setup "cam_recal_idle_ms".
// 0 disables. See recalService for where 10s comes from.
int32_t  CAM_RECAL_IDLE_MS = 10000;
uint32_t CAM_RECALS = 0;
// The match window expressed as what it is: a POSITION tolerance, in um.
// 0 = off, keep the explicit match_window_us. See setMachineSetup for the
// derivation and for why it rounds down against the SETPOINT.
//
// The budget has two claimants and only one of them is the camera. The other
// is clock drift between top-ups: 35 us/s measured, so CAM_RECAL_IDLE_MS of
// idle spends CAM_RECAL_IDLE_MS/1000 * 35 us of the window before a single
// frame has been late. At 10s that is 350us -- which fits inside 5000 and
// does not fit inside the 373us that 0.3mm buys at plate_freq 32000. Tightening
// the window without shortening the idle just moves the halt to the first
// part after every quiet spell.
int32_t  CAM_MATCH_TOL_um = 0;
static const int32_t CAM_DRIFT_US_PER_S = 35;   // measured, see CAVEATS O
static const int64_t SPINUP_TIMEOUT_MS = 30000;
static uint32_t CAL_RUNS = 0, CAL_FAILS = 0;
static int64_t  CAL_STARTED_MS = 0;
static uint32_t CAL_LAST_MS_TAKEN = 0;
// Generous: the pulse cadence is 300ms and the bootstrap needs 8 unambiguous
// samples, so a healthy machine finishes in ~3s. This is the "something is
// actually wrong" bound, not a performance target.
static const int64_t CAL_TIMEOUT_MS = 30000;
// Width of the calibration trigger pulse. Settable so this can be swept against
// a camera that turns out to want more, without a reflash per attempt:
// set_setup {"cal_pulse_us": N}. See calFireNow for why 100 was wrong.
int32_t CAL_PULSE_WIDTH_US = 600;

// Shut the gate and hold the feeder, then pulse until the offset exists.
// full=false keeps the diagnostic counters, which is what a mid-run top-up
// wants; the startup calibration zeroes everything because the run is new.
static void calibrationBegin(bool full)
{
  CAL_GATE_PREV = GATE_DISABLED;
  GATE_DISABLED = true;
  // Belt and braces on the feeder. READY's enter block is what turns it on, and
  // CAL always runs before READY, so it should already be off -- but a feeder
  // running while the gate is shut would send parts across the machine without
  // registering them, i.e. unsorted, and that is not worth leaving to inference.
  FEEDER_ON = false;
  io_drive(FEEDER_PIN, IOI_FEEDER, false);
  // A recal must not blind the machine while parts are still in it.
  //
  // Dropping the estimate here is what broke the tid-free build: with no tid,
  // byTs is the ONLY way to place a real part's frame, and byTs needs a valid
  // clock. Any part already in flight when RECAL began then came back
  // unplaceable and halted the machine on INSP_RESULT_MATCHES_NO_OBJECT --
  // caught as `NOMATCH state=104 valid=0 rb_real=2`. The entry guard checks
  // RBuf, but registration can complete just after it passes, so "empty at
  // entry" is not the same as "empty".
  //
  // The old offset stays authoritative until the pipeline is genuinely clear;
  // syncPulseService performs the reset at the moment it is, immediately before
  // the first pulse. Waiting costs nothing -- it will not fire while a real
  // part is in RBuf anyway.
  //
  // CAL is different and resets outright: it runs before anything is moving,
  // and its whole purpose is that no offset exists yet.
  // The pcnt offset is relearned on the same schedule as the clock, and for a
  // stronger reason: a refused trigger moves it permanently, so a run that has
  // hit one can only recover by measuring it again. CAL is where that happens
  // -- gate shut, nothing outstanding, the one sample that needs no prior
  // knowledge.
  if(full){ CAM_SYNC.reset(); CAM_PCNT.reset(); CAL_RESET_PENDING=false; }
  else     CAL_RESET_PENDING=true;
  CAL_STARTED_MS = (int64_t)(esp_timer_get_time()/1000);
  CAL_PULSE_MS = 0;
  CAL_DEADLINE_MS = CAL_STARTED_MS + CAL_TIMEOUT_MS;
  SYNC_LAST_MS = 0;
  CAL_RUNS++;
  djrl.dbg_printf("CAMSYNC CAL start (gate shut, feeder held)");
}

static void calibrationEnd(bool ok)
{
  GATE_DISABLED = CAL_GATE_PREV;
  CAL_LAST_MS_TAKEN =
    (uint32_t)((int64_t)(esp_timer_get_time()/1000) - CAL_STARTED_MS);
  if(ok)
  {
    // READY's enter block turns the feeder on; nothing to do here but hand over.
    djrl.dbg_printf("CAMSYNC CAL ok in %u ms (offset=%lld us, %u pulses)",
                    (unsigned)CAL_LAST_MS_TAKEN,(long long)CAM_SYNC.offset_us,
                    (unsigned)SYNC_EMITTED);
    SYS_STATE_Transfer(SYS_STATE_ACT::CAL_DONE);
  }
  else
  {
    CAL_FAILS++;
    djrl.dbg_printf("CAMSYNC CAL FAILED after %u ms "
                    "(learned=%u boot_n=%u boot_fail=%u) -- not starting",
                    (unsigned)CAL_LAST_MS_TAKEN,(unsigned)CAM_SYNC.learned,
                    (unsigned)CAM_SYNC.boot_n,(unsigned)CAM_SYNC.boot_fail);
    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                       (int)GEN_ERROR_CODE::CAM_CLOCK_CAL_FAILED);
  }
}

// Fire the camera now, with an object behind it, so calibration needs nothing
// to be moving.
//
// The ordinary CAM trigger lives inside a stage driven by plate step count: a
// phantom pulse is scheduled at a future step, so a stationary plate never
// reaches it. That made calibration depend on spinning the machine up first,
// which is backwards -- the offset should exist before anything moves, and
// spinning a plate to measure a clock is a physical operation performed for a
// purely electrical reason.
//
// trig_cam_burst already fires the camera directly, but it deliberately creates
// no pipeline object, so the core reports nothing and there is no cam_ts to
// learn from. This does both: an object the core can answer, and the same pin
// sequence trig_cam_burst uses.
//
// Deliberately does NOT call ActRegister_pipeLineInfo. Those tasks are
// scheduled at step offsets and would never fire on a stationary plate; the
// camera is driven here instead, and the object is retired by the sweep in
// syncPulseService rather than by the SWITCH stage it will never reach.
static int calFireNow()
{
  // Deliberately NOT gated on blockNewDetectedObject.
  //
  // That flag means "do not let the SENSOR put parts into the pipeline". This
  // is not the sensor -- calibration is asking for a pulse on purpose, and it
  // is the only caller. Honouring the flag here forced CAL and RECAL to clear
  // it just to be able to fire, which also opened the gate to real parts for
  // the whole phase: measured as two parts registering DURING a recal, whose
  // reports then had no valid clock to be placed by. The flag now stays set
  // through calibration and only the sensor path is blocked.
  pipeLineInfo *head = RBuf.getHead();
  if(head==NULL) return -1;

  head->w            = 0;
  head->gate_pulse   = SYS_STEP_COUNT;
  head->insp_status  = insp_status_UNSET;
  head->tid          = CAL_TID_NEXT++;
  head->trig_us      = (uint32_t)esp_timer_get_time();
  head->cam_us       = 0;
  head->cam_pcnt     = 0;              // stamped at the edge, below
  head->retired      = 0;
  head->stage        = 0;
  head->sync         = 1;              // only sync objects teach the offset
  RBuf.pushHead();

  // Same order as trig_cam_burst's emit_at: camera line first, then the light.
  //
  // The stamp is taken at the L1A rising edge, and that is both the physically
  // correct instant and the one the stage path uses -- which on this rig are
  // the same thing:
  //
  //   As of 2026-08-06 the lines are separate again: GPIO17 (CAM1) triggers
  //   the camera, GPIO16 (L1A) drives the backlight. Both are driven here, back
  //   to back in the same pass, so the stamp is good for either edge -- but the
  //   edge that MATTERS is now CAM1's. (Between 2026-08-05 and 2026-08-06 the
  //   trigger was spliced onto L1A and CAM1 went nowhere; comments from that
  //   window say the opposite.)
  //
  //   In the stage path ACT_L1A and ACT_CAM1 are both scheduled at step offset
  //   654 and run in the same ISR pass off one fetched time_us -- so an
  //   ordinary part's cam_us is stamped at the L1A edge too.
  //
  // Stamping before the CAM1 no-op instead, as this did until now, put cam_us
  // 100us ahead of the frame it describes and 100us out of step with running.
  // (An earlier attempt to correct this was backed out after burst runs went
  // from accept 618 to 21 -- but every one of those runs was at min_sep 12000,
  // i.e. driving the gate at 83Hz into a ~35Hz camera, which halts on committed
  // HEAD just the same. That was camera saturation, not this.)
  // Both lines together, which is what the stage path does: ACT_L1A and
  // ACT_CAM1 share step offset 654, so they fire in the SAME ISR pass with no
  // delay between them. This used to lead CAM1 by 100us, copied from
  // trig_cam_burst's light_delay -- a skew the path it is meant to match does
  // not have. (That skew was spent waiting on a pin that, at the time, was
  // not connected to anything -- it is the camera trigger again now.)
  const int64_t rise = esp_timer_get_time();
  io_drive(PIN_O_CAM1, IOI_CAM1, true);   // the camera trigger (GPIO17)
  // The calibration pulse is the sample BOTH pairings bootstrap from, so it
  // has to carry both stamps. It is also the only pulse fired with nothing
  // else outstanding, which is precisely what makes the count offset it
  // teaches unambiguous.
  head->cam_pcnt = ++CAM_PULSE_N;
  io_drive(PIN_O_L1A,  IOI_L1A,  true);   // the backlight (GPIO16)
  // 100us was exactly the camera's trigger floor -- the same ~100us this file
  // cites at STAGE_PULSE_OFFSET to explain why a 2-tick (66us) window "could
  // not have fired a camera at all". Sitting ON a documented floor is not a
  // margin, and the machine behaved accordingly: 2 of ~20 calibration pulses
  // answered (2026-08-06), CAL timing out at 30s with learned=2, boot_n=2,
  // boot_fail=0 -- i.e. it never collected enough samples to even attempt the
  // median, so every convergence parameter was a red herring.
  //
  // 600us matches what the RUNNING path actually uses (18 ticks = 900us at
  // pf 10000) and clears the 300us the backlight needs for full brightness.
  // It costs nothing: the plate is stationary during calibration, so pulse
  // width buys no motion blur, and a whole calibration is 8 pulses.
  delayMicroseconds(CAL_PULSE_WIDTH_US);
  io_drive(PIN_O_L1A,  IOI_L1A,  false);
  io_drive(PIN_O_CAM1, IOI_CAM1, false);
  head->cam_us = (uint64_t)rise;

  // Announce it, exactly as the CAM stage does -- this is what lets the core
  // answer with a tid, and therefore what makes the frame attributable.
  ISRTrigInfo *commInfo = ISRTrigQ.getHead();
  if(commInfo)
  {
    commInfo->trig_time_us = rise;
    commInfo->btrig_idx    = 1;
    commInfo->trig_id      = head->tid;
    commInfo->gate_pulse   = head->gate_pulse;
    commInfo->sync         = head->sync;
    ISRTrigQ.pushHead();
    calTrigQMark(head->tid, head->gate_pulse);
  }
  SYNC_EMITTED++;
  return 0;
}

// Calibration pulses whose object was retired before the report came back.
//
// A calibration pulse can be in flight when CAL ends, and its object is retired
// on the way out -- so the report arrives with nothing left to match. While the
// core paired frames this never surfaced: the core recognised those frames as
// its own sync pulses and never sent them (PairResult PAIRED_SYNC). With
// PERIF_CORE_PAIRING 0 the core reports EVERY frame, so the late one arrives
// here, matches nothing, and faults the machine with
// INSP_RESULT_MATCHES_NO_OBJECT -- observed as accept=3 judged=0 state=112.
//
// The fault itself is right and must stay: a report that matches no object
// normally means the pairing has desynced, and guessing is what this whole
// design refuses to do. So rather than softening it, remember the few pulses
// that were retired with an answer still owed and recognise their frames
// exactly. Anything that is NOT one of them still stops the machine.
//
// A stale tombstone cannot swallow a real part's frame: cam_us is monotonic
// esp_timer time, so a retired pulse is always OLDER than any later frame's
// expected time by far more than the window -- and calibration only ever runs
// with the gate shut and the pipeline drained, so no real part shares its
// moment in the first place.
static const int SYNC_TOMB_N = 4;
static uint64_t  SYNC_TOMB_US[SYNC_TOMB_N] = {0};
static uint8_t   SYNC_TOMB_W = 0;
static uint32_t  SYNC_TOMB_HITS = 0;

static void syncTombstone(uint64_t cam_us)
{
  if(cam_us==0) return;
  SYNC_TOMB_US[SYNC_TOMB_W] = cam_us;
  SYNC_TOMB_W = (uint8_t)((SYNC_TOMB_W+1)%SYNC_TOMB_N);
}

static bool syncTombMatches(uint64_t cam_ts)
{
  if(cam_ts==0 || !CAM_SYNC.valid) return false;
  const int64_t want = CAM_SYNC.expectedCamUs(cam_ts);
  for(int i=0;i<SYNC_TOMB_N;i++)
  {
    if(SYNC_TOMB_US[i]==0) continue;
    int64_t d = (int64_t)SYNC_TOMB_US[i] - want; if(d<0) d=-d;
    if(d<=CamClockSync::TOL_US) return true;
  }
  return false;
}

// Leaving CAL or RECAL, by any route -- converged, timed out, or the operator
// left. Restoring the gate here rather than in calibrationEnd means a path that
// skips that call cannot leave the machine gated shut.
static void calibrationCleanup()
{
  blockNewDetectedObject=true;
  GATE_DISABLED=CAL_GATE_PREV;
  // Leaving by any route drops a pending recal reset. The old estimate is
  // still valid and still correct; it simply did not get topped up.
  CAL_RESET_PENDING=false;

  // Retire every calibration object, answered or not.
  //
  // calFireNow deliberately registers no stage tasks, so SWITCH -- the only
  // thing that ever sets insp_status_DEL -- will never reach these. The in-phase
  // sweep only runs while the phase does, so the last pulse (still unanswered at
  // handover) would sit in RBuf forever, and one slot per calibration is a leak:
  // measured as rej_busy 518 and twelve auto-rate backoffs, dropping the
  // effective gate from 83Hz to 20Hz. Calibration is over; these have no part to
  // sort and nowhere to go.
  for(int k=0;k<RBuf.size();k++)
  {
    pipeLineInfo *p=RBuf.getTail(k);
    if(p==NULL) break;
    // Only the ones still owed an answer leave a tombstone -- an already
    // answered pulse has no report left to arrive.
    if(p->sync && p->insp_status==insp_status_UNSET) syncTombstone(p->cam_us);
    if(p->sync){ p->insp_status=insp_status_DEL; p->retired=1; }
  }
}

// Top the offset up before idle drift can move a frame off its object.
//
// The offset is re-measured on every report, so while parts flow it is never
// more than one part old (~2us at 18/s). An idle line stops reporting, and from
// then on it drifts at the crystal rate with nothing to correct it.
//
// The threshold comes from a position tolerance, not from the clock:
//
//   300us of clock error x 10000 pulse/s = 3 plate steps
//   240mm plate -> 753.98mm / 60000 steps = 0.012566 mm/step
//   3 steps = 0.0377mm at the rim
//
// So 300us is "the frame is attributed to within 0.038mm of where the part
// actually was", and at the 35us/s worst-case drift measured on this machine
// that is reached in 8.6s. Hence 10s.
//
// Careful with that sentence, because it conflates two things that behave
// differently (2026-08-06):
//
//   - Clock OFFSET error does not move the part in the image at all. The
//     trigger fires from the step ISR (Run_ACTS inside onTimer, alongside
//     StepGo), so it is locked to plate position rather than to wall time; a
//     stalled ISR stalls the motor too. Offset error only decides which object
//     record a frame is matched against, and neighbours are 33ms / 8.3mm apart.
//   - The camera's trigger-to-exposure latency jitter DOES displace the part,
//     and it lands in the same matching residual.
//
// So the position framing is not wrong, but it applies to the jitter term, not
// to the drift term. Drift's real cost is spending the match window: 350us of
// it after 10s idle. See the TODO beside the cam_match_window_us floor.
//
// Note this is well inside the point where anything would BREAK: 5000us window
// / 35us/s = 143s before a returning frame would even fall outside the match
// window, and the first frame back re-measures the offset anyway. This is a
// precision guarantee, not a failure guard.
static void recalService()
{
  if(sysinfo.state != SYS_STATE::INSPECTION_MODE_READY) return;
  if(CAM_RECAL_IDLE_MS <= 0) return;              // 0 disables
  if(!CAM_SYNC.valid || CAM_SYNC.est_cam_us==0) return;

  const int64_t now_ms  = (int64_t)(esp_timer_get_time()/1000);
  const int64_t idle_us =
    (int64_t)esp_timer_get_time() - (int64_t)CAM_SYNC.est_cam_us;
  if(idle_us < (int64_t)CAM_RECAL_IDLE_MS*1000) return;

  // The clock going stale is not the same thing as the line being idle, and
  // only the second one makes it safe to shut the gate.
  //
  // est_cam_us advances on every ACCEPTED report, so normally a running line
  // keeps it fresh by itself. But reports that fall outside the window do not
  // update it -- so a machine that is busily producing while something is wrong
  // with the pairing would look "idle" here, and this would shut the gate and
  // stop the feeder in the middle of production. Require that no real part has
  // been registered either.
  //
  // A part registered but not yet reported also counts as "not idle": RBuf is
  // checked below, and the drain guard in syncPulseService will hold the pulses
  // until it clears anyway.
  if(REAL_ACCEPT_MS!=0 &&
     (now_ms-REAL_ACCEPT_MS) < (int64_t)CAM_RECAL_IDLE_MS) return;

  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    if(!p->sync) return;            // a real part is still in the machine
  }

  CAM_RECALS++;
  djrl.dbg_printf("CAMSYNC RECAL: %llds idle, est drift %lld us -- topping up",
                  (long long)(idle_us/1000000),
                  (long long)(idle_us/1000000*35));
  SYS_STATE_Transfer(SYS_STATE_ACT::RECAL_START);
}

static void spinupBegin()
{
  SPINUP_DEADLINE_MS =
    (int64_t)(esp_timer_get_time()/1000) + SPINUP_TIMEOUT_MS;
}

static void spinupService()
{
  if(sysinfo.state != SYS_STATE::INSPECTION_MODE_SPINUP) return;
  // Compared against the setpoint explicitly, not just SYS_FREQ_STABLE.
  //
  // That flag means "current == target", and calibration now runs with the
  // plate deliberately held at zero -- where current and target are both 0 and
  // the flag is therefore TRUE. Entering spin-up would see the previous
  // phase's flag before the ramp had even started and declare the plate up to
  // speed at 0 Hz, which is exactly what it did on the first run.
  if(SYS_FREQ_STABLE && PLATE_FREQ_CURRENT == PLATE_FREQ_SETPOINT)
  {
    djrl.dbg_printf("PLATE spinup done (freq=%d)",(int)PLATE_FREQ_CURRENT);
    SYS_STATE_Transfer(SYS_STATE_ACT::SPIN_READY);
    return;
  }
  if((int64_t)(esp_timer_get_time()/1000) > SPINUP_DEADLINE_MS)
  {
    djrl.dbg_printf("PLATE spinup TIMEOUT (cur=%d target=%d)",
                    (int)PLATE_FREQ_CURRENT,(int)PLATE_FREQ_TARGET);
    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                       (int)GEN_ERROR_CODE::PLATE_SPINUP_TIMEOUT);
  }
}

static void syncPulseService()
{
  // Phantom pulses exist only to calibrate. Once READY, the offset is
  // maintained by ordinary reports, so injecting anything would just be
  // stealing gate slots from real parts again.
  if(sysinfo.state != SYS_STATE::INSPECTION_MODE_CAL &&
     sysinfo.state != SYS_STATE::INSPECTION_MODE_RECAL) return;

  const int64_t now_ms = (int64_t)(esp_timer_get_time()/1000);

  // While the reset is still pending the old estimate is deliberately still
  // valid, so this test would declare the recal finished before it had taken a
  // single sample.
  if(!CAL_RESET_PENDING && CAM_SYNC.valid){ calibrationEnd(true); return; }
  if(now_ms > CAL_DEADLINE_MS)
  {
    if(CAL_RESET_PENDING)
    {
      // The machine never emptied, so the recal never started -- and because it
      // never started, the previous offset is intact and still authoritative.
      // Nothing was lost, so this is not a failure: give up on the top-up and
      // carry on rather than stopping a working machine over a refinement.
      CAL_RESET_PENDING=false;
      CAL_RESET_SKIPPED++;
      djrl.dbg_printf("CAMSYNC RECAL skipped -- pipeline never emptied; "
                      "keeping previous offset");
      calibrationEnd(true);
      return;
    }
    calibrationEnd(false);
    return;
  }

  // The gate is shut, so there is no real work to collide with and no
  // REAL_ACCEPT_MS guard to get wrong. 300ms is simply how fast the samples can
  // be collected.
  if(SYNC_LAST_MS!=0 && (now_ms-SYNC_LAST_MS) < 300) return;

  // Retire answered calibration objects.
  //
  // SWITCH is the only stage that ever sets insp_status_DEL, and SWITCH is
  // driven by plate steps -- so on a stationary plate nothing would retire
  // these and they would fill RBuf and stall the very phase they belong to. A
  // calibration object has no selector to reach and no part to sort: once it
  // has been answered it is finished.
  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    // Braces, and they are load-bearing: without them `p->retired=1` ran for
    // EVERY object in RBuf, including the sync pulse fired 300ms ago that was
    // still waiting for its answer. The drain reads only `retired`, so that
    // object was freed before its report could arrive -- insp_status stayed
    // UNSET, but the slot was gone, so the `outstanding` guard below saw
    // nothing, fired another pulse, and repeated.
    //
    // It hid because it is a race against the round trip. With the camera ROI
    // cropped the report comes back in ~10ms, inside a single main-loop pass,
    // so the object was almost always answered before this sweep saw it. Take
    // the crop away and the round trip never fits: measured 2026-08-11 at full
    // frame as CAL FAILED after 30001ms with learned=0, boot_n=0 and
    // cal_pulse_lost=0 -- the last of those being the tell, since a pulse that
    // was fired and unanswered increments it. Nothing was timing out. The
    // pulses were being deleted.
    if(p->sync && p->insp_status!=insp_status_UNSET)
    { p->insp_status=insp_status_DEL; p->retired=1; }
  }

  // One at a time, so the returning frame has exactly one candidate by
  // construction and the sample cannot need the estimate it is meant to
  // produce.
  //
  // But an outstanding pulse gets its own deadline, not just the phase's. A
  // frame that never comes back -- the camera declined the trigger, the core
  // dropped it, the link hiccuped -- would otherwise hold this guard shut for
  // the whole 30s phase timeout and then fail the machine, having taken exactly
  // one sample. Observed once on the bench: cal_fails=1, cal_ms=30001,
  // learned=0, accept=1. Give up on that pulse and fire another instead; one
  // lost frame is not a reason to refuse to start.
  // A real part still anywhere in the pipeline blocks the pulses outright, and
  // "still in the pipeline" means still in RBuf -- not merely "already
  // answered". An answered part has a verdict but has not yet reached SWITCH to
  // act on it, and firing the backlight across a part mid-flight is not
  // something to do on the assumption that it happens to be harmless. It is
  // harmless today only because calFireNow registers no stage tasks and so
  // cannot collide with a selector action; that is an accident of the
  // implementation, not a property anyone guaranteed.
  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    if(!p->sync) return;
  }

  // Past the drain guard: no real part is left in RBuf, and a part that has
  // left RBuf has already been answered -- so no real report is still owed.
  // This is the first moment at which dropping the estimate cannot orphan
  // anything, which is why the recal reset happens here rather than at entry.
  if(CAL_RESET_PENDING)
  {
    CAM_SYNC.resetEstimate();
    // Drop the count offset with it. A top-up exists to re-measure what may
    // have moved, and the count offset is the one that moves in whole pulses;
    // keeping it across a recal would carry a slip through the exact event
    // meant to clear it. valid=false makes the next sync pulse teach it again.
    CAM_PCNT.valid=false;
    CAL_RESET_PENDING=false;
    djrl.dbg_printf("CAMSYNC RECAL: pipeline clear, re-measuring");
  }

  bool outstanding=false;
  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    if(p->insp_status==insp_status_UNSET){ outstanding=true; break; }
  }
  if(outstanding)
  {
    if(CAL_PULSE_MS==0 || (now_ms-CAL_PULSE_MS) < CAL_PULSE_TIMEOUT_MS) return;
    // Abandon it. Marking DEL both frees the RBuf slot and clears the guard;
    // the sample is simply never taken, which is the honest outcome.
    for(int i=0;i<RBuf.size();i++)
    {
      pipeLineInfo *p=RBuf.getTail(i);
      if(p==NULL) break;
      if(p->sync && p->insp_status==insp_status_UNSET)
      {
        // It may yet come back late -- that is the whole reason we gave up on
        // it rather than knowing it was lost.
        syncTombstone(p->cam_us);
        p->insp_status=insp_status_DEL; p->retired=1;
      }
    }
    CAL_PULSE_LOST++;
    djrl.dbg_printf("CAMSYNC CAL pulse unanswered after %dms -- retrying",
                    (int)CAL_PULSE_TIMEOUT_MS);
  }

  calFireNow();
  CAL_PULSE_MS = now_ms;
  SYNC_LAST_MS = now_ms;
}

// Gate width of the object a cam_trig refers to, in plate steps. Looked up
// here rather than carried through the ISR trigger queues, which are kept
// deliberately small.
static uint32_t gateWidthOf(uint32_t tid)
{
  for(int i=0;i<RBuf.size();i++)
  {
    pipeLineInfo *p=RBuf.getTail(i);
    if(p==NULL) break;
    if(p->tid==tid) return p->w;
  }
  return 0;
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

  // Even spacing plus noise, not even spacing.
  //
  // A perfectly regular train is a degenerate test: every object sits at the
  // same offset from its neighbour, so the match either always succeeds or
  // always fails and the boundary is never explored. Real lines are regular
  // ISH -- a nominal pitch with jitter -- and the risk is a tail event, the
  // occasional interval short enough to bring a neighbour near the window.
  // Sweeping the jitter is how the margin gets measured as a distribution
  // rather than guessed from a single point.
  //
  // The noise is a seeded LCG, not esp_random(), so a run that finds a
  // mis-sort can be replayed exactly. An unreproducible failure is most of the
  // way to no failure at all -- learned the hard way on this machine, from one
  // misplaced-verdict run that thirteen repeats could not bring back.
  //
  // Phase is kept against the absolute schedule (+= period), so jitter
  // displaces each pulse without the train drifting away from its nominal rate.
  int32_t step = PH_TRAIN_PERIOD_US;
  if(PH_TRAIN_JITTER_US > 0)
  {
    PH_TRAIN_RNG = PH_TRAIN_RNG*1664525u + 1013904223u;
    const int32_t span = PH_TRAIN_JITTER_US*2 + 1;
    step += (int32_t)((PH_TRAIN_RNG >> 8) % (uint32_t)span) - PH_TRAIN_JITTER_US;
    // The gate enforces its own minimum anyway, but a non-positive step would
    // make the schedule run backwards.
    if(step < 1) step = 1;
  }
  PH_TRAIN_NEXT_US += step;
  // If the loop was held up longer than a whole period, do not try to catch up
  // by firing back-to-back -- that would hand the pipeline a burst it never
  // asked for. Give up the missed slots and stay on the original phase.
  if(PH_TRAIN_NEXT_US < now) PH_TRAIN_NEXT_US = now + step;
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
        
        // 64-bit, like the ISR path at the CAM stage. Truncating to uint32 and then
    // widening into an int64 field does not preserve congruence mod 2^32, so
    // past 71.6 minutes of uptime this command announced a t_us up to 4294.97 s
    // in the past -- and t_us is what the pairing consumes.
    uint64_t time_us=(uint64_t)esp_timer_get_time();
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
    // A manual pulse is still a pulse the camera counts. Not counting it here
    // would move the pcnt offset by one for the rest of the run, and it would
    // be indistinguishable from a trigger the camera refused.
    if(cam_PIN==PIN_O_CAM1) CAM_PULSE_N++;
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

      // Backlight identity pattern. "prbs" or "alt"; absent = off (the plain
      // train this command has always emitted).
      //
      // Two patterns because they fail to see different things, and the one
      // that matters here is a slip that CORRECTS ITSELF:
      //
      //   alt (010101)  a single slip inverts the phase, and a slip back
      //                 inverts it again -- so a transient shows up as a BAND
      //                 of inverted parity with a readable start and end.
      //                 Blind to slips of an even number of pulses, which is
      //                 the price of a period-2 pattern.
      //   prbs          sees a slip of any size, but a transient appears as a
      //                 short loss of alignment rather than as a band, which
      //                 takes correlation to read rather than an eye.
      //
      // Run both. Neither alone is a complete instrument, and the 2026-08-08
      // slip that a regular pattern concealed is why "just use a pattern" is
      // not good enough on its own.
      const char *pat = doc["pattern"].is<const char*>() ? (const char*)doc["pattern"] : NULL;
      const bool  prbs_on = (pat && strcmp(pat,"prbs")==0);
      const bool  alt_on  = (pat && strcmp(pat,"alt")==0);
      uint32_t prbs_state = doc["seed"].is<uint32_t>() ? (uint32_t)doc["seed"] : 0xACE1u;
      if(prbs_state==0) prbs_state=0xACE1u;
      const uint32_t seed0 = prbs_state;
      // The emitted sequence, echoed back so the host compares against what
      // was ACTUALLY driven rather than against what it asked for.
      // The dim level, and it has to be SHORTER THAN THE EXPOSURE to be dim at
      // all. The sensor integrates only during its exposure window -- 50us on
      // this station -- so 400us and 100us of light produce identical frames.
      // Measured: means 157..161 across a whole 010101 train, no two levels at
      // all, and the pattern was unreadable. 10us against a 50us exposure is
      // roughly a fifth of the photons, which separates cleanly.
      int dim_us = doc["dim_us"].is<int>() ? (int)doc["dim_us"] : 10;
      if(dim_us<1) dim_us=1;
      uint8_t pat_bits[32]; memset(pat_bits,0,sizeof(pat_bits));
      int pat_n=0;
      int alt_i=0;

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
        if(cam_PIN==PIN_O_CAM1) CAM_PULSE_N++;   // see trig_cam_pulse
        // Per-pulse backlight, so the IMAGE says which pulse exposed it.
        //
        // Neither pairing mechanism can arbitrate itself: cam_ts is a claim
        // about when the frame was exposed and pcnt is a claim about which
        // trigger asked for it, and when they disagree there has been nothing
        // to appeal to. Varying the light per pulse puts the answer in the
        // frame -- brightness is a measurement OF the exposure, not a claim
        // about it -- so whichever mechanism agrees with the picture is right.
        //
        // A PRBS and not a repeating cycle, deliberately: a regular pattern
        // hides a slip of exactly its period. That is not hypothetical, it is
        // the measured 2026-08-08 result -- 510 parts passed a regular-pattern
        // check that was concealing a real 10-part slip. Both ends regenerate
        // the sequence from `seed`, which is echoed in the reply.
        //
        // The bit chooses the DURATION rather than switching the light off:
        // a dark frame and a dropped frame look identical, and telling those
        // two apart is the whole exercise. Two bright levels always mean a
        // frame arrived, and the level says which pulse it was.
        int _lit = Light_Duration;
        if(alt_on)
        {
          const bool bit = ((alt_i++) & 1) != 0;
          if(pat_n < (int)(sizeof(pat_bits)*8) && bit)
            pat_bits[pat_n>>3] |= (uint8_t)(1u<<(pat_n&7));
          pat_n++;
          _lit = bit ? Light_Duration : dim_us;
        }
        else if(prbs_on)
        {
          prbs_state = (uint32_t)((prbs_state<<1) |
                       (((prbs_state>>31)^(prbs_state>>21)^
                         (prbs_state>>1)^prbs_state) & 1u));
          const bool bit = (prbs_state & 1u) != 0;
          if(pat_n < (int)(sizeof(pat_bits)*8) && bit)
            pat_bits[pat_n>>3] |= (uint8_t)(1u<<(pat_n&7));
          pat_n++;
          _lit = bit ? Light_Duration : dim_us;
        }
        io_drive(cam_PIN,cam_idx,true);
        delayMicroseconds(Light_Delay);
        io_drive(light_PIN,light_idx,true);
        delayMicroseconds(_lit);
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
      if(pat)
      {
        retdoc["pattern"]=pat;
        retdoc["seed"]=seed0;
        retdoc["pat_n"]=pat_n;
        // Hex, most significant nibble first per byte, bit i of the sequence
        // living at byte i/8 bit i%8 -- the same order the host rebuilds it in.
        char hex[sizeof(pat_bits)*2+1];
        const int nb=(pat_n+7)/8;
        for(int b=0;b<nb && b<(int)sizeof(pat_bits);b++)
          snprintf(hex+b*2,3,"%02x",pat_bits[b]);
        hex[(nb<(int)sizeof(pat_bits)?nb:(int)sizeof(pat_bits))*2]=0;
        retdoc["pat_hex"]=hex;
      }
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
    // Raw pin access takes any GPIO number the caller sends, and SEL1 is 25
    // while STEPPER_EN is 13 (HardwareConfig.hpp). In READY that means an
    // arbitrary actuator fired at an arbitrary plate position, or the driver
    // de-energised at speed. `light` already checks this; these did not.
    if(cfgPersistDeny()!=NULL)
    {
      retdoc["type"]="pin_mode";
      retdoc["err"]=cfgPersistDeny();
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
    }
    else {
    doRsp=true;
    }
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
  // The enum lives here, so the names have to come from here too. A copy of the
  // table in the WebUI is a copy that drifts: it carried five of the ten states
  // and one that never existed, so a machine waiting in CAL displayed
  // "state 102" -- a number that tells the person watching it nothing. Both
  // lists are generated from the same X-macros the enums are, so adding a state
  // cannot leave the panel behind.
  else if(strcmp(type,"get_state_names")==0)
  {
    retdoc["type"]="state_names";
    {
      JsonObject js = retdoc.createNestedObject("state");
      #define SMM_GEN_NAME_X(NAME,VALUE,X) js[#VALUE]=#NAME;
      SMM_STATE_DECLARE(SMM_GEN_NAME_X)
      #undef SMM_GEN_NAME_X
    }
    {
      JsonObject je = retdoc.createNestedObject("err");
      #define ERR_NAME_X(NAME,VALUE,TEXT) je[String((int)(VALUE))]=TEXT;
      GEN_ERROR_CODE_DECLARE(ERR_NAME_X)
      #undef ERR_NAME_X
    }
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
    // Settings the stored config still carries that this firmware dropped.
    // Present means: those values are NOT in effect, whatever they
    // configured is at its compiled default, and nothing has been written
    // back. The UI warns, shows the old values, and offers the conversion.
    if(MachineConfig::staleKeyCount()>0)
    {
      retdoc["cfg_stale_n"]=MachineConfig::staleKeyCount();
      retdoc["cfg_stale_keys"]=MachineConfig::staleKeyNames();
      retdoc["cfg_stale_values"]=MachineConfig::staleKeyValues();
    }

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"set_setup")==0)
  {
    retdoc["type"]="set_setup";

    // Name back anything the schema does not contain, and refuse the whole
    // command. Applying the half it understood and acking true is how eight
    // tools spent a week configuring nothing after the regroup -- see
    // cfgUnknownKeys. Checked BEFORE anything is applied, so a document with a
    // typo in it changes nothing at all rather than partially.
    char unk[160];
    const int nunk = cfgUnknownKeys(doc.as<JsonObject>(), unk, sizeof(unk));
    if(nunk>0)
    {
      retdoc["err"]="unknown_keys";
      retdoc["unknown"]=unk;
      retdoc["n_unknown"]=nunk;
      djrl.dbg_printf("SET_SETUP REFUSED: %d unknown key(s): %s",nunk,unk);
      doRsp=true; rspAck=false;
    }
    else
    {
    setMachineSetup(doc, true);
    MachineConfig::invalidateHash();

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
    }                       // end of the schema-clean body

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
    //
    // That reasoning is about the RAM values and it is correct, but it is not
    // the hazard cfgPersistDeny exists for. An NVS erase disables the
    // instruction cache, and only onTimer() is IRAM -- StepGo, GateSensing,
    // Run_ACTS and newPulseEvent are all ordinary flash-resident functions the
    // ISR calls straight into. Erasing with the timer live risks a stall or a
    // reset mid-production, with a selector possibly energised.
    //
    // Every other flash writer is gated on this. This one was not, which also
    // means RELIABILITY_ROADMAP's decision not to IRAM the ISR chain -- taken
    // because "flash writes have converged to a single entry point and are
    // hard-blocked by the standstill guard" -- was resting on a false premise.
    // There were two entry points.
    retdoc["type"]="clear_saved_setup";
    const char* deny_clr=cfgPersistDeny();
    if(deny_clr==NULL)
    {
      retdoc["cleared"]=MachineConfig::clear();
      doRsp=rspAck=true;
    }
    else
    {
      retdoc["cleared"]=false;
      retdoc["persist_err"]=deny_clr;
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
    }
  }
  else if(strcmp(type,"reset_running_stat")==0)
  {
    // "hwm":true resets ONLY the high-water and averaged values, leaving the
    // counters and the clock model alone. It exists because every max in here
    // is a since-reset high-water, so during a long run they latch onto
    // whatever happened at spin-up and then say nothing for hours: a 3h soak
    // reported isr_gap_max 12720us and lat_max 366984us from t+136s onward,
    // both set by the entry seam, and the running values were invisible
    // underneath them.
    //
    // The reason this needs its own flag rather than "just call reset every
    // few minutes" is CAM_SYNC.reset() below. Clearing the clock mid-run is
    // NOT recoverable while the line is moving: recalService() bails on
    // `!CAM_SYNC.valid` before it does anything, so the one thing that could
    // rebuild the offset is gated on the offset already existing. The machine
    // silently falls back to tid pairing for the rest of the run and nothing
    // says so -- measured, as agree=0 and delta_max=0 across a whole window.
    const bool hwm_only = doc["hwm"].is<bool>() ? doc["hwm"].as<bool>() : false;
    if(hwm_only)
    {
      ISR_GAP_MAX_CY=0;
      ISR_DUR_MAX_CY=0; ISR_OVERRUN_N=0; ISR_DUR_SUM_CY=0; ISR_DUR_N=0;
      RBUF_PEAK=0;
      ISRTRIGQ_HWM=0; ISRTRIGQ_BURST=0;
      ACT_LATE_MAX=0;
      LOOP_N=0; LOOP_MAX_US=0;
      SEG_SVC_US=SEG_ST_US=SEG_RX_US=SEG_TX_US=0;
      REP_LAT_N=0; REP_LAT_SUM_US=0; REP_LAT_MAX_US=0;
      REP_CAMLAT_N=0; REP_CAMLAT_SUM_US=0; REP_CAMLAT_MAX_US=0;
      CAM_SYNC.delta_max_us=0;      // the margin, windowed; the offset stays
      CAM_SYNC.max_resid_us=0;
      CAM_SYNC.miss_delta_max_us=0;
      // The histogram is the instrument for SIZING the window, so a tail
      // carried over from the previous setting is the one thing it must
      // not show. reset() misses it too -- added there in the same pass.
      for(int i=0;i<CamClockSync::DELTA_BUCKETS;i++) CAM_SYNC.delta_hist[i]=0;
      retdoc["hwm"]=true;
      retdoc["clock_reset"]=false;
      doRsp=rspAck=true;
    }
    else
    {
    // Full reset from here. Announced in the reply because it takes the clock
    // with it, and a caller that did not mean to should be able to see that.
    retdoc["clock_reset"]=true;

    SEL1_Count=SEL2_Count=SEL3_Count=NA_Count=0;
    SKIP_Count=0;
    UNANSWERED_Count=0;
    CONSEC_UNANSWERED=0;
    ISR_GAP_MAX_CY=0;
    ISR_DUR_MAX_CY=0; ISR_OVERRUN_N=0; ISR_DUR_SUM_CY=0; ISR_DUR_N=0;
    RBUF_PEAK=0;
    ISRTRIGQ_HWM=0;
    ISRTRIGQ_OVF=0;
    ISRTRIGQ_BURST=0;
    HWM_TID_NEW=HWM_TID_OLD=HWM_GATE_NEW=HWM_GATE_OLD=HWM_STEP=0; HWM_STATE=0;
    HWM_AGE_US=0;
    ACT_LATE_MAX=0;
    LATE_TID=LATE_GATE=LATE_TARGET=LATE_CUR=0;
    LATE_QDEPTH=LATE_PREV_TARGET=LATE_LAST_TARGET=0;
    LATE_SYNC=LATE_PREV_SYNC=LATE_LAST_SYNC=0; LATE_W=0;
    PUSHLOG_I=0; PUSHLOG_FROZEN=0; PUSHLOG_SEEN=0;
    LOOP_N=0;
    LOOP_MAX_US=0;
    SEG_SVC_US=SEG_ST_US=SEG_RX_US=SEG_TX_US=0;
    GATE_ACCEPT=GATE_REJ_RATE=GATE_REJ_DIST=GATE_REJ_BUSY=0;
    GATE_EDGES=GATE_REJ_WIDTH=GATE_REJ_UNSTABLE=GATE_REJ_BLOCKED=0;
    GATE_REJ_STEPPER_OFF=GATE_REJ_GATE_OFF=GATE_REJ_DRYRUN=0;
    // The clock model too. Leaving it out made every segmented experiment
    // read the previous segment's numbers: an A/B control appeared to show 12
    // disagreements that were entirely leftovers, which nearly produced the
    // wrong conclusion about which matching mode is safe.
    CAM_SYNC.reset();
    REP_REPEAT_N=REP_REPEAT_DIFF_N=REP_REPEAT_WORSE_N=0;
    REP_LAT_N=0;
    REP_LAT_SUM_US=0;
    REP_LAT_MAX_US=0;

    doRsp=rspAck=true;
    }

  }
  // The hot poll. Everything a host checks in a loop -- am I running, is the
  // plate at speed, has anything faulted -- in ~120 bytes instead of the 1174
  // of get_setup or the 1325 of get_running_stat.
  //
  // Why this exists: those two are the ONLY way to read step_count and state,
  // so every wait loop in the tools (and the core) polls a full configuration
  // document to read one counter. At 230400 a 1200-byte reply owns the TX path
  // for ~100ms, and Serial.write blocks the main loop for the duration -- the
  // same loop that drains ISRTrigQ. That queue is 32 entries and every object
  // pushes 2, so ~16 objects of headroom; at the rates measured here a single
  // poll can eat a third of it, and a host that polls in a tight loop
  // overflows it. That overflow is INSP_CAM_TRIG_INFO_CANNOT_BE_SENT, which
  // has been showing up under churn at object rates nowhere near any limit.
  //
  // cfg_crc rides along so a host can hold the big document in cache and
  // re-read it only when the fingerprint moves -- "send only what changed",
  // without any delta machinery to get wrong.
  else if(strcmp(type,"poll")==0)
  {
    retdoc["type"]="poll";
    retdoc["state"]=(int)sysinfo.state;
    retdoc["plate_freq"]=PLATE_FREQ_TARGET;      // COMMANDED
    retdoc["plate_freq_meas"]=PLATE_FREQ_MEAS;   // MEASURED -- use this to
                                                 // decide whether it stopped
    retdoc["step_count"]=SYS_STEP_COUNT;
    retdoc["q"]=RBuf.size();
    // The camera-trigger queue, which is NOT `q`. See ISRTRIGQ_HWM: this is
    // the one that overflows, and until now it was the only queue in the
    // machine with no instrument on it.
    retdoc["tq"]=ISRTrigQ.size();
    retdoc["tqhwm"]=ISRTRIGQ_HWM;
    retdoc["tqcap"]=ISRTrigQ.capacity();
    retdoc["tqovf"]=ISRTRIGQ_OVF;
    retdoc["tqburst"]=ISRTRIGQ_BURST;
    // Phantom requests the ISR has not emitted yet, and requests the gate
    // refused. Non-zero pend with a stopped plate is the expected shape (the
    // timer alarm is off); non-zero pend on a MOVING plate means the ISR is not
    // running, which is a different and much worse fault.
    retdoc["ph_pend"]=(uint32_t)(PHANTOM_REQ_N-PHANTOM_DONE_N);
    retdoc["ph_drop"]=PHANTOM_DROP_N;
    // The tick-domain injector: emitted / refused-by-the-gate.
    retdoc["virt_n"]=VIRT_EMIT_N;
    retdoc["virt_drop"]=VIRT_DROP_N;
    retdoc["trig_report_on"]=(bool)TRIG_REPORT_ON;
    retdoc["trig_suppressed"]=TRIG_REPORT_SUPPRESSED;
    retdoc["hwm_tid_new"]=HWM_TID_NEW;
    retdoc["hwm_tid_old"]=HWM_TID_OLD;
    retdoc["hwm_gate_new"]=HWM_GATE_NEW;
    retdoc["hwm_gate_old"]=HWM_GATE_OLD;
    retdoc["hwm_step"]=HWM_STEP;
    retdoc["hwm_state"]=HWM_STATE;
    retdoc["hwm_age_us"]=HWM_AGE_US;
    retdoc["act_late_max"]=ACT_LATE_MAX;
    retdoc["late_tid"]=LATE_TID;
    retdoc["late_gate"]=LATE_GATE;
    retdoc["late_target"]=LATE_TARGET;
    retdoc["late_cur"]=LATE_CUR;
    retdoc["late_qdepth"]=LATE_QDEPTH;
    retdoc["late_prev_target"]=LATE_PREV_TARGET;
    retdoc["late_sync"]=LATE_SYNC;
    retdoc["late_w"]=LATE_W;
    retdoc["late_prev_sync"]=LATE_PREV_SYNC;
    retdoc["loopn"]=LOOP_N;
    retdoc["loopmax_us"]=LOOP_MAX_US;
    retdoc["svc_us"]=SEG_SVC_US;
    retdoc["st_us"]=SEG_ST_US;
    retdoc["rx_us"]=SEG_RX_US;
    retdoc["tx_us"]=SEG_TX_US;
    retdoc["err"]=ERROR_HIST.size() ? (int)*ERROR_HIST.getTail(0) : 0;
    retdoc["nerr"]=ERROR_HIST.size();
    retdoc["cfg_crc"]=MachineConfig::hash();
    doRsp=rspAck=true;
  }
  // Read out the frozen registration window (see PUSHLOG). Oldest first.
  else if(strcmp(type,"pushlog")==0)
  {
    retdoc["type"]="pushlog";
    retdoc["frozen"]=(bool)PUSHLOG_FROZEN;
    retdoc["seen"]=PUSHLOG_SEEN;
    JsonArray a=retdoc.createNestedArray("e");
    uint8_t n = (PUSHLOG_SEEN < PUSHLOG_N) ? (uint8_t)PUSHLOG_SEEN : PUSHLOG_N;
    for(uint8_t k=0;k<n;k++)
    {
      uint8_t i = (uint8_t)((PUSHLOG_I + PUSHLOG_N - n + k) % PUSHLOG_N);
      JsonArray r=a.createNestedArray();
      r.add(PUSHLOG[i].tid); r.add(PUSHLOG[i].gate);
      r.add(PUSHLOG[i].target); r.add(PUSHLOG[i].at);
    }
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


    // The funnel: one acceptance ratio per gate, so tuning has a number to aim
    // at instead of a symptom to guess from.
    //
    // Every "the rate will not come up" on 2026-08-09 was a DIFFERENT stage
    // throttling, and they look identical from outside -- parts on the plate,
    // machine running, throughput low. The counters to tell them apart all
    // existed; what did not exist was the chain that turns them into "this
    // stage passes 22%, and the loss is spacing".
    //
    // Denominator is the stage's own input, not the raw edge count, so a stage
    // is judged on what reached it. The raw edges stay visible as `edges` so
    // the whole chain can still be multiplied out.
    {
      JsonObject jY = retdoc.createNestedObject("yield");
      const uint32_t edges  = GATE_EDGES;
      const uint32_t acc    = GATE_ACCEPT;
      const uint32_t judged = SEL1_Count + SEL2_Count + SEL3_Count + NA_Count;
      const uint32_t acted  = SEL1_Count + SEL2_Count + SEL3_Count;

      jY["edges"] = edges;

      // gate: sensor edges -> registered objects
      {
        JsonObject g = jY.createNestedObject("gate");
        g["in"]=edges; g["out"]=acc;
        g["pct"]= edges ? (100.0*acc/edges) : 0.0;
        // The dominant loss, named. Ranking them here rather than in the UI
        // keeps the reasons and their meaning in the same place.
        const char *why="none"; uint32_t worst=0;
        struct { const char *n; uint32_t v; } R[] = {
          {"rate",GATE_REJ_RATE},{"width",GATE_REJ_WIDTH},
          {"unstable",GATE_REJ_UNSTABLE},{"dist",GATE_REJ_DIST},
          {"busy",GATE_REJ_BUSY},{"blocked",GATE_REJ_BLOCKED},
          {"stepper_off",GATE_REJ_STEPPER_OFF},{"gate_off",GATE_REJ_GATE_OFF},
          {"dryrun",GATE_REJ_DRYRUN},
        };
        for(unsigned i=0;i<sizeof(R)/sizeof(R[0]);i++)
          if(R[i].v>worst){worst=R[i].v; why=R[i].n;}
        g["loss"]=why; g["loss_n"]=worst;
      }

      // verdict: registered objects -> objects that got an answer at all
      {
        JsonObject v = jY.createNestedObject("verdict");
        v["in"]=acc; v["out"]=judged;
        v["pct"]= acc ? (100.0*judged/acc) : 0.0;
        // UNANSWERED is the report that never arrived in time; SKIP passed the
        // selector unjudged and raises no error, so it under-reports as err=2.
        v["unanswered"]=UNANSWERED_Count;
        v["skip"]=SKIP_Count;
        v["loss"]= (UNANSWERED_Count>=SKIP_Count) ? "unanswered" : "skip";
      }

      // sort: answered objects -> objects an ejector acted on. NA is not a
      // fault: nothing fires and the part recirculates for another pass.
      {
        JsonObject a2 = jY.createNestedObject("sort");
        a2["in"]=judged; a2["out"]=acted;
        a2["pct"]= judged ? (100.0*acted/judged) : 0.0;
        a2["na"]=NA_Count; a2["loss"]="na";
      }

      // End to end, the number to maximise.
      jY["overall_pct"] = edges ? (100.0*acted/edges) : 0.0;
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

    retdoc["plate_freq"]=PLATE_FREQ_TARGET;      // COMMANDED
    retdoc["plate_freq_meas"]=PLATE_FREQ_MEAS;   // MEASURED
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
      // Current free heap, not just the all-time minimum.
      //
      // min_heap is a high-water mark, so it cannot tell a LEAK from a
      // transient allocation that happens to get bigger each time -- both make
      // it step down. A soak showed it dropping exactly 96 bytes on every
      // RECAL and never at any other moment, which is suspicious enough to
      // need the direct measurement rather than an inference from a minimum.
      jHl["free_heap"]=esp_get_free_heap_size();
      jHl["min_heap"]=esp_get_minimum_free_heap_size();
      jHl["max_block"]=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      jHl["stack_hwm"]=(uint32_t)uxTaskGetStackHighWaterMark(NULL);
      jHl["isr_gap_max_us"]=ISR_GAP_MAX_CY/240;   // 240MHz CPU
      jHl["isr_dur_max_us"]=ISR_DUR_MAX_CY/240;
      jHl["isr_dur_last_us"]=ISR_DUR_LAST_CY/240;
      jHl["isr_dur_avg_us"]=ISR_DUR_N ? (uint32_t)(ISR_DUR_SUM_CY/ISR_DUR_N/240) : 0;
      jHl["isr_overrun_n"]=ISR_OVERRUN_N;
      jHl["isr_ticks"]=ISR_DUR_N;
      jHl["rbuf_peak"]=RBUF_PEAK;
      jHl["uptime_s"]=(uint32_t)(esp_timer_get_time()/1000000ULL);
      jHl["consec_unanswered"]=CONSEC_UNANSWERED;
      jHl["rx_frames"]=djrl.rx_frames;
      jHl["rx_crc_ok"]=djrl.rx_crc_ok;
      jHl["rx_crc_fail"]=djrl.rx_crc_fail;
      // Why the chip last booted: lets a host that finds the board freshly in
      // IDLE tell a panic/watchdog/brownout from a plain power cycle. Moved
      // here from get_setup, beside the other once-per-boot facts.
      {
        static const char* rr_names[]={"UNKNOWN","POWERON","EXT","SW","PANIC",
                                       "INT_WDT","TASK_WDT","WDT","DEEPSLEEP",
                                       "BROWNOUT","SDIO"};
        int rr=(int)esp_reset_reason();
        jHl["reset_reason"]=rr;
        jHl["reset_reason_name"]=(rr>=0 && rr<11)?rr_names[rr]:"?";
        jHl["xtal_mhz"]=(int)rtc_clk_xtal_freq_get();
      }
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
      jG["edges"]=GATE_EDGES;
      jG["rej_width"]=GATE_REJ_WIDTH;
      jG["rej_unstable"]=GATE_REJ_UNSTABLE;   // now ONLY "not at speed"
      jG["rej_stepper_off"]=GATE_REJ_STEPPER_OFF;
      jG["rej_gate_off"]=GATE_REJ_GATE_OFF;
      jG["rej_dryrun"]=GATE_REJ_DRYRUN;
      jG["rej_blocked"]=GATE_REJ_BLOCKED;
      jG["min_dist_um"]=GATE_MIN_DIST_um;
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
      // Bootstrap forensics. boot[] is the live window; its spread says whether
      // the samples disagree by a constant, a drift, or noise.
      // Sent as deltas from boot[0]: retdoc is a 3072B static and this handler
      // is already dense, so eight full-width offsets would risk truncating the
      // whole response. The deltas are what carries the information anyway.
      jS["last_sample_us"]=(double)CAM_SYNC.last_sample_us;
      jS["boot_fail"]=CAM_SYNC.boot_fail;
      // How many times the offset was measured outright (not converged toward),
      // and how old the current measurement is. resid/age is the drift rate,
      // which is the number that sizes the match window.
      jS["established"]=CAM_SYNC.established;
      // The two numbers the window is set against. delta_max is how much of it
      // was actually needed; compare with gate.eff_sep_us above, which is how
      // far apart the objects are and therefore the hard upper bound.
      jS["window_us"]=(double)CamClockSync::TOL_US;
      jS["delta_max_us"]=(double)CAM_SYNC.delta_max_us;
      jS["delta_last_us"]=(double)CAM_SYNC.delta_last_us;
      jS["cal_runs"]=CAL_RUNS;
      jS["cal_fails"]=CAL_FAILS;
      jS["cal_ms"]=CAL_LAST_MS_TAKEN;
      jS["drift_comp"]=CamClockSync::DRIFT_COMP;
      jS["slope_ppb"]=CAM_SYNC.slope_ppb;
      jS["slope_n"]=CAM_SYNC.slope_n;
      jS["recal_idle_ms"]=CAM_RECAL_IDLE_MS;
      jS["recals"]=CAM_RECALS;
      jS["cal_pulse_lost"]=CAL_PULSE_LOST;
      // Calibration frames answered after their object was retired. Expect a
      // small number per calibration; a large or growing one means pulses are
      // routinely outliving the phase, which is worth looking at.
      jS["sync_late"]=SYNC_TOMB_HITS;
      jS["recal_skipped"]=CAL_RESET_SKIPPED;
      // Accepted-residual histogram: bucket i is [32<<i, 32<<(i+1)) us, last
      // bucket open-ended. This is what the match window has to clear, and a
      // max alone cannot show whether the tail is one outlier or the norm.
      {
        JsonArray jh = jS["delta_hist"].to<JsonArray>();
        for(int i=0;i<CamClockSync::DELTA_BUCKETS;i++) jh.add(CAM_SYNC.delta_hist[i]);
      }
      jS["gap_us"]=(double)CAM_SYNC.last_gap_us;
      // resid/gap. Tens of us/s is two crystals; a sudden jump is not.
      jS["drift_us_per_s"]=CAM_SYNC.last_gap_us
        ? (double)CAM_SYNC.last_resid_us*1000000.0/(double)CAM_SYNC.last_gap_us : 0.0;
      jS["miss_delta_last_us"]=(double)CAM_SYNC.miss_delta_last_us;
      jS["miss_delta_max_us"]=(double)CAM_SYNC.miss_delta_max_us;
      jS["est_age_s"]=CAM_SYNC.est_cam_us
        ? (double)((int64_t)(esp_timer_get_time()-(int64_t)CAM_SYNC.est_cam_us)/1000000.0) : 0.0;
      if(CAM_SYNC.boot_n>0)
      {
        jS["boot0_us"]=(double)CAM_SYNC.boot[0];
        JsonArray jB=jS.createNestedArray("bootd");
        for(int i=1;i<CAM_SYNC.boot_n;i++) jB.add((double)(CAM_SYNC.boot[i]-CAM_SYNC.boot[0]));
      }
    }
    {
      // The pulse-count pairing, reported beside cam_sync rather than inside
      // it: they answer the same question by unrelated means, and folding one
      // into the other's object would invite reading a single "pairing is
      // healthy" number out of two independent measurements.
      JsonObject jP=retdoc.createNestedObject("cam_pcnt");
      jP["mode"]=REPORT_MATCH_PCNT ? (REPORT_MATCH_TS?"both":"pcnt")
                                   : (REPORT_MATCH_TS?"ts":"tid");
      jP["valid"]=CAM_PCNT.valid;
      jP["offset"]=(double)CAM_PCNT.offset;
      jP["learned"]=CAM_PCNT.learned;
      // Nonzero means a pulse went out that the camera did not count (or the
      // reverse). This is the number the whole mechanism exists to surface.
      jP["slip"]=CAM_PCNT.slip;
      jP["hit"]=CAM_PCNT.hit;
      jP["miss"]=CAM_PCNT.miss;
      // Which side went blind. See the declaration for why miss alone is not
      // an answer.
      jP["ts_blind"]=CAM_PCNT.ts_blind;
      jP["pcnt_blind"]=CAM_PCNT.pcnt_blind;
      jP["mismatch"]=CAM_PCNT.mismatch;
      jP["last_sample"]=(double)CAM_PCNT.last_sample;
      jP["last_pcnt"]=(double)CAM_PCNT.last_pcnt;
      jP["dev_pulses"]=(uint32_t)CAM_PULSE_N;
    }
    {
      JsonObject jL=retdoc.createNestedObject("report_latency");
      jL["repeat"]=REP_REPEAT_N;
      jL["repeat_diff"]=REP_REPEAT_DIFF_N;
      jL["repeat_worse"]=REP_REPEAT_WORSE_N;
      jL["n"]=REP_LAT_N;
      jL["avg_us"]=REP_LAT_N ? (uint32_t)(REP_LAT_SUM_US/REP_LAT_N) : 0;
      jL["max_us"]=REP_LAT_MAX_US;
      // From the camera trigger: the electronics alone, directly comparable to
      // the CAM->SWITCH budget. avg_us above is NOT.
      jL["cam_n"]=REP_CAMLAT_N;
      jL["cam_avg_us"]=REP_CAMLAT_N ? (uint32_t)(REP_CAMLAT_SUM_US/REP_CAMLAT_N) : 0;
      jL["cam_max_us"]=REP_CAMLAT_MAX_US;
    }

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"report")==0)
  {
    int tid=(doc["tid"].is<int>()==true)?doc["tid"]:-1;
    int cat=(doc["cat"].is<int>()==true)?doc["cat"]:-1;

    // A verdict is one of four values. Anything else is refused here rather
    // than written into insp_status and discovered at SWITCH.
    //
    // Two reasons it has to be checked, and neither is hypothetical:
    //
    //  - insp_status carries negative sentinels (SKIP -2100, UNSET -2000,
    //    DEL -1000) and the repeat guard keeps the MOST severe by `cat <
    //    current`. A cat below -1000 therefore beats DEL and lands on an object
    //    that has already passed the selector. Retirement no longer depends on
    //    that field (see pipeLineInfo::retired), so it can no longer wedge the
    //    drain -- but writing a host value over a finished object's verdict is
    //    still nonsense and there is no reason to allow it.
    //  - An unrecognised value reaches `default:` in the SWITCH dispatch, which
    //    counts it as UNANSWERED and raises OBJECT_HAS_NO_INSP_RESULT: "object
    //    reached SWITCH with no verdict". That is false -- a verdict arrived
    //    and was unusable -- and it points the investigation at latency and
    //    pairing, which at ~1% margin is exactly where it will find nothing.
    //    A stale machine_setting.json with cat_ok:0 produces a stream of these.
    //
    // -1 stays legal: it is this handler's own "no tid/cat supplied" sentinel
    // for the paths that carry only a timestamp.
    const bool cat_ok = (cat==-1 || cat==1 || cat==2 || cat==3 || cat==0xFFFF);
    if(!cat_ok)
    {
      // Answered, unlike every other outcome of this handler. `report` ends
      // with doRsp=false, so a host that sends something unusable normally
      // hears nothing at all and finds out when the machine halts. An early
      // `return` here would have reproduced exactly that.
      retdoc["type"]="report";
      retdoc["err"]="bad_cat";
      retdoc["cat"]=cat;
      djrl.dbg_printf("REPORT REFUSED: cat=%d is not 1/2/3/65535",cat);
    }
    else
    {
    // The camera's own timestamp for the frame this verdict came from. The host
    // knows this without knowing anything else, which is the point: it stops
    // having to work out WHICH object it inspected.
    uint64_t cam_ts = 0;
    if(doc["cam_ts"].is<uint64_t>()==true) cam_ts=doc["cam_ts"];
    else if(doc["cam_ts"].is<double>()==true) cam_ts=(uint64_t)(double)doc["cam_ts"];

    // The camera's own trigger counter for this frame, decoded by the host out
    // of the frame watermark. Absent (-1) whenever the watermark is off, which
    // is the default -- so every path below has to work without it.
    int64_t pcnt = -1;
    if(doc["pcnt"].is<uint32_t>()==true) pcnt=(int64_t)(uint32_t)doc["pcnt"];
    else if(doc["pcnt"].is<int>()==true) pcnt=(int64_t)(int)doc["pcnt"];
    else if(doc["pcnt"].is<double>()==true) pcnt=(int64_t)(double)doc["pcnt"];
    if(pcnt>=0) CAM_PCNT.last_pcnt=pcnt;

    pipeLineInfo *tarP=NULL;

    // --- find the object, both ways, before touching anything ------------
    //
    // Separated from the SKIP sweep below on purpose. The old code found and
    // swept in one pass, which meant the sweep depended on which object it
    // happened to find first -- fine when there was only one way to look, wrong
    // now that there are two and they can disagree.
    pipeLineInfo *byTid=NULL;
    pipeLineInfo *byTs=NULL;
    // The nearest candidate regardless of the window. byTs is that same object
    // only when it is close enough to be believed; `nearest` is kept separately
    // because "the best we could do, and it was not good enough" is exactly the
    // signal that stops the machine, and it is invisible if the search itself
    // discards anything outside the window.
    pipeLineInfo *nearest=NULL;
    int64_t nearestDelta=0;
    int64_t bestDelta=0;
    // The one outstanding calibration pulse -- the tid-free way to identify a
    // sync sample.
    //
    // syncPulseService fires strictly one at a time and refuses to fire at all
    // while any non-sync object is in RBuf, so during CAL/RECAL "the sync object
    // still awaiting an answer" is unique by construction. That is what makes it
    // a valid substitute for the tid lookup, and it is the only identification
    // that survives PERIF_CORE_PAIRING 0 (see PerifTriggerPairing.hpp) -- with
    // the core no longer assigning tids, byTid is permanently NULL and the
    // bootstrap would otherwise have nothing to match against.
    //
    // Counted rather than taken on first hit: if the invariant is ever broken
    // the honest answer is "I cannot tell", not "here, have the oldest one". A
    // wrong sample here poisons the offset that everything downstream trusts.
    pipeLineInfo *bySync=NULL;
    int syncOutstanding=0;
    // Exact arithmetic, no window: the object whose trigger was the one the
    // camera counted. NULL whenever the offset has not been learned or the
    // report carries no pcnt, which is what keeps this inert until it is
    // deliberately switched on at both ends.
    pipeLineInfo *byPcnt=NULL;
    if(cat!=-1)
    {
      int64_t want = CAM_SYNC.valid ? CAM_SYNC.expectedCamUs(cam_ts) : 0;
      for (int i = 0; i < RBuf.size(); i++)
      {
        pipeLineInfo *pipe = RBuf.getTail(i);
        if (pipe == NULL) break;
        if(tid!=-1 && pipe->tid==(uint32_t)tid) byTid=pipe;
        if(pipe->sync && pipe->insp_status==insp_status_UNSET)
        { bySync=pipe; syncOutstanding++; }
        if(pcnt>=0 && CAM_PCNT.valid && pipe->cam_pcnt!=0 &&
           (int64_t)pipe->cam_pcnt == pcnt - CAM_PCNT.offset) byPcnt=pipe;
        if(cam_ts!=0 && CAM_SYNC.valid && pipe->cam_us!=0)
        {
          int64_t d = (int64_t)pipe->cam_us - want; if(d<0) d=-d;
          if(nearest==NULL || d<nearestDelta){ nearest=pipe; nearestDelta=d; }
        }
      }
      if(nearest!=NULL && nearestDelta<=CamClockSync::TOL_US)
      { byTs=nearest; bestDelta=nearestDelta; }
      if(syncOutstanding!=1) bySync=NULL;
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
    //
    // tid still wins while it is there, so this change cannot alter today's
    // behaviour: with a tid the teacher is byTid exactly as before, and bySync
    // is only consulted when the tid is absent. Note byTid must still be a sync
    // object to teach -- an ordinary part carrying a tid teaches nothing, which
    // is the circularity guard, not a lookup detail.
    pipeLineInfo *teach = (byTid!=NULL) ? (byTid->sync ? byTid : NULL) : bySync;
    if(teach!=NULL && cam_ts!=0) CAM_SYNC.observe(cam_ts, teach->cam_us);
    // Same teacher, same reason: only a pulse fired with nothing else
    // outstanding gives a pairing that is certain without already knowing the
    // offset. Unlike the clock, this learns exactly once and then only checks.
    if(teach!=NULL && pcnt>=0) CAM_PCNT.observe(pcnt, teach->cam_pcnt);

    // Once the offset exists, every report maintains it: nearest object inside
    // the window re-measures it, outside the window twice running stops the
    // machine.
    // A frame belonging to a retired calibration pulse must not reach the gate
    // either. Its nearest surviving object is whatever real part happens to be
    // in the machine, which is arbitrarily far away -- that counts as a miss,
    // and two in a row halt on CAM_CLOCK_LOST. The frame is accounted for; it
    // is simply not evidence about the clock.
    const bool retired_sync = (byTid==NULL && byTs==NULL && bySync==NULL &&
                               syncTombMatches(cam_ts));

    if(cam_ts!=0 && cat!=-1 && nearest!=NULL && !retired_sync)
      CAM_SYNC.gate(cam_ts, nearest->cam_us, nearestDelta);

    if(CAM_SYNC.fault_pending)
    {
      // Stop rather than guess. A halt an operator can see is recoverable; a
      // part quietly sorted into the wrong bin is not.
      CAM_SYNC.fault_pending=false;
      djrl.dbg_printf("CAMSYNC LOST: %u consecutive frames whose nearest object "
                      "was outside the window (last delta=%lld us, tol=%lld us)",
                      (unsigned)CamClockSync::LOST_N,
                      (long long)nearestDelta,(long long)CamClockSync::TOL_US);
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                         (int)GEN_ERROR_CODE::CAM_CLOCK_LOST);
    }

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
    // bySync is the last resort, and it only ever fires during CAL/RECAL: in
    // READY no sync pulse is outstanding, so bySync is NULL and this reduces to
    // the previous expression exactly. It matters because calibration runs
    // BEFORE the clock exists -- byTs is necessarily NULL there -- so without
    // it a tid-free calibration report would match no object at all and raise
    // INSP_RESULT_MATCHES_NO_OBJECT on every single pulse.
    // Which device-side mechanisms are switched on, and what they decided.
    //
    // Three configurations, and the middle one is the reason this exists:
    //
    //   ts only    -- as before.
    //   pcnt only  -- exact matching, no window, but blind to a refused
    //                 trigger from the moment one happens.
    //   both       -- each covers the other's blind spot. A clock mispairing
    //                 is continuous and can look plausible; a count mispairing
    //                 is a whole-pulse step. For them to agree on the wrong
    //                 object, the clock would have to be off by exactly the
    //                 same object the count is off by, which is not a way
    //                 either of them fails.
    //
    // "One found an object and the other did not" is a disagreement, not a
    // fallback. The whole value of running two is refusing to sort a part that
    // only one of them can identify -- accepting the survivor would give
    // exactly the single-mechanism confidence the dual mode was turned on to
    // avoid. Both silent is different and NOT a conflict: that is simply a
    // report neither can place yet (during CAL, before either offset exists),
    // and it falls through to bySync/byTid below as it always did.
    bool pairConflict=false;
    pipeLineInfo *byDev=NULL;
    if(REPORT_MATCH_TS && REPORT_MATCH_PCNT)
    {
      if(byTs!=NULL && byPcnt!=NULL)
      {
        if(byTs==byPcnt){ CAM_PCNT.hit++; byDev=byTs; }
        else { CAM_PCNT.miss++; CAM_PCNT.mismatch++; pairConflict=true; }
      }
      else if(byTs!=NULL || byPcnt!=NULL)
      {
        CAM_PCNT.miss++;
        if(byPcnt!=NULL) CAM_PCNT.ts_blind++;   // count placed it, clock did not
        else             CAM_PCNT.pcnt_blind++; // clock placed it, count did not
        pairConflict=true;
      }
    }
    else if(REPORT_MATCH_TS)   byDev = byTs;
    else if(REPORT_MATCH_PCNT)
    {
      byDev = byPcnt;
      if(byPcnt!=NULL)                     CAM_PCNT.hit++;
      else if(CAM_PCNT.valid && pcnt>=0)   CAM_PCNT.miss++;
    }

    if(pairConflict)
    {
      djrl.dbg_printf("CAMPAIR DISAGREE tid_ts=%d tid_pcnt=%d pcnt=%lld "
                      "off=%lld d=%lld slip=%u",
                      byTs!=NULL?(int)byTs->tid:-1,
                      byPcnt!=NULL?(int)byPcnt->tid:-1,
                      (long long)pcnt,(long long)CAM_PCNT.offset,
                      (long long)bestDelta,(unsigned)CAM_PCNT.slip);
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                         (int)GEN_ERROR_CODE::CAM_PAIRING_DISAGREE);
    }

    tarP = pairConflict ? NULL
         : (byDev!=NULL ? byDev
         : (byTid!=NULL ? byTid
         : (byTs!=NULL ? byTs : (byPcnt!=NULL ? byPcnt : bySync))));

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
        const int64_t now64=esp_timer_get_time();
        uint32_t lat=(uint32_t)now64-tarP->trig_us;
        REP_LAT_N++;
        REP_LAT_SUM_US+=lat;
        if(lat>REP_LAT_MAX_US)REP_LAT_MAX_US=lat;
        // cam_us is zero until this object's CAM stage has fired. A report can
        // arrive for an object whose camera pulse never went out (a retired
        // calibration slot, a frame paired by timestamp alone), and counting
        // those would put the whole uptime into the average.
        if(tarP->cam_us!=0 && now64>(int64_t)tarP->cam_us)
        {
          uint32_t clat=(uint32_t)(now64-(int64_t)tarP->cam_us);
          REP_CAMLAT_N++;
          REP_CAMLAT_SUM_US+=clat;
          if(clat>REP_CAMLAT_MAX_US)REP_CAMLAT_MAX_US=clat;
        }
      }
      // A second report for an object that already has a verdict: keep the
      // WORSE one.
      //
      // cat is the severity class and SMALLER IS WORSE -- SEL1 is the most
      // severe reject, the last selector is OK. PERIF_CAT_NA is 0xFFFF, larger
      // than any selector, so "no verdict" is automatically the least severe
      // and a real verdict beats it without a special case.
      //
      // This was an unconditional overwrite, i.e. last writer wins, and that
      // has exactly one bad direction: an NG overwritten by a later OK lets a
      // defective part through the selectors, while the reverse only costs air.
      // The same asymmetry the stage pulse widths are rounded up for -- too
      // long loses nothing, too short loses a part.
      //
      // It cannot currently happen: the camera free-runs at ~70 fps, a frame
      // every 14.3 ms, against a 2*TOL_US = 10 ms window, so no two frames land
      // in one object's window. It becomes reachable above 100 fps -- the same
      // 100 that bounds the object rate, for the same reason (nothing may be
      // closer than 2*TOL_US to anything else). A small ROI gets there.
      //
      // UNSET and SKIP are negative sentinels, so they must not be fed to the
      // comparison; both mean "no verdict yet" and any real verdict replaces
      // them, exactly as before.
      const bool had_verdict = (tarP->insp_status!=insp_status_UNSET &&
                                tarP->insp_status!=insp_status_SKIP);
      if(!had_verdict)
      {
        tarP->insp_status=cat;
      }
      else
      {
        // Counted whether or not it changes anything. An overwrite left no
        // trace at all before this, which is why nothing could be said about
        // how often it happens -- the one blind spot left after 2026-08-07,
        // and the only one of that day's findings with no instrument.
        REP_REPEAT_N++;
        if(cat!=tarP->insp_status) REP_REPEAT_DIFF_N++;
        if(cat<tarP->insp_status)
        {
          REP_REPEAT_WORSE_N++;
          tarP->insp_status=cat;
        }
      }
      // Which verdict landed on which object, in the order the device applied
      // them. The APPLIED value, not the received one -- with worst-wins those
      // can differ, and what a slip check needs to know is what the machine
      // actually did. REP_REPEAT_DIFF_N is what explains a difference.
      //
      // Counters cannot show a slip. If the pairing is off by one, SEL1 and
      // SEL2 still come out roughly 50/50 and every part still gets an answer
      // -- the totals are identical to a correct run. The only thing that
      // changes is WHICH object each verdict went to, so that is what has to be
      // recorded. Feed the machine a verdict pattern keyed on the object id
      // (INSP_PERIF_VERDICT_PATTERN in the core) and a slip shows up here as
      // the block boundary sitting on the wrong tid.
      VERD_LOG[VERD_W].tid = tarP->tid;
      VERD_LOG[VERD_W].cat = tarP->insp_status;
      VERD_W = (uint16_t)((VERD_W+1)%VERD_LOG_N);
      if(VERD_N<VERD_LOG_N) VERD_N++;
      rspAck=true;
    }
    else if(retired_sync)
    {
      // Known-harmless: our own calibration pulse, answered after its object
      // was retired. Nothing to judge and nothing to learn -- just do not
      // pretend the pairing broke.
      SYNC_TOMB_HITS++;
      rspAck=true;
    }
    else if(pairConflict)
    {
      // Already stopped, with the reason that is actually true. Falling
      // through to the NOMATCH branch would halt a second time under
      // INSP_RESULT_MATCHES_NO_OBJECT -- "no object was found", when in fact
      // two were and they contradicted each other.
      rspAck=false;
    }
    else
    {
      int rb_real=0, rb_sync=0;
      for(int i=0;i<RBuf.size();i++)
      {
        pipeLineInfo *p=RBuf.getTail(i);
        if(p==NULL) break;
        if(p->sync) rb_sync++; else rb_real++;
      }
      djrl.dbg_printf("NOMATCH state=%d tid=%d cam_ts=%llu valid=%d "
                      "nearest=%d nd=%lld rb_real=%d rb_sync=%d syncOut=%d",
                      (int)sysinfo.state, tid, (unsigned long long)cam_ts,
                      (int)CAM_SYNC.valid, nearest!=NULL?1:0,
                      (long long)nearestDelta, rb_real, rb_sync, syncOutstanding);
      SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_MATCHES_NO_OBJECT);
      rspAck=false;
    }







    // Answer. This used to be doRsp=false unconditionally, so a host learned
    // that its verdict had matched nothing only when the machine halted, and
    // correlating WHICH report caused it was guesswork. The reply is small --
    // the id, the ack, and on failure the reason.
    doRsp=true;
    }                       // end of the cat_ok body
    if(!cat_ok){ doRsp=true; rspAck=false; }

  }

  else if(strcmp(type,"clear_error")==0)
  {
    RESET_ALL_PIPELINE_QUEUE(); 

    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);

    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"get_verdict_log")==0)
  {
    // Oldest first, so the sequence reads in the order the machine applied it.
    JsonArray at = retdoc["tid"].to<JsonArray>();
    JsonArray ac = retdoc["cat"].to<JsonArray>();
    int start = (VERD_N==VERD_LOG_N) ? VERD_W : 0;
    for(int k=0;k<VERD_N;k++)
    {
      const VerdRec &r = VERD_LOG[(start+k)%VERD_LOG_N];
      at.add(r.tid);
      ac.add(r.cat);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"clear_verdict_log")==0)
  {
    VERD_W=0; VERD_N=0;
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
    // Raw pin access takes any GPIO number the caller sends, and SEL1 is 25
    // while STEPPER_EN is 13 (HardwareConfig.hpp). In READY that means an
    // arbitrary actuator fired at an arbitrary plate position, or the driver
    // de-energised at speed. `light` already checks this; these did not.
    if(cfgPersistDeny()!=NULL)
    {
      retdoc["type"]="pin_on";
      retdoc["err"]=cfgPersistDeny();
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
    }
    else {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      digitalWrite(pin,HIGH);
    }
    doRsp=rspAck=true;
    }
  }
  else if(strcmp(type,"pin_off")==0)
  {
    // Raw pin access takes any GPIO number the caller sends, and SEL1 is 25
    // while STEPPER_EN is 13 (HardwareConfig.hpp). In READY that means an
    // arbitrary actuator fired at an arbitrary plate position, or the driver
    // de-energised at speed. `light` already checks this; these did not.
    if(cfgPersistDeny()!=NULL)
    {
      retdoc["type"]="pin_off";
      retdoc["err"]=cfgPersistDeny();
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
    }
    else {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];

      digitalWrite(pin,LOW);
    }
    doRsp=rspAck=true;
    }
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
    // Refuse at zero speed. CAL completes normally on a stationary plate (it
    // no longer needs motion), and SPINUP's test is `SYS_FREQ_STABLE &&
    // CURRENT == SETPOINT`, which 0 == 0 satisfies instantly -- so the machine
    // reaches READY and READY's enter block turns the FEEDER ON. With
    // PLATE_FREQ_CURRENT at 0 the timer alarm is disabled, so GateSensing never
    // runs: no detection, no gate counters, no error, and the feeder quietly
    // piling parts onto a plate that is not moving, reporting state 101.
    //
    // And that is the state every stop leaves behind, because stopping must
    // write plate_freq:0 (CAVEATS B). The WebUI refuses to start from it; the
    // firmware did not, so any script or a mis-fired RUN barrier got here.
    if(PLATE_FREQ_SETPOINT<=0.0f)
    {
      retdoc["type"]="enter_insp_mode";
      retdoc["err"]="plate_freq_is_zero";
      retdoc["hint"]="set plate.freq before entering inspection mode";
      doRsp=true; rspAck=false;
    }
    else
    {
      SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
      doRsp=rspAck=true;
    }
  }
  else if(strcmp(type,"exit_insp_mode")==0)
  {

    SYS_STATE_Transfer(SYS_STATE_ACT::EXIT_INSPECTION_MODE);
    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"reboot_bootloader")==0)
  {
    // crash_test and wdt_test both require confirm:true; this one ends in
    // RTC_CNTL_SW_SYS_RST and did not. Mid-run it resets the chip with the
    // plate at speed and the outputs in whatever state the ISR left them.
    if(!(doc["confirm"].is<bool>() && doc["confirm"].as<bool>()))
    {
      retdoc["type"]="reboot_bootloader";
      retdoc["err"]="confirm_required";
      doRsp=true; rspAck=false;
      goto reboot_bl_done;
    }
    if(cfgPersistDeny()!=NULL)
    {
      retdoc["type"]="reboot_bootloader";
      retdoc["err"]=cfgPersistDeny();
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
      goto reboot_bl_done;
    }
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
    }
    reboot_bl_done:
    ;
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

  // A virtual object train paced by PLATE POSITION rather than by time.
  //
  //   {"type":"virt_pulse","period_ticks":N,"jitter_ticks":J}   N=0 stops it
  //
  // Spacing is a distance, so it holds across a speed change and across speed
  // jitter -- unlike trig_phantom_train, whose interval is a duration and
  // therefore drifts against the plate the moment the speed moves. Real parts
  // are registered at a position; this is the only injector that behaves the
  // same way.
  //
  // Ticks, not millimetres, on purpose: the tick IS the machine's unit of
  // position (gate_pulse, the stage offsets, SWITCH). Converting from mm here
  // would bake in the plate geometry at the wrong layer, and the tick<->mm
  // relation is exactly what V-31 was ambiguous about until it was measured.
  //   period_ticks at plate_freq f spans period_ticks/(2f) seconds.
  //
  // jitter_ticks defaults to a tenth of the period rather than to zero: a
  // perfectly even train is the one traffic pattern in which an off-by-N
  // pairing is invisible, because every object sits at the same offset from
  // its neighbour. Ask for 0 explicitly if that degeneracy is the point.
  else if(strcmp(type,"trig_report")==0)
  {
    retdoc["type"]="trig_report";
    if(doc["on"].is<bool>()==true) TRIG_REPORT_ON = (bool)doc["on"];
    retdoc["on"]=(bool)TRIG_REPORT_ON;
    retdoc["suppressed"]=TRIG_REPORT_SUPPRESSED;
    doRsp=true;
  }
  else if(strcmp(type,"virt_pulse")==0)
  {
    retdoc["type"]="virt_pulse";
    retdoc["prev_emitted"]=VIRT_EMIT_N;
    retdoc["prev_dropped"]=VIRT_DROP_N;

    uint32_t p_ticks = doc["period_ticks"].is<unsigned int>()
                     ? (uint32_t)doc["period_ticks"] : 0;
    uint32_t j_ticks = doc["jitter_ticks"].is<unsigned int>()
                     ? (uint32_t)doc["jitter_ticks"] : (p_ticks/10);
    if(j_ticks >= p_ticks && p_ticks) j_ticks = p_ticks - 1;  // keep step >= 1

    VIRT_EMIT_N=0; VIRT_DROP_N=0;
    VIRT_JITTER_TICKS = j_ticks;
    VIRT_PERIOD_TICKS = p_ticks;      // last: arms the ISR

    retdoc["period_ticks"]=p_ticks;
    retdoc["jitter_ticks"]=j_ticks;
    retdoc["plate_freq"]=PLATE_FREQ_SETPOINT;
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

    // jitter_us: +-noise on each interval. Clamped below the period so the
    // schedule cannot invert; a seed makes the sequence replayable.
    int jitter_us=doc["jitter_us"].is<int>() ? (int)doc["jitter_us"] : 0;
    if(jitter_us<0) jitter_us=0;
    if(jitter_us>period_us-1) jitter_us=period_us-1;
    uint32_t seed=doc["seed"].is<unsigned int>() ? (uint32_t)doc["seed"] :
                  (doc["seed"].is<int>() ? (uint32_t)(int)doc["seed"] : 1u);
    if(seed==0) seed=1;

    PH_TRAIN_EMITTED=0; PH_TRAIN_PREV_US=0;
    PH_TRAIN_MIN_US=0;  PH_TRAIN_MAX_US=0;
    PH_TRAIN_PERIOD_US=period_us;
    PH_TRAIN_JITTER_US=jitter_us;
    PH_TRAIN_RNG=seed;
    PH_TRAIN_NEXT_US=esp_timer_get_time();
    PH_TRAIN_LEFT=count;

    retdoc["count"]=count;
    retdoc["period_us"]=period_us;
    retdoc["jitter_us"]=jitter_us;
    retdoc["seed"]=seed;
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"set_sel1_cd")==0)
  {
    
    // A missing or non-integer count used to mean 0, and 0 is falsy in the
    // ACT_SEL1 guard -- so a mistyped command turned the reject station off for
    // the rest of the boot. Nothing restored it: not clear_error, not RESET,
    // not re-entering inspection mode. And because "not blown" means recirculate
    // on this machine, the symptom is not a mis-sort but every NG part going
    // round forever, throughput collapsing with nothing saying why.
    //
    // -1 is the documented "unlimited". Refuse the malformed form instead of
    // interpreting it.
    if(doc["count"].is<int>()==true)
    {
      SEL1_ACT_COUNTDOWN=doc["count"];
      retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;
      doRsp=rspAck=true;
    }
    else
    {
      retdoc["err"]="count_required";
      retdoc["hint"]="integer; -1 = unlimited";
      retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;
      doRsp=true; rspAck=false;
    }
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
    // De-energising the driver at speed frees a spinning plate, and leaves
    // PLATE_FREQ_TARGET non-zero so the ramp still believes it is driving.
    // Every SEL actuation is also gated on !SYS_STEPPER_DISABLED, so parts
    // already past SWITCH with a verdict stop being ejected -- silently, since
    // the counters only move when the actuation happens.
    if(cfgPersistDeny()!=NULL)
    {
      retdoc["type"]="stepper_disable";
      retdoc["err"]=cfgPersistDeny();
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
    }
    else
    {
      digitalWrite(STEPPER_EN_PIN,!stepper_en_active);
      SYS_STEPPER_DISABLED=true;
      doRsp=rspAck=true;
    }
  }



  else if(strcmp(type,"sel_act")==0)
  {
    // Blocking the main loop is only safe with the plate stopped, which is the
    // same condition an NVS save uses and the same one trig_cam_burst and
    // `light` already check. This one did not: in READY it blocks the loop that
    // drains ISRTrigQ and services report, so at 39/s with ~1% latency margin
    // even 200ms pushes parts past SWITCH unanswered, and 32 entries of
    // ISRTrigQ overflow into INSP_CAM_TRIG_INFO_CANNOT_BE_SENT. It also fires
    // the selector at whatever happens to be under it.
    const char* deny_sel=cfgPersistDeny();
    if(deny_sel!=NULL)
    {
      retdoc["type"]="sel_act";
      retdoc["err"]=deny_sel;
      retdoc["state"]=(int)sysinfo.state;
      doRsp=true; rspAck=false;
      goto sel_act_done;
    }
    {
    // `idx` was read without is<int>(), so an absent field gave 0 and fell
    // through the switch silently. `delay` had no upper bound at all --
    // delay(600000) is ten minutes with the loop stopped, and delay() takes
    // uint32_t, so a negative became ~49 days and starved the task watchdog
    // (vTaskDelay yields but does not call esp_task_wdt_reset, and
    // firmwareLoop's reset is out of reach), i.e. panic and reboot.
    int idx = doc["idx"].is<int>() ? (int)doc["idx"] : 0;
    int delay_ms=10;

    if(doc["delay"].is<int>()==true)
    {
      delay_ms=doc["delay"];
    }
    if(delay_ms<0)    delay_ms=0;
    if(delay_ms>2000) delay_ms=2000;   // far past any real blow, still bounded

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
      default:
      retdoc["err"]="idx_must_be_1_2_or_3";
      rspAck=false;
      break;
    }
    retdoc["idx"]=idx;
    retdoc["delay"]=delay_ms;
    }
    sel_act_done:
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
    
    // Static, not stack: this is the largest response the device produces and
    // stack_hwm has been as low as 2500 bytes.
    static uint8_t buff[3072];
    // Never fail silently. get_running_stat outgrew a 2048 byte buffer once the
    // clock diagnostics went in, and the symptom was simply no reply at all --
    // which reads as a dead link and cost an hour of looking in the wrong place.
    // A short error is infinitely more useful than nothing.
    if(retdoc.overflowed())
    {
      // With the id, or a host that correlates replies by id (uinsp_test,
      // regress_watch) never matches this and blocks until its timeout -- so
      // the guard turned silent truncation into a silent timeout plus an
      // unattributable error line.
      char e[96];
      snprintf(e,sizeof(e),"{\"err\":\"stat_doc_overflow\",\"id\":%d,\"ack\":false}",
               (int)HACK_cur_cmd_id);
      send_json_string(0,(uint8_t*)e,strlen(e),0);
      return 0;
    }
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    if(slen<=0 || slen>=(int)sizeof(buff))
    {
      const char *e="{\"err\":\"stat_buf_overflow\"}";
      send_json_string(0,(uint8_t*)e,strlen(e),0);
      return 0;
    }
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
          break;
          // case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_ENC :
          //   while(mstp.EncV<info.wait_enc.value)
          //   {
          //   }

      
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
            // }
            // if(info.ioCtrl.state==0)
            // {
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
  // A TX buffer, for the same reason the RX one is here: without it
  // Serial.write BLOCKS until the last byte is on the wire. The announce drain
  // is one cam_trig per pass at 105 bytes = 4.6ms at 230400, so clearing a
  // queue of 32 costs the main loop 147ms -- measured, as SEG_TX_US, and it is
  // the whole of the worst loop pass (svc and st are 0.0ms). Meanwhile the
  // step ISR keeps pushing into that same 32-entry queue, and when it wins the
  // race the machine stops with INSP_CAM_TRIG_INFO_CANNOT_BE_SENT.
  //
  // With a buffer the drain hands bytes to the UART ISR and returns, so the
  // queue empties at memcpy speed instead of at wire speed. The wire is not
  // the constraint: at 30 objects/s the announcements are ~13% of 230400.
  // 4096 holds ~39 announcements, more than the ISRTrigQ can ever contain.
  Serial.setTxBufferSize(4096);
  // 230400. Raised 2026-08-07 -- and NOT because the link was congested: at
  // 10 objects/s the wire ran at 969 B/s against 11520 B/s of capacity (8.4%)
  // and a `pong` answered in 9.6 ms. The 1.4-2.9 s figures quoted from the
  // perif log were `now - last_tx_us` on UNSOLICITED announcements, which is
  // not a round trip at all (CORE0_1_CAVEATS J14).
  //
  // What it does buy is headroom for the rates being tested next: 30 objects/s
  // triples the announcement traffic to ~25% of 115200, and the margin for a
  // burst is what disappears first.
  //
  // Every host that opens this port must match: the CONN dicts in
  // tools/regress_watch.py and tools/slip_probe.py, and whatever the WebUI
  // sends in its CONNECT.
  // Measured 2026-08-08, and the reason it is still 230400: raising this to
  // 921600 (the classic CP2102's ceiling) moves the trigger->verdict latency
  // from 40.7ms to 32.7ms and leaves `ping` at 44ms, UNCHANGED. The round trip
  // is ~44ms of fixed overhead plus the bytes; the verdict path is only 165
  // bytes, so there is barely any bytes to win. Baud is not the latency knob.
  // What it does halve is big replies -- get_setup went 97.6ms -> 45.0ms --
  // and that is a head-of-line problem better fixed by making the reply small
  // than by making the wire fast.
  Serial.begin(230400);
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
// How fast the main loop actually turns, and its worst single pass.
//
// This is the loop that drains ISRTrigQ, one entry per pass, so its period IS
// the drain rate. Announcements were sitting in that queue for ~350ms while
// the wire could have carried each in 4.6ms, and there was no way to tell a
// slow loop from a blocked write from a queue that simply is not being
// serviced -- because nothing measured the loop.
volatile uint32_t LOOP_N=0;
volatile uint32_t LOOP_MAX_US=0;

// Where the worst pass spends its time. LOOP_MAX_US said 160ms and nothing
// more; a number with no address is a number you can only guess about, and
// the last guess (big replies blocking the drain) cost a full run to disprove.
// Four segments, worst case each, in the order the loop runs them:
//   svc   the periodic services (syncPulse/spinup/recal/phantomTrain)
//   st    the state machine pass
//   rx    read serial AND handle the command -- the reply is serialized and
//         written here, so a 1174-byte get_setup lands in this segment
//   tx    the announce drain: ISRTrigQ first, then the other queues
volatile uint32_t SEG_SVC_US=0, SEG_ST_US=0, SEG_RX_US=0, SEG_TX_US=0;
#define SEG_BEGIN() uint32_t _seg_t0=(uint32_t)esp_timer_get_time()
#define SEG_END(V) do{ uint32_t _d=(uint32_t)esp_timer_get_time()-_seg_t0; \
                       if(_d>(V)) (V)=_d; }while(0)

void firmwareLoop()
{
  {
    static uint32_t loop_last_us=0;
    uint32_t now=(uint32_t)esp_timer_get_time();
    if(loop_last_us)
    {
      uint32_t d=now-loop_last_us;
      if(d>LOOP_MAX_US) LOOP_MAX_US=d;
    }
    loop_last_us=now;
    LOOP_N++;
  }
  esp_task_wdt_reset();
  { SEG_BEGIN();
  syncPulseService();
  spinupService();
  recalService();
  phantomTrainService();
  SEG_END(SEG_SVC_US); }
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

  { SEG_BEGIN();
  SYS_STATE_Transfer(SYS_STATE_ACT::NOP);
  djrl.loop();
  SEG_END(SEG_ST_US); }
  
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
    SEG_BEGIN();
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      size_t recvLen = Serial.read(recvBuf,sizeof(recvBuf));
      //
      if(recvLen==0)continue;
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
    }
    SEG_END(SEG_RX_US);
  }


  {
    SEG_BEGIN();
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

        // Consume the queue either way: it is the queue's OVERFLOW that stops
        // the machine (INSP_CAM_TRIG_INFO_CANNOT_BE_SENT), so suppressing the
        // send must not also suppress the drain.
        if(!TRIG_REPORT_ON)
        {
          TRIG_REPORT_SUPPRESSED++;
          continue;
        }

        retdoc.clear();
        retdoc["type"]="cam_trig";
        retdoc["q"]=EVENT_SEQ++;
        retdoc["tid"]=trig.trig_id;
        retdoc["cam"]=trig.btrig_idx;
        retdoc["t_us"]=trig.trig_time_us;
        retdoc["gate_pulse"]=trig.gate_pulse;
        retdoc["w"]=gateWidthOf(trig.trig_id);
        retdoc["Qs"]=RBuf.size();
        if(trig.sync) retdoc["sync"]=1;   // omitted for parts: it is the common case
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
      switch (info.type)
      {
        case TaskQ2CommInfo_Type::trigInfo :
        {
          retdoc["type"]="cam_trig_tagged"; 
          retdoc["camera_id"]=info.camera_id;


          string tag = info.trig_tag;
          // if(info.curFreq==info.curFreq)

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
          retdoc["w"]=gateWidthOf(info.trig_id);
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
    SEG_END(SEG_TX_US);
  }



  static int subDiv=0;
  static int64_t lastRampUs=0;
  do{//timer freq ctrl
    subDiv=(subDiv+1)&(0xFF);
    if(subDiv!=0)break;
    int64_t nowUs=esp_timer_get_time();
    float dt=(nowUs-lastRampUs)*1e-6f;
    lastRampUs=nowUs;
    {
      // Measured, not commanded. Tick rate is 2*plate_freq (StepGo emits one
      // driver pulse per two ticks), so halve it to get the same unit the
      // setpoint is in.
      static uint32_t meas_step=0;
      static int64_t  meas_us=0;
      if(meas_us!=0 && nowUs>meas_us+100000)
      {
        const uint32_t d=(uint32_t)(SYS_STEP_COUNT-meas_step);
        PLATE_FREQ_MEAS = (float)d*1e6f/(float)(nowUs-meas_us)/2.0f;
        meas_step=SYS_STEP_COUNT; meas_us=nowUs;
      }
      else if(meas_us==0){ meas_step=SYS_STEP_COUNT; meas_us=nowUs; }
    }
    // Re-derive the station windows against the speed the plate is ACTUALLY
    // running at, so a width in microseconds stays that many microseconds while
    // the speed moves. Without this the windows are a fixed arc pinned to
    // whatever speed the last set_setup happened to see.
    //
    // Here rather than in the step ISR: this does seven divides and a publish,
    // and the ISR's whole discipline is that it reads one pointer and touches
    // nothing else. The publish is a double-buffer swap, so a reader either
    // sees the old set or the new one, never a mix.
    //
    // Guarded by a relative threshold, not run every pass. Below ~0.4% the
    // derived tick counts do not change (they are rounded up to whole ticks),
    // so re-deriving would burn the divides to write back what is already
    // there. During a ramp this still fires many times; that is the point.
    {
      static float lastApplyFreq = 0.0f;
      const float f = PLATE_FREQ_CURRENT;
      // TARGET > 0, not just CURRENT > 0: a plate ramping down to a stop passes
      // through arbitrarily small speeds, and deriving there leaves every
      // window at us2t's 1-tick floor -- 50ms of blow stored as 1 tick, which
      // is 0 to anything that reads it. Observed on a real stop, so this is not
      // a hypothetical: after the plate settled, CAM1 and SEL1 both read 1 t.
      //
      // Nothing is lost by freezing: while the plate is stopped the windows
      // are not used, and the first pass of the next spin-up re-derives them.
      // The gate stays shut through that spin-up anyway -- admission needs
      // INSPECTION_MODE_READY, which is only reached at speed.
      (void)lastApplyFreq; (void)f;
      // Keep the ISR budget in integers for onTimer, which cannot do this.
      ISR_BUDGET_CY = (f > 0.0f) ? (uint32_t)(240000000.0f/(2.0f*f)) : 0;
      // Deliberately NOT re-derived here any more. See STAGE_PULSE_WIDTH_apply:
      // the windows are converted once, for the speed the plate is being sent
      // to, and the gate is what keeps the plate near that speed while parts
      // are moving. Tracking the live speed from this loop is what turned a
      // rare publish into a continuous one, and that is the interaction the
      // 2026-08-11 hang is pinned on.
    }
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
  // unsigned long currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  // if (currentMillis - startMillis >= 100)  //test whether the period has elapsed
  // {
  //   startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.

  // }



  {//clean up finished 
    pipeLineInfo * tail;
    while (tail=RBuf.getTail())
    {
      // task->src->insp_status = insp_status_DEL;
      if(tail->retired)
      {
        RBuf.consumeTail();
      }
      else
      {
        break;
      }
    }
  }

  // An exhausted countdown means the reject station has stopped ejecting, and
  // on this machine that is silent: unblown parts recirculate, so NG material
  // simply goes round and round while SEL1_Count stays flat and nothing faults.
  // SEL_ACT_LIMIT_REACHES was declared in FirmwareTypes.hpp for exactly this
  // and had never been raised by anything -- the guard was commented out.
  //
  // Only from a live inspection state, and only once: SEL1_ACT_COUNTDOWN is set
  // to -1 (unlimited) by the transition so this cannot re-fire every pass while
  // the operator is reading the error.
  if(SEL1_ACT_COUNTDOWN==0 &&
     sysinfo.state==SYS_STATE::INSPECTION_MODE_READY)
  {
    SEL1_ACT_COUNTDOWN=-1;
    SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,
                       (int)GEN_ERROR_CODE::SEL_ACT_LIMIT_REACHES);
  }
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




  {
    JsonObject jP = jdoc.createNestedObject("plate");
    jP["freq"]=PLATE_FREQ_SETPOINT;
    jP["accel"]=SYS_FREQ_ACCEL;
    jP["pulses_per_rev"]=pulses_per_rev;
    jP["diameter_mm"]=plate_diameter_mm;
    jP["stepper_en_active"]=stepper_en_active;
    jP["stepper_dir"]=stepper_dir_level;
    jP["speed_band_pct"]=SPEED_BAND_PCT;
  }
  {
    JsonObject jGT = jdoc.createNestedObject("gate");
    jGT["min_detect_sep_us"]=SYS_MIN_PULSE_TIME_SEP_us;
    jGT["pulse_min_width"]=minWidth;
    jGT["pulse_max_width"]=maxWidth;
    jGT["debounce_rise"]=DEBOUNCE_H_THRES;
    jGT["debounce_fall"]=DEBOUNCE_L_THRES;
    jGT["min_detect_dist_um"]=GATE_MIN_DIST_um;
  }
  {
    JsonObject jCM = jdoc.createNestedObject("cam");
    jCM["report_match_ts"]=REPORT_MATCH_TS;
    jCM["report_match_pcnt"]=REPORT_MATCH_PCNT;
    jCM["match_window_us"]=CamClockSync::TOL_US;
    // The setting, and what it currently BUYS in millimetres. The second one is
    // emitted whichever mode is in use, because the hazard the microsecond form
    // leaves open is precisely that raising plate_freq loosens the position
    // tolerance and nothing says so. Now something does.
    jCM["match_tolerance_mm"]=CAM_MATCH_TOL_um/1000.0f;
    {
      const double um_per_tick = (double)_PLAT_CIRC_um/(double)_PLAT_PULSE_PER_TURN;
      const double um_per_s    = um_per_tick*2.0*(double)PLATE_FREQ_SETPOINT;
      jCM["match_tolerance_mm_eff"] =
        (PLATE_FREQ_SETPOINT>0.0f)
          ? (float)(um_per_s*(double)CamClockSync::TOL_US/1e6/1000.0)
          : 0.0f;
    }
    jCM["recal_idle_ms"]=CAM_RECAL_IDLE_MS;
    jCM["cal_pulse_us"]=CAL_PULSE_WIDTH_US;
    jCM["drift_comp"]=CamClockSync::DRIFT_COMP;
  }
  // Emitted as an integer, not a double: this document IS the persisted config,
  // and JSON_SETIF_ABLE gates on is<int64_t>(). A value written back as a float
  // would fail that test on the next boot and silently revert to the default.
  {
    // Widths in microseconds. 0 = not configured, the *_off offset above rules.
    JsonObject jW = jdoc.createNestedObject("stage_pulse_width_us");
    jW["CAM1"]=STAGE_PULSE_WIDTH_US.CAM1;
    jW["L1A"] =STAGE_PULSE_WIDTH_US.L1A;
    jW["CAM2"]=STAGE_PULSE_WIDTH_US.CAM2;
    jW["L2A"] =STAGE_PULSE_WIDTH_US.L2A;
    jW["SEL1"]=STAGE_PULSE_WIDTH_US.SEL1;
    jW["SEL2"]=STAGE_PULSE_WIDTH_US.SEL2;
    jW["SEL3"]=STAGE_PULSE_WIDTH_US.SEL3;
  }
  {
    // Window centres in TICKS -- a position, so the same unit as
    // stage_pulse_offset and for the same reason: a tick is a fixed distance
    // on the plate, so the centre does not move when the speed does. 0 = the
    // station keeps the forward-only shape.
    JsonObject jC = jdoc.createNestedObject("stage_pulse_center");
    jC["CAM1"]=STAGE_PULSE_CENTER.CAM1;
    jC["L1A"] =STAGE_PULSE_CENTER.L1A;
    jC["CAM2"]=STAGE_PULSE_CENTER.CAM2;
    jC["L2A"] =STAGE_PULSE_CENTER.L2A;
    jC["SEL1"]=STAGE_PULSE_CENTER.SEL1;
    jC["SEL2"]=STAGE_PULSE_CENTER.SEL2;
    jC["SEL3"]=STAGE_PULSE_CENTER.SEL3;
  }




  // What the machine does about a part that reached the selector unjudged.
  //
  // These were three separate flat keys (auto_rate, unanswered_policy,
  // unanswered_stop_after) plus two more for tuning, and nothing said they
  // were one decision -- so the combination that reacts to nothing at all
  // (auto_rate off, policy 0: unjudged parts pass forever, silently) was
  // reachable by setting two unrelated-looking values.
  //
  // The two halves are NOT alternatives, which is why the default arms both:
  //   slow  reacts to the RATE of skips -- scattered ones mean "a bit fast",
  //         and the answer is to admit fewer parts and keep running.
  //   stop  reacts to CONSECUTIVE skips -- ten in a row means the host or the
  //         camera stopped answering, and slowing down cannot fix that; it
  //         would only pass unjudged parts more slowly.
  //
  // The only representation. The five flat keys this replaced were migrated
  // out on 2026-08-08 by persisting the group with the firmware that still
  // emitted both, then dropping them -- an NVS image older than that has no
  // skip_policy and comes up on the compiled defaults (AUTO_RATE off). This
  // machine's image was saved through the transition; backup in
  // tools/machine_config_backup_2026-08-08.json.
  {
    JsonObject jSP = jdoc.createNestedObject("skip_policy");
    jSP["mode"] = AUTO_RATE ? (UNANSWERED_POLICY==1 ? "slow_and_stop" : "slow_only")
                            : (UNANSWERED_POLICY==1 ? "stop_only"     : "none");
    jSP["stop_after"]    = UNANSWERED_STOP_AFTER;
    jSP["rate_floor_us"] = AUTO_RATE_FLOOR_us;
    jSP["recover_n"]     = AUTO_RATE_RECOVER_N;
    // Said out loud rather than refused: "none" is a legitimate thing to ask
    // for on a bench, and a machine that silently declines a setting is worse
    // than one that tells you what you chose.
    if(!AUTO_RATE && UNANSWERED_POLICY!=1) jSP["unsafe"]=true;
  }

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

  // reset_reason / xtal_mhz / error_hist / cur_state / step_count are NOT
  // here any more. This document's own contract is that it IS the persisted
  // config, and none of those are persisted or config -- they made it bigger
  // for every host that only wanted the settings, on a link where a long
  // reply does get truncated. state and step_count are on `poll` (122 bytes),
  // error_hist and the boot facts on get_running_stat.

  
}




#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}


// apply_hw=false assigns the globals and touches nothing else.
//
// MachineConfig::begin() runs BEFORE pinMode(), so driving a pin from there
// would write to an unconfigured GPIO. It does not need to: firmwareSetup rests
// every actuator at its logical OFF level right after pinMode, and that reads
// IO_INV_MASK -- so setting the variables is enough to come up correct.
// --- what a set_setup is allowed to contain ---------------------------------
//
// The parser applies what it recognises and ignores the rest, and set_setup
// acks true either way. That is how eight tools in tools/ spent a week
// configuring nothing: the setup document was regrouped into plate/gate/cam,
// their flat `{"plate_freq": 0}` stopped meaning anything, and every one of
// them was told the command had succeeded -- including the one that stops the
// plate. A 60-second run reported "=> clean" with accept=0; nothing had turned.
//
// So the schema is written down once, here, and anything not in it is named
// back to the caller. An unrecognised key is a caller that believes something
// false about the machine, and that is worth an error rather than silence.
static const char *const K_PLATE[] =
  {"freq","accel","speed_band_pct","pulses_per_rev","diameter_mm","stepper_en_active",
   "stepper_dir",NULL};
static const char *const K_GATE[] =
  {"min_detect_sep_us","pulse_min_width","pulse_max_width","debounce_rise",
   "debounce_fall","min_detect_dist_um",NULL};
static const char *const K_CAM[] =
  {"report_match_ts","report_match_pcnt","match_window_us","match_tolerance_mm",
   "match_tolerance_mm_eff","recal_idle_ms","cal_pulse_us","drift_comp",NULL};
static const char *const K_SKIP[] =
  {"mode","stop_after","rate_floor_us","recover_n","unsafe",NULL};
static const char *const K_SPO[] =
  {"L1A_on","L1A_off","CAM1_on","CAM1_off","L2A_on","L2A_off","CAM2_on",
   "CAM2_off","SWITCH","SEL1_on","SEL1_off","SEL2_on","SEL2_off","SEL3_on",
   "SEL3_off",NULL};
static const char *const K_WIDTH[] =
  {"L1A","CAM1","L2A","CAM2","SEL1","SEL2","SEL3",NULL};
// Same stations as the widths -- a centre is only meaningful beside one.
static const char *const K_CENTER[] =
  {"L1A","CAM1","L2A","CAM2","SEL1","SEL2","SEL3",NULL};
// io_on_level is keyed by IO_POL_TAB, so it is checked against that table
// rather than duplicated here -- two copies of a name list drift.
static const char *const K_TOP[] =
  {"type","id","persist",                       // command envelope, not config
   "machine_id","host_timeout_ms",
   "CAM1_ID","CAM2_ID","CAM1_Tags","CAM2_Tags",
   "cfg_from_nvs",                              // reported, harmless to echo back
   NULL};

static bool cfgKeyKnown(const char *const *tab, const char *k)
{
  for(int i=0;tab[i];i++) if(strcmp(tab[i],k)==0) return true;
  return false;
}

static void cfgNoteUnknown(const char *grp,const char *k,char *out,size_t outN,int *n)
{
  (*n)++;
  size_t used=strlen(out);
  if(used+strlen(k)+strlen(grp?grp:"")+4 >= outN) return;   // truncate, keep counting
  if(used) { strcat(out,","); }
  if(grp){ strcat(out,grp); strcat(out,"."); }
  strcat(out,k);
}

// Returns the count; `out` gets the names, comma separated, truncated if long.
int cfgUnknownKeys(JsonObject in, char *out, size_t outN)
{
  int n=0;
  out[0]='\0';
  if(in.isNull()) return 0;
  for(JsonPair kv : in)
  {
    const char *k = kv.key().c_str();
    const char *const *tab = NULL;
    if     (strcmp(k,"plate")==0)                tab=K_PLATE;
    else if(strcmp(k,"gate")==0)                 tab=K_GATE;
    else if(strcmp(k,"cam")==0)                  tab=K_CAM;
    else if(strcmp(k,"skip_policy")==0)          tab=K_SKIP;
    else if(strcmp(k,"stage_pulse_offset")==0)   tab=K_SPO;
    else if(strcmp(k,"stage_pulse_width_us")==0) tab=K_WIDTH;
    else if(strcmp(k,"stage_pulse_center")==0)   tab=K_CENTER;
    else if(strcmp(k,"io_on_level")==0)
    {
      JsonObject g = kv.value().as<JsonObject>();
      if(!g.isNull()) for(JsonPair p : g)
      {
        bool ok=false;
        for(unsigned i=0;i<sizeof(IO_POL_TAB)/sizeof(IO_POL_TAB[0]);i++)
          if(strcmp(IO_POL_TAB[i].name,p.key().c_str())==0){ ok=true; break; }
        if(!ok) cfgNoteUnknown("io_on_level",p.key().c_str(),out,outN,&n);
      }
      continue;
    }
    else
    {
      if(!cfgKeyKnown(K_TOP,k)) cfgNoteUnknown(NULL,k,out,outN,&n);
      continue;
    }
    JsonObject g = kv.value().as<JsonObject>();
    if(g.isNull()){ cfgNoteUnknown(NULL,k,out,outN,&n); continue; }  // group sent as a scalar
    for(JsonPair p : g)
      if(!cfgKeyKnown(tab,p.key().c_str()))
        cfgNoteUnknown(k,p.key().c_str(),out,outN,&n);
  }
  return n;
}

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

  // The config arrives grouped: plate / gate / cam / skip_policy /
  // stage_pulse_offset / stage_pulse_width_us / io_on_level. An absent group
  // binds to a null JsonObject, on which is<T>() is false, so every setter
  // below simply does not fire -- exactly what "key not present" did before.
  JsonObject jP  = jdoc["plate"];
  JsonObject jGT = jdoc["gate"];
  JsonObject jCM = jdoc["cam"];

  JSON_SETIF_ABLE(PLATE_FREQ_SETPOINT,jP,"freq");
  JSON_SETIF_ABLE(SYS_FREQ_ACCEL,jP,"accel");
  JSON_SETIF_ABLE(SPEED_BAND_PCT,jP,"speed_band_pct");
  if(SPEED_BAND_PCT>50) SPEED_BAND_PCT=50;   // beyond this the arcs mean nothing
  // Neither of these had any bound at all, and both are divisors or step sizes
  // in the ramp that drives the timer alarm.
  //
  //   freq -1000   the ramp converges to -1000 (the == clamp works fine, it
  //                just clamps to a negative), then
  //                (uint64_t)(5000000.0f / -1000.0f) is UB and saturates to 0
  //                on Xtensa -- timerAlarmEnable with an alarm value of 0.
  //                Meanwhile CURRENT==TARGET so SYS_FREQ_STABLE goes true and
  //                spinupService declares the machine READY, feeder on.
  //   freq 5e6     5000000/5000000 -> alarm period 1 tick: interrupt storm.
  //   accel 0      step becomes 3.4e38, so the plate goes 0 -> full speed in a
  //                single loop iteration, scattering the parts on it.
  //
  // And freq persists to NVS, so a bad value survives a power cycle.
  //
  // Clamped rather than refused: these arrive from a slider, and refusing the
  // write would leave the machine on the previous value with nothing on screen
  // to say so. The ceiling is the mechanical limit the plate has actually been
  // run at with margin, not a number from the arithmetic.
  if(PLATE_FREQ_SETPOINT < 0.0f)     PLATE_FREQ_SETPOINT = 0.0f;
  if(PLATE_FREQ_SETPOINT > 60000.0f) PLATE_FREQ_SETPOINT = 60000.0f;
  if(SYS_FREQ_ACCEL <= 0.0f)         SYS_FREQ_ACCEL = 2000.0f;
  if(SYS_FREQ_ACCEL > 100000.0f)     SYS_FREQ_ACCEL = 100000.0f;
  JSON_SETIF_ABLE(SYS_MIN_PULSE_TIME_SEP_us,jGT,"min_detect_sep_us");
  // Changing the configured rate always resets the live one: the operator asked
  // for a rate, not for whatever the loop had crept to.
  if(jGT["min_detect_sep_us"].is<int>()) GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us;
  if(GATE_SEP_EFF_us<SYS_MIN_PULSE_TIME_SEP_us)
    GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us;
  JSON_SETIF_ABLE(REPORT_MATCH_TS,jCM,"report_match_ts");
  JSON_SETIF_ABLE(REPORT_MATCH_PCNT,jCM,"report_match_pcnt");
  JSON_SETIF_ABLE(CamClockSync::TOL_US,jCM,"match_window_us");

  // "match_tolerance_mm": the window expressed as what it actually is.
  //
  // Opt-in, and the microsecond setting stays the primary one. The 2026-08-06
  // note below decided against deriving from plate speed because a window that
  // moves underneath you makes failures hard to reproduce -- "the same test at
  // two speeds stops being the same test" -- and that reasoning still holds.
  // What it also said, and what this closes, is the hazard it left open:
  // "raising plate_freq silently loosens the position tolerance, and nothing
  // says so."
  //
  // So there are now two modes and BOTH report the position tolerance:
  //   match_tolerance_mm > 0   derive the window from it and the setpoint
  //   match_tolerance_mm == 0  keep the explicit microseconds (default)
  // and genMachineSetup always emits match_tolerance_mm_eff, so a fixed
  // window can no longer loosen in millimetres without saying so.
  //
  // Rounded DOWN, and against SETPOINT rather than CURRENT. Both are the safe
  // direction here and both are the OPPOSITE of what stage_pulse_width_us does
  // (C: rounded up, so a pulse is never too short to fire) -- because the
  // asymmetry is opposite: too WIDE a window mis-sorts, too narrow only halts.
  // During spin-up CURRENT is below SETPOINT, so a window derived from SETPOINT
  // is tighter than it needs to be, which is again the safe side.
  // Accepted as millimetres and held as micrometres: JSON_SETIF_ABLE gates on
  // is<typeof(var)>(), and an int32 target silently never matches a value a
  // host wrote as 0.3 (see the float/int trap at the stage offsets).
  if(jCM["match_tolerance_mm"].is<float>())
    CAM_MATCH_TOL_um = (int32_t)(jCM["match_tolerance_mm"].as<float>()*1000.0f+0.5f);
  if(CAM_MATCH_TOL_um<0) CAM_MATCH_TOL_um=0;
  if(CAM_MATCH_TOL_um>0 && PLATE_FREQ_SETPOINT>0.0f)
  {
    const double um_per_tick = (double)_PLAT_CIRC_um/(double)_PLAT_PULSE_PER_TURN;
    const double um_per_s    = um_per_tick*2.0*(double)PLATE_FREQ_SETPOINT;
    int32_t derived = (int32_t)((double)CAM_MATCH_TOL_um*1e6/um_per_s);   // truncates = down
    if(derived<1) derived=1;                 // the floors below take it from here
    if(derived!=CamClockSync::TOL_US)
      djrl.dbg_printf("CAMSYNC window %ld -> %ld us, derived from %ld um at "
                      "plate_freq %d (%.0f mm/s)",
                      (long)CamClockSync::TOL_US,(long)derived,
                      (long)CAM_MATCH_TOL_um,(int)PLATE_FREQ_SETPOINT,um_per_s/1000.0);
    CamClockSync::TOL_US = derived;
  }

  // TODO (deferred 2026-08-06): the window is really a POSITION tolerance, and
  // expressing it in microseconds hides that.
  //
  // What the window absorbs is the camera's trigger-to-exposure latency jitter,
  // and that jitter is a real displacement of the part in the image -- unlike
  // the clock offset, which does not move anything, because the trigger fires
  // from the step ISR (Run_ACTS in onTimer, alongside StepGo) and so is locked
  // to plate POSITION, not to wall time.
  //
  // The accepted tolerance is 0.2mm. On this machine (240mm plate, 60000
  // ticks/rev -> 0.01257mm/tick) that is:
  //
  //     plate_freq 10000 (251 mm/s)  ->  796 us
  //     plate_freq 15000 (377 mm/s)  ->  531 us
  //
  // against the 5000us in use, which is 1.26mm at plate_freq 10000 -- six times
  // looser than intended. Measured residual is 152..244us (0.038..0.061mm), so
  // there is room to tighten a long way.
  //
  // The budget also has a second claimant that is invisible at 5000us: after
  // CAM_RECAL_IDLE_MS of idle the offset has drifted by up to 10s x 35us/s =
  // 350us, which is 44% of the 796us. Tightening the window without also
  // shortening the recal idle would spend most of the tolerance on drift.
  //
  // NOT deriving the window from plate speed, though that is what makes the
  // tolerance actually constant: a window that changes underneath you makes
  // failures hard to reproduce, and the same test at two speeds stops being the
  // same test. A fixed number that is occasionally too strict is easier to
  // debug than a correct one that moves. The hazard that remains, and the
  // reason this is written down rather than dropped: raising plate_freq
  // silently loosens the position tolerance, and nothing says so.
  //
  // Only a sanity floor, deliberately not a policy. A window narrower than the
  // measurement noise (~50us observed) can never match anything and would stop
  // the machine forever; above that the choice is the operator's, and
  // cam_sync.delta_max_us against gate.eff_sep_us is how it gets checked.
  if(CamClockSync::TOL_US < 200) CamClockSync::TOL_US = 200;

  // The window must stay well inside the object spacing, or a missing frame
  // stops being a halt and becomes a mis-sorted part.
  //
  // There are two separate margins here and they are easy to conflate:
  //
  //  1. The true object is present. Matching takes the NEAREST object and only
  //     then applies the window, so the neighbour wins only if it is actually
  //     closer -- spacing < 2x the clock error. At the measured ~150us error
  //     that needs spacing under 300us, against 33000us in use. Margin ~110x.
  //     This one is not in danger and the window does not govern it.
  //
  //  2. The true object is ABSENT -- its frame was lost, or it was already
  //     swept. Now the nearest candidate IS a neighbour, one full spacing away,
  //     and the window is the only thing standing between "refuse and halt" and
  //     "answer the wrong part". That requires window < spacing, and nothing
  //     enforced it: the shipped defaults were min_detect_sep_us 4000 against a
  //     5000us window, i.e. already the wrong way round. It never bit because
  //     the camera cannot service 4000us (250Hz) so the real spacing is set far
  //     wider -- the safety came from the camera's frame rate, by accident,
  //     rather than from the configuration.
  //
  // Half the spacing, so a neighbour sits at least two windows out. Clamped
  // rather than rejected: refusing the write would leave the machine on the
  // previous value with no obvious sign, and a narrower window is always the
  // safe direction -- it can only cause a halt, never a mis-sort.
  if(SYS_MIN_PULSE_TIME_SEP_us > 0)
  {
    int32_t cap = (int32_t)(SYS_MIN_PULSE_TIME_SEP_us/2);
    if(cap < 200) cap = 200;   // the noise floor wins; see below
    if(CamClockSync::TOL_US > cap)
    {
      djrl.dbg_printf("CAMSYNC window %ld us clamped to %ld us "
                      "(min_detect_sep_us=%lu, window must stay under half)",
                      (long)CamClockSync::TOL_US,(long)cap,
                      (unsigned long)SYS_MIN_PULSE_TIME_SEP_us);
      CamClockSync::TOL_US = cap;
    }
    // Below this the two floors conflict and no window is both matchable and
    // safe. Physically unreachable (400us spacing is 2500 parts/s), but say so
    // rather than leaving a silently unsafe combination.
    if(SYS_MIN_PULSE_TIME_SEP_us < 400)
      djrl.dbg_printf("CAMSYNC WARNING: min_detect_sep_us=%lu is below twice "
                      "the window noise floor -- a lost frame could be matched "
                      "to a neighbour",(unsigned long)SYS_MIN_PULSE_TIME_SEP_us);
  }
  JSON_SETIF_ABLE(CamClockSync::DRIFT_COMP,jCM,"drift_comp");
  JSON_SETIF_ABLE(CAM_RECAL_IDLE_MS,jCM,"recal_idle_ms");
  JSON_SETIF_ABLE(CAL_PULSE_WIDTH_US,jCM,"cal_pulse_us");
  // Negative is meaningless; 0 is the documented "off". A floor above that stops
  // a small positive value from recalibrating continuously and never running.
  if(CAM_RECAL_IDLE_MS < 0) CAM_RECAL_IDLE_MS = 0;
  if(CAM_RECAL_IDLE_MS > 0 && CAM_RECAL_IDLE_MS < 2000) CAM_RECAL_IDLE_MS = 2000;

  // Drift against the window. Warned, not clamped: shortening the idle costs
  // production time and lengthening the window costs position tolerance, and
  // which one the operator would rather spend is not this function's call. But
  // silence here is how a tightened window turns into "it halts after every
  // pause" with no visible reason -- the first frame after a quiet spell is
  // late by drift alone.
  if(CAM_RECAL_IDLE_MS > 0 && CamClockSync::TOL_US > 0)
  {
    const int32_t drift_us = (CAM_RECAL_IDLE_MS/1000)*CAM_DRIFT_US_PER_S;
    if(drift_us*2 > CamClockSync::TOL_US)
      djrl.dbg_printf("CAMSYNC WARNING: %ld ms idle drifts up to %ld us, which is "
                      "%ld%% of the %ld us window -- shorten recal_idle_ms or the "
                      "first part after a pause is late on drift alone",
                      (long)CAM_RECAL_IDLE_MS,(long)drift_us,
                      (long)(100L*drift_us/CamClockSync::TOL_US),
                      (long)CamClockSync::TOL_US);
  }

  JSON_SETIF_ABLE(minWidth,jGT,"pulse_min_width");
  JSON_SETIF_ABLE(maxWidth,jGT,"pulse_max_width");
  // Compared against a uint32 width, so a negative converts to ~4.29e9:
  // pulse_min_width -1 rejects every part as a width failure and counts it
  // only in GATE_REJ_WIDTH, while pulse_max_width -1 accepts everything.
  // The debounce thresholds got this floor already; these did not.
  if(minWidth<0) minWidth=0;
  if(maxWidth<0) maxWidth=0;
  JSON_SETIF_ABLE(DEBOUNCE_H_THRES,jGT,"debounce_rise");
  JSON_SETIF_ABLE(DEBOUNCE_L_THRES,jGT,"debounce_fall");
  JSON_SETIF_ABLE(GATE_MIN_DIST_um,jGT,"min_detect_dist_um");

  if(jP["stepper_en_active"].is<int>() || jP["stepper_dir"].is<int>())
  {
    JSON_SETIF_ABLE(stepper_en_active,jP,"stepper_en_active");
    JSON_SETIF_ABLE(stepper_dir_level,jP,"stepper_dir");
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

  // The group is applied AFTER the flat keys, so a document carrying both --
  // which is what get_setup emits and therefore what NVS stores -- ends on the
  // group. A document carrying only the old keys still works untouched.
  if(jdoc["skip_policy"].is<JsonObject>())
  {
    JsonObject jSP=jdoc["skip_policy"];
    if(jSP["mode"].is<const char*>())
    {
      const char* m=jSP["mode"];
      bool ar=AUTO_RATE; int up=UNANSWERED_POLICY;
      if(strcmp(m,"slow_and_stop")==0){ ar=true;  up=1; }
      else if(strcmp(m,"slow_only")==0){ ar=true;  up=0; }
      else if(strcmp(m,"stop_only")==0){ ar=false; up=1; }
      else if(strcmp(m,"none")==0)     { ar=false; up=0; }
      if(ar!=AUTO_RATE){ AUTO_RATE=ar; AUTO_RATE_OK_RUN=0;
                         if(!ar) GATE_SEP_EFF_us=SYS_MIN_PULSE_TIME_SEP_us; }
      UNANSWERED_POLICY=up;
    }
    if(jSP["stop_after"].is<int>()){ int v=jSP["stop_after"]; UNANSWERED_STOP_AFTER=(v<1)?1:v; }
    JSON_SETIF_ABLE(AUTO_RATE_FLOOR_us,jSP,"rate_floor_us");
    JSON_SETIF_ABLE(AUTO_RATE_RECOVER_N,jSP,"recover_n");
    if(AUTO_RATE_RECOVER_N<1) AUTO_RATE_RECOVER_N=1;
    if(AUTO_RATE_FLOOR_us<SYS_MIN_PULSE_TIME_SEP_us)
      AUTO_RATE_FLOOR_us=SYS_MIN_PULSE_TIME_SEP_us;
  }
  JSON_SETIF_ABLE(host_timeout_ms,jdoc,"host_timeout_ms");
  JSON_SETIF_ABLE(pulses_per_rev,jP,"pulses_per_rev");
  JSON_SETIF_ABLE(plate_diameter_mm,jP,"diameter_mm");

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

  if (jdoc.containsKey("stage_pulse_width_us")) {
    JsonObject jW = jdoc["stage_pulse_width_us"];
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.CAM1,jW,"CAM1");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.L1A ,jW,"L1A");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.CAM2,jW,"CAM2");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.L2A ,jW,"L2A");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.SEL1,jW,"SEL1");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.SEL2,jW,"SEL2");
    JSON_SETIF_ABLE(STAGE_PULSE_WIDTH_US.SEL3,jW,"SEL3");
  }
  if (jdoc.containsKey("stage_pulse_center")) {
    JsonObject jC = jdoc["stage_pulse_center"];
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.CAM1,jC,"CAM1");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.L1A ,jC,"L1A");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.CAM2,jC,"CAM2");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.L2A ,jC,"L2A");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.SEL1,jC,"SEL1");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.SEL2,jC,"SEL2");
    JSON_SETIF_ABLE(STAGE_PULSE_CENTER.SEL3,jC,"SEL3");
  }
  // Re-derive unconditionally, not only when a width key was present: plate_freq
  // may have changed in this same set_setup, and every configured width is a
  // function of it. Cheap, and it removes an ordering dependency between two
  // keys in one message.
  STAGE_PULSE_WIDTH_apply(stageWidthRefFreq());
  if(STAGE_WIDTH_SEL_WARN)
  {
    STAGE_WIDTH_SEL_WARN = false;
    djrl.dbg_printf("SEL width exceeds half the part spacing -- the blow is "
                    "still open when the next part arrives");
  }
  if(STAGE_CENTER_CLAMP_WARN)
  {
    STAGE_CENTER_CLAMP_WARN = false;
    djrl.dbg_printf("stage_pulse_center sits closer to the gate than half its "
                    "own window -- leading edge clamped to 0, so the window is "
                    "no longer centred on it");
  }




}

