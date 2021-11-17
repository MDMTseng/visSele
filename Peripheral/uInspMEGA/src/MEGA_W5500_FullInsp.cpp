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
#include "include/main.h"

uint8_t FEEDER_PIN_status=0;
uint32_t tar_pulseHZ_ = 0;//perRevPulseCount_HW / 4;
uint32_t cur_pulseHZ_ = tar_pulseHZ_;

uint32_t curRevCount = 0;

uint32_t pulseHZ_step = 50;
int32_t OK_LIMIT_CD = -1;//ignore

uint8_t g_max_frame_rate = 50;

//#define TEST_MODE
uint16_t TCount = 0;
uint16_t CCount = 0;
uint16_t ExeUpdateCount = 0;
uint16_t PassCount = 0;
uint16_t stageUpdated = 0;

uint16_t g_max_inspLatency = 0;

struct sharedInfo* p_sInfo= get_SharedInfo();
bool blockNewDetectedObject=false;

GEN_ERROR_CODE errorBuf[20];
RingBuf<typeof(*errorBuf), uint8_t> ERROR_HIST(errorBuf, SARRL(errorBuf));

SYS_INFO sysinfo = {
    .pre_state = SYS_STATE::INIT,
    .state = SYS_STATE::INIT,
    .extra_code = 0,
    .status = 0,
    .PTSyncInfo = {.state = PulseTimeSyncInfo_State::INIT},
  };



run_mode_info mode_info = {
  mode : run_mode_info::NORMAL,
  misc_info : 0
};

InspResCount inspResCount = {0};

int cur_insp_counter = -1;

class Websocket_FI;
Websocket_FI *WS_Server = NULL;


//in libraries/Ethernet/src/utility/w5100.h CS pin is pin 10

//The index type uint8_t would be enough if the buffersize<255

RingBuf_Static<pipeLineInfo, PIPE_INFO_LEN, uint8_t> RBuf;

IPAddress _ip(192, 168, 2, 43);
IPAddress _gateway(192, 168, 1, 1);
IPAddress _subnet(255, 255, 0, 0);
int _port = 5213;

uint32_t state_pulseOffset[] =
    {0, 654, 657, 659, 660, 697, 750, 900, 910, 1073, 1083};


void feederCtrlWithOKLimit()
{
  if(OK_LIMIT_CD!=0)
  {// needs more OK
    if(FEEDER_PIN_status==0)//kick feeder run
      digitalWrite(FEEDER_PIN, FEEDER_PIN_status=1);
  }
  else
  {// OK is full
    if(FEEDER_PIN_status!=0)//stop feeder
      digitalWrite(FEEDER_PIN, FEEDER_PIN_status=0);
  }
}

void ERROR_LOG_PUSH(GEN_ERROR_CODE code)
{
  GEN_ERROR_CODE *head_code = ERROR_HIST.getHead();
  if (head_code == NULL)
  {
    ERROR_HIST.consumeTail();
    head_code = ERROR_HIST.getHead();
  }

  if (head_code != NULL)
  {
    *head_code = code;
    ERROR_HIST.pushHead();

    // DEBUG_print("errorLOG:");
    // DEBUG_println((int)code);
    // //errorAction(sysinfo.err_act);
  }
}

struct ACT_SCH act_S;
char* uint64_t_str(uint64_t n);
char* uint32_t_str(uint32_t n);

void SYS_STATE_LIFECYCLE(SYS_STATE pre_sate, SYS_STATE new_state);
int task_Pulse_Time_Sync(uint32_t pulse);

void SYS_STATE_Transfer(SYS_STATE_ACT act,int extraCode=0)
{
  SYS_STATE state = sysinfo.state;
  

#define _MX1(CASE_STATE,CONTENT) \
  case CASE_STATE :{\
    switch(act){\
      CONTENT\
    }\
    break;\
  }
#define _MX2(CASE_ACT,NEW_STATE) \
  case CASE_ACT :{\
    state = NEW_STATE;\
    break;\
  }\

  switch(state)
  {
    SMM_STATE_TRANSFER_DECLARE(
      _MX1,
      _MX2,
      SYS_STATE,SYS_STATE_ACT
    )
  }
  #undef _MX1
  #undef _MX2
  
  if (sysinfo.state != state)
  { //state changed
    sysinfo.pre_state = sysinfo.state;
    sysinfo.state = state;
    sysinfo.extra_code=extraCode;
    DEBUG_printf("=========s:%d=>%d\n",sysinfo.pre_state,sysinfo.state);
    SYS_STATE_LIFECYCLE(sysinfo.pre_state, sysinfo.state );
  }
}

void SYS_STATE_LOOP()
{
  SYS_STATE_LIFECYCLE(sysinfo.state,sysinfo.state);//same state means loop
}


void RESET_ALL_PIPELINE_QUEUE()
{
  
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
}

