
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR=	./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_logctrl)/include \
			$(MODULE_zlib)/src \
			$(MODULE_lodepng) \
			$(MODULE_mjpegLib)/include \
			$(MODULE_Compat_dirent)/include \

ODIR=obj
_OBJ = Util.o Util2.o SOCK_Msg_Flow.opp ImageSampler.opp 

target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
include ../Makefile.in
