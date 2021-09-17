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
#include <avr/wdt.h>

//#define TEST_MODE 
uint16_t TCount=0;
uint16_t CCount=0;
uint16_t ExeUpdateCount=0;
uint16_t PassCount=0;
uint16_t stageUpdated=0;
uint16_t __newDist=0;
uint16_t __exDist=0;

void(* resetFunc) (void) = 0;
enum class GEN_ERROR_CODE { 
  RESET=0,
  INSP_RESULT_HAS_NO_OBJECT=1,
  OBJECT_HAS_NO_INSP_RESULT=2,
  INSP_RESULT_COUNTER_ERROR=3,
  };


enum class ERROR_ACTION_TYPE { 
  NOP=0,
  ALL_STOP=1,
  FREE_SPIN_2_REV=2,
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



uint32_t thres_skip_counter=0;
#define insp_status_UNSET -2000
#define insp_status_DEL 0xFE

#define insp_status_NA -128 //insp_status_NA is just for unknown insp result
#define insp_status_OK 0 //insp_status_NA is just for unknown insp result
#define insp_status_NG -1 //insp_status_NA is just for unknown insp result


typedef struct InspResCount{
  uint64_t NA;
  uint64_t OK;
  uint64_t NG;
  uint64_t ERR;
}InspResCount;
InspResCount inspResCount={0};



int cur_insp_counter=-1;

class Websocket_FI;
Websocket_FI *WS_Server=NULL;

#define SARRL(SARR) (sizeof((SARR))/sizeof(*(SARR)))




GEN_ERROR_CODE errorBuf[20];
RingBuf<typeof(*errorBuf),uint8_t > ERROR_HIST(errorBuf,SARRL(errorBuf));




uint32_t logicPulseCount = 0;

#define PIPE_INFO_LEN 30

#define LED_PIN 13
#define CAMERA_PIN 16
#define FEEDER_PIN 14
#define BACK_LIGHT_PIN 28
#define AIR_BLOW_OK_PIN 24
#define AIR_BLOW_NG_PIN 25
#define GATE_PIN 30

#define FAKE_GATE_PIN 31
//in libraries/Ethernet/src/utility/w5100.h CS pin is pin 10

//The index type uint8_t would be enough if the buffersize<255

RingBuf_Static<pipeLineInfo,PIPE_INFO_LEN,uint8_t > RBuf;

IPAddress _ip(192,168,2,43);
IPAddress _gateway(192,168,1,1);
IPAddress _subnet(255, 255, 0, 0);
int _port = 5213;


const uint32_t subPulseSkipCount=16;//We don't do task processing for every hardware pulse, so we can save computing power for other things
const uint32_t perRevPulseCount_HW = (uint32_t)2400*16;//the real hardware pulse count per rev
const uint32_t perRevPulseCount = perRevPulseCount_HW/subPulseSkipCount;// the software pulse count that processor really care


void errorLOG(GEN_ERROR_CODE code,char* errorLog=NULL);


uint32_t PRPC= perRevPulseCount;

uint32_t tar_pulseHZ_ = perRevPulseCount_HW/4; 

uint8_t g_max_frame_rate=40;

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
{0, 654 ,657,659, 660,  697, 750,  900,910,1073,1083};


struct ACT_INFO{
  pipeLineInfo* src;
  int info;
  uint32_t targetPulse;
};


struct ACT_SCH{
  RingBuf_Static<ACT_INFO,PIPE_INFO_LEN > 
    ACT_BACKLight1H,
    ACT_BACKLight1L,
    ACT_CAM1,
    ACT_SWITCH,
    ACT_SEL1H,
    ACT_SEL1L,
    ACT_SEL2H,
    ACT_SEL2L;
};

ACT_SCH act_S;

#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info,cusCode_task) \
        {\
          ACT_INFO *task;\
          task=(rb).getHead();\
          if(task){\
            task->targetPulse=mod_sim(plinfo->gate_pulse + pulseOffset,perRevPulseCount);\
            task->src=plinfo;\
            task->info=_info;\
            cusCode_task \
            (rb).pushHead();\
          }\
        }


int skip_pulse_show(pipeLineInfo* pli)
{

    ACT_PUSH_TASK (act_S.ACT_BACKLight1H, pli, state_pulseOffset[1],1,
    );
    ACT_PUSH_TASK (act_S.ACT_BACKLight1L, pli, state_pulseOffset[1]+1, 2,
    );
}
int ActRegister_pipeLineInfo(pipeLineInfo* pli)
{
  if(mode_info.mode==run_mode_info::TEST)
  {
    switch(mode_info.misc_var)
    {

      
      case 0:
        pli->insp_status=insp_status_NA;
      break;
      
      case 1:
        pli->insp_status=(mode_info.misc_var2&1)?insp_status_OK:insp_status_NG;
      break;
      
      case 2:
        pli->insp_status=(mode_info.misc_var2&1)?insp_status_OK:insp_status_NA;
      break;
      
      case 3:
        pli->insp_status=insp_status_OK;
      break;
      
      case 4:
        pli->insp_status=(mode_info.misc_var2&1)?insp_status_NG:insp_status_NA;
      break;
      
      case 5:
        pli->insp_status=insp_status_NG;
      break;
      
      default:
        mode_info.misc_var=0;
    }
  
    mode_info.misc_var2++;
  }





  if(act_S.ACT_BACKLight1H.size_left()>=1 &&act_S.ACT_BACKLight1L.size_left()>=1 && 
  act_S.ACT_CAM1.size_left()>=2 && act_S.ACT_SWITCH.size_left()>=1)
  {
    // DEBUG_printf(">>>>src:%p gate_pulse:%d ",pli,pli->gate_pulse);
    // DEBUG_printf("s:%d ",pli->s_pulse);
    // DEBUG_printf("e:%d ",pli->e_pulse);
    // DEBUG_printf("cur:%d\n",logicPulseCount);

    ACT_PUSH_TASK (act_S.ACT_BACKLight1H, pli, state_pulseOffset[1],1,
    );
    ACT_PUSH_TASK (act_S.ACT_BACKLight1L, pli, state_pulseOffset[4], 2,
    );

    ACT_PUSH_TASK (act_S.ACT_CAM1, pli, state_pulseOffset[2], 1,
    );
    ACT_PUSH_TASK (act_S.ACT_CAM1, pli, state_pulseOffset[3], 2,
    );

    ACT_PUSH_TASK (act_S.ACT_SWITCH, pli, state_pulseOffset[5], 2,
    );


    return 0;
    // pli->insp_status=insp_status_OK;
  }
  return -1;
}



#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \
        {\
           ACT_INFO *task=act_rb.getTail();\
          if(task && task->targetPulse==cur_pulse)\
          {\
            cmd_task\
            act_rb.consumeTail();\
          }\
        }

int Run_ACTS(ACT_SCH *acts,uint32_t cur_pulse)
{
  // static uint32_t pre_pulse=0;

  // uint32_t diff = cur_pulse-pre_pulse;
  // if(diff!=1)
  // {
  //   DEBUG_printf("pre_pulse:%d ",pre_pulse);
  //   DEBUG_printf("cur_pulse:%d \n",cur_pulse);
  // }
  // pre_pulse=cur_pulse;
    
  ACT_TRY_RUN_TASK(acts->ACT_BACKLight1H, cur_pulse, 

      // DEBUG_printf("BL1 src:%p tp:%d ",task->src,task->targetPulse);
      // DEBUG_printf("info:%d\n",task->info);

    digitalWrite(BACK_LIGHT_PIN, 1);
  );

  ACT_TRY_RUN_TASK(acts->ACT_BACKLight1L, cur_pulse, 

      // DEBUG_printf("BL1 src:%p tp:%d ",task->src,task->targetPulse);
      // DEBUG_printf("info:%d\n",task->info);

    digitalWrite(BACK_LIGHT_PIN, 0);
  );


  ACT_TRY_RUN_TASK(acts->ACT_CAM1, cur_pulse, 

    // 
    if(task->info==1)
    {
      // DEBUG_printf("CAM1 s:%p tp:%d\n",task->src,task->targetPulse);
      digitalWrite(CAMERA_PIN, 1);
    }
    else if(task->info==2)
    {
      digitalWrite(CAMERA_PIN, 0);
    }
  );


  ACT_TRY_RUN_TASK(acts->ACT_SEL1H, cur_pulse, 

    // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

    digitalWrite(AIR_BLOW_OK_PIN, 1);
  );
  
  ACT_TRY_RUN_TASK(acts->ACT_SEL1L, cur_pulse, 

    // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

    digitalWrite(AIR_BLOW_OK_PIN, 0);
  );

  
  ACT_TRY_RUN_TASK(acts->ACT_SEL2H, cur_pulse, 

    // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

    digitalWrite(AIR_BLOW_NG_PIN, 1);
  );
  
  ACT_TRY_RUN_TASK(acts->ACT_SEL2L, cur_pulse, 

    // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

    digitalWrite(AIR_BLOW_NG_PIN, 0);
  );

  ACT_TRY_RUN_TASK(acts->ACT_SWITCH, cur_pulse, 
    
    // DEBUG_printf("SW src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);


    pipeLineInfo* pli=task->src;
    // DEBUG_print("insp_status:");
    // DEBUG_println(pli->insp_status);

    switch(pli->insp_status)
    {
      case insp_status_OK:
        ACT_PUSH_TASK (act_S.ACT_SEL1H, pli, state_pulseOffset[7], 1,);
        ACT_PUSH_TASK (act_S.ACT_SEL1L, pli, state_pulseOffset[8], 2,);
        inspResCount.OK++;
      break;
      case insp_status_NG:
        ACT_PUSH_TASK (act_S.ACT_SEL2H, pli, state_pulseOffset[9], 1,);
        ACT_PUSH_TASK (act_S.ACT_SEL2L, pli, state_pulseOffset[10],2,);
        inspResCount.NG++;
      break;
      case insp_status_NA:
        // ACT_PUSH_TASK (act_S.ACT_SEL2H, pli, state_pulseOffset[9], 1,);
        // ACT_PUSH_TASK (act_S.ACT_SEL2L, pli, state_pulseOffset[10],2,);
        inspResCount.NA++;
      break;

      case insp_status_DEL://ERROR
      break;

      case insp_status_UNSET:
      default:
        inspResCount.ERR++;
        errorLOG(GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);



        // PassCount=0;
        // inspResCount.ERR++;
        // //Error:The inspection result isn't back
        // //TODO: Send error msg and stop machine
        // errorLOG(GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);

      break;
    }
    // 
    if(RBuf.getTail()==task->src)
    {
      task->src->insp_status=insp_status_DEL;
      task->src->insp_status=insp_status_DEL;
      task->src=NULL;
      RBuf.consumeTail();
    
    }
    // while(1)//remove the DEL tag
    // {
    //   pipeLineInfo* pipe=RBuf.getTail();
    //   if(pipe==NULL)break;

    //   if(pipe->insp_status==insp_status_DEL)
    //   {
    //     RBuf.consumeTail();
    //   }
    //   else
    //   {
    //     break;
    //   }
    // }

  );



  
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


  MessageL += sprintf( (char*)send_rsp+MessageL, "\"OK\":%lu,",inspResCount.OK);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"NG\":%lu,",inspResCount.NG);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"NA\":%lu,",inspResCount.NA);
  MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":%lu,",inspResCount.ERR);

  MessageL--;//remove the last comma',';
  MessageL += sprintf( (char*)send_rsp+MessageL, "},");
  
  return MessageL;                              
}


