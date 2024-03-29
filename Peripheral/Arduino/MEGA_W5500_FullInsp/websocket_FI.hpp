#ifndef WEBSOCKET_FI____
#define WEBSOCKET_FI____

#include "include/json_seg_parser.hpp"


class Websocket_FI_proto:public Websocket_Server{
  private:
  json_seg_parser jsCMDparser;
  protected:
  uint8_t json_sec_buffer_size=0;
  uint8_t json_sec_buffer[200];
  
  uint8_t json_rsp_buffer[200];

  
  public:
  json_seg_parser jsparser;
  Websocket_FI_proto(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_Server(buff,buffL,ip,port,gateway,subnet),
    jsparser(),jsCMDparser(){}


  
  
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

  
  virtual int CMDExec(WebSocketProtocol* WProt,uint8_t *recv_cmd, int cmdL, sending_buffer *send_pack,int data_in_pack_maxL)//the recv_cmd & send_rsp is using the same buffer, so use it carefully
  {
    sending_buffer *json_rsp_pack=(sending_buffer *)json_rsp_buffer;
//    {
//      if(cmdL==0 ||recv_cmd==NULL )
//      {
//        return 0;
//      }
//
//      recv_cmd[cmdL]='\0';
//      uint8_t *offset_cmd=send_rsp+(rspMaxL-cmdL);
//      memcpy(offset_cmd,recv_cmd,cmdL+1);
//      recv_cmd = offset_cmd;
//      rspMaxL-=cmdL;
//      return Json_CMDExec(recv_cmd,  cmdL,send_rsp, rspMaxL);
//    }
//
//    return 0;
//    
//
//    recv_cmd[cmdL]='\0';
//    
//    
//    int rspLen = Json_CMDExec(WProt,recv_cmd,  cmdL,json_rsp_pack, sizeof(json_rsp_buffer)-sizeof(sending_buffer));
//    if(rspLen>0)
//      SEND(WProt,json_rsp_pack, rspLen, 0);
//    return 0;


    bool doSplit=false;
    char ch;
  
    char *pjson_stream=recv_cmd;
    //printf("json_stream:::%s\n",json_stream);
    for(int i=0;i<cmdL;i++)
    {
      ch=*(pjson_stream++);
      int ret = jsCMDparser.newChar(ch);
      
      // printf("%c:",ch);
      // bin(ret,6);
      // printf(" str:%d C:%d S:%d\n",jsp.jsonInStrState,jsp.jsonCurlyB_C,jsp.jsonSquareB_C);
      bool isEnded=false;
      bool isError=false;
      
      if(ret&JSON_SEG_PARSER_SEG_START)
      {
        if(json_sec_buffer_size>0)
        {
          json_sec_buffer[json_sec_buffer_size]='\0';
          json_sec_buffer_size++;
//          printf("EEEE%d len:%d\n",__LINE__,json_sec_buffer_size);
          
          int rspLen = Json_ERROR(WProt,json_sec_buffer,  json_sec_buffer_size,json_rsp_pack, sizeof(json_rsp_buffer)-sizeof(sending_buffer));
          if(rspLen>0)
            SEND(WProt,json_rsp_pack, rspLen, 0);
        }
        json_sec_buffer_size=0;
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
  
      
      if((json_sec_buffer_size==0 && ch!='{')||json_sec_buffer_size>100)
      {
//          printf("ERROR!!!! RECV(%d):%s\n",cmdL,recv_cmd);
//          printf("ERROR!!!! RECV(%d)+10:%s\n",cmdL,recv_cmd+10);
//          printf("ERROR!!!! json_sec_buffer:%s\n",json_sec_buffer);
      }
      json_sec_buffer[json_sec_buffer_size]=ch;
      json_sec_buffer_size++;

      if(json_sec_buffer_size==200 && ch!='{')
      {
        
          json_sec_buffer[json_sec_buffer_size-1]='\0';
          //printf("ERROR!!!! FULL:%s\n",json_sec_buffer);
          json_sec_buffer_size=0;
      }
      if(isEnded)
      {
        
        if(cmdL>80 || cmdL!=json_sec_buffer_size)
        { 
          doSplit=true;
          //printf("recv:%d  buf:%d   isError:%d\n",cmdL,json_sec_buffer_size,isError);
        }
        json_sec_buffer[json_sec_buffer_size]='\0';
        json_sec_buffer_size++;
        if(isError)
        {
          //printf("EEEE%d len:%d\n",__LINE__,json_sec_buffer_size);
          int rspLen = Json_ERROR(WProt,json_sec_buffer,  json_sec_buffer_size,json_rsp_pack, sizeof(json_rsp_buffer)-sizeof(sending_buffer));
          if(rspLen>0)
            SEND(WProt,json_rsp_pack, rspLen, 0);
          
        }
        else
        {
          char bkChar=json_sec_buffer[json_sec_buffer_size];
          json_sec_buffer[json_sec_buffer_size]='\0';
          
          int rspLen = Json_CMDExec(WProt,json_sec_buffer,  json_sec_buffer_size,json_rsp_pack, sizeof(json_rsp_buffer)-sizeof(sending_buffer));
          json_sec_buffer[json_sec_buffer_size]=bkChar;
          if(rspLen>0)
            SEND(WProt,json_rsp_pack, rspLen, 0);
          
          //
        }
        json_sec_buffer_size=0;
      }

    }
    
//    json_sec_buffer[json_sec_buffer_size]='\0';
//    if(json_sec_buffer_size)
//    {
//      printf(">>>recv_cmd>%d>>>%s\n",cmdL,recv_cmd);
//      printf(">>>rest len>%d>>>%s\n",json_sec_buffer_size,json_sec_buffer);
//    }
//  


    
    return 0;//stop using lagacy way to send msg back
  }
  virtual int Json_ERROR(WebSocketProtocol* WProt,uint8_t *recv_error_seg, int segL,sending_buffer *send_pack,int data_in_pack_maxL)
  {
    
    return -1;
  }
  
  virtual int Json_CMDExec(WebSocketProtocol* WProt,uint8_t *recv_cmd, int cmdL,sending_buffer *send_pack,int data_in_pack_maxL)
  {
    
    unsigned int MessageL = 0; //echo

    
    recv_cmd[cmdL]='\0';

    if (MessageL == 0)
    {
      //strcpy(tmpStr, (char*)recv_cmd);
      MessageL = sprintf( (char*)send_pack->data, "{\"type\":\"ER\",\"MSG\":\"EMPTY, do cmd in child class\"}");
    }
    return MessageL;
  }
  

};



#endif
