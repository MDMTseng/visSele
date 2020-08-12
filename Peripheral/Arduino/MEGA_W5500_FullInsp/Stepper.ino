

#define DEBUG_
boolean stepM_seq_a[] = {1, 1, 0, 0, 0, 0, 0, 1};
boolean stepM_seq_b[] = {0, 1, 1, 1, 0, 0, 0, 0};
boolean stepM_seq_c[] = {0, 0, 0, 1, 1, 1, 0, 0};
boolean stepM_seq_d[] = {0, 0, 0, 0, 0, 1, 1, 1};


#define TIMER_SET_ISR(TN,PRE_SCALER) \
  void timer##TN##_HZ(int HZ){\
    TIMSK##TN=(HZ==0)?0:(1 << OCIE##TN##A);\
    uint16_t OCR =  16000000 / PRE_SCALER / HZ;\
    OCR##TN##A = OCR;\
    if(TCNT##TN>OCR){\
      TCNT##TN=0;\
    }\
  } \
  void timer##TN##Setup(int HZ)\
  {\
    noInterrupts();\
    TCCR##TN##A = 0;\
    TCCR##TN##B = 0;\
    TCCR##TN##B |= (1 << WGM##TN##2);\
    if(PRE_SCALER==1)TCCR##TN##B        |= (1 << CS##TN##0);\
    else if(PRE_SCALER==8)TCCR##TN##B   |= (1 << CS##TN##1);\
    else if(PRE_SCALER==64)TCCR##TN##B  |= (1 << CS##TN##1) | (1 << CS##TN##0);\
    else if(PRE_SCALER==256)TCCR##TN##B |= (1 << CS##TN##2);\
    else if(PRE_SCALER==1024)TCCR##TN##B|= (1 << CS##TN##2) | (1 << CS##TN##0);\
    TIMSK##TN |= (1 << OCIE##TN##A);\
    interrupts();\
    timer##TN##_HZ(HZ);\
  }


class StepperMotor
{
  public:
    int p1, p2, p3, p4;

    StepperMotor(int p1, int p2, int p3, int p4){

      pinMode(p1, OUTPUT);
      pinMode(p2, OUTPUT);
      pinMode(p3, OUTPUT);
      pinMode(p4, OUTPUT);
      this->p1 = p1;
      this->p2 = p2;
      this->p3 = p3;
      this->p4 = p4;
    }

    int stepX_number = 0;
    bool cacheDir=false;
    void OneStep(bool dir) { //DRV8825 p1=step p2=dir
      if(cacheDir!=dir)
      {
        digitalWrite(p2, dir);
        cacheDir=dir;
      }

      digitalWrite(p1, HIGH);
      digitalWrite(p1, LOW);

    }
    void OneStep_Original(bool dir) {


      digitalWrite(p1, stepM_seq_a[stepX_number]);
      digitalWrite(p2, stepM_seq_b[stepX_number]);
      digitalWrite(p3, stepM_seq_c[stepX_number]);
      digitalWrite(p4, stepM_seq_d[stepX_number]);
      if (dir)
      {
        stepX_number++;
        if (stepX_number == sizeof(stepM_seq_a)) {
          stepX_number = 0;
        }
      }
      else
      {

        if (stepX_number == 0) {
          stepX_number = sizeof(stepM_seq_a) - 1;
        }
        else
          stepX_number--;
      }
    }
};


StepperMotor stepperMotor(22, 23, 24, 25);


//StepperMotor stepperMotorDRV8825(22,23,24,25);
uint32_t mod_sim(uint32_t num,uint32_t mod_N)
{
  while(num>=mod_N)
  {
    num-=mod_N;
  }
  return num;
}




pipeLineInfo* actionExecTask[ SARRL(state_pulseOffset)];
RingBuf<typeof(*actionExecTask),uint8_t > actionExecTaskQ(actionExecTask,SARRL(state_pulseOffset));


int next_state(pipeLineInfo* pli);
int next_state(pipeLineInfo* pli)
{
  if (pli->stage < 0 || 
    pli->stage > SARRL(state_pulseOffset) ||
    stage_action(pli) < 0)
  {
    pli->gate_pulse=perRevPulseCount;
    pli->stage=-1;//termination
    return -1;
  }
  
  pli->trigger_pulse = mod_sim(pli->gate_pulse + state_pulseOffset[pli->stage],perRevPulseCount);
  
  return 0;
}


typedef struct GateInfo {
  uint8_t state;
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint8_t debunce;
  uint8_t pre_Sense;
  uint8_t cur_Sense;
  uint8_t Sense;
} GateInfo;
GateInfo gateInfo = {.state = 1};