uint32_t g_latest_sense_pulse=0;
void SYS_STATE_LIFECYCLE(SYS_STATE pre_sate, SYS_STATE new_state)
{
  SYS_STATE states[3] = {SYS_STATE::NOP};//0: enter, 1:loop, 2:exit
  int i_from, i_to;
  if (pre_sate == new_state)
  {
    i_from = 1;
    i_to = 1;
    states[1] = new_state;
  }
  else
  {
    i_from = 2;
    i_to = 0;
    states[0] = new_state;
    states[2] = pre_sate;
  }
  for (int i = i_from; i >= i_to; i--)//2(exit) -> 1(loop) -> 0(enter) the reversed order is to make sure exit(from old state) run first then run enter(to new state) block
  {

    SYS_STATE state = states[i];
    switch (state)
    {
    case SYS_STATE::NOP:
      break;
    case SYS_STATE::INIT:
      if (i == 2)
      {//For INIT state "EXIT"(i==2) is the first and last action that would run
        blockNewDetectedObject=true;
        digitalWrite(FEEDER_PIN, FEEDER_PIN_status=0);
      } //exit
      break;
    case SYS_STATE::WAIT_FOR_CLIENT_CONNECTION:
      if(i==0)
      {
        blockNewDetectedObject=true;
        RESET_ALL_PIPELINE_QUEUE();
      }
      //No action need to be done
      break;
    case SYS_STATE::DATA_EXCHANGE:
      
      break;
      
    case SYS_STATE::IDLE:
      if (i == 0)
      {
        blockNewDetectedObject=false;//Accept pulse to trigger camera
        //but in this state will not handle other event
        RESET_ALL_PIPELINE_QUEUE();
      } //enter
      else if (i == 1)
      {
        // SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
        // SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);//the event sould be issued by remote
      } //loop
      else
      {
      } //exit
      break;



    case SYS_STATE::INSPECTION_MODE_TEST:
      if (i == 0)
      {
        blockNewDetectedObject=false;
      } //enter
      else if (i == 1)
      {
        
      } //loop
      else
      {
      } //exit
      break;
    break;
    case SYS_STATE::WAIT_FOR_PULSE_STABLE:
    {
      static uint32_t enterPulse=0;
      if (i == 0)
      {
        blockNewDetectedObject=true;//ignore all incoming real object

        RESET_ALL_PIPELINE_QUEUE();

        enterPulse=get_Stepper_pulse_count();
        digitalWrite(BACK_LIGHT_PIN, 0);
      } //enter
      else if (i == 1)
      {
        // uint32_t cur_pulse = get_Stepper_pulse_count();//just a show off flashing
        // uint32_t pulseDiff  = cur_pulse-enterPulse;
        // digitalWrite(BACK_LIGHT_PIN, (pulseDiff>>3)&1);
      } //loop
      else
      {
        
        digitalWrite(BACK_LIGHT_PIN, 0);
      } //exit
      break;
    }

    case SYS_STATE::PULSE_TIME_SYNCING:
    {
      static uint32_t checkPointPulse=0;
      static uint32_t seqInitPulse=0;
      static uint8_t innerState=0;
      static uint8_t preBackLight=0;
      if (i == 0)
      {
        blockNewDetectedObject=true;//ignore all incoming real object

        DEBUG_println(">>ENTER PULSE_TIME_SYNCING>>>");

        RESET_ALL_PIPELINE_QUEUE();

        digitalWrite(AIR_BLOW_OK_PIN, 0);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
        preBackLight=0;
        digitalWrite(BACK_LIGHT_PIN, preBackLight);
        innerState=0;
      } //enter
      else if (i == 1)
      {
        uint32_t cur_pulse = get_Stepper_pulse_count();
        if(innerState==0)
        {
          task_Pulse_Time_Sync(cur_pulse + 0);
          task_Pulse_Time_Sync(cur_pulse + 500);
          uint32_t lastPulse = cur_pulse + 1000;
          task_Pulse_Time_Sync(lastPulse);
          sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::INIT;
          checkPointPulse = lastPulse+2000;
          innerState=1;
          seqInitPulse=cur_pulse;
        }
        else if(innerState==1)
        {//wait for checkpoint
          {
            int32_t seqDiff_0_1sec = 20*(cur_pulse-seqInitPulse)*subPulseSkipCount/cur_pulseHZ_;
            if(seqDiff_0_1sec<16)
            {
              // uint16_t blinkSeq=0;
              // blinkSeq=(blinkSeq<<8)|0b11110111;
              // blinkSeq=(blinkSeq<<8)|0b11110111;
              // blinkSeq<<=(seqDiff_0_1sec);
              // uint8_t newLight= (blinkSeq&0x8000)?1:0;
              // if(preBackLight!=newLight)
              // {
              //   preBackLight=newLight;
              //   digitalWrite(BACK_LIGHT_PIN, preBackLight);
              // }

              
              uint8_t newLight= (seqDiff_0_1sec&1)?1:0;
              if(preBackLight!=newLight)
              {
                preBackLight=newLight;
                digitalWrite(BACK_LIGHT_PIN, preBackLight);
              }
            }
          }

          



          int32_t diff = cur_pulse-checkPointPulse;
          if(diff>0)
          {
            DEBUG_println("INIT Pulse time syncing Timeout!!! retry...");
            innerState=0;
          }
        }
      } //loop
      else
      {

        digitalWrite(BACK_LIGHT_PIN, 0);
        g_latest_sense_pulse=get_Stepper_pulse_count();
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_READY:
    {
      static uint8_t loopDivCounter=0;// just to make the buzy loop not so buzy
       
      if (i == 0)//enter
      {
        blockNewDetectedObject=false;

        // feederCtrlWithOKLimit();
        if (tar_pulseHZ_ * 20 > perRevPulseCount_HW)//faster than 20s pre rev
        {
          feederCtrlWithOKLimit();
        }
        // 
      } 
      else if (i == 1)
      {
        loopDivCounter++;
        if((loopDivCounter&(0xFF))==0)
        {
          
          feederCtrlWithOKLimit();
          uint32_t cur_pulse = get_Stepper_pulse_count();
          uint32_t pulse_diff = cur_pulse-g_latest_sense_pulse;
          uint32_t pulseSep=20*cur_pulseHZ_/subPulseSkipCount;//every 20sec
          // DEBUG_printf("diff:%d",pulse_diff);

          if(pulse_diff>pulseSep)//give 4 rev time
          {//if too long there is no new object incoming, to maintain pulse-time in sync we need to add a phantom pulse to sample camera system time info
            DEBUG_printf("FAKE pulse:%s\n",uint32_t_str(cur_pulse));
            task_newPulseEvent(cur_pulse, cur_pulse, cur_pulse, 10);
          }
        }
      } //loop
      else
      {
        blockNewDetectedObject=true;
        digitalWrite(FEEDER_PIN, FEEDER_PIN_status=0);
      } //exit
      break;
    }

    case SYS_STATE::INSPECTION_MODE_ERROR:
    {
      static uint32_t targetPulse=0;
      if (i == 0)
      {
        blockNewDetectedObject=true;
        
        RESET_ALL_PIPELINE_QUEUE();
        DEBUG_printf(">>ENTER ERROR(%d)>>>\n",sysinfo.extra_code);

        RESET_ALL_PIPELINE_QUEUE();

        digitalWrite(AIR_BLOW_OK_PIN, 0);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
        digitalWrite(BACK_LIGHT_PIN, 1);
        targetPulse=get_Stepper_pulse_count()+perRevPulseCount/3;//in jail for a bit
        ERROR_LOG_PUSH((GEN_ERROR_CODE)sysinfo.extra_code);
      } //enter
      else if (i == 1)
      {
        // switch()
        // {
          
        // }
        int32_t diff=get_Stepper_pulse_count()-targetPulse;
        
        if(diff>0)//times up
        {
          SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);
        }
      }
      else
      {
        digitalWrite(BACK_LIGHT_PIN, 0);
      }
    }


    
    }
  }
}


int ActRegister_pipeLineInfo(pipeLineInfo *pli);



int task_newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  
  uint32_t minPulseDist=cur_pulseHZ_/subPulseSkipCount/g_max_frame_rate;
  if (blockNewDetectedObject)
  {
    return -10;
  }

  {
    uint32_t dist = middle_pulse-g_latest_sense_pulse;
    if(dist<minPulseDist)
    {
      return -11;//pulse interval is too short
    }
    g_latest_sense_pulse=middle_pulse;
  }

  
  //
  pipeLineInfo *head = RBuf.getHead();
  if (head == NULL)
    return -1;

  //get a new object and find a space to log it
  // TCount++;
  // head->s_pulse = start_pulse;
  // head->e_pulse = end_pulse;
  // head->pulse_width = pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status = insp_status_UNSET;
  if (ActRegister_pipeLineInfo(head) != 0)
  { //register failed....
    return -2;
  }
  RBuf.pushHead();
  return 0;
}
int ActRegister_pipeLineInfo(pipeLineInfo *pli)
{


  if (act_S.ACT_BACKLight1H.size_left() >= 1 && act_S.ACT_BACKLight1L.size_left() >= 1 &&
      act_S.ACT_CAM1.size_left() >= 2 && act_S.ACT_SWITCH.size_left() >= 1)
  {
    // DEBUG_printf(">>>>src:%p gate_pulse:%d ",pli,pli->gate_pulse);
    // DEBUG_printf("s:%d ",pli->s_pulse);
    // DEBUG_printf("e:%d ",pli->e_pulse);
    // DEBUG_printf("cur:%d\n",logicPulseCount);

    ACT_PUSH_TASK(act_S.ACT_BACKLight1H, pli, state_pulseOffset[1], 1, );
    ACT_PUSH_TASK(act_S.ACT_BACKLight1L, pli, state_pulseOffset[4], 2, );

    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, state_pulseOffset[2], 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, state_pulseOffset[3], 2, );

    ACT_PUSH_TASK(act_S.ACT_SWITCH, pli, state_pulseOffset[5], 2, );

    return 0;
    // pli->insp_status=insp_status_OK;
  }
  return -1;
}


