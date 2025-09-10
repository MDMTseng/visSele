// Pipeline.hpp - extracted pipeline object buffer (Stage S1.1)
#pragma once
#include <stdint.h>
#include <cstddef>
#include "RingBuf.hpp"

// Object lifecycle info (formerly defined in SyncTask.cpp)
struct pipeLineInfo {
  uint32_t gate_pulse;   // pulse at detection (middle or gate reference)
  int8_t   stage;        // reserved / unused currently
  int32_t  insp_status;  // inspection status or sentinel values
  uint32_t tid;          // unique tracking id
};

// Capacity chosen to match previous larger usage (100) to avoid overflow risk.
constexpr int PIPELINE_CAPACITY = 100;

// Exposed ring buffer instance (kept global style for minimal churn this stage).
extern RingBuf_Static<pipeLineInfo, PIPELINE_CAPACITY, uint8_t> RBuf;

namespace Pipeline {
  // Clear all queued objects.
  void reset();
  // (Future stages) helper to iterate / cleanup can be added here.
}
