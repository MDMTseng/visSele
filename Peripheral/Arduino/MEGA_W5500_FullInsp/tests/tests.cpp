#include <stdio.h>
#include <json_seg_parser.hpp>


void bin(unsigned int n,int size=10) 
{ 
    unsigned i; 
    for (i = 1 << size; i > 0; i = i / 2) 
        (n & i)? printf("1"): printf("0"); 
} 

int main()
{
  json_seg_parser jsp;
  char json_stream[]="{\"aaa\":12}{\"bbb\":[12,66,74,\"AA\",}{\"1\":\"DDD\"}]}{\"aaa\":12}";
  char *pjson_stream=json_stream;

  char ch;

  
  printf("json_stream:::%s\n",json_stream);
  int json_buff_size=-1;
  char json_buff[100];
  while((ch=*(pjson_stream++)))
  {
    int ret = jsp.newChar(ch);
    
    // printf("%c:",ch);
    // bin(ret,6);
    // printf(" str:%d C:%d S:%d\n",jsp.jsonInStrState,jsp.jsonCurlyB_C,jsp.jsonSquareB_C);
    bool isEnded=false;
    bool isError=false;
    
    if(ret&JSON_SEG_PARSER_SEG_START)
    {
      if(json_buff_size>0)
      {
        json_buff[json_buff_size]='\0';
        json_buff_size++;
          printf("iii:::%s\n",json_buff);
      }
      json_buff_size=0;
      //printf("SEG_START\n");
    }
    if(ret&JSON_SEG_PARSER_SEG_END)
    {
      isEnded=true;
      //printf("SEG_END\n");
    }
    if(ret&(JSON_SEG_PARSER_ERROR))
    {
      isEnded=true;
      isError=true;

      //printf("ERROR\n");
    }

    
    json_buff[json_buff_size]=ch;
    json_buff_size++;
    if(isEnded)
    {
      json_buff[json_buff_size]='\0';
      json_buff_size++;
      if(isError)
        printf("XXX:::%s\n",json_buff);
      else
        printf(">>>:::%s\n",json_buff);
      json_buff_size=0;
    }

  }
  
  return 0;
}