
ifeq ($(OS)$(CC),Windows_NTcc)
	export platform=win_x64
	FLAGS+= -static -static-libgcc -static-libstdc++ -lws2_32
else
	LIBS+= -lz
	export platform=mac_x64
endif




export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_MindVision_GIGE=$(abspath contrib/MindVision_GIGE/$(platform))
export MODULE_Compat_dirent=$(abspath contrib/Compat_dirent/$(platform))
export MODULE_cJSON=$(abspath contrib/cJSON)
export MODULE_logctrl=$(abspath logctrl)
export MODULE_MatchingEngine=$(abspath MatchingEngine)
export MODULE_common_lib=$(abspath common_lib)
export MODULE_zlib=$(abspath contrib/zlib-1.2.11)
export MODULE_SOIL=$(abspath contrib/SOIL)
export MODULE_lodepng=$(abspath contrib/lodepng)
export MODULE_DataChannel=$(abspath DataChannel)
export MODULE_CameraLayer=$(abspath CameraLayer)

export SO_EXPORT_PATH=$(abspath .)



export GLEWSRCDir=$(abspath ../../glew-2.1.0)
export GLFWSRCDir=$(abspath ../../glfw-3.2.1)
export MODULE_GLACC=$(abspath GLAcc)


target_bin=visSele
ODIR=obj
IDIR=	include/ \
			$(MODULE_acvImage)/include/ \
			$(MODULE_MLNN)/include/  \
			$(MODULE_circleFitting) \
			$(MODULE_common_lib)/include \
			$(MODULE_cJSON) \
			$(MODULE_logctrl)/include \
			$(MODULE_MatchingEngine)/include \
			$(MODULE_MatchingEngine)/MorphEngine/include \
			$(MODULE_zlib)/src \
			$(MODULE_DataChannel)/include\
			$(MODULE_CameraLayer)/include \
			$(MODULE_MindVision_GIGE)/include \
			$(MODULE_Compat_dirent)/include \
			$(MODULE_lodepng)



_OBJ = main.opp playground.opp

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix MLNN/obj/,$(MLNN_OBJS)) \
					$(MODULE_MatchingEngine)/MatchingEngine.a \
					$(MODULE_circleFitting)/CircleFitting.a \
					$(MODULE_cJSON)/cJSON.a \
					$(MODULE_logctrl)/logctrl.a \
					$(MODULE_common_lib)/common_lib.a \
					$(MODULE_DataChannel)/DataChannel.a \
					$(MODULE_acvImage)/acvImage.a \
					$(MODULE_CameraLayer)/CameraLayer.a \
					$(MODULE_lodepng)/lodepng.a \

ifeq ($(OS)$(CC),Windows_NTcc)
	EXT_OBJS+= $(MODULE_zlib)/staticlib/libz.a \
			   $(MODULE_MindVision_GIGE)/lib/MVCAMSDK.lib
	FLAGS+= -D_MINGW -D_WINDOWS
	
else
	FLAGS+= -lmvsdk  -std=c++11 -L$(MODULE_MindVision_GIGE)/lib/
	EXT_OBJS+=
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

	ESS_TRACK= $(addsuffix /*.h* ,$(IDIR))
	SUB_MAKEFILES = $(MODULE_acvImage) \
									$(MODULE_common_lib) \
									$(MODULE_MLNN) \
									$(MODULE_circleFitting) \
									$(MODULE_cJSON) \
									$(MODULE_logctrl) \
									$(MODULE_MatchingEngine) \
									$(MODULE_zlib) \
									$(MODULE_GLACC) \
									$(MODULE_SOIL) \
									$(MODULE_lodepng) \
									$(MODULE_DataChannel) \
									$(MODULE_CameraLayer) \
									sidePrj
endif



export MakeTemplate:= $(abspath Makefile.in)
STRICT_FLAGS= -Wall -Wextra -Werror -Wreturn-type -Werror=return-type

 
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
