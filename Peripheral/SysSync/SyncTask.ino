
#include "main.hpp"
#include "MSteppers.hpp"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5


#define PIN_M1_DIR 32
#define PIN_M1_STP 33
#define PIN_M1_SEN1 19


#define PIN_M2_DIR 12
#define PIN_M2_STP 13
#define PIN_M2_SEN1 18





#define SUBDIV (1600)
#define mm_PER_REV 10


class MStp_M:public MStp{
  public:

  int FACCT=0;
  
  MStp_M(RingBuf<struct runBlock> *_blocks, MSTP_setup *_axisSetup):MStp(_blocks,_axisSetup)
  {
    
    TICK2SEC_BASE=10*1000*1000;
    minSpeed=500;//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    acc=SUBDIV*1000/mm_PER_REV;
    junctionMaxSpeedJump=1200;//5200;
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_STP, OUTPUT);
    pinMode(PIN_M1_SEN1, INPUT);
    pinMode(PIN_M2_SEN1, INPUT);

    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_STP, OUTPUT);
    // pinMode(PIN_DBG0, OUTPUT);    
  }

  int ZeroStatus=0;
  uint32_t zeroIndex=0;
  xVec posWhenHit;
  int ZeroAxis(uint32_t index,int distance)
  {


    StepperForceStop();
    zeroIndex=index;



    int sensorPIN;

    if(zeroIndex==0)
    {
      sensorPIN=PIN_M1_SEN1;
    }
    else if(zeroIndex==1)
    {
      sensorPIN=PIN_M2_SEN1;
    }
    else if(zeroIndex==100)
    {
      sensorPIN=PIN_M2_SEN1;
    }


    xVec cpos;

    Serial.printf("STP1");
    
      StepperForceStop();
      cpos=(xVec){0};
      cpos.vec[index]=distance;
    Serial.printf("STP2");
      VecAdd(cpos,minSpeed*10);
      ZeroStatus=1;
    Serial.printf("STP3\n");

      while(ZeroStatus==1 && blocks->size()!=0);
      {
        Serial.printf("");//somehow it need this or the ZeroStatus detection would never work
      }
      
      Serial.printf("ZeroStatus:%d blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(sensorPIN));
      if(ZeroStatus!=0)
      {
        ZeroStatus=0;
        return -1;
      }
        
      Serial.printf("pos=>posWhenHit[%d]:%d\n",index,posWhenHit.vec[index]);
    




    
      StepperForceStop();
      cpos=(xVec){0};
      cpos.vec[index]-=distance/2;
      VecAdd(cpos,200);
      ZeroStatus=2;



      while( (ZeroStatus==2) && (blocks->size()!=0) )
      {
        Serial.printf("");//somehow it need this or the ZeroStatus detection would never work
      }
      Serial.printf("ZeroStatus:%d  blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(sensorPIN));
      if(ZeroStatus!=0)
      {
        ZeroStatus=0;
        return -2;
      }
        
      Serial.printf("pos=>posWhenHit[%d]:%d\n",index,posWhenHit.vec[index]);
    



  
    StepperForceStop();
    curPos_c.vec[index]=0;
    lastTarLoc=curPos_c;
    ZeroStatus=0;
    return 0;
    // ZeroStatus=0;

  }





  int ZeroJointAxis(int distance)
  {
    StepperForceStop();
    zeroIndex=100;
    xVec cpos;

    Serial.printf("STP1");
    
      StepperForceStop();
      cpos=(xVec){0};
      cpos.vec[0]=distance;
      cpos.vec[1]=distance;
    Serial.printf("STP2");
      VecAdd(cpos,minSpeed*10);
      ZeroStatus=1;
    Serial.printf("STP3");

      while(ZeroStatus==1 && blocks->size()!=0);
      {
        Serial.printf("");//somehow it needs this or the ZeroStatus detection would never work
      }
      
      Serial.printf("ZeroStatus:%d  blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(PIN_M1_SEN1));
      if(ZeroStatus!=0)
      {
        ZeroStatus=0;
        return -1;
      }
        
      StepperForceStop();
      cpos=(xVec){0};
      cpos.vec[0]-=distance/2;
      cpos.vec[1]-=distance/2;
      VecAdd(cpos,200);
      ZeroStatus=2;


      while( (ZeroStatus==2) && (blocks->size()!=0) )
      {
        Serial.printf("");//somehow it needs this or the ZeroStatus detection would never work
      }
      Serial.printf("ZeroStatus:%d  blocks->size():%d  PINRead:%d\n",ZeroStatus,blocks->size(),digitalRead(PIN_M1_SEN1));
      if(ZeroStatus!=0)
      {
        ZeroStatus=0;
        return -2;
      }
        
    



  
    StepperForceStop();
    curPos_c.vec[0]=0;
    curPos_c.vec[1]=0;
    lastTarLoc=curPos_c;
    ZeroStatus=0;
    return 0;
    // ZeroStatus=0;

  }


  int ZeroAxisStepEvent()
  {
        // Serial.printf("ZeroStatus:%d blocks->size():%d\n",ZeroStatus,blocks->size());
    
    int sensorPIN;

    if(zeroIndex==0)
    {
      sensorPIN=PIN_M1_SEN1;
    }
    else if(zeroIndex==1)
    {
      sensorPIN=PIN_M2_SEN1;
    }
    else if(zeroIndex==100)
    {
      sensorPIN=PIN_M2_SEN1;
    }
    else
    {
      return -1;
    }


    {
      int sensorRead=digitalRead(sensorPIN);
      if(ZeroStatus==1)
      {
        if(sensorRead==1 && digitalRead(sensorPIN)==1)//somehow digitalRead is not stable, to a doulbe check
        {
          StepperForceStop();
          posWhenHit=curPos_c;
          ZeroStatus=0;
        }
      }
      else if(ZeroStatus==2)
      {
        if(sensorRead==0 && digitalRead(sensorPIN)==0)
        {
          StepperForceStop();
          posWhenHit=curPos_c;
          ZeroStatus=0;
        }
      }
    }
    return 0;
  }







  int M1_reader=1<<0;
  int M2_reader=1<<1;

  void BlockDirEffect(uint32_t idxes)
  {

    digitalWrite(PIN_M1_DIR, (idxes&M1_reader)!=0);
    digitalWrite(PIN_M2_DIR, (idxes&M2_reader)!=0);
    // Serial.printf("dir:%s \n",int2bin(idxes,MSTP_VEC_SIZE));
  }


  
  bool PIN_DBG0_st=false;
  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)
  {
    if(ZeroStatus!=0)
    {
      ZeroAxisStepEvent();
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

#define MSTP_BLOCK_SIZE 30
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
void setup()
{
  Serial.begin(921600);

  // // setup_comm();
  timer = timerBegin(0, 8, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_O1, OUTPUT);

  // int retErr=mstp.ZeroAxis(0,-50000);
  // if(retErr==0)
  // {
  //   isSystemZeroOK=true;
  // }
  // retErr+=mstp.ZeroAxis(1,-50000)*10;
  // int retErr=0;


}

uint32_t xendpos=470*SUBDIV/mm_PER_REV;
uint32_t speed=45000;
bool inZeroProcess=false;
void loop()
{
  if(mstp.blocks->size()==0)
  {
    delay(2000);

    for(int i=0;i<5;i++)
    {
      mstp.VecTo((xVec){xendpos,xendpos},speed);
      mstp.VecTo((xVec){xendpos/80,xendpos/80},speed); 
    }
    mstp.VecTo((xVec){0,0},speed/10);
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
