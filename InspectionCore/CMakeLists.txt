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
elseif(APPLE)
  set(PLAT mac_x64)
elseif(UNIX)
  set(PLAT linux_x64)
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
# OpenCV is now mandatory: we are migrating the engine off acvImage onto cv::Mat
# (see docs/OPENCV_MIGRATION.md). The option remains only for build-system
# introspection -- forcing it OFF will fail at find_package.
option(FEATURE_OPENCV     "Build MatchingEngine against OpenCV"    ON)

# Always emit compile_commands.json so editors (clangd / VSCode C/C++) see the real
# per-file flags & defines (e.g. FEATURE_OPENCV) -- no need to #define them in source.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- shared compile options (mirrors old FLAGS: -w -O3 + suppressions) ----
add_compile_options(-w -Wno-sign-compare -Wno-write-strings -Wno-unused-variable)
add_compile_options($<$<CONFIG:Release>:-O3>)
# Ship Release as -O3 AND -g (debug info). The deploy bundle (build.sh) strips
# the shipped exe and archives the separated symbols (symbols/<exe>-<sha>.debug),
# so a field crash dump stays symbolicatable via addr2line without shipping fat
# binaries. -g doesn't change codegen, only adds DWARF sections that get stripped.
add_compile_options($<$<CONFIG:Release>:-g>)

find_package(OpenMP)
# OpenCV is now an unconditional dependency of MatchingEngine. Provide via vcpkg.
find_package(OpenCV REQUIRED)

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
target_include_directories(acvImage PUBLIC acvImage/include common_lib/include)
target_link_libraries(acvImage PUBLIC opencv_core)

# --- cJSON -----------------------------------------------------------------
add_library(cJSON STATIC contrib/cJSON/cJSON.c contrib/cJSON/cJSON_Utils.c)
force_cxx(contrib/cJSON/cJSON.c contrib/cJSON/cJSON_Utils.c)
target_include_directories(cJSON PUBLIC contrib/cJSON)

# --- logctrl ---------------------------------------------------------------
# Phase A: C++ rewrite with levels / per-tag filter / mutex / pluggable sinks.
# Phase A2: shm-ring sink on contrib/smem_channel.
# Header is extern-"C"-wrapped so C callers still work.  Phase F will add the
# drainer daemon that reads from this same ring.
# ---- build provenance header (git hash + build time), regenerated each build ----
# Imprinted into the shm ring (producer_build) + printed in crash dumps and at
# startup, so any dump traces back to the exact commit/build (-> symbols/<sha>.debug).
set(BUILD_INFO_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BUILD_INFO_DIR}")
add_custom_target(build_info_gen ALL
  BYPRODUCTS "${BUILD_INFO_DIR}/build_info.h"
  COMMAND ${CMAKE_COMMAND}
          -DOUT=${BUILD_INFO_DIR}/build_info.h
          -DCFG=$<CONFIG>
          -DSRC=${CMAKE_SOURCE_DIR}
          -P ${CMAKE_SOURCE_DIR}/cmake/gen_build_info.cmake
  COMMENT "Generating build_info.h (git hash + build time)"
  VERBATIM)

add_library(logctrl STATIC logctrl/logctrl.cpp
                          logctrl/log_crash_posix.cpp
                          logctrl/log_crash_win.cpp)
target_include_directories(logctrl PUBLIC logctrl/include ${BUILD_INFO_DIR})
add_dependencies(logctrl build_info_gen)
# smem_channel target is defined further down; we just record the dep here
# and CMake resolves the order.
target_link_libraries(logctrl PUBLIC smem_channel)

# Phase A2 smoke test for the shm ring sink.
add_executable(logctrl_test_ring logctrl/test_ring.cpp)
target_link_libraries(logctrl_test_ring PRIVATE logctrl)

# Phase F.1: drainer daemon.  Forked from main visSele at boot via
# log_spawn_drainer().  Reads the shm ring, writes INFO+ to a rolling disk
# file, holds DEBUG/TRACE in an in-RAM ephemeral buffer for crash dump
# (Phase G).
add_executable(inspd_log logctrl/inspd_log_main.cpp logctrl/inspd_log_ws.cpp)
target_link_libraries(inspd_log PRIVATE logctrl ws_server cJSON)
if(WIN32 OR MINGW OR MSYS)
  # SymInitialize / SymFromAddr / SymGetLineFromAddr64 live in dbghelp.
  target_link_libraries(inspd_log PRIVATE dbghelp)
endif()

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
if(WIN32)
  target_link_libraries(simple_uart PUBLIC winmm)   # timeBeginPeriod (1ms timer res)
endif()

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
  common_lib/json_seg_parser.cpp common_lib/vis_geom.cpp)
force_cxx(common_lib/Util.c common_lib/Util2.c)
target_include_directories(common_lib PUBLIC
  common_lib/include contrib/Compat_dirent/${PLAT}/include)
