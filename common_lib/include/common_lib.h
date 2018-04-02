#ifndef COMMON_LIB_HPP
#define COMMON_LIB_HPP

#include "cJSON.h"
#include "acvImage_BasicTool.hpp"
int getDataFromJsonObj(cJSON * obj,void **ret_ptr);
int getDataFromJsonObj(cJSON * obj,int idx,void **ret_ptr);
int getDataFromJsonObj(cJSON * obj,char *name,void **ret_ptr);

#endif
