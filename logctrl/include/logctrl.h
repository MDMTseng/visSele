#ifndef LOGCTRL_HEADER
#define LOGCTRL_HEADER
#include <stdio.h>


#define VA_ARGS(...) , ##__VA_ARGS__

void logv(const char *fmt, ...);
//#define logv
#define LOGV(fmt,...) logv("%s:" fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
void logd(const char *fmt, ...);
void logi(const char *fmt, ...);
void loge(const char *fmt, ...);


#endif
