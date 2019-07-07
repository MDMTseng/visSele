
class Websocket_FI:public Websocket_Server{
  public:
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_Server(buff,buffL,ip,port,gateway,subnet){}
    
  int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo
    
    if (MessageL == 0)
    {
      char *tmpX = send_rsp+200;
      strcpy(tmpX,recv_cmd);
      MessageL = sprintf(send_rsp, "UNKNOWN:%s",tmpX);
    }
    return MessageL;
  }
  
  
};
