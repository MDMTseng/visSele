
MakeTemplate?=../../Makefile.in

PATH_libwebsockets=$(MODULE_contrib)/libwebsockets
#FLAGS+=-DLWS_WITH_ESP32
target_bin=websocket
ODIR=obj
IDIR=$(PATH_libwebsockets)/win32port/ $(PATH_libwebsockets)/build/include
_OBJ = websocket.o

EXT_OBJS= $(PATH_libwebsockets)/build/lib/libwebsockets.a
$(info $(EXT_OBJS))

include $(MakeTemplate)
