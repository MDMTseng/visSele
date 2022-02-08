
#include "main.hpp"
#include "MSteppers.hpp"
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
#define PIN_M2_DIR 26
#define PIN_M2_SEN1 17




#define PIN_OUT_0 25
#define PIN_OUT_1 26
#define PIN_OUT_2 32
#define PIN_OUT_3 33


#define SUBDIV (800)
#define mm_PER_REV 10



struct MSTP_BlkCtx{
  int type;
  // int delay_time_ms;
  int d0;
  int d1;
};

class MStp_M:public MStp{
  public:

  int FACCT=0;
  

  int POut1=0;
  
  MStp_M(RingBuf<struct runBlock> *_blocks, MSTP_setup *_axisSetup):MStp(_blocks,_axisSetup)
  {
    
    TICK2SEC_BASE=10*1000*1000;
    minSpeed=100;//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    acc=SUBDIV*1500/mm_PER_REV;//SUBDIV*3200/mm_PER_REV;
    junctionMaxSpeedJump=000;//5200;
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_STP, OUTPUT);
    pinMode(PIN_M1_SEN1, INPUT);
    pinMode(PIN_M2_SEN1, INPUT);

    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_STP, OUTPUT);
    // pinMode(PIN_DBG0, OUTPUT);    
    pinMode(PIN_OUT_1, OUTPUT);    

    
  }


  xVec posWhenHit;

  int runUntil_sensorPIN=0;
  int runUntil_sensorVal=0;


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
    Serial.printf("STP1-3\n");

    while(runUntil_sensorPIN!=0 && blocks->size()!=0)
    {
      Serial.printf("");
    }//wait for touch sensor
    

    // Serial.printf("ZeroStatus:%d blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(sensorPIN));
    if(runUntil_sensorPIN!=0)
    {
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
  int ZeroAxis(uint32_t index,int distance)
  {

    switch(index)
    {
      
      case 0://rough zeroing
      {
        int sensorDetectVLvl=0;
        int runSpeed=minSpeed*5;
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

        while(blocks->size()!=0)
        {
          Serial.printf("");
        }//wait for end

        break;
      }
      case 1:
      {
        int sensorDetectVLvl=0;
        int runSpeed=minSpeed*5;
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

    return 0;
    // ZeroStatus=0;

  }

  int runUntilDetected()
  {
        // Serial.printf("ZeroStatus:%d blocks->size():%d\n",ZeroStatus,blocks->size());

    int sensorRead=digitalRead(runUntil_sensorPIN);

  
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

  void BlockEndEffect(runBlock* blk)
  {
    
    if(blk==NULL)
    {
      return;
    }

    if(blk->ctx==NULL)
    {
      return;
    }

    MSTP_BlkCtx *ctx=(MSTP_BlkCtx*)blk->ctx;
  
    if(ctx->type!=1)
    {
      return;
    }

    if(ctx->d0==0)
    {
      if(ctx->d1==0)
      {
        digitalWrite(PIN_OUT_0,0);
        digitalWrite(PIN_OUT_1,1);
      }
      if(ctx->d1==1)
      {
        digitalWrite(PIN_OUT_0,1);
        digitalWrite(PIN_OUT_1,0);
      }
    }
    else if(ctx->d0==1)
    {
      if(ctx->d1==0)
      {
        digitalWrite(PIN_OUT_2,0);
        digitalWrite(PIN_OUT_3,1);
      }
      if(ctx->d1==1)
      {
        digitalWrite(PIN_OUT_2,1);
        digitalWrite(PIN_OUT_3,0);
      }
    }

  }
  void BlockInitEffect(runBlock* blk,uint32_t dir_idxes)
  {
    
    if(blk==NULL)
    {
      return;
    }

  
    if(blk->ctx!=NULL)//new block
    {


      MSTP_BlkCtx *ctx=(MSTP_BlkCtx*)blk->ctx;
    
      if(ctx->type==0)
      {
        if(ctx->d0==0)
        {
          if(ctx->d1==0)
          {
            digitalWrite(PIN_OUT_0,0);
            digitalWrite(PIN_OUT_1,1);
          }
          if(ctx->d1==1)
          {
            digitalWrite(PIN_OUT_0,1);
            digitalWrite(PIN_OUT_1,0);
          }
        }
        else if(ctx->d0==1)
        {
          if(ctx->d1==0)
          {
            digitalWrite(PIN_OUT_2,0);
            digitalWrite(PIN_OUT_3,1);
          }
          if(ctx->d1==1)
          {
            digitalWrite(PIN_OUT_2,1);
            digitalWrite(PIN_OUT_3,0);
          }
        }
      }


    }
    // pre_blk->ctx;//do sth... start
    
    // digitalWrite(PIN_OUT_1, POut1=(!POut1));
    digitalWrite(PIN_M1_DIR, (dir_idxes&M1_reader)!=0);
    digitalWrite(PIN_M2_DIR, (dir_idxes&M2_reader)!=0);
    
    // Serial.printf("dir:%s \n",int2bin(idxes,MSTP_VEC_SIZE));
  }

  
  bool PIN_DBG0_st=false;
  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)
  {
    if(runUntil_sensorPIN)
    {
      runUntilDetected();
    }
    // if(idxes==0)return;
    // printf("===p:%s",int2bin(idxes,5));
    // printf(" d:%s >>",int2bin(axis_dir,5));

    // // printf("PULSE_ROUNDSCALE:%d  ",PULSE_ROUNDSCALE);

    // for(int i=0;i<MSTP_VEC_SIZE;i++)
    // {
    //   int idx_p = axis_pul&(1<<i);
    //   printf("%03d ",curPos_residue.vec[i]*(idx_p?1:0));

    // }

    // printf("\n");
    
 
    axis_st|=idxes_H;
    axis_st&=~idxes_L;
    // if(idxes_L && (idxes_H==0))
    // {
    //   digitalWrite(PIN_DBG0, PIN_DBG0_st);
    //   PIN_DBG0_st=!PIN_DBG0_st;
    // }
    if(idxes_L&M1_reader)
    {

      digitalWrite(PIN_M1_STP, 0);
    }

    if(idxes_L&M2_reader)
    {
      digitalWrite(PIN_M2_STP, 0);
    }
    // Serial.printf("id:%s  ",int2bin(idxes,MSTP_VEC_SIZE));
    // Serial.printf("ac:%s ",int2bin(axis_collectpul,MSTP_VEC_SIZE));

    // int Midx=0;

    
    // Serial.printf("PINs:%s\n",int2bin(axis_st,MSTP_VEC_SIZE));

    if(idxes_H&M1_reader)
    {
      digitalWrite(PIN_M1_STP, 1);
    }

    if(idxes_H&M2_reader)
    {
      digitalWrite(PIN_M2_STP, 1);
    }
  }


};

#define MSTP_BLOCK_SIZE 50
runBlock blockBuff[MSTP_BLOCK_SIZE];
RingBuf <runBlock> __blocks(blockBuff,MSTP_BLOCK_SIZE);

MStp_M mstp(&__blocks,NULL);


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

void IRAM_ATTR onTimer()
{

  // uint32_t nextT=100;
  // Serial.printf("nextT:%d mstp.axis_RUNState:%d\n",mstp.T_next,mstp.axis_RUNState);



  int T = mstp.taskRun();
  
  // printf("T:%d\n",T);
  
  if(T<0)
  {
    
    Serial.printf("ERROR:: T(%d)<0\n",T);
    return;;
  }
  if(T==0)
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


  // 
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;


uint32_t xendpos=4700*SUBDIV/mm_PER_REV;

void pickOn(int lidx,int pos,int speed,MSTP_BlkCtx *p_ctx=NULL)
{
  int upR=30;
  if(lidx==1)
  {
    mstp.VecTo((xVec){mstp.M1Info_Limit1*upR/100,pos},speed);
    mstp.VecTo((xVec){mstp.M1Info_Limit1,pos},speed,p_ctx);
    mstp.VecTo((xVec){mstp.M1Info_Limit1*upR/100,pos},speed);
  }
  else if(lidx==2)
  {
    // int M1L1L2Dist=-30*SUBDIV/mm_PER_REV;
    int M1L1L2Dist=(-30+12)*SUBDIV/mm_PER_REV;
    pos+=M1L1L2Dist;
    mstp.VecTo((xVec){mstp.M1Info_Limit2*upR/100,pos},speed);
    mstp.VecTo((xVec){mstp.M1Info_Limit2,pos},speed,p_ctx);
    mstp.VecTo((xVec){mstp.M1Info_Limit2*upR/100,pos},speed);
  }


}


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


  
  int retErr=0;//mstp.ZeroAxis(1,50000)+mstp.ZeroAxis(0,500*2);

  rzERROR=retErr;
  if(retErr==0)
  {
    // isSystemZeroOK=true;
  }
  // retErr+=mstp.ZeroAxis(1,-50000)*10;
  // int retErr=0;


}


MSTP_BlkCtx ctx[10]={0};
void loop()
{
  if(rzERROR==0 && mstp.blocks->size()==0)
  {

  uint32_t speed=25000/10;//25000;


    int pt1=(-30-12*2)*SUBDIV/mm_PER_REV;
    int pt2=(-30-12*2+1)*SUBDIV/mm_PER_REV;
    // int pt2=-30*SUBDIV/mm_PER_REV;
    int cidx=0;
    for(int i=0;i<1;i++)
    {
      // delay(1000);

      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=1,.d1=0 };
      pickOn(1,pt1, speed,&(ctx[cidx++]));
      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=0,.d1=1 };
      pickOn(2,pt1, speed,&(ctx[cidx++]));


      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=1,.d1=0 };
      pickOn(1,pt2, speed,&(ctx[cidx++]));
      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=0,.d1=0 };
      pickOn(2,pt2, speed,&(ctx[cidx++]));


      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=1,.d1=0 };
      pickOn(1,pt2, speed,&(ctx[cidx++]));
      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=0,.d1=1 };
      pickOn(2,pt2, speed,&(ctx[cidx++]));


      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=1,.d1=0 };
      pickOn(1,pt1, speed,&(ctx[cidx++]));
      ctx[cidx]=(MSTP_BlkCtx){.type=0,.d0=0,.d1=0 };
      pickOn(2,pt1, speed,&(ctx[cidx++]));






      // mstp.VecTo((xVec){0,pt1},speed);
      // mstp.VecTo((xVec){0,pt2},speed);

    }
    // mstp.VecTo((xVec){0,0},speed);
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
