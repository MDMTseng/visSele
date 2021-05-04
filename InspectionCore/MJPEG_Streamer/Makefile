
export MODULE_logctrl?=$(abspath ../logctrl)

export MODULE_common_lib?=$(abspath ../common_lib)
ESS_TRACK= $(wildcard include/*)
IDIR=./include/	$(MODULE_logctrl)/include $(MODULE_common_lib)/include 

ODIR=obj
_OBJ = MJPEG_Streamer.opp

target_a = MJPEG_Streamer.a

include ../Makefile.in
