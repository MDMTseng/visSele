#include <SPI.h>
#define private public //dirty trick
#include <Ethernet.h>
#undef private
#include "WebSocketProtocol.h"
#include "include/ETH_Extra.h"
#include "include/UTIL.hpp"


class Websocket_Server{
  private:
   uint8_t *buff;
   uint32_t buffL;
   
   uint8_t counter2Pin;
   uint8_t *retPackage;
  public:
  byte LiveClient = 0;
  
  #define MAX_WSP_CLIENTs MAX_SOCK_NUM
  WebSocketProtocol WSP[MAX_WSP_CLIENTs];
  EthernetServer server;
  
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
//  Websocket_Server(uint8_t* buff,uint32_t buffL)
//  {
//    
//    IPAddress _ip(192,168,2,2);
//    IPAddress _gateway(169, 254, 170, 254);
//    IPAddress _subnet(255, 255, 255, 0);
//    this(buff,buffL,_ip,5213,_gateway,_subnet);
//  }

  Websocket_Server(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    server(port)
  {
    this->ip=ip;
    this->gateway=gateway;
    this->subnet=subnet;
    
    counter2Pin=0;
    byte mac[] = {
      0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
    };
    
    this->buff=buff;
    this->buffL=buffL;
    retPackage=buff;
    Ethernet.begin(mac, ip, gateway, subnet);
    server.begin();
    DEBUG_print("Chat server address:");
    DEBUG_println(Ethernet.localIP());
  }

  WebSocketProtocol::WPFrameInfo retframeInfo;//={.opcode = 1, .isMasking = 0, .isFinal = 1 };
  
  int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo
    if (strcmp(recv_cmd, "/cue/LEFT") == 0) {
      MessageL = sprintf(send_rsp, "/rsp/LEFT");
    }
    else if (strcmp(recv_cmd, "/cue/RIGHT") == 0) {
      MessageL = sprintf(send_rsp, "/rsp/RIGHT");
    }else if (strncmp(recv_cmd, "/cue/TIME/",9) == 0) {
      MessageL = sprintf(send_rsp, "?");
    }else if (strcmp(recv_cmd, "/cue/PING") == 0) {
      MessageL = sprintf(send_rsp, "/rsp/PONG");
    }
    
    if (MessageL == 0)
    {
      char *tmpX = send_rsp+200;
      
      strcpy(tmpX,recv_cmd);
      MessageL = sprintf(send_rsp, "UNKNOWN:%s",tmpX);
    }
  
    return MessageL;
  }
  
  
  
  int RECVWebSocketPkg_binary(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
  {
  
    unsigned int RECVDataL = WProt->getPkgframeInfo().length;
    retframeInfo.opcode = 2; //binary
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
  
    char *dataBuff = retPackage + 8;//write data after 8 bytes(8 bytes are for header)
    dataBuff[0] = 0x55; //add data in front
    memcpy ( dataBuff + 1, RECVData, RECVDataL ); //echo
    unsigned int totalPackageL;
    char* retPkg = WProt->codeFrame(dataBuff, RECVDataL + 1, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
    WProt->getClientOBJ().write(retPkg, totalPackageL);
    //DoRECVData( WProt, client, RECVData);
  }
  
  
  
  int RECVWebSocketPkg(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
  {
    if (WProt->getPkgframeInfo().opcode == 2)//binary data
    {
      DEBUG_print("Binary");
      RECVWebSocketPkg_binary( WProt, client, RECVData);
      return;
    }
    unsigned int RECVDataL = WProt->getPkgframeInfo().length;
    retframeInfo.opcode = 1; //text
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
    //If RECVData is text message, it will end with '\0'
    DEBUG_println(RECVData);
    char *dataBuff = retPackage + 8;//write data after 8 bytes(8 bytes are for header)
    unsigned int MessageL = CMDExec(RECVData, RECVDataL,dataBuff,buffL-8);
    unsigned int totalPackageL;
    char* retPkg = WProt->codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
    WProt->getClientOBJ().write(retPkg, totalPackageL);
  }
  
  
  
  int FindLiveClient()
  {
    
    LiveClient = 0;
  
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      if (WSP[i].alive())
      {
        LiveClient++;
      }
    }
    return LiveClient;
  }
  void clearUnreachableClient()
  {
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      EthernetClient Rc = WSP[i].getClientOBJ();
      if(!WSP[i].alive())continue;
      DEBUG_print("sock:");
      DEBUG_print(Rc.sockindex);
      DEBUG_print(" status:");
  
      int stat = Rc.status();
      DEBUG_println(stat);
  
      if (stat == 0|| stat == 20)
      {
        DEBUG_print("clear timeout sock::sock");
        DEBUG_println(Rc.sockindex);
        DEBUG_print(" state::");
        DEBUG_println(stat);
        Rc.stop();
        WSP[i].rmClientOBJ();
  
      }
    }
    FindLiveClient();
  }
  void PingAllClient()
  {
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      
      if (WSP[i].alive())
      {
        EthernetClient Rc = WSP[i].getClientOBJ();
        DEBUG_print(Rc);
        DEBUG_print(":::sock:");
        DEBUG_println(Rc.sockindex);
        //byte SnIR = ReadSn_IR(WSP[i].getClientOBJ()._sock);
  
        testAlive(Rc.sockindex);
      }
    }
  }


