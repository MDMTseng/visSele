
MakeTemplate?=../../Makefile.in


target_bin=test
ODIR=obj
GLEWSRCDir=$(abspath ../../../glew-2.1.0)
GLFWSRCDir=$(abspath ../../../glfw-3.2.1)
VPATH+=$(GLEWSRCDir)/src
_OBJ = test.opp glew.o

EXT_OBJS=$(GLFWSRCDir)/lib-mingw-w64/libglfw3.a

IDIR+= ./include $(GLEWSRCDir)/include $(GLFWSRCDir)/include
LIBS+=-L$(StaticFreeGLutDir)/lib/x64 -lopengl32 -lwinmm -lgdi32 -Wl,--subsystem,windows
SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
