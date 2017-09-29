
MODULE_acvImage?=../../acvImage
MakeTemplate?=../../Makefile.in


target_bin=licensePlate
ODIR=obj
IDIR=$(MODULE_acvImage)/include/
_OBJ = main.o


acvImage_OBJS= $(shell ls $(MODULE_acvImage)/obj/)

EXT_OBJS= $(addprefix $(MODULE_acvImage)/obj/,$(acvImage_OBJS))
$(info $(EXT_OBJS))

SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
