#pragma once
#ifdef X86_PLATFORM
#define __PRT_D_(fmt,...) printf("%04d %.*s:d " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)
#define __PRT_I_(fmt,...) printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)
#else
#include <Arduino.h>
#define __PRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __PRT_I_(fmt,...) Serial.printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)




void G_LOG(const char* str);
#endif