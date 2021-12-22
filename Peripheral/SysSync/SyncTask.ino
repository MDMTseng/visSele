
#include "main.hpp"
#include "MSteppers.hpp"
#include "xtensa/core-macros.h"
#include "soc/rtc_wdt.h"
hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5


#define PIN_M1_DIR 32
#define PIN_M1_STP 33
#define PIN_M2_DIR 12
#define PIN_M2_STP 13



#define PIN_DBG0 18

#define SUBDIV 400
#define mm_PER_REV 2


class MStp_M:public MStp{
  public:

  int FACCT=0;
  MStp_M(RingBuf<struct runBlock> *_blocks):MStp(_blocks)
  {
    
    TICK2SEC_BASE=1000000;
    PULSE_ROUND_SHIFT=7;
    minSpeed=100;//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    acc=SUBDIV*100/mm_PER_REV;
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_STP, OUTPUT);
    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_STP, OUTPUT);
    pinMode(PIN_DBG0, OUTPUT);

    
  }

  int M1_reader=1<<(MSTP_VEC_SIZE-2);
  int M2_reader=1<<(MSTP_VEC_SIZE-1);

  void BlockDirEffect(uint32_t idxes)
  {

    digitalWrite(PIN_M1_DIR, (idxes&M1_reader)!=0);
    digitalWrite(PIN_M2_DIR, (idxes&M2_reader)!=0);
    Serial.printf("dir:%s \n",int2bin(idxes,MSTP_VEC_SIZE));
  }


  
  bool PIN_DBG0_st=false;
  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)
  {
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
    
 
    if(axis_st&idxes_H)
    {
      // Serial.printf("ERROR pull up\n");
    }

    axis_st|=idxes_H;

 
    if(axis_st&idxes_L!=idxes_L)
    {
      // Serial.printf("ERROR pin down\n");
    }
    axis_st&=~idxes_L;
    if(idxes_L)
    {
      digitalWrite(PIN_DBG0, PIN_DBG0_st);
      PIN_DBG0_st=!PIN_DBG0_st;
    }
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
runBlock blockBuff[20];
RingBuf <runBlock> __blocks(blockBuff,20);

MStp_M mstp(&__blocks);


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
    T=1000000;
    
  }
  else
  {
    mstp.FACCT+=T;
  }
  timerAlarmWrite(timer,T, true);


  // 
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;
void setup()
{
  Serial.begin(921600);

  // // setup_comm();
  timer = timerBegin(0, 80, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_O1, OUTPUT);

  int speed = 400*40*2/4/10;
  // for(int i=0;i<4;i++)
  // {
  //   
  //   int posMult=2;
  //   xVec dst;

    
  //   for(int k=0;k<1;k++)
  //   {
  //     for(int j=0;j<MSTP_VEC_SIZE;j++)
  //     {
  //       // dst.vec[j]=SUBDIV*5-j*200;
  //       dst.vec[j]=posMult*((int32_t)(esp_random()%(SUBDIV*10))-SUBDIV*5);
  //     }
  //     mstp.VecTo(dst,speed);
  //   }

  
  //   for(int j=0;j<MSTP_VEC_SIZE;j++)
  //   {
  //     // dst.vec[j]=SUBDIV*5-j*200;
  //     dst.vec[j]=10*10/mm_PER_REV*SUBDIV-j*SUBDIV;
  //   }
  //   mstp.VecTo(dst,speed);

  
  //   for(int j=0;j<MSTP_VEC_SIZE;j++)
  //   {
  //     // dst.vec[j]=SUBDIV*5-j*200;
  //     dst.vec[j]=-(10*10/mm_PER_REV*SUBDIV-j*SUBDIV);
  //   }
  //   mstp.VecTo(dst,speed);


  //   for(int j=0;j<MSTP_VEC_SIZE;j++)
  //   {
  //     dst.vec[j]=0;
  //   }
  //   mstp.VecTo(dst,speed);
  // }
  mstp.VecTo((xVec){100,99},speed);
  mstp.VecTo((xVec){0,0},speed);

}
void loop()
{
  static uint64_t preTick=0;
  uint64_t cur_tick=getCurTick();
  // oGS.mainLoop();
  delayMicroseconds(1001000);

  
  static uint32_t preccount=0;
  uint32_t ccount = XTHAL_GET_CCOUNT();
  // delay(1001);
  // Serial.printf("SysTick:%d\n", cur_tick-preTick);
  // Serial.printf("ccount:%d\n", ccount-preccount);
  preTick=cur_tick;
  preccount=ccount;
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
