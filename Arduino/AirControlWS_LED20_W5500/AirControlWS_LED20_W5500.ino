/*
  Websocket Server Protocol

  This example demostrate a simple echo server.
  It demostrate how the library <WebSocketProtocol.h> works
  and how to handle the state changes.

  dependent library:WIZNET <Ethernet.h>,ETH_Extra.h

  created  14 Feb 2015
  by MDM Tseng
*/

#include <SPI.h>
#define private public //dirty trick
#include <Ethernet2.h>
#undef private
#include <WebSocketProtocol.h>
//#include "utility/w5100.h"
//#include "utility/socket.h"
//#include <ETH_Extra.h>

#define DEBUG_
#ifdef DEBUG_
#define DEBUG_print(A, ...) Serial.print(A,##__VA_ARGS__)
#define DEBUG_println(A, ...) Serial.println(A,##__VA_ARGS__)
#else
#define DEBUG_print(A, ...)
#define DEBUG_println(A, ...)
#endif

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192,168,2,2);
IPAddress gateway(169, 254, 170, 254);
IPAddress subnet(255, 255, 255, 0);

EthernetServer server(5213);
#define MAX_WSP_CLIENTs 2
WebSocketProtocol WSP[MAX_WSP_CLIENTs];

char buff[600];
char *buffiter;

char *retPackage = buff;

WebSocketProtocol::WPFrameInfo retframeInfo;//={.opcode = 1, .isMasking = 0, .isFinal = 1 };
int LEFTPIN = 2;
int RIGHTPIN = 3;
void setup() {
  setupLED();
  pinMode(LEFTPIN,OUTPUT);
  pinMode(RIGHTPIN,OUTPUT);
  Ethernet.begin(mac, ip, gateway, subnet);
  server.begin();
  Serial.begin(57600);
  DEBUG_print("HYVisionServer address:");
  DEBUG_println(Ethernet.localIP());
  setRetryTimeout(4, 1000);
}

unsigned int counter2Pin = 0;
byte LiveClient = 0;
boolean firstTime=true;
boolean LEFT_ACT = false;
boolean RIGHT_ACT = false;
boolean TEST_ACT=false;
boolean TEST_ACT_TOGGLE=false;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
int airTime=10;
//int LED_Time=33;
//unsigned long LED_previousMillis = 0;
static uint8_t led_hue=0;
static uint8_t led_value=60;

//void timerLED(){
//  currentMillis = millis();
//  if (currentMillis - LED_previousMillis >= LED_Time) {
//    LED_previousMillis = currentMillis;
//  }  
//}

float valueApproach(float nowVal, float destVal, float speed) { 
  return (float) ((nowVal * (1.0 - speed)) + (destVal * speed));
}
void timerX() {
  currentMillis = millis();
  
  if (previousMillis == 0) {
    previousMillis = currentMillis;
    digitalWrite(LEFTPIN, LEFT_ACT);
    digitalWrite(RIGHTPIN, RIGHT_ACT);
    //DEBUG_print(LEFT_ACT);
    //DEBUG_print(RIGHT_ACT);
  }else if (currentMillis - previousMillis >= airTime) {
    previousMillis = currentMillis;
    //    PORTB ^= _BV(PB5);
    if(TEST_ACT){
      TEST_ACT_TOGGLE=!TEST_ACT_TOGGLE;
      LEFT_ACT = !TEST_ACT_TOGGLE;
      RIGHT_ACT = TEST_ACT_TOGGLE;
      digitalWrite(LEFTPIN, LEFT_ACT);
      digitalWrite(RIGHTPIN, RIGHT_ACT);
    }else{
      LEFT_ACT = false;
      RIGHT_ACT = false;
      digitalWrite(LEFTPIN, LEFT_ACT);
      digitalWrite(RIGHTPIN, RIGHT_ACT);
      //DEBUG_print(LEFT_ACT);
      //DEBUG_print(RIGHT_ACT);
    }
  }
}


int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
{
  unsigned int MessageL = 0; //echo
  if (strcmp(recv_cmd, "/cue/LEFT") == 0) {
    MessageL = sprintf(send_rsp, "[O]RECV:/cue/LEFT");
    LEFT_ACT = true;
    led_hue=0;
    led_value=60;
    previousMillis = 0;
  }
  else if (strcmp(recv_cmd, "/cue/RIGHT") == 0) {
    MessageL = sprintf(send_rsp, "[O]RECV:/cue/RIGHT");
    RIGHT_ACT = true;
    led_hue=96;
    led_value=60;
    previousMillis = 0;
  }else if (strcmp(recv_cmd, "/cue/TEST") == 0) {
    MessageL = sprintf(send_rsp, "[O]RECV:/cue/TEST");
    TEST_ACT = !TEST_ACT;
    previousMillis = 0;
  }else if (strncmp(recv_cmd, "/cue/TIME/",9) == 0) {
    char *airTimeStr = recv_cmd+10;
    char tmp[10];
    strncpy(tmp,airTimeStr,10);
    int t=(int)atoi(tmp);//1690byte
    if(t>0&&t<9999)
      airTime=t;
    MessageL = sprintf(send_rsp,"airTime:%d,t=%d,s=%s", airTime,t,tmp);
    led_hue=160;
    led_value=60;
  }
  
  if (MessageL == 0)
  {
    char *tmpX = send_rsp+200;
    
    strcpy(tmpX,recv_cmd);
    MessageL = sprintf(send_rsp, "UNKNOWN:%s",tmpX);
  }

  return MessageL;
}


