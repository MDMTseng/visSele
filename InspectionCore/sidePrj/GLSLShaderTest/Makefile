
MakeTemplate?=../../Makefile.in


target_bin=test
ODIR=obj
GLEWSRCDir=$(abspath ../../../glew-2.1.0)
GLFWSRCDir=$(abspath ../../../glfw-3.2.1)
VPATH+=$(GLEWSRCDir)/src
_OBJ = test.opp glew.o

IDIR+= ./include $(GLEWSRCDir)/include $(GLFWSRCDir)/include
ifeq ($(OS),Windows_NT)
	EXT_OBJS += $(GLFWSRCDir)/lib-mingw-w64/libglfw3.a
	LIBS+=-lopengl32 -lwinmm -lgdi32
else
	LIBS+= -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
endif
SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
