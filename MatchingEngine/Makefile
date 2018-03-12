

$(shell basename $(CURDIR))

IDIR=./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_LOGCTRL)/include

ODIR=obj
_OBJ = MatchingEngine.opp

target_a= MatchingEngine.a \
					$(MODULE_cJSON)/cJSON.a \

include ../Makefile.in
