
export LIBICONV_ROOT= $(abspath libiconv)
export MODULE_LIBDXFRW= $(abspath libdxfrw)
export MODULE_LIBDXFRW_src=$(abspath ../src)

export MODULE_DWG2DXF= $(abspath dwg2dxf)
export MODULE_DWG2DXF_src=$(abspath ../dwg2dxf)

SUB_MAKEFILES= $(MODULE_LIBDXFRW) $(MODULE_DWG2DXF)

export MakeTemplate:= $(abspath Makefile.in)
include $(MakeTemplate)
