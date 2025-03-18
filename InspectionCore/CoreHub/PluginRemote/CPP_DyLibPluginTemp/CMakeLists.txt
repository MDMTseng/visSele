cmake_minimum_required(VERSION 3.10)
project(inspect_process_plugin)

# Find OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Set cJSON paths
set(CJSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/../../../contrib/cJSON CACHE PATH "Path to cJSON include directory")
set(CJSON_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../../../contrib/cJSON CACHE PATH "Path to cJSON library directory")

#convert to absolute path
get_filename_component(CJSON_INCLUDE_DIR "${CJSON_INCLUDE_DIR}" ABSOLUTE)
get_filename_component(CJSON_LIBRARY_DIR "${CJSON_LIBRARY_DIR}" ABSOLUTE)


message(STATUS "CJSON_INCLUDE_DIR: ${CJSON_INCLUDE_DIR}")
message(STATUS "CJSON_LIBRARY_DIR: ${CJSON_LIBRARY_DIR}")

include_directories(${CJSON_INCLUDE_DIR})
link_directories(${CJSON_LIBRARY_DIR})

# For library - include PluginImpl.cpp for the plugin interface functions
set(LIB_SOURCES
    PluginImpl.cpp
    InspectPlugin.cpp
    ConversionUtil.cpp
)

# For test executable
set(TEST_SOURCES
    TestPluginMain.cpp
    ConversionUtil.cpp
)

# Create shared library
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
add_library(inspect_process_plugin SHARED ${LIB_SOURCES})

# Link with OpenCV and cJSON
target_link_libraries(inspect_process_plugin 
    ${OpenCV_LIBS} 
    ${CJSON_LIBRARY_DIR}/cJSON.a #it's working DO NOT CHANGE
)

# Set C++ standard
set_target_properties(inspect_process_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Create binary directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add test executable
add_executable(test_plugin ${TEST_SOURCES})
target_link_libraries(test_plugin 
    ${OpenCV_LIBS} 
    ${CJSON_LIBRARY_DIR}/cJSON.a #it's working DO NOT CHANGE
)