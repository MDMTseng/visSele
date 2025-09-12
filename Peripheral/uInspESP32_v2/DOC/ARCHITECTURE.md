# uInspESP32 Architecture Documentation

## Overview

The uInspESP32 is a modular, platform-agnostic inspection system controller built around a clean separation of concerns. The architecture follows a layered design with clear interfaces between hardware abstraction, core logic, and communication layers.

## Architecture Principles

### 1. Single Responsibility Principle
Each module has one clear, well-defined responsibility:
- **State Machine**: Manages system state transitions and lifecycle
- **Scheduler**: Handles timing-based action scheduling
- **Pipeline**: Manages object lifecycle and inspection data
- **Gate Sensor**: Detects and processes object detection events
- **Message Bus**: Routes messages between components
- **Diagnostics**: Handles error logging and reporting
- **HAL**: Abstracts hardware-specific operations

### 2. Layered Design
The system is organized in distinct layers with clear boundaries:

```
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│  (State Machine, Scheduler, Pipeline, Gate Sensor)     │
├─────────────────────────────────────────────────────────┤
│                    Communication Layer                  │
│  (Message Bus, Protocol, Command Handler)              │
├─────────────────────────────────────────────────────────┤
│                    Hardware Abstraction Layer           │
│  (HAL Interfaces: GPIO, Timer, Clock, Transport)       │
├─────────────────────────────────────────────────────────┤
│                    Platform Implementation              │
│  (ESP32 HAL, STM32 HAL, Mock HAL for Testing)          │
└─────────────────────────────────────────────────────────┘
```

### 3. Platform Independence
Core logic is completely platform-agnostic:
- No direct hardware dependencies in logic modules
- All hardware access through HAL interfaces
- Easy porting between ESP32, STM32, and other platforms
- Testable on host machines without hardware

## Module Architecture

### Core Logic Modules

#### State Machine (`StateMachine.hpp/.cpp`)
**Responsibility**: System state management and lifecycle control

**Key Components**:
- State enumeration (`SystemState`)
- Action enumeration (`SystemAction`)
- State transition logic
- Callback registration for state changes

**Interface**:
```cpp
class StateMachine {
public:
    void init();
    void applyAction(SystemAction action, int extra = 0);
    SystemState getCurrentState() const;
    void pump();  // Execute current state loop
    void registerStateChangeCallback(StateChangeCallback callback);
};
```

#### Scheduler (`Scheduler.hpp/.cpp`)
**Responsibility**: Timing-based action scheduling and execution

**Key Components**:
- Action queues (`ActQueues`)
- Action information structures (`ACT_INFO`)
- Scheduling algorithms
- Hardware action callbacks

**Interface**:
```cpp
class Scheduler {
public:
    void init();
    void scheduleAction(ActionType type, uint32_t target_pulse, bool state, void* context);
    void runScheduled(uint32_t current_pulse);
    void reset();
};
```

#### Pipeline (`Pipeline.hpp/.cpp`)
**Responsibility**: Object lifecycle management and inspection data

**Key Components**:
- Object information structures (`ObjectInfo`)
- Object queues and lifecycle management
- Inspection status tracking
- Object cleanup and reset

**Interface**:
```cpp
class Pipeline {
public:
    void init();
    void registerObject(const ObjectInfo& obj);
    ObjectInfo* getObject(uint32_t tid);
    void cleanup();
    void reset();
};
```

#### Gate Sensor (`GateSensor.hpp/.cpp`)
**Responsibility**: Object detection and gate sensing logic

**Key Components**:
- Debounce logic
- Pulse width filtering
- Object detection callbacks
- ISR-safe tick processing

**Interface**:
```cpp
class GateSensor {
public:
    void init();
    void tick(uint32_t pulse_count);  // ISR-safe
    void reset();
    void registerObjectDetectedCallback(ObjectDetectedCallback callback);
};
```

#### Stepper Controller (`StepperController.hpp/.cpp`)
**Responsibility**: Stepper motor control with frequency ramping and timing management

**Key Components**:
- Frequency ramping logic
- Acceleration/deceleration control
- Timer integration
- Direction control
- Enable/disable functionality

**Interface**:
```cpp
class StepperController {
public:
    StepperController(IStepperDriver& stepper_driver, 
                     ITimerTickSource& timer_tick_source,
                     IClock& clock);
    bool init(uint8_t pulse_pin, uint8_t dir_pin, uint8_t enable_pin, 
              bool enable_active_state, float initial_freq_hz,
              float max_accel_hz_per_sec);
    void setTargetFrequency(float target_freq_hz);
    float getCurrentFrequency() const;
    bool isStable() const;
    void enableStepper(bool enabled);
    bool isStepperEnabled() const;
    void setDirection(IStepperDriver::Direction direction);
    void update();  // Called from main loop for frequency ramping
    void setMaxAcceleration(float max_accel_hz_per_sec);
};
```

### Communication Modules

#### Message Bus (`MessageBus.hpp/.cpp`)
**Responsibility**: Inter-component message routing and queuing

