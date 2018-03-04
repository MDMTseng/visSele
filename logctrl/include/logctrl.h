#ifndef LOGCTRL_HEADER
#define LOGCTRL_HEADER
#include <stdio.h>

void logv(const char *fmt, ...);
//#define logv
void logd(const char *fmt, ...);
void logi(const char *fmt, ...);
void loge(const char *fmt, ...);


#endif
