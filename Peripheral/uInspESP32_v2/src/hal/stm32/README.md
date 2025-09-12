# STM32 HAL Implementation

This directory contains STM32-specific implementations of the HAL interfaces for the uInspESP32 project.

## Status: **PLANNED - NOT IMPLEMENTED**

This directory is prepared for future STM32 portability but does not contain actual implementations yet. The files listed below are placeholders that would need to be implemented when porting to STM32.

## Planned Files

### Core HAL Implementations
- `STM32HAL.hpp/.cpp` - Main HAL implementation for STM32
- `STM32Gpio.hpp/.cpp` - GPIO operations using STM32 HAL
- `STM32TimerTickSource.hpp/.cpp` - Timer interrupt handling
- `STM32StepperDriver.hpp/.cpp` - Stepper motor control
- `STM32Clock.hpp/.cpp` - Time functions (micros, millis, delay)
- `STM32Logger.hpp/.cpp` - Logging implementation
- `STM32Lock.hpp/.cpp` - Synchronization primitives
- `UartTransport.hpp/.cpp` - UART communication transport

### Board-Specific Configurations
- `STM32F4HAL.hpp` - STM32F4-specific implementation
- `STM32F7HAL.hpp` - STM32F7-specific implementation  
- `STM32H7HAL.hpp` - STM32H7-specific implementation
- `STM32G4HAL.hpp` - STM32G4-specific implementation

## Implementation Requirements

When implementing STM32 support, the following considerations apply:

### Timer Requirements
- **Resolution**: Minimum 1µs resolution for precise timing
- **Frequency**: Support for stepper motor frequencies up to 50kHz
- **ISR Performance**: < 10µs ISR execution time
- **Jitter**: < 1µs timing jitter for reliable scheduling

### GPIO Requirements
- **Speed**: Fast GPIO toggle capability for stepper pulses
- **Current**: Sufficient drive capability for opto-isolators
- **Voltage**: 3.3V or 5V compatibility based on target hardware
- **Protection**: Input protection for gate sensor signals

### Communication Requirements
- **UART**: 115200 baud, 8N1 configuration
- **Buffering**: Ring buffer for reliable data reception
- **Interrupts**: Interrupt-driven UART handling
- **DMA**: Optional DMA support for improved performance

### Memory Requirements
- **RAM**: Minimum 64KB available for application
- **Flash**: Minimum 256KB for firmware storage
- **Stack**: Sufficient stack space for FreeRTOS tasks
- **Heap**: Dynamic allocation support for JSON parsing

## Hardware Considerations

### Pin Mapping
The STM32 implementation would need to map the following pins:
- Stepper motor: STEP, DIR, EN
- Gate sensor: Digital input with pull-up
- Camera triggers: Digital outputs
- Light controls: Digital outputs  
- Selectors: Digital outputs
- Feeder control: Digital output

### Clock Configuration
- **System Clock**: 168MHz (STM32F4) or higher
- **APB1/APB2**: Appropriate peripheral bus frequencies
- **Timer Clocks**: High-frequency timer clocks for precise timing

### Interrupt Priorities
- **Timer ISR**: Highest priority for stepper control
- **UART ISR**: Medium priority for communication
- **GPIO ISR**: Low priority for gate sensor

## Development Workflow

When implementing STM32 support:

1. **Setup Development Environment**
   - STM32CubeIDE or PlatformIO with STM32 support
   - STM32 HAL libraries
   - Debug probe (ST-Link, J-Link, etc.)

2. **Implement Core Interfaces**
   - Start with `STM32Gpio` for basic functionality
   - Implement `STM32TimerTickSource` for timing
   - Add `STM32Clock` for time functions
   - Implement remaining interfaces

3. **Board Configuration**
   - Update `BoardConfig.hpp` with STM32 pin mappings
   - Configure system clocks and peripherals
   - Set up FreeRTOS tasks and priorities

4. **Testing and Validation**
   - Use host-side tests with mock HAL
   - Validate timing accuracy and jitter
   - Test communication reliability
   - Verify stepper motor control

5. **Integration**
   - Update build system for STM32 target
   - Add STM32-specific build flags
   - Configure memory layout and linker scripts

## Performance Targets

### Timing Accuracy
- **Stepper Pulses**: ±0.1µs accuracy
- **Camera Triggers**: ±1µs timing precision
- **Gate Detection**: < 10µs response time

### Communication Performance
- **JSON Parsing**: < 1ms for typical commands
- **Response Time**: < 5ms end-to-end latency
- **Throughput**: Support 1000+ messages/second

### System Performance
- **CPU Usage**: < 50% average load
- **Memory Usage**: < 80% RAM utilization
- **Power Consumption**: Optimized for battery operation

## Future Enhancements

Potential STM32-specific features:
- **CAN Bus**: For industrial communication protocols
- **Ethernet**: Network-based communication
- **USB**: USB device mode for direct PC connection
- **ADC**: Analog sensor support
- **DAC**: Analog output capabilities
- **SPI/I2C**: Additional peripheral support

## Migration Notes

When migrating from ESP32 to STM32:
- **GPIO Latency**: STM32 may have different GPIO timing characteristics
- **Timer Resolution**: Verify timer resolution meets requirements
- **Memory Layout**: Different memory organization may affect performance
- **Interrupt Handling**: STM32 interrupt system differs from ESP32
- **FreeRTOS**: Ensure FreeRTOS configuration is optimized for STM32
