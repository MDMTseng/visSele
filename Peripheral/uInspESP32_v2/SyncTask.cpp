
#include "main.hpp"
#include "LOG.h"
#include "Pipeline.hpp" // pipeline objects
#include "Scheduler.hpp" // action queues
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include "src/protocol/Data_Layer_Protocol.hpp"
#include "driver/timer.h"
#include <string>

#pragma once
#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)


#define GPIOLS32_SET(PIN) GPIO.out_w1ts=1<<(PIN);
#define GPIOLS32_CLR(PIN) GPIO.out_w1tc=1<<(PIN);
  

#define SARRL(SARR) (sizeof((SARR)) / sizeof(*(SARR)))

// Error handling now managed by Diagnostics module
// Legacy ERROR_HIST removed - use Diagnostics::pushError() instead

// sysinfo global variable removed - now using StateMachine class
// Access to SYS_INFO is now through stateMachine.getSystemInfo()



float SETUP_TAR_FREQ=0;
bool SYS_FREQ_STABLE=false;
float SYS_TAR_FREQ=1000;
float SYS_CUR_FREQ=0;
float SYS_FREQ_ADV_STEP=5;
bool SYS_STEPPER_DISABLED=false;

uint32_t SYS_MIN_PULSE_TIME_SEP_us=(1000000/15);
int SEL1_ACT_COUNTDOWN=-1;

#define _PLAT_DIAMITER_mm 350
#define _PLAT_CIRC_um (_PLAT_DIAMITER_mm*3.14159*1000)
#define _PLAT_SUB_STEP 800
#define _PLAT_PULSE_PER_TURN (_PLAT_SUB_STEP*18 *2)
#define _PLAT_DIST_um_PER_STEP ((int)(_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))

#define _PLAT_DIST_um(stepCount) ((int)(stepCount*_PLAT_CIRC_um/_PLAT_PULSE_PER_TURN))
#define _PLAT_DIST_step(dist_um) ((int)(dist_um*_PLAT_PULSE_PER_TURN/_PLAT_CIRC_um))

//disk D=350 circumference 350*Pi
//1600*9 steps per round
//0.076mm per step


// pipeLineInfo moved to Pipeline.hpp

uint64_t SEL1_Count=0;
uint64_t SEL2_Count=0;
uint64_t SEL3_Count=0;
uint64_t NA_Count=0;
uint64_t SKIP_Count=0;

typedef struct stagePulseOffset{
  uint32_t CAM1_on;
  uint32_t CAM1_off;
  uint32_t L1A_on;
  uint32_t L1A_off;

  uint32_t CAM2_on;
  uint32_t CAM2_off;
  uint32_t L2A_on;
  uint32_t L2A_off;

  uint32_t SWITCH;


  uint32_t SEL1_on;
  uint32_t SEL1_off;

  uint32_t SEL2_on;
  uint32_t SEL2_off;

  uint32_t SEL3_on;
  uint32_t SEL3_off;
}stagePulseOffset;

stagePulseOffset STAGE_PULSE_OFFSET={
  .CAM1_on =654,
  .CAM1_off=656,
  .L1A_on =654,
  .L1A_off=666,


  .CAM2_on =654,
  .CAM2_off=656,
  .L2A_on =654,
  .L2A_off=656,


  .SWITCH =697,


  .SEL1_on=700,
  .SEL1_off=701,
  .SEL2_on=710,
  .SEL2_off=711,
  .SEL3_on=720,
  .SEL3_off=721



};


// PIPE_INFO_LEN definition & RBuf moved to Pipeline module (Pipeline.hpp / Pipeline.cpp)



// ACT_INFO & act_S moved to Scheduler module. Macros retained (will migrate later)
#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info, cusCode_task) \
  { ACT_INFO *_task_ = (rb).getHead(); if (_task_) { _task_->targetPulse = (plinfo->gate_pulse + pulseOffset); _task_->src = plinfo; _task_->info = _info; cusCode_task (rb).pushHead(); } }

//EXP:
// ((0-1)>>1)+1
// ((0xFF)>>1)+1
// (0x7F)+1
// (0x80)
#define UNSIGNED_NUM_HIGHEST_BIT(num) ( (( ((typeof(num))0)-1 )>>1)+1   )


//if targetPulse-cur_pulse <=0
//=> targetPulse-cur_pulse-1 <0 (in unsigned number the highest bit is 1)
//return Yes or no
#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \ 
  {                                                   \
    ACT_INFO *task = act_rb.getTail();                \
    if (task && ((task->targetPulse-cur_pulse-1)&UNSIGNED_NUM_HIGHEST_BIT(cur_pulse)))\
    {                                                 \
      {cmd_task }                                     \
      act_rb.consumeTail();                           \
    }else  task=NULL;                                 \
    task!=NULL;                                       \
  }




string CAM1_ID;
string CAM1_Tags;
string CAM2_ID;
string CAM2_Tags;

// act_S now defined in Scheduler.cpp
void RESET_ALL_PIPELINE_QUEUE()
{
  Pipeline::reset();
  Scheduler::reset();
  gateSensor.reset();
}


// Message handling now managed by MessageBus module
// Legacy TaskQ2CommInfo structures removed - use MessageBus::sendMessage() instead


// ERROR_LOG_PUSH now handled by Diagnostics module
// Use Diagnostics::pushError() instead


bool blockNewDetectedObject=false;


// SYS_STATE_LIFECYCLE function removed - now using StateMachine class




// SYS_STATE_Transfer function removed - now using StateMachine class
  

// Old state machine macros removed - now using StateMachine class\

// Remaining state machine code removed - now using StateMachine class








int ActRegister_pipeLineInfo(pipeLineInfo *pli);


