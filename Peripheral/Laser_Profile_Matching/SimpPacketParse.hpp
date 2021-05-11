#ifndef SimpPacketParse_HPP___
#define SimpPacketParse_HPP___


#include "json_seg_parser.hpp"
class SimpPacketParse{
  int cur_size;
  
  json_seg_parser jsparser;
public:
  static const char _START_='@';
  static const char _END_='$';
  char *buffer;
  SimpPacketParse(int size)
  {
    buffer=new char[size];
    cur_size=0;
  }
  
  void add(char ch)
  {
    buffer[cur_size]=ch;
    cur_size++;
  }
  
  int size()
  {
    return cur_size;
  }
  
  void clean()
  {
    cur_size=0;
  }

  int CMD_parse(SimpPacketParse *spp)
  {
    return 0;
  }
  
  bool feed(char ch,bool wCallbackStyle=false)
  {
    if(size()==0)//looking for $
    {
      if(ch==_START_)
      {
        add((char)ch);
      }
    }
    else//looking for @
    {
      if(ch==_END_)
      {
        add('\0');
        if(wCallbackStyle)
        {
          CMD_parse(this);
          clean();
        }
        else
          return true;
      }
      else
      {
        if(buffer[0]==_START_)
        {
          clean();
        }
        add((char)ch);
      }
    }
    return false;
  }



  
  
  int findJsonScopeLen(char *json)
  {
    char* jptr=json;
    
    if(*jptr=='{' || *jptr =='[')//obj/arr scope
    {
      jsparser.reset(); 
      for(;*jptr;jptr++)
      {
        int retVal = jsparser.newChar(*jptr);
        if(retVal&JSON_SEG_PARSER_SEG_END)
        {
          break;
        }
      }
      return jptr-json+1;
    }

    if(*jptr=='"')//string scope
    {
      jsparser.reset(); 
      jsparser.newChar(*jptr++);
      for(;*jptr;jptr++)
      {
        jsparser.newChar(*jptr);
        if(!jsparser.jsonInStrState)
        {
          break;
        }
      }
      return jptr-json+1;
    }

    //normal data scope Number?
    for(;*jptr;jptr++)
    {
      char ch = *jptr;
      if(ch==',' || ch=='}')
      {
        break;
      }
    }
    return jptr-json;
  }

  
  char * findJsonScope(char *json,char *anchor,int *scopeL)
  {
    if(scopeL)*scopeL=-1;
    char* pch = strstr (json,anchor);
    if(pch==NULL)return NULL;
    pch+=strlen(anchor);
    int Len = findJsonScopeLen(pch);

    if(scopeL)*scopeL=Len;
    return pch;
  }


  static int ParseNumberFromArr(char *numArrStr,uint32_t *numArr, int targetArrLen)
  {//return parsed string length, not number array length
    int ptr_idx=0;
    for(int i=0;i<targetArrLen;i++)
    {
      int ptr_adv = PopNumberFromArr(numArrStr+ptr_idx,&(numArr[i]));
      
      if(ptr_adv==0)
      {
        return 0;
      }

      ptr_idx+=ptr_adv+1;
    }
    return ptr_idx-1;
  }
  
  static int PopNumberFromArr(char *numArrStr,uint32_t *ret_num)
  {
    uint32_t num=0;
    int idx=0;
    if(ret_num==NULL)return 0;
    if(numArrStr[0]<'0' || numArrStr[0] > '9')
    {
      *ret_num=0;
      return 0;
    }
    while(1)
    {
      char c = numArrStr[idx];
      
      if(c <'0' || c > '9')
        break;
        
      num=num*10+(c-'0');
      
      idx++;
    }
    *ret_num = num;
    return idx;
  }

  
  int findJsonScope(char *json,char *anchor,char *extBuff,int extBuffL)
  {
    int scopeL;
    char * scope= findJsonScope(json,anchor,&scopeL);
    if(scope==NULL)return -1;
    if(extBuffL<scopeL+1)return -1;
    memcpy(extBuff,scope,scopeL);
    extBuff[scopeL]='\0';
    return scopeL;
  }
  
  int findJsonResetEnd(char *buff, uint16_t buffL)
  {
    
    for(uint16_t i=0;i<buffL-1;i++)
    {
      if(buff[i]=='}' && buff[i]=='{' )
        return i;
    }
    return -1;
  }

};


#endif