  WebSocketProtocol* findFromProt(EthernetClient client)
  {
  
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      EthernetClient Rc = WSP[i].getClientOBJ();
      if (Rc.sockindex == client.sockindex)
      {
        FindLiveClient();
        return WSP + i;
      }
    }
  
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      if (!WSP[i].alive())
      {
  
        WSP[i].setClientOBJ(client);
  
        //LiveClient = i;
        byte ii = i;
        DEBUG_print("new socket:::");
        DEBUG_print(client.sockindex);
        DEBUG_print('/');
        DEBUG_println(LiveClient);
        // OnClientsChange();
        FindLiveClient();
        return WSP + i;
      }
    }
    return NULL;
  }
  
  void loop_WS()
  {
    
    EthernetClient client = server.available();
    if (!client)
    {
      if (LiveClient)
      {
        
        
        if (counter2Pin++ > 300)//check client still alive periodically
        {
          PingAllClient();
          clearUnreachableClient();
          counter2Pin = 0;
        }
      }
      return;
    }
  
    uint8_t *buffiter = buff;
    unsigned int  KL = 0;
    unsigned int PkgL =  client.available();
    KL = PkgL;
  //  EthernetClass::socketRecv(client._sock, (uint8_t*)buffiter, PkgL);//get raw data
    client.read((uint8_t*)buffiter, PkgL);
    WebSocketProtocol* WSPptr  = findFromProt(client);
    DEBUG_println(">>>>");
    if (WSPptr == NULL)
    {
      client.stop();
      return;
    }
    client = WSPptr->getClientOBJ();
    char *recvData = WSPptr->processRecvPkg(buff, KL);//Check/process the websocket PKG
    DEBUG_println(">>>>");
    byte frameL = WSPptr->getPkgframeInfo().length; //get frame
    
    DEBUG_print(">>>>WSPptr->getState() ");
    DEBUG_print(WSPptr->getState() );
    DEBUG_print(" OP:");
    DEBUG_println(WSPptr->getRecvOPState() );
    if (WSPptr->getState() == WS_HANDSHAKE)//On hand shaking
    {
      DEBUG_print("WS_HANDSHAKE::");
      //DEBUG_println(client._sock);
      client.print((char*)buff);
      return;
    }
  
    if (WSPptr->getState() == UNKNOWN_CONNECTED)
      //not websocket package. might be AJAX or normal TCP data
      //handle it by yourself.
    {
      /*DEBUG_print("unusual close::");
      DEBUG_println(client._sock);
      client.print(WSPptr->codeSendPkg_endConnection(buff));
  
      client.stop();
      WSPptr->rmClientOBJ();*/
  
      DEBUG_println("WSOP_UNKNOWN");
      
      int retL=  CMDExec(buff, KL,buff,buffL);
      WSPptr->getClientOBJ().write(buff, retL);
      return;
    }
  
  
  
  
    
    if (WSPptr->getRecvOPState() == WSOP_CLOSE)//websocket close
    {
  
      DEBUG_print("Normal close::");
      //DEBUG_println(client._sock);
      client.stop();
      WSPptr->rmClientOBJ();
      return;
    }
    // Normal websocket section
    // client::WSPptr
    // recv Data::recvData
    RECVWebSocketPkg(WSPptr, &client, recvData);
  }
  


};
