
#include "main.hpp"
#include "MSteppers.hpp"
#include "GCodeParser_M.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
#include <Data_Layer_Protocol.hpp>
#include "driver/timer.h"


extern "C" {
#include "direct_spi.h"
}

#pragma once
#define __UPRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __UPRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)



inline float mm2Pulse_conv(int axisIdx,float dist);

void genMachineSetup(JsonDocument &jdoc);
void setMachineSetup(JsonDocument &jdoc);
uint32_t g_step_trigger_edge=(1<<AXIS_IDX_X);//0xFFFFFFF;//each bits means trigger edge setting on each axis, 0 for posedge 1 for negedge
uint32_t g_dir_inv=0^(1<<AXIS_IDX_Z1)^(1<<AXIS_IDX_Z2)^(1<<AXIS_IDX_Z3)^(1<<AXIS_IDX_Z4);
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



void G_LOG(char* str)
{
  djrl.dbg_printf(str);
}


hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5
#define PIN_LED 2



//#define _HOMING_DBG_FLAG_ 50

#define PIN_DBG 14
#define PIN_DBG2 27


int pin_SH_165=17;
int pin_TRIG_595=5;

#define GPIOLS32_SET(PIN) GPIO.out_w1ts=1<<(PIN);
#define GPIOLS32_CLR(PIN) GPIO.out_w1tc=1<<(PIN);
  

#define SUBDIV (3200)
#define mm_PER_REV 95

spi_device_handle_t spi1=NULL;

array<string, 10> CameraIDList;
enum MSTP_SegCtx_TYPE{
  NA=0,
  IO_CTRL=1

};




struct triggerInfo{//TODO: rename the infoQ to be more versatile
  string camera_id;
  string trig_tag;
  int trig_id;
  float curFreq;
  bool isTrigInfo;
  string log;
};

RingBuf_Static<struct triggerInfo,20,uint8_t> triggerInfoQ;



struct MSTP_SegCtx{
  MSTP_SegCtx_TYPE type;
  // int delay_time_ms;
  uint32_t PORT,S;
  int32_t P,T;
  string CID;
  string TTAG;
  int TID;
};


const int SegCtxSize=40;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,sizeof(resbuff)/sizeof(resbuff[0]));


#define _TICK2SEC_BASE_ (10*1000*1000)
class MStp_M:public MStp{
  public:

  int FACCT=0;
  

  int POut1=0;




  MStp_M(MSTP_segment *buffer, int bufferL):MStp(buffer,bufferL)
  {
    
    this->TICK2SEC_BASE=_TICK2SEC_BASE_;
    main_acc=mm2Pulse_conv(AXIS_IDX_X,2000);//SUBDIV*3200/mm_PER_REV;
    minSpeed=sqrt(main_acc);//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    main_junctionMaxSpeedJump=minSpeed*2;//5200;

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

    int general_max_freq=65*1000;
    axisInfo[AXIS_IDX_X].VirtualStep=1;
    axisInfo[AXIS_IDX_X].AccW=1;
    axisInfo[AXIS_IDX_X].MaxSpeedJumpW=1;
    axisInfo[AXIS_IDX_X].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_Y].VirtualStep=1.3;
    axisInfo[AXIS_IDX_Y].AccW=0.7;
    axisInfo[AXIS_IDX_Y].MaxSpeedJumpW=1;
    axisInfo[AXIS_IDX_Y].MaxSpeed=general_max_freq;


    axisInfo[AXIS_IDX_Z].VirtualStep=1;
    axisInfo[AXIS_IDX_Z].AccW=1;
    axisInfo[AXIS_IDX_Z].MaxSpeedJumpW=1;
    axisInfo[AXIS_IDX_Z].MaxSpeed=general_max_freq;

    auto mainAXIS_VSTEP=axisInfo[AXIS_IDX_Y].VirtualStep;


//inline float mm2Pulse_conv(int axisIdx,float dist);
    float _ZX_VS=mm2Pulse_conv(AXIS_IDX_X,1)/mm2Pulse_conv(AXIS_IDX_Z1,1);
    float _RX_VS=mm2Pulse_conv(AXIS_IDX_X,1)/mm2Pulse_conv(AXIS_IDX_R1,1)*(M_PI/180*5);//R axis uses Deg as unit, to convert to mm effect length we convert it as rad * R(effect radius in mm) as effect arc length

