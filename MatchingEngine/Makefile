
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR= ./include_priv/ \
			./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_circleFitting) \
			$(MODULE_LOGCTRL)/include

ODIR=obj
_OBJ = MatchingEngine.opp MatchingCore.opp FeatureManager.opp
target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(addsuffix /* ,$(IDIR))
include ../Makefile.in
