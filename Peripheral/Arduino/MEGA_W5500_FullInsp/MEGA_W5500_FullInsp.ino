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



enum class GEN_ERROR_CODE { 
  INSP_RESULT_HAS_NO_OBJECT=1,
  OBJECT_HAS_NO_INSP_RESULT=2,
  INSP_RESULT_COUNTER_ERROR=3,
  };



typedef struct run_mode_info{
  enum RUN_MODE{ 
    INIT,
    NORMAL,
    TEST
    } mode;

  int misc_info;
  int misc_var;
  int misc_var2;
}run_mode_info;

run_mode_info mode_info={
  mode:run_mode_info::NORMAL,
  misc_info:0
};

#define insp_status_UNSET -1654

#define insp_status_NA -334 //insp_status_NA is just for unknown insp result
#define insp_status_OK 0 //insp_status_NA is just for unknown insp result
#define insp_status_NG -1 //insp_status_NA is just for unknown insp result


typedef struct InspResCount{
  uint32_t NA;
  uint32_t OK;
  uint32_t NG;
  uint32_t ERR;
}InspResCount;
InspResCount inspResCount={0};



typedef struct pipeLineInfo{
  uint32_t gate_pulse;
  uint32_t trigger_pulse;
  int8_t stage;
  int8_t sent_stage;
  int8_t notifMark;
  int16_t insp_status;
}pipeLineInfo;

int cur_insp_counter=-1;

class Websocket_FI;
Websocket_FI *WS_Server=NULL;

#define SARRL(SARR) (sizeof((SARR))/sizeof(*(SARR)))




GEN_ERROR_CODE errorBuf[20];
RingBuf<typeof(*errorBuf),uint8_t > ERROR_HIST(errorBuf,SARRL(errorBuf));


uint32_t logicPulseCount = 0;

#define PIPE_INFO_LEN 60
pipeLineInfo pbuff[PIPE_INFO_LEN];

#define LED_PIN 13
#define CAMERA_PIN 16
#define FEEDER_PIN 14
#define BACK_LIGHT_PIN 28
#define AIR_BLOW_OK_PIN 18
#define AIR_BLOW_NG_PIN 19
#define GATE_PIN 30

#define FAKE_GATE_PIN 31


//The index type uint8_t would be enough if the buffersize<255

RingBuf<typeof(*pbuff),uint8_t > RBuf(pbuff,SARRL(pbuff));

uint8_t buff[600];//For websocket
IPAddress _ip(192,168,2,43);
IPAddress _gateway(192,168,1,1);
IPAddress _subnet(255, 255, 0, 0);




uint32_t subPulseSkipCount=16;//We don't do task processing for every hardware pulse, so we can save computing power for other things
uint32_t perRevPulseCount_HW = (uint32_t)2400*16;//the real hardware pulse count per rev
uint32_t perRevPulseCount = perRevPulseCount_HW/subPulseSkipCount;// the software pulse count that processor really care


void errorLOG(GEN_ERROR_CODE code,char* errorLog=NULL);


uint32_t PRPC= perRevPulseCount;

uint32_t tar_pulseHZ_ = perRevPulseCount_HW/30;

int offsetAir=80;
int cam_angle=103;
int angle=149;
int blowPCount=10;
uint32_t state_pulseOffset[] = 
//{
//  0,//INIT
//  PRPC*cam_angle/360, PRPC*cam_angle/360+5,  //Camera pin toggle
//
//  PRPC*cam_angle/360+10, //Waiting for inspection result
//  
//  PRPC*angle/360-blowPCount/2+offsetAir-10, //NA Exit
//  
//  PRPC*angle/360-blowPCount/2+offsetAir, PRPC*angle/360+blowPCount/2+offsetAir, //OK air blow
//  PRPC*angle/360+20-blowPCount/2+offsetAir, PRPC*angle/360+20+blowPCount/2+offsetAir};//NG air blow
{0, 654 ,657,659, 660,  697, 1450,   1475,1480,1573,1583};

