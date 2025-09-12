#!/bin/bash

# Test runner script for uInspESP32 host-side tests
# This script builds and runs the test suite for logic components

set -e  # Exit on any error

echo "🧪 uInspESP32 Host-Side Test Runner"
echo "=================================="

# Check if we're in the right directory
if [ ! -f "Makefile" ]; then
    echo "❌ Error: Must be run from the test/ directory"
    echo "   Current directory: $(pwd)"
    exit 1
fi

# Clean previous build
echo "🧹 Cleaning previous build..."
make clean

# Build tests
echo "🔨 Building test executables..."
make all

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
else
    echo "❌ Build failed!"
    exit 1
fi

# Run tests
echo ""
echo "🚀 Running test suite..."
echo "========================"

# Run scheduler tests
echo "📊 Running Scheduler Tests..."
if ./scheduler_test; then
    echo "✅ Scheduler tests passed!"
else
    echo "❌ Scheduler tests failed!"
    exit 1
fi

echo ""

# Run command tests
echo "📝 Running Command Tests..."
if ./command_test; then
    echo "✅ Command tests passed!"
else
    echo "❌ Command tests failed!"
    exit 1
fi

echo ""
echo "🎉 All tests passed successfully!"
echo ""
echo "Test Summary:"
echo "  ✅ Scheduler logic tests"
echo "  ✅ Command parsing tests"
echo "  ✅ Mock HAL integration"
echo "  ✅ Error handling tests"
echo ""
echo "The core logic components are working correctly!"
echo "Ready for firmware integration and hardware testing."