int ActRegister_Pulse_Time_Sync(pipeLineInfo *pli);
int task_Pulse_Time_Sync(uint32_t pulse)
{

  pipeLineInfo *head = RBuf.getHead();
  if (head == NULL)
    return -1;

  //get a new object and find a space to log it
  // TCount++;
  // head->s_pulse = pulse;
  // head->e_pulse = pulse;
  // head->pulse_width = 10;
  head->gate_pulse = pulse;
  head->insp_status = insp_status_UNSET;
  if (ActRegister_Pulse_Time_Sync(head) != 0)
  { //register failed....
    return -2;
  }
  RBuf.pushHead();
  return 0;
}
int ActRegister_Pulse_Time_Sync(pipeLineInfo *pli)
{

  if (act_S.ACT_BACKLight1H.size_left() >= 1 && act_S.ACT_BACKLight1L.size_left() >= 1 &&
      act_S.ACT_CAM1.size_left() >= 2 && act_S.ACT_SWITCH.size_left() >= 1)
  {
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, state_pulseOffset[2], 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, state_pulseOffset[3], 2, );
    ACT_PUSH_TASK(act_S.ACT_SWITCH, pli, state_pulseOffset[5], 2, );
    return 0;
    // pli->insp_status=insp_status_OK;
  }
  return -1;
}

#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \
  {                                                   \
    ACT_INFO *task = act_rb.getTail();                \
    if (task && task->targetPulse == cur_pulse)       \
    {                                                 \
      cmd_task                                        \
          act_rb.consumeTail();                       \
    }                                                 \
  }

