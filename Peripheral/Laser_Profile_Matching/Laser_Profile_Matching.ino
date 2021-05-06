// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const int potPin = 34;

const int selectActPin = 33;


volatile int interruptCounter=0;
int totalInterruptCounter=0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;



struct profileObjectInfo{
  uint16_t sim;
};

#define UINT16ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))
int16_t test_template[]={456,
456,509,587,598,648,688,740,784,806,832,849,888,938,928,848,768,661,602,576,545,528,568,532,672,752,832,932,1052,1163,1268,1362,1376,
1377,1348,1360,1181,992,800,656,656,656,698,724,752,772,800,800,770,792,856,916,1041,1168,1162,1264,1295,1328,1361,1395,1428,1466,1501,1535,1579,
1533,1495,1217,922,624,


};
int16_t test_template2[]={530,
530,560,534,504,576,648,640,710,754,796,845,868,912,976,1007,921,832,737,624,588,588,585,588,625,704,805,946,1141,1268,1370,1417,1314,
1144,973,752,700,747,792,841,866,905,999,1068,1063,1126,1205,992,946,824,752,837,784,716,770,776,690,717,525,324,528,832,1077,1232,1340,
1130,1167,1350,1008,848,584,

};



struct GLOB_FLAGS{
  bool printSTATEChange;
  bool printCurrentReading;
};

GLOB_FLAGS GLOB_F;

#define templateSIZE 256
struct profileInspData{
  uint16_t BGValueH;
  uint16_t BGValueL;
  uint16_t NAValue;
  uint8_t BG_TimeNoiseTolerance;

  uint8_t BG_Time_Count;
  
  uint8_t STATE_LOCK;
  int16_t _template[10][templateSIZE];

  int16_t rec_head_idx;
  int16_t rec[templateSIZE];
  enum{
    background,
    in_detection,
    perform_matching,
    clean_state,
    }detection_state;
   
};

profileInspData pID;

void IRAM_ATTR onTimer() {

  static uint16_t dbg_counter=0;  
  dbg_counter++;
  
  if(pID.STATE_LOCK)
  {
    return;
  }
  
  int potValue = analogRead(potPin);
  
  if(dbg_counter>3000/2 && GLOB_F.printCurrentReading)
  {
    dbg_counter=0;
    Serial.print("INT AR: ");
    Serial.println(potValue);
  }
  
  int pre_state=pID.detection_state;
  
  switch(pID.detection_state)//state switch
  {
  
    case profileInspData::clean_state:
      if(pID.BG_Time_Count>pID.BG_TimeNoiseTolerance)
      {
        pID.rec_head_idx=0;//full reset
        pID.BG_Time_Count=0;
        pID.detection_state=profileInspData::background;
      }
    break;
    case profileInspData::background:
      if(pID.BG_Time_Count>pID.BG_TimeNoiseTolerance)
      {
        pID.BG_Time_Count=0;
        pID.detection_state=profileInspData::in_detection;
      }
    break;
    case profileInspData::in_detection:
      if(pID.rec_head_idx>=templateSIZE)
      {//data overload
        pID.detection_state=profileInspData::clean_state;
        pID.BG_Time_Count=0;
        pID.rec_head_idx=0;
      }
      else if(pID.BG_Time_Count==pID.BG_TimeNoiseTolerance)
      {
        pID.BG_Time_Count=0;
//        pID.rec_head_idx=0;
        pID.rec_head_idx-=pID.BG_TimeNoiseTolerance;
        pID.detection_state=profileInspData::perform_matching;
        pID.STATE_LOCK=true;//hand over to main thread to process the matching
      }
    break;
    case profileInspData::perform_matching:
    
    break;
  }
  
  bool state_change=pre_state!=pID.detection_state;
  if(state_change && GLOB_F.printSTATEChange)
  {
    Serial.print("STATE: ");
    Serial.println(pID.detection_state);
  }

  switch(pID.detection_state)//Action
  {
  
    case profileInspData::clean_state:
      pID.rec_head_idx=0;
      if(potValue<pID.BGValueH)
      {
        pID.BG_Time_Count++;
      }
      
    break;

    
    case profileInspData::background:
      pID.rec[pID.rec_head_idx]=potValue;
      if(potValue>pID.BGValueH)
      {
        pID.BG_Time_Count++;
        pID.rec_head_idx++;
      }
      else
      {
        pID.BG_Time_Count=0;
        pID.rec_head_idx=0;
      }
    break;

    
    case profileInspData::in_detection:
      pID.rec[pID.rec_head_idx]=potValue;
      pID.rec_head_idx++;
      if(potValue<pID.BGValueH)
      {
        pID.BG_Time_Count++;
      }
      else
      {
        pID.BG_Time_Count=0;
      }
    break;
  }

}


void IRAM_ATTR onTimer_TEST() 
{
  static uint8_t counter=0;  
  counter++;
  if(counter==0)return;
  Serial.print("INT AR: ");
  int potValue = analogRead(potPin);
  Serial.println(potValue);

}
 
void setup() {
  pinMode(selectActPin, OUTPUT);
  
//  digitalWrite(selectActPin, HIGH);
  pID.BGValueH=400;
  pID.BGValueL=400;
  pID.NAValue=3500;
  pID.BG_TimeNoiseTolerance=10;
  pID.detection_state=profileInspData::background;
  Serial.begin(115200);
 
  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 2000, true);
  timerAlarmEnable(timer);
 
}


