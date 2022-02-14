
target_bin = main
_OBJ = main.opp MSteppers.opp

SysSyncFolder=../../SysSync


VPATH=$(SysSyncFolder) ./
export MakeTemplate:= ./Makefile.in

ESS_TRACK=$(SysSyncFolder)/include/* $(SysSyncFolder)/*

IDIR=	./ $(SysSyncFolder)/ $(SysSyncFolder)/include
# EXT_OBJS= $(MODULE_libxd)/lib/libglad.a \
# 	$(MODULE_libxd)/lib/libxd.a \
# 	$(MODULE_libxd)/lib/libglfw3.a \

export FLAGS= -w -O3 -std=c++0x -DX86_PLATFORM -DMSTP_VEC_SIZE=2

include $(MakeTemplate)
