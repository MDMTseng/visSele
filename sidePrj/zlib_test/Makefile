
IDIR = $(MODULE_zlib)/src
_OBJ = zlib_test.o

target_bin = zlib_test


ifeq ($(OS)$(CC),Windows_NTcc)
	EXT_OBJS= $(MODULE_zlib)/staticlib/libz.a
else
	LDIR=$(MODULE_zlib)/lib
	LIBS=-lz
endif

export MakeTemplate:= $(abspath Makefile.in)
include $(MakeTemplate)
