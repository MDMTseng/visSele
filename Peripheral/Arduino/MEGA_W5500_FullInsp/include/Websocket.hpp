#include "WebSocketProtocol.h"
#include "ETH_Extra.h"
#include "UTIL.hpp"


class Websocket_Server{
  protected:
   uint8_t *buff;
   uint32_t buffL;
   
   uint32_t counter2Pin;
   uint8_t *retPackage;
   typedef struct {
     uint8_t header[8];
     uint8_t data[0];
   }sending_buffer;
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
  
  virtual int CMDExec(WebSocketProtocol* WProt,uint8_t *recv_cmd, int cmdL,sending_buffer *send_pack,int data_in_pack_maxL)
  {
    unsigned int MessageL = 0; //echo
    if (strcmp((char*)recv_cmd, "/cue/LEFT") == 0) {
      MessageL = sprintf(send_pack->data, "/rsp/LEFT");
    }
  
    return MessageL;
  }
  
  virtual void onPeerDisconnect(WebSocketProtocol* WProt)
  {
    return;
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
    //DEBUG_println(RECVData);
    sending_buffer *send_rsp=(sending_buffer *)retPackage;
    unsigned int MessageL = CMDExec(WProt,(uint8_t*)RECVData, RECVDataL,send_rsp,buffL-sizeof(sending_buffer));
    if(!MessageL)return 0;
    unsigned int totalPackageL;
    char* retPkg = WProt->codeFrame(send_rsp->data, MessageL, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
    WProt->getClientOBJ().write(retPkg, totalPackageL);
    return 0;
  }

  virtual int SEND(WebSocketProtocol *WP,sending_buffer* data_buff, uint32_t data_inbuff_L, int isBinary)
  {
    if(WP==NULL)return -1;
    retframeInfo.opcode = isBinary?2:1; //text
    retframeInfo.isMasking = 0; //no masking on server side
    retframeInfo.isFinal = 1; //is Final package
    //If RECVData is text message, it will end with '\0'
    uint8_t* dataBuff=(uint8_t*)(data_buff->data);

    unsigned int totalPackageL;
    char* retPkg=NULL;
    
    if (WP->alive())
    {
      if(WP->getState()==UNKNOWN_CONNECTED)
      {
        WP->getClientOBJ().write(dataBuff, data_inbuff_L);
      }
      else
      {
        if(!retPkg)
          retPkg = WP->codeFrame(dataBuff, data_inbuff_L, &retframeInfo, &totalPackageL); 
        //get complete package might have some shift compare to "retPackage"
        WP->getClientOBJ().write(retPkg, totalPackageL);
      }
    }
    return 0;
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
    char* retPkg=NULL;
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
          if(!retPkg)
            retPkg = WSP[i].codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); 
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
    
    // DEBUG_print("LiveClient:");
    // DEBUG_println(LiveClient);
    return LiveClient;
  }
  void clearUnreachableClient()
  {
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      EthernetClient Rc = WSP[i].getClientOBJ();
      if(!WSP[i].alive())continue;
      // DEBUG_print("sock:");
      // DEBUG_print(Rc.getSocketNumber());
      // DEBUG_println(" status:");
      int stat = Rc.status();
      // DEBUG_print(stat);
  
      // DEBUG_print(" WSOP:");
      // DEBUG_print(WSP[i].getRecvOPState());
      // DEBUG_print(" WSStat:");
      // DEBUG_println(WSP[i].getState());
      if (stat == 0|| stat == 20)
      {
        DEBUG_print("clear timeout sock::");
        DEBUG_println(Rc.getSocketNumber());
        DEBUG_print(" state::");
        DEBUG_println(stat);
        Rc.stop();
        
        onPeerDisconnect(&WSP[i]);
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
        // DEBUG_print(Rc);
        // DEBUG_print(":::sock:");
        // DEBUG_println(Rc.getSocketNumber());
        //byte SnIR = ReadSn_IR(WSP[i].getClientOBJ()._sock);
  
        testAlive(Rc.getSocketNumber());
      }
    }
  }


  WebSocketProtocol* findFromProt(EthernetClient client)
  {
  
    for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    {
      EthernetClient Rc = WSP[i].getClientOBJ();
      if (Rc.getSocketNumber() == client.getSocketNumber())
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
        DEBUG_println(client.getSocketNumber());
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
        if (counter2Pin++ > 40000)//check client still alive periodically
        {
          //DEBUG_println("PingAllClient:");
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
    if(PkgL>(buffL-50))
    {
      PkgL=buffL-50;
    }
    KL = PkgL;
  //  EthernetClass::socketRecv(client._sock, (uint8_t*)buffiter, PkgL);//get raw data
    client.read((uint8_t*)buffiter, PkgL);
    buffiter[PkgL]=0;
    // DEBUG_print("RAW::");
    // DEBUG_println((char*)buffiter);
    WebSocketProtocol* WSPptr  = findFromProt(client);
    //DEBUG_println(">>>>");
    if (WSPptr == NULL)
    {
      DEBUG_println("client.stop()");
      client.stop();
      return;
    }
    client = WSPptr->getClientOBJ();
    char *recvData = WSPptr->processRecvPkg((char*)buff, KL);//Check/process the websocket PKG
    //DEBUG_println(">>>>");
    byte frameL = WSPptr->getPkgframeInfo().length; //get frame
    
    // DEBUG_print(">>>>WSPptr->getState() ");
    // DEBUG_print(WSPptr->getState() );
    // DEBUG_print(" OP:");
    // DEBUG_println(WSPptr->getRecvOPState() );
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
      

      sending_buffer *send_rsp=(sending_buffer *)buff;

      int retL=  CMDExec(WSPptr,buff, KL,send_rsp,buffL-sizeof(sending_buffer));
      WSPptr->getClientOBJ().write(buff, retL);
      return;
    }
  
  
  
  
    
    if (WSPptr->getRecvOPState() == WSOP_CLOSE)//websocket close
    {
  
      DEBUG_print("Normal close::");
      //DEBUG_println(client._sock);
      client.stop();
      onPeerDisconnect(WSPptr);
      WSPptr->rmClientOBJ();
      return;
    }
    // Normal websocket section
    // client::WSPptr
    // recv Data::recvData
    RECVWebSocketPkg(WSPptr, &client, recvData);
  }
  


};
