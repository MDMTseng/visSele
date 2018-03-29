
export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_cJSON=$(abspath contrib/cJSON)
export MODULE_LOGCTRL=$(abspath logctrl)
export MODULE_MatchingEngine=$(abspath MatchingEngine)
export MODULE_common_lib=$(abspath common_lib)

export SO_EXPORT_PATH=$(abspath .)


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

LDIR=./ $(MODULE_acvImage)
LIBS=-lacvImage


_OBJ = hw.o

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix MLNN/obj/,$(MLNN_OBJS)) \
					$(MODULE_circleFitting)/circleFitting.a \
					$(MODULE_MatchingEngine)/MatchingEngine.a \
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
								sidePrj

export MakeTemplate:= $(abspath Makefile.in)
export FLAGS= -w -O3
include $(MakeTemplate)
