# Makefile for host-side testing of uInspESP32 logic components
# This allows testing the core logic without hardware dependencies

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -g -O2
INCLUDES = -I../include -I../src -I.
LDFLAGS = 

# Source files for testing
TEST_SOURCES = SchedulerTest.cpp CommandTest.cpp
MOCK_SOURCES = mock/MockHAL.cpp
CORE_SOURCES = ../src/scheduler/Scheduler.cpp \
               ../src/pipeline/Pipeline.cpp \
               ../src/pipeline/GateSensor.cpp \
               ../src/state/StateMachine.cpp \
               ../src/comm/MessageBus.cpp \
               ../src/diagnostics/Diagnostics.cpp

# Object files
TEST_OBJECTS = $(TEST_SOURCES:.cpp=.o)
MOCK_OBJECTS = $(MOCK_SOURCES:.cpp=.o)
CORE_OBJECTS = $(CORE_SOURCES:.cpp=.o)

# Executables
TEST_EXECUTABLES = scheduler_test command_test

# Default target
all: $(TEST_EXECUTABLES)

# Individual test executables
scheduler_test: SchedulerTest.o $(MOCK_OBJECTS) $(CORE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

command_test: CommandTest.o $(MOCK_OBJECTS) $(CORE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Object file compilation
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Mock HAL implementation
mock/MockHAL.cpp: mock/MockHAL.hpp
	@echo "MockHAL.hpp already exists, no compilation needed"

# Clean build artifacts
clean:
	rm -f $(TEST_OBJECTS) $(MOCK_OBJECTS) $(CORE_OBJECTS) $(TEST_EXECUTABLES)

# Run all tests
test: $(TEST_EXECUTABLES)
	@echo "Running Scheduler Tests..."
	./scheduler_test
	@echo "Running Command Tests..."
	./command_test
	@echo "All tests completed!"

# Run individual tests
test-scheduler: scheduler_test
	./scheduler_test

test-command: command_test
	./command_test

# Debug build
debug: CXXFLAGS += -DDEBUG -g3
debug: $(TEST_EXECUTABLES)

# Release build
release: CXXFLAGS += -DNDEBUG -O3
release: $(TEST_EXECUTABLES)

# Help target
help:
	@echo "Available targets:"
	@echo "  all          - Build all test executables"
	@echo "  scheduler_test - Build scheduler test executable"
	@echo "  command_test   - Build command test executable"
	@echo "  test         - Run all tests"
	@echo "  test-scheduler - Run scheduler tests only"
	@echo "  test-command   - Run command tests only"
	@echo "  clean        - Remove build artifacts"
	@echo "  debug        - Build with debug flags"
	@echo "  release      - Build with release flags"
	@echo "  help         - Show this help message"

# Dependencies (simplified - in a real project you'd use proper dependency tracking)
SchedulerTest.o: SchedulerTest.cpp mock/MockHAL.hpp ../include/Scheduler.hpp
CommandTest.o: CommandTest.cpp mock/MockHAL.hpp ../include/MessageBus.hpp

.PHONY: all clean test test-scheduler test-command debug release help