void RECVWebSocketPkg(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
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
  unsigned int MessageL = CMDExec(RECVData, RECVDataL,dataBuff,sizeof(buff)-8);
  unsigned int totalPackageL;
  char* retPkg = WProt->codeFrame(dataBuff, MessageL, &retframeInfo, &totalPackageL); //get complete package might have some shift compare to "retPackage"
  WProt->getClientOBJ().write(retPkg, totalPackageL);
}


void RECVWebSocketPkg_binary(WebSocketProtocol* WProt, EthernetClient* client, char* RECVData)
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

void loop() {
  // wait for a new client:
  timerX();
loopLED();
  
  EthernetClient client = server.available();
  if (!client)
  {
    if (LiveClient)
    {

      if (counter2Pin++ > 1000)//check client still alive periodically
      {
        PingAllClient();
        clearUnreachableClient();
        counter2Pin = 0;
      }
      delay(10 >> LiveClient);
    }
    else
      delay(1);
    return;
  }


  buffiter = buff;
  unsigned int  KL = 0;
  unsigned int PkgL =  client.available();
  KL = PkgL;
  recv(client._sock, (uint8_t*)buffiter, PkgL);//get raw data
  WebSocketProtocol* WSPptr  = findFromProt(client);
  DEBUG_println(">>>>");
  if (WSPptr == null)
  {
    client.stop();
    return;
  }
  client = WSPptr->getClientOBJ();
  char *recvData = WSPptr->processRecvPkg(buff, KL);//Check/process is the websocket PKG
  DEBUG_println(">>>>");
  byte frameL = WSPptr->getPkgframeInfo().length; //get frame
  
  DEBUG_print(">>>>WSPptr->getState() ");
  DEBUG_print(WSPptr->getState() );
  DEBUG_print(" OP:");
  DEBUG_println(WSPptr->getRecvOPState() );
  if (WSPptr->getState() == WS_HANDSHAKE)//On hand shaking
  {
    DEBUG_print("WS_HANDSHAKE::");
    DEBUG_println(client._sock);
    client.print(buff);
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
    
    int retL=  CMDExec(buff, KL,buff,sizeof(buff));
    WSPptr->getClientOBJ().write(buff, retL);
    return;
  }




  
  if (WSPptr->getRecvOPState() == WSOP_CLOSE)//websocket close
  {

    DEBUG_print("Normal close::");
    DEBUG_println(client._sock);
    client.stop();
    WSPptr->rmClientOBJ();
    return;
  }
  // Normal websocket section
  // client::WSPptr
  // recv Data::recvData
  RECVWebSocketPkg(WSPptr, &client, recvData);
  
}
void clearUnreachableClient()
{
  LiveClient = 0;
  for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
  {
    EthernetClient Rc = WSP[i].getClientOBJ();
    if (Rc && Rc.status() == 0x00)
    {
      DEBUG_print("clear timeout sock::");
      DEBUG_println(Rc._sock);
      Rc.stop();
      WSP[i].rmClientOBJ();

    }
    else if (Rc)
      LiveClient++;
  }
}
void PingAllClient()
{
  for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    if (WSP[i].getClientOBJ())
    {
      //byte SnIR = ReadSn_IR(WSP[i].getClientOBJ()._sock);

      testAlive(WSP[i].getClientOBJ()._sock);
    }
}
byte countConnected()
{
  byte C = 0;
  for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
    if (WSP[i].getClientOBJ())
      C++;
  return C;
}

WebSocketProtocol* findFromProt(EthernetClient client)
{

  LiveClient = 0;
  for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
  {
    EthernetClient Rc = WSP[i].getClientOBJ();
    if (Rc == client)
      return WSP + i;
  }

  for (byte i = 0; i < MAX_WSP_CLIENTs; i++)
  {
    if (!WSP[i].getClientOBJ())
    {

      WSP[i].setClientOBJ(client);

      LiveClient = i;
      byte ii = i;
      for (; ii < MAX_WSP_CLIENTs; ii++)if (WSP[ii].getClientOBJ())LiveClient++;

      DEBUG_print("new socket:::");
      DEBUG_print(client._sock);
      DEBUG_print('/');
      DEBUG_println(LiveClient);
      // OnClientsChange();
      return WSP + i;
    }
  }
  return null;
}

