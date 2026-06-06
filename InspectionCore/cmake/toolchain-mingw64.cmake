# Cross-compile toolchain: macOS (host) -> Windows x86_64 via mingw-w64.
# Used together with vcpkg's x64-mingw-dynamic triplet:
#
#   cmake -S InspectionCore -B InspectionCore/build/win64-mingw \
#     -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
#     -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic \
#     -DVCPKG_HOST_TRIPLET=arm64-osx \
#     -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$PWD/InspectionCore/cmake/toolchain-mingw64.cmake \
#     -DFEATURE_HIKROBOT=ON -DFEATURE_ARAVIS=OFF -DFEATURE_MINDVISION=OFF \
#     -DCMAKE_BUILD_TYPE=Release
#
# Assumes `brew install mingw-w64` -- the binaries land in /opt/homebrew/opt/mingw-w64/bin.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX "/opt/homebrew/opt/mingw-w64" CACHE PATH "mingw-w64 install prefix (homebrew)")
set(TRIPLE x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   ${MINGW_PREFIX}/bin/${TRIPLE}-gcc)
set(CMAKE_CXX_COMPILER ${MINGW_PREFIX}/bin/${TRIPLE}-g++)
set(CMAKE_RC_COMPILER  ${MINGW_PREFIX}/bin/${TRIPLE}-windres)
set(CMAKE_AR           ${MINGW_PREFIX}/bin/${TRIPLE}-ar)
set(CMAKE_RANLIB       ${MINGW_PREFIX}/bin/${TRIPLE}-ranlib)
set(CMAKE_LINKER       ${MINGW_PREFIX}/bin/${TRIPLE}-ld)
set(CMAKE_NM           ${MINGW_PREFIX}/bin/${TRIPLE}-nm)
set(CMAKE_OBJCOPY      ${MINGW_PREFIX}/bin/${TRIPLE}-objcopy)
set(CMAKE_OBJDUMP      ${MINGW_PREFIX}/bin/${TRIPLE}-objdump)
set(CMAKE_STRIP        ${MINGW_PREFIX}/bin/${TRIPLE}-strip)

# Locate target headers / libs inside the mingw sysroot. Programs (host
# binaries like cmake-time scripts) stay on the host filesystem.
set(CMAKE_FIND_ROOT_PATH "${MINGW_PREFIX}/toolchain-x86_64/${TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# BOTH (not ONLY): vcpkg's manifest-mode install dir (build/.../vcpkg_installed)
# lives OUTSIDE the mingw sysroot; restricting find_package to the sysroot would
# hide OpenCVConfig.cmake et al. that vcpkg just installed.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
