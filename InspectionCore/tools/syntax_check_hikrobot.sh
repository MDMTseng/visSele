#!/bin/sh
# Syntax-check the HikRobot camera layer on a non-Windows box.
#
# CameraLayer_HikRobot_Camera.cpp only enters the build on Windows
# (CMakeLists.txt: FEATURE_HIKROBOT defaults ON only for WIN32/MINGW/MSYS, and
# the only binary shipped in contrib is libraries/win64/MvCameraControl.lib).
# The SDK *headers*, though, are platform-independent and are checked in -- so
# everything except linking can be verified here. That is enough to catch the
# whole class of mistakes that editing this file usually produces.
#
# Exit 0 = clean. Run from anywhere.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

CXX=${CXX:-clang++}

$CXX -fsyntax-only -std=c++17 \
  -DFEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK \
  -Icontrib/hikrobot_camera_sdk/includes \
  -ICameraLayer/include \
  -Icommon_lib/include \
  -IacvImage/include \
  -Ilogctrl/include \
  CameraLayer/CameraLayer_HikRobot_Camera.cpp

echo "HikRobot camera layer: syntax OK (link still requires the Windows build)"