target_link_libraries(common_lib PUBLIC cJSON acvImage logctrl lodepng mjpegLib simple_uart opencv_core)
if(WIN32 OR MINGW OR MSYS)
  target_compile_options(common_lib PRIVATE -Wa,-mbig-obj)
endif()

# --- MJPEG_Streamer --------------------------------------------------------
add_library(MJPEG_Streamer STATIC MJPEG_Streamer/MJPEG_Streamer.cpp)
target_include_directories(MJPEG_Streamer PUBLIC MJPEG_Streamer/include)
target_link_libraries(MJPEG_Streamer PUBLIC logctrl common_lib)

# --- ws_server (reusable WebSocket server -- shared by BPG and inspd_log) --
# Sources still live under BPG_Protocol/ for path stability; the *target* is
# split so consumers (like inspd_log) can link the WS bits without dragging
# in BPG.
add_library(ws_server STATIC
  BPG_Protocol/ws_server_util.cpp
  contrib/cwebsocket/lib/websocket.c)
force_cxx(contrib/cwebsocket/lib/websocket.c)
target_include_directories(ws_server PUBLIC
  BPG_Protocol/include BPG_Protocol/include_priv contrib/cwebsocket/lib)
target_link_libraries(ws_server PUBLIC logctrl)
if(WIN32 OR MINGW OR MSYS)
  target_link_libraries(ws_server PUBLIC ws2_32)
endif()

# --- BPG_Protocol ----------------------------------------------------------
add_library(BPG_Protocol STATIC
  BPG_Protocol/BPG_Protocol.cpp BPG_Protocol/IPC_Channel.cpp
  BPG_Protocol/BPG_Link_Interface_WebSocket.cpp
  BPG_Protocol/BPG_Link_Interface_SharedMemoryChannel.cpp)
target_include_directories(BPG_Protocol PUBLIC
  BPG_Protocol/include BPG_Protocol/include_priv)
target_link_libraries(BPG_Protocol PUBLIC ws_server logctrl common_lib smem_channel)

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

# --- shape_based_matching (line2Dup + roi_refine localizer) ----------------
# The submodule's own CMakeLists only builds test exes against a hard-coded
# Windows OpenCV path, so we compile the high-level API into our own static lib
# and link it into MatchingEngine. The public surface (include/shape_matcher.h)
# is cv::Mat + std only -- MIPP/AVX2 stays internal to these TUs, so the rest of
# MatchingEngine need not be built with AVX2.
add_library(shape_based_matching STATIC
  contrib/shape_based_matching/line2Dup.cpp
  contrib/shape_based_matching/icp_refine.cpp
  contrib/shape_based_matching/roi_refine.cpp
  contrib/shape_based_matching/shape_matcher.cpp)
target_include_directories(shape_based_matching PUBLIC
  contrib/shape_based_matching
  contrib/shape_based_matching/include
  contrib/shape_based_matching/MIPP
  ${OpenCV_INCLUDE_DIRS})
target_link_libraries(shape_based_matching PUBLIC opencv_core opencv_imgproc)
# MIPP picks the AVX2 backend off __AVX2__; -mavx2 (not -march=native) keeps the
# binary portable across build/deploy hosts. big-obj: line2Dup.cpp is large.
# arm64 has no AVX2 -- MIPP auto-selects NEON there, so only force the flags
# on x86 hosts.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  target_compile_definitions(shape_based_matching PRIVATE __AVX2__)
  target_compile_options(shape_based_matching PRIVATE -mavx2 -mfma)
endif()
if(WIN32 OR MINGW OR MSYS)
  target_compile_options(shape_based_matching PRIVATE -Wa,-mbig-obj)
endif()
if(OpenMP_CXX_FOUND)
  target_link_libraries(shape_based_matching PUBLIC OpenMP::OpenMP_CXX)
endif()

# --- MatchingEngine --------------------------------------------------------
add_library(MatchingEngine STATIC
  MatchingEngine/MatchingEngine.cpp MatchingEngine/MatchingCore.cpp
  MatchingEngine/FeatureManager.cpp MatchingEngine/FeatureExtractor.cpp
  MatchingEngine/ContourGrid.cpp MatchingEngine/FeatureManager_group.cpp
  MatchingEngine/FeatureManager_sig360_circle_line.cpp
  MatchingEngine/JudgeCALC.cpp
  MatchingEngine/MorphEngine/MorphEngine.cpp MatchingEngine/FeatureReport_UTIL.cpp
  MatchingEngine/FeatureManager_stage_light_report.cpp
  MatchingEngine/BackLightFieldCalib.cpp
  MatchingEngine/TelecentricCalib.cpp
  MatchingEngine/EdgeSelect.cpp
  MatchingEngine/Caliper.cpp
  MatchingEngine/AnchorPatch.cpp
  # OpenCV-based sources are now unconditional (acv -> cv migration step 1).
  # Link ONLY the modules we use; the full OpenCV_LIBS pulls opencv_dnn which
  # has a broken protobuf dep on Homebrew -> dyld abort.
  MatchingEngine/ChessboardExtract.cpp MatchingEngine/LensCalib.cpp
  MatchingEngine/FieldCalib.cpp
  MatchingEngine/LabelingCV.cpp MatchingEngine/BinarizeCV.cpp
  MatchingEngine/CvBridge.cpp MatchingEngine/SearchPointCV.cpp)
