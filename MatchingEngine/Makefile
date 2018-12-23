
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR= ./include_priv/ \
			./include/ \
			./MorphEngine/include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_common_lib)/include \
			$(MODULE_circleFitting) \
			$(MODULE_logctrl)/include\
			$(MODULE_zlib)/src

VPATH = ./ ./MorphEngine
ODIR=obj
_OBJ = MatchingEngine.opp MatchingCore.opp FeatureManager.opp FeatureExtractor.opp ContourGrid.opp \
			 FeatureManager_group.opp FeatureManager_platingCheck.opp FeatureManager_sig360_circle_line.opp \
			 MorphEngine.opp
target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
include ../Makefile.in
