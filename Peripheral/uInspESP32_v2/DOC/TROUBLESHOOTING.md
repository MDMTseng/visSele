# uInspESP32 Troubleshooting Guide

This guide helps diagnose and resolve common issues with the uInspESP32 system.

## Table of Contents

1. [Build Issues](#build-issues)
2. [Runtime Issues](#runtime-issues)
3. [Hardware Issues](#hardware-issues)
4. [Communication Issues](#communication-issues)
5. [Performance Issues](#performance-issues)
6. [Debugging Tools](#debugging-tools)
7. [Common Error Codes](#common-error-codes)
8. [Recovery Procedures](#recovery-procedures)

## Build Issues

### Compilation Errors

#### Error: "No such file or directory"
**Symptoms**: Build fails with missing header files
**Causes**:
- Incorrect include paths
- Missing source files
- PlatformIO configuration issues

**Solutions**:
1. Check `platformio.ini` configuration
2. Verify all source files are present
3. Clean and rebuild: `pio run --target clean && pio run`

#### Error: "Undefined reference to"
**Symptoms**: Linker errors for missing function implementations
**Causes**:
- Missing source files in build
- Incorrect function signatures
- Missing HAL implementations

**Solutions**:
1. Verify all `.cpp` files are included in build
2. Check function signatures match declarations
3. Ensure HAL implementations are complete

#### Error: "Multiple definition of"
**Symptoms**: Linker errors for duplicate symbols
**Causes**:
- Multiple definitions of same function
- Header files with implementations
- Incorrect `extern` declarations

**Solutions**:
1. Move implementations to `.cpp` files
2. Use `inline` for header-only implementations
3. Check for duplicate function definitions

### Platform-Specific Build Issues

#### ESP32 Build Issues
**Common Problems**:
- Flash size exceeded
- RAM allocation failures
- Timer configuration errors

**Solutions**:
1. **Flash Size**: Reduce code size or enable compression
   ```ini
   build_flags = -DCORE_DEBUG_LEVEL=0 -Os
   ```

2. **RAM Issues**: Optimize memory usage
   ```cpp
   // Use constexpr for constants
   constexpr size_t BUFFER_SIZE = 256;
   
   // Use static allocation
   static uint8_t buffer[BUFFER_SIZE];
   ```

3. **Timer Issues**: Check timer configuration
   ```cpp
   // Verify timer frequency is within limits
   if (frequency_hz > 0 && frequency_hz <= 1000000) {
       // Configure timer
   }
   ```

#### STM32 Build Issues
**Common Problems**:
- Missing HAL libraries
- Incorrect clock configuration
- Memory layout issues

**Solutions**:
1. **HAL Libraries**: Install STM32 HAL packages
   ```bash
   pio lib install "STM32duino STM32F4"
   ```

2. **Clock Configuration**: Verify system clock settings
   ```cpp
   // Check clock configuration in BoardConfig.hpp
   constexpr uint32_t SYSTEM_CLOCK_FREQ = 168000000; // 168 MHz
   ```

3. **Memory Layout**: Check linker script configuration
   ```cpp
   // Verify memory regions in platformio_stm32.ini
   board_build.ldscript = ldscript.ld
   ```

## Runtime Issues

### System Initialization Failures

#### Error: "System failed to initialize"
**Symptoms**: System doesn't start properly
**Causes**:
- HAL initialization failure
- Module initialization errors
- Hardware connection issues

**Debugging Steps**:
1. Check serial output for error messages
2. Verify hardware connections
3. Test individual modules

**Solutions**:
```cpp
// Add debug logging to initialization
void custom_hal_init() {
    hal.logger().log(ILogger::LogLevel::INFO, "Starting initialization...");
    
    // Initialize each module with error checking
    if (!stepperController->init(...)) {
        hal.logger().log(ILogger::LogLevel::ERROR, "StepperController init failed");
        return;
    }
    
    hal.logger().log(ILogger::LogLevel::INFO, "Initialization complete");
}
```

#### Error: "State machine stuck in INIT"
**Symptoms**: System remains in INIT state
**Causes**:
- Missing INIT_OK action
- State machine callback issues
- Module dependency failures

**Solutions**:
1. Verify INIT_OK action is called
2. Check state machine callbacks
3. Ensure all dependencies are initialized

```cpp
// Ensure INIT_OK is called after initialization
void custom_hal_init() {
    // ... initialization code ...
    
    // Apply INIT_OK action
    stateMachine.applyAction(SYS_STATE_ACT::INIT_OK);
}
```

### Object Detection Issues

#### Problem: Objects not detected
**Symptoms**: Gate sensor doesn't detect objects
**Causes**:
- Incorrect pin configuration
- Hardware connection issues
- Debounce settings too strict

**Debugging Steps**:
1. Check pin configuration
2. Verify hardware connections
3. Test with oscilloscope
4. Adjust debounce settings

**Solutions**:
```cpp
// Check gate sensor configuration
gateSensor.init(PIN_I_GATE, true, minWidth, maxWidth, 1);

// Add debug logging
void onObjectDetected(uint32_t tid, uint32_t pulse, uint32_t width) {
    hal.logger().log(ILogger::LogLevel::INFO, 
        "Object detected: tid=" + std::to_string(tid) + 
        ", pulse=" + std::to_string(pulse) + 
        ", width=" + std::to_string(width));
}
```

#### Problem: False object detections
**Symptoms**: Objects detected when none present
**Causes**:
- Electrical noise
- Loose connections
- Incorrect debounce settings

**Solutions**:
1. Improve electrical connections
2. Add filtering
3. Adjust debounce parameters

```cpp
// Increase debounce time
gateSensor.init(PIN_I_GATE, true, minWidth, maxWidth, 5); // 5ms debounce

// Add noise filtering
if (width < MIN_OBJECT_WIDTH || width > MAX_OBJECT_WIDTH) {
    // Ignore false detections
    return;
}
```

### Stepper Motor Issues

#### Problem: Stepper not moving
**Symptoms**: Motor doesn't respond to commands
**Causes**:
- Enable pin not set
- Incorrect pin configuration
- Power supply issues

**Debugging Steps**:
1. Check enable pin state
2. Verify pin configuration
3. Test with multimeter
4. Check power supply

**Solutions**:
```cpp
// Ensure stepper is enabled
stepperController->enableStepper(true);

// Check pin configuration
hal.gpio().setPinMode(STEPPER_PLS_PIN, IGpio::PinMode::PIN_OUTPUT);
hal.gpio().setPinMode(STEPPER_DIR_PIN, IGpio::PinMode::PIN_OUTPUT);
hal.gpio().setPinMode(STEPPER_EN_PIN, IGpio::PinMode::PIN_OUTPUT);
```

#### Problem: Stepper frequency issues
**Symptoms**: Motor runs at wrong speed
**Causes**:
- Incorrect frequency calculation
- Timer configuration errors
- Acceleration settings

**Solutions**:
```cpp
// Verify frequency calculation
float target_freq = 1000.0f; // 1000 Hz
stepperController->setTargetFrequency(target_freq);

// Check if frequency is stable
if (stepperController->isStable()) {
    hal.logger().log(ILogger::LogLevel::INFO, 
        "Stepper frequency stable: " + 
        std::to_string(stepperController->getCurrentFrequency()) + " Hz");
}
```

## Hardware Issues

### GPIO Problems

#### Problem: GPIO not responding
**Symptoms**: Pins don't change state
**Causes**:
- Incorrect pin mode
- Hardware connection issues
- Pin conflicts

**Solutions**:
```cpp
// Verify pin mode
hal.gpio().setPinMode(PIN_O_CAM1, IGpio::PinMode::PIN_OUTPUT);

// Test GPIO functionality
hal.gpio().digitalWrite(PIN_O_CAM1, true);
delay(100);
hal.gpio().digitalWrite(PIN_O_CAM1, false);
```

#### Problem: Input pins reading incorrectly
**Symptoms**: Input values don't match expected state
**Causes**:
- Missing pullup/pulldown
- Incorrect voltage levels
- Hardware issues

**Solutions**:
```cpp
// Enable pullup for input pins
hal.gpio().setPinMode(PIN_I_GATE, IGpio::PinMode::PIN_INPUT_PULLUP);

// Test input reading
bool gate_state = hal.gpio().digitalRead(PIN_I_GATE);
hal.logger().log(ILogger::LogLevel::INFO, 
    "Gate state: " + std::to_string(gate_state));
```

### Timer Issues

#### Problem: Timer not triggering
**Symptoms**: ISR not called
**Causes**:
- Timer not started
- Incorrect frequency
- ISR not registered

**Solutions**:
```cpp
// Verify timer initialization
if (!hal.timerTickSource().init(1000.0f)) {
    hal.logger().log(ILogger::LogLevel::ERROR, "Timer init failed");
    return;
}

// Register ISR callback
hal.timerTickSource().registerTickCallback(hal_onTimer);

// Start timer
hal.timerTickSource().start();
```

#### Problem: Timer frequency incorrect
**Symptoms**: Timing is off
**Causes**:
- Incorrect frequency calculation
- Clock configuration issues
- Timer resolution limits

**Solutions**:
```cpp
// Verify frequency calculation
float target_freq = 1000.0f;
hal.timerTickSource().setFrequencyHz(target_freq);

// Check actual frequency
float actual_freq = hal.timerTickSource().getFrequencyHz();
hal.logger().log(ILogger::LogLevel::INFO, 
    "Timer frequency: " + std::to_string(actual_freq) + " Hz");
```

## Communication Issues

### Serial Communication Problems

#### Problem: No serial output
**Symptoms**: No messages received
**Causes**:
- Incorrect baud rate
- Hardware connection issues
- Buffer overflow

**Solutions**:
1. **Check baud rate**:
   ```cpp
   Serial.begin(115200);
   ```

2. **Verify connections**:
   - TX to RX
   - RX to TX
   - Ground connection

3. **Check buffer size**:
   ```cpp
   Serial.setRxBufferSize(500);
   ```

#### Problem: Corrupted data
**Symptoms**: Garbled messages
**Causes**:
- Incorrect baud rate
- Electrical noise
- Buffer overflow

**Solutions**:
1. **Verify baud rate** on both ends
2. **Improve electrical connections**
3. **Add error checking**:
   ```cpp
   if (serialTransport.available() > 0) {
       uint8_t buffer[256];
       size_t bytesRead = serialTransport.read(buffer, sizeof(buffer));
       if (bytesRead > 0) {
           // Process data with error checking
           processReceivedData(buffer, bytesRead);
       }
   }
   ```

### JSON Protocol Issues

#### Problem: JSON parsing errors
**Symptoms**: Invalid JSON responses
**Causes**:
- Malformed JSON
- Buffer overflow
- Encoding issues

**Solutions**:
```cpp
// Add JSON validation
bool isValidJson(const std::string& json) {
    // Basic JSON validation
    return json.find('{') != std::string::npos && 
           json.find('}') != std::string::npos;
}

// Process JSON with error handling
if (isValidJson(receivedData)) {
    // Parse JSON
    processJsonCommand(receivedData);
} else {
    hal.logger().log(ILogger::LogLevel::ERROR, "Invalid JSON received");
}
```

## Performance Issues

### Timing Problems

#### Problem: ISR timing issues
**Symptoms**: System timing is off
**Causes**:
- ISR too slow
- Blocking operations in ISR
- Timer resolution issues

**Solutions**:
1. **Minimize ISR code**:
   ```cpp
   void hal_onTimer() {
       SYS_STEP_COUNT++;
       gateSensor.tick(SYS_STEP_COUNT);
       Run_ACTS(SYS_STEP_COUNT);
       // Keep ISR minimal and fast
   }
   ```

2. **Avoid blocking operations**:
   ```cpp
   // Don't do this in ISR
   // hal.logger().log(...);  // Too slow
   
   // Do this instead
   // Set flag for main loop to handle
   ```

#### Problem: Main loop too slow
**Symptoms**: System responsiveness issues
**Causes**:
- Heavy operations in main loop
- Blocking operations
- Memory allocation

**Solutions**:
1. **Optimize main loop**:
   ```cpp
   void custom_hal_update() {
       // Keep main loop fast
       readSerialIntoProtocol();
       flushOutboundMessages(serialTransport);
       stepperController->update();
       stateMachine.pump();
       yield(); // Yield to other tasks
   }
   ```

2. **Use non-blocking operations**:
   ```cpp
   // Non-blocking serial read
   if (serialTransport.available() > 0) {
       // Process available data
   }
   ```

### Memory Issues

#### Problem: Out of memory
**Symptoms**: System crashes or hangs
**Causes**:
- Memory leaks
- Large allocations
- Stack overflow

**Solutions**:
1. **Use static allocation**:
   ```cpp
   // Instead of dynamic allocation
   // uint8_t* buffer = new uint8_t[256];
   
   // Use static allocation
   static uint8_t buffer[256];
   ```

2. **Monitor memory usage**:
   ```cpp
   void logMemoryUsage() {
       hal.logger().log(ILogger::LogLevel::INFO, 
           "Free heap: " + std::to_string(ESP.getFreeHeap()));
   }
   ```

## Debugging Tools

### Serial Debugging

#### Enable Debug Logging
```cpp
// Set debug level
hal.logger().setLogLevel(ILogger::LogLevel::DEBUG);

// Add debug messages
hal.logger().log(ILogger::LogLevel::DEBUG, "Debug message");
```

#### Monitor System State
```cpp
void logSystemState() {
    hal.logger().log(ILogger::LogLevel::INFO, 
        "State: " + std::to_string(static_cast<int>(stateMachine.getCurrentState())));
    hal.logger().log(ILogger::LogLevel::INFO, 
        "Pulse count: " + std::to_string(SYS_STEP_COUNT));
    hal.logger().log(ILogger::LogLevel::INFO, 
        "Stepper enabled: " + std::to_string(stepperController->isStepperEnabled()));
}
```

### Hardware Debugging

#### GPIO Testing
```cpp
void testGPIO() {
    // Test output pins
    hal.gpio().digitalWrite(PIN_O_CAM1, true);
    delay(100);
    hal.gpio().digitalWrite(PIN_O_CAM1, false);
    
    // Test input pins
    bool gate_state = hal.gpio().digitalRead(PIN_I_GATE);
    hal.logger().log(ILogger::LogLevel::INFO, 
        "Gate state: " + std::to_string(gate_state));
}
```

#### Timer Testing
```cpp
void testTimer() {
    static uint32_t last_pulse = 0;
    uint32_t current_pulse = SYS_STEP_COUNT;
    
    if (current_pulse != last_pulse) {
        hal.logger().log(ILogger::LogLevel::INFO, 
            "Pulse: " + std::to_string(current_pulse));
        last_pulse = current_pulse;
    }
}
```

## Common Error Codes

### System Errors

| Error Code | Description | Solution |
|------------|-------------|----------|
| `GATE_SENSOR_ERROR` | Gate sensor malfunction | Check hardware connections |
| `STEPPER_ERROR` | Stepper motor issues | Verify enable pin and power |
| `COMMUNICATION_ERROR` | Serial communication failure | Check baud rate and connections |
| `STATE_MACHINE_ERROR` | State transition error | Check state machine logic |
| `TIMER_ERROR` | Timer configuration error | Verify timer settings |
| `MEMORY_ERROR` | Memory allocation failure | Optimize memory usage |

### Recovery Procedures

#### System Reset
```cpp
void resetSystem() {
    // Stop all operations
    stepperController->enableStepper(false);
    hal.timerTickSource().stop();
    
    // Reset modules
    stateMachine.init();
    scheduler.reset();
    pipeline.reset();
    
    // Clear error history
    Diagnostics::clearErrorHistory();
    
    // Restart system
    stateMachine.applyAction(SYS_STATE_ACT::INIT_OK);
}
```

#### Error Recovery
```cpp
void handleError(ErrorCode error) {
    switch (error) {
        case ErrorCode::GATE_SENSOR_ERROR:
            // Reset gate sensor
            gateSensor.reset();
            break;
            
        case ErrorCode::STEPPER_ERROR:
            // Stop stepper and reset
            stepperController->enableStepper(false);
            stepperController->setTargetFrequency(0.0f);
            break;
            
        case ErrorCode::COMMUNICATION_ERROR:
            // Clear communication buffers
            serialTransport.flush();
            break;
            
        default:
            // General error handling
            resetSystem();
            break;
    }
}
```

## Prevention Strategies

### Code Quality
1. **Use constexpr for constants**
2. **Avoid dynamic allocation in ISR**
3. **Add error checking for all operations**
4. **Use static analysis tools**

### Testing
1. **Unit test individual modules**
2. **Integration test complete system**
3. **Performance test timing-critical operations**
4. **Stress test error conditions**

### Monitoring
1. **Log system state regularly**
2. **Monitor memory usage**
3. **Track error rates**
4. **Profile performance**

This troubleshooting guide should help resolve most common issues with the uInspESP32 system. For additional support, refer to the source code, test files, and architecture documentation.
