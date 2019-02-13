
MakeTemplate?=../Makefile.in

export GLEWSRCDir?=$(abspath ../../../glew-2.1.0)
export GLFWSRCDir?=$(abspath ../../../glfw-3.2.1)

target_a=GLAcc.a
ODIR=obj
VPATH+=$(GLEWSRCDir)/src
_OBJ = glew.o GLAcc_GPU_Buffer.opp GLAcc_Framework.opp Shader.opp


ifeq ($(OS),Windows_NT)
	EXT_OBJS=$(GLFWSRCDir)/lib-mingw-w64/libglfw3.a
	IDIR+= ./include $(GLEWSRCDir)/include  $(GLFWSRCDir)/include
	LIBS+=-lopengl32 -lwinmm -lgdi32
else
	EXT_OBJS=
	IDIR+= ./include $(GLEWSRCDir)/include
	LIBS+= -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
	FLAGS+=-Wc++11-extensions
endif

SUB_MAKEFILES = $(MODULE_acvImage)
include $(MakeTemplate)
