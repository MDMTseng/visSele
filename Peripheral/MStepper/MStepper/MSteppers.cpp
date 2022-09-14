/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppers.hpp"
using namespace std;

#include "LOG.h"

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
char* toStr(const MSTP_SEG_PREFIX xVec &vec)
{
  static char buff[(MSTP_VEC_SIZE)*(10+2)];//format 3433, 43432 ....
  char* ptr=buff;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    ptr+=sprintf(ptr,"%d, ",vec.vec[i]);
  }
  return buff;
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

inline uint32_t vecMachStepCount(xVec vec,int *ret_idx=NULL)
{
  uint32_t maxDist=0;
  int idx=-1;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    int32_t dist = vec.vec[i];
    if(dist<0)dist=-dist;
    if(maxDist<dist)
    {
      idx=i;
      maxDist=dist;
    }
  }
  if(ret_idx)*ret_idx=idx;
  return maxDist;
}


inline uint32_t vecMachStepCount(xVec v1,xVec v2,int *ret_idx=NULL)
{
  return vecMachStepCount(vecSub(v1 ,v2),ret_idx);
}

inline void vecAssign(MSTP_SEG_PREFIX xVec &to,MSTP_SEG_PREFIX xVec from)
{
  
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    to.vec[i]=from.vec[i];
  }
}
inline void vecAssign_ref(MSTP_SEG_PREFIX xVec &to,MSTP_SEG_PREFIX xVec &from)
{
  
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    to.vec[i]=from.vec[i];
  }
}


inline float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1, float *ret_T2)
{

  float T1L=(VT-V1)/a1;
  float T2L=(V2-VT)/a2;
  float baseT=T1L+T2L;


  float tri1=(VT*VT-V1*V1)/(2*a1);
  float tri2=(V2*V2-VT*VT)/(2*a2);
  
  __PRT_D_("V1:%f, a1:%f, VT:%f, V2:%f, a2:%f, D:%f  tri1:%f tri2:%f\n",V1,a1,VT, V2,a2,D,tri1,tri2);

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
  // __PRT_D_("baseT:%f Tcut:%f   -restRect:%f\n",baseT,Tcut,-restRect);
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


inline float DeAccTimeNeeded2(float VT, float V2, float a2,float *ret_D)
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


inline float DeAccDistNeeded_f(float V1, float V2, float a2)
{
  return (V2*V2-V1*V1)/a2/2;
  // int32_t resx2=((V2*V2-VT*VT)<<2)/a2;
  // return (resx2>>3)+((resx2&(1<<2))?1:0);//do round
}


inline float accTo_DistanceNeeded(float Vc, float Vd, float ad, float *ret_Td=NULL)
{
  float T=(Vd-Vc)/ad;
  float D=(Vd*Vc)*T/2;

  if(ret_Td)*ret_Td=T;

  return D;
}


//See info in MStepper header for VirtualStep 
inline float SpeedFactor(xVec vec,MSTP_axisSetup *axis_setup,int *ret_idx,int *ret_vidx=NULL)
{
  float maxDist=0;
  float maxVDist=0;
  int vidx=-1;
  int idx=-1;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    int32_t dist = vec.vec[i];
    if(dist<0)dist=-dist;
    float virtualDist=dist*axis_setup[i].VirtualStep;
    
    if(maxVDist<virtualDist)
    {
      vidx=i;
      maxVDist=virtualDist;
    }
    
    if(maxDist<dist)
    {
      idx=i;
      maxDist=dist;
    }
  }
  if(ret_idx)*ret_idx=idx;
  if(ret_vidx)*ret_vidx=vidx;
  return maxDist/maxVDist;
}

inline float SpeedFactor_onRefAxis(xVec vec,MSTP_axisSetup *axis_setup,int *ret_idx,int ref_axis_idx)
{
  float maxDist=0;
  float maxVDist= vec.vec[ref_axis_idx]*axis_setup[ref_axis_idx].VirtualStep;
  if(maxVDist<0)maxVDist=-maxVDist;
  int idx=-1;


  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    int32_t dist = vec.vec[i];
    if(dist<0)dist=-dist;
    
    if(maxDist<dist)
    {
      idx=i;
      maxDist=dist;
    }
  }

  if(ret_idx)*ret_idx=idx;
  return maxDist/maxVDist;
}


inline float SpeedCap(xVec vec,MSTP_axisSetup *axis_setup,int phy_main_axis,float main_axis_speed)
{
  float mainDist= vec.vec[phy_main_axis];
  if(mainDist<0)mainDist=-mainDist;
  int idx=-1;

  float maxAllowed_MA_Speed=main_axis_speed;//on main axis
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    int32_t dist = vec.vec[i];
    if(dist<0)dist=-dist;

    float cur_allowed_MA_Speed=axis_setup[i].MaxSpeed*mainDist/dist;
    if(maxAllowed_MA_Speed>cur_allowed_MA_Speed )
    {
      maxAllowed_MA_Speed=cur_allowed_MA_Speed;
    }
  }

  return maxAllowed_MA_Speed;
}


