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

#define IO_WRITE_DBG(pinno,val) //digitalWrite(pinno,val)
#define IO_SET_DBG(pinno,val) //pinMode(pinno,val)


#define PIN_DBG0 18
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
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]-=v2.vec[i];
  }
  return v1;
}

xVec vecAdd(xVec v1,xVec v2)
{
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]+=v2.vec[i];
  }
  return v1;
}

uint32_t vecMachStepCount(xVec vec)
{
  uint32_t maxDist=0;

  for(int i=0;i<MSTP_VEC_SIZE;i++)
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


float DeAccTimeNeeded2(float VT, float V2, float a2,float *ret_D)
{
  float T = (V2-VT)/a2;
  if(ret_D)
  {
    *ret_D=T*(V2+VT)/2;
  }

  return T;
}



inline float findV1(float D, float a, float V2)
{

      /*
      
      V1
      |\ 
      |  \  a
      |    \   
      |      \ V2 
      |   D   |
      |       |
      |_______|_______

      t=(V2-V1)/a
      D=(V1+V2)t/2
       =V2^2-V1^2/2a
      V2^2-V1^2 = 2aD

      Known  D(area, step count), V2(final speed),  a(acceleration  which a<0)
      try to find initial speed V1

      V1=sqrt(V2^2-2aD) //note  a<0
        =sqrt(V2^2+2|a|D)


      D=
      */
  return sqrt(V2*V2-2*a*D);
}

inline int32_t DeAccDistNeeded(int32_t VT, int32_t V2, int32_t a2)
{
  return (V2*V2-VT*VT)/a2/2;
  // int32_t resx2=((V2*V2-VT*VT)<<2)/a2;
  // return (resx2>>3)+((resx2&(1<<2))?1:0);//do round
}


float accTo_DistanceNeeded(float Vc, float Vd, float ad, float *ret_Td=NULL)
{
  float T=(Vd-Vc)/ad;
  float D=(Vd*Vc)*T/2;

  if(ret_Td)*ret_Td=T;

  return D;
}








/*

to calc junction speed
i for the axis index
  Xi =>  Yi  // speed change target

aXi => abYi // junction speed control with param a & b

TARGET=>
MAX_ALLOWED_SPEED_JUMP>=Max(|aXi-ab'Yi|);



b = argMin( Max(|Xi-bYi|) ) //find b to have the smallest Speed jump
      b
  = argMin( (Sigma((Xi-bYi)^N))^1/N ) // When N is big
      b         i
  ~=argMin( (Sigma((Xi-bYi)^2))^1/2 ) // N = 2 is easier to solve with good enough result
      b         i
b'= argMin( (Sigma((Xi-bYi)^2)) )  //b' is the approximation of b
      b         i
  
  => d(Sigma((Xi-b'Yi)^2)) ) /d(b')=0
        i

b'= Sigma(XiYi)/Sigma(YiYi) = XoY/YoY // o is cross product
        i         i


a=MAX_ALLOWED_SPEED_JUMP/Max(|Xi-b'Yi|);



we have two variables here 
JunctionNormCoeff=b'
JunctionNormMaxDiff=Max(|Xi-b'Yi|)

for further use 
b'=JunctionNormCoeff
a=MAX_ALLOWED_SPEED_JUMP/JunctionNormMaxDiff;
*/


//return ret_blk2_coeff let  blk1.vec => ret_blk2_coeff*blk2.vec would have the smallest speed diff 
int Calc_JunctionNormCoeff(runBlock &blk1,runBlock &blk2,float &ret_blk2_coeff)
{
  ret_blk2_coeff=NAN;
  float BBsum=0;
  float ABsum=0;//basically a dot product
  

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    // float A=(float)tail->runvec.vec[i]/tail->steps;//normalize here, a bit slower
    // float B=(float)rb.runvec.vec[i]/rb.steps;

    float A=(float)blk1.runvec.vec[i];//X
    float B=(float)blk2.runvec.vec[i];//Y

    // if(AB<0 && AB<-junctionMaxSpeedJump)
    // {//The speed from +to-(or reverse) and the difference is too huge, then set target speed to zero
    //   dotp=0;
    //   BB=1;
    //   break;
    // }
    ABsum+=A*B;
    BBsum+=B*B;
  }
  if(ABsum<0)
  {
    ret_blk2_coeff=NAN;
    return -1;
  }
  // rb.JunctionCoeff=dotp/BB;//normalize in the loop, a bit slower
  ret_blk2_coeff = ABsum/BBsum*blk2.steps/blk1.steps;

  
  return 0;
}

