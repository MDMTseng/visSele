

ESS_TRACK= $(wildcard include/*)
IDIR=./include/
ODIR=obj
_OBJ = acvImage.opp acvImage_BasicTool.opp acvImage_BasicDrawTool.opp\
 acvImage_MophologyTool.opp acvImage_ToolBox.opp acvImage_ComponentLabelingTool.opp\
 acvImage_SpDomainTool.opp

#ifeq ($(OS)$(CC),Windows_NTcc)
#target_lib=$(SO_EXPORT_PATH)/libacvImage.dll
#else
#target_lib=$(SO_EXPORT_PATH)/libacvImage.so
#endif
target_a = acvImage.a
include ../Makefile.in
