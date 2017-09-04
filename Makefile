
target_bin=visSele
ODIR=obj
IDIR=acvImage/include/ MLNN/include/ include/
_OBJ = hw.o

acvImage_OBJS= acvImage.opp acvImage_BasicTool.opp acvImage_BasicDrawTool.opp\
 acvImage_MophologyTool.opp acvImage_ToolBox.opp acvImage_ComponentLabelingTool.opp\
 acvImage_SpDomainTool.opp

MLNN_OBJS=MLNNUtil.opp MLNL.opp MLNN.opp MLOpt.opp

EXT_OBJS= $(addprefix acvImage/obj/,$(acvImage_OBJS)) $(addprefix MLNN/obj/,$(MLNN_OBJS))
ESS_TRACK= $(wildcard include/* acvImage/include/* include/*)
SUB_MAKEFILES = jpeglib acvImage MLNN
export MakeTemplate:= Makefile.in
export FLAGS= -w -O3
include $(MakeTemplate)
