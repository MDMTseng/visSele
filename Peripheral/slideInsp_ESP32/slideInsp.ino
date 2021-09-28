
#include "main.hpp"

// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6)
const int O_CameraPin = 26;
const int O_BackLight = 27;
const int O_LED_Status = 2;

hw_timer_t *timer = NULL;
#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

struct GLOB_FLAGS
{
  bool printSTATEChange;
  bool printCurrentReading;
  int rec_copy2cache;
};

GLOB_FLAGS GLOB_F;

#define TRACKING_LEN 10
class twoGateSense
{
public:
  const int I_gate1Pin = 25;
  const int I_gate2Pin = 26;
  const bool senseFlip = false;

  const int pulseDiffMIN = 300;
  const int pulseDiffMAX = 5000;
  typedef struct GateInfo
  {
    uint32_t start_pulse;
    uint32_t end_pulse;
    uint32_t mid_pulse;
    uint16_t debunce;
    uint8_t cur_Sense;
    uint8_t pin;
    enum gateState
    {
      INIT = -1,
      NORMAL = 0,
      POS_EDGE = 1,
      NEG_EDGE_OK = 2,
      NEG_EDGE_NG = 3,
    } state;
  } GateInfo;

  typedef struct pipeLineInfo
  {
    uint32_t gate_pulse;
    uint32_t s_pulse;
    uint32_t e_pulse;
    uint32_t pulse_width;
    int8_t stage;
    int8_t sent_stage;
    int8_t notifMark;
    int16_t insp_status;
  } pipeLineInfo;

  struct ACT_INFO
  {
    pipeLineInfo *src;
    int info;
    uint32_t targetPulse;
  };

  struct ACT_SCH
  {
    RingBuf_Static<ACT_INFO, TRACKING_LEN>
        ACT_BACKLight1H,
        ACT_BACKLight1L;

    RingBuf_Static<ACT_INFO, (TRACKING_LEN * 2)>
        ACT_CAM1;
  } act_S;

  RingBuf_Static<pipeLineInfo, TRACKING_LEN> objTrack;
  GateInfo g1 = {0}, g2 = {0};
  twoGateSense()
  {

    pinMode(I_gate1Pin, INPUT_PULLUP);
    pinMode(I_gate2Pin, INPUT_PULLUP);
    g1.pin = I_gate1Pin;
    g1.state = GateInfo::INIT;
    g2.pin = I_gate2Pin;
    g2.state = GateInfo::INIT;
  }

#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info, cusCode_task) \
  {                                                                 \
    ACT_INFO *task;                                                 \
    task = (rb).getHead();                                          \
    if (task)                                                       \
    {                                                               \
      task->targetPulse = plinfo->gate_pulse + pulseOffset;         \
      task->src = plinfo;                                           \
      task->info = _info;                                           \
      cusCode_task(rb).pushHead();                                  \
    }                                                               \
  }

#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \
  {                                                   \
    ACT_INFO *task = act_rb.getTail();                \
    if (task)                                         \
    {                                                 \
      int32_t __diff = task->targetPulse - cur_pulse; \
      if (__diff <= 0)                                \
      {                                               \
        cmd_task                                      \
            act_rb.consumeTail();                     \
      }                                               \
    }                                                 \
  }

