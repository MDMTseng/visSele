/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppers.hpp"
using namespace std;

#ifdef X86_PLATFORM
#define __PRT__(...) printf(__VA_ARGS__)
#else
#include <Arduino.h>
#define __PRT__(...) //Serial.printf(__VA_ARGS__)
#endif


char *int2bin(uint32_t a, int digits, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    for (int i = digits-1; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }
    buffer++;
    return buffer;
}


char *int2bin(uint32_t a, int digits) {
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

float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1, float *ret_T2)
{

  float T1L=(VT-V1)/a1;
  float T2L=(V2-VT)/a2;
  float baseT=T1L+T2L;


  float tri1=(VT*VT-V1*V1)/(2*a1);
  float tri2=(V2*V2-VT*VT)/(2*a2);
  
  __PRT__("V1:%f, a1:%f, VT:%f, V2:%f, a2:%f, D:%f  tri1:%f tri2:%f\n",V1,a1,VT, V2,a2,D,tri1,tri2);

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
  // __PRT__("baseT:%f Tcut:%f   -restRect:%f\n",baseT,Tcut,-restRect);
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

void MStp::VecTo(xVec VECTo,float speed)
{
  runBlock rb={
    .from = lastTarLoc,
    .to=VECTo,
    .vec = vecSub(VECTo,lastTarLoc),
    .steps=vecMachStepCount(VECTo,lastTarLoc),
    .cur_step=0,
    .vcur=0,
    .vcen=speed,
    .vto=0,
  };

  lastTarLoc=VECTo;
  preVec=rb.vec;


  __PRT__("steps:%d\n",rb.steps);

  runBlock *tail = NULL;
  
  if(blocks->size()>0)
  {
    tail = blocks->getTail(blocks->size()-1);
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
    __PRT__("cosinSim:%f  vto:%f\n",cosinSim,tail->vto);

  }
  else
  {






    // rb.vcur=100;
    T_next=200;
  }
  *(blocks->getHead())=rb;

  blocks->pushHead();
}





void MStp::BlockRunStep(runBlock &rb)
{
  float T1,T2;
  float a1=acc;
  float a2=-acc;
  float V1=rb.vcur;
  float V2=rb.vto;
  float D = (rb.steps-rb.cur_step);
  
  float D_=D-1;
  float T= totalTimeNeeded2(V1,a1,rb.vcen, V2,a2,D_,&T1,&T2);
  

  float Vc=V1+a1*T1;

  
  float D2Area= (Vc+V2)*(T-T2)/2;
  float D2Step= D_-D2Area;
  
  __PRT__("step:%5d T:%04.3f T1:%04.3f  T2:%04.3f D_deacc:%f\n",rb.cur_step,T,T1,T-T2,D2Step);

  float calc_D=
      (V1+Vc)*T1/2+
      (T2-T1)*Vc+
      (Vc+V2)*(T-T2)/2;
  if(T2-T1>0.01)
  {//reaches VT
    __PRT__("/   \\");
  }
  else
  {//T1 T2 meets
    __PRT__("   /\\");
  }
  // __PRT__(" Vc:(%f~%f) calc_D:%f D:%f  T_next:%d  T_lapsed:%d  realEstT:%f\n",Vc,V2-a2*(T-T2),calc_D ,D,T_next,T_lapsed, T+T_lapsed/TICK2SEC_BASE.0);


  // float DBufferDist=accTo_DistanceNeeded(rb.vcur, rb.vto, acc*(rb.vcur>rb.vto?-1:1), &T2);

  // __PRT__("DBufferDist:%f T2:%f \n",DBufferDist,T2);
  __PRT__(" rb.vcur:%f, rb.vto:%f\n\n",rb.vcur, rb.vto);


  // if(T_next>TICK2SEC_BASE)
  // {
  //   T_next=TICK2SEC_BASE;
  // }

  if(D2Step<1.01)
  {
    float calcAcc=acc;
    rb.vcur-=acc*(T_next-T2)/TICK2SEC_BASE;
    if(rb.vcur<rb.vto)
    {
      rb.vcur=rb.vto;
    }

  }
  else if(rb.vcur<rb.vcen)
  {
    rb.vcur+=acc*T_next/TICK2SEC_BASE;
    
    if(rb.vcur>Vc)
    {
      rb.vcur=Vc;
    }
  }



  if(rb.vcur<minSpeed)//set min speed
  {
    rb.vcur=minSpeed;
  }

  __PRT__("=%03d==: ",rb.cur_step);
  int stepx=100*(rb.cur_step+1);//100x is for round

  uint32_t _axis_pul=0;
  for(int k=0;k<VEC_SIZE;k++)
  {
    int diff = stepx*rb.vec.vec[k]/(int)rb.steps;
    if(diff%100>=50)//
    {
      diff+=100;
    }
    diff/=100;
    int newVec=rb.from.vec[k]+diff;
    // __PRT__("%d,",newVec);
    if(curPos.vec[k]!=newVec)
      _axis_pul|=1<<k;

    curPos.vec[k]=newVec;
  }
  axis_pul=_axis_pul;
  if(rb.cur_step==0)
  {
    uint32_t _axis_dir=0;
    for(int k=0;k<VEC_SIZE;k++)
    {
      if(rb.vec.vec[k]<0)
      {
        _axis_dir|=1<<k;
      }
    }

    axis_dir=_axis_dir;

  }
  rb.cur_step++;
}



void MStp::BlockRunEffect()
{
// int2bin(uint32_t a, int digits=sizeof(uint32_t)*8) 

}

void MStp::timerTask()
{

  if(blocks->size()>0)
  {
    runBlock &blk=*blocks->getTail();
    BlockRunStep(blk);

    float T = TICK2SEC_BASE/blk.vcur;
    T_next=(int)(T);
    delayRoundX+=T-T_next;
    if(delayRoundX>1)
    {
      delayRoundX-=1;
      T_next+=1;
    }


    T_lapsed+=T_next;
    // std::this_thread::sleep_for(std::chrono::milliseconds(sysInfo.T_next));
    
    // BlockRunEffect();
    if(blk.cur_step==blk.steps)
    {
      float vcur= blk.vcur;
      __PRT__("======vcur:%f=vto:%f===T_lapsed:%d===\n",vcur,blk.vto,T_lapsed);
      blocks->consumeTail();
      runBlock *new_blk=blocks->getTail();
      if(new_blk!=NULL)
      {
        new_blk->vcur= vcur;
      }
      T_lapsed=0;
    }
    
    // __PRT__("\n\n\n");
  }
  else
  {
    T_lapsed=200;//empty action
    // cout << "This is the first thread "<< endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    axis_pul=0;
  }

}