int Run_ACTS(uint32_t cur_pulse)
{
  struct ACT_SCH *acts= &act_S;
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

                   digitalWrite(BACK_LIGHT_PIN, 1););

  ACT_TRY_RUN_TASK(acts->ACT_BACKLight1L, cur_pulse,

                   // DEBUG_printf("BL1 src:%p tp:%d ",task->src,task->targetPulse);
                   // DEBUG_printf("info:%d\n",task->info);

                   digitalWrite(BACK_LIGHT_PIN, 0););

  ACT_TRY_RUN_TASK(
      acts->ACT_CAM1, cur_pulse,

      //
      if (task->info == 1)
      {
        // DEBUG_println("C");
        digitalWrite(CAMERA_PIN, 1);
      } else if (task->info == 2)
      {
        digitalWrite(CAMERA_PIN, 0);
      });

  ACT_TRY_RUN_TASK(acts->ACT_SEL1H, cur_pulse,

                   // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

                    if(OK_LIMIT_CD!=0)
                    {
                      digitalWrite(AIR_BLOW_OK_PIN, 1);
                      inspResCount.OK++;
                      if(OK_LIMIT_CD>0)
                        OK_LIMIT_CD--;
                    }
                    else
                    {
                      inspResCount.NA++;
                    }
                  
                   );

  ACT_TRY_RUN_TASK(acts->ACT_SEL1L, cur_pulse,

                   // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

                   digitalWrite(AIR_BLOW_OK_PIN, 0););

  ACT_TRY_RUN_TASK(acts->ACT_SEL2H, cur_pulse,

                   // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

                    digitalWrite(AIR_BLOW_NG_PIN, 1);
                    inspResCount.NG++;);

  ACT_TRY_RUN_TASK(acts->ACT_SEL2L, cur_pulse,

                   // DEBUG_printf("SEL1 src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

                   digitalWrite(AIR_BLOW_NG_PIN, 0););

  ACT_TRY_RUN_TASK(
      acts->ACT_SWITCH, cur_pulse,

      // DEBUG_printf("SW src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

      pipeLineInfo *pli = task->src;
      // DEBUG_print("insp_status:");
      // DEBUG_println(pli->insp_status);

      switch (pli->insp_status)
      {
        case insp_status_OK:
          ACT_PUSH_TASK(act_S.ACT_SEL1H, pli, state_pulseOffset[7], 1, );
          ACT_PUSH_TASK(act_S.ACT_SEL1L, pli, state_pulseOffset[8], 2, );
          break;
        case insp_status_NG:
          ACT_PUSH_TASK(act_S.ACT_SEL2H, pli, state_pulseOffset[9], 1, );
          ACT_PUSH_TASK(act_S.ACT_SEL2L, pli, state_pulseOffset[10], 2, );
          break;
        case insp_status_NA:
          // ACT_PUSH_TASK (act_S.ACT_SEL2H, pli, state_pulseOffset[9], 1,);
          // ACT_PUSH_TASK (act_S.ACT_SEL2L, pli, state_pulseOffset[10],2,);
          inspResCount.NA++;
          break;

        case insp_status_DEL: //ERROR
          break;

        case insp_status_UNSET:
        default:
          SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);
          break;
      }
      //
      if (RBuf.getTail() == task->src)
      {
        task->src->insp_status = insp_status_DEL;
        task->src->insp_status = insp_status_DEL;
        task->src = NULL;
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

int AddErrorCodesToJson(char *send_rsp, uint32_t send_rspL)
{

  if (ERROR_HIST.size() == 0)
    return 0;
  uint32_t MessageL = 0;
  MessageL += sprintf((char *)send_rsp + MessageL, "\"error_codes\":[");
  for (int i = 0; i < ERROR_HIST.size(); i++)
  {
    GEN_ERROR_CODE *head_code = ERROR_HIST.getTail(i);
    MessageL += sprintf((char *)send_rsp + MessageL, "%d,", *head_code);
  }
  MessageL--; //remove the last comma',';
  MessageL += sprintf((char *)send_rsp + MessageL, "],");

  return MessageL;
}

int AddResultCountToJson(char *send_rsp, uint32_t send_rspL, struct InspResCount &inspResCount)
{
  uint32_t MessageL = 0;
  MessageL += sprintf((char *)send_rsp + MessageL, "\"res_count\":{");

  MessageL += sprintf((char *)send_rsp + MessageL, "\"OK\":%lu,", inspResCount.OK);
  MessageL += sprintf((char *)send_rsp + MessageL, "\"NG\":%lu,", inspResCount.NG);
  MessageL += sprintf((char *)send_rsp + MessageL, "\"NA\":%lu,", inspResCount.NA);
  MessageL += sprintf((char *)send_rsp + MessageL, "\"ERR\":%lu,", inspResCount.ERR);

  MessageL--; //remove the last comma',';
  MessageL += sprintf((char *)send_rsp + MessageL, "},");

  return MessageL;
}

class Websocket_FI : public Websocket_FI_proto
{

public:
  Websocket_FI(uint8_t *buff, uint32_t buffL, IPAddress ip, uint32_t port, IPAddress gateway, IPAddress subnet) : Websocket_FI_proto(buff, buffL, ip, port, gateway, subnet) {}

  WebSocketProtocol *peer = NULL;
  virtual void onPeerDisconnect(WebSocketProtocol *WProt)
  {
    DEBUG_println("onPeerDisconnect");
    //    tar_pulseHZ_=0;
    peer = NULL;
    
    SYS_STATE_Transfer(SYS_STATE_ACT::CLIENT_DISCONNECTED);
    return;
  }

  virtual void onPeerConnect(WebSocketProtocol *WProt)
  {
    DEBUG_println("onPeerConnect");
    SYS_STATE_Transfer(SYS_STATE_ACT::CLIENT_CONNECTED);
    peer = WProt;
  }

  int Mach_state_ToJson(char *jbuff, uint32_t jbuffL, int *ret_status)
  {
    char *send_rsp = jbuff;
    uint32_t MessageL = 0;

    MessageL += sprintf((char *)send_rsp + MessageL, "\"state_pulseOffset\":[");
    for (int i = 0; i < SARRL(state_pulseOffset); i++)
      MessageL += sprintf((char *)send_rsp + MessageL, "%d,", state_pulseOffset[i]);
    MessageL--; //remove the last comma',';
    MessageL += sprintf((char *)send_rsp + MessageL, "],");
    MessageL += sprintf((char *)send_rsp + MessageL, "\"perRevPulseCount\":%d,", perRevPulseCount);
    MessageL += sprintf((char *)send_rsp + MessageL, "\"maxFrameRate\":%d,", g_max_frame_rate);
    MessageL += sprintf((char *)send_rsp + MessageL, "\"subPulseSkipCount\":%d,", subPulseSkipCount);
    MessageL += sprintf((char *)send_rsp + MessageL, "\"pulse_hz\":%d,", tar_pulseHZ_);
    MessageL += sprintf((char *)send_rsp + MessageL, "\"mode\":\"%s\",",
                        mode_info.mode == run_mode_info::TEST ? "TEST_NO_BLOW" : "NORMAL");

                        
    MessageL += sprintf((char *)send_rsp + MessageL, "\"senseInv\":%s,", *getSenseInvPtr()?"true":"false");

    if (ret_status)
      *ret_status = 0;
    return MessageL;
  }

  int Json_state_pulseOffset_ToMach(char *send_rsp, uint32_t send_rspL, char *jbuff, uint32_t jbuffL, int *ret_status)
  {
    char *recv_cmd = jbuff;
    uint32_t MessageL = 0;
    if (ret_status)
      *ret_status = 0;
    do
    {
      int table_scopeL = 0;
      char *table_scope = findJsonScope((char *)recv_cmd, "\"state_pulseOffset\":", &table_scopeL);
      if (table_scope == NULL)
      {
        //MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"There is no 'table' in the message\",");
        if (ret_status)
          *ret_status = -1;

        break;
      }

      if (table_scope[0] == '[' && table_scope[table_scopeL - 1] == ']')
      {
        table_scope++;
        table_scopeL -= 2;
      }

      uint32_t new_state_pulseOffset[SARRL(state_pulseOffset)];

      int adv_len = ParseNumberFromArr(table_scope, new_state_pulseOffset, SARRL(state_pulseOffset)); //return parsed string length

      if (adv_len)
      {
        ret_status = 0;
        memcpy(state_pulseOffset, new_state_pulseOffset, sizeof(new_state_pulseOffset));
      }
      else
      {
        //MessageL += sprintf( (char*)send_rsp+MessageL, "\"ERR\":\"Table message length is not sufficient, expected length:%d\",",SARRL(state_pulseOffset));
        if (ret_status)
          *ret_status = -1;
      }

    } while (0);

    return MessageL;
  }

  int readJsonScopeNumber(char *jsonStr, char *keyStr, int *ret_num)
  {
    uint8_t nbuffer[10];
    int retL = 10;
    if ((retL = findJsonScope(jsonStr, keyStr, nbuffer, sizeof(nbuffer))) <= 0)
      return -1;

    int readN;
    int readL = sscanf(nbuffer, "%d", &readN);
    if (readL != 1)
    {
      return -2;
    }
    if (ret_num)
      *ret_num = readN;
    return 0;
  }

  int MachToJson(char *jbuff, uint32_t jbuffL, int *ret_status)
  {
    char *send_rsp = jbuff;
    uint32_t MessageL = 0;
    MessageL += Mach_state_ToJson((char *)send_rsp + MessageL, jbuffL - MessageL, ret_status);
    return MessageL;
  }

  int JsonToMach(char *send_rsp, uint32_t send_rspL, char *jbuff, uint32_t jbuffL, int *ret_status)
  {
    int retS = 0;
    uint32_t MessageL = 0;
    MessageL += Json_state_pulseOffset_ToMach(send_rsp + MessageL, send_rspL - MessageL, jbuff, jbuffL, &retS);

    char scopebuff[20];
    int retL = findJsonScope(jbuff, "\"pulse_hz\":", scopebuff, sizeof(scopebuff));

    if (retL > 0)
    {
      int newHZ = tar_pulseHZ_;
      sscanf(scopebuff, "%d", &newHZ);
      tar_pulseHZ_ = newHZ;
      retS = 0;
    }

    retL = findJsonScope(jbuff, "\"maxFrameRate\":", scopebuff, sizeof(scopebuff));
    if (retL > 0)
    {
      int mfr = g_max_frame_rate;
      sscanf(scopebuff, "%d", &mfr);
      g_max_frame_rate = mfr;
      retS = 0;
    }

    retL = findJsonScope(jbuff, "\"senseInv\":", scopebuff, sizeof(scopebuff));
    if (retL > 0)
    {
      if (strstr(scopebuff, "true"))
      {
        *getSenseInvPtr()=true;
      }
      else if (strstr(scopebuff, "false"))
      {
        *getSenseInvPtr()=false;
      }
    }




    if (strstr(jbuff, "\"mode\":\"TEST_NO_BLOW\""))
    {
      mode_info.mode = run_mode_info::TEST;
      mode_info.misc_var = 0;
    }

    if (strstr(jbuff, "\"mode\":\"NORMAL\""))
    {
      mode_info.mode = run_mode_info::NORMAL;
      mode_info.misc_var = 0;
    }

    if (retL > 0)
    {

      retS = 0;
    }

    if (ret_status)
      *ret_status = retS;
    return MessageL;
  }

  int convertStringToU64(const char *str, uint64_t *ret_val) // char * preferred
  {
    uint64_t val = 0;
    for (int i = 0; str[i] != NULL; i++)
    {
      val *= 10;
      int dig = str[i] - '0';
      if (dig < 0 || dig > 9)
        return -1;
      val += dig;
    }
    if (ret_val)
      *ret_val = val;
    return 0;
  }

  virtual int Json_CMDExec(WebSocketProtocol *WProt, uint8_t *recv_cmd, int cmdL, sending_buffer *send_pack, int data_in_pack_maxL)
  {
    // DEBUG_printf(">>>s:%d\n",sysinfo.state);
    
    if (peer == NULL)
    {
      onPeerConnect(WProt);
    }

    char *send_rsp = send_pack->data;
    int send_rspL = data_in_pack_maxL;
    // DEBUG_println((char*)recv_cmd);

    char extBuff_[100];
    char *extBuff = extBuff_;
    int extBuffL = sizeof(extBuff_);

    unsigned int MessageL = 0; //response Length

    {
      char type_[30];
      char *typeStr = type_;
      int typeL = findJsonScope((char *)recv_cmd, "\"type\":", typeStr, sizeof(type_));

      if (typeL < 0)
        typeStr = NULL;
      else
      {
        typeStr += 1;
        typeL -= 2;
        typeStr[typeL] = '\0';
        //        DEBUG_print("typeStr:");
        //        DEBUG_println(typeStr);
      }

      char *idStr = extBuff;
      int idStrL = findJsonScope((char *)recv_cmd, "\"id\":", extBuff, extBuffL);
      if (idStrL < 0)
        idStr = NULL;

      extBuff += idStrL + 1;
      extBuffL -= idStrL + 1;

      bool isIdAStr = false;
      if (idStr && idStr[0] == '\"' && idStr[idStrL - 1] == '\"')
      {
        isIdAStr = true;
        idStr[idStrL - 1] = '\0';
        idStr++;
      }

      MessageL += sprintf((char *)send_rsp + MessageL, "{");
      int ret_status = -1;

      if (strcmp(typeStr, "inspRep") == 0)
      {
        uint32_t reportRX_Pulse = get_Stepper_pulse_count();
        int new_count = -99;
        int pre_count = cur_insp_counter; //0~255
        char *counter_str = extBuff;

        if(0)//skip the counter verification part
        {
          int retL = findJsonScope((char *)recv_cmd, "\"count\":", extBuff, extBuffL);
          if (retL > 0)
          {
            //          extBuff+=retL+1;
            //          extBuffL-=retL+1;
            sscanf(counter_str, "%d", &new_count);

            if (new_count == -1)
            {
              pre_count = 254;
              new_count = 255;
            }

            cur_insp_counter = new_count;
            if (((pre_count + 1) & 0xFF) == new_count)
            { //PASS
            }
            else
            {
              counter_str = NULL;
              DEBUG_print(pre_count);
              DEBUG_print("<p  n>");
              DEBUG_println(new_count);
              
              SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_COUNTER_ERROR);
            }
          }
          else
          {
            counter_str = NULL;
          }
        }


        int insp_status = -99;
        {
          char *statusStr = extBuff;
          int retL = findJsonScope((char *)recv_cmd, "\"status\":", extBuff, extBuffL);
          if (retL < 0)
            statusStr = NULL;
          else
          {
            //          extBuff+=retL+1;
            //          extBuffL-=retL+1;
            sscanf(statusStr, "%d", &insp_status);
            // DEBUG_print(">>status>>>>");
            // DEBUG_println(insp_status);
          }
        }

        uint64_t time_us = 0;
        char *time_us_str = extBuff;
        {

          
          int retL;
          retL = findJsonScope((char *)recv_cmd, "\"time_100us\":", extBuff, extBuffL);
          if(retL>=0)
          {
            if(convertStringToU64(time_us_str, &time_us)==0)
            {
              time_us*=100;
            }
            else
            {
              time_us=0;
            }
          }

          if(time_us==0)
          {
            
            retL = findJsonScope((char *)recv_cmd, "\"time_us\":", extBuff, extBuffL);
            if(retL>=0)
            {
              if(convertStringToU64(time_us_str, &time_us)==0)
              {
              }
              else
              {
                time_us=0;
              }
            }
          }

          if(time_us==0)
          {//no time info
            sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::INIT;
            SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_HAS_NO_TIME_STAMP);
            return 0;
          }
          else
          {
            if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::INIT)
            {
              sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_preBaseTime;
            }

            if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_preBaseTime)
            {
              sysinfo.PTSyncInfo.pre_basePulse_us = time_us;
              sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_preBasePulse;
            }
            else if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_BaseTime)
            {
              sysinfo.PTSyncInfo.basePulse_us = time_us;
              sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_BasePulse;
            }
          }
        }

        uint32_t targetObjGatePulse = 0;
        static int resetCount = 0;
        if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_Verify ||
            sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::READY)
        {
         
          // uint32_t pulseDiff=gate_pulse-sysinfo.PTSyncInfo.basePulseCount;
          uint64_t pulseTimeDiff = time_us - sysinfo.PTSyncInfo.basePulse_us;
          uint64_t pulseDiff = (pulseTimeDiff * sysinfo.PTSyncInfo.pulses_per_1shiftXus) >> (PULSE_TIME_SYNC_USSHIFT-1);
          pulseDiff=(pulseDiff>>1)+((pulseDiff&1)?1:0);//binary round
          targetObjGatePulse = pulseDiff + sysinfo.PTSyncInfo.basePulseCount;

          
        }

        noInterrupts();
        TCount--;
        int search_i = -1;
        int len = RBuf.size();

        pipeLineInfo *pipeTarget = NULL;

        int32_t diff;
        int32_t signed_diff;


        uint32_t matched_obj_pulse = 0;
        uint8_t re_search_range=4;
        for (int i = 0; i < RBuf.size(); i++)
        {
          pipeLineInfo *pipe = RBuf.getTail(i);
          if (pipe == NULL)
            break;

          if (pipe->insp_status == insp_status_UNSET)
          {
            if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::READY ||
                sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_Verify)
            {

              matched_obj_pulse = pipe->gate_pulse;
              diff = targetObjGatePulse - matched_obj_pulse;
              signed_diff=diff;
              if (diff < 0)
                diff = -diff;
                
              if (diff < 5)
              {
                
                pipeTarget = pipe;
                ret_status = 0;
                break;
              }
              else
              {
                pipe->insp_status =insp_status_NA;//skip this one and try to find next match
              }

              if((--re_search_range)==0)
              {
                pipeTarget = pipe;
                ret_status = -1;
                break;
              }
            }

            else
            {
              
              pipeTarget = pipe;
              ret_status = 0;
              break;
            }
            continue;
          }
        }

        if (pipeTarget != NULL)
        {

          matched_obj_pulse = pipeTarget->gate_pulse;
          if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_preBasePulse)
          {
            sysinfo.PTSyncInfo.pre_basePulseCount = matched_obj_pulse;
            sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_BaseTime;
          }
          else if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_BasePulse)
          {
            sysinfo.PTSyncInfo.basePulseCount = matched_obj_pulse;
            sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_DATA_CALC;
          }
          // DEBUG_printf("Get insp_status:%d c:%d\n",insp_status,cur_insp_counter);
        }

        interrupts();

        // DEBUG_printf("matched_obj_pulse=%" PRIu32 "  state:%d\n", matched_obj_pulse, sysinfo.PTSyncInfo.state);

        if (pipeTarget == NULL)
        {
          sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::INIT;
        }
        else if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::READY ||
                 sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_Verify)
        {


          
          // DEBUG_printf("ser:%d\n",re_search_range);
          if (diff < 5)
          { //Pulse sync error is in tolerable region
            // DEBUG_printf("ist:%d\n",insp_status);
            pipeTarget->insp_status = insp_status;//accept the status

            if (mode_info.mode == run_mode_info::TEST && 
              (insp_status==insp_status_OK ||  insp_status==insp_status_NG))
            {//if in test mode override the status
              switch (mode_info.misc_var)
              {

              case 0:
                pipeTarget->insp_status = insp_status_NA;
                break;

              case 1:
                pipeTarget->insp_status = (mode_info.misc_var2 & 1) ? insp_status_OK : insp_status_NG;
                break;

              case 2:
                pipeTarget->insp_status = (mode_info.misc_var2 & 1) ? insp_status_OK : insp_status_NA;
                break;

              case 3:
                pipeTarget->insp_status = insp_status_OK;
                break;

              case 4:
                pipeTarget->insp_status = (mode_info.misc_var2 & 1) ? insp_status_NG : insp_status_NA;
                break;

              case 5:
                pipeTarget->insp_status = insp_status_NG;
                break;

              default:
                mode_info.misc_var = 0;
              }

              mode_info.misc_var2++;
            }



            sysinfo.PTSyncInfo.basePulse_us = time_us;
            sysinfo.PTSyncInfo.basePulseCount = matched_obj_pulse;
            sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::READY;
            SYS_STATE_Transfer(SYS_STATE_ACT::PULSE_TIME_SYNC);

            uint16_t latency = reportRX_Pulse-matched_obj_pulse-state_pulseOffset[2];//Offset[2] is camera trigger
            
            if(g_max_inspLatency<latency)
            {
              g_max_inspLatency=latency;
            }
            else
            {
              g_max_inspLatency=(g_max_inspLatency*15+latency)>>4;
            }
            // DEBUG_printf("max_latency=%" PRIu16 "\n",g_max_inspLatency);
            // g_max_inspLatency
          }
          else
          {
          
            sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::INIT; //unmatch..

            
            SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR ,(int)GEN_ERROR_CODE::INSP_RESULT_PULSE_TIME_OUT_OF_SYNC);
          }
        }
        else if (sysinfo.PTSyncInfo.state == PulseTimeSyncInfo_State::SETUP_DATA_CALC)
        {

          uint32_t pulseDiff = sysinfo.PTSyncInfo.basePulseCount - sysinfo.PTSyncInfo.pre_basePulseCount;
          uint64_t pulseTimeDiff = sysinfo.PTSyncInfo.basePulse_us - sysinfo.PTSyncInfo.pre_basePulse_us;

          uint64_t factor = ((uint64_t)pulseDiff << (PULSE_TIME_SYNC_USSHIFT+1)) / pulseTimeDiff;
          sysinfo.PTSyncInfo.pulses_per_1shiftXus = (factor>>1)+((factor&1)?1:0);//binary round
          // sysinfo.PTSyncInfo.pulses_per_1shiftXus+=1;
          sysinfo.PTSyncInfo.state = PulseTimeSyncInfo_State::SETUP_Verify;

          DEBUG_printf("pulseDiff=%d ", pulseDiff);
          DEBUG_printf("timeDiff=%s\n",uint64_t_str(pulseTimeDiff));
          DEBUG_printf("pulses_per_1shiftXus=%s\n",uint64_t_str(sysinfo.PTSyncInfo.pulses_per_1shiftXus));
        }

        if (ret_status)
        {
          // DEBUG_print("ERROR:ret_status=");
          // DEBUG_println(ret_status);

          SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_HAS_NO_OBJECT);
          //Error:The inspection result matches no object
          //TODO: Send error msg and stop machine
          return 0;
        }
        return 0;
      }
      else if (strcmp(typeStr, "PING") == 0)
      {

        //DEBUG_println("PING....");
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"PONG\",");
        MessageL += AddErrorCodesToJson((char *)send_rsp + MessageL, send_rspL - MessageL);
        MessageL += AddResultCountToJson((char *)send_rsp + MessageL, send_rspL - MessageL, inspResCount);
        
        MessageL += sprintf((char *)send_rsp + MessageL, "\"latency\":%"PRIu16",",g_max_inspLatency);
        MessageL += sprintf((char *)send_rsp + MessageL, "\"sys_state\":%d,",sysinfo.state);
        
        if(OK_LIMIT_CD>=0)
          MessageL += sprintf((char *)send_rsp + MessageL, "\"OK_LIMIT_CD\":%lu,", OK_LIMIT_CD);

        ret_status = 0;
      }
      else if (strcmp(typeStr, "set_pulse_hz") == 0)
      {

        char *bufPtr = extBuff;
        int retL = findJsonScope((char *)recv_cmd, "\"pulse_hz\":", extBuff, extBuffL);
        if (retL > 0)
        {
          int newHZ = tar_pulseHZ_;
          sscanf(bufPtr, "%d", &newHZ);
          tar_pulseHZ_ = newHZ;
          ret_status = 0;
        }
      }
      else if (strcmp(typeStr, "test_action") == 0)
      {
        int retL;

        if (strstr((char *)recv_cmd, "\"sub_type\":\"trigger_test\"") != NULL)
        {
          int trigger_count = 10;
          int trigger_duration = 20;
          int trigger_post_duration = 80;
          int backlight_extra_duration = 20;
          int tmpNum;

          if (readJsonScopeNumber((char *)recv_cmd, "\"count\":", &tmpNum) == 0)
            trigger_count = tmpNum;

          if (readJsonScopeNumber((char *)recv_cmd, "\"duration\":", &tmpNum) == 0)
            trigger_duration = tmpNum;

          if (readJsonScopeNumber((char *)recv_cmd, "\"post_duration\":", &tmpNum) == 0)
            trigger_post_duration = tmpNum;

          if (readJsonScopeNumber((char *)recv_cmd, "\"backlight_extra_duration\":", &tmpNum) == 0)
            backlight_extra_duration = tmpNum;

          digitalWrite(CAMERA_PIN, 0);
          digitalWrite(BACK_LIGHT_PIN, 0);
          delay(100);
          delay(trigger_post_duration);
          for (int ix = 0; ix < trigger_count; ix++)
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

      else if (strcmp(typeStr, "get_setup") == 0)
      {
        int ret_st = 0;
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"get_setup_rsp\","
                                                         "\"ver\":\"0.9.8.0\",");
        MessageL += MachToJson(send_rsp + MessageL, send_rspL - MessageL, &ret_st);
        DEBUG_print("get_setup:");
        DEBUG_println(send_rsp);
        ret_status = ret_st;
      }

      else if (strcmp(typeStr, "set_setup") == 0)
      {
        int ret_st = 0;
        // DEBUG_print("set_setup:");
        MessageL += JsonToMach(send_rsp + MessageL, send_rspL - MessageL, recv_cmd, cmdL, &ret_st);
        //        DEBUG_print("set_setupL::");
        //        DEBUG_println(MessageL);
        //        DEBUG_println((char*)send_rsp);
        //
        //MessageL=0;
        ret_status = ret_st;
        
        SYS_STATE_Transfer(SYS_STATE_ACT::DATA_EXCHANGE_OK);
      }

      else if (strcmp(typeStr, "error_get") == 0)
      {
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"error_info\",", idStr);
        MessageL += AddErrorCodesToJson((char *)send_rsp + MessageL, send_rspL - MessageL);
        ret_status = 0;
      }

      else if (strcmp(typeStr, "enter_inspection") == 0)
      {
        SYS_STATE_Transfer(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
        ret_status = 0;
      }
      
      else if (strcmp(typeStr, "exit_inspection") == 0)
      {
        SYS_STATE_Transfer(SYS_STATE_ACT::EXIT_INSPECTION_MODE);
        ret_status = 0;
      }

      else if (strcmp(typeStr, "res_count_get") == 0)
      {
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"res_count\",", idStr);
        MessageL += AddResultCountToJson((char *)send_rsp + MessageL, send_rspL - MessageL, inspResCount);
        ret_status = 0;
      }
      else if (strcmp(typeStr, "res_count_clear") == 0)
      {
        memset(&inspResCount, 0, sizeof(inspResCount));
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"res_count\",", idStr);
        MessageL += AddResultCountToJson((char *)send_rsp + MessageL, send_rspL - MessageL, inspResCount);
        ret_status = 0;
      }
      if (strcmp(typeStr, "set_OK_limit_cd") == 0)
      {

        char *bufPtr = extBuff;
        int retL = findJsonScope((char *)recv_cmd, "\"value\":", extBuff, extBuffL);
        if (retL > 0)
        {
          
          // DEBUG_println(bufPtr);
          int32_t limit;
          sscanf(bufPtr, "%" SCNd32, &limit);
          // DEBUG_printf("limit:%d\n",limit);
          OK_LIMIT_CD=limit;
          ret_status = 0;
          
        }
      }

      else if (strcmp(typeStr, "error_clear") == 0)
      {
        ERROR_HIST.clear();
        MessageL += sprintf((char *)send_rsp + MessageL, "\"type\":\"error_info\",", idStr);
        MessageL += AddErrorCodesToJson((char *)send_rsp + MessageL, send_rspL - MessageL);
        ret_status = 0;
      }

      else if (strcmp(typeStr, "mode_set") == 0)
      {
        if (strstr((char *)recv_cmd, "\"mode\":\"NORMAL\""))
        {
          mode_info.mode = run_mode_info::NORMAL;
          ret_status = 0;
        }

        if (strstr((char *)recv_cmd, "\"mode\":\"TEST_INC\""))
        {
          if (mode_info.mode != run_mode_info::TEST)
          {
            mode_info.mode = run_mode_info::TEST;
            mode_info.misc_var = 0;
          }
          else
          {
            mode_info.misc_var++;

            mode_info.misc_var %= 6;
          }
          ret_status = 0;
        }

        if (strstr((char *)recv_cmd, "\"mode\":\"TEST_NO_BLOW\""))
        {
          if (mode_info.mode != run_mode_info::TEST)
          {
            mode_info.mode = run_mode_info::TEST;
          }
          mode_info.misc_var = 0;
          ret_status = 0;
        }
        if (strstr((char *)recv_cmd, "\"mode\":\"TEST_ALTER_BLOW\""))
        {
          if (mode_info.mode != run_mode_info::TEST)
          {
            mode_info.mode = run_mode_info::TEST;
          }
          mode_info.misc_var = 1;
          ret_status = 0;
        }

        if (strstr((char *)recv_cmd, "\"mode\":\"ERROR_TEST\""))
        {
          
          SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT);
          ret_status = 0;
        }
      }
      else if (strcmp(typeStr, "MISC/BACK_LIGHT/ON") == 0)
      {
        digitalWrite(BACK_LIGHT_PIN, 1);
      }
      else if (strcmp(typeStr, "MISC/BACK_LIGHT/OFF") == 0)
      {
        digitalWrite(BACK_LIGHT_PIN, 0);
      }
      else if (strcmp(typeStr, "MISC/FEEDER/ON") == 0)
      {
        digitalWrite(FEEDER_PIN, FEEDER_PIN_status=1);
      }
      else if (strcmp(typeStr, "MISC/FEEDER/OFF") == 0)
      {
        digitalWrite(FEEDER_PIN, FEEDER_PIN_status=0);
      }
      else if (strcmp(typeStr, "MISC/CAM_TRIGGER") == 0)
      {
        digitalWrite(CAMERA_PIN, 1);
        delay(10);
        digitalWrite(CAMERA_PIN, 0);
      }

      else if (strcmp(typeStr, "MISC/OK_BLOW") == 0)
      {
        digitalWrite(AIR_BLOW_OK_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_OK_PIN, 0);
      }

      else if (strcmp(typeStr, "MISC/NG_BLOW") == 0)
      {
        digitalWrite(AIR_BLOW_NG_PIN, 1);
        delay(10);
        digitalWrite(AIR_BLOW_NG_PIN, 0);
      }

      if (idStr)
      {
        if (isIdAStr)
        {
          MessageL += sprintf((char *)send_rsp + MessageL, "\"id\":\"%s\",", idStr);
        }
        else
        {
          MessageL += sprintf((char *)send_rsp + MessageL, "\"id\":%s,", idStr);
        }
      }

      MessageL += sprintf((char *)send_rsp + MessageL, "\"st\":%d,", ret_status);
      if (send_rsp[MessageL - 1] == ',')
        MessageL--;
      MessageL += sprintf((char *)send_rsp + MessageL, "}");
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