    float ZX_VS=_ZX_VS*0.3;
    float ZXAccW=1/_ZX_VS;
    float RX_VS=_RX_VS*0.3;
    float RXAccW=1/_RX_VS;
    float JW=10/3;
    axisInfo[AXIS_IDX_Z1].VirtualStep=ZX_VS;
    axisInfo[AXIS_IDX_Z1].AccW=ZXAccW;
    axisInfo[AXIS_IDX_Z1].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_Z1].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R1].VirtualStep=RX_VS;
    axisInfo[AXIS_IDX_R1].AccW=RXAccW;
    axisInfo[AXIS_IDX_R1].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_R1].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z2].VirtualStep=ZX_VS;
    axisInfo[AXIS_IDX_Z2].AccW=ZXAccW;
    axisInfo[AXIS_IDX_Z2].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_Z2].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R2].VirtualStep=RX_VS;
    axisInfo[AXIS_IDX_R2].AccW=RXAccW;
    axisInfo[AXIS_IDX_R2].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_R2].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z3].VirtualStep=ZX_VS;
    axisInfo[AXIS_IDX_Z3].AccW=ZXAccW;
    axisInfo[AXIS_IDX_Z3].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_Z3].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R3].VirtualStep=RX_VS;
    axisInfo[AXIS_IDX_R3].AccW=RXAccW;
    axisInfo[AXIS_IDX_R3].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_R3].MaxSpeed=general_max_freq;



    axisInfo[AXIS_IDX_Z4].VirtualStep=ZX_VS;
    axisInfo[AXIS_IDX_Z4].AccW=ZXAccW;
    axisInfo[AXIS_IDX_Z4].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_Z4].MaxSpeed=general_max_freq;

    axisInfo[AXIS_IDX_R4].VirtualStep=RX_VS;
    axisInfo[AXIS_IDX_R4].AccW=RXAccW;
    axisInfo[AXIS_IDX_R4].MaxSpeedJumpW=JW;
    axisInfo[AXIS_IDX_R4].MaxSpeed=general_max_freq;


    
    // axisInfo[AXIS_IDX_R12].VirtualStep=3;
    // axisInfo[AXIS_IDX_R12].AccW=SUBDIV*1500/mm_PER_REV/main_acc/axisInfo[AXIS_IDX_Y].VirtualStep;
    // axisInfo[AXIS_IDX_R12].MaxSpeedJumpW=1/axisInfo[AXIS_IDX_Y].VirtualStep;
  
    doCheckSoftLimit=false;

    spi1= direct_spi_init(1,40*1000*1000,PIN_NUM_MOSI,PIN_NUM_MISO,PIN_NUM_CLK,PIN_NUM_CS);
    dspi_device_select(spi1,1);

    ShiftRegAssign(0,0);
    ShiftRegUpdate();


    endstopPins_normalState=0xff;
    endstopPins=0xff;
    
  }


  xVec posWhenHit;

  int volatile runUntil_ExtPIN=-1;
  int volatile runUntil_sensorVal=0;


  void stopTimer(){
    //do not stop, for constant pull input pins
    // if(timerRunning==true)
    // {
    //   timerAlarmDisable(timer); 
    //   timerRunning=false;
    // }
    // __PRT_I_(">\n");
  }
  void startTimer(){
    // if(timerRunning==false)
    // {
    //   timerAlarmEnable(timer);  
    //   // timerAlarmWrite(timer,1, true);
    //   
    // }
    timerRunning=true;
  }
  void setTimer(uint64_t T)
  {
    
    // printf("T:%d\n",T);
    if(T==0)//go idle update speed
    {
      T=_TICK2SEC_BASE_/1000;
      
    }
    // if(timerT_TOP)
    // {
    //   T=timerT_TOP;
    // }
    // int64_t td=T-timerRead(timer);
    // if(td<50)td=50;


    // timerAlarmWrite(timer,T, true);
    // T-=27;
    timer_set_alarm_value(timer_group_t::TIMER_GROUP_0, timer_idx_t::TIMER_0, (uint64_t)T);

  }
  void FatalError(int errorCode,const char* errorText)
  {
     __UPRT_D_("FATAL error:%d  %s\n",errorCode,errorText);
  }

  int _HOMING_DBG_COUNTER=0;
  int runUntil(int axis,int ext_pin,int pinVal,int distance,int speed,xVec *ret_posWhenHit)
  {
    runUntil_sensorVal=pinVal;


#ifdef _HOMING_DBG_FLAG_
    _HOMING_DBG_COUNTER=_HOMING_DBG_FLAG_;
#endif

    MT_StepperForceStop();
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
      yield();
    }//wait to touch sensor
    
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
      case AXIS_IDX_Z:

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
        
        xVec retHitPos;
        if(runUntil(axisIdx,sensor_pin,sensorDetectVLvl,distance,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }

        delay(100);
        
        if(runUntil(axisIdx,sensor_pin,!sensorDetectVLvl,-distance/2,runSpeed/100,&retHitPos)!=0)
        {
          return -1;
        }
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

    
#ifdef _HOMING_DBG_FLAG_
    if(_HOMING_DBG_COUNTER==0)
#else
    if(sensorRead==runUntil_sensorVal)//somehow digitalRead is not stable, to a doulbe check
#endif
    {
      IT_StepperForceStop();
      posWhenHit=curPos_c;
      runUntil_ExtPIN=-1;
      return true;
    }

#ifdef _HOMING_DBG_FLAG_
    if(_HOMING_DBG_COUNTER>0)
      _HOMING_DBG_COUNTER--;
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
    return ctx->type==MSTP_SegCtx_TYPE::IO_CTRL;
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
      case MSTP_SegCtx_TYPE::IO_CTRL:

        if(ctx->CID.length()>0)
        {
          //send camera idx 
          struct triggerInfo tinfo={.camera_id=ctx->CID,.trig_tag=ctx->TTAG,.trig_id=ctx->TID,.curFreq=seg->vcur,.isTrigInfo=true};

          triggerInfo* Qhead=NULL;
          while( (Qhead=triggerInfoQ.getHead()) ==NULL)
          {
            yield();
          }
          *Qhead=tinfo;
          triggerInfoQ.pushHead();
        }


        //P: pin number, S: 0~255 PWM, T: pin setup (0:input, 1:output, 2:input_pullup, 3:input_pulldown)
        if(ctx->PORT)
        {
          static_Pin_update_needed=true;
          static_Pin_info=((~ctx->PORT)&static_Pin_info)|(ctx->PORT&ctx->S);


        }
        else if(ctx->P>=0)
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
    { //if there is a Pin_= need to be updated yet next segment isn't IOCtrl => then do IO update right now
      if(isIOCtrl(n_seg)==false)//if update pin is needed
      {
        ShiftRegUpdate();
        ShiftRegAssign(latest_dir_pins,0);
        ShiftRegUpdate();
      }
    }



    if(seg!=NULL)
    {
      BlockCtxReturn(seg);
    }
  }
  void BlockCtxReturn(MSTP_SEG_PREFIX MSTP_segment* seg)
  {    
    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    if(ctx==NULL )return;
    sctx_pool.returnResource(ctx);
    seg->ctx=NULL;
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

  int shiftRegAssignedCount=0;//the count hs to be 1 in order to get correct input data
  #define BIT_CUT(V32,offset,width)  (   ( (V32)>>(offset) )     &    ((1<<(width))-1)    )

  #define BIT_CUT2(V32,LBitPosition,HBitPosition)  (   ( (V32)>>(LBitPosition) )     &    ((1<<(HBitPosition-LBitPosition+1))-1)    )
  #define ENDIAN_SWITCH(B32)  (((B32)<<24)|(((B32)&0xFF00)<<8)|(((B32)&0xFF0000)>>8)|((B32)>>24))


  void ShiftRegAssign(uint32_t dir,uint32_t step)
  {
    _latest_stp_pins=step;
    _latest_dir_pins=dir;



    // uint32_t Seg1=V_CUT(dir,0,3)<<8|V_CUT(step,0,3)<<8;


    // uint32_t Seg2=V_CUT(dir,0,3)<<8|V_CUT(step,0,3)<<8;
    uint32_t m_dir=dir^g_dir_inv;
    uint32_t m_step=step^g_step_trigger_edge;

    // uint32_t portPins=((testCounter&0xff)<<24)|((testCounter&0xff)<<16);
    // testCounter++;


    //demo board    pX  pY  pZ pZ1 pR1 pZ2 pR2 ___  dX  dY  dZ dZ1 dR1 dZ2 dR2 ___ pZ3 pR3 pZ4 pR4 ___ dZ3 dR3 dZ4 dR4  ____  
    //               0                               8                              16                 (21)         24    25   
    

    //demo board    __  __  pZ pZ1 pR1 pZ2 pR2 pX   __  __  dZ dZ1 dR1 dZ2 dR2  dX pZ3 pR3 pZ4 pR4  pY dZ3 dR3 dZ4 dR4   dY  
    //               0                               8                              16                 (21)         24    25   
    
    //demo board
    // uint32_t portPins=
    //   ( ((BIT_CUT(m_step,0,  1)<< 7)|(BIT_CUT(m_dir,0,  1)<<15)))|

    //   ( ((BIT_CUT(m_step,3,  4)<< 3)|(BIT_CUT(m_dir,3,  4)<<11))       )|


    //   ( ((BIT_CUT(m_step,1,  1)<<20)|(BIT_CUT(m_dir,1,  1)<<25)))|
    //   ( ((BIT_CUT(m_step,7,  4)    )|(BIT_CUT(m_dir,7,  4)<<5))<<(16) );


    //V0.1.0 board  pX  pY  dX dY pR1 pZ2 pR2 pX   __  __  dZ dZ1 dR1 dZ2 dR2  dX pZ3 pR3 pZ4 pR4  pY dZ3 dR3 dZ4 dR4   dY  
    //               0                               8                              16                 (21)         24    25   
    //V0.1.0 board
    // uint32_t portPins=
    //  ( ((BIT_CUT(m_step,0,  2)<< ( 0))|(BIT_CUT(m_dir,0,  2)<<( 2))))//XY
    // |( ((BIT_CUT(m_step,2,  5)<< ( 4))|(BIT_CUT(m_dir,2,  5)<<( 9))))//axis idx step & dir 2~ 6


    // //HW bug FIX: the dir10 would use pin 25 as backup and needs air wire to route 
    // |   (BIT_CUT(m_step,7,  5)<< (14)) //axis idx step 7 ~11, step is OK
    // |   (BIT_CUT(m_dir ,7,  1)<< (19))| (BIT_CUT(m_dir,9,  3)<<(20)) //dir 7 8 9 11, 
    // |   (BIT_CUT(m_dir ,8,  1)<< (25))//
     
    // |((uint32_t)0<<24)|((uint32_t)0<<23)//pin 24 => XY disable value (1 as disable 0 as enable)
    // ;





    uint32_t portPins=
     ( ((BIT_CUT2(m_step,AXIS_IDX_X,AXIS_IDX_Y)<< ( 0))|(BIT_CUT2(m_dir,AXIS_IDX_X,AXIS_IDX_Y)<<( 2))))//XY


    |( ((BIT_CUT2(m_step,AXIS_IDX_R1,AXIS_IDX_G2_RS)<< ( 4))|(BIT_CUT2(m_dir,AXIS_IDX_R1,AXIS_IDX_G2_RS)<<( 9))))//axis idx step & dir 2~ 6


    //HW V0.1.0 board bug FIX: the dir10 would use pin 25 as backup and needs air wire to route 
    |   (BIT_CUT2(m_step,AXIS_IDX_Z1,AXIS_IDX_G1_RS)<< (14)) //axis idx step 7 ~11, step is OK
    |   (BIT_CUT2(m_dir ,AXIS_IDX_Z1,   AXIS_IDX_Z1)<< (19)) | (BIT_CUT2(m_dir,AXIS_IDX_Z3,  AXIS_IDX_G1_RS)<<(20)) //dir 7 8 9 11, 
    |   (BIT_CUT2(m_dir ,AXIS_IDX_Z2,   AXIS_IDX_Z2)<< (31))//
    |   ((uint32_t)0<<24)|((uint32_t)0<<23)//pin 24 => XY disable value (1 as disable 0 as enable)
    ;

    // uint32_t portPins=
    //   ( ((BIT_CUT(m_step,0,3+5)  )|(BIT_CUT(m_dir,0,3+5)<<8))       );
    //demo board    pX  pY  pZ pZ1 pZ2 pZ3 pZ4 pRx dX  dY dZ dZ1 dZ2 dZ3 dZ4 dRx 
    //               0                              8                          15



    // (( dir&0xFF)<<16 | ( (step^g_step_trigger_edge) & 0xFF)<<24|static_Pin_info<<0)>>16;
    // uint32_t portPins=(dir&0xF)<<16 | (step & 0xFF)<<24|(static_Pin_info&0xF)<<20;

    //((portPins&0xFF)<<24)|((portPins&0xFF00)<<8)|((portPins&0xFF0000)>8)|((portPins&0xFF000000)>24);
    // ((portPins)<<24)|((portPins&0xFF00)<<8)|((portPins&0xFF0000)>>8)|((portPins)>>24);//endieness conversion
    int groupCount=5;
    int groupIdx=groupCount-1;
    spi1->host->hw->data_buf[groupIdx--]=ENDIAN_SWITCH(portPins);
    spi1->host->hw->data_buf[groupIdx--]=ENDIAN_SWITCH(static_Pin_info);
    
    GPIOLS32_SET(pin_SH_165);//switch to keep in 165 register(stop 165 load pin to reg)
    GPIOLS32_CLR(pin_TRIG_595);//
    direct_spi_transfer(spi1,32*(groupCount));
    shiftRegAssignedCount++;
    //send_SPI(portPins);
  }
  


  bool isEndStopHit(uint32_t inputPins)
  {
    auto endStopPinHit= (inputPins^endstopPins_normalState) & endstopPins;
    if( endStopPinHit )
    {
      endstopPins_hit=inputPins;
      return true;
    }
    return false;
  }

  void ShiftRegUpdate()
  {
    static_Pin_update_needed=false;//will
    while (direct_spi_in_use(spi1));//wait for SPI bus available
    GPIOLS32_CLR(pin_SH_165);//switch to load(165 keeps load pin to internal reg)
    if(shiftRegAssignedCount==1)
    {
      latest_input_pins=spi1->host->hw->data_buf[0];
      if(runUntil_ExtPIN!=-1)//in zeroing state
      {
        if(runUntilDetected(latest_input_pins)==true)//if reaches, do not let 595 update pins, to prevent further movement
          return;
      }
      else
      {//or check end stop hit
        if(endStopDetection)
        {
          if(endStopHitLock || isEndStopHit(latest_input_pins))
          {
            if(endStopHitLock==false)
            {
              endStopHitLock=true;
              
              struct triggerInfo tinfo={.isTrigInfo=false,.log="End_stop hit, EM STOP...."+to_string(endstopPins)+" "+to_string(endstopPins_normalState)};

              triggerInfo* Qhead=NULL;
              while( (Qhead=triggerInfoQ.getHead()) ==NULL);
              *Qhead=tinfo;
              triggerInfoQ.pushHead();
              IT_StepperForceStop();
            }
          }
        }

        if( endStopHitLock )
        {
          //hit.... em stop
          endStopHitLock=true;
          return;
        }
      }
    }


    shiftRegAssignedCount=0;
    GPIOLS32_SET(pin_TRIG_595);//trigger 595 internal register update to 959 phy pin
    
    latest_stp_pins=_latest_stp_pins;
    latest_dir_pins=_latest_dir_pins;
  }


  void BlockPinInfoUpdate(uint32_t dir,uint32_t idxes_T,uint32_t idxes_R)
  {
     ShiftRegAssign(dir,idxes_T);
  }
  
  
  void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)
  {
    ShiftRegUpdate();
  }
};