uint32_t _prePulse=0;
uint64_t _preTime=0;
int newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  static uint32_t acc_tid=1;
  uint32_t _prePulse_BK=_prePulse;
  _prePulse=middle_pulse;
  if(middle_pulse-_prePulse_BK<(_PLAT_DIST_step(3500)))return -9;
  uint64_t curTime = esp_timer_get_time();
  if(curTime-_preTime<SYS_MIN_PULSE_TIME_SEP_us)return -8;
  _preTime=curTime;


  if(blockNewDetectedObject)return -1;
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
  head->tid=acc_tid;
  if (ActRegister_pipeLineInfo(head) != 0)
  { //register failed....
    return -2;
  }
  RBuf.pushHead();
  acc_tid++;
  return 0;
}
int ActRegister_pipeLineInfo(pipeLineInfo *pli)
{


  if (act_S.ACT_L1A.space() >= 2 && act_S.ACT_L2A.space() >= 2 &&
      act_S.ACT_CAM1.space() >= 2 && act_S.ACT_CAM2.space() >= 2 && act_S.ACT_SWITCH.space() >= 1)
  {
    // DEBUG_printf(">>>>src:%p gate_pulse:%d ",pli,pli->gate_pulse);
    // DEBUG_printf("s:%d ",pli->s_pulse);
    // DEBUG_printf("e:%d ",pli->e_pulse);
    // DEBUG_printf("cur:%d\n",logicPulseCount);

    ACT_PUSH_TASK(act_S.ACT_L1A, pli, STAGE_PULSE_OFFSET.L1A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L1A, pli, STAGE_PULSE_OFFSET.L1A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, STAGE_PULSE_OFFSET.CAM1_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM1, pli, STAGE_PULSE_OFFSET.CAM1_off, 0, );


    ACT_PUSH_TASK(act_S.ACT_L2A, pli, STAGE_PULSE_OFFSET.L2A_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_L2A, pli, STAGE_PULSE_OFFSET.L2A_off, 0, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, STAGE_PULSE_OFFSET.CAM2_on, 1, );
    ACT_PUSH_TASK(act_S.ACT_CAM2, pli, STAGE_PULSE_OFFSET.CAM2_off, 0, );

    ACT_PUSH_TASK(act_S.ACT_SWITCH, pli,STAGE_PULSE_OFFSET.SWITCH, 0, );
    return 0;
    // pli->insp_status=insp_status_OK;
  }
  return -1;
}





int Run_ACTS(uint32_t cur_pulse)
{
  bool time_us_fetched=false;
  uint64_t time_us=0;
  ActQueues *acts= &act_S; // updated type from extracted Scheduler module
  // static uint32_t pre_pulse=0;

  // uint32_t diff = cur_pulse-pre_pulse;
  // if(diff!=1)
  // {
  //   DEBUG_printf("pre_pulse:%d ",pre_pulse);
  //   DEBUG_printf("cur_pulse:%d \n",cur_pulse);
  // }
  // pre_pulse=cur_pulse;

  GEN_ERROR_CODE ecode=GEN_ERROR_CODE::NOP;

  ACT_TRY_RUN_TASK(acts->ACT_L1A, cur_pulse,
                   if(task->info)
                   {
                    GPIOLS32_SET(PIN_O_L1A);
                   }
                   else 
                   {         
                    GPIOLS32_CLR(PIN_O_L1A);
                   }
                   
                   
                   );






  ACT_TRY_RUN_TASK(acts->ACT_CAM1, cur_pulse,

                  if(task->info)
                  {

                    GPIOLS32_SET(PIN_O_CAM1);
                    if(time_us_fetched==false)
                    {
                      time_us=esp_timer_get_time();
                      time_us_fetched=true;
                    }
                    Message msg = Message::createBriefTriggerInfo(1, time_us, task->src->tid);
                    if(!MessageBus::sendMessage(msg))
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    GPIOLS32_CLR(PIN_O_CAM1);
                  }
                   

                   );


  ACT_TRY_RUN_TASK(acts->ACT_L2A, cur_pulse,
                   if(task->info)
                   {
                    GPIOLS32_SET(PIN_O_L2A);
                   }
                   else 
                   {         
                    GPIOLS32_CLR(PIN_O_L2A);
                   }
                    );


  ACT_TRY_RUN_TASK(acts->ACT_CAM2, cur_pulse,

                  if(task->info)
                  {

                    GPIOLS32_SET(PIN_O_CAM2);
                    if(time_us_fetched==false)
                    {
                      time_us=esp_timer_get_time();
                      time_us_fetched=true;
                    }
                    Message msg = Message::createBriefTriggerInfo(2, time_us, task->src->tid);
                    if(!MessageBus::sendMessage(msg))
                    {
                      ecode=GEN_ERROR_CODE::INSP_CAM_TRIG_INFO_CANNOT_BE_SENT;
                    }
                  }
                  else
                  {
                    GPIOLS32_CLR(PIN_O_CAM2);
                  }
                   
                   
                   
                   
                   );



  ACT_TRY_RUN_TASK(
      acts->ACT_SWITCH, cur_pulse,

      // DEBUG_printf("SW src:%p tp:%d info:%d\n",task->src,task->targetPulse,task->info);

      pipeLineInfo *pli = task->src;
      // DEBUG_print("insp_status:");
      // DEBUG_println(pli->insp_status);

      switch (pli->insp_status)
      {
        case 1:
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, STAGE_PULSE_OFFSET.SEL1_on, 1, _task_->src =NULL;);//the src will be cleaned up right after
          ACT_PUSH_TASK(act_S.ACT_SEL1, pli, STAGE_PULSE_OFFSET.SEL1_off, 0, _task_->src =NULL; );
          break;
        case 2:
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_on, 1, _task_->src =NULL; );
          ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_off, 0, _task_->src =NULL; );
          break;
        case 3:
          SEL3_Count++;
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_on, 1, _task_->src =NULL; );
          // ACT_PUSH_TASK(act_S.ACT_SEL2, pli, STAGE_PULSE_OFFSET.SEL2_off, 0, _task_->src =NULL; );
          break;
        case 0xFFFF:
          NA_Count++;
          // inspResCount.NA++;
          break;

        case insp_status_SKIP: 
          SKIP_Count++;
          break;
        case insp_status_DEL: //ERROR
          break;

        case insp_status_UNSET:
        default:
          ecode=GEN_ERROR_CODE::OBJECT_HAS_NO_INSP_RESULT;
          
          break;
      }
      //
      
      {
        // task->src->insp_status = insp_status_DEL;
        task->src->insp_status = insp_status_DEL;
        task->src = NULL;
        // RBuf.consumeTail();
      }
  );



  ACT_TRY_RUN_TASK(acts->ACT_SEL1, cur_pulse,
                   if(task->info)
                   {

                    if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false && SEL1_ACT_COUNTDOWN)
                    {
                      if(SEL1_ACT_COUNTDOWN>0)SEL1_ACT_COUNTDOWN--;
                      SEL1_Count++;
                      GPIOLS32_SET(PIN_O_SEL1);
                    }
                   }
                   else 
                   {         
                    GPIOLS32_CLR(PIN_O_SEL1);
                   }
                  );


  ACT_TRY_RUN_TASK(acts->ACT_SEL2, cur_pulse,
                  
                  if(task->info)
                  {

                  if(SYS_FREQ_STABLE && SYS_STEPPER_DISABLED==false)
                  {
                    SEL2_Count++;
                    GPIOLS32_SET(PIN_O_SEL2);
                  }
                  }
                  else 
                  {         
                    GPIOLS32_CLR(PIN_O_SEL2);
                  }
                
                  
                  );


  if(ecode!=GEN_ERROR_CODE::NOP)
  {
    stateMachine.applyAction(SYS_STATE_ACT::INSPECTION_ERROR,(int)ecode);
  }

  return 0;
}





