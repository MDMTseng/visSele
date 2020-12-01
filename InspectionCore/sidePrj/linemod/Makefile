
MakeTemplate?=../../Makefile.in


relPath=../../
export MODULE_acvImage=$(abspath $(relPath)/acvImage)
export MODULE_Compat_dirent=$(abspath $(relPath)/contrib/Compat_dirent/$(platform))
export MODULE_cJSON=$(abspath $(relPath)/contrib/cJSON)
export MODULE_logctrl=$(abspath $(relPath)/logctrl)
export MODULE_common_lib=$(abspath $(relPath)/common_lib)
export MODULE_lodepng=$(abspath $(relPath)/contrib/lodepng)





IDIR=	include/ \
			$(MODULE_acvImage)/include/ \
			$(MODULE_common_lib)/include \
			$(MODULE_cJSON) \
			$(MODULE_logctrl)/include \
			$(MODULE_Compat_dirent)/include \
			$(MODULE_lodepng)




EXT_OBJS= $(MODULE_cJSON)/cJSON.a \
					$(MODULE_logctrl)/logctrl.a \
					$(MODULE_common_lib)/common_lib.a \
					$(MODULE_acvImage)/acvImage.a \
					$(MODULE_lodepng)/lodepng.a \



SUB_MAKEFILES = $(MODULE_acvImage) \
								# $(MODULE_common_lib) \
								# $(MODULE_MLNN) \
								# $(MODULE_circleFitting) \
								# $(MODULE_cJSON) \
								# $(MODULE_logctrl) \
								# $(MODULE_MatchingEngine) \
								# $(MODULE_GLACC) \
								# $(MODULE_lodepng) \



ifeq ($(OS)$(CC),Windows_NTcc)
	FLAGS+= -D_MINGW -D_WINDOWS
	
else
	EXT_OBJS+=
endif

STRICT_FLAGS= -Wall -Wextra -Werror -Wreturn-type -Werror=return-type
STRICT_FLAGS+= -march=core2 -mtune=intel -fdeclspec
 
export FLAGS+= -w -O3 $(STRICT_FLAGS)

target_bin=linemod
ODIR=obj
_OBJ = linemod.opp

include $(MakeTemplate)