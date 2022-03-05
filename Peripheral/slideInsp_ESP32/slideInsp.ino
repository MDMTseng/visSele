
#include "main.hpp"


#include <Data_Layer_Protocol.hpp>
#include <ArduinoJson.h>
#define __PRT_D_(fmt,...) //Serial.printf("D:"__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __PRT_I_(fmt,...) Serial.printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)

// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6)
const int O_CameraPin = 32;
bool O_CameraPin_ON=true;
const int O_BackLight = 33;
bool O_BackLight_ON=true;
const int O_LED_Status = 2;

const int I_gate1Pin = 17;
bool I_gate1Pin_ON=true;






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
    //   // __PRT_D_("G2OK %lu: %lu\n",g2.start_pulse,g2.end_pulse);

    //   __PRT_D_("G2OK %lu\n",g2.mid_pulse);
    // }
    // if(g2.state==GateInfo::NEG_EDGE_NG)
    // {
    //   __PRT_D_("G2NG %lu\n",g2.mid_pulse);
    // }
    // if(g1.state==GateInfo::NEG_EDGE_OK)
    // {
    //   __PRT_D_("G1OK %lu\n",g1.mid_pulse);
    // }
    // if(g1.state==GateInfo::NEG_EDGE_NG)
    // {
    //   __PRT_D_("G1NG %lu\n",g1.mid_pulse);
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
        // __PRT_D_("pulseAdd objTrack.s:%d  \n", objTrack.size());


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
          __PRT_D_("ERROR: pulseDiff:%d TOO SHORT\n",pulseDiff);
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
          __PRT_D_("pulseDiff: %u pulse:%d \n", pulseDiff, plInfo->gate_pulse);

          __PRT_D_("BKL.s:%d BKH.s:%d CAM.s:%d\n",
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

      __PRT_D_("ERROR: cur_PULSE:%d track_PULSE:%d diff:%d OVERTIME\n",cur_tick,plInfo->gate_pulse,head_diff);
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
                       __PRT_D_("BKL off:%d\n", _timerCount););
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

  unsigned long skipPulseCount = 0;
  const uint32_t pulseSepMIN = 1000;
  uint32_t preAcceptedPulse=0;
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

    pinMode(I_gate1Pin, INPUT_PULLDOWN);
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

        uint32_t diff = plInfo.gate_pulse-preAcceptedPulse;
        bool accpeted=diff>pulseSepMIN;
        if(accpeted)
        {
          preAcceptedPulse=plInfo.gate_pulse;
          uint32_t targetOffset=1;//;
          int flashOffset=80;//esp:150(10us?)
          int flashTime=100;


          int camOffset=flashOffset+5;//esp:150(10us?)
          int camTime=100;

          ACT_PUSH_TASK(act_S.ACT_BACKLight1H, (&plInfo), flashOffset+ targetOffset, 0, );
          ACT_PUSH_TASK(act_S.ACT_BACKLight1L, (&plInfo), flashOffset+flashTime+ targetOffset, 0,);

          ACT_PUSH_TASK(act_S.ACT_CAM1, (&plInfo),          camOffset+targetOffset, 1, );
          ACT_PUSH_TASK(act_S.ACT_CAM1, (&plInfo),  camTime+camOffset+targetOffset, 2, );

        }
        else
        {
          skipPulseCount++;
          __PRT_D_("skip:%d  \n", skipPulseCount);
        }

        // __PRT_D_("pulseAdd objTrack.s:%d  \n", objTrack.size());


      }
    
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
    }

    {

      ACT_TRY_RUN_TASK(
          act_S.ACT_CAM1, _timerCount,
          // __PRT_D_("ACT_CAM1:%d\n", task->info);
          if (task->info == 1) {
            digitalWrite(O_CameraPin, 1);
          } else if (task->info == 2) {
            digitalWrite(O_CameraPin, 0);
          });

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1H, _timerCount,
                       digitalWrite(O_BackLight, 1););

      ACT_TRY_RUN_TASK(act_S.ACT_BACKLight1L, _timerCount,
                       digitalWrite(O_BackLight, 0););
    }
  }

  const int minWidth = 0;
  const int maxWidth = 800;

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
    uint8_t new_Sense = digitalRead(ginfo->pin)==I_gate1Pin_ON;
    
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

          // __PRT_D_("OK_");
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
void setup()
{

  pinMode(O_CameraPin, OUTPUT);
  pinMode(O_LED_Status, OUTPUT);
  pinMode(O_BackLight, OUTPUT);
  
  pinMode(I_gate1Pin, INPUT_PULLUP);
  // Serial.begin(921600);
  Serial.begin(230400);


  while(0)
  {
      
    delay(200);
    digitalWrite(O_CameraPin, 0);
    digitalWrite(O_BackLight, 0);
    digitalWrite(O_LED_Status, 0);
    
    delay(200);
    digitalWrite(O_CameraPin, 1);
    digitalWrite(O_BackLight, 1);
    digitalWrite(O_LED_Status, 1);
    __PRT_D_("OK_ g:%d\n",digitalRead(I_gate1Pin));
  }
  // // setup_comm();
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 100, true);
  timerAlarmEnable(timer);

  digitalWrite(O_BackLight, 1);
  digitalWrite(O_CameraPin, 1);
  delay(3000);
  // digitalWrite(O_BackLight, 0);
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