int16_t tempSamp(int idx,uint16_t targetLen ,int16_t* temp,uint16_t tempL)
{
  if(idx<0)return 0;
  int eq_idx=idx*tempL/targetLen;
  if(eq_idx>=tempL)return 0;
  return temp[eq_idx];
}




uint32_t tempSAD(int16_t* temp1,uint16_t temp1L,int16_t* temp2,uint16_t temp2L,int16_t NA_Thres=3000, int *ret_NA_Count=NULL)
{
  uint32_t diffSum=0;
  int NA_Count=0;
  for(int i=0;i<temp1L;i++)
  {
    if(temp1[i]>NA_Thres)
    {
      NA_Count++;
      continue;
    }
    int16_t tempV = tempSamp(i,temp1L,temp2,temp2L);
    int16_t diff = temp1[i]-tempV;
    if(diff<0)diff=-diff;
    diffSum+=diff;
  }
  if(ret_NA_Count)*ret_NA_Count=NA_Count;
  
  int divCount=(temp1L-NA_Count);
  if(divCount==0)
  {
    return NA_Thres;
  }
  return diffSum/divCount;
}



uint32_t tempSAD2(int16_t* temp1,uint16_t temp1L,int16_t* temp2,uint16_t temp2L,int16_t NA_Thres=3000, int *ret_NA_Count=NULL)
{

  if(  (temp1L<(temp2L*2/3)) ||
       (temp1L>(temp2L*3/2)))
       {
          return -1;
       }

  
  uint32_t diffSum=0;
  const int tCacheSize=3;
  
  const int tCacheLen=tCacheSize*2+1;
  
  int16_t tCache[tCacheLen];
  
  for(int i=0;i<tCacheLen-1;i++)
  {
    tCache[i] = tempSamp(i,temp1L,temp2,temp2L);
  }
  int tCacheHead=tCacheLen-1;
  int NA_Count=0;
  for(int i=tCacheSize;i<temp1L-tCacheSize;i++)
  {
    if(temp1[i]>NA_Thres)
    {
      NA_Count++;
      continue;
    }
    int16_t tempV = tempSamp(i+tCacheSize,temp1L,temp2,temp2L);
    tCache[tCacheHead]=tempV;
    tCacheHead++;
    if(tCacheHead>=tCacheLen)tCacheHead=0;

    int16_t minDiff=NA_Thres;
    for(int j=0;j<tCacheLen;j++)
    {
      int16_t diff = temp1[i]-tCache[j];
      if(diff<0)diff=-diff;
      if(minDiff>diff)
      {
        minDiff=diff;
      }
      
    }
    
    diffSum+=minDiff;
  }
  if(ret_NA_Count)*ret_NA_Count=NA_Count;
  int divCount=(temp1L-2*tCacheSize-NA_Count);
  if(divCount==0)
  {
    return NA_Thres;
  }
  return diffSum/divCount;
}



void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if(inChar=='C')
    {
      GLOB_F.printCurrentReading=!GLOB_F.printCurrentReading;
      
      Serial.print("printCurrentReading:");
      Serial.println(GLOB_F.printCurrentReading);
    }
   
    if(inChar=='S')
    {
      GLOB_F.printSTATEChange=!GLOB_F.printSTATEChange;
      Serial.print("printSTATEChange:");
      Serial.println(GLOB_F.printSTATEChange);
    }

    if(inChar=='?')
    {
      Serial.print("print[C]urrentReading ");
      Serial.print("print[S]TATEChange ");
      Serial.println();
    }

  }
  if(pID.detection_state==profileInspData::perform_matching)
  {
//    delay(1000);

    if(GLOB_F.printCurrentReading)
    {
      Serial.print(">>>");
      for(int i=0;i<pID.rec_head_idx;i++)
      {
        Serial.print(pID.rec[i]);
        Serial.print(',');
        if((i&0x1F)==0)Serial.println();
      }
      Serial.println();
    }
    bool isNG=true;
    if(1)
    {
      int NA_Count=0;
      uint32_t diffSum=tempSAD2(
        pID.rec,         pID.rec_head_idx,
        test_template,   UINT16ARR_LEN(test_template),3000,&NA_Count);
  
      Serial.print("d1:");
      Serial.print(diffSum);
      Serial.print("(");
      Serial.println(NA_Count);

      int NA_Count2=0;
      uint32_t diffSum2=tempSAD2(
        pID.rec,         pID.rec_head_idx,
        test_template2,   UINT16ARR_LEN(test_template2),3000,&NA_Count2);
  
      if( (NA_Count<2 && diffSum<100 ) || (NA_Count2<2 && diffSum2<100 ) )
      {
        isNG=false;
      }
      
      Serial.print("d2:");
      Serial.print(diffSum2);
      Serial.print("(");
      Serial.println(NA_Count2);
      Serial.print("====");
      Serial.print(" NG?");
      Serial.println(isNG);

      
    }
    if(isNG)
    {
      digitalWrite(selectActPin, HIGH);
      delay(15);
      digitalWrite(selectActPin, LOW);
    }
    pID.detection_state=profileInspData::background;
    pID.STATE_LOCK=false;
  }

}
