
target_bin = main
_OBJ = main.o

export MakeTemplate:= ../../Makefile.in
export FLAGS= -w -O3
ifeq ($(OS)$(CC),Windows_NTcc)
    export FLAGS+= -lws2_32
endif
include $(MakeTemplate)
