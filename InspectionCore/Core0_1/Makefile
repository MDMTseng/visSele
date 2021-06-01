
ifeq ($(OS)$(CC),Windows_NTcc)
	export platform=win_x64
	FLAGS+= -static -static-libgcc -static-libstdc++ -lws2_32
else
	LIBS+= -lz 
	export platform=mac_x64
endif



moduleRoot=../


export MODULE_acvImage=$(abspath $(moduleRoot)/acvImage)
export MODULE_cwebsocket=$(abspath $(moduleRoot)/contrib/cwebsocket)
export MODULE_circleFitting=$(abspath $(moduleRoot)/contrib/circleFitting)
export MODULE_aravis=$(abspath $(moduleRoot)/contrib/aravis/$(platform))
export MODULE_MindVision_GIGE=$(abspath $(moduleRoot)/contrib/MindVision_GIGE/$(platform))
export MODULE_Compat_dirent=$(abspath $(moduleRoot)/contrib/Compat_dirent/$(platform))
export MODULE_cJSON=$(abspath $(moduleRoot)/contrib/cJSON)
export MODULE_mjpegLib=$(abspath $(moduleRoot)/contrib/mjpegLib)
export MODULE_logctrl=$(abspath $(moduleRoot)/logctrl)
export MODULE_common_lib=$(abspath $(moduleRoot)/common_lib)
export MODULE_zlib=$(abspath $(moduleRoot)/contrib/zlib-1.2.11)
export MODULE_SOIL=$(abspath $(moduleRoot)/contrib/SOIL)
export MODULE_polyfit=$(abspath $(moduleRoot)/contrib/polyfit)
export MODULE_lodepng=$(abspath $(moduleRoot)/contrib/lodepng)
export MODULE_DataChannel=$(abspath $(moduleRoot)/DataChannel)
export MODULE_CameraLayer=$(abspath $(moduleRoot)/CameraLayer)
export MODULE_MJPEG_Streamer=$(abspath $(moduleRoot)/MJPEG_Streamer)
export MODULE_hikrobot_camera_sdk=$(abspath $(moduleRoot)/contrib/hikrobot_camera_sdk)


# export MODULE_MLNN=$(abspath $(moduleRoot)/MLNN)
export MODULE_MatchingEngine=$(abspath $(moduleRoot)/MatchingEngine)

export SO_EXPORT_PATH=$(abspath .)


export FEATURE_COMPILE_W_ARAVIS=0
export FEATURE_COMPILE_W_hikrobot_CAMERA_SDK=0


# export GLEWSRCDir=$(abspath ../../$(moduleRoot)/glew-2.1.0)
# export GLFWSRCDir=$(abspath ../../$(moduleRoot)/glfw-3.2.1)
# export MODULE_GLACC=$(abspath $(moduleRoot)/GLAcc)


target_bin=visSele
ODIR=obj
IDIR=	include/ \
			$(MODULE_acvImage)/include/ \
			$(MODULE_circleFitting) \
			$(MODULE_common_lib)/include \
			$(MODULE_cJSON) \
			$(MODULE_logctrl)/include \
			$(MODULE_MatchingEngine)/include \
			$(MODULE_MatchingEngine)/include_priv \
			$(MODULE_MatchingEngine)/MorphEngine/include \
			$(MODULE_zlib)/src \
			$(MODULE_DataChannel)/include\
			$(MODULE_CameraLayer)/include \
			$(MODULE_MindVision_GIGE)/include \
			$(MODULE_Compat_dirent)/include \
			$(MODULE_polyfit)/include \
			$(MODULE_mjpegLib)/include \
			$(MODULE_MJPEG_Streamer)/include \
			$(MODULE_lodepng)\
			# $(MODULE_MLNN)/include/  \



_OBJ = main.opp playground.opp MicroInsp_FType.opp Ext_Util_API.opp tmpCodes.opp wiringPanel.opp democode.opp

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(MODULE_MatchingEngine)/MatchingEngine.a \
					$(MODULE_circleFitting)/CircleFitting.a \
					$(MODULE_cJSON)/cJSON.a \
					$(MODULE_logctrl)/logctrl.a \
					$(MODULE_common_lib)/common_lib.a \
					$(MODULE_DataChannel)/DataChannel.a \
					$(MODULE_acvImage)/acvImage.a \
					$(MODULE_CameraLayer)/CameraLayer.a \
					$(MODULE_lodepng)/lodepng.a \
					$(MODULE_mjpegLib)/mjpegLib.a \
					$(MODULE_polyfit)/polyfit.a \
					$(MODULE_MJPEG_Streamer)/MJPEG_Streamer.a \
					# $(addprefix $(MODULE_MLNN)/obj/,$(MLNN_OBJS)) \

