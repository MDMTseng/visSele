
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR= ./ \
			./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_circleFitting) \
			$(MODULE_LOGCTRL)/include

ODIR=obj
_OBJ = MatchingEngine.opp MatchingCore.opp FeatureManager.opp
target_a= $(addsuffix .a ,$(MODULE_NAME))

include ../Makefile.in