inline float mm2Pulse_conv(int axisIdx,float dist);

void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc);
bool doDataLog=false;
class MData_JR:public Data_JsonRaw_Layer
{
  
  public:
  MData_JR():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {
    doDataLog=false;
  } 
  int recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data=NULL,size_t dataL=0);
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  void connected(Data_Layer_IF* ch){}

  int send_data(int head_room,uint8_t *data,int len,int leg_room);
  void disconnected(Data_Layer_IF* ch){}

  int close(){}

  
  char dbgBuff[500];
  int dbg_printf(const char *fmt, ...);

  int msg_printf(const char *type,const char *fmt, ...);

  void loop();


};
MData_JR djrl;


int HACK_cur_cmd_id=-1;


void G_LOG(char* str)
{
  djrl.dbg_printf(str);
}


hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5
#define PIN_LED 2



//#define _HOMING_DBG_FLAG_ 50


int pin_SH_165=17;
int pin_TRIG_595=5;

#define SUBDIV (3200)
#define mm_PER_REV 95

enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1,
  INPUT_MON_CTRL=2,
  ON_TIME_REPLY=3,

};





struct MSTP_SegCtx_IOCTRL{
  uint32_t PORT=0,S=0;
  int32_t P=0,T=0;
};


struct MSTP_SegCtx_INPUTMON{
  uint32_t PINS,PIN_NS;
  uint32_t existField;
  bool doMonitor;
};


struct MSTP_SegCtx_OnTimeReply{
  int id;
  bool isAck;
};

struct MSTP_SegCtx{
  MSTP_SegCtx(){}
  ~MSTP_SegCtx(){}

  bool isProcessed;
  MSTP_SegCtx_TYPE type;
  union {
    struct MSTP_SegCtx_IOCTRL IO_CTRL;
    struct MSTP_SegCtx_INPUTMON INPUT_MON;
    struct MSTP_SegCtx_OnTimeReply ON_TIME_REP;
  }; 
  string CID;
  string TTAG;
  int TID;
};


const int SegCtxSize=40;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,SegCtxSize);





#define _TICK2SEC_BASE_ (10*1000*1000)







extern void __digitalWrite(uint8_t pin, uint8_t val)
{
    if(val) {
        if(pin < 32) {
            GPIO.out_w1ts = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
        }
    } else {
        if(pin < 32) {
            GPIO.out_w1tc = ((uint32_t)1 << pin);
        } else if(pin < 34) {
            GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
        }
    }
}





uint32_t SYS_STEP_COUNT=0;

// Global instances
GateSensor gateSensor;
StateMachine stateMachine;

// GateInfo struct and gateInfo variable removed - now using GateSensor class




// Gate sensor configuration parameters (used for JSON config and GateSensor initialization)
int minWidth = 0;
int maxWidth = 1000; // 1+40000/_PLAT_DIST_um_PER_STEP;

// GateSensing2 function removed - now using GateSensor class




// GateSensing function removed - now using GateSensor class




int stepRun=-1;
void StepGo()
{

  if((SYS_STEP_COUNT&1)==0)
  {
    // if(stepRun)
    // {
    //   if(stepRun>0)stepRun--;
    //   GPIOLS32_SET(STEPPER_PLS_PIN);
    // }
    GPIOLS32_SET(STEPPER_PLS_PIN);
  }
  else
  {
    GPIOLS32_CLR(STEPPER_PLS_PIN);
  }


  

}



void IRAM_ATTR onTimer()
{

  // static uint32_t cp0_regs[18];
  // GPIOLS32_SET(PIN_LED);

  // enable FPU
  // xthal_set_cpenable(1);
  // // Save FPU registers
  // xthal_save_cp0(cp0_regs);
  // uint32_t nextT=100;
  // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);
  



  SYS_STEP_COUNT++;

  //Step adv
  StepGo();



  gateSensor.tick(SYS_STEP_COUNT);

  Run_ACTS(SYS_STEP_COUNT);

  //sensor detection
  //Try run task


  
  // Restore FPU
  // xthal_restore_cp0(cp0_regs);
  // // and turn it back off
  // xthal_set_cpenable(0);
  // 
  // GPIOLS32_CLR(PIN_LED);

}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;


