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






class Websocket_FI:public Websocket_Server{
  public:
  json_seg_parser jsparser;
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
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

  
  
  int findJsonScope(char *json,char *anchor,char *extBuff,int extBuffL)
  {
    extBuff[0]='\0';
    char* pch = strstr (json,anchor);
    if(pch==NULL)return -1;
    pch+=strlen(anchor);
    int Len = findJsonScopeLen(pch);
    if(extBuffL<Len+1)return -1;

    memcpy(extBuff,pch,Len);
    extBuff[Len]='\0';
    return Len;
  }
  
  int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo

    recv_cmd[cmdL]='\0';
    if(cmdL!=0)
    {
      if(strstr ((char*)recv_cmd,"\"type\":\"inspRep\"")!=NULL)
      {
        int buffL=100;
        char *idxStr = (char*)send_rsp+rspMaxL/2;
        int retL = findJsonScope((char*)recv_cmd,"\"idx\":",idxStr,buffL);
        if(retL<0)idxStr=NULL;

        char *statusStr = idxStr+buffL;
        retL = findJsonScope((char*)recv_cmd,"\"status\":",statusStr,buffL);
        if(retL<0)statusStr=NULL;


        if(idxStr && statusStr)
        {
          Serial.println(idxStr);
          Serial.println(statusStr);
        }
      }
    }
    
    

    /*if (MessageL == 0)
    {
      strcpy(tmpStr, (char*)recv_cmd);
      MessageL = sprintf( (char*)send_rsp, "UNKNOWN:%s",tmpStr);
    }*/
    return MessageL;
  }
  

};



#endif
