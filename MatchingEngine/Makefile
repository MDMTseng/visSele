

MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR=./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_VisSeleFeatureManager)/include \
			$(MODULE_circleFitting) \
			$(MODULE_LOGCTRL)/include

ODIR=obj
_OBJ = MatchingEngine.opp experiment.opp
ESS_TRACK=$(addsuffix /* ,$(IDIR))
target_a= $(addsuffix .a ,$(MODULE_NAME))

include ../Makefile.in
