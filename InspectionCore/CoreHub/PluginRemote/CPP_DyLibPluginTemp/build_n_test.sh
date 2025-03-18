#!/bin/bash
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure and build
cmake ..
cmake --build .

echo "Build completed successfully!"
echo "Plugin library is at: $(pwd)/build/lib/libinspect_process_plugin.dylib"
echo "Test program is at: $(pwd)/bin/test_plugin_main"

# Run the test program
echo -e "\nRunning test program..."
./bin/test_plugin

echo -e "\nTest completed!" 