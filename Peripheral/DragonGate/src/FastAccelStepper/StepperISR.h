#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_AVR)
#include <Arduino.h>
#endif
#include <stdint.h>

#include "FastAccelStepper.h"
#include "common.h"

// Here are the global variables to interface with the interrupts
#if __has_include("fas_config.h")
#include "fas_config.h"
#endif

// CURRENT QUEUE IMPLEMENTATION WASTES ONE UNUSED ENTRY => BUG/TODO

#if defined(TEST)
#define NUM_QUEUES 2
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define QUEUE_LEN 16
#elif defined(ARDUINO_ARCH_AVR)
#if defined(__AVR_ATmega328P__)
#define NUM_QUEUES 2
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define QUEUE_LEN 16
enum channels { channelA, channelB };
#elif defined(__AVR_ATmega2560__)
#define NUM_QUEUES 3
#define fas_queue_A fas_queue[0]
#define fas_queue_B fas_queue[1]
#define fas_queue_C fas_queue[2]
#define QUEUE_LEN 16
enum channels { channelA, channelB, channelC };
#else
#error "Unsupported derivate"
#endif
#elif defined(ARDUINO_ARCH_ESP32)
#define NUM_QUEUES 6
#define QUEUE_LEN 32
#else
#define NUM_QUEUES 6
#define QUEUE_LEN 32
#endif

// These variables control the stepper timing behaviour
#define QUEUE_LEN_MASK (QUEUE_LEN - 1)

#ifndef TEST
#define inject_fill_interrupt(x)
#endif

#define TICKS_FOR_STOPPED_MOTOR 0xffffffff

#if defined(ARDUINO_ARCH_ESP32)
#include <driver/gpio.h>
#include <driver/mcpwm.h>
#include <driver/pcnt.h>
#include <soc/mcpwm_reg.h>
#include <soc/mcpwm_struct.h>
#include <soc/pcnt_reg.h>
#include <soc/pcnt_struct.h>
struct mapping_s {
  mcpwm_unit_t mcpwm_unit;
  uint8_t timer;
  mcpwm_io_signals_t pwm_output_pin;
  pcnt_unit_t pcnt_unit;
  uint8_t input_sig_index;
  uint32_t timer_tez_int_clr;
  uint32_t timer_tez_int_ena;
};
#endif

struct queue_entry {
  uint8_t steps;  // if 0,  then the command only adds a delay
  bool toggle_dir;
  uint16_t ticks;
};
class StepperQueue {
 public:
  struct queue_entry entry[QUEUE_LEN];
  uint8_t read_idx;  // ISR stops if readptr == next_writeptr
  uint8_t next_write_idx;
  uint8_t dirPin;
  bool dirHighCountsUp;
  volatile bool isRunning;
#if defined(ARDUINO_ARCH_ESP32)
  const struct mapping_s* mapping;
#endif
#if defined(ARDUINO_ARCH_AVR)
  enum channels channel;
#endif
  uint16_t ticks;
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
  uint8_t checksum;
#endif

  struct queue_end_s queue_end;

  void init(uint8_t queue_num, uint8_t step_pin);
  inline bool isQueueFull() {
    noInterrupts();
    uint8_t rp = read_idx;
    uint8_t wp = next_write_idx;
    interrupts();
    rp += QUEUE_LEN;
    return (wp == rp);
  }
  inline uint8_t size() {
    noInterrupts();
    uint8_t diff = next_write_idx-read_idx;
    interrupts();
    return diff&(QUEUE_LEN_MASK|QUEUE_LEN);
  }
  inline bool isQueueEmpty() {
    noInterrupts();
    bool res = (next_write_idx == read_idx);
    interrupts();
    inject_fill_interrupt(0);
    return res;
  }
  int addQueueEntry(struct stepper_command_s* cmd) {
    if (isQueueFull()) {
      return AQE_FULL;
    }
    uint32_t period_ticks = cmd->ticks;
    if (period_ticks > 65535) {
      return AQE_TOO_HIGH;
    }
    uint16_t period = period_ticks;

    uint8_t wp = next_write_idx;
    struct queue_entry* e = &entry[wp & QUEUE_LEN_MASK];
    uint8_t steps = cmd->steps;
    queue_end.pos += cmd->count_up ? steps : -steps;
    if (steps == 0) {
      // This is a pause
      uint32_t tfls = queue_end.ticks_from_last_step;
      if (tfls <= 0xffff0000) {
        queue_end.ticks_from_last_step = tfls + cmd->ticks;
      }
    } else {
      uint32_t tfls = queue_end.ticks_from_last_step;
      if (tfls <= 0xffff0000) {
        queue_end.ticks = tfls + cmd->ticks;
      } else {
        queue_end.ticks = tfls;
      }
      queue_end.ticks_from_last_step = 0;
    }
    bool dir = (cmd->count_up == dirHighCountsUp);
    e->steps = steps;
    e->toggle_dir = (dir != queue_end.dir) ? true : false;
    e->ticks = period;
    queue_end.dir = dir;
    queue_end.count_up = cmd->count_up;
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    {
      // checksum is in the struct and will updated here
      unsigned char* x = (unsigned char*)e;
      for (uint8_t i = 0; i < sizeof(struct queue_entry); i++) {
        if (checksum & 0x80) {
          checksum <<= 1;
          checksum ^= 0xde;
        } else {
          checksum <<= 1;
        }
        checksum ^= *x++;
      }
    }
#endif
    wp++;
    noInterrupts();
    next_write_idx = wp;
    bool run = isRunning;
    interrupts();
    if (!run) {
      startQueue();
    }
    return AQE_OK;
  }
  bool hasTicksInQueue(uint32_t min_ticks) {
    noInterrupts();
    uint8_t rp = read_idx;
    uint8_t wp = next_write_idx;
    interrupts();
    if (wp == rp) {
      return 0;
    }
    rp++;  // ignore currently processed entry
    while (wp != rp) {
      struct queue_entry* e = &entry[rp & QUEUE_LEN_MASK];
      uint32_t tmp = e->ticks;
      uint8_t steps = max(e->steps, 1);
      tmp *= steps;
      if (tmp >= min_ticks) {
        return true;
      }
      min_ticks -= tmp;
      rp++;
    }
    return false;
  }

  // startQueue is called, if motor is not running.
  void startQueue();
  void forceStop();
  void _initVars() {
    dirPin = PIN_UNDEFINED;
    read_idx = 0;
    next_write_idx = 0;
    queue_end.dir = true;
    queue_end.count_up = true;
    queue_end.pos = 0;
    queue_end.ticks = TICKS_FOR_STOPPED_MOTOR;
    queue_end.ticks_from_last_step = 0xffffffff;
    dirHighCountsUp = true;
    isRunning = false;
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    checksum = 0;
#endif
  }
#if defined(ARDUINO_ARCH_ESP32)
  uint8_t _step_pin;
#endif
  void connect();
  void disconnect();
  static bool isValidStepPin(uint8_t step_pin);
  static int8_t queueNumForStepPin(uint8_t step_pin);
};

extern StepperQueue fas_queue[NUM_QUEUES];
