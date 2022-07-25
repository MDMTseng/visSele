
#include "main.hpp"
#include "MSteppers.hpp"
#include "GCodeParser_M.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include <Data_Layer_Protocol.hpp>


extern "C" {
#include "direct_spi.h"
}

#pragma once
#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)



hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5
#define PIN_LED 2



// #define _ZEROING_DBG_FLAG_

#define PIN_DBG 14
#define PIN_DBG2 27


int pin_SH_165=17;
int pin_TRIG_595=PIN_NUM_CS;



#define SUBDIV (800)
#define mm_PER_REV 10


spi_device_handle_t spi1=NULL;

struct MSTP_SegCtx{
  int type;
  // int delay_time_ms;
  int d0;
  int d1;

  int32_t I,P,S,T;
};


bool doDataLog=false;
const int SegCtxSize=40;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,sizeof(resbuff)/sizeof(resbuff[0]));


class MStp_M:public MStp{
  public:

  int FACCT=0;
  

  int POut1=0;




  MStp_M(MSTP_segment *buffer, int bufferL):MStp(buffer,bufferL)
  {
    
    TICK2SEC_BASE=10*1000*1000;
    main_acc=SUBDIV*2000/mm_PER_REV;//SUBDIV*3200/mm_PER_REV;
    minSpeed=sqrt(main_acc);//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    main_junctionMaxSpeedJump=minSpeed;//5200;

    maxSpeedInc=minSpeed;
    // pinMode(PIN_Z1_DIR, OUTPUT);
    // pinMode(PIN_Z1_STP, OUTPUT);
    // pinMode(PIN_Z1_SEN1, INPUT);
    // pinMode(PIN_Z1_SEN2, INPUT);
    // pinMode(PIN_Y_SEN1, INPUT);

    // pinMode(PIN_Y_DIR, OUTPUT);
    // pinMode(PIN_Y_STP, OUTPUT);


    // pinMode(PIN_R11_DIR, OUTPUT);
    // pinMode(PIN_R11_STP, OUTPUT);
    // pinMode(PIN_R11_SEN1, INPUT);
    // pinMode(PIN_R12_DIR, OUTPUT);
    // pinMode(PIN_R12_STP, OUTPUT);


    // pinMode(PIN_OUT_1, OUTPUT);    
    // pinMode(PIN_DBG, OUTPUT);    

    int general_max_freq=20000;
    axisInfo[AXIS_IDX_X].VirtualStep=1;
    axisInfo[AXIS_IDX_X].AccW=1;
    axisInfo[AXIS_IDX_X].MaxSpeedJumpW=1;
    axisInfo[AXIS_IDX_X].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_Y].VirtualStep=1;
    axisInfo[AXIS_IDX_Y].AccW=1;
    axisInfo[AXIS_IDX_Y].MaxSpeedJumpW=1;
    axisInfo[AXIS_IDX_Y].MaxSpeed=general_max_freq;

    auto mainAXIS_VSTEP=axisInfo[AXIS_IDX_Y].VirtualStep;







