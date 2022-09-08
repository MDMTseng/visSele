pacman -Sy

pacman-key --init
pacman -Syu




pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-extra-cmake-modules
pacman -S mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-gdb
pacman -S mingw-w64-x86_64-toolchain

pacman -S vim

pacman -S base-devel

pacman -S git

#put opencv 4.5.2 in C:\\    => CoreHub CMakeList, set(OpenCV_DIR C:/OpenCV-MinGW)
#WebUI1 => the node modules might need to use the old existed node module to compile(some packages are too old)