int stage_action(pipeLineInfo* pli);
int stage_action(pipeLineInfo* pli)
{
  pli->notifMark = 0;
  
  static int cctest=0;

//  
//  DEBUG_print("..");
//  DEBUG_println(pli->stage);
  switch (pli->stage)
  {
    case 0:
      pli->stage++;
      pli->insp_status=insp_status_UNSET;
      if(mode_info.mode==run_mode_info::TEST)
      {
        switch(mode_info.misc_var)
        {
          case 0:
            pli->insp_status=(mode_info.misc_var2&1)?insp_status_OK:insp_status_NG;
            mode_info.misc_var2++;
          break;
          
          case 1:
            pli->insp_status=(mode_info.misc_var2&1)?insp_status_OK:insp_status_NA;
            mode_info.misc_var2++;
          break;
          
          case 2:
            pli->insp_status=(mode_info.misc_var2&1)?insp_status_NA:insp_status_NG;
            mode_info.misc_var2++;
          break;
          
          case 3:
            pli->insp_status=insp_status_OK;
          break;
          
          case 4:
            pli->insp_status=insp_status_NG;
          break;

          default:
            mode_info.misc_var=0;
        }
      
      }
      break;
    
    case 1://BackLight ON
      digitalWrite(BACK_LIGHT_PIN, 1);
      pli->stage++;
      break;
    case 2://Trigger shutter ON
      digitalWrite(CAMERA_PIN, 1);
      pli->notifMark = 1;
      pli->stage++;
      break;
    case 3://Trigger shutter OFF
      digitalWrite(CAMERA_PIN, 0);
      pli->stage++;
      break;
    case 4://BackLight OFF
      digitalWrite(BACK_LIGHT_PIN, 0);
      pli->stage=6;
      break;

    case 6://Last moment switch
    
      if(pli->insp_status==insp_status_OK)//OK
      {
        inspResCount.OK++;
        pli->stage=7;
        return 0;
      }
      if(pli->insp_status==insp_status_NG)//NG
      {
        inspResCount.NG++;
        pli->stage=9;
        return 0;
      }

      
      if(pli->insp_status==insp_status_UNSET)
      {
        
        inspResCount.ERR++;
        //Error:The inspection result isn't back
        //TODO: Send error msg and stop machine
        errorLOG(GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);
        errorAction();

      } 
      else
      {
        inspResCount.NA++;
      }
      return -1;
      

    case 7://Air Blow OK ON
      digitalWrite(AIR_BLOW_OK_PIN, 1);
      pli->stage++;
      return 0;
    case 8://Air Blow OK OFF
      digitalWrite(AIR_BLOW_OK_PIN, 0);
       //DEBUG_println("OK");
      return -1;
      
    case 9://Air Blow NG ON
      digitalWrite(AIR_BLOW_NG_PIN, 1);
      pli->stage++;
      return 0;
    case 10://Air Blow NG OFF
      digitalWrite(AIR_BLOW_NG_PIN, 0);
      //DEBUG_println("NG");
      return -1;
  }
  return 0;
}



  
int AddErrorCodesToJson(char* send_rsp, uint32_t send_rspL)
{
  
  if(ERROR_HIST.size()==0)return 0;   
  uint32_t MessageL=0;                                           
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"error_codes\":[");
  for(int i=0;i<ERROR_HIST.size();i++)
  {
    GEN_ERROR_CODE* head_code = ERROR_HIST.getTail(i);
    MessageL += sprintf( (char*)send_rsp+MessageL, "%d,",*head_code);
  }
  MessageL--;//remove the last comma',';
  MessageL += sprintf( (char*)send_rsp+MessageL, "],");
  
  return MessageL;                              
}



int AddResultCountToJson(char* send_rsp, uint32_t send_rspL,InspResCount &inspResCount)
{
  uint32_t MessageL=0;                                           
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"res_count\":{");


  MessageL += sprintf( (char*)send_rsp+MessageL, "\"OK\":%d,",inspResCount.OK);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"NG\":%d,",inspResCount.NG);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"NA\":%d,",inspResCount.NA);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":%d,",inspResCount.ERR);

  MessageL--;//remove the last comma',';
  MessageL += sprintf( (char*)send_rsp+MessageL, "},");
  
  return MessageL;                              
}

  
  