    axisInfo[AXIS_IDX_Z1].VirtualStep=1;
    axisInfo[AXIS_IDX_Z1].AccW=1;
    axisInfo[AXIS_IDX_Z1].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_Z1].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R1].VirtualStep=1;
    axisInfo[AXIS_IDX_R1].AccW=1;
    axisInfo[AXIS_IDX_R1].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_R1].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z2].VirtualStep=1;
    axisInfo[AXIS_IDX_Z2].AccW=1;
    axisInfo[AXIS_IDX_Z2].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_Z2].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R2].VirtualStep=1;
    axisInfo[AXIS_IDX_R2].AccW=1;
    axisInfo[AXIS_IDX_R2].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_R2].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z3].VirtualStep=1;
    axisInfo[AXIS_IDX_Z3].AccW=1;
    axisInfo[AXIS_IDX_Z3].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_Z3].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R3].VirtualStep=1;
    axisInfo[AXIS_IDX_R3].AccW=1;
    axisInfo[AXIS_IDX_R3].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_R3].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z4].VirtualStep=1;
    axisInfo[AXIS_IDX_Z4].AccW=1;
    axisInfo[AXIS_IDX_Z4].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_Z4].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R4].VirtualStep=1;
    axisInfo[AXIS_IDX_R4].AccW=1;
    axisInfo[AXIS_IDX_R4].MaxSpeedJumpW=1/mainAXIS_VSTEP;
    axisInfo[AXIS_IDX_R4].MaxSpeed=general_max_freq;


    
    // axisInfo[AXIS_IDX_R12].VirtualStep=3;
    // axisInfo[AXIS_IDX_R12].AccW=SUBDIV*1500/mm_PER_REV/main_acc/axisInfo[AXIS_IDX_Y].VirtualStep;
    // axisInfo[AXIS_IDX_R12].MaxSpeedJumpW=1/axisInfo[AXIS_IDX_Y].VirtualStep;
  
    doCheckHardLimit=false;

    
  }

  void INIT()
  {
    

    spi1= direct_spi_init(1,10*1000*1000,PIN_NUM_MOSI,PIN_NUM_MISO,PIN_NUM_CLK,PIN_NUM_CS);
    spi_device_select(spi1,1);

    ShiftRegAssign(0,0);
    ShiftRegUpdate();
  }


  xVec posWhenHit;

  int volatile runUntil_ExtPIN=-1;
  int volatile runUntil_sensorVal=0;


  void stopTimer(){
    
    if(timerRunning==true)
    {
      timerAlarmDisable(timer); 
      timerRunning=false;
    }
    // __PRT_I_(">\n");
  }
  void startTimer(){
    if(timerRunning==false)
    {
      timerAlarmEnable(timer);  
      // timerAlarmWrite(timer,1, true);
      timerRunning=true;
    }
  }
  void FatalError(int errorCode,const char* errorText)
  {
     __UPRT_D_("FATAL error:%d  %s\n",errorCode,errorText);
  }

  int _ZERO_DBG_COUNTER=0;
  int runUntil(int axis,int ext_pin,int pinVal,int distance,int speed,xVec *ret_posWhenHit)
  {
    runUntil_sensorVal=pinVal;

    StepperForceStop();
    __UPRT_D_("STP1-1\n");

    xVec cpos=(xVec){0};
    cpos.vec[axis]=distance;
    __UPRT_D_("STP1-2\n");
    runUntil_ExtPIN=ext_pin;
    VecAdd(cpos,speed);
    __UPRT_D_("STP1-3  pin:%d\n",runUntil_ExtPIN);
    int cccc=0;
    while(runUntil_ExtPIN!=-1 && SegQ_IsEmpty()==false)
    { 
      cccc++;
      if((cccc&0xFFFF)==0)
        __UPRT_D_("%d",digitalRead(runUntil_ExtPIN));
      else 
        Serial.printf("");
    }//wait for touch sensor
    
    __UPRT_D_("\nSTP1-3 res  pin:%d\n",runUntil_ExtPIN);

    // __UPRT_D_("ZeroStatus:%d blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(sensorPIN));
    if(runUntil_ExtPIN!=-1)
    {
      __UPRT_D_("\nFAIL:runUntil_ExtPIN:%d R:%d\n",runUntil_ExtPIN,digitalRead(runUntil_ExtPIN));
      runUntil_ExtPIN=-1;
      return -1;
    }

    if(ret_posWhenHit)
    {
      *ret_posWhenHit=posWhenHit;
    }

    return 0;

  }



  
  int Z1Info_Limit1=300;
  int Z1Info_Limit2=-300;
  int MachZeroRet(uint32_t axis_index,uint32_t sensor_pin,int distance,int speed,void* context)
  {

    switch(axis_index)
    {

      case AXIS_IDX_X:
      case AXIS_IDX_Y:
      case AXIS_IDX_Z1://rough
      case AXIS_IDX_R1:
      case AXIS_IDX_Z2://rough
      case AXIS_IDX_R2:
      case AXIS_IDX_Z3://rough
      case AXIS_IDX_R3:
      case AXIS_IDX_Z4://rough
      case AXIS_IDX_R4:
      {
        int sensorDetectVLvl=0;
        int runSpeed=speed;
        int axisIdx=axis_index;
        
        _ZERO_DBG_COUNTER=30;
        xVec retHitPos;
        if(runUntil(axisIdx,sensor_pin,sensorDetectVLvl,distance,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }
        _ZERO_DBG_COUNTER=30;

        delay(10);
        
        if(runUntil(axisIdx,sensor_pin,!sensorDetectVLvl,-distance/2,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }
        delay(10);
        curPos_c.vec[axisIdx]=0;//zero the Cur_pos
        lastTarLoc=curPos_c;
        break;
      }
    }

    return 0;
    // ZeroStatus=0;

  }

  bool runUntilDetected(uint32_t extInputPort)
  {
        // __UPRT_D_("ZeroStatus:%d blocks->size():%d\n",ZeroStatus,blocks->size());

    volatile int sensorRead=(extInputPort>>runUntil_ExtPIN)&1;

    
#ifdef _ZEROING_DBG_FLAG_
    if(_ZERO_DBG_COUNTER==0)
#else
    if(sensorRead==runUntil_sensorVal)//somehow digitalRead is not stable, to a doulbe check
#endif
    {
      StepperForceStop();
      posWhenHit=curPos_c;
      runUntil_ExtPIN=-1;
      return true;
    }

#ifdef _ZEROING_DBG_FLAG_
      _ZERO_DBG_COUNTER--;
#endif
    return false;
  }


  bool isIOCtrl(MSTP_SEG_PREFIX MSTP_segment* seg)
  {    
    if(seg==NULL ||seg->ctx==NULL )
    {
      return false;
    }
    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    return ctx->type==42;
  }

  bool static_Pin_update_needed=false;
  uint32_t static_Pin_info=0;
  int PIN_DBG_ST=0;
  void BlockInitEffect(MSTP_SEG_PREFIX MSTP_segment* seg)
  {
    if(isIOCtrl(seg)==false)
    {
      return;
    }
    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    switch(ctx->type)
    {
      case 42:
        if(ctx->P<0)break;
        //P: pin number, S: 0~255 PWM, T: pin setup (0:input, 1:output, 2:input_pullup, 3:input_pulldown)

        {
          static_Pin_update_needed=true;
          if(ctx->S==0)
          {
            static_Pin_info&=~(1<<ctx->P);
          }
          else
          {
            static_Pin_info|=(1<<ctx->P);
          }
        }



      break;
    }
  }

  void BlockEndEffect(MSTP_SEG_PREFIX MSTP_segment* seg,MSTP_SEG_PREFIX MSTP_segment* n_seg)
  {    


    if(static_Pin_update_needed)
    {//if no following segment to execute, then do IO update here
      

      if(isIOCtrl(n_seg)==false)
      {

        if(n_seg && n_seg->type== MSTP_segment_type::seg_line)
        {
          ShiftRegAssign(latest_dir_pins,latest_stp_pins);
          static_Pin_update_needed=false;
        }
        else
        {
          ShiftRegAssign(latest_dir_pins,latest_stp_pins);
          ShiftRegUpdate();
          static_Pin_update_needed=false;
        }

      }
    }



    if(seg==NULL)
    {
      return;
    }
    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    if(ctx==NULL )return;
    sctx_pool.returnResource(ctx);
  }

  void BlockDirEffect(uint32_t dir_idxes)
  {
    // pre_seg->ctx;//do sth... start
    
    // digitalWrite(PIN_OUT_1, POut1=(!POut1));
    // digitalWrite(PIN_Y_DIR,  (dir_idxes&(1<<AXIS_IDX_Y  ))!=0);
    // digitalWrite(PIN_Z1_DIR, (dir_idxes&(1<<AXIS_IDX_Z1 ))!=0);
    // digitalWrite(PIN_R11_DIR,(dir_idxes&(1<<AXIS_IDX_R11))!=0);
    // digitalWrite(PIN_R12_DIR,(dir_idxes&(1<<AXIS_IDX_R12))!=0);


    
    // __UPRT_D_("dir:%s \n",int2bin(idxes,MSTP_VEC_SIZE));
  }
    
  uint32_t _latest_stp_pins=0;//info in the register
  uint32_t _latest_dir_pins=0;
  uint32_t latest_stp_pins=0;//info that really on pins
  uint32_t latest_dir_pins=0;

  
  void ShiftRegAssign(uint32_t dir,uint32_t step)
  {
    _latest_stp_pins=step;
    _latest_dir_pins=dir;
    uint32_t portPins=(dir&0xFF)<<16 | (step & 0xFF)<<24|static_Pin_info<<0;
    // uint32_t portPins= (step & 0xFF)<<24| (dir&0x7)<<16 | static_Pin_info<<(16+3);

    spi1->host->hw->data_buf[0]=portPins;
    
    gpio_set_level((gpio_num_t) pin_SH_165, 1);//switch to keep in 165 register(stop 165 load pin to reg)
    gpio_set_level((gpio_num_t) pin_TRIG_595, 0);//
    direct_spi_transfer(spi1,32);
    //send_SPI(portPins);
  }
  

  void ShiftRegUpdate()
  {
    static_Pin_update_needed=false;//will
    while (direct_spi_in_use(spi1));//wait for SPI bus available
    gpio_set_level((gpio_num_t) pin_SH_165, 0);//switch to load(165 keeps load pin to internal reg)
    latest_input_pins=spi1->host->hw->data_buf[0];
    if(latest_input_pins&(1<<PIN_X_SEN1))
    {
      // gpio_set_level((gpio_num_t)PIN_LED, 1);
    }
    else
    {
    }
    if(runUntil_ExtPIN!=-1)
    {
      if(runUntilDetected(latest_input_pins)==true)//if reaches, do not let 595 update pins, to prevent further movement
        return;
    }

    gpio_set_level((gpio_num_t) pin_TRIG_595, 1);//trigger 595 internal register update to 959 phy pin
    
    latest_stp_pins=_latest_stp_pins;
    latest_dir_pins=_latest_dir_pins;
  }


  void BlockPinInfoUpdate(uint32_t dir,uint32_t idxes_T,uint32_t idxes_R)
  {
     ShiftRegAssign(dir,idxes_T);
  }
  
  
  uint32_t latest_input_pins=0;
  void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)
  {
    ShiftRegUpdate();
  }
};

