ODIR = obj
IDIR = include



ifeq ($(OS),Windows_NT)
	_OBJ = tserial_win.opp
else

	_OBJ = tserial_mac.opp
endif




target_a = serialCom.a

include $(MakeTemplate)