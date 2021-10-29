cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_STANDARD 14)
project(CORE0_1)

set (PWD ${CMAKE_CURRENT_SOURCE_DIR})
set (MWD ${CMAKE_CURRENT_SOURCE_DIR}/..)




if(MSVC OR MSYS OR MINGW)
  set ( platform win_x64)
  
  set (PLAT_LIBS wsock32 ws2_32)

else(APPLE)
  set ( platform mac_x64)
endif()


set(OpenCV_DIR C:/OpenCV-MinGW)


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


set ( OS_SP_inc )


  
if(MSVC OR MSYS OR MINGW)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
else(APPLE)
endif()
# add_executable (helloTest helloTest.cpp )


add_custom_target(really-clean
  COMMAND "$(CMAKE)" --build "${CMAKE_BINARY_DIR}" clean
  COMMAND make -f "${CMAKE_CURRENT_SOURCE_DIR}/Makefile_mods" clean
)

add_subdirectory($ENV{MODULE_MatchingEngine} $ENV{MODULE_MatchingEngine})
add_subdirectory($ENV{MODULE_shape_based_matching} $ENV{MODULE_shape_based_matching})

add_custom_target(
    mod_target
    COMMAND make -f "${CMAKE_CURRENT_SOURCE_DIR}/Makefile_mods"

)


SET(COMMON_COMPILE_FLAGS "-Wall -Wno-sign-compare -Wno-write-strings -Wno-unused-variable")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS}")

if($ENV{FEATURE_COMPILE_W_ARAVIS} EQUAL 1)
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_ARAVIS")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_aravis}/include $ENV{MODULE_aravis}/src)
endif()
if($ENV{FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK} EQUAL 1)
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_hikrobot_camera_sdk}/includes)
  SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_hikrobot_camera_sdk}/libraries/win64/MvCameraControl.lib)
endif()
if($ENV{FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK} EQUAL 1)

  if(MSVC OR MSYS OR MINGW)
    set (OS_DEP_LIB  $ENV{MODULE_MindVision_GIGE}/lib/MVCAMSDK.lib )
  else(APPLE)
    set (OS_DEP_LIB  $ENV{MODULE_MindVision_GIGE}/lib/libmvsdk.dylib )
  endif()
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_MINDVISION_CAMERA_SDK")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_MindVision_GIGE}/include)
  SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_MindVision_GIGE}/lib/MVCAMSDK.lib)
  
endif()

add_executable (visSele main.cpp Ext_Util_API.cpp tmpCodes.cpp wiringPanel.cpp)

add_dependencies(visSele mod_target MatchingEngine shape_based_matching)

target_link_libraries(visSele
  $ENV{MODULE_MatchingEngine}/libMatchingEngine.a
  $ENV{MODULE_circleFitting}/CircleFitting.a 
  $ENV{MODULE_cJSON}/cJSON.a 
  $ENV{MODULE_logctrl}/logctrl.a 
  $ENV{MODULE_common_lib}/common_lib.a 
  $ENV{MODULE_acvImage}/acvImage.a 
  $ENV{MODULE_CameraLayer}/CameraLayer.a 
  $ENV{MODULE_circleFitting}/CircleFitting.a
  $ENV{MODULE_lodepng}/lodepng.a 
  $ENV{MODULE_polyfit}/polyfit.a 
  $ENV{MODULE_zlib}/staticlib/libz.a
  $ENV{MODULE_mjpegLib}/mjpegLib.a 
  $ENV{MODULE_MJPEG_Streamer}/MJPEG_Streamer.a
  $ENV{MODULE_BPG_Protocol}/BPG_Protocol.a

  $ENV{MODULE_shape_based_matching}/libshape_based_matching.a
  ${OS_DEP_LIB}
  ${OpenCV_LIBS}
  ${PLAT_LIBS}
  ${LINK_LIBS_CAMERA}
  )

message(------------${INCLUDE_DIRECTORIES})
include_directories (
  ${INCLUDE_DIRECTORIES}
  "." 
  "./include"
  $ENV{MODULE_acvImage}/include/ 
  $ENV{MODULE_circleFitting} 
  $ENV{MODULE_common_lib}/include 
  $ENV{MODULE_cJSON} 
  $ENV{MODULE_logctrl}/include 
  $ENV{MODULE_zlib}/src 
  $ENV{MODULE_CameraLayer}/include 
  $ENV{MODULE_MindVision_GIGE}/include 
  $ENV{MODULE_Compat_dirent}/include 
  $ENV{MODULE_polyfit}/include 
  $ENV{MODULE_shape_based_matching}/include 
  $ENV{MODULE_shape_based_matching}/MIPP 
  $ENV{MODULE_MatchingEngine}/include
  $ENV{MODULE_MatchingEngine}/include_priv
  $ENV{MODULE_MatchingEngine}/MorphEngine/include
  $ENV{MODULE_mjpegLib}/include
  $ENV{MODULE_MJPEG_Streamer}/include
  $ENV{MODULE_BPG_Protocol}/include 
  $ENV{MODULE_smem_channel}
  $ENV{MODULE_wfqueue}
  $ENV{MODULE_lodepng}
  ${OS_SP_inc}
  ${OpenCV_INCLUDE_DIRS}
  )
  


install (TARGETS visSele  DESTINATION .)
install (DIRECTORY init_data  DESTINATION .)
install (FILES ${OS_DEP_LIB}  DESTINATION .)