#define MSTP_BLOCK_SIZE 40
static MSTP_segment blockBuff[MSTP_BLOCK_SIZE];

MStp_M mstp(blockBuff,MSTP_BLOCK_SIZE);




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
  int recv_ERROR(ERROR_TYPE errorcode);
  char gcodewait_gcode[100];
  int gcodewait_id=-1;
  

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





uint64_t SystemTick=0;
inline uint64_t getCurTick()
{
  return SystemTick+timerRead(timer);
}

uint32_t preCD=0;



bool isSystemZeroOK=false;
int timerCount=0;
uint32_t cp0_regs[18];
void IRAM_ATTR onTimer()
{
  gpio_set_level((gpio_num_t)PIN_LED,1);
  // enable FPU
  xthal_set_cpenable(1);
  // Save FPU registers
  xthal_save_cp0(cp0_regs);
  // uint32_t nextT=100;
  // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);
  

  uint32_t T=mstp.taskRun();
  // printf("T:%d\n",T);
  if(T>100000)
  {
    
    __UPRT_D_("ERROR:: T(%d)<0\n",T);
    T=100*10000;
    // return;;
  }
  

  if(T==0)
  {
    T=100*1000;
    
  }
  // if(timerT_TOP)
  // {
  //   T=timerT_TOP;
  // }
  // int64_t td=T-timerRead(timer);
  // if(td<50)td=50;


  timerAlarmWrite(timer,T, true);


  // Restore FPU
  xthal_restore_cp0(cp0_regs);
  // and turn it back off
  xthal_set_cpenable(0);
  // 
  gpio_set_level((gpio_num_t)PIN_LED,0);
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;


uint32_t xendpos=4700*SUBDIV/mm_PER_REV;

void vecToWait(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL)
{
  // digitalWrite(PIN_OUT_1,1);
  while(mstp.VecTo(VECTo,speed,ctx,exinfo)==false)
  {
    Serial.printf("");
  }
  
  // digitalWrite(PIN_OUT_1,0);
}


void addWaitWait(uint32_t period,int times=1,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL)
{
  // digitalWrite(PIN_OUT_1,1);
  while( mstp.AddWait(period,times,ctx,exinfo)==false)
  {
    Serial.printf("");
  }
  
  // digitalWrite(PIN_OUT_1,0);
}

class GCodeParser_M2:public GCodeParser_M
{
public:
  GCodeParser_M2(MStp *mstp):GCodeParser_M(mstp)
  {

  }

  float unit2Pulse_conv(int axisIdx,float dist)
  {
    // __UPRT_D_("unitConv[%s]:%f\n",code,dist);
    switch(axisIdx)
    {
      case AXIS_IDX_X:return unit2Pulse(1*dist,SUBDIV/mm_PER_REV);
      case AXIS_IDX_Y:return unit2Pulse(1*dist,SUBDIV/mm_PER_REV);//-1 for reverse the direction
      
      case AXIS_IDX_FEEDRATE:
      case AXIS_IDX_ACCELERATION:
      case AXIS_IDX_DEACCELERATION:
        return unit2Pulse(dist,SUBDIV/mm_PER_REV);


      case AXIS_IDX_Z1:
      case AXIS_IDX_Z2:
      case AXIS_IDX_Z3:
      case AXIS_IDX_Z4:return dist;//as pulse count

      case AXIS_IDX_R1:
      case AXIS_IDX_R2:
      case AXIS_IDX_R3:
      case AXIS_IDX_R4://assume it's 800 pulses pre rev
        return dist*12800/360;//-1 for reverse the direction




    }

    return NAN;
  }


  // virtual int MTPSYS_MachZeroRet(uint32_t index,int distance,int speed,void* context);
  // virtual float MTPSYS_getMinPulseSpeed();

  bool MTPSYS_VecTo(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // __UPRT_D_("vecto speed:%f\n",speed);
    // djrl.dbg_printf(">>%f %f>>",VECTo.vec[AXIS_IDX_R11],VECTo.vec[AXIS_IDX_R12]);
    while(_mstp->VecTo(VECTo,speed,ctx,exinfo)==false)
    {
      Serial.printf("");
    }
    return true;
  }
  bool MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // __UPRT_D_("I:%d,P:%d,S:%d,T:%d\n",I,P,S,T);
    while(_mstp->VecAdd(VECTo,speed,ctx,exinfo)==false)
    {
      Serial.printf("");
    }
    return true;
  }



  bool MTPSYS_AddWait(uint32_t period_ms,int times, void* ctx,MSTP_segment_extra_info *exinfo)
  {
    uint32_t waitTick=((int64_t)period_ms*_mstp->TICK2SEC_BASE)/1000;
    while(_mstp->AddWait(waitTick,times,ctx,exinfo)==false)
    {
      Serial.printf("");
    }
    return true;
  }

  bool MTPSYS_AddIOState(int32_t I,int32_t P, int32_t S,int32_t T)
  {
    MSTP_SegCtx *p_res;
    while((p_res=sctx_pool.applyResource())==NULL)//check release
    {
      Serial.printf("");
    }
    p_res->I=I;
    p_res->P=P;
    p_res->S=S;
    p_res->T=T;
    p_res->type=42;
    __UPRT_D_("I:%d,P:%d,S:%d,T:%d\n",I,P,S,T);
    while(_mstp->AddWait(0,0,p_res,NULL)==false)
    {
      Serial.printf("");
    }


    return true;
  }  

};



