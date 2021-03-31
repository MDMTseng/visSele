cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_STANDARD 14)
project(CORE_HUB)

set (PWD ${CMAKE_CURRENT_SOURCE_DIR})
set (MWD ${CMAKE_CURRENT_SOURCE_DIR}/..)


if(MSVC OR MSYS OR MINGW)
  set ( platform win_x64)
else(APPLE)
  set ( platform mac_x64)
endif()



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




set ( MODULE_acvImage ${MWD}/acvImage)
set ( MODULE_cwebsocket ${MWD}/contrib/cwebsocket)
set ( MODULE_circleFitting ${MWD}/contrib/circleFitting)
set ( MODULE_MindVision_GIGE ${MWD}/contrib/MindVision_GIGE/${platform})
set ( MODULE_Compat_dirent ${MWD}/contrib/Compat_dirent/${platform})
set ( MODULE_cJSON ${MWD}/contrib/cJSON)
set ( MODULE_logctrl ${MWD}/logctrl)
set ( MODULE_common_lib ${MWD}/common_lib)
set ( MODULE_zlib ${MWD}/contrib/zlib-1.2.11)
set ( MODULE_SOIL ${MWD}/contrib/SOIL)
set ( MODULE_polyfit ${MWD}/contrib/polyfit)
set ( MODULE_lodepng ${MWD}/contrib/lodepng)
set ( MODULE_shape_based_matching ${MWD}/contrib/shape_based_matching)
set ( MODULE_DataChannel ${MWD}/DataChannel)
set ( MODULE_CameraLayer ${MWD}/CameraLayer)



set ( OS_SP_inc )

if(MSVC OR MSYS OR MINGW)
    # for detecting Windows compilers
endif()


include_directories (
  ${INCLUDE_DIRECTORIES}
  "." 
  "./include"
  ${MODULE_acvImage}/include/ 
  ${MODULE_circleFitting} 
  ${MODULE_common_lib}/include 
  ${MODULE_cJSON} 
  ${MODULE_logctrl}/include 
  ${MODULE_zlib}/src 
  ${MODULE_DataChannel}/include
  ${MODULE_CameraLayer}/include 
  ${MODULE_MindVision_GIGE}/include 
  ${MODULE_Compat_dirent}/include 
  ${MODULE_polyfit}/include 
  ${MODULE_shape_based_matching}/include 
  ${MODULE_shape_based_matching}/MIPP 
  ${MODULE_shape_based_matching}
  ${MODULE_lodepng}
  ${OS_SP_inc}
  )
  

add_executable (${PROJECT_NAME} main.cpp Ext_Util_API.cpp tmpCodes.cpp wiringPanel.cpp)

add_custom_command(TARGET ${PROJECT_NAME}
                  PRE_BUILD 
                  COMMAND make -f "${CMAKE_CURRENT_SOURCE_DIR}/Makefile_mods"
                  )



if(MSVC OR MSYS OR MINGW)
  set (OS_DEP_LIB  
    ${MODULE_MindVision_GIGE}/lib/MVCAMSDK.lib
    )
else(APPLE)
  set (OS_DEP_LIB  
    ${MODULE_MindVision_GIGE}/lib/libmvsdk.dylib
     )
endif()



target_link_libraries(${PROJECT_NAME} 
  ${MODULE_circleFitting}/CircleFitting.a 
  ${MODULE_cJSON}/cJSON.a 
  ${MODULE_logctrl}/logctrl.a 
  ${MODULE_common_lib}/common_lib.a 
  ${MODULE_DataChannel}/DataChannel.a 
  ${MODULE_acvImage}/acvImage.a 
  ${MODULE_CameraLayer}/CameraLayer.a 
  ${MODULE_lodepng}/lodepng.a 
  ${MODULE_polyfit}/polyfit.a 
  ${MODULE_zlib}/staticlib/libz.a

  ${MODULE_shape_based_matching}/libshape_based_matching.a
  ${OS_DEP_LIB}
  ${OpenCV_LIBS}
  
  )