
ODIR=obj
IDIR= ../mac_x64/include/ ../mac_x64/src
_OBJ = arvcameratest.o

glib_20_Dir=$(shell pkg-config --variable prefix glib-2.0)
# $(info $(glib_20_Dir))

FLAGS=`pkg-config --cflags glib-2.0`
LIBS=`pkg-config --libs glib-2.0` -lgobject-2.0  libaravis-0.8.0.dylib  -Wl,-rpath .
# /usr/lib/libiconv.dylib

# EXT_OBJS= $(glib_20_Dir)/lib/libgobject-2.0.a
# EXT_OBJS= \
# 	$(glib_20_Dir)/lib/libglib-2.0.a \
# 	$(glib_20_Dir)/lib/libgmodule-2.0.a \
# 	$(glib_20_Dir)/lib/libgobject-2.0.a

#_OBJ += TestAlgebraicCircleFits.opp


target_bin=arvcameratest

MakeTemplate?=../../../Makefile.in
include $(MakeTemplate)
