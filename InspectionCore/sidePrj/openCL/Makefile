
MakeTemplate?=../../Makefile.in


target_bin=OUT
ODIR=obj
VPATH+=example
_OBJ = main.opp

ifeq ($(OS),Windows_NT)
	IDIR+= include
	FLAGS+= -I"C:\Program Files (x86)\IntelSWTools\system_studio_2020\OpenCL\sdk\include"
	LIBS+= "C:\Program Files (x86)\IntelSWTools\system_studio_2020\OpenCL\sdk\lib\x64\OpenCL.lib"
	# FLAGS+= -I"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v9.1\include"
	# LIBS+= "C:\Windows\System32\OpenCL.dll"
else
	IDIR+= include
	LIBS+= -std=c++0x -framework OpenCL
endif

include $(MakeTemplate)
