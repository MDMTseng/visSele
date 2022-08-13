
#include "main.hpp"

#include <EncoderCounter.hpp>

#include <Data_Layer_Protocol.hpp>
#include <ArduinoJson.h>
#define __PRT_D_(fmt,...) //djrl.dbg_printf("%04d %.*s:d " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)
// #define __PRT_I_(...) Serial.printf("I:" __VA_ARGS__)
#define __PRT_I_(fmt,...) djrl.dbg_printf("%04d %.*s:i " fmt,__LINE__,PRT_FUNC_LEN,__func__ , ##__VA_ARGS__)

const char *ModuleVer="0.0.1";

const char *ModuleType="ManualTapeReelInsp";


const int O_LEDPin = 2;
//O => 26 25 33 32
//I => 17 18 19 23

const int O_CameraPin = 33;
const int O_FlashLight = 32;
const int O_EM_STOP = 25;
const int O_TBD = 26;


const int I_EncAPin = 17;
const int I_EncBPin = 18;


bool O_CameraPin_ON=true;
bool O_FlashLight_ON=true;
bool O_EM_STOP_ON=true;
bool I_Enc_Inv=true;

// twoGateSense tGS;
int g_act_trigger_type=0;//0 for timer trigger, 1 for counter trigger
int g_flash_on_time=0;//from object detected how long to flash
int g_cam_trig_time=400;//from object detected how long to trigger camera
int g_flash_off_time=2000;//flash how long


// int g_act_trigger_type=1;//0 for timer trigger, 1 for counter trigger
// int g_flash_on_time=0;//from object detected how long to flash
// int g_cam_trig_time=1;//from object detected how long to trigger camera
// int g_flash_off_time=4;//flash how long




int encStepSkip=4;


int encStepsPerObject=4;
int ObjCountPreSet=5;




void genMachineSetup(JsonDocument &jdoc)
{

  jdoc["act_trigger_type"]=g_act_trigger_type;
  jdoc["flash_on_time"]=g_flash_on_time;
  jdoc["cam_trig_time"]=g_cam_trig_time;
  jdoc["flash_off_time"]=g_flash_off_time;
  // jdoc["pulse_sep_min"]=g_pulse_sep_min;
  // jdoc["pulse_width_min"]=g_pulse_width_min;
  // jdoc["pulse_width_max"]=g_pulse_width_max;
  // jdoc["pulse_debounce_high"]=g_pulse_debounce_high;
  // jdoc["pulse_debounce_low"]=g_pulse_debounce_low;

  jdoc["O_CameraPin_ON"]=O_CameraPin_ON;
  jdoc["O_FlashLight_ON"]=O_FlashLight_ON;
  jdoc["O_EM_STOP_ON"]=O_EM_STOP_ON;
  jdoc["I_Enc_Inv"]=I_Enc_Inv;

}

#define JSON_SETIF_ABLE(tarVar,jsonObj,key) \
  {if(jsonObj[key].is<typeof(tarVar)>()  ) tarVar=jsonObj[key];}

void setMachineSetup(JsonDocument &jdoc)
{
  JSON_SETIF_ABLE(g_act_trigger_type,jdoc,"act_trigger_type");
  JSON_SETIF_ABLE(g_flash_on_time,jdoc,"flash_on_time");
  JSON_SETIF_ABLE(g_cam_trig_time,jdoc,"cam_trig_time");
  JSON_SETIF_ABLE(g_flash_off_time,jdoc,"flash_off_time");
  // JSON_SETIF_ABLE(g_pulse_sep_min,jdoc,"pulse_sep_min");
  // JSON_SETIF_ABLE(g_pulse_width_min,jdoc,"pulse_width_min");
  // JSON_SETIF_ABLE(g_pulse_width_max,jdoc,"pulse_width_max");
  // JSON_SETIF_ABLE(g_pulse_debounce_high,jdoc,"pulse_debounce_high");
  // JSON_SETIF_ABLE(g_pulse_debounce_low,jdoc,"pulse_debounce_low");


  JSON_SETIF_ABLE(O_CameraPin_ON,jdoc,"O_CameraPin_ON");
  JSON_SETIF_ABLE(O_FlashLight_ON,jdoc,"O_FlashLight_ON");
  JSON_SETIF_ABLE(O_EM_STOP_ON,jdoc,"O_EM_STOP_ON");
  JSON_SETIF_ABLE(I_Enc_Inv,jdoc,"I_Enc_Inv");

}

static void enc_cb(void* arg);



EncoderCounter encoder(I_EncAPin,I_EncBPin,encStepSkip,enc_cb, NULL);
int32_t count=0;
static bool leds = false;


hw_timer_t *timer = NULL;

int timerRunStage_outSeq=0;
int timerRunStage=0;
int lastCount=0;

static IRAM_ATTR void enc_cb(void* arg) {
  //Serial.printf("enc cb: count: %d\n", enc->getCount());
  if(I_Enc_Inv) count=-encoder.getCount();
  else          count=-encoder.getCount();



  if(lastCount>=count)
  {
    lastCount=count;
    //reverse run skip
    return;
  }
  // digitalWrite(O_LEDPin, leds);
  // leds=!leds;
  int t=count%(encStepsPerObject*ObjCountPreSet);
  if(t==0)
  {
      
    if(timerRunStage!=0)
    {
      timerRunStage_outSeq++;
    }
    else if(g_act_trigger_type==0)
    {
      timerRunStage=1;
      timerAlarmWrite(timer, 1, true);
      timerAlarmEnable(timer);
    }
  }

  if(g_act_trigger_type==1)
  {
    
    static int pret=0;
    if(pret>t)pret=-1;
    if(t>=g_flash_on_time && pret<g_flash_on_time)
    {
      digitalWrite(O_LEDPin, 1);
      digitalWrite(O_FlashLight, O_FlashLight_ON);
    }
    if(t>=g_cam_trig_time && pret<g_cam_trig_time)
    {

      digitalWrite(O_CameraPin, O_CameraPin_ON);
    }
    if(t>=g_flash_off_time && pret<g_flash_off_time)
    {
      digitalWrite(O_FlashLight, !O_FlashLight_ON);
      digitalWrite(O_CameraPin, !O_CameraPin_ON);
      digitalWrite(O_LEDPin, 0);
    }

    pret=t;
  }
 
  lastCount=count;
  

}