void errorAction()
{
  //if there is an error
  //clear plate
  RBuf.clear();

  //set speed to zero
  tar_pulseHZ_=0; 
}
int toggle_LED=0;
class Websocket_FI:public Websocket_FI_proto{
  public:
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_FI_proto(buff,buffL,ip,port,gateway,subnet){}

     
  int Mach_state_ToJson(char* jbuff,uint32_t jbuffL, int *ret_status)
  {
    char* send_rsp=jbuff;
    uint32_t MessageL=0;
                                                  
    MessageL += sprintf( (char*)send_rsp+MessageL, "\"state_pulseOffset\":[");
    for(int i=0;i<SARRL(state_pulseOffset);i++)
      MessageL += sprintf( (char*)send_rsp+MessageL, "%d,",state_pulseOffset[i]);
    MessageL--;//remove the last comma',';
    MessageL += sprintf( (char*)send_rsp+MessageL, "],");
    MessageL += sprintf( (char*)send_rsp+MessageL, "\"perRevPulseCount\":%d,",perRevPulseCount);
    MessageL += sprintf( (char*)send_rsp+MessageL, "\"subPulseSkipCount\":%d,",subPulseSkipCount);
    MessageL += sprintf( (char*)send_rsp+MessageL, "\"pulse_hz\":%d,",tar_pulseHZ_);

    if(ret_status)*ret_status=0;
    return MessageL;      
  }
  
