# uInspESP32 API Reference

This document provides comprehensive API documentation for all modules in the uInspESP32 system.

## Table of Contents

1. [Core Logic Modules](#core-logic-modules)
   - [StateMachine](#statemachine)
   - [Scheduler](#scheduler)
   - [Pipeline](#pipeline)
   - [GateSensor](#gatesensor)
   - [StepperController](#steppercontroller)
2. [Communication Modules](#communication-modules)
   - [MessageBus](#messagebus)
   - [Diagnostics](#diagnostics)
   - [ITransport](#itransport)
3. [Hardware Abstraction Layer](#hardware-abstraction-layer)
   - [HAL Interfaces](#hal-interfaces)
   - [ESP32 Implementations](#esp32-implementations)
4. [System Configuration](#system-configuration)
   - [BoardConfig](#boardconfig)
   - [SystemTypes](#systemtypes)
   - [SystemConstants](#systemconstants)

## Core Logic Modules

### StateMachine

**File**: `include/StateMachine.hpp`, `src/state/StateMachine.cpp`

**Purpose**: Manages system state transitions and lifecycle control.

#### Public Interface

```cpp
class StateMachine {
public:
    // Initialization
    void init(SystemState initial_state = SystemState::INIT);
    
    // State management
    void applyAction(SystemAction action, int extra = 0);
    SystemState getCurrentState() const;
    void pump();  // Execute current state loop
    
    // Callback registration
    void registerStateChangeCallback(StateChangeCallback callback);
    
    // State queries
    bool isInState(SystemState state) const;
    bool canTransitionTo(SystemState target_state) const;
};
```

#### Key Methods

**`void init(SystemState initial_state)`**
- Initializes the state machine with the specified initial state
- Sets up internal state tracking and callbacks
- **Parameters**: `initial_state` - Starting state (default: INIT)

**`void applyAction(SystemAction action, int extra)`**
- Applies a state transition action
- Validates transition legality and executes side effects
- **Parameters**: 
  - `action` - Action to apply (INIT_OK, ENTER_INSP_MODE, etc.)
  - `extra` - Additional parameter for action (optional)

**`SystemState getCurrentState() const`**
- Returns the current system state
- **Returns**: Current state enum value

**`void pump()`**
- Executes the current state's main loop logic
- Should be called regularly from main loop
- Handles state-specific processing and transitions

#### State Types

```cpp
enum class SystemState {
    INIT,
    IDLE,
    INSPECTION_MODE_READY,
    INSPECTION_MODE_TEST,
    INSPECTION_MODE_ERROR
};

enum class SystemAction {
    INIT_OK,
    ENTER_INSP_MODE,
    EXIT_INSP_MODE,
    CLEAR_ERROR,
    // ... other actions
};
```

### Scheduler

**File**: `include/Scheduler.hpp`, `src/scheduler/Scheduler.cpp`

**Purpose**: Handles timing-based action scheduling and execution.

#### Public Interface

```cpp
class Scheduler {
public:
    // Initialization
    void init();
    
    // Action scheduling
    void scheduleAction(ActionType type, uint32_t target_pulse, 
                       bool state, void* context = nullptr);
    
    // Execution
    void runScheduled(uint32_t current_pulse);
    
    // Management
    void reset();
    void clearActions();
    
    // Status queries
    size_t getPendingActionCount() const;
    bool hasActionsForPulse(uint32_t pulse) const;
};
```

#### Key Methods

**`void scheduleAction(ActionType type, uint32_t target_pulse, bool state, void* context)`**
- Schedules an action to be executed at a specific pulse count
- **Parameters**:
  - `type` - Type of action (CAM1_ON, CAM2_ON, SEL1_ACT, etc.)
  - `target_pulse` - Pulse count when action should execute
  - `state` - Action state (true/false for on/off)
  - `context` - Additional context data (optional)

**`void runScheduled(uint32_t current_pulse)`**
- Executes all actions scheduled for the current pulse count
- Called from ISR or main loop
- **Parameters**: `current_pulse` - Current system pulse count

**`void reset()`**
- Clears all scheduled actions and resets internal state
- Used during system reset or error recovery

#### Action Types

```cpp
enum class ActionType {
    CAM1_ON,
    CAM1_OFF,
    CAM2_ON,
    CAM2_OFF,
    SEL1_ACT,
    SEL2_ACT,
    SEL3_ACT,
    LIGHT1_ON,
    LIGHT1_OFF,
    LIGHT2_ON,
    LIGHT2_OFF
};
```

### Pipeline

**File**: `include/Pipeline.hpp`, `src/pipeline/Pipeline.cpp`

**Purpose**: Manages object lifecycle and inspection data.

#### Public Interface

```cpp
class Pipeline {
public:
    // Initialization
    void init();
    
    // Object management
    void registerObject(const ObjectInfo& obj);
    ObjectInfo* getObject(uint32_t tid);
    void removeObject(uint32_t tid);
    
    // Lifecycle management
    void cleanup();
    void reset();
    
    // Status queries
    size_t getObjectCount() const;
    bool hasObject(uint32_t tid) const;
    std::vector<ObjectInfo> getAllObjects() const;
};
```

#### Key Methods

**`void registerObject(const ObjectInfo& obj)`**
- Registers a new object in the pipeline
- **Parameters**: `obj` - Object information structure

**`ObjectInfo* getObject(uint32_t tid)`**
- Retrieves object information by ID
- **Parameters**: `tid` - Object ID
- **Returns**: Pointer to object info or nullptr if not found

**`void cleanup()`**
- Removes completed or expired objects
- Called periodically to maintain pipeline health

#### Object Information Structure

```cpp
struct ObjectInfo {
    uint32_t tid;           // Object ID
    uint32_t gate_pulse;    // Pulse count when detected
    uint32_t width;         // Object width in pulses
    bool inspection_done;   // Inspection completion status
    int category;           // Inspection result category
    uint32_t timestamp;     // Detection timestamp
};
```

### GateSensor

**File**: `include/GateSensor.hpp`, `src/pipeline/GateSensor.cpp`

**Purpose**: Object detection and gate sensing logic.

#### Public Interface

```cpp
class GateSensor {
public:
    // Initialization
    void init(uint8_t pin, bool pullup = true, 
              int min_width = 10, int max_width = 1000, 
              int debounce_ms = 1);
    
    // ISR-safe processing
    void tick(uint32_t pulse_count);
    
    // Management
    void reset();
    
    // Callback registration
    void registerObjectDetectedCallback(ObjectDetectedCallback callback);
    
    // Status queries
    bool isObjectPresent() const;
    uint32_t getLastDetectionPulse() const;
};
```

#### Key Methods

**`void init(uint8_t pin, bool pullup, int min_width, int max_width, int debounce_ms)`**
- Initializes the gate sensor
- **Parameters**:
  - `pin` - GPIO pin number for gate sensor
  - `pullup` - Enable internal pullup resistor
  - `min_width` - Minimum object width in pulses
  - `max_width` - Maximum object width in pulses
  - `debounce_ms` - Debounce time in milliseconds

**`void tick(uint32_t pulse_count)`**
- ISR-safe method to process gate sensor state
- Should be called from timer ISR
- **Parameters**: `pulse_count` - Current system pulse count

**`void registerObjectDetectedCallback(ObjectDetectedCallback callback)`**
- Registers callback for object detection events
- **Parameters**: `callback` - Function to call when object detected

#### Callback Types

```cpp
using ObjectDetectedCallback = std::function<void(uint32_t tid, uint32_t pulse, uint32_t width)>;
```

### StepperController

**File**: `include/StepperController.hpp`, `src/StepperController.cpp`

**Purpose**: Stepper motor control with frequency ramping and timing management.

#### Public Interface

```cpp
class StepperController {
public:
    // Constructor
    StepperController(IStepperDriver& stepper_driver, 
                     ITimerTickSource& timer_tick_source,
                     IClock& clock);
    
    // Initialization
    bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, 
              bool enable_active_state, float initial_freq_hz,
              float max_accel_hz_per_sec);
    
    // Frequency control
    void setTargetFrequency(float target_freq_hz);
    float getCurrentFrequency() const;
    bool isStable() const;
    
    // Motor control
    void enableStepper(bool enabled);
    bool isStepperEnabled() const;
    void setDirection(IStepperDriver::Direction direction);
    
    // Update and configuration
    void update();  // Called from main loop
    void setMaxAcceleration(float max_accel_hz_per_sec);
};
```

#### Key Methods

**`bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, bool enable_active_state, float initial_freq_hz, float max_accel_hz_per_sec)`**
- Initializes the stepper controller
- **Parameters**:
  - `pulse_pin` - GPIO pin for step pulses
  - `dir_pin` - GPIO pin for direction control
  - `enable_pin` - GPIO pin for enable control
  - `enable_active_state` - Active state for enable pin
  - `initial_freq_hz` - Starting frequency in Hz
  - `max_accel_hz_per_sec` - Maximum acceleration in Hz/sec
- **Returns**: true if initialization successful

**`void setTargetFrequency(float target_freq_hz)`**
- Sets the target frequency for the stepper motor
- Frequency ramping will smoothly transition to target
- **Parameters**: `target_freq_hz` - Target frequency in Hz

**`void update()`**
- Updates frequency ramping and motor control
- Should be called regularly from main loop
- Handles acceleration/deceleration logic

## Communication Modules

### MessageBus

**File**: `include/MessageBus.hpp`, `src/comm/MessageBus.cpp`

**Purpose**: Inter-component message routing and queuing.

#### Public Interface

```cpp
class MessageBus {
public:
    // Initialization
    static void init();
    
    // Message sending
    bool sendMessage(const Message& msg);
    bool sendTriggerInfo(uint32_t tid, int camera_idx, uint64_t timestamp);
    bool sendSystemInfo(const std::string& state, const std::string& log);
    bool sendDebugMessage(const std::string& message);
    
    // Message retrieval
    std::vector<Message> getOutboundMessages();
    void clearOutboundMessages();
    
    // Status queries
    size_t getOutboundMessageCount() const;
    bool hasOutboundMessages() const;
};
```

#### Key Methods

**`bool sendMessage(const Message& msg)`**
- Sends a message to the outbound queue
- **Parameters**: `msg` - Message structure to send
- **Returns**: true if message queued successfully

**`bool sendTriggerInfo(uint32_t tid, int camera_idx, uint64_t timestamp)`**
- Sends camera trigger information
- **Parameters**:
  - `tid` - Object ID
  - `camera_idx` - Camera index (1 or 2)
  - `timestamp` - Trigger timestamp

**`std::vector<Message> getOutboundMessages()`**
- Retrieves all pending outbound messages
- **Returns**: Vector of pending messages

#### Message Structure

```cpp
struct Message {
    MessageType type;
    std::string content;
    uint32_t timestamp;
    uint32_t id;
};
```

### Diagnostics

**File**: `include/Diagnostics.hpp`, `src/diagnostics/Diagnostics.cpp`

**Purpose**: Error logging, history, and reporting.

#### Public Interface

```cpp
class Diagnostics {
public:
    // Initialization
    static void init();
    
    // Error logging
    void logError(ErrorCode code, const std::string& message);
    void logError(ErrorCode code, const std::string& message, uint32_t extra);
    
    // Error history
    std::vector<ErrorEntry> getErrorHistory();
    std::string exportErrorHistoryAsJson();
    void clearErrorHistory();
    
    // Error queries
    size_t getErrorCount() const;
    bool hasErrors() const;
    ErrorEntry getLastError() const;
};
```

#### Key Methods

**`void logError(ErrorCode code, const std::string& message)`**
- Logs an error with the specified code and message
- **Parameters**:
  - `code` - Error code enum
  - `message` - Error description

**`std::string exportErrorHistoryAsJson()`**
- Exports error history as JSON string
- **Returns**: JSON-formatted error history

**`void clearErrorHistory()`**
- Clears all stored error history
- Used for system reset or maintenance

#### Error Types

```cpp
enum class ErrorCode {
    NONE = 0,
    GATE_SENSOR_ERROR,
    STEPPER_ERROR,
    COMMUNICATION_ERROR,
    STATE_MACHINE_ERROR,
    // ... other error codes
};

struct ErrorEntry {
    ErrorCode code;
    std::string message;
    uint32_t timestamp;
    uint32_t extra;
};
```

### ITransport

**File**: `include/ITransport.hpp`, `src/comm/SerialTransport.cpp`

**Purpose**: Communication transport abstraction.

#### Public Interface

```cpp
class ITransport {
public:
    virtual ~ITransport() = default;
    
    // Data operations
    virtual size_t available() const = 0;
    virtual size_t read(uint8_t* buffer, size_t size) = 0;
    virtual size_t write(const uint8_t* data, size_t size) = 0;
    virtual size_t write(const std::string& data) = 0;
    
    // Status queries
    virtual bool isConnected() const = 0;
    virtual void flush() = 0;
};
```

#### SerialTransport Implementation

```cpp
class SerialTransport : public ITransport {
public:
    explicit SerialTransport(HardwareSerial& serial);
    
    // ITransport implementation
    size_t available() const override;
    size_t read(uint8_t* buffer, size_t size) override;
    size_t write(const uint8_t* data, size_t size) override;
    size_t write(const std::string& data) override;
    bool isConnected() const override;
    void flush() override;
};
```

## Hardware Abstraction Layer

### HAL Interfaces

#### IGpio

**File**: `include/hal/IGpio.hpp`

**Purpose**: GPIO operations abstraction.

```cpp
class IGpio {
public:
    enum class PinMode {
        PIN_INPUT,
        PIN_OUTPUT,
        PIN_INPUT_PULLUP,
        PIN_INPUT_PULLDOWN
    };
    
    virtual ~IGpio() = default;
    
    // Pin operations
    virtual void setPinMode(uint8_t pin, PinMode mode) = 0;
    virtual void digitalWrite(uint8_t pin, bool value) = 0;
    virtual bool digitalRead(uint8_t pin) const = 0;
    virtual void togglePin(uint8_t pin) = 0;
};
```

#### ITimerTickSource

**File**: `include/hal/ITimerTickSource.hpp`

**Purpose**: Timer interrupt management abstraction.

```cpp
class ITimerTickSource {
public:
    using TickCallback = std::function<void()>;
    
    virtual ~ITimerTickSource() = default;
    
    // Timer control
    virtual bool init(float frequency_hz) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void setFrequencyHz(float frequency_hz) = 0;
    virtual float getFrequencyHz() const = 0;
    
    // Callback management
    virtual void registerTickCallback(TickCallback callback) = 0;
};
```

#### IStepperDriver

**File**: `include/hal/IStepperDriver.hpp`

**Purpose**: Stepper motor control abstraction.

```cpp
class IStepperDriver {
public:
    enum class Direction {
        FORWARD,
        REVERSE
    };
    
    virtual ~IStepperDriver() = default;
    
    // Driver control
    virtual bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, 
                     bool enable_active_state) = 0;
    virtual void step() = 0;
    virtual void setDirection(Direction direction) = 0;
    virtual void setEnabled(bool enabled) = 0;
    virtual bool isEnabled() const = 0;
};
```

#### IClock

**File**: `include/hal/IClock.hpp`

**Purpose**: Time functions abstraction.

```cpp
class IClock {
public:
    virtual ~IClock() = default;
    
    // Time functions
    virtual uint64_t micros() const = 0;
    virtual uint64_t millis() const = 0;
    virtual void delay(uint32_t ms) = 0;
    virtual void delayMicroseconds(uint32_t us) = 0;
};
```

#### ILogger

**File**: `include/hal/ILogger.hpp`

**Purpose**: Logging operations abstraction.

```cpp
class ILogger {
public:
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    
    virtual ~ILogger() = default;
    
    // Logging
    virtual void log(LogLevel level, const std::string& message) = 0;
    virtual void setLogLevel(LogLevel level) = 0;
    virtual LogLevel getLogLevel() const = 0;
};
```

### ESP32 Implementations

All ESP32 implementations are located in `src/hal/esp32/` and provide concrete implementations of the HAL interfaces:

- **ESP32Gpio**: GPIO operations using ESP32 registers
- **ESP32TimerTickSource**: Timer management using ESP32 hardware timers
- **ESP32StepperDriver**: Stepper control using ESP32 GPIO
- **ESP32Clock**: Time functions using ESP32 system clock
- **ESP32Logger**: Logging using ESP32 Serial output
- **ESP32Lock**: Synchronization using FreeRTOS primitives
- **ESP32HAL**: Main HAL implementation that provides all interfaces

## System Configuration

### BoardConfig

**File**: `include/BoardConfig.hpp`

**Purpose**: Pin mappings and hardware constants.

```cpp
namespace ESP32 {
    // Stepper motor pins
    constexpr int STEPPER_PLS_PIN = 22;
    constexpr int STEPPER_DIR_PIN = 23;
    constexpr int STEPPER_EN_PIN = 13;
    constexpr int STEPPER_EN_ACTIVATION = 0;
    
    // Camera trigger pins
    constexpr int PIN_O_CAM1 = 17;
    constexpr int PIN_O_CAM2 = 19;
    
    // Lighting pins
    constexpr int PIN_O_L1A = 18;
    constexpr int PIN_O_L2A = 21;
    
    // Selector pins
    constexpr int PIN_O_SEL1 = 4;
    constexpr int PIN_O_SEL2 = 5;
    constexpr int PIN_O_SEL3 = 16;
    
    // Input pins
    constexpr int PIN_I_GATE = 27;
    
    // System constants
    constexpr int MIN_OBJECT_WIDTH = 10;
    constexpr int MAX_OBJECT_WIDTH = 1000;
    constexpr int GATE_DEBOUNCE_MS = 1;
}
```

### SystemTypes

**File**: `include/SystemTypes.hpp`

**Purpose**: Common types and enumerations.

```cpp
// System states
enum class SystemState {
    INIT,
    IDLE,
    INSPECTION_MODE_READY,
    INSPECTION_MODE_TEST,
    INSPECTION_MODE_ERROR
};

// System actions
enum class SystemAction {
    INIT_OK,
    ENTER_INSP_MODE,
    EXIT_INSP_MODE,
    CLEAR_ERROR,
    // ... other actions
};

// Object information
struct ObjectInfo {
    uint32_t tid;
    uint32_t gate_pulse;
    uint32_t width;
    bool inspection_done;
    int category;
    uint32_t timestamp;
};
```

### SystemConstants

**File**: `include/SystemConstants.hpp`

**Purpose**: System-wide constants and configuration.

```cpp
namespace SystemConstants {
    // Timing constants
    constexpr uint32_t DEFAULT_FREQUENCY_HZ = 1000;
    constexpr uint32_t MAX_FREQUENCY_HZ = 10000;
    constexpr uint32_t MIN_FREQUENCY_HZ = 1;
    
    // Buffer sizes
    constexpr size_t MESSAGE_BUFFER_SIZE = 256;
    constexpr size_t ERROR_HISTORY_SIZE = 100;
    constexpr size_t OBJECT_QUEUE_SIZE = 50;
    
    // Timing limits
    constexpr uint32_t MAX_ISR_TIME_US = 10;
    constexpr uint32_t MIN_PULSE_WIDTH_US = 1;
    constexpr uint32_t MAX_PULSE_WIDTH_US = 1000;
}
```

## Usage Examples

### Basic System Initialization

```cpp
#include "main.hpp"

void setup() {
    // Initialize HAL
    custom_hal_init();
}

void loop() {
    // Update system
    custom_hal_update();
}
```

### Custom Module Integration

```cpp
#include "StateMachine.hpp"
#include "Scheduler.hpp"
#include "Pipeline.hpp"

// Initialize modules
StateMachine stateMachine;
Scheduler scheduler;
Pipeline pipeline;

void setup() {
    stateMachine.init();
    scheduler.init();
    pipeline.init();
    
    // Register callbacks
    stateMachine.registerStateChangeCallback([](SystemState old_state, SystemState new_state) {
        // Handle state changes
    });
}

void loop() {
    // Update modules
    stateMachine.pump();
    scheduler.runScheduled(getCurrentPulseCount());
    pipeline.cleanup();
}
```

### Custom HAL Implementation

```cpp
#include "hal/IGpio.hpp"

class CustomGpio : public IGpio {
public:
    void setPinMode(uint8_t pin, PinMode mode) override {
        // Custom GPIO implementation
    }
    
    void digitalWrite(uint8_t pin, bool value) override {
        // Custom GPIO write implementation
    }
    
    bool digitalRead(uint8_t pin) const override {
        // Custom GPIO read implementation
        return false;
    }
    
    void togglePin(uint8_t pin) override {
        // Custom GPIO toggle implementation
    }
};
```

This API reference provides comprehensive documentation for all modules in the uInspESP32 system. For implementation details and examples, refer to the source code and test files.
