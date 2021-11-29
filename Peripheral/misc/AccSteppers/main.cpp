/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
using namespace std;



#define VEC_SIZE 3

struct xVec
{
  int32_t vec[VEC_SIZE];
};
xVec VECNow={0};



struct runBlock
{
  xVec from;
  xVec to;
  xVec vec;
  uint32_t steps;
  uint32_t cur_step;
  float vcur;
  float vcen;
  float vto;

};

struct sysState
{
  uint32_t axis_pul;
  uint32_t axis_dir;
  xVec curPos;
  xVec lastTarLoc;
  xVec preVec;
  float acc;

  uint32_t T_next;
  uint32_t T_lapsed;
};

void BlockRun(runBlock &rb);


RingBuf_Static <runBlock,100> blocks;


sysState sysInfo;

char *int2bin(uint32_t a, int digits, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    for (int i = digits-1; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }
    buffer++;
    return buffer;
}


char *int2bin(uint32_t a, int digits=sizeof(uint32_t)*8) {
  static char binChar[64+1];
  binChar[sizeof(binChar)-1]='\0';
  return int2bin(a,digits, binChar, sizeof(binChar));
}

xVec vecSub(xVec v1,xVec v2)
{
  for(int i=0;i<VEC_SIZE;i++)
  {
    v1.vec[i]-=v2.vec[i];
  }
  return v1;
}

xVec vecAdd(xVec v1,xVec v2)
{
  for(int i=0;i<VEC_SIZE;i++)
  {
    v1.vec[i]+=v2.vec[i];
  }
  return v1;
}

uint32_t vecMachStepCount(xVec vec)
{
  uint32_t maxDist=0;

  for(int i=0;i<VEC_SIZE;i++)
  {
    int32_t dist = vec.vec[i];
    if(dist<0)dist=-dist;
    if(maxDist<dist)
    {
      maxDist=dist;
    }
  }

  return maxDist;
}
uint32_t vecMachStepCount(xVec v1,xVec v2)
{
  return vecMachStepCount(vecSub(v1 ,v2));
}



float Q_rsqrt( float number )
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;                       // evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}


float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1=NULL, float *ret_T2=NULL)
{

  float T1L=(VT-V1)/a1;
  float T2L=(V2-VT)/a2;
  float baseT=T1L+T2L;


  float tri1=(VT*VT-V1*V1)/(2*a1);
  float tri2=(V2*V2-VT*VT)/(2*a2);
  
  printf("V1:%f, a1:%f, VT:%f, V2:%f, a2:%f, D:%f  tri1:%f tri2:%f\n",V1,a1,VT, V2,a2,D,tri1,tri2);

  float restRect=D-(tri1+tri2);




  if(restRect>0)
  {
    float T=baseT;
    T+=D/VT;
    T+=-(tri1+tri2)/(VT);

    if(ret_T1)
    {
      *ret_T1=T1L;
    }
    if(ret_T2)
    {
      *ret_T2=T-T2L;
    }


    return T;
  }

  float ar=a1/(a1-a2);
  float Tcut=(-VT+sqrt(VT*VT+2*a2*ar*(-restRect)))/(a2*ar);
  // printf("baseT:%f Tcut:%f   -restRect:%f\n",baseT,Tcut,-restRect);
  float T=baseT-Tcut;
  if(ret_T1)
  {
    *ret_T1=T1L-Tcut*(1-ar);
  }
  if(ret_T2)
  {
    *ret_T2=T-T2L+Tcut*ar;
  }



  return T;
}

float accTo_DistanceNeeded(float Vc, float Vd, float ad, float *ret_Td=NULL)
{
  float T=(Vd-Vc)/ad;
  float D=(Vd*Vc)*T/2;

  if(ret_Td)*ret_Td=T;

  return D;
}

void VecTo(xVec VECTo,float speed=30)
{
  runBlock rb={
    .from = sysInfo.lastTarLoc,
    .to=VECTo,
    .vec = vecSub(VECTo,sysInfo.lastTarLoc),
    .steps=vecMachStepCount(VECTo,sysInfo.lastTarLoc),
    .cur_step=0,
    .vcur=0,
    .vcen=30,
    .vto=0,
  };

  sysInfo.lastTarLoc=VECTo;
  sysInfo.preVec=rb.vec;


  printf("steps:%d\n",rb.steps);

  runBlock *tail = NULL;
  
  if(blocks.size()>0)
  {
    tail = blocks.getTail(blocks.size()-1);
  }
  if(tail!=NULL)
  {// you need to deal with the junction speed
    // tail->vec;
    // rb.vec;
    
    float dotp=0;
    float absA=0;
    float absB=0;
    for(int i=0;i<VEC_SIZE;i++)
    {
      dotp+=tail->vec.vec[i]*rb.vec.vec[i];
      absA+=tail->vec.vec[i]*tail->vec.vec[i];
      absB+=rb.vec.vec[i]*rb.vec.vec[i];
    }
    float cosinSim=dotp*Q_rsqrt(absA*absB);
    if(cosinSim<0)cosinSim=0;
    tail->vto=cosinSim*rb.vcen;
    printf("cosinSim:%f  vto:%f\n",cosinSim,tail->vto);

  }
  else
  {






    // rb.vcur=100;
    sysInfo.T_next=100000;
  }
  *blocks.getHead()=rb;

  blocks.pushHead();

}