  void timerGateSensingRun(uint32_t cur_tick)
  {
    task_gateSensing(&g1,cur_tick);
    task_gateSensing(&g2,cur_tick);
    // if(g2.state==GateInfo::NEG_EDGE_OK)
    // {
    //   // Serial.printf("G2OK %lu: %lu\n",g2.start_pulse,g2.end_pulse);

    //   Serial.printf("G2OK %lu\n",g2.mid_pulse);
    // }
    // if(g2.state==GateInfo::NEG_EDGE_NG)
    // {
    //   Serial.printf("G2NG %lu\n",g2.mid_pulse);
    // }
    // if(g1.state==GateInfo::NEG_EDGE_OK)
    // {
    //   Serial.printf("G1OK %lu\n",g1.mid_pulse);
    // }
    // if(g1.state==GateInfo::NEG_EDGE_NG)
    // {
    //   Serial.printf("G1NG %lu\n",g1.mid_pulse);
    // }

    if (g1.state == GateInfo::NEG_EDGE_OK || g1.state == GateInfo::NEG_EDGE_NG)
    {
      twoGateSense::pipeLineInfo *plInfo = objTrack.getHead();
      if (plInfo != NULL)
      {
        plInfo->e_pulse = g1.end_pulse;
        plInfo->s_pulse = g1.start_pulse;
        plInfo->gate_pulse = g1.mid_pulse;
        plInfo->pulse_width = g1.start_pulse - g1.end_pulse;
        objTrack.pushHead();
        // Serial.printf("pulseAdd objTrack.s:%d  \n", objTrack.size());


        // uint32_t targetOffset=0;//;
        // ACT_PUSH_TASK(act_S.ACT_CAM1, plInfo,  0+targetOffset, 1, );
        // ACT_PUSH_TASK(act_S.ACT_CAM1, plInfo,  10+targetOffset, 2, );
      }
    }

    if (g2.state == GateInfo::NEG_EDGE_OK || g2.state == GateInfo::NEG_EDGE_NG)
    {
      twoGateSense::pipeLineInfo *plInfo = objTrack.getTail();
      if (plInfo != NULL)
      {
        uint32_t pulseDiff = g2.mid_pulse - plInfo->gate_pulse;
        if(pulseDiff<pulseDiffMIN)
        {
          Serial.printf("ERROR: pulseDiff:%d TOO SHORT\n",pulseDiff);
        }
        else
        {
          plInfo->gate_pulse = g2.mid_pulse;

          plInfo->gate_pulse = cur_tick;
          // uint32_t targetOffset=0;//;
          // int flashOffset=1450;//esp:150(10us?)
          // int flashTime=3;


          uint32_t targetOffset=pulseDiff;//;
          int flashOffset=1;//esp:150(10us?)
          
          int camOffset=1;//esp:150(10us?)
          int camTime=10;

          int flashTime=1;
          
          ACT_PUSH_TASK(act_S.ACT_BACKLight1H, plInfo, flashOffset+ targetOffset, 0, );
          ACT_PUSH_TASK(act_S.ACT_BACKLight1L, plInfo, flashOffset+flashTime+ targetOffset, 0,);

          ACT_PUSH_TASK(act_S.ACT_CAM1, plInfo,          camOffset+targetOffset, 1, );
          ACT_PUSH_TASK(act_S.ACT_CAM1, plInfo,  camTime+camOffset+targetOffset, 2, );
          Serial.printf("pulseDiff: %u pulse:%d \n", pulseDiff, plInfo->gate_pulse);

          Serial.printf("BKL.s:%d BKH.s:%d CAM.s:%d\n",
                        act_S.ACT_BACKLight1L.size(), act_S.ACT_BACKLight1H.size(),
                        act_S.ACT_CAM1.size());
        }
        objTrack.consumeTail();
      }
    }
  }

  void cleanUpTimeoutTrackRun(uint32_t cur_tick)
  {
    twoGateSense::pipeLineInfo *plInfo = objTrack.getTail();
    if(plInfo==NULL)return;
    int32_t head_diff= cur_tick-plInfo->gate_pulse;
    if(head_diff>pulseDiffMAX)
    {

      Serial.printf("ERROR: cur_PULSE:%d track_PULSE:%d diff:%d OVERTIME\n",cur_tick,plInfo->gate_pulse,head_diff);
      objTrack.consumeTail();
    }

  }