int Calc_JunctionNormMaxDiff(runBlock &blk1,runBlock &blk2,float blk2_coeff,float &ret_MaxDiff)
{


  float maxAbsDiff=0;
  ret_MaxDiff=NAN;
  if(blk2_coeff==0 || blk2_coeff!=blk2_coeff)
  {
    ret_MaxDiff=NAN;
    return -1;
  }
  //Xi/Yi is for i axis run step count
  //Xc/Yc is the running steps count
  //target=> Max(Xi/Xc-b*Yi/Yc)
  //      =  Max(Xi-Yi*b*Xc/Yc)/Xc
  //NormCoeff= b*Xc/Yc

  float NormCoeff=blk1.steps*blk2_coeff/blk2.steps;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    float A=(float)blk1.runvec.vec[i];//pre extract steps
    float B=(float)blk2.runvec.vec[i]*NormCoeff;


    __PRT__("maxJump[%d]:%f %f\n",i,A,B);
    float diff = A-B;
    if(diff<0)diff=-diff;
    if(maxAbsDiff<diff)maxAbsDiff=diff;
  }
  maxAbsDiff/=blk1.steps;

  ret_MaxDiff=maxAbsDiff;
  
  return 0;
}







bool PIN_DBG0_st=false;

MStp::MStp(RingBuf<runBlock> *_blocks, MSTP_setup *_axisSetup)
{

  blocks=_blocks;
  axisSetup=_axisSetup;
  maxSpeedInc=500;
  minSpeed=100;
  junctionMaxSpeedJump=300;

  
  TICK2SEC_BASE=10*1000*1000;
  acc=1000
  IO_SET_DBG(PIN_DBG0, OUTPUT);
  SystemClear();
}


void MStp::SystemClear()
{
  
  curPos_c=curPos_mod=curPos_residue=lastTarLoc=preVec=(xVec){0};
  T_next=T_lapsed=0;
  minSpeed=2;
  acc=1;
  axis_pul=0;
}

void MStp::StepperForceStop()
{
  
  blocks->clear();
  lastTarLoc=curPos_c;
  T_next=T_lapsed=0;
  axis_pul=axis_dir=0;
  delayRoundX=0;
  pre_indexes=0;
  tskrun_state=0;
  isMidPulTrig=true;
  _axis_collectpul1=_axis_collectpul2=0;
  accT=curT=0;
  curPos_mod=curPos_residue=preVec=(xVec){0};
}


// void MStp::AxisZeroing(uint32_t index)
// {



// }

std::string toStr(const xVec &vec)
{
  char buff[(MSTP_VEC_SIZE)*(10+2)];//format 3433, 43432 ....
  char* ptr=buff;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    ptr+=sprintf(ptr,"%d, ",vec.vec[i]);
  }
  string str(buff);
  return str;
}


float MStp::calcMajorSpeed(runBlock &rb)
{
  return rb.vcen;
}


void MStp::Delay(int interval,int intervalCount)
{
  
}


void MStp::VecAdd(xVec VECAdd,float speed,void* ctx)
{
  VecTo(vecAdd(lastTarLoc,VECAdd),speed,ctx);
}



