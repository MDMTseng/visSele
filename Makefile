
target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ MLNN/include/ ./
_OBJ = hw.o
EXT_OBJS= $(shell find acvImage/obj/*) $(shell find MLNN/obj/*)
ESS_TRACK= $(shell find acvImage/include/*) $(shell find MLNN/include/*)
SUB_MAKEFILES = acvImage MLNN
MakeTemplate:= $(shell pwd)/MakeFile.in
export FLAGS= -w
include MakeFile.in