  uint32_t _timerCount = 0;
  void timerRun()
  {
    _timerCount++;
    int subTick = (_timerCount & 0b11);
    if (subTick == 0)
    {
      timerGateSensingRun(_timerCount);
    }
    else if (subTick == 1)
    {
      cleanUpTimeoutTrackRun(_timerCount);
    }

    {

      ACT_TRY_RUN_TASK(
          act_S.ACT_CAM1, _timerCount,

          if (task->info == 1) {
            digitalWrite(O_CameraPin, 1);
          } else if (task->info == 2) {
            digitalWrite(O_CameraPin, 0);
          });

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1H, _timerCount,
                       digitalWrite(O_BackLight, 1););

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1L, _timerCount,
                       digitalWrite(O_BackLight, 0);
                       Serial.printf("BKL off:%d\n", _timerCount););
    }
  }

  const int minWidth = 0;
  const int maxWidth = 50;

  const int DEBOUNCE_L_THRES = 5; //the keep L length to stablize as L state, object inner connection
  const int DEBOUNCE_H_THRES = 1; //the keep H length to stablize as H state 

  //example Set: 
  //H_THRES 4 
  //L_THRES 8
  //
  //                    V the H signal is too short 
  //raw        0000011111100111111111111000000000000000000111000000 note: H234v-> H state pass. H23x->H state debounce
  //                H234v L2x           L2345678v         H23x      note: L2x  -> H state too short.
  //debounce   0000000001111111111111111111111111000000000000000000 
  //                <H4-                <---L8---                    note: offset time according to HL debounce length 
  //offset     0000011111111111111111111000000000000000000000000000  
  //                |---Width--(20)----|
  //                ^start_pulse       ^end_pulse
  //  The width will need to pass the width filter to determin the final acceptance of this pulse
  
  void task_gateSensing(GateInfo *ginfo,uint32_t cur_tick)
  {
    //(perRevPulseCount/50)
    uint8_t new_Sense = digitalRead(ginfo->pin);
    if (senseFlip)
    {
      new_Sense = !new_Sense;
    }

    if (ginfo->state == GateInfo::INIT)
    {
      if (!new_Sense)
      {
        ginfo->debunce = DEBOUNCE_H_THRES;
        ginfo->cur_Sense = new_Sense;
        ginfo->start_pulse = cur_tick;
        ginfo->state = GateInfo::NORMAL; //wait for first low
      }
      return;
    }

    bool flip = false;

    if (ginfo->cur_Sense)
    {                 //H
      if (!new_Sense) //L
      {
        ginfo->debunce--;
        if (ginfo->debunce == 0)
        {
          flip = true;
          ginfo->end_pulse = cur_tick - DEBOUNCE_L_THRES;
          ginfo->debunce = DEBOUNCE_H_THRES;
        }
      }
      else
      {
        ginfo->debunce = DEBOUNCE_L_THRES;
      }
    }
    else
    {                //L cur_Sense
      if (new_Sense) //H
      {
        ginfo->debunce--;
        if (ginfo->debunce == 0)
        {
          flip = true;
          ginfo->start_pulse = cur_tick - DEBOUNCE_H_THRES;
          ginfo->debunce = DEBOUNCE_L_THRES;
        }
      }
      else
      {
        ginfo->debunce = DEBOUNCE_H_THRES;
      }
    }

    ginfo->state = GateInfo::NORMAL;

    if (flip)
    {

      if (!new_Sense)
      { //a pulse is completed

        ginfo->state = GateInfo::NEG_EDGE_NG;
        uint32_t diff = ginfo->end_pulse - ginfo->start_pulse;
        ginfo->mid_pulse = ginfo->end_pulse;//ginfo->start_pulse + (diff >> 1);
        if (diff > minWidth && diff < maxWidth)
        {

          ginfo->state = GateInfo::NEG_EDGE_OK;
        }
        else
        {
          //skip the pulse : the pulse width is not in the valid range
          //this might be caused by too large object > typ:2cm
          //or there are multiple objects too close to each other
          //control by   minWidth,maxWidth,
          //also effected DEBOUNCE_L_THRES,DEBOUNCE_H_THRES(these two are to control what is a complete pulse high time, low time)
        }
      }
      else
      {
        ginfo->state = GateInfo::POS_EDGE;
      }

      ginfo->cur_Sense = new_Sense;
    }
  }

  void mainLoop()
  {
  }
};

// twoGateSense tGS;

class oneGateSense
{
public:
  const int I_gate1Pin = 17;
  const bool senseFlip = false;

  const int pulseDiffMIN = 300;
  const int pulseDiffMAX = 5000;
  typedef struct GateInfo
  {
    uint32_t start_pulse;
    uint32_t end_pulse;
    uint32_t mid_pulse;
    uint16_t debunce;
    uint8_t cur_Sense;
    uint8_t pin;
    enum gateState
    {
      INIT = -1,
      NORMAL = 0,
      POS_EDGE = 1,
      NEG_EDGE_OK = 2,
      NEG_EDGE_NG = 3,
    } state;
  } GateInfo;

  typedef struct pipeLineInfo
  {
    uint32_t gate_pulse;
    uint32_t s_pulse;
    uint32_t e_pulse;
    uint32_t pulse_width;
    int8_t stage;
    int8_t sent_stage;
    int8_t notifMark;
    int16_t insp_status;
  } pipeLineInfo;