/*

to calc junction speed
i for the axis index



 Xi =>   Yi  // speed change target

aXi => abYi // junction speed control with param a & b


target   a->inf  b->1


max the speed(Xi)+speed(Xi)



For following Xi and Yi is the(element wise) normalized vector( the max element is 1/-1)
(1,-6,-3,2) => (1/6,-1,-1/2,1/3)
which means if we have the speed of X  Vx then the a*Vx=Vy  OR Vx=Vy/a



TARGET=>
MAX_ALLOWED_SPEED_JUMP>=Max(|aXi-ab'Yi|);



b = argMin( Max(|Xi-bYi|) ) //find b to have the smallest Speed jump
      b
  = argMin( (Sigma((Xi-bYi)^2N))^1/2N ) // When N is big
      b         i
  ~=argMin( (Sigma((Xi-bYi)^2))^1/2 ) //approximation N = 1 is easier to solve with good enough result
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
inline int Calc_JunctionNormCoeff(MSTP_SEG_PREFIX MSTP_segment *blkA,MSTP_SEG_PREFIX MSTP_segment *blkB,MSTP_axisSetup *axisInfo,float *ret_blkB_coeff1)
{
  if(ret_blkB_coeff1)
    *ret_blkB_coeff1=NAN;
  
/*


100      4032
466      466
4032     100
-9       -9
-100     -100
=> normalize(max ele to 1) =>
0.0248     1
0.1155     0.1155
1          0.0248
-0.002     -0.002
0.0248     0.0248

*/






  float BBsum=0;
  float ABsum=0;//basically a dot product

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    // float A=(float)preSeg->runvec.vec[i]/preSeg->steps;//normalize here, a bit slower
    // float B=(float)rb.runvec.vec[i]/rb.steps;

    float A=(float)blkA->runvec.vec[i];//X
    float B=(float)blkB->runvec.vec[i];//Y
    float BdivW=B/axisInfo[i].MaxSpeedJumpW;//Y
    //bigger MaxSpeedJumpW means less Weight(less important to take care of)
    // W*=W;
    // if(AB<0 && AB<-junctionMaxSpeedJump)
    // {//The speed from +to-(or reverse) and the difference is too huge, then set target speed to zero
    //   dotp=0;
    //   BB=1;
    //   break;
    // }
    // AAsum+=A*A;
    ABsum+=A*BdivW;
    BBsum+=B*BdivW;
  }
  if(ABsum<0)
  {
    // ret_blkB_coeff=NAN;
    // return -1;
    // ABsum=0;
    
    if(ret_blkB_coeff1)
      *ret_blkB_coeff1=0;
    return 0;
  }

  //__PRT_I_("ABB:%f %f\n",ABsum,BBsum);
  // AAsum/=blkA.steps;

  // rb.JunctionCoeff=dotp/BB;//normalize in the loop, a bit slower
  float coeff1 = (ABsum/blkA->steps)/(0.0001+BBsum/blkB->steps);
  // ret_blkB_coeff1=coeff1;//forward way Xi => bYi

  if(ret_blkB_coeff1)
    *ret_blkB_coeff1=coeff1;
  // // 
  // float coeff2 = (AAsum/blkA.steps)/(0.0001+ABsum/blkB.steps);
  // ret_blkB_coeff2=coeff2;//backward way forward way cXi => Yi : b=1/c
  // __PRT_D_("coeff:%f %f\n",coeff1,coeff2);
  return 0;
}

//normalize blk1 max speed as 1, blk2 max speed as blk2_coeff
//find the max abs of diff
//example:
//blk2.coeff=2
//blk1:(2, 2,4,8)  => (1/4,1/4,1/2,  1) =nbk1
//blk2:(2,-2,1,1)  => (  1, -1,1/2,1/2) *blk2.coeff => (2,-2,1,1) =ncbk2
//
//abs_diff=abs(nbk1-ncbk2): abs(-7/8, 9/4, -1/2, 0) => (7/8,9/4,1/2,0)
//max(abs_diff): 9/4 
//
//
//
inline int Calc_JunctionNormMaxDiff(MSTP_SEG_PREFIX MSTP_segment *blk1,MSTP_SEG_PREFIX MSTP_segment *blk2,float blk2_coeff,MSTP_axisSetup *axisInfo,float *ret_MaxDiff)
{


  if( blk2_coeff!=blk2_coeff)
  {
    if(ret_MaxDiff)*ret_MaxDiff=NAN;
    return -1;
  }
  //Xi/Yi is for i axis run step count
  //Xc/Yc is the running steps count
  //target=> Max(Xi/Xc-b*Yi/Yc)
  //      =  Max(Xi-Yi*b*Xc/Yc)/Xc
  //NormCoeff= b*Xc/Yc

  float NormCoeff=blk1->steps*blk2_coeff/blk2->steps;


  float maxAbsDiff=0;
  __PRT_D_("steps:%d %d    NormCoeff:%f\n",blk1->steps,blk2->steps,NormCoeff);

  __PRT_D_("blk1.runvec:%s\n",toStr(blk1->runvec));
  __PRT_D_("blk2.runvec:%s\n",toStr(blk2->runvec));

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    float A=(float)blk1->runvec.vec[i];//pre extract steps
    float B=(float)blk2->runvec.vec[i]*NormCoeff;


    float diff = (A-B)/axisInfo[i].MaxSpeedJumpW;
    if(diff<0)diff=-diff;
    __PRT_D_("blk[%d]: %d,%d\n",i,blk1->runvec.vec[i],blk2->runvec.vec[i]);
    __PRT_D_("maxJump[%d]: A:%f B:%f  diff:%f\n",i,
      A,B, diff);
    if(maxAbsDiff<diff)maxAbsDiff=diff;
  }
  maxAbsDiff/=(float)blk1->steps;

  if(ret_MaxDiff)*ret_MaxDiff=maxAbsDiff;
  __PRT_D_("maxAbsDiff:%f,%f\n",maxAbsDiff,*ret_MaxDiff);
  
  return 0;
}




  // bool isQueueEmpty();
  // void printBLKInfo();
  // void StepperForceStop();
  // bool VecTo(xVec VECTo,float speed,void* ctx=NULL);

  // uint32_t T_next=0;
  // void BlockRunStep(MSTP_SEG_PREFIX MSTP_segment *curSeg);

  // // virtual void BlockRunEffect(uint32_t idxes)=0;
  // virtual void BlockPulEffect(uint32_t idxes_T,uint32_t idxes_R)=0;
  // virtual void BlockDirEffect(uint32_t idxes)=0;
  // virtual void BlockInitEffect(MSTP_segment* blk)=0;

  // virtual void BlockEndEffect(MSTP_segment* blk)=0;
  
  // bool timerRunning=false;
  // virtual void stopTimer(){timerRunning=false;}
  // virtual void startTimer(){timerRunning=true;}

  // uint32_t taskRun();



