/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include <initializer_list>
#include <thread>


#include "MSteppers.hpp"
using namespace std;



int TIMESCALE_ms=100;



 
// MSTP_setup mstp_setup={
//   .axis_setup={
//     {
//       .maxAcc=20,
//       .minSpeed=100,
//       .maxSpeed=10000,
//       .dirFlip=false,
//       .zeroDir=true

//     },
//     {
//       .maxAcc=20,
//       .minSpeed=100,
//       .maxSpeed=10000,
//       .dirFlip=false,
//       .zeroDir=true

//     },
//     // {
//     //   .maxAcc=20,
//     //   .minSpeed=100,
//     //   .maxSpeed=10000,
//     //   .dirFlip=false,
//     //   .zeroDir=true

//     // },
//   }
// };



#define SUBDIV (1600)
#define mm_PER_REV 10


class MStp_M:public MStp{
  public:

  int stepCount[MSTP_VEC_SIZE];
  int FACCT=0;
  int FACCT2=0;

  void TAdd(int T)
  {
    FACCT+=T;
    FACCT2+=T;
  }

  MStp_M(MSTP_segment *buffer, int bufferL):MStp(buffer,bufferL)
  {
    
    // TICK2SEC_BASE=100000;
    // // PULSE_ROUND_SHIFT=15;
    // minSpeed=SUBDIV*TICK2SEC_BASE/10000/200;
    // acc=SUBDIV/20;

    // for(int i=0;i<MSTP_VEC_SIZE;i++)
    // {
    //   MSTP_axis_setup &aset=axisSetup->axis_setup[i];
    //   printf("%f,%f,%f, %d,%d\n",
    //   aset.mmpp,
    //   aset.maxAcc,
    //   aset.minSpeed,
    //   aset.dirFlip,
    //   aset.zeroDir);
      
    // }
    
    TICK2SEC_BASE=10*1000*1000;
    main_acc=SUBDIV*1500/mm_PER_REV;//SUBDIV*3200/mm_PER_REV;
    minSpeed=sqrt(main_acc);//SUBDIV*TICK2SEC_BASE/10000/200/10/mm_PER_REV;
    main_junctionMaxSpeedJump=minSpeed;//5200;

    maxSpeedInc=minSpeed;
    
    axisInfo[0].AccW=1.5;
    axisInfo[0].MaxSpeedJumpW=1.5;

    axisInfo[1].AccW=1;
    axisInfo[1].MaxSpeedJumpW=1;
  
  }

  int M1_reader=2;//1<<(MSTP_VEC_SIZE-2);
  int M2_reader=1<<(MSTP_VEC_SIZE-1);

  void BlockEndEffect(MSTP_segment* seg)
  {

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      printf("%d  ",stepCount[i]);
      // stepCount[i]=0;
    }
    printf("<<<<<<<<<<<<<\n");
  }

  void BlockInitEffect(MSTP_segment* seg)
  {
  }



  void BlockDirEffect(uint32_t dir_idxes)
  {

    // digitalWrite(PIN_M1_DIR, idxes&M1_reader);
    // digitalWrite(PIN_M2_DIR, idxes&M2_reader);
    printf("DIR:%s  ",int2bin(dir_idxes,MSTP_VEC_SIZE));
    
    
  }
    
  

  uint32_t axis_st=0;
  void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)
  {
    
    printf("===T:%s",int2bin(idxes_T,MSTP_VEC_SIZE));
    printf(" R:%s >>\n",int2bin(idxes_R,MSTP_VEC_SIZE));

    // for(int i=0;i<MSTP_VEC_SIZE;i++)
    // {
    //   printf("%d  ",curPos_c.vec[i]);
    //   // stepCount[i]=0;
    // }
    // printf("\n");




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
    
 
    if(axis_st&idxes_T)
    {
      printf("==============ERROR pull up\n");
    }

    axis_st|=idxes_T;

 
    if(axis_st&idxes_R!=idxes_R)
    {
      printf("==============ERROR pin down\n");
    }
    axis_st&=~idxes_R;

    if(idxes_R&M1_reader)
    {
      // digitalWrite(PIN_M1_STP, 0);
    }

    if(idxes_R&M2_reader)
    {
      // digitalWrite(PIN_M2_STP, 0);
    }
    // printf("id:%s  ",int2bin(idxes_T,MSTP_VEC_SIZE));
    // printf("ac:%s \n",int2bin(idxes_R,MSTP_VEC_SIZE));

    // int Midx=0;

    
    // Serial.printf("PINs:%s\n",int2bin(axis_st,MSTP_VEC_SIZE));

    if(idxes_T&1)
    {
      // printf("M1H:TS:%d\n",FACCT);
      FACCT=0;
      // digitalWrite(PIN_M1_STP, 1);
    }
    if(idxes_T&2)
    {
      // printf("M2H:TS:%d\n",FACCT2);
      FACCT2=0;
      // digitalWrite(PIN_M1_STP, 1);
    }


    if(idxes_T&M2_reader)
    {
      // digitalWrite(PIN_M2_STP, 1);
    }

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      if(idxes_T&(1<<i))
      {
        stepCount[i]+=(axis_dir&(1<<i))==0?1:-1;
      }
    }


    // if(idxes_R&1)
    // {
      
    //   printf("MxL:T:%d\n",FACCT);
    //   FACCT=0;
    //   // digitalWrite(PIN_M1_STP, 0);
    // }
    
    // if(idxes_T&1)
    // {
    //   // digitalWrite(PIN_M1_STP, 1);
    //   printf("MxH:T:%d\n",FACCT);
    //   FACCT=0;
    // }
  }

};



