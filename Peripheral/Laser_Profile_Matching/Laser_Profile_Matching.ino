#include"UTIL.h"
#include"RingBuf.hpp"
#include"SimpPacketParse.hpp"
#include <ArduinoJson.h>
// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6)
const int potPin = 34;

const int selectActPin = 33;


volatile int interruptCounter = 0;
int totalInterruptCounter = 0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


#define templateSIZE 100

#define S_ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
int16_t test_template[] = {1522,
1668,1711,1729,1744,1744,1721,1706,1690,1649,1593,1668,1535,1527,1533,1759,1907,1977,2096,2134,1867,1732,2149,2608,2195,1766,1776,1809,1888,1889,1887,1770,1313,
1402,1776,1907,1756,1443,1241,1294,1296,1251,1427,1871,1705,1296};


struct GLOB_FLAGS {
  bool printSTATEChange;
  bool printCurrentReading;
  int  rec_copy2cache;
  
};



bool OK_NG_FLIP=false;
GLOB_FLAGS GLOB_F;

class profileInspData {
  public:


    bool STATE_LOCK;

    mArray<mArray<int16_t>> tempCollection;

#define ACT_TIMMING_BUFFER_SIZE 18

    uint16_t stepCounter;
    RingBuf<uint16_t> act1TimingQ;
    RingBuf<uint16_t> act2TimingQ;

    
    enum _detection_state{
      S_RESET,
      S_WAITING,
      S_RECORDING,
      S_CLEAN,
    };
    
    enum _detection_action{
      A_NA,
      A_OK,
      A_NG,
      A_ER,
    };



    mArray<uint16_t> object_record;
    mArray<uint16_t> object_record_cache;
    
    struct _matching_subject{
      uint16_t step_start;
      uint16_t step_end;
      enum{
        NORMAL,
        START_ONLY,
        END_ONLY
      } stepType;
      typeof(object_record)* recordInfo;
    };

    RingBuf<_matching_subject> object_track;//to track object

    struct{
      uint16_t aheadSpace;
      uint16_t tailSpace;
      uint16_t BGValueH;
      uint16_t NAValue;
      uint8_t LOW_STATE_DEBOUNCE_COUNTER;
      uint8_t HIGH_STATE_DEBOUNCE_COUNTER;
      uint8_t state_Counter;
      uint8_t utilCounter;
      _detection_action act;
      _detection_state state,pre_state;
      uint16_t act1TimingQ_std_offset;
      uint16_t act2TimingQ_std_offset;
      _matching_subject cur_object;
    }s_data;

    profileInspData()
    {
      object_record.RESET(templateSIZE);
      object_record_cache.RESET(templateSIZE*2);
      act1TimingQ.RESET(ACT_TIMMING_BUFFER_SIZE);
      act2TimingQ.RESET(ACT_TIMMING_BUFFER_SIZE);
      object_track.RESET(5);
      RESET();
    }

    void RESET()
    {
      
      s_data.BGValueH = 1200;
      s_data.aheadSpace = 6;
//      s_data.tailSpace = 15;
      
      s_data.NAValue = 3500;
      s_data.HIGH_STATE_DEBOUNCE_COUNTER = 1;
      s_data.LOW_STATE_DEBOUNCE_COUNTER = 10;
      s_data.pre_state=
      s_data.state = S_RESET;
      stepCounter=0;

      s_data.act1TimingQ_std_offset=172;
      s_data.act2TimingQ_std_offset=175;
      

      
      object_record.resize(0);
      act1TimingQ.clear();
      act2TimingQ.clear();
      object_track.clear();
    }

