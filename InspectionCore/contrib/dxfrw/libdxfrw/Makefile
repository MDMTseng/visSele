

VPATH = $(MODULE_LIBDXFRW_src) $(MODULE_LIBDXFRW_src)/intern
ODIR=obj
IDIR=$(MODULE_LIBDXFRW_src) $(MODULE_LIBDXFRW_src)/intern $(LIBICONV_ROOT)/include

srcs=$(shell find $(MODULE_LIBDXFRW_src) -name  "*.c*" -printf "%f\n")
_OBJ = $(srcs:.cpp=.opp)


target_a = libdxfrw.a
export FLAGS= -w
include $(MakeTemplate)