#define MSTP_BLOCK_SIZE 30
static MSTP_segment blockBuff[MSTP_BLOCK_SIZE];

MStp_M mstp(blockBuff,MSTP_BLOCK_SIZE);



float delayRoundX=0;
void first_thread_job()
{
  while(1)
  {
 
    {
      // if(mstp.tskrun_state==0)
      //   printf("tskrun_state:0\n");
      
      int T = mstp.taskRun();
      printf("T:%d\n",T);
      

      if(T<0)
      {
        
        printf("ERROR:: T(%d)<0\n",T);
        break;
      }
      if(T==0)
      {
        // std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        break;
        // mstp.VecTo((xVec){.vec={50,49,10}},40);
        
      }
      else
      {
        mstp.TAdd(T);
      }
      // std::this_thread::sleep_for(std::chrono::milliseconds(T/TIMESCALE_ms));
      continue;
    }
  }
}


int main()
{
  
  // for(int i=0;i<3;i++)
  // {
  //   mstp.VecTo((xVec){.vec={122,217,97}},340);
  //   mstp.VecTo((xVec){.vec={20,-10,2}},340);
  //   mstp.VecTo((xVec){.vec={0,0,0}},340);
  // }
  // mstp.VecTo((xVec){20,20,0},1000);
  // mstp.VecTo((xVec){0,0,0},1000);
  // mstp.VecTo((xVec){-20,0,-0},1000);
  // mstp.VecTo((xVec){0,0,0},1000);
  // mstp.VecTo((xVec){20,20,0},1000);


  int pos=18000*2;
  uint32_t speed=45000;

  xVec cpos=(xVec){0};
  // for(int i=0;i<5;i++)
  // {
    
  //   // mstp.VecTo((xVec){pos*(i+50)/100,pos*50/100},speed);
  //   // mstp.VecTo((xVec){pos,pos},speed);
  //   // mstp.VecTo((xVec){pos*(i+26)/50,pos*26/50},speed);
  //   // mstp.VecTo((xVec){0,0},speed);
  //   mstp.VecTo((xVec){pos*(50+2*i)/100,pos*50/100},speed);
  //   mstp.VecTo((xVec){pos,pos},speed);
  //   mstp.VecTo((xVec){pos*(50+2*i)/100,pos*50/100},speed);
  //   mstp.VecTo((xVec){0,0},speed);
  // }
  // mstp.VecTo((xVec){0,100},speed);
  // mstp.VecTo((xVec){100,100},speed);
  // mstp.VecTo((xVec){0,0},speed);

  mstp.VecTo((xVec){0,0},speed);
  for(int k=0;k<2;k++)
  {
    int speed=300;
    // pickOn(1,0+posDiff, speed);posDiff-=12*SUBDIV/mm_PER_REV;
    // pickOn(2,0+posDiff, speed);posDiff-=12*SUBDIV/mm_PER_REV;

    mstp.VecTo((xVec){20,20},speed);
    mstp.VecTo((xVec){10,10},speed);
    
    mstp.AddWait(1000);
    // busyLoop(1000);
    
    mstp.VecTo((xVec){0,0},speed);
    mstp.VecTo((xVec){1,1},speed);
    mstp.VecTo((xVec){0,0},speed);
    // sleep(1);

  }
  // mstp.VecAdd((xVec){1000,1000},speed);
  // mstp.VecAdd((xVec){800,1000},speed);
  // mstp.VecAdd((xVec){1000,1000},speed);
  // return 0;
  // mstp.VecTo((xVec){0,0},speed);
  printf("===========\n");
  mstp.printSEGInfo();
  thread first_thread(first_thread_job);
  first_thread.join();
  return 0;
}
