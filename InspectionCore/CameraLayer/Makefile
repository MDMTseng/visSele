

MODULE_acvImage?=../acvImage
MODULE_MindVision_GIGE?=../contrib/MindVision_GIGE
MODULE_aravis?=../contrib/aravis/mac_x64
IDIR = include \
			$(MODULE_acvImage)/include/ \
			$(MODULE_MindVision_GIGE)/include/ \
			$(MODULE_logctrl)/include \
			$(MODULE_aravis)/include \
			$(MODULE_aravis)/src \

FLAGS= -std=c++11 `pkg-config --cflags glib-2.0`

_OBJ = CameraLayer.opp CameraLayer_BMP.opp CameraLayer_GIGE_MindVision.opp CameraLayer_Aravis.opp

target_a = CameraLayer.a

ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
export MakeTemplate:= $(abspath ../Makefile.in)
include $(MakeTemplate)
