
target_bin = main
_OBJ = main.opp MSteppers.opp GCodeParser.opp

MStepperFolder=../../MStepper


VPATH=$(MStepperFolder) ./
export MakeTemplate:= ./Makefile.in

ESS_TRACK=$(MStepperFolder)/include/* $(MStepperFolder)/*

IDIR=	./ $(MStepperFolder)/ $(MStepperFolder)/include
# EXT_OBJS= $(MODULE_libxd)/lib/libglad.a \
# 	$(MODULE_libxd)/lib/libxd.a \
# 	$(MODULE_libxd)/lib/libglfw3.a \

export FLAGS= -w -O3 -std=c++0x -DX86_PLATFORM -DMSTP_VEC_SIZE=2

include $(MakeTemplate)
