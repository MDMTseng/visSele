
export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_cJSON=$(abspath contrib/cJSON)
export MODULE_LOGCTRL=$(abspath logctrl)
export MODULE_MatchingEngine=$(abspath MatchingEngine)
export MODULE_common_lib=$(abspath common_lib)
export MODULE_zlib=$(abspath contrib/zlib-1.2.11)
export MODULE_SOIL=$(abspath contrib/SOIL)
export MODULE_lodepng=$(abspath contrib/lodepng)

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
			$(MODULE_LOGCTRL)/include \
			$(MODULE_MatchingEngine)/include \
			$(MODULE_zlib)/src \

LDIR=./ $(MODULE_acvImage)
LIBS=-lacvImage -lz


_OBJ = hw.o

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix MLNN/obj/,$(MLNN_OBJS)) \
					$(MODULE_MatchingEngine)/MatchingEngine.a \
					$(MODULE_circleFitting)/CircleFitting.a \
					$(MODULE_cJSON)/cJSON.a \
					$(MODULE_LOGCTRL)/logctrl.a \
					$(MODULE_common_lib)/common_lib.a \


ESS_TRACK= $(addsuffix /* ,$(IDIR))
SUB_MAKEFILES = $(MODULE_acvImage) \
								$(MODULE_common_lib) \
								$(MODULE_MLNN) \
								$(MODULE_circleFitting) \
								$(MODULE_cJSON) \
								$(MODULE_LOGCTRL) \
								$(MODULE_MatchingEngine) \
								$(MODULE_zlib) \
								$(MODULE_GLACC) \
								$(MODULE_SOIL) \
								$(MODULE_lodepng) \
								sidePrj

export MakeTemplate:= $(abspath Makefile.in)
export FLAGS= -w -O3
include $(MakeTemplate)