MStp::MStp(MSTP_segment *buffer, int bufferL)
{

  segBuf=buffer;
  segBufL=bufferL;
  int segBufHeadIdx=0;
  int segBufTailIdx=0;
  minSpeed=100;
  main_junctionMaxSpeedJump=300;

  maxSpeedInc=minSpeed;
  
  TICK2SEC_BASE=10*1000*1000;
  main_acc=1000;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    limit1.vec[i]=-1000;
    limit2.vec[i]=1000;
  }




  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    axisInfo[i].AccW=1;
    axisInfo[i].MaxSpeedJumpW=1;
    axisInfo[i].VirtualStep=1;
    axisInfo[i].MaxSpeed=100;
  }
  // IO_SET_DBG(PIN_DBG0, OUTPUT);
  SystemClear();
  // motionFinishMutex=xSemaphoreCreateMutex();

}


void MStp::SystemClear()
{
  StepperForceStop();
  curPos_c=
  curPos_mod=
  lastTarLoc=(xVec){0};
  // curPos_residue=(xVec){0};
  T_next=0;
  minSpeed=2;
  main_acc=1000;
  doCheckHardLimit=true;
  fatalErrorCode=0;
}

void MStp::StepperForceStop()
{
  stopTimer();
  SegQ_Clear();
  p_runSeg=NULL;
  lastTarLoc=curPos_c;
  T_next=0;
  // axis_pul_1st=axis_pul_2nd=
  axis_pul=axis_dir=0;
  tskrun_state=0;
  curPos_mod=(xVec){0};
  // curPos_residue=(xVec){0};
  
}


void MStp::SegQ_Clear() MSTP_SEG_PREFIX
{
  segBufHeadIdx=segBufTailIdx;
}
bool MStp::SegQ_IsEmpty() MSTP_SEG_PREFIX
{
  return segBufHeadIdx==segBufTailIdx;
}

bool MStp::SegQ_IsFull() MSTP_SEG_PREFIX
{
  return ((segBufHeadIdx+1)%segBufL)==segBufTailIdx;
}

int MStp::SegQ_Size() MSTP_SEG_PREFIX
{
  return ((segBufHeadIdx-segBufTailIdx)+segBufL)%segBufL;
}

int MStp::SegQ_Capacity() MSTP_SEG_PREFIX
{
  return segBufL-1;
}

int MStp::SegQ_Space() MSTP_SEG_PREFIX
{
  return SegQ_Capacity()-SegQ_Size();
}


MSTP_SEG_PREFIX MSTP_segment* MStp::SegQ_Head(int idx) MSTP_SEG_PREFIX
{
  if(idx>SegQ_Size())return NULL;
  int rIdx=((segBufHeadIdx-idx)+segBufL)%segBufL;
  return segBuf+rIdx;
}
bool MStp::SegQ_Head_Push() MSTP_SEG_PREFIX
{
  if(endStopHitLock || SegQ_IsFull() )return false;
  int newIdx=(segBufHeadIdx+1)%segBufL;
  segBufHeadIdx=newIdx;
  return true;
}



MSTP_SEG_PREFIX MSTP_segment* MStp::SegQ_Tail(int idx) MSTP_SEG_PREFIX
{
  if(idx>=SegQ_Size())return NULL;
  int rIdx=((segBufTailIdx+idx)+segBufL)%segBufL;
  return segBuf+rIdx;
}

MSTP_SEG_PREFIX bool MStp::SegQ_Tail_Pop() MSTP_SEG_PREFIX
{
  if(SegQ_IsEmpty())return false;
  int rIdx=(segBufTailIdx+1)%segBufL;
  segBufTailIdx=rIdx;
  return true;
}

void MStp::_FatalError(int errorCode,const char* errorText)
{
  StepperForceStop();
  fatalErrorCode=errorCode;
  FatalError(errorCode,errorText);
}

bool MStp::AddWait(uint32_t period,int times, void* ctx,MSTP_segment_extra_info *exinfo)
{

  if(endStopHitLock || fatalErrorCode!=0)return false;

  if(SegQ_Space() <=2)
  {
    return false;
  }

  MSTP_SEG_PREFIX MSTP_segment* hrb=SegQ_Head();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  newSeg.ctx=ctx;
  newSeg.type=MSTP_segment_type::seg_wait;
  newSeg.vcen=0;
  newSeg.JunctionNormCoeff=0;
  newSeg.steps=times;
  newSeg.step_period=period;
  if(SegQ_Head_Push())
  {  
    startTimer();
    return true;
  }
  return false;
  //__PRT_I_("steps:%d step_period:%d\n",newSeg.steps,period);

}


