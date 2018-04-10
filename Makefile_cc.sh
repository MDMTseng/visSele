
TOOLCHAIN_DIR=/c/SysGCC/raspberry/
export SYSROOT_FLAG="--sysroot=${TOOLCHAIN_DIR}/arm-linux-gnueabihf/sysroot"

#TOOLCHAIN_PREFIX=${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-
export CC="${TOOLCHAIN_PREFIX}gcc.exe ${SYSROOT_FLAG}"
export CXX="${TOOLCHAIN_PREFIX}g++.exe ${SYSROOT_FLAG}"
export AR=${TOOLCHAIN_PREFIX}ar.exe
export RANLIB=${TOOLCHAIN_PREFIX}ranlib.exe
export PREFIX=${TOOLCHAIN_PREFIX}
make
