cmake_minimum_required(VERSION 3.24.1)
set (CMAKE_CXX_STANDARD 14)
project(CORE0_1)

set (PWD ${CMAKE_CURRENT_SOURCE_DIR})
set (MWD ${CMAKE_CURRENT_SOURCE_DIR}/..)


message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
  set (APPLEARM64 1)
endif()


if(MSVC OR MSYS OR MINGW)
  set ( platform win_x64)
  
  set (PLAT_LIBS wsock32 ws2_32)

elseif(APPLEARM64)
  set ( platform mac_arm64)
  set (PLAT_LIBS $ENV{MODULE_libuvc}/libopenpnp-capture.0.0.28.dylib)
elseif(APPLE)
  set ( platform mac_x64)
endif()

set(CMAKE_VERBOSE_MAKEFILE ON)

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


find_package(PkgConfig REQUIRED)



# # set(PYTHON_EXECUTABLE "/Applications/Xcode.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/3.8/")
# find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
# # pkg_search_module(LLL REQUIRED python3-embed)

# message(STATUS "Python3_LIBRARIES: ${Python3_LIBRARIES}")
# message(STATUS "Python3_INCLUDE_DIRS: ${Python3_INCLUDE_DIRS}")


# # using find package to load pybind11
# find_package(pybind11 REQUIRED)
# message(STATUS "pybind11_INCLUDE_DIRS: ${pybind11_INCLUDE_DIRS}")
# message(STATUS "pybind11_LIBRARIES: ${pybind11_LIBRARIES}")
# message(STATUS "pybind11_DEFINITIONS: ${pybind11_DEFINITIONS}")
# message(STATUS "pybind11_FOUND: ${pybind11_FOUND}")


# message(STATUS "=== PYTHON LIBRARIES: ${LLL_LIBRARIES}")
# message(STATUS "=== PYTHON LIBRARY_DIRS: ${LLL_LIBRARY_DIRS}")
# message(STATUS "=== PYTHON INCLUDE_DIRS: ${LLL_INCLUDE_DIRS}")


# find_package(PythonLibs REQUIRED)

# include_directories(/Library/Python/3.8/include)

  
if(MSVC OR MSYS OR MINGW)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
elseif(APPLEARM64)
  set(CMAKE_EXE_LINKER_FLAGS " -Wl,-framework,IOKit -Wl,-framework,CoreFoundation -Wl,-framework,Security -framework AVFoundation")

  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS}  -fobjc-arc  -fmodules -gfull")
  SET(PLAT_FILES  "PlatUtil/macUtil.mm")



  message(STATUS "OpenCV_LIBS ${OpenCV_LIBS}")
  foreach(lib ${OpenCV_LIBS})
      get_target_property(lib_path ${lib} LOCATION)
      message(STATUS "${lib} is located at ${lib_path}")
  endforeach()

elseif(APPLE)

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


SET(COMMON_COMPILE_FLAGS "-Wall -Wno-sign-compare -Wno-write-strings -Wno-unused-variable  -g")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${COMMON_COMPILE_FLAGS}")