void MStp::VecTo(xVec VECTo,float speed,void* ctx)
{
  __PRT__("=================\n");

  runBlock* head=blocks->getHead();
  if(head==NULL)
  {
    return;
  }


  runBlock &rb=*head;

  rb=(runBlock){
    .ctx=NULL,
    .from = lastTarLoc,
    .to=VECTo,
    .runvec = vecSub(VECTo,lastTarLoc),
    .posvec = (xVec){0},
    .steps=vecMachStepCount(VECTo,lastTarLoc),
    .cur_step=0,
    .JunctionNormCoeff=0,
    .JunctionNormMaxDiff=NAN,
    .isInDeAccState=false,
    .vcur=minSpeed,
    .vcen=speed,
    .vto=0,
    .vto_JunctionMax=0,
    .vto_StopMax=0
  };

  lastTarLoc=VECTo;




  // rb.vcen=calcMajorSpeed(rb);
  if(rb.steps==0)
  {
    return;
  }



  runBlock *tail = NULL;
  
  if(blocks->size()>0)
  {
    tail = blocks->getTail(blocks->size()-1);
  }
  if(tail!=NULL)
  {// you need to deal with the junction speed
    // tail->vec;
    // rb.vec;
    



    int coeffSt= Calc_JunctionNormCoeff(*tail,rb,rb.JunctionNormCoeff);
    if(coeffSt<0)
    {
      rb.JunctionNormCoeff=0;
    }

    rb.JunctionNormMaxDiff=NAN;
    if(rb.JunctionNormCoeff!=0)
    {
      float maxDiff=NAN;
      int retSt=Calc_JunctionNormMaxDiff(*tail,rb,rb.JunctionNormCoeff,maxDiff);
      if(retSt==0)
      {
        rb.JunctionNormMaxDiff=maxDiff;



        tail->vto_JunctionMax=junctionMaxSpeedJump/rb.JunctionNormMaxDiff;//max allowed speed change
        tail->vto_StopMax=tail->vto_JunctionMax;
        tail->vto=tail->vto_JunctionMax;


        if(tail->vto>rb.vcen)
        {
          tail->vto=rb.vcen;
        }



        rb.vcur=tail->vto*rb.JunctionNormCoeff;




        __PRT__("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n DIFF:",tail->vto_JunctionMax,tail->vto,rb.vcur);
        for(int k=0;k<MSTP_VEC_SIZE;k++)
        {
          float v1=(tail->vto*tail->runvec.vec[k]/tail->steps);
          float v2=(  rb.vcur*   rb.runvec.vec[k]/   rb.steps);
          float diff = v1-v2;
          __PRT__("(A(%f)-B(%f)=%04.2f )",v1,v2,diff);
        }
        __PRT__("\n");
















      }
      else
      {
        rb.vcur=tail->vto=0;
      }
    }
    else
    {
      rb.vcur=tail->vto=0;
    }



    __PRT__("maxJump:%f coeff:%f  maxDiff:%f\n",junctionMaxSpeedJump, rb.JunctionNormCoeff, rb.JunctionNormMaxDiff);

    __PRT__("pre vcen:%0.3f vto:%0.3f =>new vcur:%0.3f vcen:%0.3f\n",tail->vcen,tail->vto,rb.vcur,rb.vcen);



    // rb.vcur=
    // tail->vto=0;//cosinSim*rb.vcen;
    







    // Serial.printf("tail vcur:%f  vcen:%f  vto:%f    rb:vcur:%f  vcen:%f  vto:%f   \n",tail->vcur,tail->vcen,tail->vto,   rb.vcur,rb.vcen,rb.vto );





  }
  else
  {
    // rb.vcur=100;
    // T_next=TICK2SEC_BASE/minSpeed;
    T_next=0;
  }

  blocks->pushHead();



  if(1)
  {

    /*
            ________
           /        |
          /         |
         /          |
        /           |
       /____________|


    */



    //look back
    int stoppingMargin=2;
    float Vdiff=0;
    //stopping look ahead planing

    runBlock* advblk = blocks->getTail(blocks->size()-1);
    for(int i=1;i<blocks->size();i++)
    {

      if(advblk->steps<stoppingMargin)
      {
        __PRT__("ACC skip\n");
        runBlock* curblk = blocks->getTail(blocks->size()-1-i);
        curblk->vto_StopMax=advblk->vto/advblk->JunctionNormCoeff;
        curblk->vto=curblk->vto_StopMax<curblk->vto_JunctionMax?curblk->vto_StopMax:curblk->vto_JunctionMax;
        advblk=curblk;
        continue;

      }
      int32_t minDistNeeded= DeAccDistNeeded(advblk->vcen,advblk->vto, -acc);


      // __PRT__("[%d]:blk.steps:%d v:%f,%f,%f minDistNeeded:%d  \n",i,blk.steps,blk.vcur,blk.vcen,blk.vto,minDistNeeded);
      if( ((int32_t)advblk->steps) <= (minDistNeeded+stoppingMargin) )
      {  
        float v1 = findV1(advblk->steps-stoppingMargin, -acc, advblk->vto);

        runBlock* curblk = blocks->getTail(blocks->size()-1-i);

        curblk->vto_StopMax=v1/advblk->JunctionNormCoeff;

        curblk->vto=curblk->vto_StopMax<curblk->vto_JunctionMax?curblk->vto_StopMax:curblk->vto_JunctionMax;
        // __PRT__("[%d]:v1:%f  ori_V1:%f Vdiff:%f\n",i,v1,ori_V1,Vdiff);
        advblk=curblk;
        continue;
      }
      else
      {
        break;
      }


    }


  }
}



