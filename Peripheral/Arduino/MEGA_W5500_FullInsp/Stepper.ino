

#define DEBUG_
boolean stepM_seq_a[] = {1, 1, 0, 0, 0, 0, 0, 1};
boolean stepM_seq_b[] = {0, 1, 1, 1, 0, 0, 0, 0};
boolean stepM_seq_c[] = {0, 0, 0, 1, 1, 1, 0, 0};
boolean stepM_seq_d[] = {0, 0, 0, 0, 0, 1, 1, 1};



#define TIMER_SET_ISR(TN) \
  void timer##TN##_HZ(int HZ){TCNT##TN=0;OCR##TN##A = 16000000 / 256 / HZ;} \
  void timer##TN##Setup(int HZ)\
  {\
    noInterrupts();\
    TCCR##TN##A = 0;\
    TCCR##TN##B = 0;\
    timer##TN##_HZ(HZ);\
    TCCR##TN##B |= (1 << WGM##TN##2);\
    TCCR##TN##B |= (1 << CS12);\
    TIMSK##TN |= (1 << OCIE##TN##A);\
    interrupts();\
  }\
  ISR(TIMER##TN##_COMPA_vect) 


class StepperMotor
{
  public:
    int p1, p2, p3, p4;

    StepperMotor(int p1, int p2, int p3, int p4)
    {

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
    void OneStep(bool dir) { //DRV8825 p1=step p2=dir
      digitalWrite(p2, dir);

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


uint32_t mod_sim(uint32_t num,uint32_t mod_N)
{
  while(num>=mod_N)
  {
    num-=mod_N;
  }
  return num;
}



StepperMotor stepperMotor(22, 23, 24, 25);
//StepperMotor stepperMotorDRV8825(22,23,24,25);


#define CAMERA_PIN 16
#define AIR_BLOW_OK_PIN 18
#define AIR_BLOW_NG_PIN 19
#define GATE_PIN 30


uint32_t perRevPulseCount_HW = (uint32_t)2400*32;//the real hardware pulse count per rev
uint32_t pulseSkip=32;//We don't do task processing for every hardware pulse, so we can save computing power for other things
uint32_t perRevPulseCount = perRevPulseCount_HW/pulseSkip;// the software pulse count that processor really care


uint32_t PRPC= perRevPulseCount;
int offsetAir=80;
uint32_t state_pulseOffset[] = {
  PRPC*30/360, PRPC*30/360+5, 
  PRPC*30/360+10, 
  
  PRPC*240/360+offsetAir, PRPC*240/360+6+offsetAir, 
  PRPC*240/360+20+offsetAir, PRPC*240/360+26+offsetAir};



pipeLineInfo* actionExecTask[ (sizeof(state_pulseOffset) / sizeof(*state_pulseOffset))];
RingBuf<typeof(*actionExecTask),uint8_t > actionExecTaskQ(actionExecTask,sizeof(actionExecTask)/sizeof(*actionExecTask));




int stage_action(pipeLineInfo* pli);
int stage_action(pipeLineInfo* pli)
{
  pli->notifMark = 0;
  switch (pli->stage)
  {
    case 0:

      break;
    case 1://Trigger shutter ON
      digitalWrite(CAMERA_PIN, 1);
      pli->notifMark = 1;
      break;
    case 2://Trigger shutter OFF
      digitalWrite(CAMERA_PIN, 0);
      break;

    case 3://Termination stage
      //pli->stage = 5;
      //return -1;
      break;


    case 4://Air Blow OK ON
      digitalWrite(AIR_BLOW_OK_PIN, 1);
      break;
    case 5://Air Blow OK OFF
      digitalWrite(AIR_BLOW_OK_PIN, 0);
      //pli->stage = -1;
      //return -1;
      break;
    case 6://Air Blow NG ON
      digitalWrite(AIR_BLOW_NG_PIN, 1);
      break;
    case 7://Air Blow NG OFF
      digitalWrite(AIR_BLOW_NG_PIN, 0);
      pli->stage = -1;
      return -1;
      break;
  }
  return 0;
}

int next_state(pipeLineInfo* pli);
int next_state(pipeLineInfo* pli)
{
  if (pli->stage < 0 || 
    pli->stage > (sizeof(state_pulseOffset) / sizeof(*state_pulseOffset)) ||
    stage_action(pli) < 0)
  {
    pli->gate_pulse=perRevPulseCount;
    return -1;
  }
  
  pli->trigger_pulse = mod_sim(pli->gate_pulse + state_pulseOffset[pli->stage],perRevPulseCount);
  
  pli->stage++;
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
  if(tarP>curP)
    return tarP-curP;
  
  return (warp+tarP)-curP;
}



uint32_t next_processing_pulse=perRevPulseCount;//equal perRevPulseCount to means never hit processing pulse
uint32_t countX = 0;
uint32_t countSkip = 0;

void task_gateSensing()
{
  
  
  uint8_t cur_Sense = digitalRead(GATE_PIN);
          
  if (cur_Sense != gateInfo.pre_Sense)
  {
    gateInfo.debunce = 0;
  }
  else if (gateInfo.debunce <= 1)gateInfo.debunce++;

  if (gateInfo.debunce == 1)
  {
    if (cur_Sense == 0)
    {
      if (gateInfo.state != cur_Sense)
      {
        gateInfo.state = cur_Sense;
        gateInfo.start_pulse = countX;
      }
    }
    else
    {
      if (gateInfo.state != cur_Sense)
      {

        gateInfo.end_pulse = countX;
        if (gateInfo.start_pulse > gateInfo.end_pulse)
        {
          gateInfo.end_pulse += perRevPulseCount;
        }
        uint32_t middle_pulse = (gateInfo.end_pulse + gateInfo.start_pulse) >> 1;
        middle_pulse = mod_sim(middle_pulse,perRevPulseCount);
        pipeLineInfo* head = RBuf.getHead();
        if (head != NULL)
        {
          head->gate_pulse = middle_pulse;
          head->stage = 0;
          next_state(head);
          RBuf.pushHead();
//          uint32_t cur_dist = pulse_distance(countX,next_processing_pulse, perRevPulseCount);
//          uint32_t new_dist = pulse_distance(countX,head->trigger_pulse, perRevPulseCount);
//          if(new_dist<cur_dist)
//          {
//            next_processing_pulse=head->trigger_pulse;
//          }
//          DEBUG_print("====g_pulse:");
//          DEBUG_print(head->gate_pulse);
//          DEBUG_print(" t_pulse:");
//          DEBUG_print(head->trigger_pulse);
//          DEBUG_print(" perRevPulseCount:");
//          DEBUG_print(perRevPulseCount);
//          DEBUG_print(" CX:");
//          DEBUG_println(countX);

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



uint32_t getMinDistTaskPulse(RingBuf<pipeLineInfo*,uint8_t > &queue)
{
  if(queue.size()==0)return perRevPulseCount;
  pipeLineInfo** taskToDo;
  for(int i=0;i<queue.size();i++)
  {
    taskToDo=queue.getTail(i);
    pipeLineInfo* tail = (*taskToDo);
    if(tail==NULL)continue;
    return tail->trigger_pulse;
  }
  return perRevPulseCount;
  
}


void task_ExecuteMinDistTasks(uint8_t stage,uint8_t stageLen)
{  
  if(stage==0)
  {
    pipeLineInfo** taskToDo =NULL;
    int i=0;
    while(taskToDo=actionExecTaskQ.getTail())
    {
      pipeLineInfo* tail = (*taskToDo);
      if(tail==NULL)continue;
      if(tail->trigger_pulse!=countX)break;
      
      int ret = next_state(tail);
      if (ret)
      {
        tail->stage = -3;
      }
      actionExecTaskQ.consumeTail();
    }
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
  if(!doCollection)return;

  
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
  
  if(proE>RBuf.size())proE=RBuf.size();
  for (int i = proS; i < proE ; i++) //Check if trigger pulse is hit, then do action/ mark deletion
  {
    pipeLineInfo* tail = RBuf.getTail(i);

    uint32_t dist = pulse_distance(countX,tail->trigger_pulse, perRevPulseCount);
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
  task_CollectMinDistTasks(stage-1,stageLen-2);//1~stageLen-2
  task_CleanCompletedPipe(stage-(stageLen-1),1);//stageLen-1 only

}


TIMER_SET_ISR(1)
{
  stepperMotor.OneStep(true);

  
  countSkip = mod_sim(countSkip+1,pulseSkip);
  
  if (countSkip==0)
  {
    countX = mod_sim(countX+1,perRevPulseCount);
    task_gateSensing();
  }
  

  task_pulseStageExec(countSkip,pulseSkip);

}

uint32_t pulseHZ = 0;
uint32_t tar_pulseHZ = 32 * 800;
uint32_t pulseHZ_step = 1;

void setup_Stepper() {
  pinMode(CAMERA_PIN, OUTPUT);
  pinMode(AIR_BLOW_OK_PIN, OUTPUT);
  pinMode(AIR_BLOW_NG_PIN, OUTPUT);
  pinMode(GATE_PIN, INPUT);
  DEBUG_println(".....");
  timer1Setup(1);
}


void loop_Stepper() {
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
}
