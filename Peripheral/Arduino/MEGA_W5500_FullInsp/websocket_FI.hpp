#ifndef WEBSOCKET_FI____
#define WEBSOCKET_FI____


class Websocket_FI:public Websocket_Server{
  public:
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_Server(buff,buffL,ip,port,gateway,subnet){}


  

  
  int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo

    char *tmpStr = (char*)send_rsp+200;
    recv_cmd[cmdL]='\0';
    if(cmdL!=0)
    {
      char head_str[]="{\"type\":\"inspRep\",";
      char* pch = strstr (recv_cmd,"{\"type\":\"inspRep\",");
      pch+=sizeof(head_str);
      
      
      Serial.println(pch);
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
