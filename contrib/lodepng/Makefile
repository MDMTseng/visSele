
ODIR=obj
IDIR=src include
VPATH=src
_OBJ =lodepng_util.opp lodepng.opp pngdetail.opp
#_OBJ += TestAlgebraicCircleFits.opp
#target_bin=TestAlgebraicCircleFits
target_a=lodepng.a
FLAGS=  -ansi -pedantic -Wall -Wextra -o pngdetail -O3

ifeq ($(OS)$(CC),Windows_NTcc)
FLAGS+= -std=gnu++11
endif

include $(MakeTemplate)
