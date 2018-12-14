

MODULE_acvImage?=../acvImage

IDIR = include $(MODULE_acvImage)/include/
_OBJ = CameraLayer.opp CameraLayer_BMP.opp

target_a = CameraLayer.a

ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
export MakeTemplate:= $(abspath ../Makefile.in)
include $(MakeTemplate)
