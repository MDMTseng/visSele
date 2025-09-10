// Scheduler.cpp - action queue container implementation
#include "Scheduler.hpp"

ActQueues act_S; // definition

namespace Scheduler {
  void reset() {
    act_S.ACT_L1A.clear();
    act_S.ACT_CAM1.clear();
    act_S.ACT_L2A.clear();
    act_S.ACT_CAM2.clear();
    act_S.ACT_SWITCH.clear();
    act_S.ACT_SEL1.clear();
    act_S.ACT_SEL2.clear();
  }
}
