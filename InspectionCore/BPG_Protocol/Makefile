
export MODULE_logctrl?=$(abspath ../logctrl)

export MODULE_common_lib?=$(abspath ../common_lib)
ESS_TRACK= $(wildcard include/*)
IDIR=./include/ ./include_priv	\
	$(MODULE_logctrl)/include \
	$(MODULE_common_lib)/include  \
	$(MODULE_smem_channel)\
	$(MODULE_cwebsocket)/lib/ 

VPATH = $(MODULE_cwebsocket)/lib/
ODIR=obj
_OBJ = BPG_Protocol.opp IPC_Channel.opp BPG_Link_Interface_WebSocket.opp ws_server_util.opp BPG_Link_Interface_SharedMemoryChannel.opp websocket.o 

target_a = BPG_Protocol.a

export FLAGS= -w -O3 -std=c++11
ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif

include $(MakeTemplate)
