
#include "include/UTIL.hpp"

#include "include/main.h"
#define DEBUG_

uint32_t _logicPulseCount_ = 0;
bool _senseInv_=false;
bool* getSenseInvPtr()
{
  return &_senseInv_;
}

static struct sharedInfo sInfo;
struct sharedInfo* get_SharedInfo()
{
  return &sInfo;
}

uint32_t thres_skip_counter = 0;

class StepperMotor
{
  public:
    int p1, p2;

    StepperMotor(int p1, int p2){

      pinMode(p1, OUTPUT);
      pinMode(p2, OUTPUT);
      this->p1 = p1;
      this->p2 = p2;
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


StepperMotor stepperMotor(STEPPER_PLS_PIN, STEPPER_DIR_PIN);
uint32_t pulseHZ = 0;//real current Pulse Hz


typedef struct GateInfo {
  uint32_t start_pulse;
  uint32_t end_pulse;
  uint16_t debunce;
  uint8_t cur_Sense;


} GateInfo;



//uint32_t logicPulseCount = 0;

GateInfo gateInfo;




void RESET_GateSensing()
{
  GateInfo ngateInfo = {0};
  ngateInfo.cur_Sense=0;
  ngateInfo.start_pulse=~0;
  ngateInfo.end_pulse=~0;
  gateInfo=ngateInfo;
}



void insert_fake_pulse()
{
}


const int  SINGLE_PULSE_DIST_um = (int)(240000/perRevPulseCount*2*3.141);
void task_gateSensing(uint8_t stage,uint8_t stageLen)
{
  if(stage>=stageLen)return;
  const int  minWidth = 2;
  const int  maxWidth = 1+40000/SINGLE_PULSE_DIST_um;
  
  const int  DEBOUNCE_L_THRES = 1+3000/SINGLE_PULSE_DIST_um;//object inner connection
  const int  DEBOUNCE_H_THRES = 1;
  //(perRevPulseCount/50)
  uint8_t new_Sense = digitalRead(GATE_PIN);
  if(_senseInv_)new_Sense=!new_Sense;
  bool onSenseEdge=false;

  
  if(gateInfo.cur_Sense)
  {//H
    if(!new_Sense)//L
    {
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        onSenseEdge=true;
        gateInfo.debunce = DEBOUNCE_H_THRES;
      
      }
    }
    else
    {
      gateInfo.debunce = DEBOUNCE_L_THRES;
      gateInfo.end_pulse=_logicPulseCount_;
    }
  }
  else
  {//L cur_Sense
    if(new_Sense)//H
    {
      gateInfo.debunce--;
      if(gateInfo.debunce==0)
      {
        onSenseEdge=true;
        gateInfo.debunce = DEBOUNCE_L_THRES;
      }
    }
    else
    {
      gateInfo.debunce = DEBOUNCE_H_THRES;
      gateInfo.start_pulse=_logicPulseCount_;
    }
  }

  
  if(onSenseEdge)
  {
    if(!new_Sense)
    {//a pulse is completed

      uint32_t diff=gateInfo.end_pulse-gateInfo.start_pulse;
      if( diff>minWidth && diff<maxWidth )
      {
        uint32_t middle_pulse=gateInfo.start_pulse+(diff>>1);

        // uint32_t avg_PD_B2M=(pre_pulseDist_B2M+pulseDist_B2M)>>1;
        // if(pulseDist_B2M>(minPulseDist*2/3) && avg_PD_B2M>minPulseDist)
        task_newPulseEvent(gateInfo.start_pulse,gateInfo.end_pulse,middle_pulse,diff);
  
      }
      else
      {

        sInfo.skippedPulse++;
          //skip the pulse : the pulse width is not in the valid range
          //this might be caused by too large object > typ:2cm
          //or there are multiple objects too close to each other 
          //control by   minWidth,maxWidth,      
          //also effected DEBOUNCE_L_THRES,DEBOUNCE_H_THRES(these two are to control what is a complete pulse high time, low time)
      }
      gateInfo.start_pulse=_logicPulseCount_;
    }
    else
    {
      gateInfo.end_pulse=_logicPulseCount_;
    }

    gateInfo.cur_Sense=new_Sense;

  }



}



void task_pulseStageExec(uint8_t stage,uint8_t stageLen)
{//assume stageLen==5  
 //0 1 2 3 4 0 1 2 3 4 
 //P   E     P   E
 //P for pulse detection
 //E for action execution
  //exp:stageLen=10  0~9
  task_gateSensing(stage-0,1);//only run at the first stage
  if(stage==stageLen/2)//only run at the middle stage
  {
    Run_ACTS(_logicPulseCount_);
  }
}


uint32_t SubPulseStage = 0;
uint32_t revCount = 0;
TIMER_SET_ISR(1,8)
uint32_t _logicPulseCount_preRev=0;
ISR(TIMER1_COMPA_vect) 
{
  stepperMotor.OneStep(true);

  
  SubPulseStage = (SubPulseStage+1)&(subPulseSkipCount-1);
  
  if (SubPulseStage==0)
  {
    _logicPulseCount_ ++;
    if(_logicPulseCount_-_logicPulseCount_preRev+1>=perRevPulseCount)
    {
      _logicPulseCount_preRev=_logicPulseCount_;
      revCount++;
      EV_Axis0_Origin(revCount);
    }
  }


  task_pulseStageExec(SubPulseStage,subPulseSkipCount);

}


void setup_Stepper() {
  DEBUG_println(".....");
  
  RESET_GateSensing();
  timer1Setup(1);
  timer1_HZ(0);
}
uint32_t get_Stepper_pulse_count() {
  return _logicPulseCount_;
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
