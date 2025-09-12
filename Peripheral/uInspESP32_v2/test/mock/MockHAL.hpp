#pragma once

#include "hal/HAL.hpp"
#include "hal/IGpio.hpp"
#include "hal/ITimerTickSource.hpp"
#include "hal/IStepperDriver.hpp"
#include "hal/IClock.hpp"
#include "hal/ILogger.hpp"
#include "hal/ILock.hpp"
#include "hal/ITransport.hpp"

#include <vector>
#include <string>
#include <map>
#include <chrono>

/**
 * @brief Mock implementations of HAL interfaces for testing
 * 
 * These classes provide deterministic, controllable implementations
 * of the HAL interfaces for host-side testing of logic components.
 */

// Mock GPIO implementation
class MockGpio : public IGpio {
private:
    std::map<uint8_t, PinMode> pin_modes_;
    std::map<uint8_t, PinState> pin_states_;
    std::vector<std::string> write_log_;

public:
    MockGpio() = default;
    virtual ~MockGpio() = default;

    void setPinMode(uint8_t pin, PinMode mode) override {
        pin_modes_[pin] = mode;
    }

    void writePin(uint8_t pin, PinState state) override {
        pin_states_[pin] = state;
        std::string state_str = (state == PinState::PIN_HIGH) ? "HIGH" : "LOW";
        write_log_.push_back("GPIO" + std::to_string(pin) + "=" + state_str);
    }

    PinState readPin(uint8_t pin) override {
        auto it = pin_states_.find(pin);
        return (it != pin_states_.end()) ? it->second : PinState::PIN_LOW;
    }

    // Test helper methods
    PinMode getPinMode(uint8_t pin) const {
        auto it = pin_modes_.find(pin);
        return (it != pin_modes_.end()) ? it->second : PinMode::PIN_INPUT;
    }

    PinState getPinState(uint8_t pin) const {
        auto it = pin_states_.find(pin);
        return (it != pin_states_.end()) ? it->second : PinState::PIN_LOW;
    }

    const std::vector<std::string>& getWriteLog() const {
        return write_log_;
    }

    void clearWriteLog() {
        write_log_.clear();
    }
};

// Mock Timer Tick Source implementation
class MockTimerTickSource : public ITimerTickSource {
private:
    float frequency_hz_;
    bool is_running_;
    TickCallback callback_;
    std::vector<uint64_t> tick_times_;

public:
    MockTimerTickSource() : frequency_hz_(0.0f), is_running_(false) {}
    virtual ~MockTimerTickSource() = default;

    bool init(float frequency_hz) override {
        frequency_hz_ = frequency_hz;
        return true;
    }

    bool start() override {
        is_running_ = true;
        return true;
    }

    bool stop() override {
        is_running_ = false;
        return true;
    }

    bool setFrequencyHz(float frequency_hz) override {
        frequency_hz_ = frequency_hz;
        return true;
    }

    float getFrequencyHz() const override {
        return frequency_hz_;
    }

    void registerTickCallback(TickCallback callback) override {
        callback_ = callback;
    }

    // Test helper methods
    void simulateTick(uint64_t tick_time = 0) {
        if (callback_ && is_running_) {
            tick_times_.push_back(tick_time);
            callback_();
        }
    }

    void simulateTicks(size_t count, uint64_t start_time = 0) {
        for (size_t i = 0; i < count; ++i) {
            simulateTick(start_time + i);
        }
    }

    const std::vector<uint64_t>& getTickTimes() const {
        return tick_times_;
    }

    bool isRunning() const {
        return is_running_;
    }
};

// Mock Stepper Driver implementation
class MockStepperDriver : public IStepperDriver {
private:
    bool is_enabled_;
    bool direction_;
    uint32_t step_count_;
    std::vector<uint64_t> step_times_;

public:
    MockStepperDriver() : is_enabled_(false), direction_(true), step_count_(0) {}
    virtual ~MockStepperDriver() = default;

    void enable(bool enable) override {
        is_enabled_ = enable;
    }

    void setDirection(bool direction) override {
        direction_ = direction;
    }

    void step(uint64_t timestamp = 0) override {
        if (is_enabled_) {
            step_count_++;
            step_times_.push_back(timestamp);
        }
    }

    bool isEnabled() const override {
        return is_enabled_;
    }

    bool getDirection() const override {
        return direction_;
    }

    // Test helper methods
    uint32_t getStepCount() const {
        return step_count_;
    }

    const std::vector<uint64_t>& getStepTimes() const {
        return step_times_;
    }

    void reset() {
        step_count_ = 0;
        step_times_.clear();
    }
};