StaticJsonDocument <1024>doc;
StaticJsonDocument  <1024>retdoc;

class MData_uInsp:public Data_JsonRaw_Layer
{
  
  public:
  MData_uInsp():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
    sprintf(peerVERSION,"");
  }
  int recv_RESET()
  {

  } 
  int recv_ERROR(ERROR_TYPE errorcode)
  {

  }

  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
    
    if(opcode==1 )
    {
      doc.clear();
      retdoc.clear();
      DeserializationError error = deserializeJson(doc, raw);

      const char* type = doc["type"];
      // const char* id = doc["id"];
      if(strcmp(type,"RESET")==0)
      {
        return msg_printf("RESET_OK","");
      }
      else if(strcmp(type,"ask_JsonRaw_version")==0)
      {
        
        const char* _version = doc["version"];
        sprintf(peerVERSION,_version);
        return this->rsp_JsonRaw_version();
      }
      else if(strcmp(type,"rsp_JsonRaw_version")==0)
      {
        const char* _version = doc["version"];
        sprintf(peerVERSION,_version);
        return 0;
      }
      else if(strcmp(type,"PING")==0)
      {

        retdoc["type"]="PONG";
        retdoc["id"]=doc["id"];
        uint8_t buff[200];
        int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
        send_json_string(0,buff,slen,0);

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
                retdoc["id"]=doc["id"];
                uint8_t buff[200];
                int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
                send_json_string(0,buff,slen,0);
                break;
              }
              case -1://digital
              {
                int value=digitalRead(pinNo);
                
                retdoc["type"]="PIN_INFO";
                retdoc["value"]=value;
                retdoc["id"]=doc["id"];
                uint8_t buff[200];
                int slen=serializeJson(retdoc, (char*)buff,sizeof(buff));
                send_json_string(0,buff,slen,0);
                break;
              }
              case 0:digitalWrite(pinNo, LOW);break;
              case 1:digitalWrite(pinNo, HIGH);break;
            }
          }
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
        msg_printf("SEEYOU","BYE");
      }

    }
    return 0;


  }
  void connected(Data_Layer_IF* ch){}

  int send_data(int head_room,uint8_t *data,int len,int leg_room){
    Serial.write(data,len);
    return 0;
  }
  void disconnected(Data_Layer_IF* ch){}
  void DBGINFO()
  {
    
  }

  int close(){}

  
  char dbgBuff[500];
  int dbg_printf(const char *fmt, ...)
  {
    char *str=dbgBuff;
    int restL=sizeof(dbgBuff);
    {//start head
      int len=sprintf(str,"{\"dbgmsg\":\"");
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

  int msg_printf(const char *type,const char *fmt, ...)
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
};





MData_uInsp djrl;
void loop()
{
  {
    uint8_t recvBuf[50];
    if (Serial.available() > 0) {
      // read the incoming byte:
      // char c=Serial.read();
      // djrl.recv_data((uint8_t*)&c,1);
      int recvLen = Serial.read(recvBuf,sizeof(recvBuf));
      djrl.recv_data((uint8_t*)recvBuf,recvLen);

      // djrl.dbg_printf(">>new data, len:%d",recvLen);
      
    }
  }
  oGS.mainLoop();
  // loop_comm();
  // delay(1000);
  
}
