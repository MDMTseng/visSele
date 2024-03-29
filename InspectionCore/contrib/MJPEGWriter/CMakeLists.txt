cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_STANDARD 14)
project(MJPEGWriter)


# debug or release
SET(CMAKE_BUILD_TYPE "Release")


# arm or x86
IF(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
    SET(PLATFORM_COMPILE_FLAGS "-mfpu=neon")
ELSE()
    SET(PLATFORM_COMPILE_FLAGS "-march=native")

    # some places of the algorithm are designed for 128 SIMD
    # so 128 SSE may slightly faster than 256 AVX, you may want this
    #SET(PLATFORM_COMPILE_FLAGS "-msse ")  # SSE only -msse2 -msse3 -msse4 -mssse3
ENDIF()

SET(COMMON_COMPILE_FLAGS "-Wall -Wno-sign-compare")
SET(CMAKE_CXX_FLAGS "${PLATFORM_COMPILE_FLAGS} ${COMMON_COMPILE_FLAGS} $ENV{CXXFLAGS}")
SET(CMAKE_CXX_FLAGS_DEBUG "-O3")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3")


# opencv
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /opt/ros/kinetic)
find_package(OpenCV 4 REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()




include_directories (${CMAKE_CURRENT_SOURCE_DIR})


add_library(${PROJECT_NAME} SHARED MJPEGWriter.cpp)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})


# test exe
add_executable(${PROJECT_NAME}_test main.cpp)
target_link_libraries(${PROJECT_NAME}_test ${OpenCV_LIBS} ${PROJECT_NAME})

