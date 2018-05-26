
export MakeTemplate?=$(abspath ../../Makefile.in)

export GLEWSRCDir=$(abspath ../../../glew-2.1.0)
export GLFWSRCDir=$(abspath ../../../glfw-3.2.1)
MODULE_GLACC=$(abspath GLAcc)

SUB_MAKEFILES = $(MODULE_GLACC)
EXT_OBJS = $(MODULE_GLACC)/GLAcc.a
_OBJ = test.opp
target_bin=test




ifeq ($(OS),Windows_NT)
	EXT_OBJS += $(GLFWSRCDir)/lib-mingw-w64/libglfw3.a
	IDIR+= ./include $(MODULE_GLACC)/include  $(GLEWSRCDir)/include
	LIBS+=-lopengl32 -lwinmm -lgdi32
else
	IDIR+= ./include $(MODULE_GLACC)/include  $(GLEWSRCDir)/include
	LIBS+= -lglfw3 -framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo
endif

include $(MakeTemplate)