uint32_t pulse_distance(uint32_t curP,uint32_t tarP,uint32_t warp)
{
  if(tarP>=warp)return warp;
  if(tarP>=curP)
    return tarP-curP;
  
  return (warp+tarP)-curP;
}



uint32_t getMinDistTaskPulse(RingBuf<pipeLineInfo*,uint8_t > &queue)
{
  for(int i=0;i<queue.size();i++)
  {
    pipeLineInfo** taskToDo=queue.getTail(i);
    if(taskToDo==NULL)break;
    pipeLineInfo* tail = (*taskToDo);
    if(tail==NULL)continue;
    return tail->trigger_pulse;
  }
  return perRevPulseCount;
}


uint32_t next_processing_pulse=perRevPulseCount;//equal perRevPulseCount to means never hit processing pulse
//uint32_t logicPulseCount = 0;
uint32_t countSkip = 0;
#define DEBOUNCE_THRES 10
#define OBJECT_SEP_THRES (150/2)

uint32_t step_thres_pulse_down=0;
void task_gateSensing()
{
  
  
  uint8_t cur_Sense = !digitalRead(GATE_PIN);
          
  if (cur_Sense != gateInfo.pre_Sense)
  {
    gateInfo.debunce = 0;
  }
  else if (gateInfo.debunce <= DEBOUNCE_THRES)gateInfo.debunce++;

  if(step_thres_pulse_down!=0)
  {
    step_thres_pulse_down--;
  }

  if (gateInfo.debunce == DEBOUNCE_THRES)
  {
    if (cur_Sense == 0)
    {
      if (gateInfo.state != cur_Sense)
      {
        gateInfo.state = cur_Sense;
        gateInfo.start_pulse = logicPulseCount;
      }
    }
    else
    {
      if (gateInfo.state != cur_Sense)
      {
        gateInfo.end_pulse = logicPulseCount;
        if (gateInfo.start_pulse > gateInfo.end_pulse)
        {
          gateInfo.end_pulse += perRevPulseCount;
        }
        uint32_t middle_pulse = (gateInfo.end_pulse + gateInfo.start_pulse) >> 1;
        if(middle_pulse<DEBOUNCE_THRES)
        {
          middle_pulse = perRevPulseCount+middle_pulse-DEBOUNCE_THRES;
        }
        else
        {
          middle_pulse = mod_sim(middle_pulse-DEBOUNCE_THRES,perRevPulseCount);
        }

        bool accept_pulse=(step_thres_pulse_down==0);
        
        if(!accept_pulse)
        {
          //remove previous object
          RBuf.pullHead();
          pipeLineInfo* head = RBuf.getHead();
          step_thres_pulse_down=0;
        }
        
        pipeLineInfo* head = RBuf.getHead();
        if (accept_pulse && head != NULL)
        {
          accept_pulse=OBJECT_SEP_THRES;
          head->gate_pulse = middle_pulse;
          head->stage = 0;
          next_state(head);
          RBuf.pushHead();



          //Do pulse distance check, if the new task is closer do fresh
          {
            uint32_t exPulse = getMinDistTaskPulse(actionExecTaskQ);
            uint32_t exDist = pulse_distance(logicPulseCount,exPulse,perRevPulseCount);
            uint32_t newPulse = head->trigger_pulse;
            uint32_t newDist= pulse_distance(logicPulseCount,newPulse,perRevPulseCount);
            if(newDist<exDist)
            {
              actionExecTaskQ.clear();
              
              pipeLineInfo** newQhead = actionExecTaskQ.getHead();
              if (newQhead != NULL)
              {
                *newQhead = head;
                actionExecTaskQ.pushHead();
              }
            }
          }

          
        }
      }

    }
    gateInfo.state = cur_Sense;
  }
  gateInfo.pre_Sense = gateInfo.cur_Sense;
  gateInfo.cur_Sense = cur_Sense;

}


typedef struct pulseStageInfo
{
  uint32_t minDist;
  uint32_t minDist_Pulse;
  
}pulseStageInfo;