**Key Components**:
- Message structures and types
- Outbound message queues
- Message serialization
- Transport abstraction

**Interface**:
```cpp
class MessageBus {
public:
    void init();
    bool sendMessage(const Message& msg);
    std::vector<Message> getOutboundMessages();
    void clearOutboundMessages();
};
```

#### Diagnostics (`Diagnostics.hpp/.cpp`)
**Responsibility**: Error logging, history, and reporting

**Key Components**:
- Error code enumeration
- Error history storage
- JSON export functionality
- Error filtering and retrieval

**Interface**:
```cpp
class Diagnostics {
public:
    void init();
    void logError(ErrorCode code, const std::string& message);
    std::vector<ErrorEntry> getErrorHistory();
    std::string exportErrorHistoryAsJson();
};
```

### Hardware Abstraction Layer (HAL)

#### HAL Interfaces
**Purpose**: Platform-agnostic hardware access

**Key Interfaces**:
- `IGpio`: GPIO operations (pinMode, digitalWrite, digitalRead)
- `ITimerTickSource`: Timer interrupt management
- `IStepperDriver`: Stepper motor control
- `IClock`: Time functions (micros, millis, delay)
- `ILogger`: Logging operations
- `ILock`: Synchronization primitives
- `ITransport`: Communication transport

#### Main HAL Integration (`main_hal.cpp`)
**Responsibility**: HAL initialization, main loop orchestration, and system integration

**Key Components**:
- System initialization and setup
- Timer ISR callback management
- Main loop orchestration
- Module integration and coordination
- FreeRTOS task management

**Key Functions**:
```cpp
// HAL initialization (called from main setup)
void custom_hal_init();

// Main loop update (called from main loop)
void custom_hal_update();

// Timer ISR callback (minimal, platform-neutral)
void hal_onTimer();
```

**Integration Pattern**:
1. **Initialization**: Sets up all HAL components, modules, and FreeRTOS tasks
2. **Main Loop**: Orchestrates high-level system operations
3. **ISR Handling**: Minimal, platform-neutral timer callback
4. **Module Coordination**: Integrates all system modules through HAL interfaces

#### Platform Implementations
**ESP32 Implementation** (`src/hal/esp32/`):
- `ESP32Gpio`: ESP32 GPIO register manipulation
- `ESP32TimerTickSource`: ESP32 hardware timer management
- `ESP32StepperDriver`: ESP32 stepper control
- `ESP32Clock`: ESP32 timing functions
- `ESP32Logger`: ESP32 logging (JSON output)
- `ESP32Lock`: ESP32 FreeRTOS synchronization
- `SerialTransport`: ESP32 Serial communication

**Mock Implementation** (`test/mock/`):
- `MockGpio`: Controllable GPIO simulation
- `MockTimerTickSource`: Deterministic timer simulation
- `MockStepperDriver`: Stepper behavior simulation
- `MockClock`: Controllable time simulation
- `MockLogger`: Log capture and verification
- `MockLock`: No-op synchronization
- `MockTransport`: Controllable I/O simulation

## Data Flow Architecture

### Object Detection Flow
```
Gate Sensor → Pipeline Registration → Scheduler Actions → Hardware Output
     ↓              ↓                      ↓               ↓
  ISR Tick    Object Queuing         Timing Control    GPIO/Transport
```

### Command Processing Flow
```
Serial Input → Protocol Parser → Command Handler → Response Generation → Serial Output
     ↓              ↓                ↓                    ↓              ↓
 Transport     JSON Parsing    State Machine        Message Bus    Transport
```

### State Management Flow
```
State Machine → Action Application → Component Updates → Hardware Control
     ↓               ↓                    ↓                ↓
 State Logic    Transition Rules    Module State      HAL Interface
```

## ISR Architecture

### ISR Design Principles
1. **Minimal Logic**: Only essential operations in ISR
2. **Platform Neutral**: No platform-specific code in ISR
3. **ISR-Safe Operations**: No dynamic allocation, complex operations
4. **Callback-Based**: Heavy logic moved to callbacks

### ISR Flow
```
Timer ISR → Step Count Increment → Gate Sensor Tick → Scheduler Run → Hardware Actions
    ↓              ↓                    ↓                ↓              ↓
Platform      Global Counter      Object Detection   Action Queue    GPIO/Stepper
Specific      (Platform          (ISR-Safe)         Processing      Control
              Neutral)                              (ISR-Safe)
```

## Testing Architecture

### Host-Side Testing
- **Mock HAL**: Controllable hardware simulation
- **Logic Tests**: Pure logic component testing
- **Integration Tests**: Component interaction testing
- **Automated Test Suite**: Comprehensive test coverage

### Test Categories
1. **Unit Tests**: Individual component testing
2. **Integration Tests**: Component interaction testing
3. **Functional Tests**: End-to-end scenario testing
4. **Performance Tests**: Timing and resource usage testing

## Configuration Architecture

### Board Configuration (`BoardConfig.hpp`)
- Pin mappings and hardware constants
- Platform-specific configuration
- Compile-time feature selection

