CC      = gcc
CFLAGS  = -g
RM      = rm -f

default: all

all: ArcSplineCalc

ArcSplineCalc: ArcSplineCalc.c
	$(CC) $(CFLAGS) -o out ArcSplineCalc.c

clean veryclean:
	$(RM) out