void loop()
{
  static int emptyPlateCount = 0;
  static uint32_t totalLoop = 0;
  static int noConnectionTickCount = 0;
  if (WS_Server)
    WS_Server->loop_WS();

  totalLoop++;

  if ((totalLoop & 0x1F) == 0)
  {


    cur_pulseHZ_ = loop_Stepper(tar_pulseHZ_, pulseHZ_step);

    
    SYS_STATE_Transfer(cur_pulseHZ_ == tar_pulseHZ_?SYS_STATE_ACT::PULSE_STABLE:SYS_STATE_ACT::PULSE_UNSTABLE);
  }

  SYS_STATE_LOOP();
  // if ((totalLoop & 0xFFF) == 0)
  // {
  //   static uint32_t nextPulseN = 0;

  //   uint32_t dist = nextPulseN - logicPulseCount_;
  // }

  if ((totalLoop & (0x7FFF >> 1)) == 0)
  {

    if (WS_Server->FindLiveClient() == 0) //if no connection exists
    {
      noConnectionTickCount++;
      if (noConnectionTickCount > 20) //for quite a long time
      {
        noConnectionTickCount = 0;
        ETH_RESET();
      }
    }
    else
    {
      noConnectionTickCount = 0;
    }

    if (RBuf.size() == 0)
    {
      emptyPlateCount++;
    }
    else
    {
      emptyPlateCount = 0;
    }
  }


}



