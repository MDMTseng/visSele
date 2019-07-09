#include <SPI.h>
#define private public //dirty trick
#include <Ethernet.h>
#undef private
#include "WebSocketProtocol.h"
#include "ETH_Extra.h"
#include "UTIL.hpp"


class Websocket_Server{
  protected:
   uint8_t *buff;
   uint32_t buffL;
   
   uint32_t counter2Pin;
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
  
  virtual int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    unsigned int MessageL = 0; //echo
    if (strcmp((char*)recv_cmd, "/cue/LEFT") == 0) {
      MessageL = sprintf((char*)send_rsp, "/rsp/LEFT");
    }
    
    if (MessageL == 0)
    {
      char *tmpX = (char*)send_rsp+200;
      
      strcpy(tmpX,(char*)recv_cmd);
      MessageL = sprintf((char*)send_rsp, "UNKNOWN:%s",tmpX);
    }
  
    return MessageL;
  }
  
  
  
  int RECVWebSocketPkg_binary(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
  {
  
    unsigned int RECVDataL = WProt->getPkgframeInfo().length;
    retframeInfo.opcode = 2; //binary
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
  
    uint8_t *dataBuff = retPackage + 8;//write data after 8 bytes(8 bytes are for header)
    dataBuff[0] = 0x55; //add data in front
    memcpy ( dataBuff + 1, RECVData, RECVDataL ); //echo
    unsigned int totalPackageL;
    char* retPkg = WProt->codeFrame((char*)dataBuff, RECVDataL + 1, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
    WProt->getClientOBJ().write(retPkg, totalPackageL);
    //DoRECVData( WProt, client, RECVData);
  }
  
  
  
  int RECVWebSocketPkg(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
  {
    if (WProt->getPkgframeInfo().opcode == 2)//binary data
    {
      DEBUG_print("Binary");
      return RECVWebSocketPkg_binary( WProt, client, RECVData);
    }
    unsigned int RECVDataL = WProt->getPkgframeInfo().length;
    retframeInfo.opcode = 1; //text
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
    //If RECVData is text message, it will end with '\0'
    DEBUG_println(RECVData);
    char *dataBuff = (char*)retPackage + 8;//write data after 8 bytes(8 bytes are for header)
    unsigned int MessageL = CMDExec((uint8_t*)RECVData, RECVDataL,(uint8_t*)dataBuff,buffL-8);
    if(!MessageL)return 0;
    unsigned int totalPackageL;
    char* retPkg = WProt->codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
    WProt->getClientOBJ().write(retPkg, totalPackageL);
    if(!MessageL)return 0;
  }
  
  virtual int SEND_ALL(uint8_t* data, uint32_t dataL, int isBinary)
  {
    retframeInfo.opcode = isBinary?2:1; //text
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
    //If RECVData is text message, it will end with '\0'
    char *dataBuff = (char*)retPackage + 8;//write data after 8 bytes(8 bytes are for header)
    memcpy(dataBuff,data,dataL);
    unsigned int MessageL = dataL;
    unsigned int totalPackageL;
    for (uint8_t i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      if (WSP[i].alive())
      {
        if(WSP[i].getState()==UNKNOWN_CONNECTED)
        {
          WSP[i].getClientOBJ().write(dataBuff, dataL);
        }
        else
        {
          char* retPkg = WSP[i].codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); 
          //get complete package might have some shift compare to "retPackage"
          WSP[i].getClientOBJ().write(retPkg, totalPackageL);
        }
      }
    }
    return 0;
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
    
    //DEBUG_print("LiveClient:");
    //DEBUG_println(LiveClient);
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
      DEBUG_print(stat);
  
      DEBUG_print(" WSOP:");
      DEBUG_print(WSP[i].getRecvOPState());
      DEBUG_print(" WSStat:");
      DEBUG_println(WSP[i].getState());
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
        DEBUG_println(client.sockindex);
        // OnClientsChange();
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
      //if (LiveClient)
      {
        
        
        if (counter2Pin++ > 10000)//check client still alive periodically
        {
          PingAllClient();
          clearUnreachableClient();
          FindLiveClient();
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
    //DEBUG_println(">>>>");
    if (WSPptr == NULL)
    {
      client.stop();
      return;
    }
    client = WSPptr->getClientOBJ();
    char *recvData = WSPptr->processRecvPkg((char*)buff, KL);//Check/process the websocket PKG
    //DEBUG_println(">>>>");
    byte frameL = WSPptr->getPkgframeInfo().length; //get frame
    
    //DEBUG_print(">>>>WSPptr->getState() ");
    //DEBUG_print(WSPptr->getState() );
    //DEBUG_print(" OP:");
    //DEBUG_println(WSPptr->getRecvOPState() );
    if (WSPptr->getState() == WS_HANDSHAKE)//On hand shaking
    {
      //DEBUG_print("WS_HANDSHAKE::");
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
  
      //DEBUG_print(WSPptr->getRecvOPState());
      //DEBUG_println("UNKNOWN_CONNECTED");
      
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
