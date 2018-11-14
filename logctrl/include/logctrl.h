#ifndef LOGCTRL_HEADER
#define LOGCTRL_HEADER
#include <stdio.h>
#include <stdint.h>


#define VA_ARGS(...) , ##__VA_ARGS__

void logv(const char *fmt, ...);
//#define logv
void logd(const char *fmt, ...);
void logi(const char *fmt, ...);
void loge(const char *fmt, ...);
char* byteArrString(uint8_t *data, int length,int spaceInterval);
char* byteArrString(uint8_t *data, int length);

#define LOGV(fmt,...) logv("%s:v " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
#define LOGD(fmt,...) logd("%s:d " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
#define LOGI(fmt,...) logi("%s:i " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
#define LOGE(fmt,...) loge("%s:e " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
#endif