    void stateSwitch(_detection_action action)
    {
      _detection_state new_detection_state=s_data.state;
      switch (s_data.state) //state switch
      {

        case S_RESET:
          if(action==A_OK)
            new_detection_state=S_WAITING;
          break;

        case S_WAITING:
          if(action==A_OK)
            new_detection_state=S_RECORDING;
          break;
        case S_RECORDING:
          if(action==A_OK)
            new_detection_state=S_WAITING;
          else if(action==A_ER)
            new_detection_state=S_CLEAN;
          break;
        case S_CLEAN:
          if(action==A_OK)
            new_detection_state=S_WAITING;
          break;
      }

      if(new_detection_state!=s_data.state)
      {
        s_data.pre_state=s_data.state;
        s_data.state=new_detection_state;
        stateAction(-1,s_data.state);//exit current state
        s_data.state_Counter=-1;
        stateAction(1,s_data.state);//enter new state
        if(GLOB_F.printSTATEChange)
        {
          Serial.print(s_data.pre_state);
          Serial.print("+[");
          Serial.print(action);
          Serial.print("]=");
          Serial.println(s_data.state);
        }
      }

    }
    
    //exit_current_enter -1  state exit action
    //exit_current_enter  0  state action
    //exit_current_enter  1  state enter action
    //stateAction SHOULD always be called by timer thread/interrupt
    void stateAction(int exit_current_enter,_detection_state state)
    {
      s_data.state_Counter++;
      const int ECE=exit_current_enter;
      const int potValue = analogRead(potPin);

      object_record_cache.push_back(potValue);
      
      if ((stepCounter&0x3FF)==0 && GLOB_F.printCurrentReading)
      {
        Serial.print("INT AR: ");
        Serial.println(potValue);
      }
      switch (state) //state switch
      {

        case S_RESET:
        {//waiting for low state
          if(ECE==-1)//exit
          {
            break;
          }
          if(ECE==1)
          {
            s_data.utilCounter=0;
            break;
          }


          if(potValue<s_data.BGValueH)//LOW state
          {
            
            if(s_data.utilCounter<s_data.LOW_STATE_DEBOUNCE_COUNTER)
            {
              s_data.utilCounter++; 
            }
            else
            {
              stateSwitch(A_OK);
            }
          }
          else
          {
            s_data.utilCounter=0;
          } 
          
          break;
        }
        case S_WAITING:
        {//wait for stable high state and try to obtain available recorder
          
          if(ECE==-1)//state exit
          {
            break;
          }
          if(ECE==1)
          {
            s_data.utilCounter=0;
            s_data.cur_object.step_start=0;
            s_data.cur_object.step_end=0;
            s_data.cur_object.stepType=_matching_subject::NORMAL;
            s_data.cur_object.recordInfo=NULL;
            
            break;
          }

          

          if(potValue>s_data.BGValueH)//HIGH state
          {
            
            if(object_record.size()==0 && s_data.state_Counter>s_data.aheadSpace)//object_record is free, assign to cur_object
            {
              s_data.cur_object.recordInfo=&object_record;
              
            }
            if(s_data.cur_object.recordInfo!=NULL)//if cur_object has recorder, push new reading data in
            {
              s_data.cur_object.recordInfo->push_back(potValue);
            }

            
            if(s_data.utilCounter<s_data.HIGH_STATE_DEBOUNCE_COUNTER)
            {
              s_data.utilCounter++; 
            }
            else
            {//the high reading is stable, try to keep the step_start(where the high reading starts) info and put OK action into state machine
              s_data.cur_object.step_start=stepCounter-s_data.HIGH_STATE_DEBOUNCE_COUNTER;
              stateSwitch(A_OK);
            }
          }
          else
          {//high reading stops, reset the counter
            if(s_data.cur_object.recordInfo!=NULL)
              s_data.cur_object.recordInfo->resize(0);
            s_data.utilCounter=0;
          } 
          break;
        }
        case S_RECORDING:
        {
          //the recorder might not be here
          //wait for stable low state
          //and put 
          if(ECE==1)
          {
            s_data.utilCounter=0;
            break;
          }
          if(ECE==-1)
          {
            break;
          }
          
          
          if(s_data.cur_object.recordInfo!=NULL)//if cur_object has recorder
          {
            if(s_data.cur_object.recordInfo->push_back(potValue)==false)//false means the buffer is full => reset and release the recorder
            {
              s_data.cur_object.recordInfo->resize(0);//reset
              s_data.cur_object.recordInfo=NULL;//release
              
              stateSwitch(A_ER);
              break;
            }
          }
          else if(s_data.state_Counter>(templateSIZE-s_data.LOW_STATE_DEBOUNCE_COUNTER))//if there was a recorder, will it be full?
          {//if so put NA event
            stateSwitch(A_ER);
            break;
          }
          
          if(potValue<s_data.BGValueH)//LOW state
          {
            if(s_data.utilCounter<s_data.LOW_STATE_DEBOUNCE_COUNTER)
            {
              s_data.utilCounter++; 
            }
            else
            {//stable LOW state
              if(s_data.cur_object.recordInfo!=NULL)
              {
                s_data.cur_object.recordInfo->resize(object_record.size()-s_data.LOW_STATE_DEBOUNCE_COUNTER);//roll back the LOW_STATE_DEBOUNCE_COUNTER steps
              }
              
              s_data.cur_object.step_end=stepCounter-s_data.LOW_STATE_DEBOUNCE_COUNTER;//roll back the LOW_STATE_DEBOUNCE_COUNTER steps
              object_track.pushHead(s_data.cur_object);//put current object into the tracking list
              s_data.cur_object.step_end=s_data.cur_object.step_start=0;
              s_data.cur_object.recordInfo=NULL;
              
              stateSwitch(A_OK);
            }
          }
          else
          {
            s_data.utilCounter=0;
          } 
          break;
        }

        
        case S_CLEAN://Waiting for stable low
        {
          if(ECE==1)
          {
            s_data.utilCounter=0;
            _matching_subject tmp={0};
            if(s_data.cur_object.recordInfo!=NULL)
            {
              s_data.cur_object.recordInfo->resize(0);
              s_data.cur_object.recordInfo=NULL;
            }
            tmp.recordInfo=NULL;
            tmp.step_start=s_data.cur_object.step_start;
            tmp.stepType=_matching_subject::START_ONLY;

      
            object_track.pushHead(tmp);//put current object into the tracking list
            break;
          }
          if(ECE==-1)
          {
            break;
          }
          
          if(potValue<s_data.BGValueH)//LOW state
          {
            if(s_data.utilCounter<s_data.LOW_STATE_DEBOUNCE_COUNTER)
            {
              s_data.utilCounter++; 
            }
            else
            {//stable LOW state
               _matching_subject tmp={0};
               
              tmp.recordInfo=NULL;
              tmp.step_end=stepCounter-s_data.LOW_STATE_DEBOUNCE_COUNTER;//roll back the LOW_STATE_DEBOUNCE_COUNTER steps
              tmp.stepType=_matching_subject::END_ONLY;
              object_track.pushHead(tmp);//put current object into the tracking list
              stateSwitch(A_OK);
            }
          }
          else
          {
            s_data.utilCounter=0;
          } 
          
        }
        break;
      }

    }
    void timerRun() {
      stepCounter++;
      stateAction(0,s_data.state);  

      //act queue

      if(act1TimingQ.size()>0)
      {
        uint16_t timing =*act1TimingQ.getTail();
        if(timing==stepCounter)//check if the step is here
        {
          //start air blow
          
//          Serial.println("A1");
          digitalWrite(selectActPin, HIGH);
          act1TimingQ.consumeTail();
        }
      }
       
      if(act2TimingQ.size()>0)
      {
        uint16_t timing =*act2TimingQ.getTail();
        if(timing==stepCounter)
        {
          //stop air blow
//          Serial.println("A2");
          digitalWrite(selectActPin, LOW);
          act2TimingQ.consumeTail();
        }
      }
       
    }

