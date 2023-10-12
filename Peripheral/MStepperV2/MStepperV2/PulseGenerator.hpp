#pragma once

#include "MSteppersV2.hpp"


class PulseGenerator{

public:
  uint32_t segPeriod_next;
  uint32_t segSteps_next;
  xVec vec_next;
  xVec vec_abs_next;
  uint32_t dir_bit_next;


  uint32_t segPeriod;
  uint32_t segSteps;
  xVec vec;
  xVec vec_abs;
  uint32_t dir_bit;
  


  uint32_t cur_wait_time_total;
  uint32_t cur_stp_bit;
  uint32_t cur_step;
  xVec cur_vec_mod;


  PulseGenerator()
  {
    cur_step=segSteps_next=segSteps=0;
    dir_bit=dir_bit_next=0;
  }

  virtual void bufferEmpty()=0;
  virtual void timerNext(uint32_t period_us)=0;
  virtual void pinInfoSet(uint32_t stp,uint32_t dir)=0;
  virtual void pinUpdate()=0;

  uint32_t CalcNextStep()
  {
   
    uint32_t _axis_pul=0;
    uint32_t steps_main=segSteps;
    for(int k=0;k<MSTP_VEC_SIZE;k++)
    {
    

      int32_t vele_abs=vec_abs.vec[k];
      int32_t &curPos_mod_vec=cur_vec_mod.vec[k];
      curPos_mod_vec+=vele_abs;
      if(curPos_mod_vec>=steps_main)//a step forward
      {
        curPos_mod_vec-=steps_main;

        {
          // int32_t residue=vele_abs-curPos_mod_vec;
          _axis_pul|=1<<k;
        }

      }
      else
      {
        // curPos_residue.vec[k]=0;
      }

    }
    return _axis_pul;
  }


  void loadNext(uint32_t seg_time_us,xVec vec)
  {
    
    vec_next=vec;
    vec_abs_next=vec;

    int32_t max_vele_abs=0;
    for(int k=0;k<MSTP_VEC_SIZE;k++)
    { 
      auto vele=vec.vec[k];
      auto vele_abs=0;



      if(vele)
      {
        if(vele>0)
        {
          dir_bit_next&=~(1<<k);
          vele_abs=vele;
        }
        else
        {//reverse dir =1
          dir_bit_next|=(1<<k);
          vele_abs=-vele;
        }
        if(max_vele_abs<vele_abs)
          max_vele_abs=vele_abs;
      }


      vec_abs_next.vec[k]=vele_abs;


    }
    // max_vele_abs*=2;//subdivide

    segPeriod_next=seg_time_us;///max_vele_abs;
    
    if(max_vele_abs==0)
    {
      // dir_bit_next=dir_bit;//keep the same
      max_vele_abs=1;
    }
    segSteps_next=max_vele_abs;
  }


  void nextStep()
  {
    pinUpdate();
    if(segSteps==0)
    {
      if(segSteps_next!=0)//load new
      {
        segPeriod=segPeriod_next;
        vec=vec_next;
        vec_abs=vec_abs_next;
        dir_bit=dir_bit_next;
        cur_vec_mod=(xVec){0};


        cur_step=0;
        segSteps=segSteps_next;
        segSteps_next=0;
        cur_wait_time_total=0;
        bufferEmpty();
      }
      else//still no pulses to generate
      {
        bufferEmpty();
        pinInfoSet(0,dir_bit);//0
        timerNext(1000);
        return;
      }
    }
    
    pinInfoSet(0,dir_bit);//0


    //run step
    uint32_t wtime=(segPeriod-cur_wait_time_total)/(segSteps-cur_step);
    cur_wait_time_total+=wtime;
    timerNext(wtime);

    //Do things
    uint32_t _stp_bit=CalcNextStep();


    pinUpdate();
    pinInfoSet(_stp_bit,dir_bit);//1
    cur_stp_bit=_stp_bit;

    cur_step++;
    if(cur_step==segSteps)
    {
      segSteps=0;
    }

  }

};