  struct ACT_INFO
  {
    // pipeLineInfo *src;
    int info;
    uint32_t targetPulse;
  };

  struct ACT_SCH
  {
    RingBuf_Static<ACT_INFO, TRACKING_LEN>
        ACT_BACKLight1H,
        ACT_BACKLight1L;

    RingBuf_Static<ACT_INFO, (TRACKING_LEN * 2)>
        ACT_CAM1;
  } act_S;

  GateInfo g1 = {0};
  oneGateSense()
  {

    pinMode(I_gate1Pin, INPUT_PULLUP);
    g1.pin = I_gate1Pin;
    g1.state = GateInfo::INIT;
  }

#define ACT_PUSH_TASK(rb, plinfo, pulseOffset, _info, cusCode_task) \
  {                                                                 \
    ACT_INFO *task;                                                 \
    task = (rb).getHead();                                          \
    if (task)                                                       \
    {                                                               \
      task->targetPulse = plinfo->gate_pulse + pulseOffset;         \
      task->info = _info;                                           \
      cusCode_task(rb).pushHead();                                  \
    }                                                               \
  }

#define ACT_TRY_RUN_TASK(act_rb, cur_pulse, cmd_task) \
  {                                                   \
    ACT_INFO *task = act_rb.getTail();                \
    if (task)                                         \
    {                                                 \
      int32_t __diff = task->targetPulse - cur_pulse; \
      if (__diff <= 0)                                \
      {                                               \
        cmd_task                                      \
            act_rb.consumeTail();                     \
      }                                               \
    }                                                 \
  }

  void timerGateSensingRun(uint32_t cur_tick)
  {
    task_gateSensing(&g1,cur_tick);

    if (g1.state == GateInfo::NEG_EDGE_OK)
    {
      twoGateSense::pipeLineInfo plInfo;

      {
        plInfo.e_pulse = g1.end_pulse;
        plInfo.s_pulse = g1.start_pulse;
        plInfo.gate_pulse = g1.mid_pulse;
        plInfo.pulse_width = g1.start_pulse - g1.end_pulse;
        // Serial.printf("pulseAdd objTrack.s:%d  \n", objTrack.size());


        uint32_t targetOffset=1;//;
        int flashOffset=1;//esp:150(10us?)
        int flashTime=100;


        int camOffset=1;//esp:150(10us?)
        int camTime=100;





        ACT_PUSH_TASK(act_S.ACT_BACKLight1H, (&plInfo), flashOffset+ targetOffset, 0, );
        ACT_PUSH_TASK(act_S.ACT_BACKLight1L, (&plInfo), flashOffset+flashTime+ targetOffset, 0,);

        ACT_PUSH_TASK(act_S.ACT_CAM1, (&plInfo),          camOffset+targetOffset, 1, );
        ACT_PUSH_TASK(act_S.ACT_CAM1, (&plInfo),  camTime+camOffset+targetOffset, 2, );

      }
    
    }
  }

  void cleanUpTimeoutTrackRun(uint32_t cur_tick)
  {


  }