bool MStp::VecAdd(xVec VECAdd,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
{
  return VecTo(vecAdd(lastTarLoc,VECAdd),speed,ctx,exinfo);
}



bool MStp::VecTo(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
{
  if(fatalErrorCode!=0)return false;

  if(SegQ_Space() <=2)
  {
    return false;
  }



  if(doCheckHardLimit)
  {
    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      if(VECTo.vec[i]<limit1.vec[i] ||VECTo.vec[i]>limit2.vec[i])
      {
        //ERROR
        _FatalError(1,"Hard limit hit");
        return false;
      }
    }
  }

  MSTP_SEG_PREFIX MSTP_segment* hrb=SegQ_Head();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  newSeg.steps=vecMachStepCount(VECTo,lastTarLoc,&newSeg.main_axis_idx);
  if(newSeg.steps==0)
  {
    return true;
  }





  newSeg.ctx=ctx;
  
  newSeg.type=MSTP_segment_type::seg_line;
  vecAssign(newSeg.from,lastTarLoc);
  vecAssign(newSeg.to,VECTo);
  vecAssign(newSeg.runvec,vecSub(VECTo,lastTarLoc));

  {
    uint32_t _axis_dir=0;
    xVec _vec_abs=newSeg.runvec;
    for(int k=0;k<MSTP_VEC_SIZE;k++)
    {
      auto vecgo=_vec_abs.vec[k];
      if(vecgo<0)
      {
        _vec_abs.vec[k]=-vecgo;
        _axis_dir|=1<<k;
      }
      else
      {
        _vec_abs.vec[k]=vecgo;
      }
    }
    newSeg.runvec_abs=_vec_abs;
    newSeg.dir_bit=_axis_dir;
  }

  newSeg.vcur=0;
  int main_idx=0;
  int main_vidx=0;
  float vfactor=1;
  if(exinfo==NULL || exinfo->speedOnAxisIdx==-1)
  {
    vfactor=SpeedFactor(newSeg.runvec,axisInfo,&main_idx,&main_vidx);
    newSeg.virtual_axis_idx=main_vidx;
  }
  else
  {
    vfactor=SpeedFactor_onRefAxis(newSeg.runvec,axisInfo,&main_idx,exinfo->speedOnAxisIdx);
    newSeg.virtual_axis_idx=exinfo->speedOnAxisIdx;
  }


  newSeg.vcen=SpeedCap(newSeg.runvec,axisInfo,main_idx,speed*vfactor);
  // newSeg.vcen=speed*vfactor;
  newSeg.main_axis_idx=main_idx;

  newSeg.vto=0;

  newSeg.cur_step=0;
  newSeg.JunctionNormCoeff=0;
  newSeg.JunctionNormMaxDiff=NAN;
  newSeg.vto_JunctionMax=0;


  {

    int acc_constrain_axis=-1;
    float maxK=0;
    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      float K=newSeg.runvec.vec[i]/axisInfo[i].AccW;
      if(K<0)K=-K;
      if(maxK<K)
      {
        maxK=K;
        acc_constrain_axis=i;
      }
    }
    float accWFactor=axisInfo[acc_constrain_axis].AccW*newSeg.runvec.vec[main_idx]/newSeg.runvec.vec[acc_constrain_axis];

    if(accWFactor<0)accWFactor=-accWFactor;
    float a=main_acc;
    float dea=-main_acc;
    if(exinfo!=NULL)
    {
      if(exinfo->acc==exinfo->acc)
        a=exinfo->acc;
        
      if(exinfo->deacc==exinfo->deacc)
        dea=exinfo->deacc;
    }

    if(a<0)a=-a;
    if(dea>0)dea=-dea;
    newSeg.acc=a*accWFactor;
    newSeg.deacc=dea*accWFactor;
  }



  // __PRT_I_("\n");
  // __PRT_I_("==========NEW runvec[%s:%f,%f,%f]======idx: h:%d t:%d===\n",toStr(newSeg.runvec),newSeg.vcen,newSeg.acc,newSeg.deacc,segBufHeadIdx,segBufTailIdx);
  lastTarLoc=VECTo;




  // 
  // timerAlarmDisable(timer);

  MSTP_SEG_PREFIX MSTP_segment *preSeg = NULL;
  
  if(SegQ_Size()>0)//get previous block to calc junction info
  {

    for(int i=1;;i++){
       preSeg = SegQ_Head(i);
       if(preSeg==NULL)break;
       
        //__PRT_I_("preSeg->type:%d\n",preSeg->type);
       if(preSeg->type==MSTP_segment_type::seg_wait )
       {
         if(preSeg->steps!=0)//means it's a real wait
         {
           preSeg=NULL;
           break;
         }
         else
         {//or look for next data
            continue;
         }
       }
       else
       {
         break;
       }
    }
   
  }
  
  if(preSeg!=NULL)
  {// you need to deal with the junction speed
    // preSeg->vec;
    // newSeg.vec;
    

    float coeff1=NAN;
    int coeffSt= Calc_JunctionNormCoeff(preSeg,&newSeg,axisInfo,&coeff1);
    if(coeffSt<0)
    {
      newSeg.JunctionNormCoeff=0;
    }

    // __PRT_I_("====coeff:%f    coeffSt:%d==\n",coeff1,coeffSt);
    newSeg.JunctionNormMaxDiff=99999999;
    if(coeffSt==0)
    {
      float maxDiff1=NAN;
      int retSt=0;
      retSt |= Calc_JunctionNormMaxDiff(preSeg,&newSeg,coeff1,axisInfo,&maxDiff1);
      // retSt |= Calc_JunctionNormMaxDiff(*preSeg,newSeg,coeff2,maxDiff2);



      /*
        (1+coeff)/maxDiff
        1 is for the pre speed factor and coeff is for post speed factor

        so 1+coeff would make sure the conbined speed is the larger the better

        To devide maxDiff is to normalize the max difference number
      */




      newSeg.JunctionNormCoeff=coeff1;
      newSeg.JunctionNormMaxDiff=maxDiff1;
      //__PRT_I_("====coeff:%f,%f diff:%f==\n",newSeg.JunctionNormCoeff,coeff1,newSeg.JunctionNormMaxDiff);
      if(retSt==0)
      {
        // newSeg.JunctionNormMaxDiff=maxDiff1;

        // __PRT_D_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n DIFF:",newSeg.JunctionNormMaxDiff,preSeg->vto,newSeg.vcur);

        if(newSeg.JunctionNormMaxDiff<0.01)
          newSeg.JunctionNormMaxDiff=0.01;//min diff cap to prevent value explosion


        //max allowed end speed of pre block, so that at junction the max speed jump is within the limit, this is fixed
        preSeg->vto_JunctionMax=main_junctionMaxSpeedJump/newSeg.JunctionNormMaxDiff;

        //vcur is the current speed, for un-executed block it's the starting speed
        newSeg.vcur=preSeg->vto_JunctionMax*newSeg.JunctionNormCoeff;

        {
          float vto_JunctionMax=preSeg->vto_JunctionMax;
          float JunctionNormMaxDiff=newSeg.JunctionNormMaxDiff;
          float vcur=newSeg.vcur;
          float vcen=newSeg.vcen;
          //__PRT_I_("===JunctionMax:%f ndiff:%f vcur:%f  vcen:%f==\n",vto_JunctionMax,JunctionNormMaxDiff,vcur,vcen);
          
        }
        if(newSeg.vcur>newSeg.vcen)//check if the max initial speed is higher than target speed
        {
          newSeg.vcur=newSeg.vcen;//cap the speed

          preSeg->vto_JunctionMax=newSeg.vcur/newSeg.JunctionNormCoeff;//calc speed back to preSeg->vto

          // {
          //   float vto_JunctionMax=preSeg->vto_JunctionMax;
          //   float vcur=newSeg.vcur;
          //   float vcen=newSeg.vcen;
          //   __PRT_I_("===JunctionMax:%f  vcur:%f  vcen:%f==\n",vto_JunctionMax,vcur,vcen);
            
          // }
          
        }
        else 
        {

        }
        
        preSeg->vto=preSeg->vto_JunctionMax;


        // {
        //   float vto_JunctionMax=preSeg->vto_JunctionMax;
        //   float vto=preSeg->vto;
        //   float vcur=newSeg.vcur;
        //   __PRT_I_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n",vto_JunctionMax,vto,vcur);
          
        // }

        // for(int k=0;k<MSTP_VEC_SIZE;k++)
        // {
        //   float v1=(preSeg->vto*preSeg->runvec.vec[k]/preSeg->steps);
        //   float v2=(  newSeg.vcur*   newSeg.runvec.vec[k]/   newSeg.steps);
        //   float diff = v1-v2;
        //   __PRT_I_("(A(%f)-B(%f)=%04.2f )\n",v1,v2,diff);
        // }



      }
      else
      {
        newSeg.vcur=preSeg->vto=0;
      }
    }
    else
    {
      newSeg.vcur=preSeg->vto=0;
    }



    __PRT_D_("maxJump:%f coeff:%f  maxDiff:%f\n",main_junctionMaxSpeedJump, newSeg.JunctionNormCoeff, newSeg.JunctionNormMaxDiff);


    // {
    //   float vto=preSeg->vto;
    //   float vcen=preSeg->vcen;
    //   float nvcur=newSeg.vcur;
    //   float nvcen=newSeg.vcen;
    //   __PRT_I_("pre vcen:%0.3f vto:%0.3f =>new vcur:%0.3f vcen:%0.3f\n",vcen,vto,nvcur,nvcen);
      
    // }


    // newSeg.vcur=
    // preSeg->vto=0;//cosinSim*newSeg.vcen;
    







    // Serial.printf("preSeg vcur:%f  vcen:%f  vto:%f    newSeg:vcur:%f  vcen:%f  vto:%f   \n",preSeg->vcur,preSeg->vcen,preSeg->vto,   newSeg.vcur,newSeg.vcen,newSeg.vto );





  }
  else
  {
    // newSeg.vcur=100;
    // T_next=TICK2SEC_BASE/minSpeed;
  }
  SegQ_Head_Push();

  
  if(1)
  {

    /*


               preSeg |     curblk
           
            ________      ________
           /        \    /        \
          /          \  /          \
         /            \/            \
        /             |              \
       /              |               \




       CASE1:  curblk is too short(less than stoppingMargin)
            ________    
           /        \    
          /          \       /.....
         /            \____ /
        /             |    |
       /              |    |


      CASE2:  curblk is not long enough to be able to de-accelerate from curblk.vcen(v center max speed) to curblk.vto
              so the preSeg need to reduce the vto speed, so curblk.vcur is low enough to safely de-accelerate to curblk.vto
                         
      example:curblk.steps=4                    
              curblk.vto is 0(stop), but without changing preSeg.vto. it's impossible

          
            _________V  preSeg.vto 
          /          | \   
        /            |   \  
      /              |     \ 
    /                |      |
  /                  |      |
                      <-----> 
                      curblk.steps

            recalc preSeg.vto'(lower the vto value) so that curblk can reach curblk.vto in the end
            ______   
          /        \   
        /            \V  preSeg.vto'   
      /              | \ 
    /                |   \
  /                  |     \
                      <-----> 
                      curblk.steps




    CASE3 :the curblk has enough steps to de-accelerate to curblk.vto, so change preSeg is not needed 
            _________V___  preSeg.vto 
          /          |    \   
        /            |      \  
      /              |        \ 
    /                |          \
  /                  |            \
    */



    //look back
    int stoppingMargin=5;
    float Vdiff=0;
    //look ahead planing, to reduce
    //{oldest blk}.....preSeg, curblk, {newest blk}
    MSTP_SEG_PREFIX MSTP_segment* curblk;
    MSTP_SEG_PREFIX MSTP_segment* preSeg = SegQ_Head(1);
    for(int i=1;i<SegQ_Size();i++)
    {//can only adjust vto
      curblk = preSeg;
      preSeg = SegQ_Head(1+i);
      //__PRT_I_("preSeg:%p type:%d\n",preSeg,preSeg->type);
      if(preSeg->type==MSTP_segment_type::seg_wait)
      {
        if(preSeg->steps!=0)//means it's a real wait no junction calc
        {
          preSeg=NULL;
          break;
        }
        else
        {//or look for next data
          preSeg=curblk;//make sure the curblk is still the same
          continue;
        }
       
      }

      // if(preSeg==NULL)break;
      int32_t curDeAccSteps=curblk->steps-stoppingMargin;

      float cur_vfrom=NAN;
      if(curDeAccSteps<0)
      {//CASE 1
        //here is the steps that's too short so we don't do speed change, so vcur(v start)=vto
        __PRT_D_("ACC skip\n");
        // curblk->vcur=curblk->vto;
        cur_vfrom=curblk->vto;
      }
      else
      {
        //find minimum distance needed
        int32_t minDistNeeded= DeAccDistNeeded_f(curblk->vcen,curblk->vto, curblk->deacc);
        if( curDeAccSteps <= minDistNeeded )
        {
          cur_vfrom = findV1(curDeAccSteps, curblk->deacc, curblk->vto);
          //CASE 2
        }
        else
        {//CASE 3 the curblk has enough steps to de-accelerate to curblk.vto, so exit 

          //(curblk)the steps is long enough to de acc from vcen to vto, 
          //so we don't need to change the speed vto of (preSeg)
          break;
        }
      }


      if(cur_vfrom!=cur_vfrom)
      {
        //unset, ERROR
        break;
      }



      float preSeg_vto_max=cur_vfrom/(curblk->JunctionNormCoeff+0.01);
      float new_preSeg_vto=preSeg_vto_max<preSeg->vto_JunctionMax?preSeg_vto_max:preSeg->vto_JunctionMax;
      {
        float vcur=preSeg->vcur;
        float vcen=preSeg->vcen;
        //__PRT_I_("[%d]:blk.steps:%d v:%f,%f,%f \n",i,preSeg->steps,vcur,vcen,new_preSeg_vto);
      }
      if(preSeg->vto == new_preSeg_vto)
      {//if the preSeg vto is exactly the same then the following adjustment is not nessasary
        break;
      }
      preSeg->vto=new_preSeg_vto;
      // __PRT_D_("[%d]:v1:%f  ori_V1:%f Vdiff:%f\n",i,v1,ori_V1,Vdiff);




    }


    

  }
  startTimer();
  
  // timerAlarmEnable(timer);
  // 
  return true;

}