StaticJsonDocument <1024>doc;
StaticJsonDocument <1024>retdoc;



bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK);

int MData_JR::recv_ERROR(ERROR_TYPE errorcode,uint8_t *recv_data,size_t dataL)
{
  for(int i=0;i<buffIdx;i++)
  {
    if(dataBuff[i]=='"')
      dataBuff[i]='\'';
  }  
  dataBuff[buffIdx]='\0';
  // doDataLog=true;

  if(recv_data)
    dbg_printf("recv_ERROR:%d %s dat:%s",errorcode,dataBuff,string((char*)recv_data,0,9).c_str());
  else 
    dbg_printf("recv_ERROR:%d %s",errorcode,dataBuff);
}

int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode!=1)
  {
    return -1;
  }


  doc.clear();
  retdoc.clear();
  DeserializationError error = deserializeJson(doc, raw);
  bool rspAck=false;
  bool doRsp=false;

  const char* type = doc["type"];
  HACK_cur_cmd_id=-1;
  if(doc["id"].is<int>()==true)
  {
    HACK_cur_cmd_id=doc["id"];
  }
  // const char* id = doc["id"];
  if(strcmp(type,"RESET")==0)
  {
    return msg_printf("RESET_OK","");
  }
  else if(strcmp(type,"ask_JsonRaw_version")==0)
  {
    
    const char* _version = doc["version"];
    if(_version)
    {
      strcpy(peerVERSION,_version);
    }
    return this->rsp_JsonRaw_version();
  }
  // else if(strcmp(type,"rsp_JsonRaw_version")==0)
  // {
  //   const char* _version = doc["version"];
  //   strcpy(peerVERSION,_version);
  //   return 0;
  // }


  else if(strcmp(type,"trigCamPulse")==0)
  {
    int cam_PIN=PIN_O_CAM1;
    if(doc["cpin"].is<int>()==true)
    {
      cam_PIN=doc["cpin"];
    }
    int light_PIN=PIN_O_L1A;
    if(doc["lpin"].is<int>()==true)
    {
      light_PIN=doc["lpin"];
    }
    int Light_Delay=100;
    if(doc["light_delay"].is<int>()==true)
    {
      Light_Delay=doc["light_delay"];
    }


    int Light_Duration=100;
    if(doc["light_duration"].is<int>()==true)
    {
      Light_Duration=doc["light_duration"];
    }


    int trigger_id=924949;
    if(doc["trigger_id"].is<int>()==true)
    {
      trigger_id=doc["trigger_id"];
    }
    {
      uint32_t time_us=esp_timer_get_time();
      Message msg = Message::createBriefTriggerInfo(1, time_us, trigger_id);
      MessageBus::sendMessage(msg);
    }


    digitalWrite(cam_PIN,HIGH);
    delayMicroseconds(Light_Delay);
    digitalWrite(light_PIN,HIGH);
    delayMicroseconds(Light_Duration);
    digitalWrite(light_PIN,LOW);
    digitalWrite(cam_PIN,LOW);





    doRsp=rspAck=true;
  }
  else if(strcmp(type,"PIN_MODE")==0)
  {
    doRsp=true;
    int PIN_Mode=INPUT;
    if(doc["mode"].is<String>()==true)
    {
      String mode=doc["mode"];
      if(mode=="INPUT")
        PIN_Mode=INPUT;
      else if(mode=="OUTPUT")
        PIN_Mode=OUTPUT;
      else if(mode=="PULLUP")
        PIN_Mode=PULLUP;
      else if(mode=="PULLDOWN")
        PIN_Mode=PULLDOWN;
      else if(mode=="INPUT_PULLUP")
        PIN_Mode=INPUT_PULLUP;
      else if(mode=="INPUT_PULLDOWN")
        PIN_Mode=INPUT_PULLDOWN;
      else if(mode=="OPEN_DRAIN")
        PIN_Mode=OPEN_DRAIN;
    }


    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      pinMode(pin,PIN_Mode);
      rspAck=true;
    }
    else
    {
      rspAck=false;
    }
  }


  else if(strcmp(type,"PING")==0)
  {
    retdoc["type"]="PONG"; 
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"get_setup")==0)
  {
    retdoc["ver"]="0.0.0 Alpha";
    retdoc["name"]="uInspESP32";
    
    genMachineSetup(retdoc);

    
    doRsp=rspAck=true;

  }
  else if(strcmp(type,"set_setup")==0)
  {
    retdoc["type"]="set_setup";
    
    setMachineSetup(doc);
    doRsp=rspAck=true;

  }
  else if(strcmp(type,"reset_running_stat")==0)
  {

    SEL1_Count=SEL2_Count=SEL3_Count=NA_Count=0;

    doRsp=rspAck=true;

  }
  else if(strcmp(type,"get_running_stat")==0)
  {

    {
      JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");
      Diagnostics::exportErrorsToJson(jERROR_HIST);
    }


    JsonObject jCountInfo  = retdoc.createNestedObject("count");
    jCountInfo["SEL1"]=SEL1_Count;
    jCountInfo["SEL2"]=SEL2_Count;
    jCountInfo["SEL3"]=SEL3_Count;
    jCountInfo["NA"]=NA_Count;

    //current state
    retdoc["state"]=(int)stateMachine.currentState();

    retdoc["plateFreq"]=SYS_TAR_FREQ;//SYS_CUR_FREQ;
    // if(SEL1_ACT_COUNTDOWN>=0)
    // {
    // }
    retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;

    // retdoc["plateFreq"]=NA_Count;


    doRsp=rspAck=true;

  }
  else if(strcmp(type,"report")==0)
  {
    int tid=(doc["tid"].is<int>()==true)?doc["tid"]:-1;
    int cat=(doc["cat"].is<int>()==true)?doc["cat"]:-1;

    pipeLineInfo *tarP=NULL;

    if(tid!=-1 && cat!=-1)
    for (int i = 0; i < RBuf.size(); i++)
    {
      pipeLineInfo *pipe = RBuf.getTail(i);
      if (pipe == NULL)
        break;

      if(pipe->tid==tid)
      {
        tarP=pipe;
        break;
      }
      if(pipe->insp_status==insp_status_UNSET)
      {
        pipe->insp_status=insp_status_SKIP;
      }

      // if(i>30)
      // {//only check the last 30 info in the queue
      //   break;
      // }
    }

    if(tarP)
    {
      uint32_t pressure=tarP->gate_pulse+STAGE_PULSE_OFFSET.SWITCH-SYS_STEP_COUNT;
      // if(pressure<1000)
      // {
      //   SETUP_TAR_FREQ=SETUP_TAR_FREQ*19/20;
      // }
      retdoc["tr"]=pressure;
      tarP->insp_status=cat;
      rspAck=true;
    }
    else
    {
      stateMachine.applyAction(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::INSP_RESULT_MATCHES_NO_OBJECT);
      rspAck=false;
    }







    doRsp=false;

  }

  else if(strcmp(type,"clear_error")==0)
  {
    RESET_ALL_PIPELINE_QUEUE(); 

    stateMachine.applyAction(SYS_STATE_ACT::INSPECTION_ERROR_REDEEM);

    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"clear_error_history")==0)
  {
    Diagnostics::clearErrors();
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"PIN_ON")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];
      digitalWrite(pin,HIGH);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"PIN_OFF")==0)
  {
    
    if(doc["pin"].is<int>()==true)
    {
      int pin=doc["pin"];

      digitalWrite(pin,LOW);
    }
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"enter_insp_mode")==0)
  {

    stateMachine.applyAction(SYS_STATE_ACT::PREPARE_TO_ENTER_INSPECTION_MODE);
    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"exit_insp_mode")==0)
  {

    stateMachine.applyAction(SYS_STATE_ACT::EXIT_INSPECTION_MODE);
    
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"trig_phamton_pulse")==0)
  {
    uint32_t tatPulse= SYS_STEP_COUNT-STAGE_PULSE_OFFSET.L1A_on+_PLAT_DIST_step(3000);

    newPulseEvent(tatPulse-10, tatPulse+10, tatPulse,20);
    
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"sel1_act_countdown")==0 || strcmp(type,"set_sel1_cd")==0)
  {
    
    if(doc["count"].is<int>()==true)
    {
      SEL1_ACT_COUNTDOWN=doc["count"];
    }
    else
    {
      SEL1_ACT_COUNTDOWN=0;
    }
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"get_sel1_cd")==0)
  {
    retdoc["sel1_cd"]=SEL1_ACT_COUNTDOWN;
    doRsp=rspAck=true;
  }

  else if(strcmp(type,"stepper_enable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,STEPPER_EN_ACTIVATION);
    SYS_STEPPER_DISABLED=false;
    doRsp=rspAck=true;
  }
  else if(strcmp(type,"stepper_disable")==0)
  {
    digitalWrite(STEPPER_EN_PIN,!STEPPER_EN_ACTIVATION);
    SYS_STEPPER_DISABLED=true;
    doRsp=rspAck=true;
  }



  else if(strcmp(type,"sel_act")==0)
  {
    int idx=doc["idx"];
    int delay_ms=10;

    if(doc["delay"].is<int>()==true)
    {
      delay_ms=doc["delay"];
    }

    switch(idx)
    {
      case 1:
      digitalWrite(PIN_O_SEL1, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL1, 0);
      rspAck=true;
      break;
      case 2:
      digitalWrite(PIN_O_SEL2, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL2, 0);
      rspAck=true;
      break;
      case 3:
      digitalWrite(PIN_O_SEL3, 1);
      delay(delay_ms);
      digitalWrite(PIN_O_SEL3, 0);
      rspAck=true;
      break;
    }
    doRsp=true;
  }
  
  else if(strcmp(type,"BYE")==0)
  {
    doRsp=rspAck=true;

  }      
  else if(AUX_Task_Try_Read(doc,type,retdoc,doRsp,rspAck))
  {
  }


  if(doRsp)
  {
    retdoc["id"]=doc["id"];
    retdoc["ack"]=rspAck;
    
    uint8_t buff[1024];
    int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
    send_json_string(0,buff,slen,0);
  }
  return 0;
}
int MData_JR::send_data(int head_room,uint8_t *data,int len,int leg_room){
  Serial.write(data,len);
  return 0;
}

