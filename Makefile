
target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ ./
_OBJ = hw.o
EXT_OBJS= $(shell find acvImage/obj/*)
ESS_TRACK= $(shell find acvImage/include/*)
SUB_MAKEFILES = acvImage
MakeTemplate:= $(shell pwd)/MakeFile.in
export FLAGS= -w -O3
include MakeFile.in
