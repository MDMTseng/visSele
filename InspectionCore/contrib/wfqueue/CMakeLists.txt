cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_STANDARD 14)
project(wfq_test)

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/.")

add_executable (${PROJECT_NAME}_test MainTest.cpp)