if($ENV{FEATURE_COMPILE_W_ARAVIS} EQUAL 1)
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_ARAVIS")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_aravis}/include $ENV{MODULE_aravis}/src)

    
  
  if(MSVC OR MSYS OR MINGW)
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_aravis}/${platform}/MVCAMSDK.lib)
  

  elseif(APPLEARM64)
  
    pkg_search_module(GLIB REQUIRED glib-2.0)
    message( >>>> ${GLIB_INCLUDE_DIRS} )
    message( >>GLIB_INCLUDE_DIRS>> ${GLIB_INCLUDE_DIRS} )

    message("GLIB_INCLUDE_DIRS: ${GLIB_INCLUDE_DIRS}")
    message("GLIB_LIBRARY_DIRS: ${GLIB_LIBRARY_DIRS}")
    message("GLIB_CFLAGS_OTHER: ${GLIB_CFLAGS_OTHER}")


    set ( OS_SP_inc "${OS_SP_inc} ${GLIB_INCLUDE_DIRS}")
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA} ${GTK3_LINK_LIBRARIES})
    # message(STATUS "----- GTK3_INCLUDE_DIRS: ${GTK3_INCLUDE_DIRS}")
    # message(STATUS "----- GKT3_LIBRARIES: ${GTK3_LIBRARIES}")
    # message(STATUS "----- GTK3_LINK_LIBRARIES: ${GTK3_LINK_LIBRARIES}")
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_aravis}/src/libaravis-0.8.0.dylib)
  elseif(APPLE)
    
    pkg_search_module(GLIB REQUIRED glib-2.0)
    message( >>GLIB_INCLUDE_DIRS>> ${GLIB_INCLUDE_DIRS} )
    set ( OS_SP_inc "${OS_SP_inc} ${GLIB_INCLUDE_DIRS}")
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA} ${GTK3_LINK_LIBRARIES})
    # message(STATUS "----- GTK3_INCLUDE_DIRS: ${GTK3_INCLUDE_DIRS}")
    # message(STATUS "----- GKT3_LIBRARIES: ${GTK3_LIBRARIES}")
    # message(STATUS "----- GTK3_LINK_LIBRARIES: ${GTK3_LINK_LIBRARIES}")
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_aravis}/src/libaravis-0.8.0.dylib)
  endif()
endif()
if($ENV{FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK} EQUAL 1)
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_hikrobot_camera_sdk}/includes)
  SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_hikrobot_camera_sdk}/libraries/win64/MvCameraControl.lib)
endif()
if($ENV{FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK} EQUAL 1)

  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DFEATURE_COMPILE_W_MINDVISION_CAMERA_SDK")
  SET(INCLUDE_DIRECTORIES  ${INCLUDE_DIRECTORIES} $ENV{MODULE_MindVision_GIGE}/include)

  
  if(MSVC OR MSYS OR MINGW)
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_MindVision_GIGE}/lib/MVCAMSDK.lib)
  else(APPLE)
    SET(LINK_LIBS_CAMERA ${LINK_LIBS_CAMERA}  $ENV{MODULE_MindVision_GIGE}/lib/libmvsdk.dylib)
  endif()
  
  
endif()

file(GLOB INSPECTION_TARGET_SOURCES "InspectionTarget/InspTar_*.cpp")

add_executable (visSele main.cpp wiringPanel.cpp wiringPanel_ext.cpp 
InspectionTarget/InspectionTarget.cpp 
InspectionTarget/InspTarUtil.cpp 
${INSPECTION_TARGET_SOURCES}
${PLAT_FILES}


)
add_dependencies(visSele mod_target MatchingEngine shape_based_matching)


target_link_libraries(visSele
  PUBLIC OpenMP::OpenMP_CXX
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
  $ENV{MODULE_mjpegLib}/mjpegLib.a 
  $ENV{MODULE_MJPEG_Streamer}/MJPEG_Streamer.a
  $ENV{MODULE_BPG_Protocol}/BPG_Protocol.a
  $ENV{MODULE_simple_uart}/simple_uart.a
  #$ENV{MODULE_zlib}/staticlib/libz.a
  $ENV{MODULE_shape_based_matching}/libshape_based_matching.a
  $ENV{MODULE_quickjs}/libquickjs.a
  ${OS_DEP_LIB}
  # ${OpenCV_LIBS}
  opencv_core
  opencv_imgproc
  opencv_imgcodecs opencv_calib3d
  opencv_video
  # ${Python3_LIBRARIES}
  ${PLAT_LIBS}
  ${LINK_LIBS_CAMERA}
  
  )

