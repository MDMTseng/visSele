/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppersV2.hpp"
using namespace std;

#include "LOG.h"



#define print_E(c) print(c)
#define print_I(c) print(c)
#define print_D(c) //G_LOG(c)

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





xVec_f vecSub(xVec_f v1,xVec_f v2)
{
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]-=v2.vec[i];
  }
  return v1;
}

xVec_f vecAdd(xVec_f v1,xVec_f v2)
{
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]+=v2.vec[i];
  }
  return v1;
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

inline float SpeedFactor(float *vec,MSTP_axisSetup *axis_setup,int vecL,int *ret_idx,int *ret_vidx=NULL)
{
  float maxDist=0;
  float maxVDist=0;
  int vidx=-1;
  int idx=-1;
  for(int i=0;i<vecL;i++)
  {
    float dist = vec[i];
    if(dist<0)dist=-dist;
    float virtualDist=dist*axis_setup[i].V_Factor;
    
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
    float virtualDist=dist*axis_setup[i].V_Factor;
    
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
  float maxVDist= vec.vec[ref_axis_idx]*axis_setup[ref_axis_idx].V_Factor;
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


inline float SpeedFactor_onRefAxis(float *vec,MSTP_axisSetup *axis_setup,int vecL,int *ret_idx,int ref_axis_idx)
{
  float maxDist=0;
  float maxVDist= vec[ref_axis_idx]*axis_setup[ref_axis_idx].V_Factor;
  if(maxVDist<0)maxVDist=-maxVDist;
  int idx=-1;


  for(int i=0;i<vecL;i++)
  {
    int32_t dist = vec[i];
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


inline float SpeedCap(float *vec,MSTP_axisSetup *axis_setup,int vecL,int phy_main_axis,float main_axis_speed)
{
  float mainDist= vec[phy_main_axis];
  if(mainDist<0)mainDist=-mainDist;
  int idx=-1;

  float maxAllowed_MA_Speed=main_axis_speed;//on main axis
  for(int i=0;i<vecL;i++)
  {
    int32_t dist = vec[i];
    if(dist<0)dist=-dist;

    float cur_allowed_MA_Speed=axis_setup[i].V_Max*mainDist/dist;
    if(maxAllowed_MA_Speed>cur_allowed_MA_Speed )
    {
      maxAllowed_MA_Speed=cur_allowed_MA_Speed;
    }
  }

  return maxAllowed_MA_Speed;
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

    float cur_allowed_MA_Speed=axis_setup[i].V_Max*mainDist/dist;
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

/*
inline int Calc_JunctionNormCoeff(MSTP_SEG_PREFIX MSTP_segment *blkA,MSTP_SEG_PREFIX MSTP_segment *blkB,MSTP_axisSetup *axisInfo,float *ret_blkB_coeff1)
{
  if(ret_blkB_coeff1)
    *ret_blkB_coeff1=NAN;
  





  float BBsum=0;
  float ABsum=0;//basically a dot product

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    // float A=(float)preSeg->runvec.vec[i]/preSeg->steps;//normalize here, a bit slower
    // float B=(float)rb.runvec.vec[i]/rb.steps;

    float A=(float)blkA->vec.vec[i];//X
    float B=(float)blkB->vec.vec[i];//Y
    float BdivW=B/axisInfo[i].MaxVJump;//Y
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
/**/



inline int Calc_JunctionNormCoeff(float *v1,float dist1,float *v2,float dist2,MSTP_axisSetup *axisInfo,int vecLen,float *ret_blkB_coeff1)
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
  for(int i=0;i<vecLen;i++)
  {
    // float A=(float)preSeg->runvec.vec[i]/preSeg->steps;//normalize here, a bit slower
    // float B=(float)rb.runvec.vec[i]/rb.steps;

    float A=v1[i];//X
    float B=v2[i];//Y

    float BdivW=B/axisInfo[i].MaxVJump;//Y
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
  float coeff1 = (ABsum/dist1)/(0.0001+BBsum/dist2);
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
/*
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
    float A=(float)blk1->vec.vec[i];//pre extract steps
    float B=(float)blk2->vec.vec[i]*NormCoeff;


    float diff = (A-B)/axisInfo[i].MaxVJump;
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
/**/


inline int Calc_JunctionNormMaxDiff(float *v1,float dist1,float *v2,float dist2,MSTP_axisSetup *axisInfo,int vecLen,float blk2_coeff,float *ret_MaxDiff)
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

  float NormCoeff=dist1*blk2_coeff/dist2;


  float maxAbsDiff=0;
  // __PRT_D_("steps:%d %d    NormCoeff:%f\n",blk1->steps,blk2->steps,NormCoeff);

  // __PRT_D_("blk1.runvec:%s\n",toStr(blk1->runvec));
  // __PRT_D_("blk2.runvec:%s\n",toStr(blk2->runvec));

  for(int i=0;i<vecLen;i++)
  {
    float A=v1[i];//pre extract steps
    float B=v2[i]*NormCoeff;


    float diff = (A-B)/axisInfo[i].MaxVJump;
    if(diff<0)diff=-diff;
    // __PRT_D_("blk[%d]: %d,%d\n",i,blk1->runvec.vec[i],blk2->runvec.vec[i]);
    // __PRT_D_("maxJump[%d]: A:%f B:%f  diff:%f\n",i,
      // A,B, diff);
    if(maxAbsDiff<diff)maxAbsDiff=diff;
  }
  maxAbsDiff/=dist1;

  if(ret_MaxDiff)*ret_MaxDiff=maxAbsDiff;
  // __PRT_D_("maxAbsDiff:%f,%f\n",maxAbsDiff,*ret_MaxDiff);
  
  return 0;
}



void nextIntervalCalc( MSTP_segment *seg) 
{
 
}


StpGroup::StpGroup()
{

}

bool StpGroup::pushInPause(uint32_t pause_ms,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
{

  if(segs.space()==0)
  {
    return false;
  }

  MSTP_SEG_PREFIX MSTP_segment* hrb=segs.getHead();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  newSeg.type=MSTP_segment_type::seg_wait;
  newSeg.startCB=startCB;
  newSeg.endCB=endCB;
  newSeg.ctx=ctx;
  newSeg.Mdistance=
  newSeg.Edistance=
  newSeg.distanceEnd=pause_ms;
  newSeg.distanceStart=0;



  newSeg.vcur=0;


  segs.pushHead();
  return true;
}


bool StpGroup::pushInInstant(MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
{

  if(segs.space()==0)
  {
    return false;
  }

  MSTP_SEG_PREFIX MSTP_segment* hrb=segs.getHead();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  newSeg.type=MSTP_segment_type::seg_instant_act;
  newSeg.startCB=startCB;
  newSeg.endCB=endCB;
  newSeg.ctx=ctx;
  newSeg.Mdistance=
  newSeg.Edistance=
  newSeg.distanceEnd=-1;
  newSeg.distanceStart=0;
  newSeg.vcur=0;
  segs.pushHead();
  return true;
}


void MSTP_segment_Copy(MSTP_segment *dst,MSTP_segment *src,int locDim)
{
  
  auto sp=dst->sp;
  auto vec=dst->vec;
  auto ctx=dst->ctx;

  *dst=*src;//value copy it would override the pointer

  dst->sp=sp;//recover the pointer
  dst->vec=vec;
  dst->ctx=ctx;
  if(dst->sp!=NULL && src->sp!=NULL)//copy content if possible
    memcpy(dst->sp,src->sp,locDim*sizeof(*src->sp));
  if(dst->vec!=NULL && src->vec!=NULL)//copy content if possible
    memcpy(dst->vec,src->vec,locDim*sizeof(*src->vec));
}

bool StpGroup::pushInMoveVec(float* vec,MSTP_segment_extra_info *exinfo,int locDim,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
{
  if(segs.space()<2)
  {
    return false;
  }


  // char PrtBuff[100];


  MSTP_SEG_PREFIX MSTP_segment* hrb=segs.getHead();
  MSTP_segment *ahb=segs.getHead(-1);//get the ahead segment
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  MSTP_SEG_PREFIX MSTP_segment &aheadSeg=*ahb;


  newSeg.distanceStart=0;
  newSeg.Mdistance=ManhattanMagnitude(vec,locDim,&newSeg.main_axis_idx);
  newSeg.Edistance=EuclideanMagnitude(vec,locDim);
  newSeg.distanceEnd=newSeg.Edistance;
  
  if(newSeg.Edistance==0)
  {
    return true;
  }


  print_D((">>>"+to_string(__LINE__)+" dist:"+to_string(newSeg.Edistance)).c_str());

  newSeg.ctx=ctx;
  
  newSeg.type=MSTP_segment_type::seg_line;
  // vecAssign(newSeg.from,lastTarLoc);
  // vecAssign(newSeg.to,VECTo);


  // vecAssign(newSeg.runvec,vecSub(loc,lastTarLoc));

  // {
  //   uint32_t _axis_dir=0;
  //   xVec _vec_abs=newSeg.runvec;
  //   for(int k=0;k<MSTP_VEC_SIZE;k++)
  //   {
  //     auto vecgo=_vec_abs.vec[k];
  //     if(vecgo<0)
  //     {
  //       _vec_abs.vec[k]=-vecgo;
  //       _axis_dir|=1<<k;
  //     }
  //     else
  //     {
  //       _vec_abs.vec[k]=vecgo;
  //     }
  //   }
  //   newSeg.runvec_abs=_vec_abs;
  //   newSeg.dir_bit=_axis_dir;
  // }

  copyTo(newSeg.sp,getLatestLocation());
  copyTo(newSeg.vec,vec);
  newSeg.vcur=0;
  int main_idx=0;
  int main_vidx=0;
  float vfactor=1;
  if(exinfo==NULL || exinfo->speedOnAxisIdx==-1)
  {


    vfactor=SpeedFactor(vec,axisSetup,locDim,&main_idx,&main_vidx);
    newSeg.virtual_axis_idx=main_vidx;
  }
  else
  {

    vfactor=SpeedFactor_onRefAxis(vec,axisSetup,locDim,&main_idx,exinfo->speedOnAxisIdx);

    newSeg.virtual_axis_idx=exinfo->speedOnAxisIdx;
  }

  newSeg.vcen=SpeedCap(vec,axisSetup,locDim,main_idx,exinfo->speed*vfactor);


  print_D((
    ">>>"+to_string(__LINE__)+
    " vcen:"+to_string(newSeg.vcen)+
    " main_idx:"+to_string(main_idx)+
    " main_vidx:"+to_string(main_vidx)+
    " vfactor:"+to_string(vfactor)
  ).c_str());


  // newSeg.vcen=speed*vfactor;
  newSeg.main_axis_idx=main_idx;

  newSeg.vto=0;

  newSeg.JunctionNormCoeff=0;
  newSeg.JunctionNormMaxDiff=NAN;
  newSeg.vto_JunctionMax=0;


  {

    int acc_constrain_axis=-1;
    float maxK=0;
    for(int i=0;i<locDim;i++)
    {
      float K=vec[i]/axisSetup[i].A_Factor;
      if(K<0)K=-K;
      if(maxK<K)
      {
        maxK=K;
        acc_constrain_axis=i;
      }
    }
    float accWFactor=axisSetup[acc_constrain_axis].A_Factor*vec[main_idx]/vec[acc_constrain_axis];

    if(accWFactor<0)accWFactor=-accWFactor;
    float acc=exinfo->acc;
    float dea=exinfo->deacc;


    if(acc<0)acc=-acc;
    if(dea>0)dea=-dea;
    newSeg.acc=acc*accWFactor;
    newSeg.deacc=dea*accWFactor;
  }



  // __PRT_I_("\n");
  // __PRT_I_("==========NEW runvec[%s:%f,%f,%f]======idx: h:%d t:%d===\n",toStr(newSeg.runvec),newSeg.vcen,newSeg.acc,newSeg.deacc,segBufHeadIdx,segBufTailIdx);

  // 
  // timerAlarmDisable(timer);

  MSTP_SEG_PREFIX MSTP_segment *_preSeg = NULL;
  
  if(segs.size()>0)//get previous block to calc junction info
  {

    for(int i=1;;i++){
       _preSeg = segs.getHead(i);
       if(_preSeg==NULL)break;
       
        //__PRT_I_("preSeg->type:%d\n",preSeg->type);
       if(_preSeg->type==MSTP_segment_type::seg_wait )//if there is a wait it would NOT need to calc the junction speed(it stops)
       {
          _preSeg=NULL;
          break;
       }
       else if(_preSeg->type==MSTP_segment_type::seg_instant_act)
       {//skip this and try to load next
         continue;
       }
       else
       {
         break;
       }
    }
   
  }
  


  if(_preSeg!=NULL)
  {// you need to deal with the junction speed
    // preSeg->vec;
    // newSeg.vec;

    MSTP_segment &preSeg = *_preSeg;
    // newSeg;
    // aheadSeg;

    bool doLineJunction=true;

    if(exinfo->cornorR==exinfo->cornorR && exinfo->cornorR>0 &&segs.size()>1 )//arc the cornor
    do{//we need to add a arc segment in between two line segment

      doLineJunction=false;
      MSTP_segment aheadLineSeg=newSeg;//value copy
      MSTP_segment arcSeg=aheadSeg;




      // float M2ERatio=preSeg->Edistance/newSeg.Mdistance;

      // newSeg.sp;
      // newSeg.vec;



/*

                                                                                                                                      
                                                                                                                                      
                                            spDistRatio=0.8                                                                           
                                  -----------------|----|   
                            [P1]                 [SP2]                                                                                
                                  --------------------------------------                                                              
         ret_distance   /        /    /                                [P2]                                                             
                       /        /__--                                                                                                    
                      /        /     return ANGLE                                                                                                 
                     /        /                                                                                                       
   spDistRatio=0.8  _[SP0]   /                                                                                                        
                   /        /                                                                                                         
                  -        /                                                                                                          
                                                                                                                                      
                        [P0]                                            


*/


      typedef  xnVec_f<20> TVec;//assume 20dimension is enough... becasue the input dimentsion is not constant in child class
      TVec sp0;//spline control point
      TVec sp2;
      float distance_turnPt_cornorPt;



      {

        // Calculate the angle in radians
        float percent=exinfo->cornorR;
        float cornorR_mm=NAN;
        if(percent>1)
        {
          cornorR_mm=percent;
          percent=1;
        }


        float angleRad=NAN;
        {
          TVec p0;//newSeg.sp-preSeg->vec;
          TVec p2;//newSeg.sp+newSeg.vec;

          float avaLengthShrink=0.95;
          float presegDistRatioLeft=(1-preSeg.distanceStart/preSeg.Edistance)*avaLengthShrink;
          for(int i=0;i<locDim;i++)
          {
            p0.vec[i]=aheadLineSeg.sp[i]-preSeg.vec[i]*presegDistRatioLeft;

            
            p2.vec[i]=aheadLineSeg.sp[i]+aheadLineSeg.vec[i]*avaLengthShrink;


            // sprintf(PrtBuff,"[%d]:%f,%f,%f    ",i,p0.vec[i],aheadLineSeg.sp[i],p2.vec[i]);G_LOG(PrtBuff);
          }


          float preSegdist=preSeg.Edistance-preSeg.distanceStart;
          float newSegdist=aheadLineSeg.Edistance;
          float targetDist=newSegdist*percent;

          if(preSegdist<newSegdist)//new segment longer, percentage will affect on previous segment
          {
            percent=targetDist/preSegdist;

            if(percent>1)percent=1;
          }
          else//new segment shorter
          {
          }


        // percent/=presegDistRatioLeft;

          // sprintf(PrtBuff,"distanceStart:%f  Edistance:%f",preSeg.distanceStart,preSeg.Edistance);G_LOG(PrtBuff);


          angleRad = calcAngleAndOthers(
            p0.vec,
            aheadLineSeg.sp,
            p2.vec,
            locDim,
            percent,
            sp0.vec,
            sp2.vec,
            &distance_turnPt_cornorPt);


        }


        if(angleRad<5*M_PI/180 || angleRad>175*M_PI/180 )//too small to arc
        {

          doLineJunction=true;
          break;
        }

        // sprintf(PrtBuff,"percent:%f angleRad:%f  distance_turnPt_cornorPt:%f",percent,angleRad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);








        float arc_r_div_dist=NAN;
        float kappa=NAN;//=Ang2SplineKappa(angleRadians,&arc_r_div_dist);

        kappa=Ang2SplineKappa_PAP(angleRad);
        arc_r_div_dist=Ang2RDivDist_PAP(angleRad);

        double arc_r=arc_r_div_dist*distance_turnPt_cornorPt;

        if(cornorR_mm==cornorR_mm)
        {
          if(arc_r>cornorR_mm)
          {

            float shrinkRatio=cornorR_mm/arc_r;


            for(int i=0;i<locDim;i++)
            {
              float apexP=aheadLineSeg.sp[i];
              sp0.vec[i]=(sp0.vec[i]-apexP)*shrinkRatio+apexP;
              sp2.vec[i]=(sp2.vec[i]-apexP)*shrinkRatio+apexP;
            }





            arc_r=cornorR_mm;
            distance_turnPt_cornorPt=arc_r/arc_r_div_dist;
          }
          else
          {//arc_r cannot match cornorR_mm requirement, just let it be

          }
        }

        preSeg.distanceEnd=preSeg.Edistance-distance_turnPt_cornorPt;
        aheadLineSeg.distanceStart=distance_turnPt_cornorPt;
        // sprintf(PrtBuff,"arc_r:%f",arc_r);G_LOG(PrtBuff);
        arcSeg.Mdistance=
        arcSeg.Edistance=
        arcSeg.distanceEnd=arc_r*(M_PI-angleRad);
        arcSeg.distanceStart=0;


        // sprintf(PrtBuff,"arcSeg.distanceEnd:%f",arcSeg.distanceEnd);G_LOG(PrtBuff);
        
        float minAcc=(preSeg.acc<-aheadLineSeg.deacc)?preSeg.acc:-aheadLineSeg.deacc;
        float vmax=sqrt(arc_r*minAcc);//a=w^2*r= v^2/r => vmax=sqrt(r*a)
        if(vmax>aheadLineSeg.vcen)vmax=aheadLineSeg.vcen;
        // vmax=aheadLineSeg.vcen;



        // sprintf(PrtBuff,"vcen:%f",vmax);G_LOG(PrtBuff);
        arcSeg.vcen=vmax;//make sure the arc speed is not too high according to centripetal acceleration
        arcSeg.vcur=0;
        arcSeg.vto=0;
        
        arcSeg.acc=preSeg.acc;
        arcSeg.deacc=aheadLineSeg.deacc;

        arcSeg.JunctionNormCoeff=1;
        arcSeg.JunctionNormMaxDiff=0;
        arcSeg.vto_JunctionMax=999999;

        arcSeg.ctx=NULL;
        arcSeg.endCB=arcSeg.startCB=NULL;
        arcSeg.main_axis_idx=-1;
        arcSeg.type=MSTP_segment_type::seg_arc;

        preSeg.vto_JunctionMax=999999;



        // printf("arc_r_div_dist=%f arc_r:%f\n",arc_r_div_dist,arc_r);
        // printf("\n\n\n");


        TVec ctrlpt0;
        TVec ctrlpt2;
          
        vecLerp(ctrlpt0.vec,sp0.vec,aheadLineSeg.sp,locDim,kappa);
        vecLerp(ctrlpt2.vec,sp2.vec,aheadLineSeg.sp,locDim,kappa);



/*
                                       
                             /\                                          
                            /  \                                         
                           /    \                                        
                          /      \                                       
                         /        \                                      
                        /          \                                     
                       /            \                                    
                      /              \                                   
                     /                \                                  
                    /                  \                                 
                   /                    \                                
                  /                      \                               
                 /                        \                              
                /                          \                             
             +-+                           +-+                           
             +^+  control point1           +^+   control point2                        
             / [pt2 (ctrlpt0)]             \[pt3 (ctrlpt2)]        
            /         ----------------\        \                         
           /    -----/                 --\      \                        
          /   -/               ---\       --\    \                       
         / --/                     -->       ---\ \                      
        / /                                      -\\                     
       / /   >                               \     -\                    
      /-/   /                                 \     \\                   
     //   -/                                   \     \\                  
   +-+   /                                      v     +-+                
   +^+                                                +^+                
 [SP(sp0)]                                         [pt4(sp2)]
                                                                                                                                
*/


        vecAssign(arcSeg.sp,sp0.vec,locDim);
        vecAssign(arcSeg.aux_pt2,ctrlpt0.vec,locDim);
        vecAssign(arcSeg.aux_pt3,ctrlpt2.vec,locDim);
        vecAssign(arcSeg.aux_pt4,sp2.vec,locDim);
        vecSub(arcSeg.vec,sp2.vec,sp0.vec,locDim);


        aheadLineSeg.JunctionNormCoeff=1;
        aheadLineSeg.JunctionNormMaxDiff=0;
        aheadLineSeg.vto_JunctionMax=999999;
        
      }

      
      // preSeg->;


      segs.pushHead(arcSeg);//push twosegments
      segs.pushHead(aheadLineSeg);
    }
    while(0);

    if(doLineJunction)
    {
      
      float coeff1=NAN;
      int calcErr= Calc_JunctionNormCoeff(preSeg.vec,preSeg.Edistance,vec,newSeg.Edistance,axisSetup,locDim,&coeff1);
      if(calcErr<0)
      {
        newSeg.JunctionNormCoeff=0;
      }



      // __PRT_I_("====coeff:%f    coeffSt:%d==\n",coeff1,coeffSt);
      newSeg.JunctionNormMaxDiff=99999999;
      if(calcErr==0)
      {
        float maxDiff1=NAN;
        int retSt=0;
        retSt |= Calc_JunctionNormMaxDiff(preSeg.vec,preSeg.Edistance,vec,newSeg.Edistance,axisSetup,locDim,coeff1,&maxDiff1);
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

          if(newSeg.JunctionNormMaxDiff<0.0000001)
            newSeg.JunctionNormMaxDiff=0.0000001;//min diff cap to prevent value explosion





          //max allowed end speed of pre block, so that at junction the max speed jump is within the limit, this is fixed
          preSeg.vto_JunctionMax=1/newSeg.JunctionNormMaxDiff;

          //vcur is the current speed, for un-executed block it's the starting speed
          newSeg.vcur=0;//preSeg->vto_JunctionMax*newSeg.JunctionNormCoeff;


          // char PrtBuff[100];
          // sprintf(PrtBuff,"coeff:%.4f dMax:%.4f  jMax:%.4f vcur:%.4f",
          //   newSeg.JunctionNormCoeff,
          //   newSeg.JunctionNormMaxDiff,
          //   preSeg->vto_JunctionMax,
          //   newSeg.vcur
            
          //   );G_LOG(PrtBuff);
          // {
          //   float vto_JunctionMax=preSeg->vto_JunctionMax;
          //   float JunctionNormMaxDiff=newSeg.JunctionNormMaxDiff;
          //   float vcur=newSeg.vcur;
          //   float vcen=newSeg.vcen;
          //   //__PRT_I_("===JunctionMax:%f ndiff:%f vcur:%f  vcen:%f==\n",vto_JunctionMax,JunctionNormMaxDiff,vcur,vcen);
            
          // }
          if(false&&newSeg.vcur>newSeg.vcen)//check if the max initial speed is higher than target speed
          {
            newSeg.vcur=newSeg.vcen;//cap the speed

            preSeg.vto_JunctionMax=newSeg.vcur/newSeg.JunctionNormCoeff;//calc speed back to preSeg->vto

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
          


          // preSeg->vto=preSeg->vto_JunctionMax;


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
          newSeg.vcur=preSeg.vto=0;
        }
      }
      else
      {
        newSeg.vcur=preSeg.vto=0;
      }


        // sprintf(PrtBuff,"percent:%f angleRad:%f  distance_turnPt_cornorPt:%f",percent,angleRad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);
      // sprintf(PrtBuff,(">>>"+to_string(__LINE__)+ " acc,dea,dist:"+to_string(newSeg.acc)+","+to_string(newSeg.deacc)+","+to_string(newSeg.Edistance)).c_str());G_LOG(PrtBuff);

      print_D((">>>"+to_string(__LINE__)+ " C: JNC,JNMD,JM,vcur,ven,vto:"+
        to_string(newSeg.JunctionNormCoeff)+","+
        to_string(newSeg.JunctionNormMaxDiff)+","+
        to_string(newSeg.vto_JunctionMax)+","+
        to_string(newSeg.vcur)+","+
        to_string(newSeg.vcen)+","+
        to_string(newSeg.vto)).c_str());




      print_D((">>>"+to_string(__LINE__)+ " P: JNC,JNMD,JM,vcur,ven,vto:"+
        to_string(preSeg.JunctionNormCoeff)+","+
        to_string(preSeg.JunctionNormMaxDiff)+","+
        to_string(preSeg.vto_JunctionMax)+","+
        to_string(preSeg.vcur)+","+
        to_string(preSeg.vcen)+","+
        to_string(preSeg.vto)).c_str());
      segs.pushHead();
    }
    

  }
  else
  {
    // newSeg.vcur=100;
    // T_next=TICK2SEC_BASE/minSpeed;
    segs.pushHead();
  }



  print_D((">>>"+to_string(__LINE__) +" newLLoc:"+vec_to_string(getLatestLocation())).c_str());



  
  if(1)
  {

    /*


               preSeg |     curSeg
           
            ________      ________
           /        \    /        \
          /          \  /          \
         /            \/            \
        /             |              \
       /              |               \




       CASE1:  curSeg is too short(less than stoppingMargin)
            ________    
           /        \    
          /          \       /.....
         /            \____ /
        /             |    |
       /              |    |


      CASE2:  curSeg is not long enough to be able to de-accelerate from curSeg.vcen(v center max speed) to curSeg.vto
              so the preSeg need to reduce the vto speed, so curSeg.vcur is low enough to safely de-accelerate to curSeg.vto
                         
      example:curSeg.steps=4                    
              curSeg.vto is 0(stop), but without changing preSeg.vto. it's impossible

          
            _________V  preSeg.vto 
          /          | \   
        /            |   \  
      /              |     \ 
    /                |      |
  /                  |      |
                      <-----> 
                      curSeg.steps

            recalc preSeg.vto'(lower the vto value) so that curSeg can reach curSeg.vto in the end
            ______   
          /        \   
        /            \V  preSeg.vto'   
      /              | \ 
    /                |   \
  /                  |     \
                      <-----> 
                      curSeg.steps




    CASE3 :the curSeg has enough steps to de-accelerate to curSeg.vto, so change preSeg is not needed 
            _________V___  preSeg.vto 
          /          |    \   
        /            |      \  
      /              |        \ 
    /                |          \
  /                  |            \
    */



    //look back
    int stoppingMargin=0;
    float Vdiff=0;
    //look ahead planing, to reduce
    //{oldest blk}.....preSeg, curSeg, {newest blk}
    MSTP_SEG_PREFIX MSTP_segment* curSeg;
    MSTP_SEG_PREFIX MSTP_segment* preSeg = segs.getHead(1);

    print_D((">>>"+to_string(__LINE__) + "Size:"+to_string(segs.size())).c_str());
    for(int i=1;i<segs.size();i++)
    {//can only adjust vto
      curSeg = preSeg;
      preSeg = segs.getHead(1+i);
      //__PRT_I_("preSeg:%p type:%d\n",preSeg,preSeg->type);

      if(preSeg->type==MSTP_segment_type::seg_wait)
      {//the wait segment will hold the movement, so no need to adjust the speed
        preSeg=NULL;
        break;
      }
      else if(preSeg->type==MSTP_segment_type::seg_instant_act)
      {//skip this and try to load next
        preSeg=curSeg;
        continue;
      }

      // if(preSeg==NULL)break;
      int32_t curDeAccSteps=(int32_t)curSeg->distanceEnd-curSeg->distanceStart-stoppingMargin;




      float cur_vstart=NAN;
      if(curDeAccSteps<0)
      {//CASE 1
        //here is the steps that's too short so we don't do speed change, so vcur(v start)=vto
        __PRT_D_("ACC skip\n");
        // curSeg->vcur=curSeg->vto;
        cur_vstart=curSeg->vto;
      }
      else
      {
        //find minimum distance needed
        int32_t minDistNeeded= DeAccDistNeeded_f(curSeg->vcen,curSeg->vto, curSeg->deacc);


        print_D(("curDeAccSteps:"+to_string(curDeAccSteps)+ " minDistNeeded:"+to_string(minDistNeeded)+" curSeg->vto"+to_string(curSeg->vto)  ).c_str());

        cur_vstart = findV1(curDeAccSteps, curSeg->deacc, curSeg->vto);
        // if( curDeAccSteps <= minDistNeeded )
        // {
        //   cur_vstart = findV1(curDeAccSteps, curSeg->deacc, curSeg->vto);
        //   //CASE 2
        // }
        // else
        // {//CASE 3 the curSeg has enough steps to de-accelerate to curSeg.vto, so exit 

        //   //(curSeg)the steps is long enough to de acc from vcen to vto, 
        //   //so we don't need to change the speed vto of (preSeg)
        //   cur_vstart=curSeg->vto;
        //   break;
        // }
      }


      if(cur_vstart!=cur_vstart)
      {
        //unset, ERROR
        break;
      }



      float preSeg_vto_max=(curSeg->JunctionNormCoeff<0.1)?0:cur_vstart/(curSeg->JunctionNormCoeff+0.01);
      float new_preSeg_vto=preSeg_vto_max<preSeg->vto_JunctionMax?preSeg_vto_max:preSeg->vto_JunctionMax;
      if(new_preSeg_vto>curSeg->vcen)
      {
        new_preSeg_vto=curSeg->vcen;
      }
      uint32_t curAddr=(0xFFF&(uint32_t)curSeg);
      uint32_t preAddr=(0xFFF&(uint32_t)preSeg);
// sprintf(PrtBuff,"percent:%f angleRad:%f  distance_turnPt_cornorPt:%f",percent,angleRad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);
      print_D(("cur.t:"+ to_string(curSeg->type)+" cur_vstart:"+to_string(cur_vstart)+ " preJunM:"+to_string(preSeg->vto_JunctionMax)+ " curJCoeff:"+to_string(curSeg->JunctionNormCoeff) + " curAddr:"+to_string(curAddr)).c_str());
      // print_D(("sp[0]:"+ to_string(preSeg->sp[0]+preSeg->vec[0])+" new_preSeg_vto:"+to_string(new_preSeg_vto)).c_str());

      print_D((" pre.t:"+ to_string(preSeg->type)+" pre_vto:"+to_string(preSeg->vto)+" new_pre_vto:"+to_string(new_preSeg_vto) + " preAddr:"+to_string(preAddr)).c_str());
      if(preSeg->vto == new_preSeg_vto)
      {//if the preSeg vto is exactly the same ,then, following adjustment is not needed

        break;
      }
      preSeg->vto=new_preSeg_vto;
      // __PRT_D_("[%d]:v1:%f  ori_V1:%f Vdiff:%f\n",i,v1,ori_V1,Vdiff);




    }


    

  }
  print_D("FINISH...");
  
  // timerAlarmEnable(timer);
  // 
  return true;

}












MSTP_segment* StpGroup::segAdvance(float &T)
{


  
  MSTP_segment* trb=segs.getTail();
  if(trb==NULL)
  {
    adv_info.inInDAcc=false;
    return NULL;
  }
  //might be a new segment
  if(adv_info.dstanceWent==0)
  {
    if(trb->startCB)trb->startCB(trb,&adv_info);
    trb->startCB=NULL;
  }


RELOAD:


  if(adv_info.dstanceWent==trb->distanceEnd)//previous seg finished. Load new segment, the endCB should be called previously, so no need to call it again
  {
    float trb_BK_vcur=trb->vcur;

   //if(trb->distance==0||trb->distance==-1)//unless the distance is 0
    {
      if(trb->endCB)trb->endCB(trb,&adv_info);
      trb->endCB=NULL;
    }
    segs.consumeTail();
    adv_info.dstanceWent=0;
    adv_info.inInDAcc=false;
    trb=segs.getTail();
    if(trb==NULL)
    {
      return NULL;
    }

    adv_info.dstanceWent=trb->distanceStart;
    //new segment
    switch(trb->type)
    {
      case MSTP_segment_type::seg_arc :
      case MSTP_segment_type::seg_line :
        trb->vcur=trb_BK_vcur*trb->JunctionNormCoeff;
      break;
      case MSTP_segment_type::seg_wait :
        trb->vcur=0;
      break;
      case MSTP_segment_type::seg_instant_act ://keep the speed
        trb->vcur=trb_BK_vcur;
      break;
    }

    if(trb->startCB)trb->startCB(trb,&adv_info);
  }


  switch(trb->type)
  {
    case MSTP_segment_type::seg_arc :
    case MSTP_segment_type::seg_line :
    {
      
      auto status=segAdvance(T,trb,&adv_info);


      if(status==MSTP_segment_adv_state::FINISH)
      {
        if(trb->endCB)trb->endCB(trb,&adv_info);
        trb->endCB=NULL;
      }
      return trb;

    }
    case MSTP_segment_type::seg_wait :
    {
      int distW = adv_info.dstanceWent;
      distW++;
      if(distW>=(int)(trb->distanceEnd))
      {
        adv_info.dstanceWent=trb->distanceEnd;
        if(trb->endCB)trb->endCB(trb,&adv_info);
        trb->endCB=NULL;
      }
      else
      {
        adv_info.dstanceWent=distW;
      }
      return trb;

    }
    case MSTP_segment_type::seg_instant_act ://keep the speed
    {
      adv_info.dstanceWent=trb->distanceEnd;
      if(trb->endCB)trb->endCB(trb,&adv_info);

      trb->endCB=NULL;
      goto RELOAD;
    }
  }


  return NULL;
}





StpGroup::MSTP_segment_adv_state StpGroup::segAdvance(float &T,MSTP_segment* trb,MSTP_segment_adv_info *info)
{
  if(trb==NULL||info==NULL)
  {
    return StpGroup::MSTP_segment_adv_state::ERROR;
  }



  if(info->dstanceWent==trb->distanceEnd)
  {
    return StpGroup::MSTP_segment_adv_state::FINISH;
  }


  MSTP_SEG_PREFIX MSTP_segment &curSeg=*trb;

  if(curSeg.type!=MSTP_segment_type::seg_line && curSeg.type!=MSTP_segment_type::seg_arc)
  {
    return StpGroup::MSTP_segment_adv_state::ERROR_TYPE_NOT_SUPPORT;
  }

  float acc=curSeg.acc;
  float dea=curSeg.deacc;

  float upperBoundDEA=dea*info->deaWeagle;
  float normalDEA=dea;
  float vto=curSeg.vto;
  float vcur=curSeg.vcur;
  float vcen=curSeg.vcen;




  float vto_sq=vto*vto;
  float vtoSQ_sub_vcurSQ_div2=(vto_sq-vcur*vcur)/2;
  float distance_require=vtoSQ_sub_vcurSQ_div2/normalDEA;

  float distanceLeft=curSeg.distanceEnd-info->dstanceWent;
  float distanceDiff=distance_require-distanceLeft;

  float aug_dea=normalDEA;


  float eqvcur=vcur;

  // float eqLeft=undefined

  if(info->inInDAcc||distanceDiff>=-info->magicSpace)
  {
    info->inInDAcc=true;
    float eqLeft=distanceLeft-info->magicSpace;
    if(eqLeft<0.00001)eqLeft=0.00001;
    float eqDEA=vtoSQ_sub_vcurSQ_div2/eqLeft;
    if(eqDEA<upperBoundDEA)eqDEA=upperBoundDEA;

    float vDiff=eqDEA*T;
    vcur+=vDiff;
    if(vcur<vto)
    {
      vcur=vto;
    }

    aug_dea=eqDEA;


    eqvcur=vcur;
    float minSpeed=-upperBoundDEA*T;

    if(eqvcur<minSpeed)eqvcur=minSpeed;

  }
  else
  {
    if(info->dstanceWent!=0||vcur==0)
      vcur+=acc*T;
    {


      {//prevent high acc casued speed over shoot
        float _distance_require=(vto_sq-vcur*vcur)/2/(dea);

        float goDistance=vcur*T;

        float new_d_left=distanceLeft-goDistance;
        if(_distance_require>new_d_left)
        {
          if(new_d_left<0) new_d_left=0;
          vcur=sqrt(vto_sq-new_d_left*2*dea);
          info->inInDAcc=true;
        }
      }
    }

    if(vcur>vcen)vcur=vcen;
    eqvcur=vcur;
  } 


  curSeg.vcur=vcur;
  
  info->dstanceWent+=eqvcur*T;
  if(info->dstanceWent>=curSeg.distanceEnd)
  {
    
    float overD=info->dstanceWent-curSeg.distanceEnd;
    float overT=overD/eqvcur;
    T-=overT;
    info->dstanceWent=curSeg.distanceEnd;
    
    return StpGroup::MSTP_segment_adv_state::FINISH;
  }


  return StpGroup::MSTP_segment_adv_state::ADV;//percentage=dstanceWent/curSeg.distance;
}