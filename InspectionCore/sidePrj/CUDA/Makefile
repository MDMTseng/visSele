
MSVC_LIB="\"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Tools\MSVC\14.14.26428\lib\x64\""
export MSVC_BIN="/c/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Tools/MSVC/14.14.26428/bin/Hostx64/x64/"
CXX=export PATH=$$PATH:$(MSVC_BIN);cl
MakeTemplate?=$(abspath ../../Makefile.in)
ODIR=obj
EXT_OBJS = cuCode/obj/cuda.opp
target_bin=OUT
LIBS= /LIBPATH:$(MSVC_LIB)
SUB_MAKEFILES = cuCode
include $(MakeTemplate)
