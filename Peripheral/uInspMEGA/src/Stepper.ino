
#include "include/UTIL.hpp"

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

  // DEBUG_print("GP:");
  // DEBUG_print(pli->gate_pulse);
  // DEBUG_print("  TP:");
  // DEBUG_println(pli->trigger_pulse);
  return 0;
}


typedef struct GateInfo {
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint16_t debunce;
  uint8_t cur_Sense;


} GateInfo;


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


//uint32_t logicPulseCount = 0;
uint32_t countSkip = 0;
#define DEBOUNCE_L_THRES (perRevPulseCount/50)
#define DEBOUNCE_H_THRES (perRevPulseCount/500)

GateInfo gateInfo;




void RESET_GateSensing()
{
  GateInfo ngateInfo = {0};
  ngateInfo.cur_Sense=0;
  ngateInfo.start_pulse=~0;
  ngateInfo.end_pulse=~0;
  gateInfo=ngateInfo;
}

#define RING_SUB(A,B,MAX) ( ((A)>(B)) ? ((A)-(B))  : ((A)+(MAX)-(B)) )
int task_newPulseEvent(uint32_t middle_pulse);
void task_gateSensing(uint8_t stage,uint8_t stageLen)
{
  
  if(stage>=stageLen)return;
  uint8_t new_Sense = digitalRead(GATE_PIN);
    
  bool flip=false;

  if(gateInfo.cur_Sense)
  {//H
    if(!new_Sense)//L
    {
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        flip=true;

        // gateInfo.end_pulse=logicPulseCount;
        // gateInfo.end_pulse=RING_SUB(logicPulseCount,DEBOUNCE_L_THRES,perRevPulseCount);
        
        gateInfo.debunce = DEBOUNCE_H_THRES;
      
      }
    }
    else
    {
      gateInfo.debunce = DEBOUNCE_L_THRES;
      gateInfo.end_pulse=logicPulseCount;
    }
  }
  else
  {//L cur_Sense
    if(new_Sense)//H
    {
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        flip=true;

        //gateInfo.start_pulse=logicPulseCount;

        //gateInfo.start_pulse=RING_SUB(logicPulseCount,DEBOUNCE_H_THRES,perRevPulseCount);
        //
        gateInfo.debunce = DEBOUNCE_L_THRES;
      }
    }
    else
    {
      gateInfo.debunce = DEBOUNCE_H_THRES;
      gateInfo.start_pulse=logicPulseCount;
    }
  }

  if(flip)
  {
    if(!new_Sense)
    {//a pulse is completed

      uint32_t diff=RING_SUB(gateInfo.end_pulse,gateInfo.start_pulse,perRevPulseCount);
      diff>>=1;
      uint32_t middlePulse=mod_sim(gateInfo.start_pulse+diff,perRevPulseCount);
      
      task_newPulseEvent(middlePulse);
    }

    gateInfo.cur_Sense=new_Sense;

  }



}

int task_newPulseEvent(uint32_t middle_pulse)
{
    
  pipeLineInfo* head = RBuf.getHead();
  if (head == NULL)return -1;
  //get a new object and find a space to log it
  // TCount++;
  head->gate_pulse = middle_pulse;
  head->stage = 0;
  next_state(head);//calc next trigger pulse
  RBuf.pushHead();
  return 0;
  // actionExecTasks.clear();

      
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
  while(taskToDo=actionExecTasks.getTail())
  {
    
    pipeLineInfo* tail = (*taskToDo);
    if(tail==NULL)continue;
    if(tail->trigger_pulse!=logicPulseCount)break;

    int ret = next_state(tail);
    if (ret)
    {
      tail->stage = -3;
    }
    actionExecTasks.consumeTail();
  }

}



void task_CollectMinDistTasks(uint8_t stage,uint8_t stageLen)
{
  if(stage>stageLen)return;
  static int stageStep=0;
  static int proS=0;
  static int proE=0;

  static int doCollection;
  
  static uint32_t minDist;

  
  if(stage==0)
  {
    doCollection=(actionExecTasks.size()==0);//If the actionExecTasks still have task means the closest task is still the same
    minDist=perRevPulseCount;//set to maximum
    stageStep=RBuf.size()/(stageLen)+1;
  }
  proS=stageStep*stage;
  proE=proS+stageStep;
  
  if(!doCollection)
  { 
    return;
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
      actionExecTasks.clear();
      minDist=dist;
    }

    if(minDist==dist)
    {
      
      pipeLineInfo** head = actionExecTasks.getHead();
      if (head != NULL)
      {
        *head = tail;
        actionExecTasks.pushHead();
      }
    }
  }
  if(stage==stageLen-1 && actionExecTasks.size()!=0)
  {
    ExeUpdateCount++;
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
  //exp:stageLen=10  0~9
  uint8_t stageBase=stage;
  task_gateSensing(stageBase,1);//0 only
  stageBase-=1;
  task_CollectMinDistTasks(stageBase,stageLen-3);//1 len 7 => 1~7
  
  stageBase-=stageLen-3;
  task_CleanCompletedPipe(stageBase,1);//1 len 1  only =>8
  stageBase-=1;
  task_ExecuteMinDistTasks(stageBase,1);//1 len 1 only =>9
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
  }


  task_pulseStageExec(countSkip,subPulseSkipCount);

}

uint32_t pulseHZ = 0;

void setup_Stepper() {
  DEBUG_println(".....");
  
  RESET_GateSensing();
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