GCodeParser_M2 gcpm(&mstp);
StaticJsonDocument <500>doc;
StaticJsonDocument  <500>retdoc;

int MData_JR::recv_ERROR(ERROR_TYPE errorcode)
{
  for(int i=0;i<buffIdx;i++)
  {
    if(dataBuff[i]=='"')
      dataBuff[i]='\'';
  }  
  dataBuff[buffIdx]='\0';
  doDataLog=true;


  dbg_printf("recv_ERROR:%d %s",errorcode,dataBuff);
}

int MData_JR::recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
  
  if(opcode==1 )
  {
    doc.clear();
    retdoc.clear();
    DeserializationError error = deserializeJson(doc, raw);
    bool rspAck=false;
    bool doRsp=false;

    const char* type = doc["type"];
    // const char* id = doc["id"];
    if(strcmp(type,"RESET")==0)
    {
      return msg_printf("RESET_OK","");
    }
    else if(strcmp(type,"ask_JsonRaw_version")==0)
    {
      
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return this->rsp_JsonRaw_version();
    }
    else if(strcmp(type,"rsp_JsonRaw_version")==0)
    {
      const char* _version = doc["version"];
      strcpy(peerVERSION,_version);
      return 0;
    }
    else if(strcmp(type,"PING")==0)
    {
      retdoc["type"]="PONG"; 
      doRsp=rspAck=true;
    }
    else if(strcmp(type,"get_setup")==0)
    {

      retdoc["type"]="get_setup";
      retdoc["ver"]="0.5.0";
      retdoc["name"]="CNC_1";






      
      genMachineSetup(retdoc);

      
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"set_setup")==0)
    {
      retdoc["type"]="set_setup";
      
      setMachineSetup(doc);
      doRsp=rspAck=true;

    }
    else if(strcmp(type,"PIN_CONF")==0)
    {
      
      if(doc["pin"].is<int>())
      {
        int pinNo = doc["pin"];

      
        if(doc["mode"].is<int>())
        {
          int mode= doc["mode"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(mode)
          {
            case 0:pinMode(pinNo, INPUT);break;
            case 1:pinMode(pinNo, OUTPUT);break;
            case 2:pinMode(pinNo, INPUT_PULLUP);break;
            case 3:pinMode(pinNo, INPUT_PULLDOWN);break;
          }
        }
        else if(doc["output"].is<int>())
        {
          int output= doc["output"];//0:input 1:output 2:INPUT_PULLUP 3:INPUT_PULLDOWN
          switch(output)
          {
            case -2://analog
            {
              int value=analogRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;

              break;
            }
            case -1://digital
            {
              int value=digitalRead(pinNo);
              
              retdoc["type"]="PIN_INFO";
              retdoc["value"]=value;
              doRsp=rspAck=true;
              break;
            }
            case 0:digitalWrite(pinNo, LOW);break;
            case 1:digitalWrite(pinNo, HIGH);break;
          }
        }
      }
      else
      {

      }
      


    
      // retdoc["type"]="DBG_PRT";
      // // retdoc["msg"]=doc;
      // retdoc["error"]=error.code();
      // retdoc["id"]=doc["id"];
      // uint8_t buff[300];
      // int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      // send_json_string(0,buff,slen,0);
    }
    else if(strcmp(type,"BYE")==0)
    {
      doRsp=rspAck=true;

    }      
    else if(strcmp(type,"GCODE")==0)
    {
      
      int space = mstp.SegQ_Space();
      int safe_Margin=3;

      space-=safe_Margin;
      if(space<0)space=0;

      const char* code = doc["code"];
      if(code==NULL)
      {
        doRsp=true;
        rspAck=false;
        retdoc["buffer_space"]=space;
      }
      else if(gcodewait_id!=-1)
      {
        doRsp=true;
        rspAck=false;
        retdoc["buffer_space"]=space;
      }
      else
      {
        doRsp=true;
        rspAck=false;
        
        if( space>0)
        {
          rspAck=(gcpm.runLine(code)==GCodeParser::GCodeParser_Status::TASK_OK);
          space = mstp.SegQ_Space()-safe_Margin;
          if(space<0)space=0;
          retdoc["buffer_space"]=space;
        }
        else
        {
          //wait
          doRsp=false;
          
          strcpy(gcodewait_gcode,code);

          gcodewait_id=doc["id"];

        }


      }

      // delay(10);
      // dbg_printf("timerRunning:%d-%d timerCount:%d-%d T:%d",
      //   bkTRunning,mstp.timerRunning,
      //   bkTCount,timerCount,
      //   timerT_latest);
      // timerT_latest=-1;
    }
    else if(strcmp(type,"motion_buffer_info")==0)
    {
      
      const char* code = doc["code"];
      doRsp=rspAck=true;

      int space = mstp.SegQ_Space();
      int safe_Margin=3;
      space-=safe_Margin;
      if(space<0)space=0;
      retdoc["buffer_space"]=space;
      retdoc["buffer_size"]= mstp.SegQ_Size();
      retdoc["buffer_capacity"]=mstp.SegQ_Capacity()-safe_Margin;
    }


    if(doRsp)
    {
      retdoc["id"]=doc["id"];
      retdoc["ack"]=rspAck;
      
      uint8_t buff[700];
      int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
      send_json_string(0,buff,slen,0);
    }
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
  if(gcodewait_id!=-1)
  {//try to consume the waited gcode
    int space = mstp.SegQ_Space();
    int safe_Margin=3;

    space-=safe_Margin;
    if(space>0)//here is a space
    {
      retdoc.clear();
      bool rspAck=(gcpm.runLine(gcodewait_gcode)==GCodeParser::GCodeParser_Status::TASK_OK);

      {
        space = mstp.SegQ_Space()-safe_Margin;
        if(space<0)space=0;
        retdoc["buffer_space"]=space;
        retdoc["id"]=gcodewait_id;
        retdoc["ack"]=rspAck;
        
        char *buff=dbgBuff;
        int buffL=sizeof(dbgBuff);
        int slen=serializeJson(retdoc, buff,buffL);
        send_json_string(0,(uint8_t*)buff,slen,0);
      }
      gcodewait_id=-1;
    }
  }
}




