
export MakeTemplate?=$(abspath ../../Makefile.in)

export GLEWSRCDir?=$(abspath ../../../glew-2.1.0)
export GLFWSRCDir?=$(abspath ../../../glfw-3.2.1)
export MODULE_GLACC?=$(abspath ../../GLAcc)
export MODULE_SOIL=$(abspath ../../contrib/SOIL)
export MODULE_lodepng=$(abspath ../../contrib/lodepng)

SUB_MAKEFILES = $(MODULE_GLACC) $(MODULE_SOIL) $(MODULE_lodepng)
EXT_OBJS = $(MODULE_GLACC)/GLAcc.a
_OBJ = GLAccPG.opp
target_bin=GLAccPG
ESS_TRACK=$(MODULE_GLACC)/include/*


IDIR+= ./include $(MODULE_GLACC)/include  $(GLEWSRCDir)/include $(GLFWSRCDir)/include $(MODULE_SOIL)/include $(MODULE_lodepng)

EXT_OBJS+= $(MODULE_SOIL)/SOIL.a $(MODULE_lodepng)/lodepng.a
ifeq ($(OS),Windows_NT)
	EXT_OBJS += $(GLFWSRCDir)/lib-mingw-w64/libglfw3.a
	LIBS+=-lopengl32 -lwinmm -lgdi32
else
	LIBS+= -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
endif
FLAGS=-w
include $(MakeTemplate)