ERROR_ACTION_TYPE errorActionType=ERROR_ACTION_TYPE::NOP;

ERROR_ACTION_TYPE errorActionTransition(ERROR_ACTION_TYPE atype,GEN_ERROR_CODE code)
{
  ERROR_ACTION_TYPE actionType=ERROR_ACTION_TYPE::NOP;
  switch(code)
  {
    case GEN_ERROR_CODE::RESET:
      actionType=ERROR_ACTION_TYPE::NOP;
    break;
    case GEN_ERROR_CODE::INSP_RESULT_HAS_NO_OBJECT:

      actionType=(atype!=ERROR_ACTION_TYPE::NOP)?
        ERROR_ACTION_TYPE::ALL_STOP:
        ERROR_ACTION_TYPE::FREE_SPIN_2_REV;
    break;

    case GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT:
      
      actionType=(atype!=ERROR_ACTION_TYPE::NOP)?
        ERROR_ACTION_TYPE::ALL_STOP:
        ERROR_ACTION_TYPE::FREE_SPIN_2_REV;
    break;

    case GEN_ERROR_CODE::INSP_RESULT_COUNTER_ERROR:
      actionType=(atype!=ERROR_ACTION_TYPE::NOP)?
        ERROR_ACTION_TYPE::ALL_STOP:
        ERROR_ACTION_TYPE::FREE_SPIN_2_REV;
    break;

    default:
      actionType=ERROR_ACTION_TYPE::ALL_STOP;
    break;
  }
  errorAction(actionType);
  return actionType;
  
}


