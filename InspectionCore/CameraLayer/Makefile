

MODULE_acvImage?=../acvImage
MODULE_MindVision_GIGE?=../contrib/MindVision_GIGE
MODULE_aravis?=../contrib/aravis/mac_x64
IDIR = include \
			$(MODULE_acvImage)/include/ \
			$(MODULE_logctrl)/include \
			$(MODULE_common_lib)/include \


_OBJ = CameraLayer.opp CameraLayer_BMP.opp CameraLayer_BMP_carousel.opp 

FLAGS= -std=c++11 


ifeq ($(FEATURE_COMPILE_W_UVC),1)
	FLAGS+= -D FEATURE_COMPILE_W_UVC
	IDIR+=$(MODULE_libuvc)/include
	_OBJ+= CameraLayer_UVC.opp
endif





ifeq ($(FEATURE_COMPILE_W_ARAVIS),1)
	FLAGS+= -D FEATURE_COMPILE_W_ARAVIS
	FLAGS+=  `pkg-config --cflags glib-2.0`
	IDIR+=$(MODULE_aravis)/include $(MODULE_aravis)/src 
	_OBJ+= CameraLayer_Aravis.opp
endif

ifeq ($(FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK),1)
	FLAGS+= -D FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
	IDIR+=$(MODULE_hikrobot_camera_sdk)/includes
	_OBJ+=CameraLayer_HikRobot_Camera.opp
endif


ifeq ($(FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK),1)
	FLAGS+= -D FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK
	IDIR+=$(MODULE_MindVision_GIGE)/include
	_OBJ+=CameraLayer_GIGE_MindVision.opp
endif


target_a = CameraLayer.a

ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
export MakeTemplate:= $(abspath ../Makefile.in)
include $(MakeTemplate)
