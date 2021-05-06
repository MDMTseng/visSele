#include"UTIL.h"

// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6)
const int potPin = 34;

const int selectActPin = 33;


volatile int interruptCounter = 0;
int totalInterruptCounter = 0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


#define templateSIZE 256

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

GLOB_FLAGS GLOB_F;

class profileInspData {
  public:
    uint16_t BGValueH;
    uint16_t NAValue;
    uint8_t BG_TimeNoiseTolerance;

    uint8_t BG_Time_Count;

    bool STATE_LOCK;

    mArray<mArray<int16_t>> tempCollection;


    mArray<int16_t> record;
    enum {
      background,
      in_detection,
      perform_matching,
      clean_state,
    } detection_state;

    profileInspData()
    {
      BGValueH = 400;
      NAValue = 3500;
      BG_TimeNoiseTolerance = 10;
      detection_state = profileInspData::background;
      

      record.RESET(templateSIZE);
//      tempCollection.arr[1].RESET(400);


    }

    void writeCurrentRecIntoTemplate()
    {
      
    }

    void timerRun() {
      static uint16_t dbg_counter = 0;
      dbg_counter++;

      if (STATE_LOCK)
      {
        return;
      }

      int potValue = analogRead(potPin);

      if (dbg_counter > 3000 / 2 && GLOB_F.printCurrentReading)
      {
        dbg_counter = 0;
        Serial.print("INT AR: ");
        Serial.println(potValue);
      }

      int pre_state = detection_state;

      switch (detection_state) //state switch
      {

        case profileInspData::clean_state:
          if (BG_Time_Count > BG_TimeNoiseTolerance)
          {
            record.setSize(0);
            BG_Time_Count = 0;
            detection_state = profileInspData::background;
          }
          break;
        case profileInspData::background:
          if (BG_Time_Count > BG_TimeNoiseTolerance)
          {
            BG_Time_Count = 0;
            detection_state = profileInspData::in_detection;
          }
          break;
        case profileInspData::in_detection:
          if (record.size()== record.maxSize())
          { //data overload
            detection_state = profileInspData::clean_state;
            BG_Time_Count = 0;
            record.setSize(0);
          }
          else if (BG_Time_Count == BG_TimeNoiseTolerance)
          {
            BG_Time_Count = 0;
            //        rec_head_idx=0;
            record.setSize(record.size()-BG_TimeNoiseTolerance);
            detection_state = profileInspData::perform_matching;
            STATE_LOCK = true; //hand over to main thread to process the matching
          }
          break;
        case profileInspData::perform_matching:

          break;
      }

      bool state_change = pre_state != detection_state;
      if (state_change && GLOB_F.printSTATEChange)
      {
        Serial.print("STATE: ");
        Serial.println(detection_state);
      }

      switch (detection_state) //Action
      {

        case profileInspData::clean_state:
          record.setSize(0);
          if (potValue < BGValueH)
          {
            BG_Time_Count++;
          }

          break;


        case profileInspData::background:
          record.push_back(potValue);
          if (potValue > BGValueH)
          {
            BG_Time_Count++;
          }
          else
          {
            BG_Time_Count = 0;
            record.setSize(0);
          }
          break;


        case profileInspData::in_detection:
          record.push_back(potValue);
          if (potValue < BGValueH)
          {
            BG_Time_Count++;
          }
          else
          {
            BG_Time_Count = 0;
          }
          break;
      }

    }

    void mainLoop()
    {
      if (detection_state == profileInspData::perform_matching)
      {
        //    delay(1000);

        if (GLOB_F.printCurrentReading)
        {
          Serial.print(">>>");
          for (int i = 0; i < record.size(); i++)
          {
            Serial.print((int)record.arr[i]);
            Serial.print(',');
            if ((i & 0x1F) == 0)Serial.println();
          }
          Serial.println();
        }
        bool isNG = true;
        if (1)
        {
          int NA_Count = 0;
          uint32_t diffSum = tempSAD2(
                               record.arr,         record.size(),
                               test_template,   UINT16ARR_LEN(test_template), NAValue, &NA_Count);

          Serial.print("d1:");
          Serial.print(diffSum);
          Serial.print("(");
          Serial.println(NA_Count);

          int NA_Count2 = 0;
          uint32_t diffSum2 = tempSAD2(
                                record.arr,         record.size(),
                                test_template2,   UINT16ARR_LEN(test_template2), NAValue, &NA_Count2);

          if ( (NA_Count < 2 && diffSum < 100 ) || (NA_Count2 < 2 && diffSum2 < 100 ) )
          {
            isNG = false;
          }

          Serial.print("d2:");
          Serial.print(diffSum2);
          Serial.print("(");
          Serial.println(NA_Count2);
          if(isNG)
          {
            Serial.println("X     X");
          }
          else
          {
            Serial.println("OOOOOO");
          }


        }
        if (isNG)
        {
          digitalWrite(selectActPin, HIGH);
          delay(15);
          digitalWrite(selectActPin, LOW);
        }
        detection_state = profileInspData::background;
        STATE_LOCK = false;
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
