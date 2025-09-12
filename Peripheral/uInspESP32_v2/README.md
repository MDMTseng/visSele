# uInspESP32 - Micro Inspection System Controller

This repository contains the firmware for an ESP32-based controller designed for a high-speed, circular inspection system. It precisely controls a rotating platform, detects objects, triggers cameras for inspection, and actuates selectors (e.g., air jets) to sort objects based on inspection results communicated from a host system.

## Architecture Overview

The uInspESP32 has been refactored into a modular, platform-agnostic architecture with clean separation of concerns:

- **Core Logic Modules**: State machine, scheduler, pipeline, and gate sensor
- **Communication Layer**: Message bus, protocol handling, and diagnostics
- **Hardware Abstraction Layer (HAL)**: Platform-independent hardware interfaces
- **Platform Implementations**: ESP32-specific HAL implementations with STM32 portability

This modular design enables:
- **Platform Independence**: Easy porting between ESP32, STM32, and other platforms
- **Testability**: Host-side testing without hardware dependencies
- **Maintainability**: Clear module boundaries and responsibilities
- **Extensibility**: Easy addition of new features and components

For detailed architecture information, see [DOC/ARCHITECTURE.md](DOC/ARCHITECTURE.md).

## Project Structure

```
uInspESP32_v2/
├── include/                    # Public headers (API interfaces)
│   ├── BoardConfig.hpp        # Pin mappings and hardware constants
│   ├── SystemTypes.hpp        # Common types and enumerations
│   ├── Pipeline.hpp           # Object lifecycle management
│   ├── Scheduler.hpp          # Action scheduling interface
│   ├── GateSensor.hpp         # Object detection interface
│   ├── StateMachine.hpp       # State management interface
│   ├── MessageBus.hpp         # Message routing interface
│   ├── Diagnostics.hpp        # Error logging interface
│   └── hal/                   # Hardware abstraction interfaces
│       ├── HAL.hpp           # Main HAL interface
│       ├── IGpio.hpp         # GPIO abstraction
│       ├── ITimerTickSource.hpp # Timer abstraction
│       ├── IStepperDriver.hpp   # Stepper control abstraction
│       ├── IClock.hpp        # Time functions abstraction
│       ├── ILogger.hpp       # Logging abstraction
│       ├── ILock.hpp         # Synchronization abstraction
│       └── ITransport.hpp    # Communication abstraction
├── src/                       # Implementation files
│   ├── main.cpp              # Main application entry point
│   ├── main_hal.cpp          # HAL integration and main loop
│   ├── pipeline/             # Pipeline implementation
│   ├── scheduler/            # Scheduler implementation
│   ├── state/                # State machine implementation
│   ├── comm/                 # Communication implementation
│   ├── diagnostics/          # Diagnostics implementation
│   ├── protocol/             # Protocol layer implementation
│   └── hal/esp32/            # ESP32 HAL implementations
├── test/                     # Testing framework
│   ├── mock/                 # Mock HAL implementations
│   ├── SchedulerTest.cpp     # Scheduler logic tests
│   ├── CommandTest.cpp       # Command parsing tests
│   └── run_tests.sh          # Test runner script
├── DOC/                      # Documentation
│   ├── ARCHITECTURE.md       # Detailed architecture documentation
│   ├── REFACTOR_PLAN.md      # Refactoring plan and progress
│   └── REFACTOR_STEPS.md     # Step-by-step refactoring guide
└── SyncTask.cpp              # Legacy monolithic file (being refactored)
```

## Features

*   **Stepper Motor Control**: Manages a stepper motor for the rotating platform with a dynamically adjustable frequency.
*   **Pulse-Based Synchronization**: Uses the stepper motor's pulse count as the master clock for all system events, ensuring high-precision timing.
*   **Object Detection**: Employs a digital gate sensor to detect objects on the platform and track their position.
*   **Multi-Stage Inspection Pipeline**:
    *   Schedules and executes events for multiple inspection stages (e.g., Camera 1, Camera 2).
    *   Triggers cameras and lighting strobes at precise pulse offsets.
    *   Actuates up to three different selectors to sort objects based on results.
*   **System State Machine**: A robust state machine manages the system's operational state (e.g., `INIT`, `IDLE`, `INSPECTION_MODE_READY`, `INSPECTION_MODE_ERROR`).
*   **Serial Communication Protocol**: Communicates with a host PC or controller via a JSON-based serial protocol.
*   **Error Handling**: Logs and reports system errors.

## Hardware Requirements

The firmware is designed for an ESP32 and expects the following peripheral connections, which are defined in `src/config.h`:

*   **Stepper Motor Driver**: Connected to `STEPPER_PLS_PIN`, `STEPPER_DIR_PIN`, and `STEPPER_EN_PIN`.
*   **Gate Sensor**: A digital sensor connected to `PIN_I_GATE` to detect the presence of objects.
*   **Camera Triggers**: Digital outputs for triggering Camera 1 (`PIN_O_CAM1`) and Camera 2 (`PIN_O_CAM2`).
*   **Lighting Strobes**: Digital outputs for controlling lights for Camera 1 (`PIN_O_L1A`) and Camera 2 (`PIN_O_L2A`).
*   **Selectors**: Three digital outputs (`PIN_O_SEL1`, `PIN_O_SEL2`, `PIN_O_SEL3`) for actuating sorting mechanisms.
*   **Feeder Control**: A digital output (`FEEDER_PIN`) to control an object feeder.

