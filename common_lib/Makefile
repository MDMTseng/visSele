
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR=	./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include\
			$(MODULE_LOGCTRL)/include\
			$(MODULE_zlib)/include

ODIR=obj
_OBJ = Util.o
target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(addsuffix /* ,$(IDIR))
include ../Makefile.in
