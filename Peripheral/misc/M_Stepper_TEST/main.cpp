/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppers.hpp"
using namespace std;



int TIMESCALE_ms=100;




#define SUBDIV 800



class MStp_M:public MStp{
  public:

  int stepCount[MSTP_VEC_SIZE];
  int FACCT=0;
  MStp_M(RingBuf<struct runBlock> *_blocks):MStp(_blocks)
  {
    
    TICK2SEC_BASE=100000;
    PULSE_ROUND_SHIFT=7;
    minSpeed=SUBDIV*TICK2SEC_BASE/10000/200;
    acc=SUBDIV/20;
  }

  int M1_reader=1<<(MSTP_VEC_SIZE-2);
  int M2_reader=1<<(MSTP_VEC_SIZE-1);

  void BlockDirEffect(uint32_t idxes)
  {

    // digitalWrite(PIN_M1_DIR, idxes&M1_reader);
    // digitalWrite(PIN_M2_DIR, idxes&M2_reader);
    printf("DIR:%s  ",int2bin(idxes,MSTP_VEC_SIZE));
    

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      printf("%d  ",stepCount[i]);
      // stepCount[i]=0;
    }
    printf("\n");
  }


  

  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_H,uint32_t idxes_L)
  {
    // if(idxes==0)return;
    // printf("===p:%s",int2bin(idxes,5));
    // printf(" d:%s >>",int2bin(axis_dir,5));

    // // printf("PULSE_ROUNDSCALE:%d  ",PULSE_ROUNDSCALE);

    // for(int i=0;i<MSTP_VEC_SIZE;i++)
    // {
    //   int idx_p = axis_pul&(1<<i);
    //   printf("%03d ",curPos_residue.vec[i]*(idx_p?1:0));

    // }

    // printf("\n");
    
 
    if(axis_st&idxes_H)
    {
      printf("==============ERROR pull up\n");
    }

    axis_st|=idxes_H;

 
    if(axis_st&idxes_L!=idxes_L)
    {
      printf("==============ERROR pin down\n");
    }
    axis_st&=~idxes_L;

    if(idxes_L&M1_reader)
    {
      // digitalWrite(PIN_M1_STP, 0);
    }

    if(idxes_L&M2_reader)
    {
      // digitalWrite(PIN_M2_STP, 0);
    }
    // printf("id:%s  ",int2bin(idxes_H,MSTP_VEC_SIZE));
    // printf("ac:%s \n",int2bin(idxes_L,MSTP_VEC_SIZE));

    // int Midx=0;

    
    // Serial.printf("PINs:%s\n",int2bin(axis_st,MSTP_VEC_SIZE));

    if(idxes_H&M1_reader)
    {
      // digitalWrite(PIN_M1_STP, 1);
    }

    if(idxes_H&M2_reader)
    {
      // digitalWrite(PIN_M2_STP, 1);
    }

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      if(idxes_H&(1<<i))
      {
        stepCount[i]+=(axis_dir&(1<<i))==0?1:-1;
      }
    }


    if(idxes_L&2)
    {
      
      // printf("MxL:T:%d\n",FACCT);
      FACCT=0;
      // digitalWrite(PIN_M1_STP, 0);
    }
    
    if(idxes_H&2)
    {
      // digitalWrite(PIN_M1_STP, 1);
      // printf("MxH:T:%d\n",FACCT);
      FACCT=0;
    }
  }

};

runBlock blockBuff[20];
RingBuf <runBlock> __blocks(blockBuff,20);

MStp_M mstp(&__blocks);



float delayRoundX=0;
void first_thread_job()
{
  while(1)
  {
 
    {
      // if(mstp.tskrun_state==0)
      //   printf("tskrun_state:0\n");
      
      int T = mstp.taskRun();
      // printf("T:%d\n",T);
      

      if(T<0)
      {
        
        printf("ERROR:: T(%d)<0\n",T);
        break;
      }
      if(T==0)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // mstp.VecTo((xVec){.vec={50,49,10}},40);
        
      }
      else
      {
        mstp.FACCT+=T;
      }
      // std::this_thread::sleep_for(std::chrono::milliseconds(T/TIMESCALE_ms));
      continue;
    }
  }
}


int main()
{
  
  if(0)for(int i=0;i<100;i++)
  {
    // V1:0.000000, a1:3.000000, VT:100.099998, V2:0.000000, a2:-3.000000, D:20.000000

    float V1=i/5.0,V2=0,VT=100,a=5,D=18;


    float T,T1,T2;
    // T = totalTimeNeeded(V1, V2,VT, a,D,&T1,&T2);
    // printf(" V1:%f,V2:%f,VT:%f,a:%f,D:%f => T:%f   T1:%f   T2:%f  \n",V1,V2,VT,a,D, T,T1,T2);


    float a1=a*((V1<VT)?1:-1);
    float a2=a*((VT<V2)?1:-1);
    T = totalTimeNeeded2(V1,a1,VT, V2,a2, D,&T1,&T2);

    // printf("V1:%f, a1:%f, VT:%f, V2:%f, a2:%f, D:%f => T:%f   T1:%f   T2:%f  \n",V1,a1,VT, V2,a2,D, T,T1,T2);


    float Vc=V1+a1*T1;
    printf(" T1:%f T2:%f  \n",T1,T2);
    // printf(" T1:%f   T-T2:%f  \n",T1,T-T2);
    float calc_D=
        (V1+Vc)*T1/2+
        (T2-T1)*Vc+
        (Vc+V2)*(T-T2)/2;
    if(T2-T1>0.0001)
    {//reaches VT
      printf("/   \\");
    }
    else
    {//T1 T2 meets
      printf("   /\\");
    }
    printf(" Vc:%f  :%f calc_D:%f \n\n",Vc,V2-a2*(T-T2),calc_D );
 
  }
  thread first_thread(first_thread_job);
  
  for(int i=0;i<3;i++)
  {
    mstp.VecTo((xVec){.vec={122,217,97}},340);
    mstp.VecTo((xVec){.vec={20,-10,2}},340);
    mstp.VecTo((xVec){.vec={0,0,0}},340);
  }
  // mstp.VecTo((xVec){.vec={100,-300,-50}});
  // mstp.VecTo((xVec){.vec={50,0,0}});

  first_thread.join();
  return 0;
}