void MStp::printBLKInfo()
{
  for(int i=0;i<blocks->size();i++)
  {
    runBlock& blk = *blocks->getTail(i);

    __PRT__("[%2d]:steps:%6d vcur:%05.2f vcen:%05.2f vto:%05.2f\n",i,blk.steps,blk.vcur,blk.vcen,blk.vto);
    __PRT__("     :%s\n",toStr(blk.runvec).c_str());

  }

}


void MStp::BlockRunStep(runBlock &rb)
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  int32_t vcur_int=rb.vcur;
  int32_t vcen_int=rb.vcen;
  int32_t vto_int=rb.vto;

  int32_t a1=acc;
  int32_t a2=-a1;
  int32_t D = (rb.steps-rb.cur_step);
  

  int32_t deAccReqD=DeAccDistNeeded(vcur_int, vto_int,a2);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  // __PRT__("vcur_int:%d vcen_int:%d vto_int:%d a1:%d D:%d  deAccReqD:%d  T_next:%d\n",vcur_int,vcen_int,vto_int,a1,D,deAccReqD,T_next);

  int deAccBuffer=D-deAccReqD;
  if(deAccBuffer<4)
  {
    rb.isInDeAccState=true;
    rb.vcur+=(float)a2*T_next/TICK2SEC_BASE;
    // __PRT__("a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",a2,T_next,TICK2SEC_BASE);
    if(rb.vcur<minSpeed)
    {
      rb.vcur=minSpeed;
    }
    if(rb.vcur<rb.vto)
    {
      rb.vcur=rb.vto;
    }
  // __PRT__("rb.vcur:%f a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",rb.vcur,a2,T_next,TICK2SEC_BASE);

  }
  else if(vcur_int<vcen_int)
  {
    float speedInc=(float)a1*T_next/TICK2SEC_BASE;
    if(speedInc>maxSpeedInc)
    {
      speedInc=maxSpeedInc;
    }
    rb.vcur+=speedInc;
    
    // __PRT__("a1:%d  T_next:%d  TICK2SEC_BASE:%d\n",a1,T_next,TICK2SEC_BASE);
    if(rb.vcur>vcen_int)
    {
      rb.vcur=rb.vcen;
    }
  }






  // rb.vcur=rb.vcen;

  // __PRT__("rb.vcur:%f  \n",rb.vcur);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // uint32_t step_scal=(rb.cur_step+1)<<PULSE_ROUND_SHIFT;//100x is for round  +1 for predict next position

  // __PRT__("=%03d/%03d==: \n",step_scal,rb.steps);
  uint32_t _axis_pul=0;
  uint32_t steps_scal=rb.steps;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    curPos_mod.vec[k]+=rb.posvec.vec[k];
    if(curPos_mod.vec[k]>=steps_scal)//a step forward
    {
      curPos_mod.vec[k]-=steps_scal;
      curPos_residue.vec[k]=rb.steps-curPos_mod.vec[k];
      _axis_pul|=1<<k;
    }
    else
    {
      curPos_residue.vec[k]=0;
    }

  }

  axis_pul=_axis_pul;



  float T = TICK2SEC_BASE/rb.vcur;
  this->T_next=(uint32_t)(T);

  delayRoundX+=T-T_next;
  if(delayRoundX>1)
  {
    delayRoundX-=1;
    T_next+=1;
  }


  this->T_lapsed+=T_next;
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
}