void MStp::printSEGInfo()
{
  for(int i=0;i<SegQ_Size();i++)
  {
    MSTP_SEG_PREFIX MSTP_segment*seg=SegQ_Head(i+1);

    //__PRT_I_("[%2d]:type:%d steps:%6d v:%05.2f, %05.2f, %05.2f coeff:(%0.2f,%0.2f) \n",i,seg->type,seg->steps,
      // seg->vcur,seg->vcen,seg->vto,
      // seg->JunctionNormCoeff,
      // seg->JunctionNormMaxDiff);
    //__PRT_I_("     :%s\n",toStr(seg->runvec));

  }

}

// float Vacc(float preV,float a)
// {
//   float decision=preV*preV+4*a;
//   if(decision<0)return 0;
//   decision=sqrt(decision);
//   if(a<0)
//   {
//     return (preV-decision)/2;
//   }
//   return (preV+decision)/2;
// }

void nextIntervalCalc(MSTP_SEG_PREFIX MSTP_segment *seg,float minSpeed,float maxSpeedInc) MSTP_SEG_PREFIX
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  float a1=seg->acc;
  float a2=seg->deacc;
  int32_t D = (seg->steps-seg->cur_step);
  

  // int32_t deAccReqD=DeAccDistNeeded(seg->vcur,  seg->vto,(int32_t)a2);
  int32_t deAccReqD=(int32_t)DeAccDistNeeded_f(seg->vcur, seg->vto, a2);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  // __PRT_D_("vcur_int:%d vcen_int:%d vto_int:%d a1:%d D:%d  deAccReqD:%d  T_next:%d\n",vcur_int,vcen_int,vto_int,a1,D,deAccReqD,T_next);


  float vcur=seg->vcur;

  if(vcur<minSpeed)
  {
    vcur=minSpeed;
  }
  int deAccBuffer=D-deAccReqD;
  if(deAccBuffer<4)
  {
    // float alpha=-((deAccBuffer)/4);
    float speedInc=a2/vcur;//*(1+alpha);
    // float newV=seg->vcur+speedInc;
    // if(newV<minSpeed)
    // {
    //   speedInc=-maxSpeedInc;
    // }
    
    vcur+=speedInc;
    // __PRT_D_("a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",a2,T_next,TICK2SEC_BASE);

    if(vcur<seg->vto)
    {
      vcur=seg->vto;
    }

    if(vcur<0)
    {
      vcur=0;
    }

  // __PRT_D_("seg->vcur:%f a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",seg->vcur,a2,T_next,TICK2SEC_BASE);

  }
  else if(vcur<seg->vcen )
  {
    if(seg->cur_step>2)
    {
      float speedInc=a1/vcur;
      if(speedInc>maxSpeedInc)
      {
        speedInc=maxSpeedInc;
      }        
     vcur+=(speedInc);
    }
    
    // __PRT_D_("a1:%d  seg->vcur:%f\n",a1,seg->vcur);
    if(vcur>seg->vcen)
    {
      vcur=seg->vcen;
    }
  }
  seg->vcur=vcur;
}


