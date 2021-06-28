
target_a = DataChannel.a
IDIR = $(MODULE_cwebsocket)/lib/ $(MODULE_acvImage)/include/ \
    $(MODULE_logctrl)/include/ include include_priv \
    $(MODULE_smem_channel)\
VPATH = $(MODULE_cwebsocket)/lib/
_OBJ = DatCH_WebSocket.opp websocket.o ws_server_util.opp DatCH_BPG.opp DatCH_SharedMemory.opp

ESS_TRACK=include/*
export MakeTemplate?= $(abspath ../Makefile.in)
export FLAGS= -w -O3 -std=c++11
ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif
include $(MakeTemplate)
