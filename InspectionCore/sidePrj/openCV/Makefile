
MakeTemplate?=../../Makefile.in


relPath=../../
export MODULE_acvImage=$(abspath $(relPath)/acvImage)
export MODULE_Compat_dirent=$(abspath $(relPath)/contrib/Compat_dirent/$(platform))
export MODULE_cJSON=$(abspath $(relPath)/contrib/cJSON)
export MODULE_logctrl=$(abspath $(relPath)/logctrl)
export MODULE_common_lib=$(abspath $(relPath)/common_lib)
export MODULE_lodepng=$(abspath $(relPath)/contrib/lodepng)


export MODULE_openCV=/usr/local/Cellar/opencv/4.5.0_5


ifeq ($(OS)$(CC),Windows_NTcc)
	FLAGS+= -D_MINGW -D_WINDOWS
	
else
	EXT_OBJS+=
endif


IDIR=	include/ \
			$(MODULE_acvImage)/include/ \
			$(MODULE_common_lib)/include \
			$(MODULE_cJSON) \
			$(MODULE_logctrl)/include \
			$(MODULE_Compat_dirent)/include \
			$(MODULE_lodepng) \
			$(MODULE_openCV)/include/opencv4/




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





STRICT_FLAGS= -Wall -Wextra -Werror -Wreturn-type -Werror=return-type
STRICT_FLAGS+= -march=core2 -mtune=intel -msse4.2

OPENCVLibs=-lopencv_ml -lopencv_dnn -lopencv_highgui -lopencv_superres -lopencv_objdetect -lopencv_stitching -lopencv_videostab -lopencv_calib3d -lopencv_features2d -lopencv_photo -lopencv_flann -lopencv_shape -lopencv_video -lopencv_videoio -lopencv_imgcodecs -lopencv_imgproc -lopencv_core
 
export FLAGS+= -w -O3 -std=c++11 -L$(MODULE_openCV)/lib $(OPENCVLibs)  $(STRICT_FLAGS)

target_bin=calibration
ODIR=obj
_OBJ = calibration.opp

include $(MakeTemplate)