# uInspESP32 - Micro Inspection System Controller

This repository contains the firmware for an ESP32-based controller designed for a high-speed, circular inspection system. It precisely controls a rotating platform, detects objects, triggers cameras for inspection, and actuates selectors (e.g., air jets) to sort objects based on inspection results communicated from a host system.

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
    *   `{"type":"set_setup", "plate_freq": 1000, "stage_pulse_offset": {...}}`: Sets machine parameters.

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
    *   `{"type":"ping"}`: Responds with `{"type":"pong"}` to check connectivity.
    *   `{"type":"get_running_stat"}`: Gets real-time statistics (object counts, errors, current state).
    *   `{"type":"reset_running_stat"}`: Resets the sorting counters.
    *   `{"type":"clear_error_history"}`: Clears the persistent error log.
    *   `{"type":"trig_phantom_pulse"}`: Manually injects a fake object pulse for testing.

### Asynchronous Messages from Device

*   **Trigger Info**: `{"type":"bT", "tidx":1, "usH":..., "usL":..., "tid":...}`
    *   Sent whenever a camera is triggered. `tidx` is the camera index (1 or 2), `usH`/`usL` are the high/low parts of a 64-bit timestamp, and `tid` is the unique object ID.
*   **System Info**: `{"type":"system_info", "state":..., "log":...}`
    *   Sent on system state changes.
*   **Debug Messages**: `{"dbg":"..."}`
    *   General debug information.

## System States

The controller operates using a state machine:

*   **`INIT`**: System startup and initialization.
*   **`IDLE`**: The system is waiting for commands. The motor can be set to a specific frequency (`plate_freq`) for setup purposes, but no inspection occurs.
*   **`INSPECTION_MODE_READY`**: The main operational state. The system is running at the configured speed, detecting objects, triggering cameras, and actuating selectors.
*   **`INSPECTION_MODE_TEST`**: A test mode for inspection.
*   **`INSPECTION_MODE_ERROR`**: An error has occurred (e.g., object missed inspection, selector countdown reached). The system halts until `clear_error` is received.

## Building and Flashing

This project is configured for PlatformIO.

1.  **Install PlatformIO**: Follow the instructions for your editor (e.g., VSCode).
2.  **Build**:
    ```sh
    platformio run
    ```
3.  **Upload**:
    ```sh
    platformio run --target upload
    ```

## Project Structure

```
├── include/            # Header files for the project
├── lib/                # Project-specific libraries
│   └── DataLayer/      # Communication protocol implementation
├── src/
│   ├── main.cpp        # Main setup() and loop() functions
│   ├── config.h        # Pin definitions, constants, and global settings
│   ├── communication.* # Handles JSON command parsing and responses
│   ├── gate_sensor.*   # Logic for the object gate sensor
│   ├── pipeline.*      # Core inspection pipeline and event scheduling
│   ├── state_machine.* # System state machine logic
│   └── stepper.*       # Stepper motor control and timer ISR
├── platformio.ini      # PlatformIO project configuration
└── README.md           # This file
```