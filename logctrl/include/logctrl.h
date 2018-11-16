#ifndef LOGCTRL_HEADER
#define LOGCTRL_HEADER
#include <stdio.h>
#include <stdint.h>

//TODO: It's not a thread safe implementation at all so ......especially _SubString_Align

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define VA_ARGS(...) , ##__VA_ARGS__

void logv(const char *fmt, ...);
//#define logv
void logd(const char *fmt, ...);
void logi(const char *fmt, ...);
void loge(const char *fmt, ...);
char* byteArrString(uint8_t *data, int length,int spaceInterval);
char* byteArrString(uint8_t *data, int length);
char* _SubString(const char* str,int Count);
char* _SubString_Align(const char* str,int Count);

// #define LOGV(fmt,...) logv("%s:v " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
// #define LOGD(fmt,...) logd("%s:d " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
// #define LOGI(fmt,...) logi("%s:i " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
// #define LOGE(fmt,...) loge("%s:e " fmt "\n",__func__ VA_ARGS(__VA_ARGS__))
#define LOG_FILENAME_L 10
#define LOGV(fmt,...) logv("%s %s:v " fmt "\n",_SubString_Align(__FILENAME__,LOG_FILENAME_L),__func__ VA_ARGS(__VA_ARGS__))
#define LOGD(fmt,...) logd("%s %s:d " fmt "\n",_SubString_Align(__FILENAME__,LOG_FILENAME_L),__func__ VA_ARGS(__VA_ARGS__))
#define LOGI(fmt,...) logi("%s %s:i " fmt "\n",_SubString_Align(__FILENAME__,LOG_FILENAME_L),__func__ VA_ARGS(__VA_ARGS__))
#define LOGE(fmt,...) loge("%s %s:e " fmt "\n",_SubString_Align(__FILENAME__,LOG_FILENAME_L),__func__ VA_ARGS(__VA_ARGS__))
#endif