// Mock Clock implementation
class MockClock : public IClock {
private:
    uint64_t current_time_us_;
    uint32_t current_time_ms_;

public:
    MockClock() : current_time_us_(0), current_time_ms_(0) {}
    virtual ~MockClock() = default;

    uint64_t micros() override {
        return current_time_us_;
    }

    uint32_t millis() override {
        return current_time_ms_;
    }

    void delayMs(uint32_t ms) override {
        current_time_ms_ += ms;
        current_time_us_ += ms * 1000;
    }

    void delayUs(uint32_t us) override {
        current_time_us_ += us;
        current_time_ms_ = current_time_us_ / 1000;
    }

    // Test helper methods
    void setTime(uint64_t time_us) {
        current_time_us_ = time_us;
        current_time_ms_ = time_us / 1000;
    }

    void advanceTime(uint64_t delta_us) {
        current_time_us_ += delta_us;
        current_time_ms_ = current_time_us_ / 1000;
    }
};

// Mock Logger implementation
class MockLogger : public ILogger {
private:
    std::vector<LogEntry> log_entries_;

public:
    MockLogger() = default;
    virtual ~MockLogger() = default;

    void log(LogLevel level, const std::string& message) override {
        LogEntry entry;
        entry.level = level;
        entry.message = message;
        entry.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        log_entries_.push_back(entry);
    }

    // Test helper methods
    const std::vector<LogEntry>& getLogEntries() const {
        return log_entries_;
    }

    void clearLog() {
        log_entries_.clear();
    }

    size_t getLogCount() const {
        return log_entries_.size();
    }
};

// Mock Lock implementation
class MockLock : public ILock {
public:
    MockLock() = default;
    virtual ~MockLock() = default;

    void lock() override {
        // No-op for testing
    }

    void unlock() override {
        // No-op for testing
    }
};

// Mock Transport implementation
class MockTransport : public ITransport {
private:
    std::vector<uint8_t> input_buffer_;
    std::vector<uint8_t> output_buffer_;
    size_t read_position_;

public:
    MockTransport() : read_position_(0) {}
    virtual ~MockTransport() = default;

    size_t available() override {
        return input_buffer_.size() - read_position_;
    }

    size_t read(uint8_t* buffer, size_t size) override {
        size_t bytes_to_read = std::min(size, available());
        if (bytes_to_read > 0) {
            std::memcpy(buffer, &input_buffer_[read_position_], bytes_to_read);
            read_position_ += bytes_to_read;
        }
        return bytes_to_read;
    }

    size_t write(const uint8_t* buffer, size_t size) override {
        output_buffer_.insert(output_buffer_.end(), buffer, buffer + size);
        return size;
    }

    void flush() override {
        // No-op for testing
    }

    // Test helper methods
    void addInputData(const uint8_t* data, size_t size) {
        input_buffer_.insert(input_buffer_.end(), data, data + size);
    }

    void addInputString(const std::string& str) {
        addInputData(reinterpret_cast<const uint8_t*>(str.c_str()), str.length());
    }

    const std::vector<uint8_t>& getOutputBuffer() const {
        return output_buffer_;
    }

    std::string getOutputString() const {
        return std::string(output_buffer_.begin(), output_buffer_.end());
    }

    void clearBuffers() {
        input_buffer_.clear();
        output_buffer_.clear();
        read_position_ = 0;
    }
};

// Mock HAL implementation
class MockHAL : public IHAL {
private:
    MockGpio gpio_;
    MockTimerTickSource timer_tick_source_;
    MockStepperDriver stepper_driver_;
    MockClock clock_;
    MockLogger logger_;
    MockLock lock_;
    MockTransport transport_;

public:
    MockHAL() = default;
    virtual ~MockHAL() = default;

    IGpio& gpio() override { return gpio_; }
    ITimerTickSource& timerTickSource() override { return timer_tick_source_; }
    IStepperDriver& stepperDriver() override { return stepper_driver_; }
    IClock& clock() override { return clock_; }
    ILogger& logger() override { return logger_; }
    ILock& lock() override { return lock_; }
    ITransport& transport() override { return transport_; }

    // Test helper methods
    MockGpio& getMockGpio() { return gpio_; }
    MockTimerTickSource& getMockTimerTickSource() { return timer_tick_source_; }
    MockStepperDriver& getMockStepperDriver() { return stepper_driver_; }
    MockClock& getMockClock() { return clock_; }
    MockLogger& getMockLogger() { return logger_; }
    MockTransport& getMockTransport() { return transport_; }
};
