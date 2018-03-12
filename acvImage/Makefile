

ESS_TRACK= $(wildcard include/*)
IDIR=./include/
ODIR=obj
_OBJ = acvImage.opp acvImage_BasicTool.opp acvImage_BasicDrawTool.opp\
 acvImage_MophologyTool.opp acvImage_ToolBox.opp acvImage_ComponentLabelingTool.opp\
 acvImage_SpDomainTool.opp

ifeq ($(OS),Windows_NT)
target_lib=$(SO_EXPORT_PATH)/libacvImage.dll
else
target_lib=$(SO_EXPORT_PATH)/libacvImage.so
endif

include ../Makefile.in