  uint32_t _timerCount = 0;
  void timerRun()
  {
    _timerCount++;
    int subTick = (_timerCount & 0b11);
    if (subTick == 0)
    {
      timerGateSensingRun(_timerCount);
    }
    else if (subTick == 1)
    {
      cleanUpTimeoutTrackRun(_timerCount);
    }

    {

      ACT_TRY_RUN_TASK(
          act_S.ACT_CAM1, _timerCount,
          // Serial.printf("ACT_CAM1:%d\n", task->info);
          if (task->info == 1) {
            digitalWrite(O_CameraPin, 1);
          } else if (task->info == 2) {
            digitalWrite(O_CameraPin, 0);
          });

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1H, _timerCount,
                       digitalWrite(O_BackLight, 1););

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1L, _timerCount,
                       digitalWrite(O_BackLight, 0);
                       Serial.printf("BKL off:%d\n", _timerCount););
    }
  }

  const int minWidth = 0;
  const int maxWidth = 500;

  const int DEBOUNCE_L_THRES = 5; //the keep L length to stablize as L state, object inner connection
  const int DEBOUNCE_H_THRES = 1; //the keep H length to stablize as H state 

  //example Set: 
  //H_THRES 4 
  //L_THRES 8
  //
  //                    V the H signal is too short 
  //raw        0000011111100111111111111000000000000000000111000000 note: H234v-> H state pass. H23x->H state debounce
  //                H234v L2x           L2345678v         H23x      note: L2x  -> H state too short.
  //debounce   0000000001111111111111111111111111000000000000000000 
  //                <H4-                <---L8---                    note: offset time according to HL debounce length 
  //offset     0000011111111111111111111000000000000000000000000000  
  //                |---Width--(20)----|
  //                ^start_pulse       ^end_pulse
  //  The width will need to pass the width filter to determin the final acceptance of this pulse
  
  void task_gateSensing(GateInfo *ginfo,uint32_t cur_tick)
  {
    //(perRevPulseCount/50)
    uint8_t new_Sense = digitalRead(ginfo->pin);
    
    // Serial.print(new_Sense);
    if (senseFlip)
    {
      new_Sense = !new_Sense;
    }

    if (ginfo->state == GateInfo::INIT)
    {
      if (!new_Sense)
      {
        ginfo->debunce = DEBOUNCE_H_THRES;
        ginfo->cur_Sense = new_Sense;
        ginfo->start_pulse = cur_tick;
        ginfo->state = GateInfo::NORMAL; //wait for first low
      }
      return;
    }

    bool flip = false;

    if (ginfo->cur_Sense)
    {                 //H
      if (!new_Sense) //L
      {
        ginfo->debunce--;
        if (ginfo->debunce == 0)
        {
          flip = true;
          ginfo->end_pulse = cur_tick - DEBOUNCE_L_THRES;
          ginfo->debunce = DEBOUNCE_H_THRES;
        }
      }
      else
      {
        ginfo->debunce = DEBOUNCE_L_THRES;
      }
    }
    else
    {                //L cur_Sense
      if (new_Sense) //H
      {
        ginfo->debunce--;
        if (ginfo->debunce == 0)
        {
          flip = true;
          ginfo->start_pulse = cur_tick - DEBOUNCE_H_THRES;
          ginfo->debunce = DEBOUNCE_L_THRES;
        }
      }
      else
      {
        ginfo->debunce = DEBOUNCE_H_THRES;
      }
    }

    ginfo->state = GateInfo::NORMAL;

    if (flip)
    {

      if (!new_Sense)
      { //a pulse is completed
        Serial.println("N pulse");
        ginfo->state = GateInfo::NEG_EDGE_NG;
        uint32_t diff = ginfo->end_pulse - ginfo->start_pulse;
        ginfo->mid_pulse = ginfo->end_pulse;//ginfo->start_pulse + (diff >> 1);
        if (diff > minWidth && diff < maxWidth)
        {

          Serial.println("OK_");
          ginfo->state = GateInfo::NEG_EDGE_OK;
        }
        else
        {
          //skip the pulse : the pulse width is not in the valid range
          //this might be caused by too large object > typ:2cm
          //or there are multiple objects too close to each other
          //control by   minWidth,maxWidth,
          //also effected DEBOUNCE_L_THRES,DEBOUNCE_H_THRES(these two are to control what is a complete pulse high time, low time)
        }
      }
      else
      {
        ginfo->state = GateInfo::POS_EDGE;
      }

      ginfo->cur_Sense = new_Sense;
    }
  }

  void mainLoop()
  {
  }
};



oneGateSense oGS;

void IRAM_ATTR onTimer()
{
  oGS.timerRun();
}
StaticJsonDocument<1024> recv_doc;
StaticJsonDocument<1024> ret_doc;
void setup()
{

  pinMode(O_CameraPin, OUTPUT);
  pinMode(O_LED_Status, OUTPUT);
  pinMode(O_BackLight, OUTPUT);

  // digitalWrite(O_LED_Status, 1);
  // digitalWrite(O_BackLight, 1);

  // digitalWrite(O_CameraPin, 0);
  // delay(10);
  // digitalWrite(O_CameraPin, 1);

  Serial.begin(921600);

  // // setup_comm();
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 100, true);
  timerAlarmEnable(timer);

  digitalWrite(O_BackLight, 1);
  digitalWrite(O_CameraPin, 1);
  delay(3000);
  digitalWrite(O_BackLight, 0);
  digitalWrite(O_CameraPin, 0);
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

void loop()
{
  oGS.mainLoop();
  // loop_comm();
  delay(1000);
  
}