### System Types (`SystemTypes.hpp`)
- Common enumerations and structures
- Type definitions used across modules
- Platform-agnostic data structures

## Portability Architecture

### Platform Abstraction
- **HAL Interfaces**: Platform-agnostic hardware access
- **Conditional Compilation**: Platform-specific code isolation
- **Interface Implementations**: Platform-specific HAL implementations

### Migration Strategy
1. **ESP32 → STM32**: Implement STM32 HAL layer
2. **Hardware Changes**: Update BoardConfig.hpp
3. **Feature Changes**: Modify HAL interfaces
4. **Testing**: Use mock HAL for validation

## Performance Considerations

### ISR Performance
- **Minimal ISR Time**: < 10µs typical execution
- **No Blocking Operations**: No mutexes or complex operations
- **Efficient Data Structures**: Ring buffers and simple queues

### Memory Usage
- **Static Allocation**: No dynamic allocation in ISR
- **Efficient Storage**: Compact data structures
- **Memory Pools**: Pre-allocated object pools

### Timing Accuracy
- **Hardware Timers**: Precise timing control
- **Minimal Jitter**: ISR execution time consistency
- **Predictable Behavior**: Deterministic scheduling

## Security Considerations

### Input Validation
- **Command Validation**: JSON command structure validation
- **Parameter Bounds**: Input parameter range checking
- **Error Handling**: Graceful error handling and reporting

### Error Recovery
- **State Recovery**: Automatic state machine recovery
- **Error Logging**: Comprehensive error history
- **System Reset**: Safe system reset capabilities

## Current Implementation Status

### Refactor Completion
**Status**: 100% Complete (11/11 stages done)

The uInspESP32 has been successfully refactored from a monolithic 2300+ line `SyncTask.cpp` into a clean, modular, platform-agnostic architecture. All planned refactoring stages have been completed as of 2025-01-15.

### Implementation Highlights

#### Completed Modules
- ✅ **Core Logic**: StateMachine, Scheduler, Pipeline, GateSensor
- ✅ **Communication**: MessageBus, Diagnostics, ITransport
- ✅ **HAL Layer**: Complete ESP32 implementation with STM32 scaffolding
- ✅ **Testing**: Comprehensive host-side testing framework
- ✅ **Documentation**: Complete architecture and migration documentation

#### Key Architectural Achievements
1. **Platform Independence**: All core logic is platform-agnostic
2. **Testability**: Host-side testing without hardware dependencies
3. **Maintainability**: Clear module boundaries and single responsibilities
4. **Extensibility**: Easy addition of new features and platforms
5. **Performance**: Optimized ISR and timing-critical operations

#### Current System Architecture
```
Main Loop (main.cpp)
    ↓
HAL Integration (main_hal.cpp)
    ↓
┌─────────────────────────────────────────────────────────┐
│                    Application Layer                    │
│  StateMachine → Scheduler → Pipeline → GateSensor      │
├─────────────────────────────────────────────────────────┤
│                    Communication Layer                  │
│  MessageBus → Diagnostics → ITransport                 │
├─────────────────────────────────────────────────────────┤
│                    Hardware Abstraction Layer           │
│  ESP32 HAL Implementations (IGpio, ITimer, etc.)       │
└─────────────────────────────────────────────────────────┘
```

### Recent Updates (2025-01-15)
- **StepperController Integration**: Complete frequency ramping and control
- **Main Loop Simplification**: High-level orchestration pattern
- **ISR Optimization**: Minimal, platform-neutral interrupt handling
- **Testing Framework**: Comprehensive host-side testing capabilities
- **STM32 Preparation**: Complete portability scaffolding

## Future Architecture Evolution

### Planned Enhancements
1. **STM32 Implementation**: Complete STM32 HAL layer implementation
2. **Network Transport**: Ethernet/WiFi communication support
3. **Advanced Diagnostics**: Real-time monitoring and analysis
4. **Configuration Management**: Runtime configuration updates
5. **Plugin Architecture**: Extensible component system

### Scalability Considerations
- **Modular Design**: Easy addition of new components
- **Interface Extensions**: Backward-compatible interface evolution
- **Performance Optimization**: Profile-guided optimization
- **Resource Management**: Dynamic resource allocation strategies

## Development Guidelines

### Code Organization
- **Header Files**: Public interfaces in `include/`
- **Implementation**: Private implementation in `src/`
- **Platform Code**: Platform-specific code in `src/hal/`
- **Tests**: Test code in `test/`

### Interface Design
- **Pure Virtual**: Abstract interfaces for hardware
- **RAII**: Resource management through constructors/destructors
- **Const Correctness**: Proper const usage for immutability
- **Error Handling**: Consistent error handling patterns

### Testing Strategy
- **Test-Driven Development**: Write tests before implementation
- **Mock Objects**: Use mocks for hardware dependencies
- **Automated Testing**: Continuous integration with test automation
- **Coverage Analysis**: Comprehensive test coverage monitoring
