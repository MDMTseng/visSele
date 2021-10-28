cmake_minimum_required(VERSION 2.8)
set (CMAKE_CXX_STANDARD 14)
project(MachingEng)


include_directories (
  "./include_priv/"
  "." 
  "./include"
  "./MorphEngine/include/"
  $ENV{MODULE_CameraLayer}/include 
  $ENV{MODULE_cJSON} 
  $ENV{MODULE_acvImage}/include/ 
  $ENV{MODULE_common_lib}/include 
  $ENV{MODULE_circleFitting} 
  $ENV{MODULE_logctrl}/include 
  $ENV{MODULE_polyfit}/include 
  $ENV{MODULE_zlib}/src 
  $ENV{OpenCV_INCLUDE_DIRS}
  )
  


add_library(${PROJECT_NAME} STATIC MatchingEngine.cpp MatchingCore.cpp FeatureManager.cpp FeatureExtractor.cpp ContourGrid.cpp 
FeatureManager_group.cpp FeatureManager_platingCheck.cpp FeatureManager_sig360_circle_line.cpp
MorphEngine/MorphEngine.cpp FeatureReport_UTIL.cpp FeatureManager_stage_light_report.cpp FeatureManager_gen.cpp 
)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

# target_link_libraries(visSele
#   ${MODULE_circleFitting}/CircleFitting.a 
#   ${MODULE_MatchingEngine}/MatchingEngine.a
#   ${MODULE_cJSON}/cJSON.a 
#   ${MODULE_logctrl}/logctrl.a 
#   ${MODULE_common_lib}/common_lib.a 
#   ${MODULE_acvImage}/acvImage.a 
#   ${MODULE_CameraLayer}/CameraLayer.a 
#   ${MODULE_circleFitting}/CircleFitting.a
#   ${MODULE_lodepng}/lodepng.a 
#   ${MODULE_polyfit}/polyfit.a 
#   ${MODULE_zlib}/staticlib/libz.a
#   ${MODULE_mjpegLib}/mjpegLib.a 
#   ${MODULE_MJPEG_Streamer}/MJPEG_Streamer.a
#   ${MODULE_BPG_Protocol}/BPG_Protocol.a

#   ${MODULE_shape_based_matching}/libshape_based_matching.a
#   ${OS_DEP_LIB}
#   ${OpenCV_LIBS}
#   ${PLAT_LIBS}
#   )