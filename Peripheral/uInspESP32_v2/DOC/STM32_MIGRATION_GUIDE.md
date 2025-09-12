# STM32 Migration Guide

This guide outlines the process for porting the uInspESP32 firmware from ESP32 to STM32 platforms.

## Overview

The uInspESP32 has been designed with platform independence in mind, making it relatively straightforward to port to STM32. The modular architecture with HAL interfaces allows for clean separation between platform-specific and platform-independent code.

## Prerequisites

### Development Environment
- **STM32CubeIDE** or **PlatformIO** with STM32 support
- **STM32 HAL libraries** for the target microcontroller
- **Debug probe** (ST-Link, J-Link, etc.)
- **Target hardware** (STM32F4, STM32F7, STM32H7, or STM32G4)

### Knowledge Requirements
- STM32 HAL library usage
- STM32 timer and interrupt configuration
- FreeRTOS on STM32
- STM32 GPIO configuration
- UART/DMA communication on STM32

## Migration Steps

### Step 1: Hardware Analysis

1. **Identify Target STM32 Variant**
   - STM32F4: Good balance of performance and cost
   - STM32F7: Higher performance, more peripherals
   - STM32H7: Highest performance, advanced features
   - STM32G4: Motor control optimized

2. **Verify Hardware Requirements**
   - **Timer Resolution**: Minimum 1µs for precise timing
   - **GPIO Speed**: Fast GPIO for stepper pulses
   - **UART**: 115200 baud capability
   - **Memory**: Sufficient RAM/Flash for application
   - **Interrupts**: Low-latency interrupt handling

3. **Pin Mapping Analysis**
   - Map ESP32 pins to STM32 equivalents
   - Consider voltage levels (3.3V vs 5V)
   - Verify current drive capabilities
   - Plan for any level shifters needed

### Step 2: Development Environment Setup

1. **STM32CubeIDE Setup**
   ```bash
   # Download and install STM32CubeIDE
   # Create new project for target STM32
   # Configure system clock and peripherals
   ```

2. **PlatformIO Setup** (Alternative)
   ```ini
   # Use platformio_stm32.ini template
   # Select appropriate STM32 board configuration
   # Install required libraries
   ```

3. **HAL Library Integration**
   - Include STM32 HAL headers
   - Configure FreeRTOS for STM32
   - Set up build system for STM32

### Step 3: HAL Implementation

1. **STM32Gpio Implementation**
   ```cpp
   // Implement GPIO operations using STM32 HAL
   void setPinMode(uint8_t pin, PinMode mode) {
       GPIO_InitTypeDef GPIO_InitStruct = {0};
       // Configure pin mode using STM32 HAL
       HAL_GPIO_Init(getPort(pin), &GPIO_InitStruct);
   }
   ```

2. **STM32TimerTickSource Implementation**
   ```cpp
   // Implement timer using STM32 timer peripherals
   bool init(float frequency_hz) {
       // Configure timer for precise frequency
       // Set up interrupt handling
       // Calculate timer period based on system clock
   }
   ```

3. **STM32Clock Implementation**
   ```cpp
   // Implement time functions using STM32 SysTick
   uint64_t micros() {
       // Use SysTick for microsecond timing
       return HAL_GetTick() * 1000 + getMicroseconds();
   }
   ```

4. **STM32StepperDriver Implementation**
   ```cpp
   // Implement stepper control using STM32 GPIO
   void step(uint64_t timestamp) {
       // Fast GPIO toggle for stepper pulse
       HAL_GPIO_TogglePin(STEPPER_PORT, STEPPER_PIN);
   }
   ```

5. **STM32Logger Implementation**
   ```cpp
   // Implement logging using STM32 UART
   void log(LogLevel level, const std::string& message) {
       // Send log messages via UART
       HAL_UART_Transmit(&huart, message.data(), message.length(), 1000);
   }
   ```

6. **UartTransport Implementation**
   ```cpp
   // Implement UART communication
   size_t read(uint8_t* buffer, size_t size) {
       // Use UART with DMA or interrupt-driven reception
       return HAL_UART_Receive(&huart, buffer, size, 1000);
   }
   ```

### Step 4: Board Configuration

1. **Update BoardConfig.hpp**
   ```cpp
   // Define STM32-specific pin mappings
   namespace STM32 {
       constexpr int STEPPER_PLS_PIN = 8;   // PA8
       constexpr int STEPPER_DIR_PIN = 9;   // PA9
       constexpr int PIN_O_CAM1 = 1;        // PA1
       // ... other pins
   }
   ```

2. **System Clock Configuration**
   ```cpp
   // Configure system clock for optimal performance
   // Set up APB1/APB2 frequencies
   // Configure timer clock sources
   ```

3. **Interrupt Priority Configuration**
   ```cpp
   // Set up interrupt priorities
   // Timer ISR: Highest priority
   // UART ISR: Medium priority
   // GPIO ISR: Low priority
   ```

### Step 5: Build System Configuration

1. **Update PlatformIO Configuration**
   ```ini
   [env:stm32f4]
   platform = ststm32
   board = disco_f407vg
   framework = stm32cube
   build_flags = -DTARGET_STM32=1 -DSTM32F4=1
   ```

