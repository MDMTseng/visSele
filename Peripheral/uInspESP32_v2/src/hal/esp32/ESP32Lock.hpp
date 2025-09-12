#pragma once
#include "hal/ILock.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief ESP32-specific implementation of the lock interface using FreeRTOS semaphores
 */
class ESP32Lock : public ILock {
private:
    SemaphoreHandle_t mutex_;
    
public:
    ESP32Lock() {
        mutex_ = xSemaphoreCreateMutex();
    }
    
    ~ESP32Lock() {
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
        }
    }
    
    bool lock(int32_t timeout_ms = -1) override {
        TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
        return xSemaphoreTake(mutex_, ticks) == pdTRUE;
    }
    
    bool tryLock() override {
        return xSemaphoreTake(mutex_, 0) == pdTRUE;
    }
    
    bool unlock() override {
        return xSemaphoreGive(mutex_) == pdTRUE;
    }
};