  int Json_state_pulseOffset_ToMach(char* send_rsp, uint32_t send_rspL,char* jbuff,uint32_t jbuffL, int *ret_status)
  {
    char*recv_cmd = jbuff;
    uint32_t MessageL=0;
    if(ret_status)*ret_status=0;
    do{
      int table_scopeL=0;
      char * table_scope = findJsonScope((char*)recv_cmd,"\"state_pulseOffset\":",&table_scopeL);
      if(table_scope==NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"There is no 'table' in the message\",");
        if(ret_status)*ret_status=-1;
        
        break;
      }
      
      if( table_scope[0]=='[' && table_scope[table_scopeL-1]==']' )
      {
        table_scope++;
        table_scopeL-=2;
      }
  
      uint32_t new_state_pulseOffset[SARRL(state_pulseOffset)];
  
      int adv_len = ParseNumberFromArr(table_scope,new_state_pulseOffset, SARRL(state_pulseOffset));//return parsed string length
     
      if(adv_len)
      {
        ret_status=0;
        memcpy(state_pulseOffset,new_state_pulseOffset,sizeof(new_state_pulseOffset));
      }
      else
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"Table message length is not sufficient, expected length:%d\",",SARRL(state_pulseOffset));
        if(ret_status)*ret_status=-1;
      }
  
      
    }while(0);
    
    return MessageL;
  }
  
  
  int MachToJson(char* jbuff,uint32_t jbuffL, int *ret_status)
  {
    char* send_rsp=jbuff;
    uint32_t MessageL=0;
    MessageL += Mach_state_ToJson((char*)send_rsp+MessageL,jbuffL-MessageL,ret_status);
    return MessageL;
                                                  
  }
  
  int JsonToMach(char* send_rsp, uint32_t send_rspL,char* jbuff,uint32_t jbuffL, int *ret_status)
  {
    int retS=0;
    uint32_t MessageL=0;
    MessageL+=Json_state_pulseOffset_ToMach(send_rsp+MessageL,send_rspL-MessageL,jbuff,jbuffL,&retS);

    
    char numbuff[20];
    int retL = findJsonScope(jbuff,"\"pulse_hz\":",numbuff,sizeof(numbuff));
    if(retL>0){
      int newHZ=tar_pulseHZ_;
      sscanf(numbuff, "%d", &newHZ);
      tar_pulseHZ_=newHZ;
      retS=0;
    }
    
    if(ret_status)*ret_status=retS;
    return MessageL;
  }


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
    //DEBUG_println((char*)recv_cmd);

    char *buff= (char*)send_rsp+rspMaxL/2;
    int buffL = rspMaxL/2-cmdL;
    
    
    
    {
      char *idStr = buff;
      int idStrL = findJsonScope((char*)recv_cmd,"\"id\":",idStr,buffL);
      if(idStrL<0)idStr=NULL;

      buff+=idStrL+1;
      buffL-=idStrL+1;

      bool isIdAStr=false;
      if(idStr && idStr[0]=='\"' && idStr[idStrL-1]=='\"')
      {
        isIdAStr=true;
        idStr[idStrL-1]='\0';
        idStr++;
      }

      MessageL += sprintf( (char*)send_rsp+MessageL, "{");
      int ret_status=-1;
      
      if(strstr ((char*)recv_cmd,"\"type\":\"inspRep\"")!=NULL)
      {
        char *buffX=buff;
        
        if(mode_info.mode==run_mode_info::TEST)
        {
          return 0;
        }
        
        int new_count=-99;
        int pre_count=cur_insp_counter;//0~255
        char *counter_str = buffX;
        {
          int retL = findJsonScope((char*)recv_cmd,"\"count\":",counter_str,10);
          if(retL>0)
          {
            sscanf(counter_str, "%d", &new_count);

            if(new_count==-1)
            {
              pre_count=254;
              new_count=255;
            }
            
            cur_insp_counter=new_count;
            if( ((pre_count+1)&0xFF) ==new_count)
            {//PASS
              
            }
            else
            {
              counter_str=NULL;
              DEBUG_print(pre_count);
              DEBUG_print("<p  n>");
              DEBUG_println(new_count);
            }
          }
          else
          {
            counter_str=NULL; 
          }

          if(counter_str==NULL)
          {
            
            errorLOG(GEN_ERROR_CODE::INSP_RESULT_COUNTER_ERROR);
            errorAction();
          }


          
          buffX+=retL;
        }

        int insp_status=-99;
        char *statusStr = buffX;
        {
          int retL = findJsonScope((char*)recv_cmd,"\"status\":",statusStr,buffL);
          if(retL<0)statusStr=NULL;
          else{
            sscanf(statusStr, "%d", &insp_status);
            //DEBUG_print(">>status>>>>");
            //DEBUG_println(insp_status);
          }
          buffX+=retL;
        }
        
        char *time_100us_str = buffX;
        {
          int retL = findJsonScope((char*)recv_cmd,"\"time_100us\":",statusStr,buffL);
          if(retL<0)time_100us_str=NULL;
          buffX+=retL;
        }
        

        for(int i=0;i<RBuf.size();i++)
        {
          pipeLineInfo* pipe=RBuf.getTail(i);
          if(pipe==NULL)break;
//          if(pipe->stage<3)//No object in ready to select field
//          {
//            break;
//          }
          if(pipe->insp_status==insp_status_UNSET)
          {
            pipe->insp_status=insp_status;
            ret_status=0;
            break;
          }
        }
        if(ret_status)
        {
          DEBUG_print("ERROR:ret_status=");
          DEBUG_println(ret_status);
  
          errorLOG(GEN_ERROR_CODE::INSP_RESULT_HAS_NO_OBJECT);
          errorAction();
          //Error:The inspection result matches no object
          //TODO: Send error msg and stop machine
        }
        
//        digitalWrite(LED_PIN, toggle_LED);
//        toggle_LED=!toggle_LED;
        
        return 0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"get_dev_info\"")!=NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, 
          "\"type\":\"dev_info\","
          "\"info\":{"
          "\"type\":\"uFullInsp\","
          "\"ver\":\"0.0.0.0\","
          "\"pulse_hz\":%d"
          "},",tar_pulseHZ_);
        ret_status=0;
      }      
      else if(strstr ((char*)recv_cmd,"\"type\":\"PING\"")!=NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL,"\"type\":\"PONG\",");
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, buffL-MessageL);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, buffL-MessageL,inspResCount);
        ret_status=0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"set_pulse_hz\"")!=NULL)
      {
        char *bufPtr = buff;
        int retL = findJsonScope((char*)recv_cmd,"\"pulse_hz\":",bufPtr,buffL);
        if(retL>0){
          int newHZ=tar_pulseHZ_;
          sscanf(bufPtr, "%d", &newHZ);
          tar_pulseHZ_=newHZ;
          ret_status=0;
        }
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"get_setup\"")!=NULL)
      {
        int ret_st=0;
        MessageL += sprintf( (char*)send_rsp+MessageL,"\"type\":\"get_setup_rsp\","
                                                      "\"ver\":\"0.0.0.0\",");
        MessageL+=MachToJson(send_rsp+MessageL, send_rsp-MessageL, &ret_st);
        ret_status = ret_st;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"set_setup\"")!=NULL)
      {
        int ret_st=0;
        MessageL+=JsonToMach(send_rsp+MessageL, send_rsp-MessageL,recv_cmd,cmdL, &ret_st);
        ret_status = ret_st;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"error_get\"")!=NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"error_info\",",idStr);
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, buffL-MessageL);
        ret_status = 0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"res_count_get\"")!=NULL)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"res_count\",",idStr);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, buffL-MessageL,inspResCount);
        ret_status = 0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"res_count_clear\"")!=NULL)
      {
        memset(&inspResCount,0,sizeof(inspResCount));
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"res_count\",",idStr);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, buffL-MessageL,inspResCount);
        ret_status = 0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"error_clear\"")!=NULL)
      {
        ERROR_HIST.clear();
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"error_info\",",idStr);
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, buffL-MessageL);
        ret_status = 0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"mode_set\"")!=NULL)
      {
        if(strstr ((char*)recv_cmd,"\"mode\":\"NORMAL\""))
        {
          mode_info.mode=run_mode_info::NORMAL;
          ret_status = 0;
        }
        if(strstr ((char*)recv_cmd,"\"mode\":\"TEST\""))
        {
          if(mode_info.mode!=run_mode_info::TEST)
          {
            mode_info.mode=run_mode_info::TEST;
            mode_info.misc_var=0;
          }
          else
          {
            mode_info.misc_var++;
          }
          ret_status = 0;
        }
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"MISC/BACK_LIGHT/ON\"")!=NULL)
      {
        digitalWrite(BACK_LIGHT_PIN,1);
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"MISC/BACK_LIGHT/OFF\"")!=NULL)
      {
        digitalWrite(BACK_LIGHT_PIN,0);
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"MISC/CAM_TRIGGER\"")!=NULL)
      {
        digitalWrite(CAMERA_PIN,1);
        delay(10);
        digitalWrite(CAMERA_PIN,0);
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"MISC/OK_BLOW\"")!=NULL)
      {
        digitalWrite(AIR_BLOW_OK_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_OK_PIN, 0);
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"MISC/NG_BLOW\"")!=NULL)
      {
        digitalWrite(AIR_BLOW_NG_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
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
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"pulse_hz\":%d,",tar_pulseHZ_);
        
        ret_status=0;
      }
      else if(strstr ((char*)recv_cmd,"\"type\":\"set_pulse_offset_info\"")!=NULL)
      {
        ret_status=-1;
        do{
          int table_scopeL=0;
          char * table_scope = findJsonScope((char*)recv_cmd,"\"table\":",&table_scopeL);
          if(table_scope==NULL)
          {
            MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"There is no 'table' in the message\",");
            break;
          }
          
          if( table_scope[0]=='[' && table_scope[table_scopeL-1]==']' )
          {
            table_scope++;
            table_scopeL-=2;
          }

          uint32_t new_state_pulseOffset[SARRL(state_pulseOffset)];

          int adv_len = ParseNumberFromArr(table_scope,new_state_pulseOffset, SARRL(state_pulseOffset));//return parsed string length
         
          if(adv_len)
          {
            ret_status=0;
            memcpy(state_pulseOffset,new_state_pulseOffset,sizeof(new_state_pulseOffset));
          }
          else
          {
            MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"Table message length is not sufficient, expected length:%d\",",SARRL(state_pulseOffset));
          }

          
        }while(0);
      }


      if(idStr)
      {
        if(isIdAStr)
        {
          MessageL += sprintf( (char*)send_rsp+MessageL, "\"id\":\"%s\",",idStr);
        }
        else
        {
          MessageL += sprintf( (char*)send_rsp+MessageL, "\"id\":%s,",idStr);
        }
      }
        
      MessageL += sprintf( (char*)send_rsp+MessageL, "\"st\":%d,",ret_status);
      if(send_rsp[MessageL-1]== ',')
        MessageL--;
      MessageL += sprintf( (char*)send_rsp+MessageL, "}");
      
    }