void MStp::blockPlayer()
{
  
  if(blocks->size()>0)
  {

    runBlock &blk=*blocks->getTail();
    float vcur= blk.vcur;
    curBlk=&blk;
    if(blk.cur_step==0)
    {


      uint32_t _axis_dir=0;
      for(int k=0;k<MSTP_VEC_SIZE;k++)
      {
        if(blk.runvec.vec[k]<0)
        {
          blk.posvec.vec[k]=-blk.runvec.vec[k];
          _axis_dir|=1<<k;
        }
        else
        {
          blk.posvec.vec[k]=blk.runvec.vec[k];
        }
      }

      __PRT__("start=vcur:%f=vcen:%f==vto:%f==\n",vcur,blk.vcen,blk.vto);

      axis_dir=_axis_dir;
      // T_next=0;

      BlockInitEffect(&blk,axis_dir);//flip direction

    }

    BlockRunStep(blk);

    blk.cur_step++;
    // std::this_thread::sleep_for(std::chrono::milliseconds(sysInfo.T_next));
    
    // BlockRunEffect();
    if(blk.cur_step==blk.steps)
    {
      float vcur= blk.vcur;
      memset(&curPos_mod,0,sizeof(curPos_mod));
      memset(&curPos_residue,0,sizeof(curPos_residue));
      
      __PRT__("EndSpeed:%f\n",vcur);
      blocks->consumeTail();
      runBlock *new_blk=blocks->getTail();
      if(new_blk!=NULL)
      {
        __PRT__("=new_vcur:%f===vcur:%f=vto:%f==\n",new_blk->vcur,vcur,blk.vto);
        new_blk->cur_step=0;
        new_blk->vcur= vcur*new_blk->JunctionNormCoeff;

        // rb.vcur=tail->vto*rb.JunctionNormCoeff;


        for(int k=0;k<MSTP_VEC_SIZE;k++)
        {
          if(new_blk->runvec.vec[k]<0)
          {
            new_blk->posvec.vec[k]=-new_blk->runvec.vec[k];
          }
          else
          {
            new_blk->posvec.vec[k]=new_blk->runvec.vec[k];
          }
        }



        // __PRT__("start=vcur:%f=vcen:%f==vto:%f==\n DIFF:",vcur,blk.vcen,blk.vto);


        __PRT__("start=blk.vcur:%f=nblk.vcur:%f\n DIFF:",blk.vcur,new_blk->vcur);
        for(int k=0;k<MSTP_VEC_SIZE;k++)
        {
          float v1=(blk.vcur*blk.posvec.vec[k]/blk.steps);
          float v2= (new_blk->vcur*new_blk->posvec.vec[k]/new_blk->steps);
          float diff = v1-v2;
          // __PRT__("%04.2f ",diff);

          __PRT__("(A(%f)-B(%f)=%04.2f )",v1,v2,diff);
        }
        __PRT__("\n");







      }
    }
    

    // __PRT__("\n\n\n");
  }
  else
  {
    BlockInitEffect(NULL,0);
    T_next=0;
    // T_lapsed=0;//empty action
    // cout << "This is the first thread "<< endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    axis_pul=0;

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      curPos_residue.vec[i]=0;

    }

  }



}



uint32_t MStp::findMidIdx(uint32_t from_idxes,uint32_t totSteps)
{
  uint32_t idxes=0;

  uint32_t midP=totSteps>>1;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    if((from_idxes&(1<<k))==0)continue;
    int resd=curPos_residue.vec[k];
    // __PRT__(">[%d]>%d\n",k,resd);
    if(resd!=0 && resd<=midP)
    {
      idxes|=1<<k;
    }
  }
  return idxes;
}


uint32_t MStp::findNearstPulseIdx(uint32_t *ret_minResidue,int *ret_restCount)
{
  int idxes=0;
  uint32_t minV=999999;
  
  int restCount=0;
  int hitCount=0;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    int resd=curPos_residue.vec[k];
    // printf(">>%d",resd);
    if(resd==0)continue;
    restCount++;
    if(minV>resd)
    {
      minV=resd;
      idxes=1<<k;
      hitCount=1;
    }
    else if(minV==resd)
    {
      idxes|=1<<k;
      hitCount++;
    }
  }

  restCount-=hitCount;
  
  if(ret_restCount)
  {
    *ret_restCount=restCount;
  }

  if(ret_minResidue)
  {
    *ret_minResidue=minV;
  }
  return idxes;
}
void MStp::delIdxResidue(uint32_t idxes)
{
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    if((idxes&(1<<k)))
    {
      curPos_residue.vec[k]=0;
    }
  }
}





