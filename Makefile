
export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_cJSON=$(abspath contrib/cJSON)
export MODULE_LOGCTRL=$(abspath logctrl)
export MODULE_VisSeleFeatureManager=$(abspath VisSeleFeatureManager)
export MODULE_MatchingEngine=$(abspath MatchingEngine)

export SO_EXPORT_PATH=$(abspath .)


target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ MLNN/include/ include/ \
			$(MODULE_circleFitting) \
			$(MODULE_cJSON) \
			$(MODULE_LOGCTRL)/include \
			$(MODULE_VisSeleFeatureManager)/include \

LDIR=./ $(MODULE_acvImage)
LIBS=-lacvImage


_OBJ = hw.o experiment.opp

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix MLNN/obj/,$(MLNN_OBJS)) \
					$(MODULE_circleFitting)/circleFitting.a \
					$(MODULE_VisSeleFeatureManager)/VisSeleFeatureManager.a \
					$(MODULE_cJSON)/cJSON.a \
					$(MODULE_LOGCTRL)/logctrl.a \


ESS_TRACK= $(addsuffix /* ,$(IDIR))
SUB_MAKEFILES = $(MODULE_acvImage) \
								$(MODULE_MLNN) \
								$(MODULE_circleFitting) \
								$(MODULE_cJSON) \
								$(MODULE_LOGCTRL) \
								$(MODULE_VisSeleFeatureManager) \
								$(MODULE_MatchingEngine) \
								sidePrj

export MakeTemplate:= $(abspath Makefile.in)
export FLAGS= -w
include $(MakeTemplate)