uint32_t curRevCount=0;
int EV_Axis0_Origin(uint32_t revCount)
{
  curRevCount=revCount;
  if((revCount&7)==0)
  {
    DEBUG_print("REV:");
    DEBUG_println(curRevCount);
  }
}
  
void errorAction(ERROR_ACTION_TYPE cur_action_type)
{
  
  static ERROR_ACTION_TYPE pre_action_type=ERROR_ACTION_TYPE::NOP;
  static uint32_t targetRevCount=0;
  if(pre_action_type!=cur_action_type)
  {
    switch(cur_action_type)
    {
      case ERROR_ACTION_TYPE::FREE_SPIN_2_REV:
        targetRevCount=curRevCount+2;
        
        DEBUG_print("targetRevCount::");
        DEBUG_println(targetRevCount);
      break;
    }
    pre_action_type=cur_action_type;
  }

  switch(cur_action_type)
  {
    
    case ERROR_ACTION_TYPE::NOP:
    break;
    case ERROR_ACTION_TYPE::FREE_SPIN_2_REV:
    case ERROR_ACTION_TYPE::ALL_STOP:
    {
      //DEBUG_println("FREE_SPIN_2_REV  IN p::");
      if(targetRevCount!=curRevCount)
      {
        //if there is an error
        //clear plate
        RBuf.clear();
        act_S.ACT_BACKLight1H.clear();
        act_S.ACT_BACKLight1L.clear();
        act_S.ACT_CAM1.clear();
        act_S.ACT_SEL1H.clear();
        act_S.ACT_SEL1L.clear();
        act_S.ACT_SEL2H.clear();
        act_S.ACT_SEL2L.clear();
        act_S.ACT_SWITCH.clear();
        RESET_GateSensing();
        TCount=0;CCount=0;
         
        digitalWrite(AIR_BLOW_OK_PIN, 0);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
        
        digitalWrite(BACK_LIGHT_PIN, 1);
        //WarningLight
        
//        static uint16_t LED_C_TMP=0;
//        if(LED_C_TMP&0xFF==0)
//        {
//          if(LED_C_TMP&0x100==0)
//            digitalWrite(BACK_LIGHT_PIN, 1);
//          else
//            digitalWrite(BACK_LIGHT_PIN, 0);
//        }
//        LED_C_TMP++;
        
      }
      else
      {
        DEBUG_println("FREE_SPIN_2_REV  REACH.... ending");
        digitalWrite(BACK_LIGHT_PIN, 0);
        errorActionType = errorActionTransition( errorActionType,GEN_ERROR_CODE::RESET );
      }
    }
    break;

    default:
      //if there is an error
//      //clear plate
//      RBuf.clear();
//      RESET_GateSensing();
//    
//      TCount=0;CCount=0;
//      //set speed to zero
//      tar_pulseHZ_=0; 
    break;
  }

}



