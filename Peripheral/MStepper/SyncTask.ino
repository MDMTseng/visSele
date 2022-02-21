
#include "main.hpp"
#include "MSteppers.hpp"
#include "GCodeParser_M.hpp"
#include "LOG.h"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5


#define PIN_M1_STP 12
#define PIN_M1_DIR 13
#define PIN_M1_SEN1 19
#define PIN_M1_SEN2 18


#define PIN_M2_STP 27
#define PIN_M2_DIR 14
#define PIN_M2_SEN1 17




#define PIN_OUT_0 25
#define PIN_OUT_1 26
#define PIN_OUT_2 32
#define PIN_OUT_3 33



#define PIN_DBG 18

#define SUBDIV (800)
#define mm_PER_REV 10



struct MSTP_SegCtx{
  int type;
  // int delay_time_ms;
  int d0;
  int d1;

  int32_t I,P,S,T;
};



const int SegCtxSize=4;
ResourcePool<MSTP_SegCtx>::ResourceData resbuff[SegCtxSize];
ResourcePool <MSTP_SegCtx>sctx_pool(resbuff,sizeof(resbuff)/sizeof(resbuff[0]));


class MStp_M:public MStp{
  public:

  int FACCT=0;
  

  int POut1=0;
  MStp_M(MSTP_segment *buffer, int bufferL):MStp(buffer,bufferL)
  {
    
    TICK2SEC_BASE=10*1000*1000;
    main_acc=SUBDIV*3000/mm_PER_REV;//SUBDIV*3200/mm_PER_REV;
    minSpeed=sqrt(main_acc);//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    main_junctionMaxSpeedJump=minSpeed;//5200;

    maxSpeedInc=minSpeed;
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_STP, OUTPUT);
    pinMode(PIN_M1_SEN1, INPUT);
    pinMode(PIN_M1_SEN2, INPUT);
    pinMode(PIN_M2_SEN1, INPUT);

    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_STP, OUTPUT);
    // pinMode(PIN_DBG0, OUTPUT);    
    pinMode(PIN_OUT_1, OUTPUT);    
    // pinMode(PIN_DBG, OUTPUT);    

    axisInfo[0].AccW=0.3;
    axisInfo[0].MaxSpeedJumpW=0.4;

    axisInfo[1].AccW=1;
    axisInfo[1].MaxSpeedJumpW=1;
  
    doCheckHardLimit=false;
  }


  xVec posWhenHit;

  int volatile runUntil_sensorPIN=0;
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
     Serial.printf("FATAL error:%d  %s\n",errorCode,errorText);
  }

  int runUntil(int axis,int pin,int pinVal,int distance,int speed,xVec *ret_posWhenHit)
  {
    runUntil_sensorVal=pinVal;

    StepperForceStop();
    Serial.printf("STP1-1\n");

    xVec cpos=(xVec){0};
    cpos.vec[axis]=distance;
    Serial.printf("STP1-2\n");
    runUntil_sensorPIN=pin;
    VecAdd(cpos,speed);
    Serial.printf("STP1-3  pin:%d\n",runUntil_sensorPIN);
    int cccc=0;
    while(runUntil_sensorPIN!=0 && SegQ_IsEmpty()==false)
    { 
      cccc++;
      if((cccc&0xFFFF)==0)
        Serial.printf("%d",digitalRead(runUntil_sensorPIN));
      else 
        Serial.printf("");
    }//wait for touch sensor
    
    Serial.printf("\nSTP1-3 res  pin:%d\n",runUntil_sensorPIN);

    // Serial.printf("ZeroStatus:%d blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(sensorPIN));
    if(runUntil_sensorPIN!=0)
    {
      Serial.printf("\nFAIL:runUntil_sensorPIN:%d R:%d\n",runUntil_sensorPIN,digitalRead(runUntil_sensorPIN));
      runUntil_sensorPIN=0;
      return -1;
    }

    if(ret_posWhenHit)
    {
      *ret_posWhenHit=posWhenHit;
    }

    return 0;

  }



  
  int M1Info_Limit1=300;
  int M1Info_Limit2=-300;
  int MachZeroRet(uint32_t index,int distance,int speed,void* context)
  {

    switch(index)
    {
      
      case 0://rough zeroing
      {
        int sensorDetectVLvl=0;
        int runSpeed=speed;
        int axisIdx=0;
        xVec retHitPos;
        if(runUntil(axisIdx,PIN_M1_SEN1,sensorDetectVLvl,distance,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }

        if(runUntil(axisIdx,PIN_M1_SEN1,!sensorDetectVLvl,-distance/2,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }
        M1Info_Limit1=retHitPos.vec[axisIdx];
        Serial.printf("pos1=%d\n",retHitPos.vec[axisIdx]);

      

        if(runUntil(axisIdx,PIN_M1_SEN2,sensorDetectVLvl,-distance,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }

        if(runUntil(axisIdx,PIN_M1_SEN2,!sensorDetectVLvl,distance/2,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }
        
        M1Info_Limit2=retHitPos.vec[axisIdx];
        Serial.printf("pos2=%d\n",retHitPos.vec[axisIdx]);

        int M1Mid=(M1Info_Limit1+M1Info_Limit2)/2;
        M1Info_Limit1-=M1Mid;
        M1Info_Limit2-=M1Mid;

        StepperForceStop();
        curPos_c.vec[axisIdx]-=M1Mid;//zero the Cur_pos
        lastTarLoc=curPos_c;


        
        xVec cpos=curPos_c;
        cpos.vec[axisIdx]=0;
        VecTo(cpos,runSpeed);

        while(SegQ_IsEmpty()==false)
        {
          Serial.printf("");
        }//wait for end

        break;
      }
      case 1:
      {
        int sensorDetectVLvl=0;
        int runSpeed=speed;
        int axisIdx=index;
        
        xVec retHitPos;
        if(runUntil(axisIdx,PIN_M2_SEN1,sensorDetectVLvl,distance,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }

        
        if(runUntil(axisIdx,PIN_M2_SEN1,!sensorDetectVLvl,-distance/2,runSpeed,&retHitPos)!=0)
        {
          return -1;
        }
      

        StepperForceStop();
        curPos_c.vec[axisIdx]=0;//zero the Cur_pos
        lastTarLoc=curPos_c;

      }
    }

    
    digitalWrite(PIN_M1_DIR, 1);
    digitalWrite(PIN_M1_STP, 1);
    digitalWrite(PIN_M2_DIR, 1);
    digitalWrite(PIN_M2_STP, 1);
    return 0;
    // ZeroStatus=0;

  }

  int runUntilDetected()
  {
        // Serial.printf("ZeroStatus:%d blocks->size():%d\n",ZeroStatus,blocks->size());

    volatile int sensorRead=digitalRead(runUntil_sensorPIN);
  
    if(sensorRead==runUntil_sensorVal)//somehow digitalRead is not stable, to a doulbe check
    {
      StepperForceStop();
      posWhenHit=curPos_c;
      runUntil_sensorPIN=0;
    }
    return 0;
  }

  int M1_reader=1<<0;
  int M2_reader=1<<1;


  void BlockEndEffect(MSTP_SEG_PREFIX MSTP_segment* seg)
  {    
    if(seg==NULL ||seg->ctx==NULL )
    {
      return;
    }

    

    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    sctx_pool.returnResource(ctx);
  }
  
  int PIN_DBG_ST=0;
  void BlockInitEffect(MSTP_SEG_PREFIX MSTP_segment* seg)
  {
    
    if(seg==NULL ||seg->ctx==NULL )
    {
      return;
    }
    MSTP_SegCtx *ctx=(MSTP_SegCtx*)seg->ctx;
    switch(ctx->type)
    {
      case 42:
        if(ctx->P<0)break;
        //P: pin number, S: 0~255 PWM, T: pin setup (0:input, 1:output, 2:input_pullup, 3:input_pulldown)
        if(ctx->T>=0)
        {
          if     (ctx->T==0)pinMode(ctx->P, INPUT);
          else if(ctx->T==1)pinMode(ctx->P, OUTPUT);
          else if(ctx->T==2)pinMode(ctx->P, INPUT_PULLUP);
          else if(ctx->T==3)pinMode(ctx->P, INPUT_PULLDOWN);
        }
        else
        {
          if(ctx->S<0)
          {
            digitalWrite(ctx->P,1);
          }
          else
          {
            digitalWrite(ctx->P,ctx->S);
          }
        }



      break;
    }
  }


  void BlockDirEffect(uint32_t dir_idxes)
  {
    // pre_seg->ctx;//do sth... start
    
    // digitalWrite(PIN_OUT_1, POut1=(!POut1));
    digitalWrite(PIN_M1_DIR, (dir_idxes&M1_reader)!=0);
    digitalWrite(PIN_M2_DIR, (dir_idxes&M2_reader)!=0);
    
    // Serial.printf("dir:%s \n",int2bin(idxes,MSTP_VEC_SIZE));
  }
    
  
  bool PIN_DBG0_st=false;
  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)
  {
    if(runUntil_sensorPIN)
    {
      runUntilDetected();
    }
    // if(idxes==0)return;
    // printf("===T:%s",int2bin(idxes_T,5));
    // printf(" R:%s >>",int2bin(idxes_R,5));

    // // printf("PULSE_ROUNDSCALE:%d  ",PULSE_ROUNDSCALE);

    // for(int i=0;i<MSTP_VEC_SIZE;i++)
    // {
    //   int idx_p = axis_pul&(1<<i);
    //   printf("%03d ",curPos_residue.vec[i]*(idx_p?1:0));

    // }

    // printf("\n");
    
 
    axis_st|=idxes_T;
    axis_st&=~idxes_R;
    // if(idxes_R && (idxes_T==0))
    // {
    //   digitalWrite(PIN_DBG0, PIN_DBG0_st);
    //   PIN_DBG0_st=!PIN_DBG0_st;
    // }
    if(idxes_R&M1_reader)
    {

      digitalWrite(PIN_M1_STP, 1);
    }

    if(idxes_R&M2_reader)
    {
      digitalWrite(PIN_M2_STP, 1);
    }
    // Serial.printf("id:%s  ",int2bin(idxes,MSTP_VEC_SIZE));
    // Serial.printf("ac:%s ",int2bin(axis_collectpul,MSTP_VEC_SIZE));

    // int Midx=0;

    
    // Serial.printf("PINs:%s\n",int2bin(axis_st,MSTP_VEC_SIZE));

    if(idxes_T&M1_reader)
    {
      digitalWrite(PIN_M1_STP, 0);
    }

    if(idxes_T&M2_reader)
    {
      digitalWrite(PIN_M2_STP, 0);
    }
  }


};

#define MSTP_BLOCK_SIZE 30
static MSTP_segment blockBuff[MSTP_BLOCK_SIZE];

MStp_M mstp(blockBuff,MSTP_BLOCK_SIZE);


class SyncTask
{
  protected:
  uint32_t tarClock;
  bool is_in_action;
  char name[32];
  public:
  SyncTask(const char* name)
  {
    strcpy(this->name,name);
    is_in_action=false;
  }

  void update(uint32_t currentClock)
  {
    int32_t diff = tarClock-currentClock;
    if(diff==0)
    {
      return;
    }
    //TODO...

  }

  int pushCmd(char* cmd)
  { 

  }
};




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
  // enable FPU
  xthal_set_cpenable(1);
  // Save FPU registers
  xthal_save_cp0(cp0_regs);
  // uint32_t nextT=100;
  // Serial.printf("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);



  int T = mstp.taskRun();
  
  // printf("T:%d\n",T);
  
  timerCount=T;
  if(T<0)
  {
    
    Serial.printf("ERROR:: T(%d)<0\n",T);
    T=100*10000;
    // return;;
  }
  else if(T==0)
  {
    T=100*1000;
    
  }
  else
  {
    mstp.FACCT+=T;
  }

  // int64_t td=T-timerRead(timer);
  // if(td<50)td=50;


  timerAlarmWrite(timer,T, true);


  // Restore FPU
  xthal_restore_cp0(cp0_regs);
  // and turn it back off
  xthal_set_cpenable(0);
  // 
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
void pickOn(int lidx,int pos,int speed,MSTP_SegCtx *p_ctx=NULL)
{
  int upR=80;
  int restSpeed=speed/2;
  if(lidx==2)
  {
    vecToWait((xVec){0,pos},speed);
    vecToWait((xVec){mstp.M1Info_Limit1*upR/100,pos},speed/2);
    vecToWait((xVec){mstp.M1Info_Limit1,pos},speed/3,p_ctx);
    // mstp.VecTo((xVec){mstp.M1Info_Limit1*upR/100,pos},speed);
    vecToWait((xVec){0,pos},speed);
  }
  else if(lidx==1)
  {
    // int M1L1L2Dist=-30*SUBDIV/mm_PER_REV;
    // int M1L1L2Dist=(-30+12)*SUBDIV/mm_PER_REV;
    int M1L1L2Dist=(-30)*SUBDIV/mm_PER_REV;
    pos+=M1L1L2Dist;
    vecToWait((xVec){0,pos},speed);
    vecToWait((xVec){mstp.M1Info_Limit2*upR/100,pos},speed/2);
    vecToWait((xVec){mstp.M1Info_Limit2,pos},speed/3,p_ctx);
    vecToWait((xVec){mstp.M1Info_Limit2*upR/100,pos},speed);
  }


}



class GCodeParser_M2:public GCodeParser_M
{
public:
  GCodeParser_M2(MStp *mstp):GCodeParser_M(mstp)
  {

  }

  float unit2Pulse_conv(const char* code,float dist)
  {
    // Serial.printf("unitConv[%s]:%f\n",code,dist);
    if(code[0]=='Y')
    {
      return unit2Pulse(-dist,SUBDIV/mm_PER_REV);
    }

    if(code[0]=='Z'&&code[1]=='1')
    {
      return unit2Pulse(dist,1);
    }
    if(code[0]=='F')
    {
      return unit2Pulse(dist,SUBDIV/mm_PER_REV);
    }
    return NAN;
  }


  // virtual int MTPSYS_MachZeroRet(uint32_t index,int distance,int speed,void* context);
  // virtual float MTPSYS_getMinPulseSpeed();

  bool MTPSYS_VecTo(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // Serial.printf("vecto speed:%f\n",speed);
    while(_mstp->VecTo(VECTo,speed,ctx,exinfo)==false)
    {
      Serial.printf("");
    }
    return true;
  }
  bool MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
  {
    // Serial.printf("I:%d,P:%d,S:%d,T:%d\n",I,P,S,T);
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
    Serial.printf("I:%d,P:%d,S:%d,T:%d\n",I,P,S,T);
    while(_mstp->AddWait(0,0,p_res,NULL)==false)
    {
      Serial.printf("");
    }


    return true;
  }  

};




void pickOnGCode(GCodeParser_M2 &gcpm,int headIndex,float pos_mm,int speed_mmps, int pickPin,bool pickup=true)
{
  char gcode[128];

  int headPoseDown=0;
  if(headIndex==0)
  {
    pos_mm-=15;
    headPoseDown=mstp.M1Info_Limit1;
  }
  else if(headIndex==1)
  {
    pos_mm+=15;
    headPoseDown=mstp.M1Info_Limit2;
  }
  else
    return;



  sprintf(gcode,"G01 Y%f Z1_%d F%d",pos_mm,0,speed_mmps);gcpm.runLine(gcode);

  
  int pinPreTrigger=10;//less means earlier
  int pinKeepDelay_ms=10;

  sprintf(gcode,"G01 Z1_%d",headPoseDown*pinPreTrigger/100);gcpm.runLine(gcode);
  sprintf(gcode,"M42 P%d S%d",pickPin,pickup?1:0);gcpm.runLine(gcode);
  sprintf(gcode,"G01 Z1_%d",headPoseDown);gcpm.runLine(gcode);
  
  
  sprintf(gcode,"G04 P%d",pinKeepDelay_ms);gcpm.runLine(gcode);

  sprintf(gcode,"G01 Z1_0");gcpm.runLine(gcode);

}



GCodeParser_M2 gcpm(&mstp);
bool rzERROR=0;
void setup()
{
  // noInterrupts();
  Serial.begin(921600);

  // // setup_comm();
  timer = timerBegin(0, 8, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_O1, OUTPUT);

  pinMode(PIN_OUT_0, OUTPUT);
  pinMode(PIN_OUT_1, OUTPUT);
  pinMode(PIN_OUT_2, OUTPUT);
  pinMode(PIN_OUT_3, OUTPUT);
  rzERROR=0;
  // GCodeParser::GCodeParser_Status st=gcpm.runLine("G28");
  // rzERROR=st==GCodeParser::GCodeParser_Status::TASK_OK?0:-1;

  if(rzERROR==0)
  {
    // isSystemZeroOK=true;
  }
  // retErr+=mstp.MachZeroRet(1,-50000)*10;
  // int retErr=0;

  gcpm.runLine("M42 P2 T1");
  gcpm.runLine("M42 P4 T1");

}

void busyLoop(uint32_t count)
{
  while(count--)
  {
    Serial.printf("");
  }
}

MSTP_SegCtx ctx[10]={0};

void loop()
{
  
  if(rzERROR==0)// && mstp.SegQ_IsEmpty()==true)
  {
    // delay(1000);

    // int pt2=-30*SUBDIV/mm_PER_REV;
    int cidx=0;
    char gcode[128];
    for(int i=0;i<1;i++)
    {
      // delay(1000);
    //   sprintf(gcode,"G01 Y30 Z1_%d F2000",mstp.M1Info_Limit1);
    //   gcpm.runLine(gcode);
      
      
    // __PRT_I_(">>>>\n");
    //   gcpm.runLine("M42 P2 S1");
    // __PRT_I_(">>>>\n");
    //   gcpm.runLine("G01 Y0 Z1_0 F1000");
    //   gcpm.runLine("M42 P2 S0");
      
    // __PRT_I_(">>>>\n");
    //   sprintf(gcode,"G01 Y-30 Z1_%d F2000",mstp.M1Info_Limit2);
    //   gcpm.runLine(gcode);

    // __PRT_I_(">>>>\n");
      
    //   gcpm.runLine("G01 Y0 Z1_0 F2000");
    //   gcpm.runLine("G04 P10");

      int speed=100;
      float pos=50;
      pickOnGCode(gcpm,0,pos,speed, 2,true);
      pickOnGCode(gcpm,1,pos,speed, 4,true);
      pos+=50;
      pickOnGCode(gcpm,0,pos,speed, 2,false);
      pickOnGCode(gcpm,1,pos,speed, 4,false);

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


int CMD_parse(SimpPacketParse &SPP, buffered_print *bp, int *ret_result = NULL)
{
  char *TLC = SPP.buffer;
  char *DATA = SPP.buffer + 2;
  int dataLen = SPP.size() - 2;
  //  Serial.print(TLC[0]);
  //  Serial.println(TLC[1]);
  //  Serial.println(DATA);

  ret_doc.clear();
  bool errorCode = -1;
  int ret_len = 0;

  char retTLC[3];
  if (TLC[0] == 'T' && TLC[1] == 'T')
  {
    errorCode = 0;
  }
  else if (TLC[0] == 'S' && TLC[1] == 'T')
  {

    { //@ST{"sss":4,"ECHO":{"AAA":{"fff":7}}}$

      deserializeJson(recv_doc, DATA);

      const char *sensor = recv_doc["ECHO"];
      if (sensor != NULL)
      {
        bp->printf("tt%s", sensor);
      }

      recv_doc.clear();
    }
    errorCode = 0;
  }
  else if (TLC[0] == 'J' && TLC[1] == 'S') //@JS{"id":566,"type":"get_cache_rec"}$@JS{"id":566,"type":"empty_cache_rec"}$
  {
    sprintf(retTLC, "js");
    deserializeJson(recv_doc, DATA);
    ret_doc.clear();

    auto idObj = recv_doc["id"];
    ret_doc["id"] = idObj;

    auto typeObj = recv_doc["type"];
    if (typeObj.is<char *>())
    {
      const char *type = typeObj.as<char *>();
      if (strcmp(type, "get_cache_rec") == 0)
      {

      }
    }
    recv_doc.clear();

    errorCode = 0;
  }
  else
  {
  }

  if (bp->size() == 0)
  {
    bp->printf("%s", retTLC);
    size_t s = serializeJson(ret_doc, bp->buffer() + bp->size(), bp->rest_capacity());
    bp->resize(bp->size() + s);
  }

  if (ret_result)
  {
    *ret_result = errorCode;
  }
  return ret_len;
}


// void setup()
// {
//   // noInterrupts();
//   Serial.begin(921600);
  
//   pinMode(PIN_M1_STP, OUTPUT);
//   pinMode(PIN_M1_DIR, OUTPUT);


//   pinMode(PIN_OUT_0, OUTPUT);
//   pinMode(PIN_OUT_1, OUTPUT);
//   pinMode(PIN_OUT_2, OUTPUT);
//   pinMode(PIN_OUT_3, OUTPUT);

// }



// void loop()
// {
  
//   digitalWrite(PIN_M1_STP, 1);
//   digitalWrite(PIN_OUT_0, 1);
//   delay(1000);

//   digitalWrite(PIN_M1_STP, 0);
//   digitalWrite(PIN_OUT_0, 0);
//   delay(1000);



// }