void BlockRunEffect(runBlock &rb)
{
  sysInfo.lastTarLoc=sysInfo.curPos;
  for(int k=0;k<VEC_SIZE;k++)
  {
    printf("%04d,",sysInfo.curPos.vec[k]);
  }
  printf(" dir:%s ",int2bin(sysInfo.axis_dir,VEC_SIZE));
  printf("pul:%s v:%f T:%f \n",int2bin(sysInfo.axis_pul,VEC_SIZE),rb.vcur,1/rb.vcur);
}
void BlockRunStep(runBlock &rb)
{
  // rb.T_lapsed+=rb.T_next;
  // int idxDeAcc=(int)(rb.steps-(rb.vcur-rb.vto)/sysInfo.acc)+1;

  float T1,T2;
  float a1=sysInfo.acc;
  float a2=-sysInfo.acc;
  float V1=rb.vcur;
  float V2=rb.vto;
  float D = (rb.steps-rb.cur_step);
  
  float D_=D-1;
  float T= totalTimeNeeded2(V1,a1,rb.vcen, V2,a2,D_,&T1,&T2);
  

  float Vc=V1+a1*T1;

  
  float D2Area= (Vc+V2)*(T-T2)/2;
  float D2Step= D_-D2Area;
  
  printf("step:%5d T:%04.3f T1:%04.3f  T2:%04.3f D_deacc:%f\n",rb.cur_step,T,T1,T-T2,D2Step);

  float calc_D=
      (V1+Vc)*T1/2+
      (T2-T1)*Vc+
      (Vc+V2)*(T-T2)/2;
  if(T2-T1>0.01)
  {//reaches VT
    printf("/   \\");
  }
  else
  {//T1 T2 meets
    printf("   /\\");
  }
  printf(" Vc:(%f~%f) calc_D:%f D:%f  T_next:%d  T_lapsed:%d  realEstT:%f\n",Vc,V2-a2*(T-T2),calc_D ,D,sysInfo.T_next,sysInfo.T_lapsed, T+sysInfo.T_lapsed/1000000.0);


  // float DBufferDist=accTo_DistanceNeeded(rb.vcur, rb.vto, sysInfo.acc*(rb.vcur>rb.vto?-1:1), &T2);

  // printf("DBufferDist:%f T2:%f \n",DBufferDist,T2);
  // printf(" rb.vcur:%f, rb.vto:%f\n\n",rb.vcur, rb.vto);


  // if(sysInfo.T_next>1000)
  // {
  //   sysInfo.T_next=1000;
  // }

  if(D2Step<1.01)
  {
    float calcAcc=sysInfo.acc;
    rb.vcur-=sysInfo.acc*(sysInfo.T_next-T2)/1000000;
    if(rb.vcur<rb.vto)
    {
      rb.vcur=rb.vto;
    }
  }
  else if(rb.vcur<rb.vcen)
  {
    rb.vcur+=sysInfo.acc*sysInfo.T_next/1000000;
    
    if(rb.vcur>Vc)
    {
      rb.vcur=Vc;
    }
  }
  if(rb.vcur<3)
  {
    rb.vcur=3;
  }
  // printf("=%03d==: ",rb.cur_step);
  int stepx=100*(rb.cur_step+1);//100x is for round

  uint32_t axis_pul=0;
  for(int k=0;k<VEC_SIZE;k++)
  {
    int diff = stepx*rb.vec.vec[k]/(int)rb.steps;
    if(diff%100>=50)//
    {
      diff+=100;
    }
    diff/=100;
    int newVec=rb.from.vec[k]+diff;
    // printf("%d,",newVec);
    axis_pul<<=1;
    if(sysInfo.curPos.vec[k]!=newVec)
      axis_pul|=1;

    sysInfo.curPos.vec[k]=newVec;
  }
  sysInfo.axis_pul=axis_pul;
  if(rb.cur_step==0)
  {
    uint32_t axis_dir=0;
    for(int k=0;k<VEC_SIZE;k++)
    {
      axis_dir<<=1;
      if(rb.vec.vec[k]<0)
      {
        axis_dir|=1;
      }
    }

    sysInfo.axis_dir=axis_dir;

  }
  rb.cur_step++;
}


float delayRoundX=0;
void first_thread_job()
{
  while(1)
  {
    if(blocks.size()>0)
    {
      runBlock &blk=*blocks.getTail();
      BlockRunStep(blk);

      float T = 1000000/blk.vcur;
      sysInfo.T_next=(int)(T);
      delayRoundX+=T-sysInfo.T_next;
      if(delayRoundX>1)
      {
        delayRoundX-=1;
        sysInfo.T_next+=1;
      }


      sysInfo.T_lapsed+=sysInfo.T_next;
      std::this_thread::sleep_for(std::chrono::microseconds(sysInfo.T_next));
      
      BlockRunEffect(blk);
      if(blk.cur_step==blk.steps)
      {
        float vcur= blk.vcur;
        printf("======vcur:%f=vto:%f====sysInfo.T_lapsed:%d===\n",vcur,blk.vto,sysInfo.T_lapsed);
        blocks.consumeTail();
        runBlock *new_blk=blocks.getTail();
        if(new_blk!=NULL)
        {
          new_blk->vcur= vcur;
        }
        sysInfo.T_lapsed=0;
      }
      
      printf("\n\n\n");
    }
    else
    {
      // cout << "This is the first thread "<< endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}


int main()
{
  
  sysInfo=(sysState){.acc=100};
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

  VecTo((xVec){.vec={50,0,0}});
  VecTo((xVec){.vec={0,0,0}},100);
  VecTo((xVec){.vec={50,0,0}});

  first_thread.join();
  return 0;
}


// void setup() {
//   size(640, 480);

    

// }

// void draw() {
//   xd::rect(50, 50, 100, 100);
// }

// void destroy() {

// }