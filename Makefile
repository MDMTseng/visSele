
export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_MindVision_GIGE=$(abspath contrib/MindVision_GIGE)
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



export GLEWSRCDir=$(abspath ../glew-2.1.0)
export GLFWSRCDir=$(abspath ../glfw-3.2.1)
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
			$(MODULE_zlib)/src \
			$(MODULE_DataChannel)/include\
			$(MODULE_CameraLayer)/include \
			$(MODULE_MindVision_GIGE)/include



_OBJ = main.opp

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

ifeq ($(OS)$(CC),Windows_NTcc)
	export platform=win_x64
	EXT_OBJS+= $(MODULE_zlib)/staticlib/libz.a \
						 $(MODULE_MindVision_GIGE)/$(platform)/MVCAMSDK.lib
	
else
	LIBS+= -lz
	export platform=mac
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

 
export FLAGS= -w -O3 -static -static-libgcc -static-libstdc++ $(STRICT_FLAGS)

ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif
include $(MakeTemplate)
