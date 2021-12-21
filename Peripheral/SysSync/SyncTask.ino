
#include "main.hpp"
#include "MSteppers.hpp"
#include "xtensa/core-macros.h"
hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define PIN_O1 5


#define PIN_M1_DIR 32
#define PIN_M1_STP 33
#define PIN_M2_DIR 12
#define PIN_M2_STP 13


class MStp_M:public MStp{
  public:


  MStp_M(RingBuf<struct runBlock> *_blocks):MStp(_blocks)
  {
    pinMode(PIN_M1_DIR, OUTPUT);
    pinMode(PIN_M1_STP, OUTPUT);
    pinMode(PIN_M2_DIR, OUTPUT);
    pinMode(PIN_M2_STP, OUTPUT);
  }


  void BlockRunEffect() override
  {

    if(axis_RUNState==0)
    {
      digitalWrite(PIN_M1_STP, 0);
      digitalWrite(PIN_M2_STP, 0);
      axis_RUNState=1;
      return;
    }
    axis_RUNState=0;
    // Serial.printf("p:%s",int2bin(axis_pul));
    // Serial.printf(" d:%s \n",int2bin(axis_dir));

    uint32_t readHead=1<<0;

    // for(int i=0;i<VEC_SIZE;i++,readHead<<=1)
    {
      
      if(axis_pul&1)
      {
        digitalWrite(PIN_M1_STP, 1);
        digitalWrite(PIN_M1_DIR, axis_dir&1!=0);
      }

      
      if(axis_pul&2)
      {
        digitalWrite(PIN_M2_STP, 1);
        digitalWrite(PIN_M2_DIR, axis_dir&2!=0);
      }


    }

  }

  

  void stopTimer() override
  {
  }
  
  void startTimer() override
  {
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

  int highT=5;
  mstp.BlockRunEffect();
  if(mstp.axis_RUNState==0)
  {
    timerAlarmWrite(timer,highT, true);
  }
  else
  {
    mstp.timerTask();
    uint32_t nextT= mstp.T_next;
    if(nextT<highT*2)nextT=highT*2;
    timerAlarmWrite(timer,nextT-highT, true);
  }



  // 
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;
void setup()
{

  Serial.begin(921600);

  // // setup_comm();
  timer = timerBegin(0, 80*100, true);
  
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);
  pinMode(PIN_O1, OUTPUT);


  
  mstp.VecTo((xVec){.vec={200,150,0}},200);
  // mstp.VecTo((xVec){.vec={100,-300,-50}});
  mstp.VecTo((xVec){.vec={0,0,0}},200);


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
