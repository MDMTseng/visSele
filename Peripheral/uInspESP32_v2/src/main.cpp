// main.cpp - Main entry point for ESP32 application
// This file provides Arduino setup/loop functions and calls HAL module functions

#include <Arduino.h>
#include "main.hpp"

// External HAL functions from main_hal.cpp
extern "C" void custom_hal_init();
extern "C" void custom_hal_update();

// Use weak attribute to allow overriding in SyncTask.cpp
__attribute__((weak)) void setup() {
  // Call HAL initialization
  custom_hal_init();
}

// Use weak attribute to allow overriding in SyncTask.cpp
__attribute__((weak)) void loop() {
  // Call HAL update function
  custom_hal_update();
}