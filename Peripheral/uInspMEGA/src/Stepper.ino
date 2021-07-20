
#include "include/UTIL.hpp"

#define DEBUG_

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
};


StepperMotor stepperMotor(22, 23, 24, 25);

uint32_t pulseHZ = 0;


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

GateInfo gateInfo;




void RESET_GateSensing()
{
  GateInfo ngateInfo = {0};
  ngateInfo.cur_Sense=0;
  ngateInfo.start_pulse=~0;
  ngateInfo.end_pulse=~0;
  gateInfo=ngateInfo;
}


void skip_pulse_showS(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
  pipeLineInfo X;
  pipeLineInfo *head=&X;
  head->s_pulse=start_pulse;
  head->e_pulse=end_pulse;
  head->pulse_width=pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status=insp_status_UNSET;
  skip_pulse_show(head);
}

const int  SINGLE_PULSE_DIST_um = (int)(240000/perRevPulseCount*2*3.141);
#define RING_SUB(A,B,MAX) ( ((A)>(B)) ? ((A)-(B))  : ((A)+(MAX)-(B)) )
int task_newPulseEvent(uint32_t middle_pulse);
void task_gateSensing(uint8_t stage,uint8_t stageLen)
{
  const int  minWidth = 3;
  const int  maxWidth = 1+20000/SINGLE_PULSE_DIST_um;
  
  const int  DEBOUNCE_L_THRES = 1+6000/SINGLE_PULSE_DIST_um;//object inner connection
  const int  DEBOUNCE_H_THRES = 1;
  //(perRevPulseCount/50)
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

  
  static uint32_t pulseDist_B2M=0;
  static uint32_t pre_pulseDist_B2M=0;
  if(pulseDist_B2M!=10000)
    pulseDist_B2M++;

  if(flip)
  {
    if(!new_Sense)
    {//a pulse is completed

      uint32_t diff=RING_SUB(gateInfo.end_pulse,gateInfo.start_pulse,perRevPulseCount);
      if( diff>minWidth && diff<maxWidth )
      {
        uint32_t middle_pulse=mod_sim(gateInfo.start_pulse+(diff>>1),perRevPulseCount);

        uint32_t minPulseDist=pulseHZ/subPulseSkipCount/g_max_frame_rate;
        // uint32_t avg_PD_B2M=(pre_pulseDist_B2M+pulseDist_B2M)>>1;
        // if(pulseDist_B2M>(minPulseDist*2/3) && avg_PD_B2M>minPulseDist)
        if(pulseDist_B2M>minPulseDist)
        {
          pre_pulseDist_B2M=pulseDist_B2M;
          pulseDist_B2M=0;
          task_newPulseEvent(gateInfo.start_pulse,gateInfo.end_pulse,middle_pulse,diff);
        }
        else
        {
          // skip_pulse_showS(gateInfo.start_pulse,gateInfo.end_pulse,middle_pulse,diff);
          //skip the pulse : too fast in time
          //control by pulseHZ/subPulseSkipCount/g_max_frame_rate;
        }
  
      }
      else
      {
          //skip the pulse : the pulse width is not in the valid range
          //this might be caused by too large object > typ:2cm
          //or there are multiple objects too close to each other 
          //control by   minWidth,maxWidth,      
          //also effected DEBOUNCE_L_THRES,DEBOUNCE_H_THRES(these two are to control what is a complete pulse high time, low time)
      }
      gateInfo.start_pulse=logicPulseCount;
    }
    else
    {
      gateInfo.end_pulse=logicPulseCount;
    }

    gateInfo.cur_Sense=new_Sense;

  }



}



int task_newPulseEvent(uint32_t start_pulse, uint32_t end_pulse, uint32_t middle_pulse, uint32_t pulse_width)
{
    
  pipeLineInfo* head = RBuf.getHead();
  if (head == NULL)return -1;

  //get a new object and find a space to log it
  // TCount++;
  head->s_pulse=start_pulse;
  head->e_pulse=end_pulse;
  head->pulse_width=pulse_width;
  head->gate_pulse = middle_pulse;
  head->insp_status=insp_status_UNSET;
  if(ActRegister_pipeLineInfo(head)==0)
  {
    RBuf.pushHead();
  }
  return 0;
}

void task_pulseStageExec(uint8_t stage,uint8_t stageLen)
{
  //exp:stageLen=10  0~9
  uint8_t stageBase=stage;
  task_gateSensing(stageBase,1);//0 only
  stageBase-=1;
  if(stage==stageLen-1)
  {
    Run_ACTS(&act_S,logicPulseCount);
  }
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