int timer_run_count=0;
void IRAM_ATTR onTimer()
{
  timer_run_count++;

  switch(timerRunStage)
  {
    
    case 1:  
    {
      timerRunStage=20;
      int delay=g_flash_on_time;
      if(delay>0)
      {
        timerAlarmWrite(timer, delay, true);
      }
      else
      {
        onTimer();
      }
    }
    return;

    case 20:  
    {
      digitalWrite(O_LEDPin, 1);
      digitalWrite(O_FlashLight, O_FlashLight_ON);
      timerRunStage=30;
      int delay= g_cam_trig_time-g_flash_on_time;
      if(delay>0)
      {
        timerAlarmWrite(timer, delay, true);
      }
      else
      {
        onTimer();
      }
    }
    return;

    
    case 30:
    {
      digitalWrite(O_CameraPin, O_CameraPin_ON);
      timerRunStage=40;
      int delay= g_flash_off_time-g_cam_trig_time;
      if(delay>0)
      {
        timerAlarmWrite(timer, delay, true);
      }
      else
      {
        onTimer();
      }

    }
    return;
    
    case 40:
      digitalWrite(O_FlashLight, !O_FlashLight_ON);
      digitalWrite(O_CameraPin, !O_CameraPin_ON);
      digitalWrite(O_LEDPin, 0);

    
  }

  timerRunStage=0;
  timerAlarmDisable(timer);
  timerAlarmWrite(timer, 0, false);
}



#define S_ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))


#define TRACKING_LEN 10




void setup()
{

  pinMode(O_LEDPin, OUTPUT);
  pinMode(O_CameraPin, OUTPUT);
  pinMode(O_FlashLight, OUTPUT);
  pinMode(O_EM_STOP, OUTPUT);
  
  pinMode(I_EncAPin, INPUT_PULLUP);
  pinMode(I_EncBPin, INPUT_PULLUP);
  encoder.clearCount();
  encoder.setFilter(1023);
  // Serial.begin(921600);
  Serial.begin(460800);


  while(0)
  {
      
    delay(200);
    digitalWrite(O_CameraPin, 0);
    digitalWrite(O_FlashLight, 0);
    
    delay(200);
    digitalWrite(O_CameraPin, 1);
    digitalWrite(O_FlashLight, 1);
    __PRT_D_("OK_ g:%d\n",digitalRead(I_gate1Pin));
  }
  // // setup_comm();
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  // timerAlarmWrite(timer, 100, false);
  // timerAlarmWrite(timer, 1000000, true);
  // timerAlarmEnable(timer);

  digitalWrite(O_FlashLight, 1);
  digitalWrite(O_CameraPin, 1);
  delay(3000);
  // digitalWrite(O_FlashLight, 0);
  digitalWrite(O_CameraPin, 0);
  digitalWrite(O_FlashLight, 0);
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
        retdoc["ver"]=ModuleVer;
        retdoc["type"]=ModuleType;
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
      else if(strcmp(type,"BL_ON")==0)
      {
        digitalWrite(O_FlashLight, O_FlashLight_ON);
        doRsp=rspAck=true;

      }     
      else if(strcmp(type,"BL_OFF")==0)
      {
        digitalWrite(O_FlashLight, !O_FlashLight_ON);
        doRsp=rspAck=true;

      }
      else if(strcmp(type,"EM_STOP")==0)
      {
        int keep_ms=10;
        if(doc["keep_ms"].is<int>())
        {
          keep_ms=doc["keep_ms"];
        }
        digitalWrite(O_EM_STOP, O_EM_STOP_ON);
        delay(keep_ms);
        digitalWrite(O_EM_STOP, !O_EM_STOP_ON);

        doRsp=rspAck=true;

      }
      else if(strcmp(type,"EM_STOP_ON")==0)
      {
        digitalWrite(O_EM_STOP, O_EM_STOP_ON);
        doRsp=rspAck=true;

      }
      else if(strcmp(type,"EM_STOP_OFF")==0)
      {
        digitalWrite(O_EM_STOP, !O_EM_STOP_ON);
        doRsp=rspAck=true;

      }
      else if(strcmp(type,"Cam_Trigger")==0)
      {
        digitalWrite(O_CameraPin_ON, !O_CameraPin_ON);
        delay(10);
        digitalWrite(O_CameraPin_ON, O_CameraPin_ON);
        delay(100);
        digitalWrite(O_CameraPin_ON, !O_CameraPin_ON);


        doRsp=rspAck=true;

      }
      else if(strcmp(type,"Encoder_Reset")==0)
      {
        lastCount=0;
        encoder.clearCount();
        doRsp=rspAck=true;

      }
      else if(strcmp(type,"BYE")==0)
      {
        doRsp=rspAck=true;

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
int32_t last_count=0;
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
  // oGS.mainLoop();
  // loop_comm();
  if(last_count!=count)
  {
    djrl.dbg_printf("ENC:%06d timerC:%d timerRunStage:%d outTime:%d\n",count,timer_run_count,timerRunStage,timerRunStage_outSeq);
    last_count=count;
    delay(100);
  }
}