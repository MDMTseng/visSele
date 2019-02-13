
target_bin = main
IDIR = $(MODULE_cwebsocket)/lib/ include
VPATH = $(MODULE_cwebsocket)/lib/
_OBJ = main.o websocket.o

ESS_TRACK=include/*
export MakeTemplate:= $(abspath Makefile.in)
export FLAGS= -w -O3
ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif
include $(MakeTemplate)
