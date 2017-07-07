
target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ ./
_OBJ = hw.o
EXT_OBJS= $(shell find acvImage/obj/*)
SUB_MAKEFILES = acvImage
MakeTemplate:= $(shell pwd)/MakeFile.in
export FLAGS= -w
include MakeFile.in
