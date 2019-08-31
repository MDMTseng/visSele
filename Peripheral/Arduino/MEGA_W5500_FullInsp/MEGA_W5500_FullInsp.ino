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




#define SARRL(SARR) (sizeof((SARR))/sizeof(*(SARR)))

uint32_t logicPulseCount = 0;

#define PIPE_INFO_LEN 120
pipeLineInfo pbuff[PIPE_INFO_LEN];

//The index type uint8_t would be enough if the buffersize<255

RingBuf<typeof(*pbuff),uint8_t > RBuf(pbuff,SARRL(pbuff));

uint8_t buff[600];
IPAddress _ip(192,168,2,2);
IPAddress _gateway(169, 254, 170, 254);
IPAddress _subnet(255, 255, 255, 0);

int FAKE_GATE_PIN=31;



uint32_t subPulseSkipCount=16;//We don't do task processing for every hardware pulse, so we can save computing power for other things
uint32_t perRevPulseCount_HW = (uint32_t)2400*16;//the real hardware pulse count per rev
uint32_t perRevPulseCount = perRevPulseCount_HW/subPulseSkipCount;// the software pulse count that processor really care


uint32_t PRPC= perRevPulseCount;
int offsetAir=80;


int cam_angle=103;
int angle=149;
int blowPCount=10;
uint32_t state_pulseOffset[] = {
  0,
  PRPC*cam_angle/360, PRPC*cam_angle/360+5, 

  
  PRPC*angle/360-blowPCount/2+offsetAir-10, 
  
  PRPC*angle/360-blowPCount/2+offsetAir, PRPC*angle/360+blowPCount/2+offsetAir, 
  PRPC*angle/360+20-blowPCount/2+offsetAir, PRPC*angle/360+20+blowPCount/2+offsetAir};


class Websocket_FI:public Websocket_FI_proto{
  public:
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_FI_proto(buff,buffL,ip,port,gateway,subnet){}

 
  int CMDExec(uint8_t *recv_cmd, int cmdL,uint8_t *send_rsp,int rspMaxL)
  {
    if(cmdL==NULL)
    {
      return 0;
    }

    unsigned int MessageL = 0; //echo
    recv_cmd[cmdL]='\0';
    uint8_t *offset_cmd=send_rsp+(rspMaxL-cmdL);
    memcpy(offset_cmd,recv_cmd,cmdL);
    recv_cmd = offset_cmd;
    rspMaxL-=cmdL;


    char *buff= (char*)send_rsp+rspMaxL/2;
    int buffL = rspMaxL/2-cmdL;
    

    
    {
      char *idStr = buff;
      int idStrL = findJsonScope((char*)recv_cmd,"\"id\":",idStr,buffL);
      if(idStrL<0)idStr=NULL;

      buff+=idStrL+1;
      buffL-=idStrL+1;

      if(idStr && idStr[0]=='\"' && idStr[idStrL-1]=='\"')
      {
        idStr[idStrL-1]='\0';
        idStr++;
      }

      MessageL += sprintf( (char*)send_rsp+MessageL, "{");
      int ret_status=-1;
      
      if(strstr ((char*)recv_cmd,"\"type\":\"inspRep\"")!=NULL)
      {
        char *buffX=buff;
        char *statusStr = buffX;
        int retL = findJsonScope((char*)recv_cmd,"\"status\":",statusStr,buffL);
        if(retL<0)statusStr=NULL;
        buffX+=retL;
        
        ret_status=0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"get_pulse_offset_info\"")!=NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"pulse_offset_info\",",idStr);
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"table\":[");
        for(int i=0;i<SARRL(state_pulseOffset);i++)
          MessageL += sprintf( (char*)send_rsp+MessageL, "%d,",state_pulseOffset[i]);
        MessageL--;//remove last ','
        MessageL += sprintf( (char*)send_rsp+MessageL, "],");
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"perRevPulseCount\":%d,",perRevPulseCount);
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"subPulseSkipCount\":%d,",subPulseSkipCount);
        
        ret_status=0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"set_pulse_offset_info\"")!=NULL)
      {
        ret_status=-1;
        do{
          int table_scopeL=0;
          char * table_scope = findJsonScope((char*)recv_cmd,"\"table\":",&table_scopeL);
          if(table_scope==NULL)break;
          
          if( table_scope[0]=='[' && table_scope[table_scopeL-1]==']' )
          {
            table_scope++;
            table_scopeL-=2;
          }

          char *table_str=table_scope;
          uint32_t new_state_pulseOffset[SARRL(state_pulseOffset)];
          
          DEBUG_print(">>>");
          DEBUG_println(table_scope);
          for(int i=0;i<SARRL(new_state_pulseOffset);i++)
          {
            int ptr_adv = popNumberFromArr(table_str,&(new_state_pulseOffset[i]));
            
            DEBUG_print(">>ptr_adv>:");
            DEBUG_println(ptr_adv);
            if(ptr_adv==0)
            {
              table_str=NULL;
              break;
            }
            else
            {
              table_str+=ptr_adv+1;
            }
          }

          if(table_str)
          {
            DEBUG_print(">>new_state_pulseOffset[3]>:");
            DEBUG_println(new_state_pulseOffset[3]);
            ret_status=0;
            memcpy(state_pulseOffset,new_state_pulseOffset,sizeof(new_state_pulseOffset));
          }
          
          
        }while(0);
      }
      
      if(idStr)
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"id\":\"%s\",",idStr);
        
      MessageL += sprintf( (char*)send_rsp+MessageL, "\"st\":%d,",ret_status);
      if(send_rsp[MessageL-1]== ',')
        MessageL--;
      MessageL += sprintf( (char*)send_rsp+MessageL, "}");
      
    }
    
    
    return MessageL;
  }

};

Websocket_FI *WS_Server;
void setup() {
  Serial.begin(115200);
  WS_Server = new Websocket_FI(buff,sizeof(buff),_ip,5213,_gateway,_subnet);
  if(WS_Server)setRetryTimeout(3, 100);
  setup_Stepper();
  pinMode(FAKE_GATE_PIN, OUTPUT);
}

uint32_t ccc=0;

uint32_t totalLoop=0;

int emptyPlateCount=0;
uint32_t tar_pulseHZ_ = perRevPulseCount_HW/4;
void loop() 
{
  if(WS_Server)
    WS_Server->loop_WS();

  totalLoop++;
  
  if( (totalLoop&0x1F)==0)
  {
    if(emptyPlateCount>7)
      loop_Stepper(tar_pulseHZ_/20);
    else
      loop_Stepper(tar_pulseHZ_);
  }
    
  if( (totalLoop&0x1FFF)==0)
  {
    DEBUG_print("RBuf.size():");
    DEBUG_println(RBuf.size());
    if(RBuf.size()==0)
    {
      emptyPlateCount++;
    }
    else
    {
      emptyPlateCount=0;
    }
  }

//  if(totalLoop<0xFFFF)
//  {
//    return;
//  }
//
//  volatile int ddd=0;
//  for(uint32_t i=0;i!=1;i++)
//  {
//    
//    //ddd%=3;
//    uint32_t dddx=(uint32_t)4400*2/8;
//    if(ccc++==dddx)
//    {
//      ccc=0;
//    }
//    if(ccc==0)
//        digitalWrite(FAKE_GATE_PIN, HIGH);
//    if(ccc==dddx/2)
//        digitalWrite(FAKE_GATE_PIN, LOW);
//  }
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
