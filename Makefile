
export MODULE_acvImage=$(abspath acvImage)
export MODULE_MLNN=$(abspath MLNN)
export MODULE_cwebsocket=$(abspath contrib/cwebsocket)
export MODULE_circleFitting=$(abspath contrib/circleFitting)
export MODULE_cJSON=$(abspath contrib/cJSON)

target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ MLNN/include/ include/ $(MODULE_circleFitting) $(MODULE_cJSON)
_OBJ = hw.o experiment.opp

acvImage_OBJS= acvImage.opp acvImage_BasicTool.opp acvImage_BasicDrawTool.opp\
 acvImage_MophologyTool.opp acvImage_ToolBox.opp acvImage_ComponentLabelingTool.opp\
 acvImage_SpDomainTool.opp

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix acvImage/obj/,$(acvImage_OBJS)) \
 					$(addprefix MLNN/obj/,$(MLNN_OBJS)) \
					$(MODULE_circleFitting)/circleFitting.a \
					$(MODULE_cJSON)/cJSON.a

ESS_TRACK= $(wildcard include/* acvImage/include/* include/*)
SUB_MAKEFILES = $(MODULE_acvImage) $(MODULE_MLNN) $(MODULE_circleFitting) $(MODULE_cJSON) sidePrj
export MakeTemplate:= $(abspath Makefile.in)
export FLAGS= -w
include $(MakeTemplate)