target_include_directories(MatchingEngine PUBLIC
  MatchingEngine/include MatchingEngine/include_priv MatchingEngine/MorphEngine/include
  ${OpenCV_INCLUDE_DIRS})
target_link_libraries(MatchingEngine PUBLIC acvImage cJSON common_lib CircleFitting logctrl polyfit CameraLayer
  shape_based_matching
  opencv_core opencv_imgproc opencv_imgcodecs opencv_calib3d)
target_compile_definitions(MatchingEngine PUBLIC FEATURE_OPENCV)

# ===========================================================================
# Camera SDK glue (headers + prebuilt libs) applied to CameraLayer + app
# ===========================================================================
add_library(camera_sdk INTERFACE)
if(FEATURE_ARAVIS)
  target_compile_definitions(camera_sdk INTERFACE FEATURE_COMPILE_W_ARAVIS)
  find_package(PkgConfig REQUIRED)
  if(APPLE)
    # macOS uses the in-repo prebuilt dylib (no Homebrew aravis package).
    set(ARAVIS_DIR ${REPO}/contrib/aravis/mac_arm64)
    if(NOT EXISTS ${ARAVIS_DIR})
      set(ARAVIS_DIR ${REPO}/contrib/aravis/mac_x64)
    endif()
    target_include_directories(camera_sdk INTERFACE ${ARAVIS_DIR}/include ${ARAVIS_DIR}/src)
    pkg_search_module(GLIB REQUIRED glib-2.0)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_include_directories(camera_sdk INTERFACE ${GLIB_INCLUDE_DIRS})
    target_link_libraries(camera_sdk INTERFACE ${GTK3_LINK_LIBRARIES}
      ${ARAVIS_DIR}/src/libaravis-0.8.0.dylib)
  else()
    # Linux: link the system Aravis (apt: libaravis-dev). pkg-config pulls
    # in transitive glib / gobject for us, so no extra GTK3 plumbing.
    pkg_check_modules(ARAVIS REQUIRED aravis-0.8)
    target_include_directories(camera_sdk INTERFACE ${ARAVIS_INCLUDE_DIRS})
    target_link_libraries(camera_sdk INTERFACE ${ARAVIS_LINK_LIBRARIES})
  endif()
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

# Core-side frame<->object pairing. The header defaults it to 1 and states the
# precondition for ever defaulting to 0: real-part validation, which phantom and
# virtual traffic do not provide. So it stays 1 here and this option exists to
# run the A/B deliberately -- cmake -DPERIF_CORE_PAIRING=0 -- rather than by
# editing the default and forgetting.
#
# At 0 the core names nothing: every report goes out with tid -1 and cam_ts for
# the DEVICE to place. That also removes `disagree`, which is the only evidence
# that a verdict landed on the right part, so a run with this off cannot prove
# placement -- only latency.
if(DEFINED PERIF_CORE_PAIRING)
  target_compile_definitions(visSele PRIVATE PERIF_CORE_PAIRING=${PERIF_CORE_PAIRING})
  message(STATUS "PERIF_CORE_PAIRING forced to ${PERIF_CORE_PAIRING}")
endif()
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

# ===========================================================================
# calib_chessboard CLI -- offline lens calibration from chessboard images.
# ===========================================================================
add_executable(calib_chessboard tools/calib_chessboard.cpp)
target_link_libraries(calib_chessboard PRIVATE MatchingEngine)

# jpeg_bench -- imencode timing micro-benchmark (BGR vs grayscale, ~5MP).
add_executable(jpeg_bench tools/jpeg_bench.cpp)
target_link_libraries(jpeg_bench PRIVATE MatchingEngine)

# latency_stages -- build the core's timing up one layer at a time, to find the
# layer where jitter stops being +/-10ms. Links only cJSON on purpose: the
# camera and the matching engine are later stages and there is no point adding
# them while the cheap layers are still suspects.
# POSIX-only (openpty): a bench diagnostic, not a deploy artifact.
if(NOT WIN32)
  add_executable(latency_stages tools/latency_stages.cpp)
  target_link_libraries(latency_stages PRIVATE common_lib)
endif()

# copy aravis dylib next to the binary for rpath loading (mac)
if(FEATURE_ARAVIS)
  add_custom_command(TARGET visSele POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${ARAVIS_DIR}/src/libaravis-0.8.0.dylib $<TARGET_FILE_DIR:visSele>)
  set_target_properties(visSele PROPERTIES BUILD_RPATH "@executable_path")
endif()
