cmake_minimum_required(VERSION 3.16)

# ---------------------------------------------------------------------------
# Unified build for the visSele inspection core.
# Replaces the old compile.sh / Makefile_mods / Makefile.in / InitCMake.sh flow.
# Build:   cmake --preset mac-arm64   &&  cmake --build build/mac-arm64
# or:      cmake -B build && cmake --build build -j
# ---------------------------------------------------------------------------

project(visSele LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(REPO ${CMAKE_CURRENT_SOURCE_DIR})

# platform subdir name used to locate prebuilt SDKs / shims
if(WIN32 OR MINGW OR MSYS)
  set(PLAT win_x64)
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
  set(PLAT mac_arm64)
else()
  set(PLAT mac_x64)
endif()

# --- platform / camera-SDK feature options -------------------------------
if(WIN32 OR MINGW OR MSYS)
  set(_DEF_ARAVIS OFF)
  set(_DEF_MINDVISION ON)
  set(_DEF_HIKROBOT ON)
else()
  set(_DEF_ARAVIS ON)
  set(_DEF_MINDVISION OFF)
  set(_DEF_HIKROBOT OFF)
endif()
option(FEATURE_ARAVIS     "Build with Aravis GigE camera SDK"      ${_DEF_ARAVIS})
option(FEATURE_MINDVISION "Build with MindVision GigE camera SDK"  ${_DEF_MINDVISION})
option(FEATURE_HIKROBOT   "Build with HikRobot camera SDK"         ${_DEF_HIKROBOT})
option(FEATURE_UVC        "Build with libuvc camera support"       OFF)
option(FEATURE_OPENCV     "Build MatchingEngine against OpenCV"    OFF)

# --- shared compile options (mirrors old FLAGS: -w -O3 + suppressions) ----
add_compile_options(-w -Wno-sign-compare -Wno-write-strings -Wno-unused-variable)
add_compile_options($<$<CONFIG:Release>:-O3>)

find_package(OpenMP)
if(FEATURE_OPENCV)
  find_package(OpenCV REQUIRED)   # provide via vcpkg (-DFEATURE_OPENCV=ON)
endif()

# Compile a list of .c sources as C++ (the old build used $(CXX) for everything,
# so C-linkage assumptions match only if we keep compiling them as C++).
function(force_cxx)
  foreach(f ${ARGN})
    set_source_files_properties(${f} PROPERTIES LANGUAGE CXX)
  endforeach()
endfunction()

# ===========================================================================
# Module static libraries
# ===========================================================================

# --- acvImage --------------------------------------------------------------
add_library(acvImage STATIC
  acvImage/acvImage.cpp acvImage/acvImage_BasicTool.cpp acvImage/acvImage_BasicDrawTool.cpp
  acvImage/acvImage_MophologyTool.cpp acvImage/acvImage_ToolBox.cpp
  acvImage/acvImage_ComponentLabelingTool.cpp acvImage/acvImage_SpDomainTool.cpp)
target_include_directories(acvImage PUBLIC acvImage/include)

# --- cJSON -----------------------------------------------------------------
add_library(cJSON STATIC contrib/cJSON/cJSON.c contrib/cJSON/cJSON_Utils.c)
force_cxx(contrib/cJSON/cJSON.c contrib/cJSON/cJSON_Utils.c)
target_include_directories(cJSON PUBLIC contrib/cJSON)

# --- logctrl ---------------------------------------------------------------
add_library(logctrl STATIC logctrl/logctrl.c)
force_cxx(logctrl/logctrl.c)
target_include_directories(logctrl PUBLIC logctrl/include)

# --- circleFitting ---------------------------------------------------------
add_library(CircleFitting STATIC
  contrib/circleFitting/circle.cpp contrib/circleFitting/data.cpp
  contrib/circleFitting/CircleFitByHyper.cpp contrib/circleFitting/CircleFitByKasa.cpp
  contrib/circleFitting/CircleFitByPratt.cpp contrib/circleFitting/CircleFitByTaubin.cpp
  contrib/circleFitting/Utilities.cpp)
target_include_directories(CircleFitting PUBLIC contrib/circleFitting)

# --- lodepng ---------------------------------------------------------------
add_library(lodepng STATIC
  contrib/lodepng/lodepng.cpp contrib/lodepng/lodepng_util.cpp contrib/lodepng/pngdetail.cpp)
target_include_directories(lodepng PUBLIC contrib/lodepng)

# --- polyfit ---------------------------------------------------------------
add_library(polyfit STATIC contrib/polyfit/polyfit.c)
force_cxx(contrib/polyfit/polyfit.c)
target_include_directories(polyfit PUBLIC contrib/polyfit/include)

# --- simple_uart -----------------------------------------------------------
add_library(simple_uart STATIC contrib/simple_uart/simple_uart.c)
force_cxx(contrib/simple_uart/simple_uart.c)
target_include_directories(simple_uart PUBLIC contrib/simple_uart/include)

# --- mjpegLib (bundled libjpeg) -------------------------------------------
set(_JPEG_SRC
  jaricom.c jcomapi.c jutils.c jerror.c jmemmgr.c jmemnobs.c
  jcapimin.c jcapistd.c jcarith.c jctrans.c jcparam.c jdatadst.c jcinit.c
  jcmaster.c jcmarker.c jcmainct.c jcprepct.c jccoefct.c jccolor.c jcsample.c
  jchuff.c jcdctmgr.c jfdctfst.c jfdctflt.c jfdctint.c
  jdapimin.c jdapistd.c jdarith.c jdtrans.c jdatasrc.c jdmaster.c jdinput.c
  jdmarker.c jdhuff.c jdmainct.c jdcoefct.c jdpostct.c jddctmgr.c jidctfst.c
  jidctflt.c jidctint.c jdsample.c jdcolor.c jquant1.c jquant2.c jdmerge.c)
list(TRANSFORM _JPEG_SRC PREPEND contrib/mjpegLib/libjpeg-master/)
add_library(mjpegLib STATIC contrib/mjpegLib/mjpegLib.c ${_JPEG_SRC})
force_cxx(contrib/mjpegLib/mjpegLib.c ${_JPEG_SRC})
target_include_directories(mjpegLib PUBLIC
  contrib/mjpegLib/include contrib/mjpegLib/libjpeg-master)

# --- smem_channel ----------------------------------------------------------
if(WIN32 OR MINGW OR MSYS)
  set(_SMEM_PLAT contrib/smem_channel/sp_win.cpp)
else()
  set(_SMEM_PLAT contrib/smem_channel/sp_posix.cpp)
endif()
add_library(smem_channel STATIC contrib/smem_channel/smem_channel.cpp ${_SMEM_PLAT})
target_include_directories(smem_channel PUBLIC contrib/smem_channel)
if(NOT (WIN32 OR MINGW OR MSYS) AND NOT APPLE)
  target_link_libraries(smem_channel PUBLIC rt)   # -lrt on Linux only
endif()

# --- common_lib ------------------------------------------------------------
add_library(common_lib STATIC
  common_lib/Util.c common_lib/Util2.c common_lib/ImageSampler.cpp
  common_lib/Data_Layer_PHY.cpp common_lib/Data_Layer_Protocol.cpp
  common_lib/json_seg_parser.cpp)
force_cxx(common_lib/Util.c common_lib/Util2.c)
target_include_directories(common_lib PUBLIC
  common_lib/include contrib/Compat_dirent/${PLAT}/include)
target_link_libraries(common_lib PUBLIC cJSON acvImage logctrl lodepng mjpegLib simple_uart)
if(WIN32 OR MINGW OR MSYS)
  target_compile_options(common_lib PRIVATE -Wa,-mbig-obj)
endif()

# --- MJPEG_Streamer --------------------------------------------------------
add_library(MJPEG_Streamer STATIC MJPEG_Streamer/MJPEG_Streamer.cpp)
target_include_directories(MJPEG_Streamer PUBLIC MJPEG_Streamer/include)
target_link_libraries(MJPEG_Streamer PUBLIC logctrl common_lib)

# --- BPG_Protocol (+ bundled cwebsocket) -----------------------------------
add_library(BPG_Protocol STATIC
  BPG_Protocol/BPG_Protocol.cpp BPG_Protocol/IPC_Channel.cpp
  BPG_Protocol/BPG_Link_Interface_WebSocket.cpp BPG_Protocol/ws_server_util.cpp
  BPG_Protocol/BPG_Link_Interface_SharedMemoryChannel.cpp
  contrib/cwebsocket/lib/websocket.c)
force_cxx(contrib/cwebsocket/lib/websocket.c)
target_include_directories(BPG_Protocol PUBLIC
  BPG_Protocol/include BPG_Protocol/include_priv contrib/cwebsocket/lib)
target_link_libraries(BPG_Protocol PUBLIC logctrl common_lib smem_channel)
if(WIN32 OR MINGW OR MSYS)
  target_link_libraries(BPG_Protocol PUBLIC ws2_32)
endif()

# --- CameraLayer (camera-SDK conditional) ----------------------------------
set(_CAM_SRC
  CameraLayer/CameraLayer.cpp CameraLayer/CameraLayer_BMP.cpp
  CameraLayer/CameraLayer_BMP_carousel.cpp CameraLayer/CameraLayerManager.cpp)
if(FEATURE_UVC)
  list(APPEND _CAM_SRC CameraLayer/CameraLayer_UVC.cpp)
endif()
if(FEATURE_ARAVIS)
  list(APPEND _CAM_SRC CameraLayer/CameraLayer_Aravis.cpp)
endif()
if(FEATURE_HIKROBOT)
  list(APPEND _CAM_SRC CameraLayer/CameraLayer_HikRobot_Camera.cpp)
endif()
if(FEATURE_MINDVISION)
  list(APPEND _CAM_SRC CameraLayer/CameraLayer_GIGE_MindVision.cpp)
endif()
add_library(CameraLayer STATIC ${_CAM_SRC})
target_include_directories(CameraLayer PUBLIC CameraLayer/include)
target_link_libraries(CameraLayer PUBLIC acvImage logctrl common_lib)

# --- MatchingEngine --------------------------------------------------------
add_library(MatchingEngine STATIC
  MatchingEngine/MatchingEngine.cpp MatchingEngine/MatchingCore.cpp
  MatchingEngine/FeatureManager.cpp MatchingEngine/FeatureExtractor.cpp
  MatchingEngine/ContourGrid.cpp MatchingEngine/FeatureManager_group.cpp
  MatchingEngine/FeatureManager_platingCheck.cpp
  MatchingEngine/FeatureManager_sig360_circle_line.cpp
  MatchingEngine/MorphEngine/MorphEngine.cpp MatchingEngine/FeatureReport_UTIL.cpp
  MatchingEngine/FeatureManager_stage_light_report.cpp
  MatchingEngine/BackLightFieldCalib.cpp
  MatchingEngine/TelecentricCalib.cpp
  MatchingEngine/EdgeSignature.cpp)
target_include_directories(MatchingEngine PUBLIC
  MatchingEngine/include MatchingEngine/include_priv MatchingEngine/MorphEngine/include)
target_link_libraries(MatchingEngine PUBLIC acvImage cJSON common_lib CircleFitting logctrl polyfit CameraLayer)
if(OpenCV_FOUND)
  # Link ONLY the modules we use. Homebrew's opencv_dnn has a broken protobuf
  # dependency, and linking the full OpenCV_LIBS pulls it in -> dyld abort.
  target_sources(MatchingEngine PRIVATE MatchingEngine/ChessboardExtract.cpp MatchingEngine/LensCalib.cpp)
  target_compile_definitions(MatchingEngine PUBLIC FEATURE_OPENCV)
  target_link_libraries(MatchingEngine PUBLIC opencv_core opencv_imgproc opencv_imgcodecs opencv_calib3d)
  target_include_directories(MatchingEngine PUBLIC ${OpenCV_INCLUDE_DIRS})
endif()

# ===========================================================================
# Camera SDK glue (headers + prebuilt libs) applied to CameraLayer + app
# ===========================================================================
add_library(camera_sdk INTERFACE)
if(FEATURE_ARAVIS)
  set(ARAVIS_DIR ${REPO}/contrib/aravis/mac_arm64)
  if(NOT EXISTS ${ARAVIS_DIR})
    set(ARAVIS_DIR ${REPO}/contrib/aravis/mac_x64)
  endif()
  target_compile_definitions(camera_sdk INTERFACE FEATURE_COMPILE_W_ARAVIS)
  target_include_directories(camera_sdk INTERFACE ${ARAVIS_DIR}/include ${ARAVIS_DIR}/src)
  find_package(PkgConfig REQUIRED)
  pkg_search_module(GLIB REQUIRED glib-2.0)
  pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
  target_include_directories(camera_sdk INTERFACE ${GLIB_INCLUDE_DIRS})
  target_link_libraries(camera_sdk INTERFACE ${GTK3_LINK_LIBRARIES}
    ${ARAVIS_DIR}/src/libaravis-0.8.0.dylib)
endif()
if(FEATURE_MINDVISION)
  set(MV_DIR ${REPO}/contrib/MindVision_GIGE/win_x64)
  if(NOT (WIN32 OR MINGW OR MSYS))
    set(MV_DIR ${REPO}/contrib/MindVision_GIGE/mac_x64)
  endif()
  target_compile_definitions(camera_sdk INTERFACE FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK)
  target_include_directories(camera_sdk INTERFACE ${MV_DIR}/include)
  if(WIN32 OR MINGW OR MSYS)
    target_link_libraries(camera_sdk INTERFACE ${MV_DIR}/lib/MVCAMSDK.lib)
  else()
    target_link_libraries(camera_sdk INTERFACE ${MV_DIR}/lib/libmvsdk.dylib)
  endif()
endif()
if(FEATURE_HIKROBOT)
  set(HIK_DIR ${REPO}/contrib/hikrobot_camera_sdk)
  target_compile_definitions(camera_sdk INTERFACE FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK)
  target_include_directories(camera_sdk INTERFACE ${HIK_DIR}/includes)
  target_link_libraries(camera_sdk INTERFACE ${HIK_DIR}/libraries/win64/MvCameraControl.lib)
endif()
target_link_libraries(CameraLayer PUBLIC camera_sdk)

# ===========================================================================
# visSele application (Core0_1)
# ===========================================================================
add_executable(visSele Core0_1/main.cpp Core0_1/tmpCodes.cpp Core0_1/wiringPanel.cpp)
target_include_directories(visSele PRIVATE Core0_1 Core0_1/include)
# leaf libs (acvImage, cJSON, common_lib, logctrl, lodepng, mjpegLib, polyfit,
# CircleFitting, simple_uart, smem_channel) arrive transitively via these.
target_link_libraries(visSele PRIVATE
  MatchingEngine CameraLayer BPG_Protocol MJPEG_Streamer)
if(OpenMP_CXX_FOUND)
  target_link_libraries(visSele PRIVATE OpenMP::OpenMP_CXX)
endif()
if(WIN32 OR MINGW OR MSYS)
  target_link_libraries(visSele PRIVATE wsock32 ws2_32)
  target_link_options(visSele PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# copy aravis dylib next to the binary for rpath loading (mac)
if(FEATURE_ARAVIS)
  add_custom_command(TARGET visSele POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${ARAVIS_DIR}/src/libaravis-0.8.0.dylib $<TARGET_FILE_DIR:visSele>)
  set_target_properties(visSele PROPERTIES BUILD_RPATH "@executable_path")
endif()