void MStp::CalcNextStep(MSTP_SEG_PREFIX MSTP_segment *seg) MSTP_SEG_PREFIX
{
  // seg->vcur=seg->vcen;
  // {
  //   int space=3;
  //   if(seg->cur_step<space || seg->cur_step>=(seg->steps-space))
  //     __PRT_D_("cur_step:%d seg->vcur:%f  \n",seg->cur_step,seg->vcur);
  // }

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // uint32_t step_scal=(seg->cur_step+1)<<PULSE_ROUND_SHIFT;//100x is for round  +1 for predict next position

  // __PRT_D_("=%03d/%03d==: \n",step_scal,seg->steps);


  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);


  // uint32_t _axis_pul_1st=0;
  // uint32_t _axis_pul_2nd=0;
  uint32_t _axis_pul=0;
  uint32_t steps_main=seg->steps;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    //The algo to find when should pulse of each axis happen
    
    //exp: run to vec [10 3]
    
    //  main vec steps_main=10
    //  curr vec steps_curr= 3
    //  =>max steps 10
    //step count 1   2   3   4   5   6   7   8   9  10
    //v1[main]  10  20  30  40  50  60  70  80  90 100      each step cycle will have a main vector pulse, for sure~
    //v2         3   6   9  12  15  18  21  24  27  30
    //mod        x   x   x  @2   x   x  @1   x   x  @0      each @ means pulse hit, a puls should happen in this cycle
    
    //to find the exact location of the pulse in pulse cycle
    //we will do following calc
    //residue = steps_curr-mod
    //ratio   = residue/steps_curr
    //mod        x   x   x  @2   x   x  @1   x   x  @0
    //residue    x   x   x   1   x   x   2   x   x   3(exact last line)
    //ratio      x   x   x  1/3  x   x 2/3   x   x   1


    //a standard pulse (will happen in main vector pulse train)
    // |------|      ^
    // |      |______|

    
    // |    ^--------|
    // |____|        |
    //exp   ^ratio=1/3

    // |         ^---|
    // |_________|   |
    //exp        ^ratio=2/3

    // |             ^
    // |_____________|
    //exp            ^ratio=1

    
    // ------|      ^------|      ^------|      ^------|      ^------|      ^------|      ^------|      ^------|      ^------|      ^------|      ^
    //       |______|      |______|      |______|      |______|      |______|      |______|      |______|      |______|      |______|      |______|

    //Type P                                        ^---------|                                     ^---|                                         ^
    // _____________________________________________|         |_____________________________________|   |_________________________________________|
    //                                              1/3                                             2/3                                           1           
    //Type R (rounding)                                ^------|                                         ^------|                                  ^
    // ________________________________________________|      |_________________________________________|      |__________________________________|
    //                                              1/3=> 0.5                                       2/3=>1                                        1=>1

    //in perfect world the side vector sould happen as Type P shows
    //But to save comp power we will do Type R (align(round) to main vector transition edge (neg/pos edge))


    //WE DEFINE a step cycle as blow
    // XXXXXX|XXXXXX|   
    // "X" means unknown state(H/L), "|" means a transition might happens here
    // for main vector pulse the pulse will always happen at second "|"(transition)

    //NOTE: to reserve resources side vec pulse transition will force to happen on the "|" 
    //      (not totally accurate pulse position but save comp power& time and acceptable)






    int32_t vele_abs=vec_abs.vec[k];
    int32_t &curPos_mod_vec=curPos_mod.vec[k];
    curPos_mod_vec+=vele_abs;
    if(curPos_mod_vec>=steps_main)//a step forward
    {
      curPos_mod_vec-=steps_main;

      {

        //exp: main vec steps_main=10
        //     curr vec steps_curr= 3
        // 3|6|9|*12
        //        ^hit 12>10 mode =2 (definitely less than steps_curr)
        //        residue =steps_curr- mode = 3-2 =1  
        //        ratio   = residue/steps_curr = 1/3
        int32_t residue=vele_abs-curPos_mod_vec;
        //float ratio=residue/steps_curr;

        // main axis standard pulse
        // |------|      ^
        // |      |______|
        //exp     ^ratio=0.5

        //OR
        // |             ^
        // |_____________|
        //exp ^ratio=0.2
        //    1st    2nd    perspect to the main axis

        // if a pos edge is in the 1st section, then will push into _axis_pul_1st
        //the 1st secion pulse will perform within the middle edge
        //        ^------|
        // _______|      |
        //     1st   2nd    Note: since only main axis has the highest freq, the side axis will have time to bring down the line state
      
        //the 1st secion pulse will perform within the middle edge
        // |XXXXXX|       ^
        // |XXXXXX|_______|
        //     1st   2nd

        _axis_pul|=1<<k;
        // if((residue<<1)<=(vele_abs))//is ratio<=0.5 then it blong to the first section
        //   _axis_pul_1st|=1<<k;
        // else
        //   _axis_pul_2nd|=1<<k;
      }

    }
    else
    {
      // curPos_residue.vec[k]=0;
    }

  }

  // axis_pul_1st=_axis_pul_1st;
  // axis_pul_2nd=_axis_pul_2nd;
  axis_pul=_axis_pul;
}