## Communication Protocol

The device communicates over Serial at a baud rate of **115200**. The protocol is primarily JSON-based. All commands can include an `"id"` field, which will be echoed in the response for command-response matching.

### Key Commands

A response of `{"ack":true, "id":...}` indicates success.

*   **Get/Set Configuration**
    *   `{"type":"get_setup"}`: Retrieves the current machine configuration, including pulse offsets and error history.
    *   `{"type":"set_setup", "plateFreq": 1000, "stage_pulse_offset": {...}}`: Sets machine parameters.

*   **State Control**
    *   `{"type":"enter_insp_mode"}`: Puts the system into `INSPECTION_MODE_READY`.
    *   `{"type":"exit_insp_mode"}`: Returns the system to the `IDLE` state.
    *   `{"type":"clear_error"}`: Clears an error state and returns to `IDLE`.

*   **Inspection & Sorting**
    *   `{"type":"report", "tid": 123, "cat": 1}`: Reports the inspection result (`cat`) for a specific object (`tid`). `cat` values 1 and 2 correspond to `SEL1` and `SEL2`.
    *   `{"type":"set_sel1_cd", "count": 100}`: Sets a countdown for `SEL1`. When the count reaches zero, the system enters an error state. A count of `-1` disables the countdown.

*   **Stepper Control**
    *   `{"type":"stepper_enable"}` / `{"type":"stepper_disable"}`: Enables or disables the stepper motor output.

*   **Diagnostics & Debugging**
    *   `{"type":"PING"}`: Responds with `{"type":"PONG"}` to check connectivity.
    *   `{"type":"get_running_stat"}`: Gets real-time statistics (object counts, errors, current state).
    *   `{"type":"reset_running_stat"}`: Resets the sorting counters.
    *   `{"type":"clear_error_history"}`: Clears the persistent error log.
    *   `{"type":"trig_phamton_pulse"}`: Manually injects a fake object pulse for testing.

### Asynchronous Messages from Device

*   **Trigger Info**: `{"type":"bT", "tidx":1, "usH":..., "usL":..., "tid":...}`
    *   Sent whenever a camera is triggered. `tidx` is the camera index (1 or 2), `usH`/`usL` are the high/low parts of a 64-bit timestamp, and `tid` is the unique object ID.
*   **System Info**: `{"type":"systemInfo", "state":..., "log":...}`
    *   Sent on system state changes.
*   **Debug Messages**: `{"dbg":"..."}`
    *   General debug information.

## System States

The controller operates using a state machine:

*   **`INIT`**: System startup and initialization.
*   **`IDLE`**: The system is waiting for commands. The motor can be set to a specific frequency (`plateFreq`) for setup purposes, but no inspection occurs.
*   **`INSPECTION_MODE_READY`**: The main operational state. The system is running at the configured speed, detecting objects, triggering cameras, and actuating selectors.
*   **`INSPECTION_MODE_TEST`**: A test mode for inspection.
*   **`INSPECTION_MODE_ERROR`**: An error has occurred (e.g., object missed inspection, selector countdown reached). The system halts until `clear_error` is received.

## Building and Development

### Firmware Build
The firmware is built using PlatformIO:

```bash
# Build for ESP32
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

### Host-Side Testing
The modular architecture enables comprehensive testing without hardware:

```bash
# Run all tests
cd test/
./run_tests.sh

# Run specific tests
make test-scheduler
make test-command

# Build tests manually
make all
```

### Development Workflow
1. **Logic Development**: Develop and test core logic using the host-side test framework
2. **Hardware Integration**: Integrate with ESP32 HAL implementations
3. **System Testing**: Test complete system on hardware
4. **Validation**: Verify timing and performance characteristics

## Refactoring Status

The project has undergone a comprehensive refactoring to improve maintainability and portability:

- ✅ **Stages 0-4**: Core headers, Pipeline & Scheduler, Gate Sensing, State Machine, Message Bus & Diagnostics
- ✅ **Stages 6-8**: Protocol Layer Cleanup, HAL Abstraction, Main Loop Simplification
- ✅ **Stage 7A**: ISR verification and platform neutrality
- ✅ **Stage 9**: Host-side testing framework
- ✅ **Stage 10**: Documentation and architecture

**Current Status**: 100% complete (11/11 stages done)

For detailed refactoring information, see [DOC/REFACTOR_PLAN.md](DOC/REFACTOR_PLAN.md) and [DOC/REFACTOR_STEPS.md](DOC/REFACTOR_STEPS.md).

## Contributing

### Code Organization
- **Headers**: Public interfaces in `include/`
- **Implementation**: Private implementation in `src/`
- **Platform Code**: Platform-specific code in `src/hal/`
- **Tests**: Test code in `test/`

### Development Guidelines
- Follow the established modular architecture
- Use HAL interfaces for hardware access
- Write tests for new functionality
- Maintain platform independence in core logic
- Update documentation for architectural changes