int MData_JR::dbg_printf(const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"dbg\":\"");
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

int MData_JR::msg_printf(const char *type,const char *fmt, ...)
{
  char *str=dbgBuff;
  int restL=sizeof(dbgBuff);
  {//start head
    int len=sprintf(str,"{\"type\":\"%s\",\"data\":\"",type);
    str+=len;
    restL-=len;

  }

  {
    va_list aptr;
    int ret;
    va_start(aptr, fmt);
    ret = vsnprintf (str, restL-10, fmt, aptr);
    va_end(aptr); 
    str+=ret;
    restL-=ret;


  }
  {//end
    int len=sprintf(str,"\"}");
    str+=len;
    restL-=len;
  }

  return send_json_string(0,(uint8_t*)dbgBuff,str-dbgBuff,0);
}

void MData_JR::loop()
{
}

#define AUX_COUNT 5

enum AUX_TASK_INFO_TYPE{
  AUX_DELAY=1,
  AUX_IO_CTRL=2,
  AUX_WAIT_FOR_ENC=3,
  AUX_WAIT_FOR_FINISH=1000,

};


struct AUX_TASK_INFO_WAIT_FOR_FINISH{
  int cmd_id;

};

struct AUX_TASK_INFO_WAIT_FOR_ENC{
  int value;

};
struct AUX_TASK_INFO_DELAY{
  int time;

};
struct AUX_TASK_INFO_IO_CTRL{
  
