
target_a = DataChannel.a
IDIR = $(MODULE_cwebsocket)/lib/ $(MODULE_acvImage)/include/ include include_priv
VPATH = $(MODULE_cwebsocket)/lib/
_OBJ = DatCH_WebSocket.opp websocket.o ws_server_util.opp

ESS_TRACK=include/*
export MakeTemplate?= $(abspath ../Makefile.in)
export FLAGS= -w -O3
ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif
include $(MakeTemplate)
