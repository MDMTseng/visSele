#include "common_lib.h"
#include "logctrl.h"
#include <stdexcept>
#include <stdlib.h>

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

  if (obj->type & (cJSON_True|cJSON_False) ) {
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
int getDataFromJsonObj(cJSON * obj,const char *name,void **ret_ptr)
{

  cJSON *tmpObj = cJSON_GetObjectItem(obj,name);
  return getDataFromJsonObj(tmpObj,ret_ptr);
}

int getDataFromJson(cJSON * obj,char *path,void **ret_ptr)
{
  void *dummy_target;
  if(ret_ptr==NULL)
  {
    ret_ptr=&dummy_target;
  }
  char buff[64];//HACK no check
  if(strlen(path)>sizeof(buff))return -1;
  strcpy(buff,path);
  char *nextSection=nextSection;
  int i=0;
  char endType;
  for(i=0;i<sizeof(buff);i++)
  {
    if(i==sizeof(buff)-1)return cJSON_Invalid;
    endType = path[i];
    if(endType=='.' || endType == '[' || endType == ']' || endType == '\0')break;
    buff[i]=endType;
  }
  if(i==0)
  {
    *ret_ptr = obj;
    return obj->type;
  }
  buff[i]='\0';
  int nameLength = i;
  nextSection = path+(i+((endType=='\0')?0:1));

  //printf("%s  %s  %s  obj->type:%d\n",buff,path,nextSection,obj->type);
  cJSON * curobj=obj;
  switch(endType)
  {
    case '.':
    {
      cJSON * getobj=NULL;
      int obj_type = getDataFromJsonObj(curobj,buff,(void**)&getobj);
      if( !(obj_type&cJSON_Object) )
      {
        return cJSON_Invalid;
      }
      return getDataFromJson(getobj,nextSection,ret_ptr);
    }
    break;
    
    case '[':
    {
      
      cJSON * getobj=NULL;
      int obj_type = getDataFromJsonObj(curobj,buff,(void**)&getobj);
      if( !(obj_type&(cJSON_Object|cJSON_Array)) )
      {
        return cJSON_Invalid;
      }
      return getDataFromJson(getobj,nextSection,ret_ptr);
    }
    break;
    
    case ']':
    {
      if(obj->type&cJSON_Array)
      {
        char * pEnd;
        long int idx = strtol (buff,&pEnd,10);
          
        cJSON * getobj=NULL;
        int obj_type = getDataFromJsonObj(curobj,idx,(void**)&getobj);
        if( !(obj_type&cJSON_Object) )
        {
          *ret_ptr = (void*)getobj;
          return obj_type;
        }
        if(nextSection[0]=='.')//Skip the .
          nextSection++;
        return getDataFromJson(getobj,nextSection,ret_ptr);
      }
      else if(obj->type&cJSON_Object)
      {
        if(buff[0]!='\"' || buff[nameLength-1] !='\"')
        {
          return cJSON_Invalid;
        }
        buff[nameLength-1]='\0';
        //buff++;
        nameLength-=2;

        cJSON * getobj=NULL;
        int obj_type = getDataFromJsonObj(curobj,buff+1,(void**)&getobj);
        if( !(obj_type&cJSON_Object) )
        {
          *ret_ptr = (void*)getobj;
          return obj_type;
        }
        return getDataFromJson(getobj,nextSection,ret_ptr);

      }
      else
      {
        return cJSON_Invalid;
      }
    }
    break;
    
    case '\0':
    {
      return getDataFromJsonObj(curobj,buff,ret_ptr);
    }
    break;
    default: return cJSON_Invalid;
  }
  return cJSON_Invalid;
}


void* JFetch(cJSON * obj,char *path,int type)
{
  void* tmp_ptr=NULL;
  if(0!=(getDataFromJson(obj,path,&tmp_ptr)&type))
  {
    return tmp_ptr;
  }
  return NULL;

}

void* JFetEx(cJSON * obj,char *path,int type)
{
  void *ptr = JFetch(obj,path,type);
  if(ptr == NULL)
  {
    char ExpMsg[100];
    sprintf(ExpMsg,"ERROR: JFetEx parse error, path:%s type:%d",path,type);
    throw std::runtime_error(ExpMsg);
  }
  return ptr;
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


uint64_t sdbm(uint8_t* str)
{
    uint64_t hash = 0;
    int c;

    while (c = *str++)
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

machine_hash get_machine_hash()
{
  machine_hash hash_info={0};
  char* txt = ReadText("data/machine_info");
  if(txt)
  {
    char *ptr=txt;
    uint64_t hash = sdbm((uint8_t*)txt);
    free(txt);
    float minL = sizeof(hash)<sizeof(hash_info.machine)?sizeof(hash):sizeof(hash_info.machine);
    memcpy(hash_info.machine,&hash,minL);
  }
  return hash_info;
}

int CheckFileExistance(const char *filename)
{
  if( access( filename, F_OK ) == -1 ) {
    return -1;
  }
  return 0;
}

char* ReadText(const char *filename)
{
   char *buffer = NULL;
   int string_size, read_size;
   FILE *handler = fopen(filename, "r");

   if (handler)
   {
       // Seek the last byte of the file
       fseek(handler, 0, SEEK_END);
       // Offset from the first to the last byte, or in other words, filesize
       string_size = ftell(handler);
       // go back to the start of the file
       rewind(handler);

       // Allocate a string that can hold it all
       buffer = (char*) malloc(sizeof(char) * (string_size + 1) );

       // Read it all in one operation
       read_size = fread(buffer, sizeof(char), string_size, handler);

       // fread doesn't set it so put a \0 in the last position
       // and buffer is now officially a string
       buffer[read_size] = '\0';

       /*if (string_size != read_size)
       {
           // Something went wrong, throw away the memory and set
           // the buffer to NULL
           free(buffer);
           buffer = NULL;
       }
       */
       // Always remember to close the file.
       fclose(handler);
    }

   return buffer;
}

uint8_t* ReadByte(const char *filename,int *length)
{
   if(!length)return NULL;
   uint8_t *buffer = NULL;
   int buf_size, read_size;
   FILE *handler = fopen(filename, "rb");

   if (handler)
   {
       // Seek the last byte of the file
       fseek(handler, 0, SEEK_END);
       // Offset from the first to the last byte, or in other words, filesize
       buf_size = ftell(handler);
       // go back to the start of the file
       rewind(handler);

       // Allocate a string that can hold it all
       buffer = (uint8_t*) malloc(sizeof(uint8_t) * (buf_size) );

       // Read it all in one operation
       read_size = fread(buffer, sizeof(uint8_t), buf_size, handler);

       if (buf_size != read_size)
       {
           free(buffer);
           buffer = NULL;
       }
       *length=buf_size;
       // Always remember to close the file.
       fclose(handler);
    }

    return buffer;
}
