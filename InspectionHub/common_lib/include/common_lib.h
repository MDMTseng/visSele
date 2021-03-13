#ifndef COMMON_LIB_HPP
#define COMMON_LIB_HPP

#include "cJSON.h"
#include "acvImage_BasicTool.hpp"
#include "zlib.h"

typedef struct {
    uint8_t machine[8];
}machine_hash;


int getDataFromJsonObj(cJSON * obj,void **ret_ptr);
int getDataFromJsonObj(cJSON * obj,int idx,void **ret_ptr);
int getDataFromJsonObj(cJSON * obj,const char *name,void **ret_ptr);
int getDataFromJson(cJSON * obj,char *path,void **ret_ptr);
void* JFetch(cJSON * obj,char *path,int type);
void* JFetEx(cJSON * obj,char *path,int type);

#define JFetch_STRING(obj,path) ((char*)JFetch(obj,path,cJSON_String))
#define JFetch_NUMBER(obj,path) ((double*)JFetch(obj,path,cJSON_Number))
#define JFetch_ARRAY(obj,path) ((cJSON*)JFetch(obj,path,cJSON_Array))
#define JFetch_OBJECT(obj,path) ((cJSON*)JFetch(obj,path,cJSON_Object))

#define JFetEx_STRING(obj,path) ((char*)JFetEx(obj,path,cJSON_String))
#define JFetEx_NUMBER(obj,path) ((double*)JFetEx(obj,path,cJSON_Number))
#define JFetEx_ARRAY(obj,path) ((cJSON*)JFetEx(obj,path,cJSON_Array))
#define JFetEx_OBJECT(obj,path) ((cJSON*)JFetEx(obj,path,cJSON_Object))

#define JxSTR(obj,path) JFetEx_STRING(obj,path)
#define JxNUM(obj,path) JFetEx_NUMBER(obj,path)
#define JxARR(obj,path) JFetEx_ARRAY(obj,path)
#define JxOBJ(obj,path) JFetEx_OBJECT(obj,path)


size_t zlibDeflate(uint8_t *dst,size_t dstLen, uint8_t *src, size_t srcLen,int effort);
size_t zlibInflate(uint8_t *dst,size_t dstLen, uint8_t *src, size_t srcLen);



size_t RGB2Gray_collapse(uint8_t *dst_gray,size_t dstLen,uint8_t *src_rgb,size_t srcLen);
size_t Gray2RGB_uncollapse(uint8_t *dst_rgb,size_t dstLen,uint8_t *src_gray,size_t srcLen);
size_t RGB2BW_collapse(uint8_t *dst_bw,size_t dstLen,uint8_t *src_rgb,size_t srcLen);
size_t BW2RGB_uncollapse(uint8_t *dst_rgb,size_t dstLen,uint8_t *src_bw,size_t srcLen);

char* ReadText(const char *filename);
int CheckFileExistance(const char *filename);

uint8_t* ReadByte(const char *filename,int *length);
machine_hash get_machine_hash();


char *ReadFile(char *filename);
int SaveIMGFile(const char *filename, acvImage *img);

int SavePNGFile(const char *filename, acvImage *img);

int LoadIMGFile(acvImage *ret_img, const char *filename);

int LoadPNGFile(acvImage *img, const char *filename);

int Save2PNG(uint8_t *data, int width, int height, int channelCount, const char *filePath);
bool isDirExist(const char* dir_path);
void realfullPath(const char *curPath, char *ret_fullPath);
int cross_mkdir(const char *path);
#endif
