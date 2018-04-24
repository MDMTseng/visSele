
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR= ./include_priv/ \
			./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_common_lib)/include \
			$(MODULE_circleFitting) \
			$(MODULE_LOGCTRL)/include\
			$(MODULE_zlib)/include

ODIR=obj
_OBJ = MatchingEngine.opp MatchingCore.opp FeatureManager.opp FeatureExtractor.opp ContourGrid.opp FeatureManager_group.opp FeatureManager_platingCheck.opp
target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(addsuffix /* ,$(IDIR))
include ../Makefile.in
