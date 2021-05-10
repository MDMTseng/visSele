#include"UTIL.h"
#include"RingBuf.hpp"

// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6)
const int potPin = 34;

const int selectActPin = 33;


volatile int interruptCounter = 0;
int totalInterruptCounter = 0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


#define templateSIZE 100

#define UINT16ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
int16_t test_template[] = {540,
613,464,522,592,586,620,640,717,762,768,784,812,864,933,1006,1057,1111,864,672,681,488,496,550,650,778,905,994,1165,1314,1370,1398,1405,
1398,1392,1136,756,678,728,774,824,836,612,130,120,112,228,292,320,576,858,920,960,1061,1138,1237,1201,848,1328,1616,1332,656,


                          };
int16_t test_template2[] = {450,
528,608,640,704,740,552,792,821,822,768,914,1034,915,822,676,534,525,517,496,546,608,768,917,1066,1264,1369,1423,1477,1520,1345,1040,823,
662,624,662,682,714,733,756,784,804,909,984,994,1056,1062,1072,1168,1281,1382,1428,1484,1535,1584,1510,1385,1296,732,

                           };



struct GLOB_FLAGS {
  bool printSTATEChange;
  bool printCurrentReading;
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
      uint16_t forwardSpace;
      uint16_t BGValueH;
      uint16_t NAValue;
      uint8_t BG_TimeNoiseTolerance;

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
      act1TimingQ.RESET(ACT_TIMMING_BUFFER_SIZE);
      act2TimingQ.RESET(ACT_TIMMING_BUFFER_SIZE);
      object_track.RESET(5);
      RESET();
    }

    void RESET()
    {
      
      s_data.BGValueH = 1480;
      s_data.forwardSpace = 20;
      s_data.NAValue = 3500;
      s_data.BG_TimeNoiseTolerance = 4;
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
        
        Serial.print(s_data.pre_state);
        Serial.print("+[");
        Serial.print(action);
        Serial.print("]=");
        Serial.println(s_data.state);
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
            
            if(s_data.utilCounter<s_data.BG_TimeNoiseTolerance)
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
            
            if(object_record.size()==0 && s_data.state_Counter>s_data.forwardSpace)//object_record is free, assign to cur_object
            {
              s_data.cur_object.recordInfo=&object_record;
              
            }
            if(s_data.cur_object.recordInfo!=NULL)//if cur_object has recorder, push new reading data in
            {
              s_data.cur_object.recordInfo->push_back(potValue);
            }

            
            if(s_data.utilCounter<s_data.BG_TimeNoiseTolerance)
            {
              s_data.utilCounter++; 
            }
            else
            {//the high reading is stable, try to keep the step_start(where the high reading starts) info and put OK action into state machine
              s_data.cur_object.step_start=stepCounter-s_data.BG_TimeNoiseTolerance;
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
          else if(s_data.state_Counter>(templateSIZE-s_data.BG_TimeNoiseTolerance))//if there was a recorder, will it be full?
          {//if so put NA event
            stateSwitch(A_ER);
            break;
          }
          
          if(potValue<s_data.BGValueH)//LOW state
          {
            if(s_data.utilCounter<s_data.BG_TimeNoiseTolerance)
            {
              s_data.utilCounter++; 
            }
            else
            {//stable LOW state
              if(s_data.cur_object.recordInfo!=NULL)
              {
                s_data.cur_object.recordInfo->resize(object_record.size()-s_data.BG_TimeNoiseTolerance);//roll back the BG_TimeNoiseTolerance steps
              }
              
              s_data.cur_object.step_end=stepCounter-s_data.BG_TimeNoiseTolerance;//roll back the BG_TimeNoiseTolerance steps
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
            if(s_data.utilCounter<s_data.BG_TimeNoiseTolerance)
            {
              s_data.utilCounter++; 
            }
            else
            {//stable LOW state
               _matching_subject tmp={0};
               
              tmp.recordInfo=NULL;
              tmp.step_end=stepCounter-s_data.BG_TimeNoiseTolerance;//roll back the BG_TimeNoiseTolerance steps
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


        Serial.print("A1Q:");
        Serial.print(act1TimingQ.size());
        Serial.print(" A2Q:");
        Serial.print(act2TimingQ.size());
        Serial.print(" OTQ:");
        Serial.println(object_track.size());
        
        switch(obj->stepType)
        {
          case _matching_subject::NORMAL:
          {
            bool doPass=false;
            /**/ 
            if(obj->recordInfo!=NULL)
            {
//              delay(3);
              
//        Serial.print("recLen:");
//        Serial.println(obj->recordInfo->size());
//              doPass=OK_NG_FLIP;
//              OK_NG_FLIP=!OK_NG_FLIP;
              doPass=obj->recordInfo->size()<50*0;
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

void IRAM_ATTR onTimer_TEST()
{
  static uint8_t counter = 0;
  counter++;
  if (counter == 0)return;
  Serial.print("INT AR: ");
  int potValue = analogRead(potPin);
  Serial.println(potValue);

}

void setup() {
  pinMode(selectActPin, OUTPUT);



  //  digitalWrite(selectActPin, HIGH);

  Serial.begin(115200);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000, true);
  timerAlarmEnable(timer);

}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == 'C')
    {
      GLOB_F.printCurrentReading = !GLOB_F.printCurrentReading;

      Serial.print("printCurrentReading:");
      Serial.println(GLOB_F.printCurrentReading);
    }

    if (inChar == 'S')
    {
      GLOB_F.printSTATEChange = !GLOB_F.printSTATEChange;
      Serial.print("printSTATEChange:");
      Serial.println(GLOB_F.printSTATEChange);
    }

    if (inChar == '?')
    {
      Serial.print("print[C]urrentReading ");
      Serial.print("print[S]TATEChange ");
      Serial.println();
    }

  }

  
  pID.mainLoop();
}