void errorLOG(GEN_ERROR_CODE code,char* errorLog)
{
  GEN_ERROR_CODE* head_code = ERROR_HIST.getHead();
  if (head_code == NULL)
  {
    ERROR_HIST.consumeTail();
    head_code = ERROR_HIST.getHead();
    
  }
  
  if (head_code != NULL)
  {
    *head_code=code;
    ERROR_HIST.pushHead();

  
    DEBUG_print("errorLOG:");
    DEBUG_println((int)code);
    //errorAction(errorActionType);
  }
  errorActionType = errorActionTransition( errorActionType,code );
//
//  if(0){
//    uint8_t errBuff[100];
//    uint8_t errBuffL=0;
//    errBuffL+=sprintf(errBuff+errBuffL,"{");
//    
//    errBuffL += sprintf( errBuff+errBuffL,"\"type\":\"error_notification\",");
//    errBuffL+=AddErrorCodesToJson(errBuff+errBuffL, 20);
//    if(errorLog)
//      errBuffL+= sprintf(errBuff+errBuffL,"\"log\":\"%s\",",errorLog);
//  
//    errBuffL--;//remove the last ','
//    errBuffL+=sprintf(errBuff+errBuffL,"}");//give it an close
//    WS_Server->SEND_ALL(errBuff,errBuffL,0);
//  }
}






class Websocket_FI:public Websocket_FI_proto{

  public:
  Websocket_FI(uint8_t* buff,uint32_t buffL,IPAddress ip,uint32_t port,IPAddress gateway,IPAddress subnet):
    Websocket_FI_proto(buff,buffL,ip,port,gateway,subnet){}

