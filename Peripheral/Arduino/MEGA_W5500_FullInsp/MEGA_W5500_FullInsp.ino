/*
  Websocket Server Protocol

  This example demostrate a simple echo server.
  It demostrate how the library <WebSocketProtocol.h> works
  and how to handle the state changes.

  dependent library:WIZNET <Ethernet.h>,ETH_Extra.h

  created  14 Feb 2015
  by MDM Tseng
*/
#define DEBUG_
#include "include/Websocket.hpp"
#include "websocket_FI.hpp"
#include "include/RingBuf.hpp"

typedef struct pipeLineInfo{
  uint32_t gate_pulse;
  uint32_t trigger_pulse;
  int8_t stage;
  int8_t sent_stage;
  int8_t notifMark;
}pipeLineInfo;

#define PIPE_INFO_LEN 50
pipeLineInfo pbuff[PIPE_INFO_LEN];

//The index type uint8_t would be enough if the buffersize<255
RingBuf<typeof(*pbuff),uint8_t > RBuf(pbuff,sizeof(pbuff)/sizeof(*pbuff));

uint8_t buff[600];
IPAddress _ip(192,168,2,2);
IPAddress _gateway(169, 254, 170, 254);
IPAddress _subnet(255, 255, 255, 0);

Websocket_FI *WS_Server;
void setup() {
  Serial.begin(57600);
  WS_Server = new Websocket_FI(buff,sizeof(buff),_ip,5213,_gateway,_subnet);
  setRetryTimeout(3, 100);
  setup_Stepper();
}


uint32_t ccc=0;
void loop() 
{
  RBuf.size();
  WS_Server->loop_WS();
  loop_Stepper();
  
  char tmp[40];
  for(uint32_t i=0;i<RBuf.size();i++)
  {
    pipeLineInfo* tail = RBuf.getTail(i);
    if(!tail)break;
    if(tail->sent_stage!=tail->stage)
    {
      tail->sent_stage=tail->stage;
      int len = sprintf(tmp,"{'s':%d,'m':%d,'p':%d}",tail->stage,tail->notifMark,tail->gate_pulse);
      WS_Server->SEND_ALL((uint8_t*)tmp,len,0);
      break;
    }
  }
}