    void mainLoop()
    {
      if(object_track.size()!=0)
      {
        //process the head
        _matching_subject* obj =object_track.getTail();

//        Serial.print("obj->stepType:");
//        Serial.println(obj->stepType);
//        
//        Serial.print("stepCounter:");
//        Serial.print(stepCounter);
//
//        
//        Serial.print(" :1>");
//        Serial.print(obj->step_start+s_data.act1TimingQ_std_offset);
//        Serial.print(" :2>");
//        Serial.println(obj->step_end+s_data.act2TimingQ_std_offset);


//        Serial.print("A1Q:");
//        Serial.print(act1TimingQ.size());
//        Serial.print(" A2Q:");
//        Serial.print(act2TimingQ.size());
//        Serial.print(" OTQ:");
//        Serial.println(object_track.size());
        
        switch(obj->stepType)
        {
          case _matching_subject::NORMAL:
          {
            bool doPass=false;
            /**/ 
            if(obj->recordInfo!=NULL)
            {
                
//        Serial.print("recLen:");
//        Serial.println(obj->recordInfo->size());
//              doPass=OK_NG_FLIP;
//              OK_NG_FLIP=!OK_NG_FLIP;
              auto& record=*(obj->recordInfo);
//              delay(3);

//              if(object_record_cache.size()==0)
//              {
//                for (int i = 0; i < record.size(); i++)
//                {
//                  object_record_cache.arr[i]=record.arr[i];
//                }
//                object_record_cache.resize(record.size());
//              }

              int NA_Count = 0;
              int16_t NA_Thres = 3000;
              uint32_t diffSum = 999;
              NA_Count=999;

              const int scaleMult=7;
              if((record.size()*10)>(S_ARR_LEN(test_template)*scaleMult) &&  (record.size()*scaleMult)<(S_ARR_LEN(test_template)*10)  )
              {
                diffSum = tempSAD2(
                 (int16_t*)record.arr,      record.size(),
                           test_template,   S_ARR_LEN(test_template), NA_Thres, &NA_Count);
              }


              if (GLOB_F.printCurrentReading)
              {
                Serial.print("@SE{\"RECORD\":[");
                for (int i = 0; i < record.size(); i++)
                {
                  Serial.print((int)record.arr[i]);
                  if(i!=record.size()-1)
                    Serial.print(',');
                  if ((i & 0x1F) == 0x1F)Serial.println();
                }
                
                Serial.println("]}$");
    
                Serial.print("d1:");
                Serial.print(diffSum);
                Serial.print("(");
                Serial.println(NA_Count);
              }

              if ( (NA_Count < 2 && diffSum < 100)  )
              {
                doPass = true;
              }
              doPass = false;
              obj->recordInfo->resize(0);
            }
    
             
//            Serial.println("NORMAL");

            if(doPass==false)
            {
              act1TimingQ.pushHead(obj->step_start+s_data.act1TimingQ_std_offset);
              act2TimingQ.pushHead(obj->step_end  +s_data.act2TimingQ_std_offset);
            }
          }

          break;
          case _matching_subject::START_ONLY:
            act1TimingQ.pushHead(obj->step_start+s_data.act1TimingQ_std_offset);
          break;
          case _matching_subject::END_ONLY:
            act2TimingQ.pushHead(obj->step_end  +s_data.act2TimingQ_std_offset);
          break;
        }
        object_track.consumeTail();
      }
    }
};