uint32_t MStp::taskRun()
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    uint32_t sele=(1<<i);
    if(pre_indexes&sele)
    {
      if(axis_dir&sele)
      {
        curPos_c.vec[i]--;
      }
      else
      {
        curPos_c.vec[i]++;
      }
    }
  }
  BlockPulEffect(pre_indexes,axis_collectpul);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // delIdxResidue(pre_indexes);

  axis_collectpul=0;
  accT=curT;
  if(tskrun_state==0)
  {

    // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
    blockPlayer();
    pre_indexes=0;
    isMidPulTrig=false;
    accT=0;
    curT=0;
    axis_collectpul=0;
    if(T_next==0)
    {
      return 0;
    }
    // __PRT__(">>>>st0 T_next:%d\n",T_next);
    tskrun_state=1;
  }

  if(tskrun_state==1)
  {
    if(isMidPulTrig==false)
    {
      uint32_t idxes=findMidIdx(axis_pul,curBlk->steps);
      // Serial.printf("=curBlk->steps:%d==idxes:%s  resd:%d\n",curBlk->steps,int2bin(idxes,MSTP_VEC_SIZE),curPos_residue.vec[0]);
      if(idxes==0)
        IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=!PIN_DBG0_st);
      isMidPulTrig=true;
      axis_pul&=~idxes;//surpress current

      pre_indexes=idxes;
      axis_collectpul=_axis_collectpul1;
      _axis_collectpul1=pre_indexes;
      return T_next/2;
    }
    tskrun_state=0;

    pre_indexes=axis_pul;
    axis_collectpul=_axis_collectpul1;
    _axis_collectpul1=pre_indexes;
    return T_next/2;
  }
  
  if(tskrun_state==2)//run pulse
  {
    // printf(">>>>st1\n");
    


    uint32_t idxes;
    uint32_t mT;
    if(save_pre_indexes==0)
    {
      uint32_t minResidue;
      int restCount;
      idxes = findNearstPulseIdx(&minResidue,&restCount);
      mT=(minResidue*T_next)>>_PULSE_ROUND_SHIFT_;
    } 
    else
    {
      idxes=save_pre_indexes;
      save_pre_indexes=0;
      mT=save_mT;
      save_mT=0;
    }


    uint32_t Tdev4=T_next>>2;

    if( (mT<(Tdev4)) || (mT>(3*Tdev4)) )
    {
      _axis_collectpul1|=idxes;
    }
    else
    {
      _axis_collectpul2|=idxes;
    }



    // printf("acol1:%s ",int2bin(_axis_collectpul1,MSTP_VEC_SIZE));
    // printf("acol2:%s \n",int2bin(_axis_collectpul2,MSTP_VEC_SIZE));
    pre_indexes=idxes;

    uint32_t Tdev3=T_next/3;


    if(isMidPulTrig==false)
    {
      
      if(mT<(2*Tdev3))
      {
        if( mT<(Tdev3))
        {//let go without pull down


        }
        else
        {//we can reuse this section  _axis_collectpul1
          axis_collectpul=_axis_collectpul1;
          _axis_collectpul1=0;
          isMidPulTrig=true;
        } 
      }
      else
      {//insert a event just for pull down   _axis_collectpul1
        


        axis_collectpul=_axis_collectpul1;
        _axis_collectpul1=0;
        isMidPulTrig=true;

        save_pre_indexes=pre_indexes;
        save_mT=mT;
        pre_indexes=0;//SKIP THIS IDXES
        mT=T_next/2+1;//MIDDLE PULSE
      }
    }
    else
    {

      if(mT==T_next)//there must be a pulse matchs this (the)
      {//we can reuse this section  _axis_collectpul2
      
        axis_collectpul=_axis_collectpul2;
        _axis_collectpul2=0;
        tskrun_state=0;
      }
      else
      {//let go without pull down



      }
    }

    curT=mT;

    int delay=curT-accT;
    // Serial.printf(">mT:%d  accT:%d>   curT:%d  >delay:%d>\n",mT,accT,curT,delay);


    // if(delay<50)delay=200;
    // int debtStep=5;
    // if(mT==0)
    // {

    //   // printf("==============DEBT\n");
    //   delay+=debtStep;
    //   tskrun_adj_debt+=debtStep;
    // }
    // else if(tskrun_adj_debt && delay>(2*debtStep))//pay back
    // {
    //   // printf("==============DEBT pay back\n");
    //   mT-=debtStep;
    //   tskrun_adj_debt-=debtStep;
    // }

    return delay;
  }


  return 0;
}