2. **Memory Layout Configuration**
   ```cpp
   // Configure memory layout in linker script
   // Set up stack and heap sizes
   // Configure FreeRTOS memory allocation
   ```

3. **Compiler Optimization**
   ```ini
   build_flags = -O3 -DNDEBUG
   ```

### Step 6: Testing and Validation

1. **Host-Side Testing**
   ```bash
   # Run existing host-side tests
   cd ../uInspESP32_tests
   ./run_tests.sh
   ```

2. **Hardware Testing**
   - Test GPIO operations
   - Validate timer accuracy
   - Check UART communication
   - Verify stepper motor control

3. **Performance Validation**
   - Measure ISR timing
   - Check timing jitter
   - Validate communication latency
   - Test system stability

### Step 7: Optimization and Tuning

1. **Timer Optimization**
   - Fine-tune timer resolution
   - Optimize ISR performance
   - Minimize timing jitter

2. **GPIO Optimization**
   - Optimize GPIO toggle speed
   - Minimize GPIO latency
   - Ensure reliable signal levels

3. **Communication Optimization**
   - Optimize UART buffer sizes
   - Implement DMA if needed
   - Improve error handling

## Hardware Considerations

### Pin Mapping Strategy

| Function | ESP32 Pin | STM32 Pin | Notes |
|----------|-----------|-----------|-------|
| Stepper Pulse | 22 | PA8 | Fast GPIO required |
| Stepper Direction | 23 | PA9 | Standard GPIO |
| Stepper Enable | 13 | PA10 | Standard GPIO |
| Camera 1 Trigger | 17 | PA1 | Fast GPIO preferred |
| Camera 2 Trigger | 19 | PA3 | Fast GPIO preferred |
| Gate Sensor | 27 | PA7 | Input with pull-up |
| UART TX | - | PA2 | Hardware UART |
| UART RX | - | PA3 | Hardware UART |

### Timing Requirements

| Requirement | ESP32 | STM32 Target | Notes |
|-------------|-------|--------------|-------|
| Timer Resolution | 0.125µs | 1µs | STM32 may need optimization |
| ISR Latency | <5µs | <10µs | Acceptable for application |
| GPIO Toggle | <1µs | <2µs | Should be achievable |
| UART Latency | <1ms | <2ms | May need DMA optimization |

### Memory Requirements

| Component | ESP32 Usage | STM32 Target | Notes |
|-----------|-------------|--------------|-------|
| Application Code | ~290KB | <512KB | Should fit in most STM32 |
| RAM Usage | ~45KB | <128KB | May need optimization |
| Stack Size | 8KB | 4KB | Adjust based on needs |
| Heap Size | 32KB | 16KB | May need tuning |

## Performance Optimization

### Timer Optimization
```cpp
// Use high-frequency timer for better resolution
// Configure timer for maximum precision
// Minimize ISR overhead
```

### GPIO Optimization
```cpp
// Use direct register access for fast GPIO
// Minimize function call overhead
// Use bit-banding if available
```

### Communication Optimization
```cpp
// Implement DMA for UART
// Use ring buffers for data
// Optimize JSON parsing
```

## Troubleshooting

### Common Issues

1. **Timer Resolution**
   - Problem: Insufficient timer resolution
   - Solution: Use higher-frequency timer or optimize calculations

2. **GPIO Latency**
   - Problem: Slow GPIO operations
   - Solution: Use direct register access or bit-banding

3. **UART Reliability**
   - Problem: Data loss or corruption
   - Solution: Implement proper buffering and error handling

4. **Memory Constraints**
   - Problem: Insufficient RAM/Flash
   - Solution: Optimize code size and memory usage

### Debugging Tools

1. **STM32CubeIDE Debugger**
   - Real-time variable monitoring
   - Breakpoint debugging
   - Performance profiling

2. **Logic Analyzer**
   - GPIO timing analysis
   - UART communication debugging
   - Timer accuracy validation

3. **Oscilloscope**
   - Signal quality analysis
   - Timing measurements
   - Noise analysis

## Validation Checklist

- [ ] GPIO operations working correctly
- [ ] Timer accuracy within specifications
- [ ] UART communication reliable
- [ ] Stepper motor control functional
- [ ] Gate sensor detection working
- [ ] Camera trigger timing accurate
- [ ] JSON protocol communication working
- [ ] System stability under load
- [ ] Memory usage within limits
- [ ] Performance meets requirements

## Future Enhancements

### Potential STM32-Specific Features

1. **CAN Bus Communication**
   - Industrial-grade communication
   - Multi-device networking
   - Error detection and recovery

2. **Ethernet Support**
   - Network-based communication
   - Remote monitoring capabilities
   - Web-based configuration

3. **Advanced Timer Features**
   - PWM generation for motor control
   - Encoder interface support
   - Advanced timing modes

4. **Analog Capabilities**
   - ADC for sensor readings
   - DAC for analog outputs
   - Analog signal processing

## Conclusion

The modular architecture of uInspESP32 makes STM32 porting straightforward. The key is careful implementation of the HAL interfaces and thorough testing of timing-critical functions. With proper attention to hardware requirements and performance optimization, the STM32 port can provide excellent performance and reliability.