profileInspData pID;


void IRAM_ATTR onTimer()
{
  pID.timerRun();

}
StaticJsonDocument<1024> recv_doc;
DynamicJsonDocument ret_doc(4096);
void setup() {
  pinMode(selectActPin, OUTPUT);



  //  digitalWrite(selectActPin, HIGH);

  Serial.begin(115200);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000, true);
  timerAlarmEnable(timer);

}

     
int intArrayContent_ToJson(char* jbuff,uint32_t jbuffL, int16_t *intarray,int intarrayL)
{
  uint32_t MessageL=0;
                                                
  for(int i=0;i<intarrayL;i++)
    MessageL += sprintf( (char*)jbuff+MessageL, "%d,",intarray[i]);
  MessageL--;//remove the last comma',';
  
  return MessageL;      
}


buffered_print BP(1024);

SimpPacketParse SPP(500);
int CMD_parse(SimpPacketParse &SPP,buffered_print* bp, JsonDocument &ret_djd,int *ret_result=NULL)
{
  char* TLC=SPP.buffer;
  char* DATA=SPP.buffer+2;
  int dataLen=SPP.size()-2;
//  Serial.print(TLC[0]);
//  Serial.println(TLC[1]);
//  Serial.println(DATA);

  bool errorCode=-1;
  int ret_len=0;

  char retTLC[3];
  if(TLC[0]=='T'&&TLC[1]=='T')
  {
    
    char inChar=DATA[0];
      
    if (inChar == 'C')
    {
      GLOB_F.printCurrentReading = !GLOB_F.printCurrentReading;
      bp->print("@ttprintCurrentReading:%d$",GLOB_F.printCurrentReading);
    }

    if (inChar == 'S')
    {
      GLOB_F.printSTATEChange = !GLOB_F.printSTATEChange;
      bp->print("@ttprintSTATEChange:%d$",GLOB_F.printSTATEChange);
    }

    if (inChar == '?')
    {
      bp->print("@ttprint[C]urrentReading ",GLOB_F.printSTATEChange);
      bp->print("print[S]TATEChange $",GLOB_F.printSTATEChange);
    }
    errorCode=0;
  }
  else if(TLC[0]=='S'&&TLC[1]=='T')
  {

    

    {//@ST{"sss":4,"ECHO":{"AAA":{"fff":7}}}$
      
      deserializeJson(recv_doc, DATA);
      
      const char* sensor = recv_doc["ECHO"];
      if(sensor!=NULL)
      {
        
        bp->print("@tt%s$",sensor);
      }

      recv_doc.clear();
    }
    errorCode=0;
  }
  else if(TLC[0]=='J'&&TLC[1]=='S')//@JS{"id":566,"type":"get_cache_rec"}$@JS{"id":566,"type":"empty_cache_rec"}$
  {
    sprintf(retTLC,"js");
    deserializeJson(recv_doc, DATA);
    ret_djd.clear();

    
    auto idObj=recv_doc["id"];
    ret_djd["id"]=idObj;


      
    auto typeObj=recv_doc["type"];
    if(typeObj.is<char*>())
    {
      const char* type = typeObj.as<char*>();
      if(strcmp(type,"get_cache_rec") == 0) {
        JsonArray rec = ret_djd.createNestedArray("rec");
        if(pID.object_record_cache.size()!=0)
        {
          for (int i = 0; i < pID.object_record_cache.size(); i++)
          {
            rec.add(pID.object_record_cache.arr[i]);
          }
        }

      }
      else if(strcmp(type,"empty_cache_rec") == 0) {
        pID.object_record_cache.resize(0);
      }
      
    }
    recv_doc.clear();

    
    errorCode=0;
  }
  else
  {
    
  }


  if(BP.size()==0)
  {
    bp->print("@%s",retTLC);
    size_t s =serializeJson(ret_djd, bp->buffer()+bp->size(), bp->rest_capacity());
    BP.resize(bp->size()+s);
    bp->print("$");
  }

  if(ret_result)
  {
    *ret_result=errorCode;
  }
  return ret_len;
  
}


void loop() {
  
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if(SPP.feed(inChar))
    {
      BP.resize(0);
      CMD_parse(SPP,&BP,ret_doc);
      Serial.print(BP.buffer());
      SPP.clean();
    }
  }

  
  pID.mainLoop();
}