  virtual void onPeerDisconnect(WebSocketProtocol* WProt)
  {
    DEBUG_println("onPeerDisconnect");
//    tar_pulseHZ_=0;
    return;
  }
  
     
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
    MessageL += sprintf( (char*)send_rsp+MessageL, "\"mode\":\"%s\",",
      mode_info.mode==run_mode_info::TEST?"TEST_NO_BLOW": "NORMAL");

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
        //MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"There is no 'table' in the message\",");
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
        //MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"Table message length is not sufficient, expected length:%d\",",SARRL(state_pulseOffset));
        if(ret_status)*ret_status=-1;
      }
  
      
    }while(0);
    
    return MessageL;
  }
  
  int readJsonScopeNumber(char* jsonStr,char* keyStr, int *ret_num)
  {
    uint8_t nbuffer[10];
    int retL=10;
    if((retL= findJsonScope(jsonStr,keyStr,nbuffer,sizeof(nbuffer)))<=0)
      return -1;

      
    int readN;
    int readL = sscanf(nbuffer, "%d", &readN);
    if(readL!=1)
    {
      return -2;
    }
    if(ret_num)*ret_num=readN;
    return 0;
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

    
    char scopebuff[20];
    int retL = findJsonScope(jbuff,"\"pulse_hz\":",scopebuff,sizeof(scopebuff));
    
    if(retL>0){
      
      
//      DEBUG_print("\n pulse_hz: retL:");
//      DEBUG_println(retL);
//      DEBUG_println(scopebuff);
      int newHZ=tar_pulseHZ_;
      sscanf(scopebuff, "%d", &newHZ);
      tar_pulseHZ_=newHZ;
      retS=0;
    }


    if(strstr (jbuff,"\"mode\":\"TEST_NO_BLOW\""))
    {
      mode_info.mode=run_mode_info::TEST;
      mode_info.misc_var=0;
    }
    
    if(strstr (jbuff,"\"mode\":\"NORMAL\""))
    {
      mode_info.mode=run_mode_info::NORMAL;
      mode_info.misc_var=0;
    }
    
    if(retL>0){

      
      retS=0;
    }


    
    
    if(ret_status)*ret_status=retS;
    return MessageL;
  }

  virtual int Json_CMDExec(WebSocketProtocol* WProt,uint8_t *recv_cmd, int cmdL,sending_buffer *send_pack,int data_in_pack_maxL)
  {
    char *send_rsp=send_pack->data;
    int send_rspL=data_in_pack_maxL;
//    DEBUG_println((char*)recv_cmd);

    char extBuff_[100];
    char *extBuff=extBuff_;
    int extBuffL=sizeof(extBuff_);
    
    
    unsigned int MessageL = 0; //response Length
    
    {
      char type_[30]; 
      char *typeStr = type_;
      int typeL=findJsonScope((char*)recv_cmd,"\"type\":",typeStr,sizeof(type_));
      
      if(typeL<0)typeStr=NULL; 
      else
      {
        typeStr+=1;
        typeL-=2;
        typeStr[typeL]='\0';
//        DEBUG_print("typeStr:");
//        DEBUG_println(typeStr);
      }
      
      
      
      char *idStr = extBuff;
      int idStrL = findJsonScope((char*)recv_cmd,"\"id\":",extBuff,extBuffL);
      if(idStrL<0)idStr=NULL;

      extBuff+=idStrL+1;
      extBuffL-=idStrL+1;
      

      bool isIdAStr=false;
      if(idStr && idStr[0]=='\"' && idStr[idStrL-1]=='\"')
      {
        isIdAStr=true;
        idStr[idStrL-1]='\0';
        idStr++;
      }

      MessageL += sprintf( (char*)send_rsp+MessageL, "{");
      int ret_status=-1;

      
      if(strcmp (typeStr, "inspRep")==0)
      {
        if(mode_info.mode==run_mode_info::TEST)
        {
          return 0;
        }
        int new_count=-99;  
        int pre_count=cur_insp_counter;//0~255
        char *counter_str = extBuff;
        {
          int retL = findJsonScope((char*)recv_cmd,"\"count\":",extBuff,extBuffL);
          if(retL>0)
          {  
//          extBuff+=retL+1;
//          extBuffL-=retL+1;
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


        }
        
        if(errorActionType!=ERROR_ACTION_TYPE::NOP)
        {
          return 0;
        }
        
        if(counter_str==NULL)
        {
          
          errorLOG(GEN_ERROR_CODE::INSP_RESULT_COUNTER_ERROR);
          return 0;
        }



        

        int insp_status=-99;
        {
          char *statusStr = extBuff;
          int retL = findJsonScope((char*)recv_cmd,"\"status\":",extBuff,extBuffL);
          if(retL<0)statusStr=NULL;
          else{
//          extBuff+=retL+1;
//          extBuffL-=retL+1;
            sscanf(statusStr, "%d", &insp_status);
            // DEBUG_print(">>status>>>>");
            // DEBUG_println(insp_status);
          }
        }
        
        char *time_100us_str = extBuff;
        {
          int retL = findJsonScope((char*)recv_cmd,"\"time_100us\":",extBuff,extBuffL);
          if(retL<0)time_100us_str=NULL;
          else
          {
            extBuff+=retL+1;
            extBuffL-=retL+1;
          }
        }
        
        
        noInterrupts();
        TCount--;
        int search_i=-1;
        int len = RBuf.size();
        for(int i=0;i<RBuf.size();i++)
        {
          pipeLineInfo* pipe=RBuf.getTail(i);
          if(pipe==NULL)break;

          if(pipe->insp_status==insp_status_UNSET)
          {
            // DEBUG_printf("Get insp_status:%d c:%d\n",insp_status,cur_insp_counter);
            pipe->insp_status=insp_status;
            ret_status=0;
            search_i=len-1-i;
            break;
          }
        }

        interrupts();


  

        // DEBUG_print("bklh.s:");
        // DEBUG_print(act_S.ACT_BACKLight1H.size());
        // DEBUG_print(" bkll.s:");
        // DEBUG_print(act_S.ACT_BACKLight1L.size());

        // DEBUG_print(" cam.s:");
        // DEBUG_print(act_S.ACT_CAM1.size());

        // DEBUG_print(" actsw.s:");
        // DEBUG_println(act_S.ACT_SWITCH.size());

        // DEBUG_printf("RBuf.s:%d ========s:%d===\n",RBuf.size(),search_i);

        // for(uint8_t i=0;i<RBuf.size();i++)
        // {
        //   pipeLineInfo* tail = RBuf.getTail(i);

        //   DEBUG_printf("%d:%p,\n",tail->insp_status,tail);
        // }



        
        if(ret_status)
        {
          // DEBUG_print("ERROR:ret_status=");
          // DEBUG_println(ret_status);
  
          errorLOG(GEN_ERROR_CODE::INSP_RESULT_HAS_NO_OBJECT);
          //Error:The inspection result matches no object
          //TODO: Send error msg and stop machine
          return 0;
        }
        return 0;
      }
      else if(strcmp (typeStr, "get_dev_info")==0)
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
      else if(strcmp (typeStr, "PING")==0)
      {
        
        //DEBUG_println("PING....");
        MessageL += sprintf( (char*)send_rsp+MessageL,"\"type\":\"PONG\",");
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, send_rspL-MessageL);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, send_rspL-MessageL,inspResCount);
        ret_status=0;
      }
      else  if(strcmp (typeStr, "set_pulse_hz")==0)
      {
      
        char *bufPtr = extBuff;
        int retL = findJsonScope((char*)recv_cmd,"\"pulse_hz\":",extBuff,extBuffL);
        if(retL>0){
          int newHZ=tar_pulseHZ_;
          sscanf(bufPtr, "%d", &newHZ);
          tar_pulseHZ_=newHZ;
          ret_status=0;
        }
      }
      else if(strcmp (typeStr, "test_action")==0)
      {
        int retL;

        if(strstr ((char*)recv_cmd,"\"sub_type\":\"trigger_test\"")!=NULL)
        {
          int trigger_count=10;
          int trigger_duration=20;
          int trigger_post_duration=80;
          int backlight_extra_duration=20;
          int tmpNum;

          if(readJsonScopeNumber((char*)recv_cmd,"\"count\":", &tmpNum)==0)
            trigger_count=tmpNum;

          if(readJsonScopeNumber((char*)recv_cmd,"\"duration\":", &tmpNum)==0)
            trigger_duration=tmpNum;

           
          if(readJsonScopeNumber((char*)recv_cmd,"\"post_duration\":", &tmpNum)==0)
            trigger_post_duration=tmpNum;
           
          if(readJsonScopeNumber((char*)recv_cmd,"\"backlight_extra_duration\":", &tmpNum)==0)
            backlight_extra_duration=tmpNum;


          digitalWrite(CAMERA_PIN, 0);
          digitalWrite(BACK_LIGHT_PIN, 0);
          delay(100);
          delay(trigger_post_duration);
          for(int ix=0;ix<trigger_count;ix++)
          {
            
            digitalWrite(BACK_LIGHT_PIN, 1);
            digitalWrite(CAMERA_PIN, 1);
            
            delay(trigger_duration);
            digitalWrite(CAMERA_PIN, 0);
            delay(backlight_extra_duration);
            digitalWrite(BACK_LIGHT_PIN, 0);
            delay(trigger_post_duration);
            
          }
          
        }
      }

      else if(strcmp (typeStr, "get_setup")==0)
      {
        int ret_st=0;
        MessageL += sprintf( (char*)send_rsp+MessageL,"\"type\":\"get_setup_rsp\","
                                                      "\"ver\":\"0.0.0.0\",");
        MessageL+=MachToJson(send_rsp+MessageL, send_rspL-MessageL, &ret_st);
        DEBUG_print("get_setup:");
        DEBUG_println(send_rsp);
        ret_status = ret_st;
      }
      
      else if(strcmp (typeStr, "set_setup")==0)
      {
        int ret_st=0;
        //DEBUG_print("set_setup::");
        MessageL+=JsonToMach(send_rsp+MessageL, send_rspL-MessageL,recv_cmd,cmdL, &ret_st);
//        DEBUG_print("set_setupL::");
//        DEBUG_println(MessageL);
//        DEBUG_println((char*)send_rsp);
//        
        //MessageL=0;
        ret_status = ret_st;
      }
      
      else if(strcmp (typeStr, "error_get")==0)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"error_info\",",idStr);
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, send_rspL-MessageL);
        ret_status = 0;
      }
      
      else if(strcmp (typeStr, "res_count_get")==0)
      {
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"res_count\",",idStr);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, send_rspL-MessageL,inspResCount);
        ret_status = 0;
      }
      else if(strcmp (typeStr, "res_count_clear")==0)
      {
        memset(&inspResCount,0,sizeof(inspResCount));
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"res_count\",",idStr);
        MessageL += AddResultCountToJson( (char*)send_rsp+MessageL, send_rspL-MessageL,inspResCount);
        ret_status = 0;
      }
      
      else if(strcmp (typeStr, "error_clear")==0)
      {
        errorLOG(GEN_ERROR_CODE::RESET);
        ERROR_HIST.clear();
        MessageL += sprintf( (char*)send_rsp+MessageL, "\"type\":\"error_info\",",idStr);
        MessageL += AddErrorCodesToJson( (char*)send_rsp+MessageL, send_rspL-MessageL);
        ret_status = 0;
      }
      
      else if(strcmp (typeStr, "mode_set")==0)
      {
        if(strstr ((char*)recv_cmd,"\"mode\":\"NORMAL\""))
        {
          mode_info.mode=run_mode_info::NORMAL;
          ret_status = 0;
        }
        
        if(strstr ((char*)recv_cmd,"\"mode\":\"TEST_INC\""))
        {
          if(mode_info.mode!=run_mode_info::TEST)
          {
            mode_info.mode=run_mode_info::TEST;
            mode_info.misc_var=0;
          }
          else
          {
            mode_info.misc_var++;

            mode_info.misc_var%=6;
          }
          ret_status = 0;
        }
        
        if(strstr ((char*)recv_cmd,"\"mode\":\"TEST_NO_BLOW\""))
        {
          if(mode_info.mode!=run_mode_info::TEST)
          {
            mode_info.mode=run_mode_info::TEST;
          }
          mode_info.misc_var=0;
          ret_status = 0;
        }
        if(strstr ((char*)recv_cmd,"\"mode\":\"TEST_ALTER_BLOW\""))
        {
          if(mode_info.mode!=run_mode_info::TEST)
          {
            mode_info.mode=run_mode_info::TEST;
          }
          mode_info.misc_var=1;
          ret_status = 0;
        }
        
        if(strstr ((char*)recv_cmd,"\"mode\":\"ERROR_TEST\""))
        {
          errorLOG(GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);
          ret_status = 0;
        }
      }
      else if(strcmp (typeStr, "MISC/BACK_LIGHT/ON")==0)
      {
        
        digitalWrite(BACK_LIGHT_PIN,1);
      }
      else if(strcmp (typeStr, "MISC/BACK_LIGHT/OFF")==0)
      {
        digitalWrite(BACK_LIGHT_PIN,0);
      }
      else if(strcmp (typeStr, "MISC/CAM_TRIGGER")==0)
      {
        digitalWrite(CAMERA_PIN,1);
        delay(10);
        digitalWrite(CAMERA_PIN,0);
      }
      
      else if(strcmp (typeStr, "MISC/OK_BLOW")==0)
      {
        digitalWrite(AIR_BLOW_OK_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_OK_PIN, 0);
      }
      
      else if(strcmp (typeStr, "MISC/NG_BLOW")==0)
      {
        digitalWrite(AIR_BLOW_NG_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
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

void showOff()
{
  //int delayArr[]={140,140,360,140,140,360};
  int delayArr[]={360,140,140};
  for(int i=0;i<sizeof(delayArr)/sizeof(*delayArr);i++)
  {

    digitalWrite(AIR_BLOW_OK_PIN, 1);
    digitalWrite(AIR_BLOW_NG_PIN, 1);
    digitalWrite(BACK_LIGHT_PIN, 1);

    delay(10);
    digitalWrite(AIR_BLOW_OK_PIN, 0);
    digitalWrite(AIR_BLOW_NG_PIN, 0);
    digitalWrite(BACK_LIGHT_PIN, 0);
    delay(delayArr[i]);
  }
}


uint32_t pulseHZ_step = 50;

int serial_putc( char c, struct __file * )
{
  Serial.write( c );
  return c;
}




uint8_t ws_buff[600];//For websocket
void ETH_RESET()
{
  if(WS_Server!=NULL)
    delete WS_Server;
  WS_Server = new Websocket_FI(ws_buff,sizeof(ws_buff),_ip,_port,_gateway,_subnet);
  
  if(WS_Server)setRetryTimeout(2, 100);
  delay(100);
}


void setup() {
  Serial.begin(115200);
  fdevopen( &serial_putc, 0 );
  ETH_RESET();
  setup_Stepper();
  
  pinMode(CAMERA_PIN, OUTPUT);
  pinMode(AIR_BLOW_OK_PIN, OUTPUT);
  pinMode(AIR_BLOW_NG_PIN, OUTPUT);
  pinMode(BACK_LIGHT_PIN, OUTPUT);
  pinMode(GATE_PIN, INPUT);
  digitalWrite(GATE_PIN, HIGH);//Pull high
  pinMode(FAKE_GATE_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(FEEDER_PIN, OUTPUT);

  digitalWrite(FEEDER_PIN, HIGH);
//  pinMode(FAKE_GATE_PIN, OUTPUT);

  showOff();
  
//#ifdef TEST_MODE
//  tar_pulseHZ_ = 0;//perRevPulseCount_HW/3;
//  mode_info.mode=run_mode_info::TEST;
//  mode_info.misc_var=0;
//#else

//#endif


}

uint32_t ccc=0;

uint32_t totalLoop=0;

int emptyPlateCount=0;

uint32_t _rotl(uint32_t value, int shift) {
    if ((shift &= 31) == 0)
      return value;
    return (value << shift) | (value >> (32 - shift));
}
uint32_t cmpCheckSum(int shift,int num_args, ...)
{
   uint32_t val = 0;
   va_list ap;
   int i;

   va_start(ap, num_args);
   for(i = 0; i < num_args; i++) 
   {
      val ^= _rotl(va_arg(ap, uint32_t),3);
      val = _rotl(val,shift);
   }
   va_end(ap);
 
   return val;
}

uint32_t pre_csum=0;
void printDBGInfo()
{
  uint32_t c_sum = cmpCheckSum(7,6,
    (uint32_t)RBuf.size() ,
    (uint32_t)TCount,
    (uint32_t)CCount,
    PassCount,
    stageUpdated,
    ExeUpdateCount);
  if(pre_csum!=c_sum)
  {
    pre_csum=c_sum;
    
    DEBUG_print(" T:");
    DEBUG_print(TCount);
    

    DEBUG_print(" C:");
    DEBUG_print(CCount);

    DEBUG_print(" P:");
    DEBUG_print(PassCount);
    DEBUG_print(" E:");
    DEBUG_print(ExeUpdateCount);




    DEBUG_print(" pulse:");
    DEBUG_print(logicPulseCount);

    
    DEBUG_println();
    for(int i=0;i<RBuf.size();i++)
    {
      pipeLineInfo* tail = RBuf.getTail(i);
      
      DEBUG_print("[");
      DEBUG_print(i);
      DEBUG_print("]:s:");
      DEBUG_print(tail->stage);
      DEBUG_print(" gp:");
      DEBUG_print(tail->gate_pulse);
      DEBUG_print(" tp:");
      DEBUG_print(tail->trigger_pulse);

      
      DEBUG_println();
    }


    

    
  }

}



void HARD_RESET()
{
  cli();                  // Clear interrupts
  wdt_enable(WDTO_15MS);      // Set the Watchdog to 15ms
  while(1);            // Enter an infinite loop
}


int noConnectionTickCount=0;
void loop() 
{
  if(WS_Server)
    WS_Server->loop_WS();

  totalLoop++;

  if( (totalLoop&0x1F)==0 )
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


  if( (totalLoop&0xFFF)==0)
  {
    static uint32_t nextPulseN=0;
    

    uint32_t dist = pulse_distance(nextPulseN,logicPulseCount,perRevPulseCount);
    if(dist<(perRevPulseCount>>1))
    {
      // DEBUG_printf("curSTEP:%d\n",logicPulseCount);
      nextPulseN+=perRevPulseCount>>2;
      if(nextPulseN>=perRevPulseCount)
      {
        nextPulseN=0;
      }
    }

  }

  if( (totalLoop&(0x7FFF>>1))==0)
  {
    
    if(WS_Server->FindLiveClient()==0)//if no connection exist
    {
      noConnectionTickCount++;
      if(noConnectionTickCount>20)//for quite a lon
      {
        noConnectionTickCount=0;
        ETH_RESET();
      }
    }
    else
    {
      noConnectionTickCount=0;
    }
//    DEBUG_print("RBuf:");
//    DEBUG_println(RBuf.size());
//    DEBUG_print("thres_skip_counter:");
//    DEBUG_println(thres_skip_counter);
//    printDBGInfo();
//    DEBUG_println((char*)WS_Server->json_sec_buffer);
    if(ERROR_HIST.size()!=0)
    {
      // DEBUG_print("Error:");
      // DEBUG_println(ERROR_HIST.size());
    }
    if(errorActionType!=ERROR_ACTION_TYPE::NOP)
    {
      // DEBUG_print("errorActionType:");
      // DEBUG_println((int)errorActionType);
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
  

  //if(ERROR_HIST.size()!=0)//If there is at leaset an error, do errorAction.
  if(errorActionType!=ERROR_ACTION_TYPE::NOP)
  {
    errorAction(errorActionType);
  }

//  //test Con
//  {
//    
//    static uint8_t testE=1;
//    if(curRevCount==6 &&  testE)
//    {
//      
//      DEBUG_print("Test Error Trigger::");
//      testE=0;
//      errorLOG(GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);
//    }
//    
//  }
  
  //for(uint32_t i=0;i!=1;i++)
//  {
//
//    //ddd%=3;
//    uint32_t dddx=(uint32_t)4400*2/100*10;
//    if(ccc++==dddx)
//    {
//      ccc=0;
//    }
//    if(ccc==0)
//        digitalWrite(FAKE_GATE_PIN, HIGH);
//    if(ccc==dddx/2)
//        digitalWrite(FAKE_GATE_PIN, LOW);
//  }
  
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
