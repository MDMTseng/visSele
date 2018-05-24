
MakeTemplate?=../../Makefile.in


target_bin=OUT
ODIR=obj
VPATH+=example
_OBJ = main.opp

ifeq ($(OS),Windows_NT)
else
	IDIR+= include
	LIBS+= -std=c++0x -framework OpenCL
endif

include $(MakeTemplate)