#define MSTP_BLOCK_SIZE 40
static MSTP_segment blockBuff[MSTP_BLOCK_SIZE];

MStp_M mstp(blockBuff,MSTP_BLOCK_SIZE);


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

uint32_t cp0_regs[18];

void IRAM_ATTR onTimer()
{
  GPIOLS32_SET(PIN_LED);
  // enable FPU
  xthal_set_cpenable(1);
  // Save FPU registers
  xthal_save_cp0(cp0_regs);
  // uint32_t nextT=100;
  // __UPRT_D_("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);
  

  uint32_t T=mstp.taskRun();

  
  // Restore FPU
  xthal_restore_cp0(cp0_regs);
  // and turn it back off
  xthal_set_cpenable(0);
  // 
  GPIOLS32_CLR(PIN_LED);
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;



void vecToWait(xVec VECTo,float speed,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL)
{
  // digitalWrite(PIN_OUT_1,1);
  while(mstp.VecTo(VECTo,speed,ctx,exinfo)==false)
  {
    yield();
  }
  
  // digitalWrite(PIN_OUT_1,0);
}


void addWaitWait(uint32_t period,int times=1,void* ctx=NULL,MSTP_segment_extra_info *exinfo=NULL)
{
  // digitalWrite(PIN_OUT_1,1);
  while( mstp.AddWait(period,times,ctx,exinfo)==false)
  {
    yield();
  }
  
  // digitalWrite(PIN_OUT_1,0);
}



inline float mm2Pulse_conv(int axisIdx,float dist)
{
  // __UPRT_D_("unitConv[%s]:%f\n",code,dist);

  // return unit2Pulse(
  switch(axisIdx)
  {
    case AXIS_IDX_X:
    case AXIS_IDX_Z:return dist*SUBDIV/mm_PER_REV;//-1 for reverse the direction
    case AXIS_IDX_Y:return -1*dist*SUBDIV/mm_PER_REV;
    
    case AXIS_IDX_FEEDRATE:
    case AXIS_IDX_ACCELERATION:
    case AXIS_IDX_DEACCELERATION:
      return dist*SUBDIV/mm_PER_REV;


    case AXIS_IDX_Z1:
    case AXIS_IDX_Z2:
    case AXIS_IDX_Z3:
    case AXIS_IDX_Z4:return dist*200*4/40;//as pulse count

    case AXIS_IDX_R1:
    case AXIS_IDX_R2:
    case AXIS_IDX_R3:
    case AXIS_IDX_R4://assume it's 800 pulses pre rev
      return dist*(200*4)/360;//-1 for reverse the direction




  }

  return NAN;
}

class GCodeParser_M2:public GCodeParser_M
{
public:
  GCodeParser_M2(MStp *mstp):GCodeParser_M(mstp)
  {

  }

  float unit2Pulse_conv(int axisIdx,float dist)
  {


    return mm2Pulse_conv( axisIdx, dist);
  }
  float Pulse2Unit_conv(int axisIdx,float pulseCount)
  {
    // __UPRT_D_("unitConv[%s]:%f\n",code,dist);
    return pulseCount/unit2Pulse_conv(axisIdx,1);//since all the converts for now are direct multiplication
  }


  // virtual int MTPSYS_MachZeroRet(uint32_t index,int distance,int speed,void* context);
  // virtual float MTPSYS_getMinPulseSpeed();

  bool MTPSYS_VecTo(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // __UPRT_D_("vecto speed:%f\n",speed);
    // djrl.dbg_printf(">>%f %f>>",VECTo.vec[AXIS_IDX_R11],VECTo.vec[AXIS_IDX_R12]);
    while(_mstp->VecTo(VECTo,speed,ctx,exinfo)==false)
    {
      yield();
    }
    return true;
  }
  bool MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // __UPRT_D_("I:%d,P:%d,S:%d,T:%d\n",I,P,S,T);
    while(_mstp->VecAdd(VECTo,speed,ctx,exinfo)==false)
    {
      yield();
    }
    return true;
  }



  bool MTPSYS_AddWait(uint32_t period_ms,int times, void* ctx,MSTP_segment_extra_info *exinfo)
  {
    uint32_t waitTick=((int64_t)period_ms*_mstp->TICK2SEC_BASE)/1000;

    // G_LOG("in MTPSYS_AddWait");
    // while(_mstp->AddWait(waitTick,times,ctx,exinfo)==false)
    // {
    //   yield();
    // }
    // return true;
    return _mstp->AddWait(waitTick,times,ctx,exinfo);
  }

  bool MTPSYS_AddIOState(uint32_t PORT,int32_t P, uint32_t S,int32_t T,char* CID,char* TTAG,int TID)
  {
    MSTP_SegCtx *p_res;
    while((p_res=sctx_pool.applyResource())==NULL)//check release
    {
      yield();
    }
    p_res->PORT=PORT;
    p_res->P=P;
    p_res->S=S;
    p_res->T=T;
    p_res->CID=CID==NULL?"":string(CID);
    p_res->TTAG=TTAG==NULL?"":string(TTAG);
    p_res->TID=TID;
    p_res->type=MSTP_SegCtx_TYPE::IO_CTRL;
    __UPRT_D_("M:%d,P:%d,S:%d,T:%d CID:%s TTAG:%s TID:%d\n",M,P,S,T,CID,TTAG,TID);
    while(_mstp->AddWait(0,0,p_res,NULL)==false)
    {
      yield();
    }


    return true;
  }  

};



