
target_a=mjpegLib.a
ODIR=obj


SYSDEPMEM= jmemnobs.o
# library object files common to compression and decompression
COMOBJECTS= jaricom.o jcomapi.o jutils.o jerror.o jmemmgr.o $(SYSDEPMEM)
# compression library object files
CLIBOBJECTS= jcapimin.o jcapistd.o jcarith.o jctrans.o jcparam.o \
        jdatadst.o jcinit.o jcmaster.o jcmarker.o jcmainct.o jcprepct.o \
        jccoefct.o jccolor.o jcsample.o jchuff.o jcdctmgr.o jfdctfst.o \
        jfdctflt.o jfdctint.o
# decompression library object files
DLIBOBJECTS= jdapimin.o jdapistd.o jdarith.o jdtrans.o jdatasrc.o \
        jdmaster.o jdinput.o jdmarker.o jdhuff.o jdmainct.o \
        jdcoefct.o jdpostct.o jddctmgr.o jidctfst.o jidctflt.o \
        jidctint.o jdsample.o jdcolor.o jquant1.o jquant2.o jdmerge.o
# These objectfiles are included in libjpeg.a
LIBOBJECTS= $(CLIBOBJECTS) $(DLIBOBJECTS) $(COMOBJECTS)
# object files for sample applications (excluding library files)
COBJECTS= cjpeg.o rdppm.o rdgif.o rdtarga.o rdrle.o rdbmp.o rdswitch.o \
        cdjpeg.o
DOBJECTS= djpeg.o wrppm.o wrgif.o wrtarga.o wrrle.o wrbmp.o rdcolmap.o \
        cdjpeg.o
TROBJECTS= jpegtran.o rdswitch.o cdjpeg.o transupp.o



IDIR=./include ./libjpeg-master

VPATH=libjpeg-master

_OBJ = mjpegLib.o $(LIBOBJECTS)

MakeTemplate:=../../Makefile.in

include $(MakeTemplate)
