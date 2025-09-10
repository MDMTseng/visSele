// Pipeline.cpp - implementation of pipeline buffer reset
#include "Pipeline.hpp"

RingBuf_Static<pipeLineInfo, PIPELINE_CAPACITY, uint8_t> RBuf; // definition

namespace Pipeline {
  void reset() {
    RBuf.clear();
  }
}