  int pin;
  int state;

  char CID[50];
  char TTAG[100];
  int TID;


};

struct AUX_TASK_INFO {
  AUX_TASK_INFO(){}
  ~AUX_TASK_INFO(){}
  AUX_TASK_INFO_TYPE type;
  


  union {
    AUX_TASK_INFO_DELAY delayInfo;
    AUX_TASK_INFO_IO_CTRL ioCtrl;
    AUX_TASK_INFO_WAIT_FOR_ENC wait_enc;
    AUX_TASK_INFO_WAIT_FOR_FINISH wait_fin;
  }; 

  //Just for ioCtrl
  // string CID;
  // string TTAG;
};

static QueueHandle_t AUXTaskQueue[AUX_COUNT];

bool AUX_Task_Try_Read(JsonDocument& data,const char* type,JsonDocument& ret_doc, bool &doRsp,bool &isACK)
{
  int AUX_THREAD_ID=(doc["aid"].is<int>())?doc["aid"]:0;
  if(AUX_THREAD_ID>=AUX_COUNT)
  {
    return false;
  }
  if(strcmp(type,"AUX_TEST")==0)
  {
    ret_doc["msg"]="Try more";
    doRsp=true;
    isACK=false;
    return true;
  }

  return false;
}



// AUX2CommInfoQ now managed by MessageBus module
static SemaphoreHandle_t AUX2Comm_Lock;

void AUX_task(void *pvParameter)
{
  QueueHandle_t &Q=*(QueueHandle_t *)pvParameter;
    while(1) {
      AUX_TASK_INFO info; 
      if (xQueueReceive(Q, (void *)&info, portMAX_DELAY) == pdTRUE) {

        switch(info.type)
        {

          case AUX_TASK_INFO_TYPE::AUX_DELAY:
            vTaskDelay(info.delayInfo.time / portTICK_RATE_MS);
            // G_LOG(">>>>");
          break;
          // case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_ENC :
          //   while(mstp.EncV<info.wait_enc.value)
          //   {
          //     vTaskDelay(1 / portTICK_RATE_MS);
          //   }

          // break;

          case AUX_TASK_INFO_TYPE::AUX_IO_CTRL :

            if(info.ioCtrl.CID[0])
            {
              //send camera idx 
              Message msg = Message::createTriggerInfo("", "", 0, info.ioCtrl.TID);
              MessageBus::sendAuxMessage(msg);
            

            }
            // if(info.ioCtrl.state==1)
            // {
            //   mstp.static_Pin_info|=(uint32_t)1<<info.ioCtrl.pin;
            // }
            // if(info.ioCtrl.state==0)
            // {
            //   mstp.static_Pin_info&=~(((uint32_t)1)<<info.ioCtrl.pin);
            // }
          break;


          case AUX_TASK_INFO_TYPE::AUX_WAIT_FOR_FINISH :

              Message msg = Message::createResponseFrame(true, info.wait_fin.cmd_id);
              MessageBus::sendAuxMessage(msg);
          break;
        }
      }
    }
}

//float 100 add,sub 5.5us
//float 100 mult,div 24us
//float 100 sin 48us

//int 100 add,mult,div 5.5us
//int 100 div 5.8us


int rzERROR=0;
void setup()
{
  
  // noInterrupts();
  Serial.begin(115200);//230400);
  // Serial.begin(460800);
  Serial.setRxBufferSize(500);
  // Serial.setHwFlowCtrlMode(0);
  // // setup_comm();
  timer = timerBegin(0, 80*1000*1000/_TICK2SEC_BASE_, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 7000, true);
  // timerAlarmEnable(timer);
  timerAlarmDisable(timer);

  // Initialize new modules
  MessageBus::init();
  Diagnostics::init();

  AUX2Comm_Lock = xSemaphoreCreateMutex();
  for(int i=0;i<AUX_COUNT;i++)
  {
    AUXTaskQueue[i] = xQueueCreate(20 /* Number of queue slots */, sizeof(AUX_TASK_INFO));
    xTaskCreatePinnedToCore(&AUX_task, "AUX_task", 2048, (void*)&AUXTaskQueue[i], 1, NULL, 0);

  }

  pinMode(PIN_LED, OUTPUT);


  pinMode(STEPPER_PLS_PIN, OUTPUT);
  pinMode(STEPPER_DIR_PIN, OUTPUT);
  pinMode(STEPPER_EN_PIN, OUTPUT);

  digitalWrite(STEPPER_EN_PIN,STEPPER_EN_ACTIVATION);
  SYS_STEPPER_DISABLED=false;
  



  pinMode(PIN_O_L1A, OUTPUT);
  pinMode(PIN_O_CAM1, OUTPUT);

  pinMode(PIN_O_L2A, OUTPUT);
  pinMode(PIN_O_CAM2, OUTPUT);


  pinMode(PIN_O_SEL1, OUTPUT);
  pinMode(PIN_O_SEL2, OUTPUT);
  pinMode(PIN_O_SEL3, OUTPUT);

  pinMode(PIN_I_GATE, INPUT_PULLUP);

  // Initialize GateSensor
  gateSensor.init(PIN_I_GATE, true, minWidth, maxWidth, 1); // pin, inverted, min_width, max_width, debounce_threshold
  gateSensor.setObjectDetectedCallback([](uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width) {
    newPulseEvent(start_pulse, end_pulse, middle_pulse, pulse_width);
  });

  // Initialize StateMachine
  stateMachine.init(SYS_STATE::INIT);

  // CameraIDList[0]="ABC";
  // CameraIDList[1]="DEF";



  stateMachine.applyAction(SYS_STATE_ACT::INIT_OK);
}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    yield();
  }
}