// uint32_t MStp::findMidIdx(uint32_t from_idxes,uint32_t totSteps)
// {
//   uint32_t idxes=0;

//   uint32_t midP=totSteps>>1;
//   for(int k=0;k<MSTP_VEC_SIZE;k++)
//   {
//     if((from_idxes&(1<<k))==0)continue;
//     int resd=curPos_residue.vec[k];
//     // __PRT_D_(">[%d]>%d\n",k,resd);
//     if(resd!=0 && resd<=midP)
//     {
//       idxes|=1<<k;
//     }
//   }
//   return idxes;
// }


uint32_t MStp::taskRun()
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  
  //First, if there is an current runSeg=> 
  if(p_runSeg!=NULL)
  {
    switch(p_runSeg->type)//========Run with current segment
    {
      case MSTP_segment_type::seg_line:
        BlockPulEffect(0,0);
        // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
        // delIdxResidue(pre_indexes);

      break;
    }
  }

  //tskrun_state ==0 means () or ()
  float prevcur=0;
  if(tskrun_state==0)//
  {

    do
    {
      
      if(p_runSeg==NULL)//========if current runSeg==NULL Try to load new segment
      {
        T_next=0;
        // axis_pul_1st=axis_pul_2nd=0;
        axis_pul=0;
        p_runSeg=SegQ_Tail();
        if(p_runSeg==NULL)
        {
          stopTimer();
          // xSemaphoreGive(motionFinishMutex);
          BlockPinInfoUpdate(axis_dir,0,0);//keep pin update
          BlockPulEffect(0,0);//wait for spi input

          return 0;//EXIT, no new segment,go idle
        }
        else
        {
          p_runSeg->cur_step=0;
          BlockInitEffect(p_runSeg);
          curPos_mod=(xVec){0};
          // vecAssign(curPos_residue,vec0);
          // vecAssign(posvec,vec0);
          if(MSTP_segment_type::seg_line==p_runSeg->type)
          {
            if(axis_dir!=p_runSeg->dir_bit)
            {
            axis_dir=p_runSeg->dir_bit;

              // for(volatile int i=0;i<2000;i++)//HACK: to extend time before axis_dir do changing, to prevent dir change and step pulse (posedge) too close
              // {

              // }
            }
          }
        }
      }


      __PRT_D_("type:%d\n",p_runSeg->type);

      switch(p_runSeg->type)//========Run with current segment (short work)
      {
        case MSTP_segment_type::seg_line:
        {
          BlockPinInfoUpdate(axis_dir,0,0);
          auto *cp_vec=curPos_c.vec;
          for(int i=0;i<MSTP_VEC_SIZE;i++)//calc run psition
          {
            uint32_t sele=(1<<i);
            if(axis_pul&sele)
            {
              if(axis_dir&sele)
              {
                cp_vec[i]--;
              }
              else
              {
                cp_vec[i]++;
              }
            }
          }
        
        }

        break;
        case MSTP_segment_type::seg_wait :
        {
          __PRT_D_(">[%d~%f\n",p_runSeg->cur_step,p_runSeg->vcur);
          if(p_runSeg->cur_step==0)
          {
            p_runSeg->vcur= prevcur;//keeps the speed info
          }
          float vcur=p_runSeg->vcur;
          if(p_runSeg->step_period==0)
          {
            p_runSeg->cur_step=p_runSeg->steps;
          }
        }
        break;

      }




      if(p_runSeg->cur_step==p_runSeg->steps) //========segment reaches the end
      {

        prevcur= p_runSeg->vcur;
        __PRT_D_(">[%f\n",prevcur);
        BlockEndEffect(p_runSeg,SegQ_Tail(1));
        SegQ_Tail_Pop();
        p_runSeg=NULL;
        continue;
      } 



      switch(p_runSeg->type)//========Run with current segment
      {
        case MSTP_segment_type::seg_line:
          if(p_runSeg->cur_step==0)
          {
            
            __PRT_D_(">[%f\n",prevcur);
            p_runSeg->vcur= prevcur*p_runSeg->JunctionNormCoeff;
            axis_dir=p_runSeg->dir_bit;
            vec_abs=p_runSeg->runvec_abs;
          }
          

          CalcNextStep(p_runSeg);

          tskrun_state=1;
          p_runSeg->cur_step++;
          
        break;
        case MSTP_segment_type::seg_wait :
          if(p_runSeg->cur_step==0)
          {
            p_runSeg->vcur= prevcur;//keeps the speed info
          }
          __PRT_D_("blk_wait:::Go wait:%d\n",p_runSeg->step_period);
          p_runSeg->cur_step++;
          setTimer(p_runSeg->step_period);
          return p_runSeg->step_period;
        break;

      }

    }while(p_runSeg==NULL);




  }

  __PRT_D_("tskrun_state:%d isMidPulTrig:%d \n",tskrun_state,isMidPulTrig);
  if(tskrun_state==1)
  {
    if(p_runSeg==NULL)
    {
      tskrun_state=0;
      return 0;
    }
    __PRT_D_("cur_step:%d \n",p_runSeg->cur_step);
    // if(isMidPulTrig==false)
    // {
    //   if(p_runSeg->cur_step==1)
    //   {
    //     axis_pul_1st=0;
    //     _axis_collectpul1=(1<<MSTP_VEC_SIZE)-1;
    //   }
    //   isMidPulTrig=true;
    //   pre_indexes=axis_pul_1st;
    // }
    // else
    {
      // if(p_runSeg->cur_step==1)
      // {
      //   BlockDirEffect(axis_dir);
      // }
      tskrun_state=0;
    }


    
    {
      
      nextIntervalCalc(p_runSeg, minSpeed, maxSpeedInc);
      
      {
        uint32_t vcur=p_runSeg->vcur;
        if(vcur<(uint32_t)minSpeed)
        {
          vcur=(uint32_t)minSpeed;
        }

        uint32_t T = TICK2SEC_BASE/(uint32_t)vcur;
        p_runSeg->step_period=(T);
        T_next=p_runSeg->step_period;
      }
      
    }
    setTimer(T_next);
    BlockPulEffect(0,0);
    
    BlockPinInfoUpdate(axis_dir,axis_pul,0);

    return T_next;
  }
  
  return 0;
}
