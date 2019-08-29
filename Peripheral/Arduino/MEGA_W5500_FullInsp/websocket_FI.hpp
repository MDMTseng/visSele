#ifndef WEBSOCKET_FI____
#define WEBSOCKET_FI____




class json_seg_parser
{
    public:
    char pch;
    int jsonInStrState;
    int jsonCurlyB_C;
    int jsonSquareB_C;
    json_seg_parser();
    virtual void reset();
    virtual int newChar(char ch);
};




json_seg_parser::json_seg_parser(){
    reset();
}
void json_seg_parser::reset()
{
    pch='\0';
    jsonCurlyB_C=0;
    jsonInStrState=0;
    jsonSquareB_C=0;
}
int json_seg_parser::newChar(char ch){
    int ret_val=0;
    if(ch=='"' && pch!='\\')
    {
        jsonInStrState=!jsonInStrState;
    }

    if( (ch=='{' && pch=='}')||(ch=='[' && pch==']') )//End reset
    {
        reset();
        ret_val=0;
    }

    if(!jsonInStrState)
    {
        if( (ch=='{' || ch=='[') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //start point
            ret_val=1;
        }
        switch(ch)
        {
            case '{':
                jsonCurlyB_C++;
            break;
            case '[':
                jsonSquareB_C++;
            break;
            case '}':
                jsonCurlyB_C--;
            break;
            case ']':
                jsonSquareB_C--;
            break;
        }
        
        if( (ch=='}' || ch==']') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //end point
            ret_val=-1;
        }
        if(jsonCurlyB_C<0 || jsonSquareB_C<0 )//Error reset
        {
            reset();
            ret_val=0;
        }
    }
    pch=ch;
    return ret_val;
}






class Websocket_FI_proto:public Websocket_Server{
  public:
  json_seg_parser jsparser;
  Websocket_FI_proto(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_Server(buff,buffL,ip,port,gateway,subnet),
    jsparser(){}


  
  
  int findJsonScopeLen(char *json)
  {
    char* jptr=json;
    
    if(*jptr=='{' || *jptr =='[')//obj/arr scope
    {
      jsparser.reset(); 
      for(;*jptr;jptr++)
      {
        int retVal = jsparser.newChar(*jptr);
        if(retVal==-1)
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
  
  virtual int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo

    
    recv_cmd[cmdL]='\0';

    if (MessageL == 0)
    {
      //strcpy(tmpStr, (char*)recv_cmd);
      MessageL = sprintf( (char*)send_rsp, "{\"type\":\"ER\",\"MSG\":\"EMPTY, do cmd in child class\"}");
    }
    return MessageL;
  }
  

};



#endif
