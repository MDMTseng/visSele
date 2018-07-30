
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR=	./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include\
			$(MODULE_LOGCTRL)/include\
			$(MODULE_zlib)/src

ODIR=obj
_OBJ = Util.o
target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(wildcard $(addsuffix /* ,$(IDIR)))
include ../Makefile.in