void showOff()
{
  //int delayArr[]={140,140,360,140,140,360};
  int delayArr[] = {360, 140, 140};
  for (int i = 0; i < sizeof(delayArr) / sizeof(*delayArr); i++)
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


int serial_putc(char c, struct __file *)
{
  Serial.write(c);
  return c;
}

uint8_t ws_buff[300]; //if the buffer size less than 600 then websocket would not work (TCP would be fine tho)
void ETH_RESET()
{
  if (WS_Server != NULL)
    delete WS_Server;
  WS_Server = new Websocket_FI(ws_buff, sizeof(ws_buff), _ip, _port, _gateway, _subnet);

  if (WS_Server)
    setRetryTimeout(2, 100);
  delay(100);
}

void setup()
{
  Serial.begin(115200);
  fdevopen(&serial_putc, 0);
  ETH_RESET();
  setup_Stepper();

  pinMode(CAMERA_PIN, OUTPUT);
  pinMode(AIR_BLOW_OK_PIN, OUTPUT);
  pinMode(AIR_BLOW_NG_PIN, OUTPUT);
  pinMode(BACK_LIGHT_PIN, OUTPUT);
  pinMode(GATE_PIN, INPUT);
  digitalWrite(GATE_PIN, HIGH); //Pull high
  pinMode(FAKE_GATE_PIN, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pinMode(FEEDER_PIN, OUTPUT);

  digitalWrite(FEEDER_PIN, FEEDER_PIN_status=0);//stop
  //  pinMode(FAKE_GATE_PIN, OUTPUT);

  showOff();

  //#ifdef TEST_MODE
  //  tar_pulseHZ_ = 0;//perRevPulseCount_HW/3;
  //  mode_info.mode=run_mode_info::TEST;
  //  mode_info.misc_var=0;
  //#else

  //#endif
  
  SYS_STATE_Transfer(SYS_STATE_ACT::INIT_OK);
  *getSenseInvPtr()=false;
}


int EV_Axis0_Origin(uint32_t revCount)
{
  curRevCount = revCount;
  if ((revCount & 7) == 0)
  {
    DEBUG_print("REV:");
    DEBUG_println(curRevCount);
  }
}


uint32_t pre_csum = 0;
void printDBGInfo()
{
  uint32_t c_sum = cmpCheckSum(7, 6,
                               (uint32_t)RBuf.size(),
                               (uint32_t)TCount,
                               (uint32_t)CCount,
                               PassCount,
                               stageUpdated,
                               ExeUpdateCount);
  if (pre_csum != c_sum)
  {
    pre_csum = c_sum;

    DEBUG_print(" T:");
    DEBUG_print(TCount);

    DEBUG_print(" C:");
    DEBUG_print(CCount);

    DEBUG_print(" P:");
    DEBUG_print(PassCount);
    DEBUG_print(" E:");
    DEBUG_print(ExeUpdateCount);

    // DEBUG_print(" pulse:");
    // DEBUG_print(logicPulseCount_);

    DEBUG_println();
    for (int i = 0; i < RBuf.size(); i++)
    {
      pipeLineInfo *tail = RBuf.getTail(i);

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




char* uint32_t_str(uint32_t n)
{
  uint8_t base = 10;
  static char buf[3 * sizeof(uint32_t) + 1]; // "big enough" buffer.
  char *str = &buf[sizeof(buf) - 1];
  *str = '\0';
  do
  {
    char c = n % base;
    n /= base;
    *--str = c + '0';
  } while (n);

  // Serial.print(str);
  return str;
}

char* uint64_t_str(uint64_t n)
{
  uint8_t base = 10;
  static char buf[3 * sizeof(uint64_t) + 1]; // "big enough" buffer.
  char *str = &buf[sizeof(buf) - 1];
  *str = '\0';
  do
  {
    char c = n % base;
    n /= base;
    *--str = c + '0';
  } while (n);

  // Serial.print(str);
  return str;
}