//    
//    DEBUG_print("MessageL:");
//    DEBUG_println(MessageL);
//    DEBUG_println("send_rsp:");
//    DEBUG_println((char*)send_rsp);
//    
    return MessageL;
  }

};


void errorLOG(GEN_ERROR_CODE code,char* errorLog)
{
  GEN_ERROR_CODE* head_code = ERROR_HIST.getHead();
  if (head_code != NULL)
  {
    *head_code=code;
    ERROR_HIST.pushHead();
  }

  {
    uint8_t errBuff[100];
    uint8_t errBuffL=0;
    errBuffL+=sprintf(errBuff+errBuffL,"{");
    
    errBuffL += sprintf( errBuff+errBuffL,"\"type\":\"error_notification\",");
    errBuffL+=AddErrorCodesToJson(errBuff+errBuffL, 20);
    if(errorLog)
      errBuffL+= sprintf(errBuff+errBuffL,"\"log\":\"%s\",",errorLog);
  
    errBuffL--;//remove the last ','
    errBuffL+=sprintf(errBuff+errBuffL,"}");//give it an close
    WS_Server->SEND_ALL(errBuff,errBuffL,0);
  }
}



uint32_t pulseHZ_step = 50;

void setup() {
  Serial.begin(115200);
  WS_Server = new Websocket_FI(buff,sizeof(buff),_ip,5213,_gateway,_subnet);
  if(WS_Server)setRetryTimeout(3, 100);
  setup_Stepper();
  pinMode(FAKE_GATE_PIN, OUTPUT);
  
  pinMode(CAMERA_PIN, OUTPUT);
  pinMode(AIR_BLOW_OK_PIN, OUTPUT);
  pinMode(AIR_BLOW_NG_PIN, OUTPUT);
  pinMode(BACK_LIGHT_PIN, OUTPUT);
  pinMode(GATE_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(FEEDER_PIN, OUTPUT);

  digitalWrite(FEEDER_PIN, HIGH);

  
}

uint32_t ccc=0;

uint32_t totalLoop=0;

int emptyPlateCount=0;

uint32_t feederX=0;
void loop() 
{
  if(WS_Server)
    WS_Server->loop_WS();

  totalLoop++;

//  if(feederX<800)
//  {
//    digitalWrite(FEEDER_PIN, LOW);
//  }
//  else
//  {
//    digitalWrite(FEEDER_PIN, HIGH);
//  }
//  feederX=(feederX>20000)?0:feederX+1;
  
  if( (totalLoop&0xF)==0)
  {
    uint32_t tar=tar_pulseHZ_;
    if(0&&emptyPlateCount>14)
      tar/=5;
     
    
    uint32_t cur = loop_Stepper(tar,pulseHZ_step);
    if(cur*20<perRevPulseCount_HW)
    {
      digitalWrite(FEEDER_PIN, LOW);
    }
    else
    {
      digitalWrite(FEEDER_PIN, HIGH);
    }
  }

  if( (totalLoop&0x7FFF)==0)
  {
    DEBUG_print("RBuf:");
    DEBUG_println(RBuf.size());

    
    if(ERROR_HIST.size()!=0)
    {
      DEBUG_print("Error:");
      DEBUG_println(ERROR_HIST.size());
    }

    if(RBuf.size()==0)
    {
      emptyPlateCount++;
    }
    else
    {
      emptyPlateCount=0;
    }
  }
  
  if(totalLoop<0xFFFF)
  {
    return;
  }
  

  if(ERROR_HIST.size()!=0)//If there is at leaset an error, do errorAction.
  {
    errorAction();
  }

  
  for(uint32_t i=0;i!=1;i++)
  {

    //ddd%=3;
    uint32_t dddx=(uint32_t)4400*2/8*10;
    if(ccc++==dddx)
    {
      ccc=0;
    }
    if(ccc==0)
        digitalWrite(FAKE_GATE_PIN, HIGH);
    if(ccc==dddx/2)
        digitalWrite(FAKE_GATE_PIN, LOW);
  }
  
//  char tmp[40];
//  for(uint32_t i=0;i<RBuf.size();i++)
//  {
//    pipeLineInfo* tail = RBuf.getTail(i);
//    if(!tail)break;
//    if(tail->sent_stage!=tail->stage && tail->stage>0)
//    {
//      tail->sent_stage=tail->stage;
//      int len = sprintf(tmp,"{'type':'noti_obj_info','s':%d,'m':%d,'p':%d}",tail->stage,tail->notifMark,tail->gate_pulse);
//      DEBUG_println(tmp);
//      if(WS_Server)
//        WS_Server->SEND_ALL((uint8_t*)tmp,len,0);
//      break;
//    }
//  }
}
