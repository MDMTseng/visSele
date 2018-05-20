
MakeTemplate?=../../Makefile.in


target_bin=test
ODIR=obj
StaticFreeGLutDir=$(abspath ../../../freeglut)
GLEWSRCDir=$(abspath ../../../glew-2.1.0)
VPATH+=$(GLEWSRCDir)/src
_OBJ = test.opp shaderPlay.o glew.o

IDIR+= ./include $(StaticFreeGLutDir)/include $(GLEWSRCDir)/include
FLAGS+=-D FREEGLUT_STATIC
LIBS+=-L$(StaticFreeGLutDir)/lib/x64 -lfreeglut_static -lopengl32 -lwinmm -lgdi32 -Wl,--subsystem,windows
SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
