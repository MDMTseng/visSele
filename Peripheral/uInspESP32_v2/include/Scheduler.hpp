// Scheduler.hpp - Action scheduling queues (Stage S1.2)
#pragma once
#include <stdint.h>
#include "RingBuf.hpp"
#include "Pipeline.hpp"

// Action info previously local to SyncTask.cpp
struct ACT_INFO {
  pipeLineInfo *src;      // pointer to originating pipeline object (may be nulled after consumption)
  int info;               // semantic meaning (1=on,0=off, other values for switch routing)
  uint32_t targetPulse;   // absolute pulse at which to execute
};

// Use same capacity as pipeline to avoid overflow mismatch.
constexpr int ACT_QUEUE_CAPACITY = PIPELINE_CAPACITY;

struct ActQueues {
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_L1A;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_CAM1;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_L2A;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_CAM2;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_SWITCH;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_SEL1;
  RingBuf_Static<ACT_INFO, ACT_QUEUE_CAPACITY, uint8_t> ACT_SEL2;
};

// Global instance (kept for minimal churn at this stage)
extern ActQueues act_S;

namespace Scheduler {
  // Reset all action queues.
  void reset();
}
