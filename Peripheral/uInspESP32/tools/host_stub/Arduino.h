// Minimal host stub. Data_Layer_Protocol.cpp uses exactly one Arduino symbol.
#pragma once
#include <chrono>
static inline unsigned long millis() {
  using namespace std::chrono;
  return (unsigned long)duration_cast<milliseconds>(
      steady_clock::now().time_since_epoch()).count();
}
