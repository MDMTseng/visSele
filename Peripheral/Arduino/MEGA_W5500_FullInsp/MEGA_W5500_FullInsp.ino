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

uint32_t logicPulseCount = 0;
#define PIPE_INFO_LEN 120
pipeLineInfo pbuff[PIPE_INFO_LEN];

//The index type uint8_t would be enough if the buffersize<255
RingBuf<typeof(*pbuff),uint8_t > RBuf(pbuff,sizeof(pbuff)/sizeof(*pbuff));

uint8_t buff[600];
IPAddress _ip(192,168,2,2);
IPAddress _gateway(169, 254, 170, 254);
IPAddress _subnet(255, 255, 255, 0);

int FAKE_GATE_PIN=31;



Websocket_FI *WS_Server;
void setup() {
  Serial.begin(115200);
  //WS_Server = new Websocket_FI(buff,sizeof(buff),_ip,5213,_gateway,_subnet);
  if(WS_Server)setRetryTimeout(3, 100);
  setup_Stepper();
  pinMode(FAKE_GATE_PIN, OUTPUT);
}

uint32_t ccc=0;

uint32_t totalLoop=0;
void loop() 
{
  if(WS_Server)
    WS_Server->loop_WS();

  totalLoop++;
  if( (totalLoop&0x1F)==0)
    loop_Stepper();
  


  volatile int ddd=0;
  for(uint32_t i=0;i!=3;i++)
  {
    
    //ddd%=3;
    uint32_t dddx=(uint32_t)4400*2;
    if(ccc++==dddx)
    {
      ccc=0;
    }
    if(ccc==0)
        digitalWrite(FAKE_GATE_PIN, HIGH);
    if(ccc==2400/3)
        digitalWrite(FAKE_GATE_PIN, LOW);
  }
  
  if( (totalLoop&0x1FFF)==0)
  {
    DEBUG_print("RBuf.size():");
    DEBUG_println(RBuf.size());
  }
  
  /*
  char tmp[40];
  for(uint32_t i=0;i<RBuf.size();i++)
  {
    pipeLineInfo* tail = RBuf.getTail(i);
    if(!tail)break;
    if(tail->sent_stage!=tail->stage)
    {
      tail->sent_stage=tail->stage;
      int len = sprintf(tmp,"{'s':%d,'m':%d,'p':%d}",tail->stage,tail->notifMark,tail->gate_pulse);
      DEBUG_println(tmp);
      if(WS_Server)
        WS_Server->SEND_ALL((uint8_t*)tmp,len,0);
      break;
    }
  }*/
}