int rzERROR=0;
void setup()
{
  
  // noInterrupts();
  Serial.begin(115200);//230400);
  Serial.setRxBufferSize(500);
  // // setup_comm();
  timer = timerBegin(0, 8, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  // timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_DBG, OUTPUT);
  pinMode(PIN_DBG2, OUTPUT);
  pinMode(pin_TRIG_595, OUTPUT);
  pinMode(pin_SH_165, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  mstp.INIT();
}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    Serial.printf("");
  }
}

MSTP_SegCtx ctx[10]={0};

static uint8_t recvBuf[20];
void loop()
{
  djrl.loop();
  {
    if (Serial.available() > 0) {
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.read(recvBuf,sizeof(recvBuf-1));
      //

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
  }

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



  // jdoc["PIN_Z1_STP"]=PIN_Z1_STP;
  // jdoc["PIN_Z1_DIR"]=PIN_Z1_DIR;
  // jdoc["PIN_Z1_SEN1"]=PIN_Z1_SEN1;
  // jdoc["PIN_Z1_SEN2"]=PIN_Z1_SEN2;


  // jdoc["PIN_Y_STP"]=PIN_Y_STP;
  // jdoc["PIN_Y_DIR"]=PIN_Y_DIR;
  // jdoc["PIN_Y_SEN1"]=PIN_Y_SEN1;


  // jdoc["PIN_R11_STP"]=PIN_R11_STP;
  // jdoc["PIN_R11_DIR"]=PIN_R11_DIR;


  // jdoc["PIN_R12_STP"]=PIN_R12_STP;
  // jdoc["PIN_R12_DIR"]=PIN_R12_DIR;
  


  // jdoc["PIN_OUT_0"]=PIN_OUT_0;
  // jdoc["PIN_OUT_1"]=PIN_OUT_1;
  // jdoc["PIN_OUT_2"]=PIN_OUT_2;
  // jdoc["PIN_OUT_3"]=PIN_OUT_3;
  
  
  jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  // jdoc["cam_trig_delay"]=g_cam_trig_delay;
  // jdoc["flash_trig_delay"]=g_flash_trig_delay;
  // jdoc["flash_time"]=g_flash_time;
  // jdoc["pulse_sep_min"]=g_pulse_sep_min;
  // jdoc["pulse_width_min"]=g_pulse_width_min;
  // jdoc["pulse_width_max"]=g_pulse_width_max;
  // jdoc["pulse_debounce_high"]=g_pulse_debounce_high;
  // jdoc["pulse_debounce_low"]=g_pulse_debounce_low;

  // jdoc["O_CameraPin_ON"]=O_CameraPin_ON;
  // jdoc["O_BackLight_ON"]=O_BackLight_ON;
  // jdoc["I_gate1Pin_ON"]=I_gate1Pin_ON;

}

#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}

void setMachineSetup(JsonDocument &jdoc)
{
  // JSON_SETIF_ABLE(g_cam_trig_delay,jdoc,"cam_trig_delay");
  // JSON_SETIF_ABLE(g_flash_trig_delay,jdoc,"flash_trig_delay");
  // JSON_SETIF_ABLE(g_flash_time,jdoc,"flash_time");
  // JSON_SETIF_ABLE(g_pulse_sep_min,jdoc,"pulse_sep_min");
  // JSON_SETIF_ABLE(g_pulse_width_min,jdoc,"pulse_width_min");
  // JSON_SETIF_ABLE(g_pulse_width_max,jdoc,"pulse_width_max");
  // JSON_SETIF_ABLE(g_pulse_debounce_high,jdoc,"pulse_debounce_high");
  // JSON_SETIF_ABLE(g_pulse_debounce_low,jdoc,"pulse_debounce_low");
  // JSON_SETIF_ABLE(g_cam_trig_delay,jdoc,"cam_trig_delay");


  // JSON_SETIF_ABLE(O_CameraPin_ON,jdoc,"O_CameraPin_ON");
  // JSON_SETIF_ABLE(O_BackLight_ON,jdoc,"O_BackLight_ON");
  // JSON_SETIF_ABLE(I_gate1Pin_ON,jdoc,"I_gate1Pin_ON");
}





