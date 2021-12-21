/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppers.hpp"
using namespace std;


class MStp_M:public MStp{
  public:


  MStp_M(RingBuf<struct runBlock> *_blocks):MStp(_blocks)
  {
  }


  void BlockRunEffect() override
  {
    printf("p:%s",int2bin(axis_pul,5));
    printf(" d:%s \n",int2bin(axis_dir,5));



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


    
    mstp.timerTask();
    uint32_t nextT= mstp.T_next;
    // if(nextT<100)
    // {
    //   nextT=100;
    // }
    // uint32_t nextT=100;
    // timerAlarmWrite(timer,nextT, true);

    printf("nextT:%d\n",nextT);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
  // return 0;

  thread first_thread(first_thread_job);

  mstp.VecTo((xVec){.vec={50,0,0}});
  // mstp.VecTo((xVec){.vec={100,-300,-50}});
  // mstp.VecTo((xVec){.vec={50,0,0}});

  first_thread.join();
  return 0;
}