MSTP_SegCtx ctx[10];


string toFixed(float num,int powNum=100)
{
  int ipnum=round(num*powNum);
  int inum=ipnum/powNum;

  string istr=std::to_string(inum);
  int pnum=(ipnum%powNum);

  string pstr=std::to_string(pnum+powNum);

  string resStr=istr+pstr;
  resStr[istr.length()]='.';
  return resStr;
}


bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}


static uint8_t recvBuf[20];
void loop()
{

  stateMachine.applyAction(SYS_STATE_ACT::NOP);
  djrl.loop();
  {
    bool recvF=false;
    while(Serial.available() > 0) {
      recvF=true;
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.read(recvBuf,sizeof(recvBuf-1));
      //
      if(recvLen<=0)continue;
      // djrl.dbg_printf("recvLen:%d",recvLen);
      djrl.recv_data((uint8_t*)recvBuf,recvLen);
      if(doDataLog)
      {
        for(int i=0;i<recvLen;i++) 
        {
          if(recvBuf[i]=='"')
            recvBuf[i]='\'';
        }     
        recvBuf[recvLen]='\0';
        djrl.dbg_printf(">%s",recvBuf);
      }

    }
    if(recvF)
    {
      // djrl.dbg_printf("recv DONE");
    }
  }


  {
    uint8_t buff[700];
    while(1)
    {
      bool hasNewInfo=false;
      Message info;
      if(hasNewInfo ==false && MessageBus::getNextMainMessage(info))
      {
        hasNewInfo=true;
      }

      if(hasNewInfo ==false && MessageBus::getNextAuxMessage(info))
      {
        hasNewInfo=true;
      }




      if(hasNewInfo==false)break;

      retdoc.clear();
      // retdoc["tag"]="s_Step_"+std::to_string((int)info.step);
      // retdoc["trigger_id"]=info.step;
      switch (info.type)
      {
        case MessageType::TRIGGER_INFO :
        {
          retdoc["type"]="TriggerInfo"; 
          retdoc["camera_id"]=info.camera_id;


          string tag = info.trig_tag;
          // if(info.curFreq==info.curFreq)
          //   replace(tag,"$s_PFQ", "s_PFQ="+toFixed(info.curFreq,100));

          retdoc["tag"]=tag;
          retdoc["trigger_id"]=info.trig_id;



          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }
        
        case MessageType::BRIEF_TRIGGER_INFO :
        {
          retdoc["type"]="bT"; 
          retdoc["tidx"]=info.btrig_idx;
          retdoc["usH"]=info.trig_time_us>>32;
          retdoc["usL"]=info.trig_time_us&((uint32_t)0-1);

          retdoc["tid"]=info.trig_id;
          retdoc["Qs"]=RBuf.size();
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case MessageType::SYSTEM_INFO :
        {
          retdoc["type"]="systemInfo"; 

          retdoc["state"]=(int)stateMachine.currentState();
          

          {
            JsonArray jERROR_HIST = retdoc.createNestedArray("ERROR_HIST");
            Diagnostics::exportErrorsToJson(jERROR_HIST);
          }


          retdoc["log"]=info.log;


          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }

        case MessageType::EXT_LOG :
        {

          djrl.dbg_printf("%s",info.log.c_str());

          break;
        }
      
        case MessageType::RESP_FRAME :
        {


          retdoc["id"]=info.resp_id;
          retdoc["ack"]=info.isAck;
          
          int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
          djrl.send_json_string(0,buff,slen,0);
          break;
        }
      }
    }
  }



  static int subDiv=0;
  do{//timer freq ctrl
    subDiv=(subDiv+1)&(0xFF);
    if(subDiv!=0)break;
    if(SYS_CUR_FREQ==SYS_TAR_FREQ)
    {
      if(SYS_TAR_FREQ==0 && SYS_FREQ_STABLE==false)//just stable
      {

        GPIOLS32_CLR(PIN_O_L1A);
        GPIOLS32_CLR(PIN_O_L2A);
        GPIOLS32_CLR(PIN_O_SEL1);
        GPIOLS32_CLR(PIN_O_SEL2);
        GPIOLS32_CLR(PIN_O_SEL3);
      }
      SYS_FREQ_STABLE=true;
      break;
    }
    SYS_FREQ_STABLE=false;
    bool TimerNeedsStart=false;
    if(SYS_CUR_FREQ==0)
    {
      TimerNeedsStart=true;
    }
    if(SYS_CUR_FREQ>SYS_TAR_FREQ)
    {
      if(SYS_TAR_FREQ==0 && SYS_CUR_FREQ<10)
      {
        SYS_CUR_FREQ=0;
      }
      else
      {
        SYS_CUR_FREQ-=SYS_FREQ_ADV_STEP;
        if(SYS_CUR_FREQ<SYS_TAR_FREQ)
        {
          SYS_CUR_FREQ=SYS_TAR_FREQ;
        }
      }
    }
    else
    {
      SYS_CUR_FREQ+=SYS_FREQ_ADV_STEP;
      if(SYS_CUR_FREQ>SYS_TAR_FREQ)
      {
        SYS_CUR_FREQ=SYS_TAR_FREQ;
      }
    }



    if(SYS_CUR_FREQ==0)
    {
      timerAlarmDisable(timer);
    }
    else
    {
      timerAlarmWrite(timer, (uint64_t)((_TICK2SEC_BASE_>>1)/SYS_CUR_FREQ), true);
    }

    if(TimerNeedsStart)
    {
      timerAlarmEnable(timer);
    }


  }while(0);
  // static unsigned long startMillis=0; 
  // unsigned long currentMillis = millis();  //get the current "time" (actually the number of milliseconds since the program started)
  // if (currentMillis - startMillis >= 100)  //test whether the period has elapsed
  // {
  //   startMillis = currentMillis;  //IMPORTANT to save the start time of the current LED state.

  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>24));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>16));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins>>8));
  //   Serial.printf(PRTF_B2b_PAT,PRTF_B2b(mstp.latest_input_pins));
  //   Serial.printf("\n");
  // }



  {//clean up finished 
    pipeLineInfo * tail;
    while (tail=RBuf.getTail())
    {
      // task->src->insp_status = insp_status_DEL;
      if(tail->insp_status == insp_status_DEL)
      {
        RBuf.consumeTail();
      }
      else
      {
        break;
      }
    }
  }

  // {
  //   if(SEL1_ACT_COUNTDOWN==0)
  //   {
      
  //     SYS_STATE_Transfer(SYS_STATE_ACT::INSPECTION_ERROR,(int)GEN_ERROR_CODE::SEL_ACT_LIMIT_REACHES);
  //   }
  // }
}





