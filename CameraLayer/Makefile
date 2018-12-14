

MODULE_acvImage?=../acvImage
MODULE_MindVision_GIGE?=../contrib/MindVision_GIGE
IDIR = include \
			$(MODULE_acvImage)/include/ \
			$(MODULE_MindVision_GIGE)/include/ \
			$(MODULE_LOGCTRL)/include \

_OBJ = CameraLayer.opp CameraLayer_BMP.opp CameraLayer_GIGE_MindVision.opp

target_a = CameraLayer.a

ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
export MakeTemplate:= $(abspath ../Makefile.in)
include $(MakeTemplate)