GCodeParser_M2 gcpm(&mstp);
StaticJsonDocument <500>doc;
StaticJsonDocument  <500>retdoc;

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
          gcpm.putJSONNote(&retdoc);
          rspAck=(gcpm.runLine(code)==GCodeParser::GCodeParser_Status::TASK_OK);
          gcpm.putJSONNote(NULL);
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
      gcpm.putJSONNote(&retdoc);
      bool rspAck=(gcpm.runLine(gcodewait_gcode)==GCodeParser::GCodeParser_Status::TASK_OK);
      gcpm.putJSONNote(NULL);
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
  // // setup_comm();
  timer = timerBegin(0, 80*1000000/_TICK2SEC_BASE_, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_DBG, OUTPUT);
  pinMode(PIN_DBG2, OUTPUT);
  pinMode(pin_TRIG_595, OUTPUT);
  pinMode(pin_SH_165, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  // CameraIDList[0]="ABC";
  // CameraIDList[1]="DEF";
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

  string istr=to_string(inum);
  int pnum=(ipnum%powNum);

  string pstr=to_string(pnum+powNum);

  string resStr=istr+pstr;
  resStr[istr.length()]='.';
  return resStr;
}

static uint8_t recvBuf[20];
void loop()
{
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
    retdoc.clear();
    int curTrigQSize;
    while(0!=(curTrigQSize=triggerInfoQ.size()))
    {
      triggerInfo info=*triggerInfoQ.getTail();
      // retdoc["tag"]="s_Step_"+std::to_string((int)info.step);
      // retdoc["trigger_id"]=info.step;

      if(info.isTrigInfo)
      {
        retdoc["type"]="TriggerInfo"; 
        retdoc["camera_id"]=info.camera_id;
        retdoc["tag"]=info.trig_tag+",s_PFQ:"+toFixed(info.curFreq,100);
        retdoc["trigger_id"]=info.trig_id;



        int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
        djrl.send_json_string(0,buff,slen,0);
      }
      else
      {
        djrl.dbg_printf("%s",info.log.c_str());
      }
      triggerInfoQ.consumeTail();
    }
  }


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

  jdoc["axis"]="X,Y,Z1_,R11_,R12_";

  // auto obj=jdoc.createNestedObject("obj");
}

#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}

void setMachineSetup(JsonDocument &jdoc)
{
  // JSON_SETIF_ABLE(g_cam_trig_delay,jdoc,"cam_trig_delay");
}