ifeq ($(OS)$(CC),Windows_NTcc)
	EXT_OBJS+= $(MODULE_zlib)/staticlib/libz.a \
			   $(MODULE_MindVision_GIGE)/lib/MVCAMSDK.lib
	FLAGS+= -D_MINGW -D_WINDOWS
	FEATURE_COMPILE_W_hikrobot_CAMERA_SDK=1
	
else
	FLAGS+= -lmvsdk  -std=c++11 -L$(MODULE_MindVision_GIGE)/lib/
	EXT_OBJS+=
	FEATURE_COMPILE_W_ARAVIS=1
	

endif


ESS_TRACK=
ifeq ($(BUILDONLY),ME)
	SUB_MAKEFILES=$(MODULE_MatchingEngine)\
								$(MODULE_acvImage) \
								$(MODULE_CameraLayer) \

else ifeq ($(BUILDONLY),CL)
	SUB_MAKEFILES=$(MODULE_CameraLayer) \

else ifeq ($(BUILDONLY),NONE)
	SUB_MAKEFILES=
else

	# ESS_TRACK= $(addsuffix /*.h* ,$(IDIR))
	SUB_MAKEFILES = $(MODULE_acvImage) \
									$(MODULE_common_lib) \
									$(MODULE_circleFitting) \
									$(MODULE_cJSON) \
									$(MODULE_logctrl) \
									$(MODULE_MatchingEngine) \
									$(MODULE_lodepng) \
									$(MODULE_DataChannel) \
									$(MODULE_CameraLayer) \
									$(MODULE_polyfit) \
									$(MODULE_mjpegLib) \
									$(MODULE_MJPEG_Streamer) \
									# $(MODULE_SOIL) \
									# $(MODULE_zlib) \
									# $(MODULE_GLACC) \
									# $(MODULE_MLNN) \
									# sidePrj
endif



export MakeTemplate:= $(abspath ../Makefile.in)
STRICT_FLAGS= -Wall -Wextra -Werror -Wreturn-type -Werror=return-type

 
ifeq ($(FEATURE_COMPILE_W_ARAVIS),1)
	FLAGS+= -D FEATURE_COMPILE_W_ARAVIS `pkg-config --cflags glib-2.0` 
	IDIR+=$(MODULE_aravis)/include $(MODULE_aravis)/src 
	LIBS+=`pkg-config --libs glib-2.0` -lgobject-2.0  $(MODULE_aravis)/src/libaravis-0.8.0.dylib  -Wl,-rpath .
endif

ifeq ($(FEATURE_COMPILE_W_hikrobot_CAMERA_SDK),1)
	FLAGS+= -D FEATURE_COMPILE_W_hikrobot_CAMERA_SDK
	IDIR+=$(MODULE_hikrobot_camera_sdk)/includes
	LIBS+=$(MODULE_hikrobot_camera_sdk)/libraries/win64/MvCameraControl.lib
endif

export FLAGS+= -w -O3 $(STRICT_FLAGS) 

include $(MakeTemplate)

.PHONY:export release
export:
ifeq ($(EXPORT_PATH),)
	$(error "No EXPORT_PATH was set")
else
	-@[ ! -z "$(EXPORT_PATH)" ]&&mkdir -p $(EXPORT_PATH) || true
	cp $(target_bin)* $(EXPORT_PATH)/
	cp $(MODULE_MindVision_GIGE)/lib/* $(EXPORT_PATH)/
	-@[ ! -z "$(EXPORT_PATH)/data" ]&&mkdir $(EXPORT_PATH)/data || true
	cd data ;cp featureDetect.json cameraCalibration.json $(EXPORT_PATH)/data
	-@[ ! -z "$(EXPORT_PATH)/additional_data" ]&&mkdir $(EXPORT_PATH)/additional_data || true
	cd data ;cp  machine_info default_camera_setting.json $(EXPORT_PATH)/additional_data

endif


domake: prepare  $(SUB_MAKEFILES) build_objs $(target_bin) $(target_a) $(target_lib)
	@echo Done GOAL:\"$(MAKECMDGOALS)\" `pwd`

release: 
	make -C $(abspath .) domake
	make -C $(abspath .) export EXPORT_PATH=$(abspath .)/../release_export/InspectionCore/$(platform)
