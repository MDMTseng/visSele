
IDIR=.
ODIR=obj
_OBJ = smem_channel.opp 
$(info -------------$(OS)-------------)
ifeq ($(findstring MINGW64, $(OS)),MINGW64)
	_OBJ+= sp_win.opp 
else ifeq ($(OS)$(CC),Windows_NTcc)
	_OBJ+= sp_win.opp 
else
	_OBJ+= sp_posix.opp 
endif

LIBS= -lrt
target_a= smem_channel.a

include $(MakeTemplate)
