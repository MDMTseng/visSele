
MakeTemplate?=../../Makefile.in


target_bin=test
ODIR=obj
GLEWSRCDir=$(abspath ../../../glew-2.1.0)
GLFWSRCDir=$(abspath ../../../glfw-3.2.1)
VPATH+=$(GLEWSRCDir)/src
_OBJ = test.opp glew.o GLAcc_GPU_Buffer.opp


ifeq ($(OS),Windows_NT)
	EXT_OBJS=$(GLFWSRCDir)/lib-mingw-w64/libglfw3.a
	IDIR+= ./include $(GLEWSRCDir)/include  $(GLFWSRCDir)/include
	LIBS+=-lopengl32 -lwinmm -lgdi32
else
	EXT_OBJS=
	IDIR+= ./include $(GLEWSRCDir)/include
	LIBS+= -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
endif


SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