void task_ExecuteMinDistTasks(uint8_t stage,uint8_t stageLen)
{  
  if(stage!=0)return;
  pipeLineInfo** taskToDo =NULL;
  while(taskToDo=actionExecTaskQ.getTail())
  {
    
    pipeLineInfo* tail = (*taskToDo);
    if(tail==NULL)continue;
    if(tail->trigger_pulse!=logicPulseCount)break;


//    if(tail->stage==7)
//    {
//      pipeLineInfo** Q_tail = taskToDo;
//      for (int j = 0; j < RBuf.size() ; j++)
//      {
//        pipeLineInfo* RBuf_tail = RBuf.getTail(j);
//        if(*Q_tail == RBuf_tail)
//        {
//          static int pX_idx=0;
//          DEBUG_print("getTail_Idx:");
//          int fidx = RBuf.getTail_Idx(j);
//          DEBUG_print(fidx);
//          DEBUG_print(" diff:");
//          DEBUG_print(fidx - pX_idx);
//          pX_idx=fidx;
//          DEBUG_print(" stage:");
//          DEBUG_println(RBuf_tail->stage);
//          break;
//        }
//      }
//    }


    int ret = next_state(tail);
    if (ret)
    {
      tail->stage = -3;
    }
    actionExecTaskQ.consumeTail();
  }
  
}



void task_CollectMinDistTasks(uint8_t stage,uint8_t stageLen)
{
  if(stage>stageLen)return;
  static int initSize=0;
  static int processMult=0;
  static int proS=0;
  static int proE=0;

  static int doCollection;
  
  static uint32_t minDist;

  if(stage==0)
  {
    uint32_t minTaskPulse = getMinDistTaskPulse(actionExecTaskQ);
    doCollection=(minTaskPulse>=perRevPulseCount);
  }
  if(!doCollection)
  {
    return;
  }

  
  proS=proE;
  proE+=processMult;
  if(stage==0)
  {
    minDist=perRevPulseCount;
    initSize=RBuf.size();
    processMult=initSize/(stageLen)+1;
    actionExecTaskQ.clear();
    proS=0;
    proE=processMult;
  }
  
  if(proE>RBuf.size())
  {
    proE=RBuf.size();
  }
  
  for (int i = proS; i < proE ; i++) //Check if trigger pulse is hit, then do action/ mark deletion
  {
    pipeLineInfo* tail = RBuf.getTail(i);
    if(tail->stage<0)continue;

    uint32_t dist = pulse_distance(logicPulseCount,tail->trigger_pulse, perRevPulseCount);
    if(minDist>dist)
    {
      actionExecTaskQ.clear();
      minDist=dist;
      
      
    }

    if(minDist==dist)
    {
      
      pipeLineInfo** head = actionExecTaskQ.getHead();
      if (head != NULL)
      {
        *head = tail;
        actionExecTaskQ.pushHead();
      }
    }
  }
}


void task_CleanCompletedPipe(uint8_t stage,uint8_t stageLen)
{
  if(stage!=0)return;
  pipeLineInfo* tail;
  while (tail = RBuf.getTail()) //Clean completed tail tasks
  {
    if (tail->stage < 0)
    {
      RBuf.consumeTail();
      continue;
    }
    break;
  }

}

void task_pulseStageExec(uint8_t stage,uint8_t stageLen)
{
  //exp:stageLen=10
  task_ExecuteMinDistTasks(stage,1);//0 only
  task_CollectMinDistTasks(stage-1,stageLen-2);//1~stageLen-2 => 1~8
  task_CleanCompletedPipe(stage-(stageLen-1),1);//stageLen-1 only => 9
}


uint32_t revCount = 0;
TIMER_SET_ISR(1,8)

ISR(TIMER1_COMPA_vect) 
{
  stepperMotor.OneStep(true);

  
  countSkip = mod_sim(countSkip+1,subPulseSkipCount);
  
  if (countSkip==0)
  {
    logicPulseCount = mod_sim(logicPulseCount+1,perRevPulseCount);
    if(logicPulseCount==0)
    {
      revCount++;
      EV_Axis0_Origin(revCount);
    }
    task_gateSensing();
  }


  task_pulseStageExec(countSkip,subPulseSkipCount);

}

uint32_t pulseHZ = 0;

void setup_Stepper() {
  DEBUG_println(".....");
  timer1Setup(1);
  timer1_HZ(0);
}


uint32_t loop_Stepper(uint32_t tar_pulseHZ,uint32_t pulseHZ_step) {
  if (pulseHZ != tar_pulseHZ)
  {
    if (pulseHZ < tar_pulseHZ)
    {
      if (tar_pulseHZ - pulseHZ > pulseHZ_step)
        pulseHZ += pulseHZ_step;
      else
        pulseHZ = tar_pulseHZ;

    }
    if (pulseHZ > tar_pulseHZ)
    {
      if (pulseHZ - tar_pulseHZ > pulseHZ_step)
        pulseHZ -= pulseHZ_step;
      else
        pulseHZ = tar_pulseHZ;

    }
    timer1_HZ(pulseHZ);
  }
  return pulseHZ;
}
