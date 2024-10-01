

PLATFORM=$(uname -s)_$(uname -m)
#check if the parameter "release" exist
if [ "$1" == "release" ]; then
   rm -rf build_release
   rm visSele
   rm -rf tmp_libs
   make -f Makefile_mods runCMake


   # if PLATFORM is Darwin_arm64

   if [ "$PLATFORM" == "Darwin_arm64" ]; then
      openpnp_libs=("libopenpnp-capture.0.dylib" )

      # Loop through the array
      for element in "${openpnp_libs[@]}"
      do
         install_name_tool -change $(otool -L visSele | grep $element | awk '{print $1}') $(pwd)/../contrib/libuvc/mac_arm64/$element visSele
      done




      aravis_libs=("libaravis-0.8.0.dylib" )

      # Loop through the array
      for element in "${aravis_libs[@]}"
      do
         install_name_tool -change $(otool -L visSele | grep $element | awk '{print $1}') $(pwd)/../contrib/aravis/mac_arm64/src/$element visSele
      done

      dylibbundler -od -b -x visSele -d tmp_libs -p @executable_path/libs/

      mkdir build_release
      mv visSele build_release/
      mv  tmp_libs build_release/libs
   fi


else
   make -f Makefile_mods runCMake
   ret_code=$?
   echo "Debug mode"
   exit $ret_code
fi
