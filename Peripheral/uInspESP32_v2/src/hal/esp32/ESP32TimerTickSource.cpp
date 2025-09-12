#include "ESP32TimerTickSource.hpp"

// Initialize static member
ESP32TimerTickSource* ESP32TimerTickSource::instance_ = nullptr;

// Implementation of the static ISR function
void IRAM_ATTR ESP32TimerTickSource::timerISR() {
    if (instance_ && instance_->callback_) {
        instance_->callback_();
    }
}