include_directories (
  ${INCLUDE_DIRECTORIES}
  "." 
  "./include"
  "./PlatUtil"
  "./InspectionTarget"
  $ENV{MODULE_acvImage}/include/ 
  $ENV{MODULE_circleFitting} 
  $ENV{MODULE_common_lib}/include 
  $ENV{MODULE_cJSON} 
  $ENV{MODULE_logctrl}/include 
  #$ENV{MODULE_zlib}/src 
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
  $ENV{MODULE_simple_uart}/include
  $ENV{MODULE_libuvc}/include
  $ENV{MODULE_smem_channel}
  $ENV{MODULE_wfqueue}
  $ENV{MODULE_lodepng}
  $ENV{MODULE_quickjs}/include
  ${OS_SP_inc}
  ${OpenCV_INCLUDE_DIRS}
  # ${Python3_INCLUDE_DIRS}
  # ${pybind11_INCLUDE_DIRS}
  )
  
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
# set(CMAKE_INSTALL_RPATH "@rpath")

# set_target_properties(visSele PROPERTIES 
#                       BUILD_WITH_INSTALL_RPATH TRUE 
#                       INSTALL_RPATH "./")


set_target_properties(visSele PROPERTIES 
                      INSTALL_RPATH "@executable_path/../contrib/libuvc/mac_arm64/;@executable_path/../contrib/aravis/mac_arm64/src;"
                      BUILD_WITH_INSTALL_RPATH TRUE )

add_executable(playground playground.cpp)
target_link_libraries(playground
  PUBLIC OpenMP::OpenMP_CXX
  $ENV{MODULE_MatchingEngine}/libMatchingEngine.a
  $ENV{MODULE_common_lib}/common_lib.a 
  $ENV{MODULE_acvImage}/acvImage.a 
  ${OpenCV_LIBS}
  ${PLAT_LIBS}
)

target_include_directories(playground PRIVATE
  ${INCLUDE_DIRECTORIES}
  "."
  "./include"
  $ENV{MODULE_acvImage}/include/
  $ENV{MODULE_common_lib}/include
  ${OpenCV_INCLUDE_DIRS}
)

# Add SharedMemPluginTemp executable
add_executable(SharedMemPluginTemp 
    PluginRemote/CPP_SharedMemPluginTemp/SharedMemPluginTemp.cpp
)

# Find Boost components with specific configuration for MSYS2
if(MSYS OR MINGW)
  # For MSYS2/MinGW, we need to specify the Boost root directory if it's not in the standard path
  set(BOOST_ROOT "/mingw64")
  set(BOOST_INCLUDEDIR "/mingw64/include")
  set(BOOST_LIBRARYDIR "/mingw64/lib")
  
  # Add Windows-specific compile definitions
  add_compile_definitions(BOOST_USE_WINDOWS_H)
endif()

find_package(Boost REQUIRED COMPONENTS system thread)

target_link_libraries(SharedMemPluginTemp
  PUBLIC OpenMP::OpenMP_CXX
  $ENV{MODULE_cJSON}/cJSON.a
  ${Boost_LIBRARIES}
  ${OpenCV_LIBS}
  ${PLAT_LIBS}
)

# Add platform-specific libraries
if(MSYS OR MINGW)
  target_link_libraries(SharedMemPluginTemp PUBLIC ws2_32 psapi)
endif()

target_include_directories(SharedMemPluginTemp PRIVATE
  ${INCLUDE_DIRECTORIES}
  "."
  "./include"
  "./PluginRemote/CPP_SharedMemPluginTemp"  # Add the new source directory
  $ENV{MODULE_cJSON}
  ${Boost_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

set_target_properties(SharedMemPluginTemp PROPERTIES 
                      INSTALL_RPATH "@executable_path/../contrib/libuvc/mac_arm64/;@executable_path/../contrib/aravis/mac_arm64/src;"
                      BUILD_WITH_INSTALL_RPATH TRUE )
set_target_properties(SharedMemPluginTemp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/PluginRemote/CPP_SharedMemPluginTemp
)