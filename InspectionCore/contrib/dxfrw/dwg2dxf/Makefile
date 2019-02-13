
VPATH = $(MODULE_DWG2DXF_src)
ODIR=obj
IDIR=$(MODULE_DWG2DXF_src) $(MODULE_LIBDXFRW_src)
_OBJ = dx_iface.opp main.opp

target_bin = dwg2dxf

SUB_MAKEFILES= $(MODULE_LIBDXFRW)
EXT_OBJS=$(MODULE_LIBDXFRW)/libdxfrw.a
export FLAGS+= -L$(LIBICONV_ROOT)/lib/ -liconv
include $(MakeTemplate)
