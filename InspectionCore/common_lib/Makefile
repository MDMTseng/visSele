
MODULE_NAME=$(notdir $(basename $(CURDIR)))

IDIR=	./include/ \
			$(MODULE_cJSON) \
			$(MODULE_acvImage)/include \
			$(MODULE_logctrl)/include \
			$(MODULE_zlib)/src \
			$(MODULE_lodepng) \
			$(MODULE_mjpegLib)/include \
			$(MODULE_exprtk) \
			$(MODULE_Compat_dirent)/include \
			$(MODULE_simple_uart)/include \

ODIR=obj
_OBJ = Util.o Util2.o ImageSampler.opp Data_Layer_PHY.opp Data_Layer_Protocol.opp json_seg_parser.opp CompScript.opp

target_a= $(addsuffix .a ,$(MODULE_NAME))
ESS_TRACK=$(wildcard $(addsuffix /*.h* ,$(IDIR)))
include ../Makefile.in
