#include "common_lib.h"
#include "logctrl.h"
#include <stdexcept>


int getDataFromJsonObj(cJSON * obj,void **ret_ptr)
{
  if(obj==NULL)
  {
    return cJSON_Invalid;
  }

  if(obj->type & cJSON_Number)
  {
    *ret_ptr=&obj->valuedouble;
    return cJSON_Number;
  }

  if(obj->type & cJSON_String)
  {
    *ret_ptr=obj->valuestring;
    return obj->type;
  }

  if(obj->type & cJSON_Array)
  {
    *ret_ptr=obj;
    return obj->type;
  }

  if(obj->type & cJSON_Object)
  {
    *ret_ptr=obj;
    return obj->type;
  }

  return cJSON_Invalid;
}
int getDataFromJsonObj(cJSON * obj,int idx,void **ret_ptr)
{
  cJSON *tmpObj = cJSON_GetArrayItem(obj,idx);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}
int getDataFromJsonObj(cJSON * obj,char *name,void **ret_ptr)
{

  cJSON *tmpObj = cJSON_GetObjectItem(obj,name);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}





size_t zlibDeflate(uint8_t *dst,size_t dstLen, uint8_t *src, size_t srcLen,int effort)
{
    if(effort<0)effort=0;
    if(effort>9)effort=9;

    if(dst==NULL || src==NULL)return 0;

    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    // setup "a" as the input and "b" as the compressed output
    defstream.avail_in = srcLen; // size of input, string + terminator
    defstream.next_in = (Bytef *)src; // input char array
    defstream.avail_out = dstLen; // size of output
    defstream.next_out = (Bytef *)dst; // output char array

    // the actual compression work.
    deflateInit(&defstream, effort);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);

    return (size_t)((char*)defstream.next_out-(char*)dst);
}

size_t zlibInflate(uint8_t *dst,size_t dstLen, uint8_t *src, size_t srcLen)
{
    if(dst==NULL || src==NULL)return 0;

    z_stream infstream;
    infstream.zalloc = Z_NULL;
    infstream.zfree = Z_NULL;
    infstream.opaque = Z_NULL;
    // setup "a" as the input and "b" as the compressed output
    infstream.avail_in = srcLen; // size of input, string + terminator
    infstream.next_in = (Bytef *)src; // input char array
    infstream.avail_out = dstLen; // size of output
    infstream.next_out = (Bytef *)dst; // output char array


    // the actual DE-compression work.
    inflateInit(&infstream);
    inflate(&infstream, Z_NO_FLUSH);
    inflateEnd(&infstream);


    return (size_t)((char*)infstream.next_out-(char*)dst);

}




size_t RGB2Gray_collapse(uint8_t *dst_gray,size_t dstLen,uint8_t *src_rgb,size_t srcLen)
{
  if(srcLen%3!=0)return 0;
  if(dstLen*3<srcLen)return 0;

  for (int i = 0; i < srcLen; i += 3)
  {
    *dst_gray++=src_rgb[i];
  }
  return srcLen/3;
}

size_t Gray2RGB_uncollapse(uint8_t *dst_rgb,size_t dstLen,uint8_t *src_gray,size_t srcLen)
{
  if(dstLen<srcLen*3)return 0;

  dst_rgb+=srcLen*3-1;
  for (int i = srcLen-1; i >=0; i--)
  {
    *dst_rgb--=src_gray[i];
    *dst_rgb--=src_gray[i];
    *dst_rgb--=src_gray[i];
  }

  return srcLen/3;
}


size_t RGB2BW_collapse(uint8_t *dst_bw,size_t dstLen,uint8_t *src_rgb,size_t srcLen)
{
  if(srcLen%3!=0)return 0;
  if(dstLen*8*3<srcLen)return 0;

  uint8_t var=0;
  for (int i = 0; i < srcLen; i += 3)
  {
    var<<=1;
    var|=(*src_rgb==255)?1:0;
    src_rgb+=3;
    if((i&0x7)==0x7)
    {
      *dst_bw=var;
      dst_bw++;
    }
  }
  return srcLen/8/3;
}

size_t BW2RGB_uncollapse(uint8_t *dst_rgb,size_t dstLen,uint8_t *src_bw,size_t srcLen)
{
  if(dstLen<srcLen*8*3)return 0;

  uint8_t var=0;


  dst_rgb=&dst_rgb[srcLen*8*3-1];

  for (int i = srcLen-1; i>=0; i -= 1)
  {
    uint8_t var=src_bw[i];
    for(int j=0;j<8;j++)
    {
      uint8_t pixV=(var&0x01)?255:0;
      var>>=1;
      *dst_rgb--=pixV;
      *dst_rgb--=pixV;
      *dst_rgb--=pixV;
    }
  }



  return srcLen*8*3;
}