int intArrayContent_ToJson(char *jbuff, uint32_t jbuffL, int16_t *intarray, int intarrayL)
{
  uint32_t MessageL = 0;

  for (int i = 0; i < intarrayL; i++)
    MessageL += sprintf((char *)jbuff + MessageL, "%d,", intarray[i]);
  MessageL--; //remove the last comma',';

  return MessageL;
}


void genMachineSetup(JsonDocument &jdoc)
{

  // jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  JsonObject jSPO  = jdoc.createNestedObject("stage_pulse_offset");
  jSPO["L1A_on"]=STAGE_PULSE_OFFSET.L1A_on;
  jSPO["L1A_off"]=STAGE_PULSE_OFFSET.L1A_off;
  jSPO["CAM1_on"]=STAGE_PULSE_OFFSET.CAM1_on;
  jSPO["CAM1_off"]=STAGE_PULSE_OFFSET.CAM1_off;

  jSPO["L2A_on"]=STAGE_PULSE_OFFSET.L2A_on;
  jSPO["L2A_off"]=STAGE_PULSE_OFFSET.L2A_off;
  jSPO["CAM2_on"]=STAGE_PULSE_OFFSET.CAM2_on;
  jSPO["CAM2_off"]=STAGE_PULSE_OFFSET.CAM2_off;

  jSPO["SWITCH"]=STAGE_PULSE_OFFSET.SWITCH;
  
  jSPO["SEL1_on"]=STAGE_PULSE_OFFSET.SEL1_on;
  jSPO["SEL1_off"]=STAGE_PULSE_OFFSET.SEL1_off;
  jSPO["SEL2_on"]=STAGE_PULSE_OFFSET.SEL2_on;
  jSPO["SEL2_off"]=STAGE_PULSE_OFFSET.SEL2_off;
  jSPO["SEL3_on"]=STAGE_PULSE_OFFSET.SEL3_on;
  jSPO["SEL3_off"]=STAGE_PULSE_OFFSET.SEL3_off;



  // auto obj=jdoc.createNestedObject("obj");

  jdoc["plateFreq"]=SETUP_TAR_FREQ;
  jdoc["minDetectTimeSep_us"]=SYS_MIN_PULSE_TIME_SEP_us;


  {
    JsonArray jERROR_HIST = jdoc.createNestedArray("ERROR_HIST");
    Diagnostics::exportErrorsToJson(jERROR_HIST);
  }


  // jdoc["SYS_TAR_FREQ"]=SYS_TAR_FREQ;
  jdoc["curState"]=(int)stateMachine.currentState();
  jdoc["SYS_STEP_COUNT"]=(int)SYS_STEP_COUNT;

  
}




#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}


void setMachineSetup(JsonDocument &jdoc)
{
  if(jdoc["CAM1_ID"].is<const char*>()  )
  { 
    CAM1_ID=jdoc["CAM1_ID"].as<const char*>();
  }

  if(jdoc["CAM2_ID"].is<const char*>()  )
  { 
    CAM2_ID=jdoc["CAM2_ID"].as<const char*>();
  }

  if(jdoc["CAM1_Tags"].is<const char*>()  )
  { 
    CAM1_Tags=jdoc["CAM1_Tags"].as<const char*>();
  }

  if(jdoc["CAM2_Tags"].is<const char*>()  )
  { 
    CAM2_Tags=jdoc["CAM2_Tags"].as<const char*>();
  }
  

  JSON_SETIF_ABLE(SETUP_TAR_FREQ,jdoc,"plateFreq");
  JSON_SETIF_ABLE(SYS_MIN_PULSE_TIME_SEP_us,jdoc,"minDetectTimeSep_us");
  JSON_SETIF_ABLE(stepRun,jdoc,"stepRun");

  JSON_SETIF_ABLE(minWidth,jdoc,"pulse_minWidth");
  JSON_SETIF_ABLE(maxWidth,jdoc,"pulse_maxWidth");
  

  if (jdoc.containsKey("stage_pulse_offset")) {
    JsonObject jSPO  = jdoc["stage_pulse_offset"];
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L1A_on,jSPO,"L1A_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L1A_off,jSPO,"L1A_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM1_on,jSPO,"CAM1_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM1_off,jSPO,"CAM1_off");


    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L2A_on,jSPO,"L2A_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.L2A_off,jSPO,"L2A_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM2_on,jSPO,"CAM2_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.CAM2_off,jSPO,"CAM2_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SWITCH,jSPO,"SWITCH");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL1_on,jSPO,"SEL1_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL1_off,jSPO,"SEL1_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL2_on,jSPO,"SEL2_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL2_off,jSPO,"SEL2_off");

    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL3_on,jSPO,"SEL3_on");
    JSON_SETIF_ABLE(STAGE_PULSE_OFFSET.SEL3_off,jSPO,"SEL3_off");

  }

  // Update GateSensor configuration if minWidth or maxWidth changed
  gateSensor.init(PIN_I_GATE, true, minWidth, maxWidth, 1);

}





