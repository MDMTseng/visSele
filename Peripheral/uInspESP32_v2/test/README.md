# uInspESP32 Host-Side Testing Framework

This directory contains a comprehensive testing framework for the uInspESP32 logic components. The tests can be run on a host machine without hardware dependencies, allowing for rapid development and validation of core logic.

## Overview

The testing framework provides:
- **Mock HAL Implementation**: Platform-agnostic mock implementations of all hardware interfaces
- **Logic Component Tests**: Tests for scheduler, command parsing, state machine, and other core logic
- **Integration Tests**: Tests that verify component interactions work correctly
- **Automated Test Runner**: Scripts to build and run the complete test suite

## Test Structure

```
test/
├── mock/
│   └── MockHAL.hpp          # Mock implementations of HAL interfaces
├── SchedulerTest.cpp        # Tests for scheduling logic
├── CommandTest.cpp          # Tests for command parsing and responses
├── Makefile                 # Build configuration for tests
├── run_tests.sh            # Automated test runner script
└── README.md               # This file
```

## Mock HAL Components

The `MockHAL.hpp` provides mock implementations of all hardware interfaces:

- **MockGpio**: Simulates GPIO operations with logging
- **MockTimerTickSource**: Simulates timer interrupts with controllable timing
- **MockStepperDriver**: Simulates stepper motor control
- **MockClock**: Simulates time functions with controllable time advancement
- **MockLogger**: Captures log messages for verification
- **MockTransport**: Simulates serial communication with controllable I/O

## Test Categories

### 1. Scheduler Tests (`SchedulerTest.cpp`)
- **Basic Scheduling**: Verifies actions are scheduled and executed at correct times
- **Multiple Actions**: Tests scheduling multiple actions for multiple objects
- **Action Timing**: Validates precise timing of action execution
- **Gate Sensor Integration**: Tests integration with gate sensor logic
- **State Machine Transitions**: Verifies state machine behavior

### 2. Command Tests (`CommandTest.cpp`)
- **Command Parsing**: Tests JSON command parsing and validation
- **Response Generation**: Verifies correct response generation for commands
- **Error Handling**: Tests error logging and reporting
- **Message Bus Integration**: Tests message routing and queuing
- **Diagnostics Integration**: Tests error history and reporting

## Building and Running Tests

### Prerequisites
- C++11 compatible compiler (g++, clang++)
- Make utility
- Standard C++ libraries

### Quick Start
```bash
cd test/
./run_tests.sh
```

### Manual Build and Run
```bash
cd test/

# Build all tests
make all

# Run specific tests
./scheduler_test
./command_test

# Or run all tests
make test
```

### Build Options
```bash
# Debug build with extra debugging info
make debug

# Release build with optimizations
make release

# Clean build artifacts
make clean
```

## Test Development

### Adding New Tests

1. **Create test file**: Add a new `.cpp` file following the naming pattern `*Test.cpp`
2. **Include mock HAL**: Use `#include "mock/MockHAL.hpp"` to get mock implementations
3. **Follow test structure**: Use the existing test classes as templates
4. **Update Makefile**: Add your test executable to the build configuration

### Example Test Structure
```cpp
#include "mock/MockHAL.hpp"
#include <iostream>
#include <cassert>

class MyComponentTest {
private:
    MockHAL mock_hal_;
    MyComponent component_;

public:
    MyComponentTest() {
        component_.init(mock_hal_);
    }

    void runAllTests() {
        testBasicFunctionality();
        testEdgeCases();
    }

private:
    void testBasicFunctionality() {
        // Test implementation
        assert(condition && "Test description");
    }
};

int main() {
    MyComponentTest test;
    test.runAllTests();
    return 0;
}
```

### Mock HAL Usage

The mock HAL provides controllable, deterministic behavior:

```cpp
// GPIO testing
MockGpio& gpio = mock_hal_.getMockGpio();
gpio.writePin(5, IGpio::PinState::PIN_HIGH);
assert(gpio.getPinState(5) == IGpio::PinState::PIN_HIGH);

// Timer testing
MockTimerTickSource& timer = mock_hal_.getMockTimerTickSource();
timer.simulateTick(1000);  // Simulate tick at time 1000us

// Clock testing
MockClock& clock = mock_hal_.getMockClock();
clock.setTime(5000);  // Set time to 5000us
clock.advanceTime(100);  // Advance by 100us

// Transport testing
MockTransport& transport = mock_hal_.getMockTransport();
transport.addInputString("test command");
string output = transport.getOutputString();
```

## Continuous Integration

The test framework is designed to be CI-friendly:

- **Deterministic**: Tests produce consistent results
- **Fast**: No hardware dependencies or real-time delays
- **Comprehensive**: Covers core logic paths and edge cases
- **Automated**: Can be run in automated build pipelines

## Integration with Firmware

The test framework validates the same logic that runs in the firmware:

1. **Shared Interfaces**: Uses the same HAL interfaces as the firmware
2. **Same Logic**: Tests the actual implementation classes
3. **Compatible Data**: Uses the same data structures and enums
4. **Realistic Scenarios**: Tests realistic usage patterns

## Limitations

- **No Real-Time Testing**: Cannot test real-time behavior or timing jitter
- **No Hardware Interaction**: Cannot test actual hardware interfaces
- **Simplified Environment**: Some firmware-specific optimizations may not be tested

## Future Enhancements

- **Performance Testing**: Add timing and performance benchmarks
- **Memory Testing**: Add memory usage and leak detection
- **Fuzz Testing**: Add random input testing for robustness
- **Coverage Analysis**: Add code coverage reporting
- **Property-Based Testing**: Add property-based testing for edge cases

## Troubleshooting

### Build Issues
- Ensure C++11 compiler is available
- Check that all include paths are correct
- Verify that source files exist in expected locations

### Test Failures
- Check test output for specific assertion failures
- Verify mock HAL setup is correct
- Ensure test data and expectations are valid

### Integration Issues
- Verify that HAL interfaces match between test and firmware
- Check that data structures are compatible
- Ensure test scenarios match real-world usage
