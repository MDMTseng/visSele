/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
// #include <thread>
#include "MSteppersV2.h"
using namespace std;

#include "LOG.h"

#define print_E(c) print(c)
#define print_I(c) print(c)
#define print_D(c) //G_LOG(c)



//StpGroupType is chaild class of StpGroup
template<typename StpGroupType>
static void cubicBezier_comp(float* dst, typename StpGroupType::MSTP_segment *seg,float dstanceWent) {
  float ratio = dstanceWent / (seg->Edistance);
  float cb_coeff[4];
  cubicBezier_TCoeff4(ratio, cb_coeff);

  cubicBezier_Vec(dst, seg->sp.vec, seg->aux_pt2.vec, seg->aux_pt3.vec, seg->aux_pt4.vec, StpGroupType::vec_dim, cb_coeff);
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


inline float MaxAllowedSpeed(float *vec,MSTP_axisSetup *axis_setup,int vecL,float distance,int *ret_capIdx=NULL)
{

  if(ret_capIdx)
  {
    *ret_capIdx=-1;
  }
  float maxAllowedSpeed=9999999999;
  for(int i=0;i<vecL;i++)
  {
    float dist = vec[i];
    if(dist<0)dist=-dist;
    if(dist<0.01)continue;
    float allowedV=axis_setup[i].V_Max*distance/dist;

    if(maxAllowedSpeed>allowedV)
    {
      maxAllowedSpeed=allowedV;
      if(ret_capIdx)
      {
        *ret_capIdx=i;
      }
    }
  }




  return maxAllowedSpeed;
}




inline float MaxAllowedAcc(float *vec,MSTP_axisSetup *axis_setup,int vecL,float distance,int *ret_capIdx=NULL)
{

  if(ret_capIdx)
  {
    *ret_capIdx=-1;
  }
  float maxAllowedAcc=9999999999;
  for(int i=0;i<vecL;i++)
  {
    float dist = vec[i];
    if(dist<0)dist=-dist;
    if(dist<0.01)continue;
    float allowedA=axis_setup[i].A_Max*distance/dist;

    if(maxAllowedAcc>allowedA)
    {
      maxAllowedAcc=allowedA;
      if(ret_capIdx)
      {
        *ret_capIdx=i;
      }
    }
  }




  return maxAllowedAcc;
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


template<int VEC_DIM,int SEG_BUF_SIZE>
bool StpGroup<VEC_DIM,SEG_BUF_SIZE>::pushInPause(int id,uint32_t pause_ms,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
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
  // newSeg.Mdistance=
  newSeg.Edistance=
  newSeg.distanceEnd=pause_ms;
  newSeg.distanceStart=0;



  newSeg.vcur=0;
  newSeg.id=id*10;


  segs.pushHead();
  return true;
}


template<int VEC_DIM,int SEG_BUF_SIZE>
bool StpGroup<VEC_DIM,SEG_BUF_SIZE>::pushInInstant(int id,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
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
  // newSeg.Mdistance=
  newSeg.Edistance=
  newSeg.distanceEnd=-1;
  newSeg.distanceStart=0;
  newSeg.vcur=0;
  
  newSeg.id=id*10;
  segs.pushHead();
  return true;
}


template<int VEC_DIM,int SEG_BUF_SIZE>
void StpGroup<VEC_DIM, SEG_BUF_SIZE>::RESET()
{
  segs.clear();
  adv_info.dstanceWent=0;
  adv_info.inInDAcc = false;
}

// void MSTP_segment_Copy(MSTP_segment *dst,MSTP_segment *src,int VEC_DIM)
// {
  
//   auto sp=dst->sp;
//   auto vec=dst->vec;
//   auto ctx=dst->ctx;

//   *dst=*src;//value copy it would override the pointer

//   dst->sp=sp;//recover the pointer
//   dst->vec=vec;
//   dst->ctx=ctx;
//   if(dst->sp!=NULL && src->sp!=NULL)//copy content if possible
//     memcpy(dst->sp,src->sp,VEC_DIM*sizeof(*src->sp));
//   if(dst->vec!=NULL && src->vec!=NULL)//copy content if possible
//     memcpy(dst->vec,src->vec,VEC_DIM*sizeof(*src->vec));
// }

template<int VEC_DIM,int SEG_BUF_SIZE>
bool StpGroup<VEC_DIM,SEG_BUF_SIZE>::pushInMoveVec(int id,CMDVec vec,CMDVec startPoint,MSTP_segment_extra_info exinfo,MSTP_segment_CB startCB,MSTP_segment_CB endCB,void* ctx)
{
  if(segs.space()<2)
  {
    return false;
  }


  // char PrtBuff[100];


  MSTP_SEG_PREFIX MSTP_segment* hrb=segs.getHead();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  

  for(int i=0;i<vec_dim;i++)
  {
    vec.vec[i]*=axisSetup[i].P_PreMult;
  }

  newSeg.distanceStart=0;
  newSeg.Edistance=EuclideanMagnitude(vec.vec,VEC_DIM);
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
  newSeg.sp=startPoint;
  for(int i=0;i<VEC_DIM;i++)
  {
    newSeg.sp.vec[i]*=axisSetup[i].P_PreMult;
  }

  newSeg.vec=vec;
  // copyCMDVec(newSeg.vec,vec);
  newSeg.vcur=0;
  int main_idx=0;
  int main_vidx=0;
  float vfactor=1;
  vfactor=SpeedFactor(vec.vec,axisSetup,VEC_DIM,&main_idx,&main_vidx);
  newSeg.vmax=exinfo.speed*vfactor;

  
  float maxAllowedSpeed=MaxAllowedSpeed(vec.vec,axisSetup,VEC_DIM,newSeg.Edistance);
  
  if(newSeg.vmax>maxAllowedSpeed)//cap
  {
    newSeg.vmax=maxAllowedSpeed;
  }


  print_D((
    ">>>"+to_string(__LINE__)+
    " vmax:"+to_string(newSeg.vmax)+
    " main_idx:"+to_string(main_idx)+
    " main_vidx:"+to_string(main_vidx)+
    " vfactor:"+to_string(vfactor)
  ).c_str());


  newSeg.vto=0;

  newSeg.JunctionNormCoeff=0;
  newSeg.JunctionNormMaxDiff=NAN;
  newSeg.vto_JunctionMax=0;

  newSeg.id=id*10;
  {

    int acc_constrain_axis=-1;
    float maxK=0;
    for(int i=0;i<VEC_DIM;i++)
    {
      float K=vec.vec[i]/axisSetup[i].A_Factor;
      if(K<0)K=-K;
      if(maxK<K)
      {
        maxK=K;
        acc_constrain_axis=i;
      }
    }
    float accWFactor=axisSetup[acc_constrain_axis].A_Factor*newSeg.Edistance/vec.vec[acc_constrain_axis];

    if(accWFactor<0)accWFactor=-accWFactor;
    float acc=exinfo.acc;
    float dea=exinfo.deacc;


    if(acc<0)acc=-acc;
    if(dea>0)dea=-dea;
    
    float maxAllowedAcc=MaxAllowedAcc(vec.vec,axisSetup,VEC_DIM,newSeg.Edistance);

    newSeg.acc=acc*accWFactor;
    if(newSeg.acc>maxAllowedAcc)
    {
      newSeg.acc=maxAllowedAcc;
    }
    newSeg.deacc=dea*accWFactor;
    if(newSeg.deacc<-maxAllowedAcc)
    {
      newSeg.deacc=-maxAllowedAcc;
    }
  }



  // __PRT_I_("\n");
  // __PRT_I_("==========NEW runvec[%s:%f,%f,%f]======idx: h:%d t:%d===\n",toStr(newSeg.runvec),newSeg.vmax,newSeg.acc,newSeg.deacc,segBufHeadIdx,segBufTailIdx);

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

    if(exinfo.cornorR==exinfo.cornorR && exinfo.cornorR>0 &&segs.size()>1 )//arc the cornor
    do{//we need to add a arc segment in between two line segment



      //the queue was
      //preSeg || newSeg
      //Where newSeg = head(0)
      //and to arc the corner we need to insert a arc segment and two line segment
      //preSeg || arcSeg | newSeg

      
      //where newSeg = head(0)
      //and arcSeg = head(-1) //the buffer even after the Q write head
      //note the buffer has the aux information 




      //But the newSeg we 



      doLineJunction=false;
      MSTP_segment *ahb=segs.getHead(-1);//get the ahead segment

      MSTP_SEG_PREFIX MSTP_segment &aheadSeg=*ahb;
      MSTP_segment arcSeg=aheadSeg;



      MSTP_segment aheadLineSeg=newSeg;//value copy
  




      // float M2ERatio=preSeg->Edistance/newSeg.Mdistance;

      // newSeg.sp;
      // newSeg.vec;



    /*                                                                                                                               
                                                  spDistRatio=0.8                                                                           
                                        -----------------|----|   
                                  [P1]                 [SP2]                                                                                
                                        --------------------------------------                                                              
              ret_distance    /        /    /                                [P2]                                                             
   (distance_turnPt_cornorPt)/        /__--                                                                                                    
                            /        /     return ANGLE                                                                                                 
                           /        /                                                                                                       
         spDistRatio=0.8  _[SP0]   /                                                                                                        
                         /        /                                                                                                         
                        -        /                                                                                                          
                                                                                                                                            
                              [P0]                                            


    */

      CMDVec sp0;//spline control point
      CMDVec sp2;
      float distance_turnPt_cornorPt;



      {

        // Calculate the angle in radians

        //if cornorR>1 then it's in mm
        //if cornorR 0~1 then it's in avaliable percentage
        float percent=exinfo.cornorR;
        float cornorR_mm=NAN;
        if(percent>1)
        {
          cornorR_mm=percent;
          percent=1;
        }


        float cornor_angle_rad=NAN;
        {
          CMDVec p0;//the earliest start point that we can use on preSeg
          CMDVec p2;//the latest   end point that we can use on aheadLineSeg

          float avaLengthShrink=0.95;//it's to make sure there is still a line segment


          float preSegdist=(preSeg.Edistance-preSeg.distanceStart)*avaLengthShrink;//dist(p0,cornor)
          float newSegdist=(aheadLineSeg.Edistance)*avaLengthShrink;//dist(cornor,p2)


          for(int i=0;i<VEC_DIM;i++)
          {
            p0.vec[i]=aheadLineSeg.sp.vec[i]-preSeg.vec.vec[i]*preSegdist/preSeg.Edistance;

            
            p2.vec[i]=aheadLineSeg.sp.vec[i]+aheadLineSeg.vec.vec[i]*avaLengthShrink;


            // sprintf(PrtBuff,"[%d]:%f,%f,%f    ",i,p0.vec[i],aheadLineSeg.sp[i],p2.vec[i]);G_LOG(PrtBuff);
          }

          //now we have p0->cornor(sp)->p2

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


          cornor_angle_rad = calcAngleAndOthers(
            p0.vec,
            aheadLineSeg.sp.vec,
            p2.vec,
            VEC_DIM,
            percent,
            sp0.vec,
            sp2.vec,
            &distance_turnPt_cornorPt);


        }

        const int skipAngleMargin=1;
        if(cornor_angle_rad<skipAngleMargin*M_PI/180 || cornor_angle_rad>(180-skipAngleMargin)*M_PI/180 )//too small to arc
        {

          doLineJunction=true;
          break;
        }

        // sprintf(PrtBuff,"percent:%f cornor_angle_rad:%f  distance_turnPt_cornorPt:%f",percent,cornor_angle_rad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);







        // float kappa=Ang2SplineKappa(cornor_angle_radians,&arc_r_div_dist);

        float kappa=Ang2SplineKappa_PAP(cornor_angle_rad);
        float arc_r_div_dist=Ang2RDivDist(cornor_angle_rad);

        double arc_r=arc_r_div_dist*distance_turnPt_cornorPt;

        if(cornorR_mm==cornorR_mm)
        {
          if(arc_r>cornorR_mm)
          {

            float shrinkRatio=cornorR_mm/arc_r;


            for(int i=0;i<VEC_DIM;i++)
            {
              float apexP=aheadLineSeg.sp.vec[i];
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
        // sprintf(PrtBuff,"arc_r:%f",arc_r);G_LOG(PrtBuff);
        // arcSeg.Mdistance=
        arcSeg.Edistance=
        arcSeg.distanceEnd=arc_r*(M_PI-cornor_angle_rad);
        arcSeg.distanceStart=0;


        // sprintf(PrtBuff,"arcSeg.distanceEnd:%f",arcSeg.distanceEnd);G_LOG(PrtBuff);
        
        arcSeg.acc=aheadLineSeg.acc<preSeg.acc?aheadLineSeg.acc:preSeg.acc;
        arcSeg.deacc=aheadLineSeg.deacc>preSeg.deacc?aheadLineSeg.deacc:preSeg.deacc;



        float minAcc=(arcSeg.acc<-arcSeg.deacc)?arcSeg.acc:-arcSeg.deacc;
        float minV=(preSeg.vmax<aheadLineSeg.vmax)?preSeg.vmax:aheadLineSeg.vmax;
        float vmax=sqrt(arc_r*minAcc);//a=w^2*r= v^2/r => vmax=sqrt(r*a) centripetal force
        if(vmax>minV)vmax=minV;
        // vmax=aheadLineSeg.vmax;



        // sprintf(PrtBuff,"vmax:%f",vmax);G_LOG(PrtBuff);
        arcSeg.vmax=vmax;//make sure the arc speed is not too high according to centripetal acceleration
        arcSeg.vcur=0;
        arcSeg.vto=0;
        

        arcSeg.acc=aheadLineSeg.acc<preSeg.acc?aheadLineSeg.acc:preSeg.acc;
        arcSeg.deacc=aheadLineSeg.deacc>preSeg.deacc?aheadLineSeg.deacc:preSeg.deacc;

        arcSeg.JunctionNormCoeff=1;
        arcSeg.JunctionNormMaxDiff=0;
        arcSeg.vto_JunctionMax=999999;

        arcSeg.ctx=NULL;
        arcSeg.endCB=arcSeg.startCB=NULL;
        arcSeg.type=MSTP_segment_type::seg_arc;



        // printf("arc_r_div_dist=%f arc_r:%f\n",arc_r_div_dist,arc_r);
        // printf("\n\n\n");


        CMDVec ctrlpt0=lerp(sp0,aheadLineSeg.sp,kappa);
        CMDVec ctrlpt2=lerp(sp2,aheadLineSeg.sp,kappa);
        



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
             / [pt2 (ctrlpt0)]                \[pt3 (ctrlpt2)]        
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

        arcSeg.sp=sp0;
        arcSeg.aux_pt2=ctrlpt0;
        arcSeg.aux_pt3=ctrlpt2;
        arcSeg.aux_pt4=sp2;
        arcSeg.vec=sub(sp2,sp0);

        preSeg.distanceEnd=preSeg.Edistance-distance_turnPt_cornorPt;
        preSeg.vto_JunctionMax=999999;


        aheadLineSeg.distanceStart=distance_turnPt_cornorPt;
        aheadLineSeg.JunctionNormCoeff=1;
        aheadLineSeg.JunctionNormMaxDiff=0;
        aheadLineSeg.vto_JunctionMax=999999;
        
      }

      
      // preSeg->;
      arcSeg.id=id*10-1;
      aheadLineSeg.id=id*10;

      segs.pushHead(arcSeg);//push twosegments
      segs.pushHead(aheadLineSeg);
    }
    while(0);

    if(doLineJunction)
    {
      
      float coeff1=NAN;
      int calcErr= Calc_JunctionNormCoeff(preSeg.vec.vec,preSeg.Edistance,vec.vec,newSeg.Edistance,axisSetup,VEC_DIM,&coeff1);
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
        retSt |= Calc_JunctionNormMaxDiff(preSeg.vec.vec,preSeg.Edistance,vec.vec,newSeg.Edistance,axisSetup,VEC_DIM,coeff1,&maxDiff1);
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
          //   float vmax=newSeg.vmax;
          //   //__PRT_I_("===JunctionMax:%f ndiff:%f vcur:%f  vmax:%f==\n",vto_JunctionMax,JunctionNormMaxDiff,vcur,vmax);
            
          // }
          if(false&&newSeg.vcur>newSeg.vmax)//check if the max initial speed is higher than target speed
          {
            newSeg.vcur=newSeg.vmax;//cap the speed

            preSeg.vto_JunctionMax=newSeg.vcur/newSeg.JunctionNormCoeff;//calc speed back to preSeg->vto

            // {
            //   float vto_JunctionMax=preSeg->vto_JunctionMax;
            //   float vcur=newSeg.vcur;
            //   float vmax=newSeg.vmax;
            //   __PRT_I_("===JunctionMax:%f  vcur:%f  vmax:%f==\n",vto_JunctionMax,vcur,vmax);
              
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


        // sprintf(PrtBuff,"percent:%f cornor_angle_rad:%f  distance_turnPt_cornorPt:%f",percent,cornor_angle_rad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);
      // sprintf(PrtBuff,(">>>"+to_string(__LINE__)+ " acc,dea,dist:"+to_string(newSeg.acc)+","+to_string(newSeg.deacc)+","+to_string(newSeg.Edistance)).c_str());G_LOG(PrtBuff);

      print_D((">>>"+to_string(__LINE__)+ " C: JNC,JNMD,JM,vcur,ven,vto:"+
        to_string(newSeg.JunctionNormCoeff)+","+
        to_string(newSeg.JunctionNormMaxDiff)+","+
        to_string(newSeg.vto_JunctionMax)+","+
        to_string(newSeg.vcur)+","+
        to_string(newSeg.vmax)+","+
        to_string(newSeg.vto)).c_str());




      print_D((">>>"+to_string(__LINE__)+ " P: JNC,JNMD,JM,vcur,ven,vto:"+
        to_string(preSeg.JunctionNormCoeff)+","+
        to_string(preSeg.JunctionNormMaxDiff)+","+
        to_string(preSeg.vto_JunctionMax)+","+
        to_string(preSeg.vcur)+","+
        to_string(preSeg.vmax)+","+
        to_string(preSeg.vto)).c_str());
      newSeg.id=id*10;
      segs.pushHead();
    }
    

  }
  else
  {
    //No previous seg
    
    newSeg.id=id*10;
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


      CASE2:  curSeg is not long enough to be able to de-accelerate from curSeg.vmax(v center max speed) to curSeg.vto
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
        int32_t minDistNeeded= DeAccDistNeeded_f(curSeg->vmax,curSeg->vto, curSeg->deacc);


        print_D(("curDeAccSteps:"+to_string(curDeAccSteps)+ " minDistNeeded:"+to_string(minDistNeeded)+" curSeg->vto"+to_string(curSeg->vto)  ).c_str());

        cur_vstart = findV1(curDeAccSteps, curSeg->deacc, curSeg->vto);
        // if( curDeAccSteps <= minDistNeeded )
        // {
        //   cur_vstart = findV1(curDeAccSteps, curSeg->deacc, curSeg->vto);
        //   //CASE 2
        // }
        // else
        // {//CASE 3 the curSeg has enough steps to de-accelerate to curSeg.vto, so exit 

        //   //(curSeg)the steps is long enough to de acc from vmax to vto, 
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
      if(new_preSeg_vto>curSeg->vmax)
      {
        new_preSeg_vto=curSeg->vmax;
      }
      uint32_t curAddr=(0xFFF&(uint32_t)curSeg);
      uint32_t preAddr=(0xFFF&(uint32_t)preSeg);
// sprintf(PrtBuff,"percent:%f cornor_angle_rad:%f  distance_turnPt_cornorPt:%f",percent,cornor_angle_rad*180/3.14159,distance_turnPt_cornorPt);G_LOG(PrtBuff);
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












template<int VEC_DIM,int SEG_BUF_SIZE>  
typename StpGroup<VEC_DIM,SEG_BUF_SIZE>::MSTP_segment* StpGroup<VEC_DIM,SEG_BUF_SIZE>::segAdvance(float &T)
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





template<int VEC_DIM,int SEG_BUF_SIZE> 
typename StpGroup<VEC_DIM,SEG_BUF_SIZE>::MSTP_segment_adv_state StpGroup<VEC_DIM,SEG_BUF_SIZE >::segAdvance(float &T,MSTP_segment* trb,MSTP_segment_adv_info *info)
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
  float vmax=curSeg.vmax;




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

    if(vcur>vmax)vcur=vmax;
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


template<int VEC_DIM,int SEG_BUF_SIZE> 
bool StpGroup<VEC_DIM,SEG_BUF_SIZE>::getSegLocation(MSTP_segment* seg,CMDVec &retLoc)
{
  if(seg==NULL)return false;
  if(seg->type!=MSTP_segment_type::seg_line && seg->type!=MSTP_segment_type::seg_arc)return false;


  
  if(seg->type==MSTP_segment_type::seg_line)//linear interpolation
  {
    float ratio=adv_info.dstanceWent/(seg->Edistance);
    for(int i=0;i<VEC_DIM;i++)
    {
      retLoc.vec[i]=seg->vec.vec[i]*(ratio)+seg->sp.vec[i];
    }

  }
  else  if(seg->type==MSTP_segment_type::seg_arc)//cubic bezier interpolation
  {
    cubicBezier_comp<StpGroup<VEC_DIM,SEG_BUF_SIZE>>(retLoc.vec,seg,adv_info.dstanceWent);

  }


  for(int i=0;i<VEC_DIM;i++)
  {
    retLoc.vec[i]/=axisSetup[i].P_PreMult;
  }





  return true;
}


/*
  static CMDVec sub(CMDVec v1,CMDVec v2);
  static CMDVec add(CMDVec v1,CMDVec v2);
  static CMDVec lerp(CMDVec v1,CMDVec v2,float ratio);
  static float dotProduct(CMDVec v1,CMDVec v2);
  static float magnitude(CMDVec v1);
  static float distance(CMDVec v1,CMDVec v2);*/

template<int VEC_DIM,int SEG_BUF_SIZE>
typename StpGroup<VEC_DIM, SEG_BUF_SIZE>::CMDVec StpGroup<VEC_DIM, SEG_BUF_SIZE>::sub(CMDVec v1, CMDVec v2) {
  CMDVec result;
  for (int i = 0; i < VEC_DIM; i++) {
    result.vec[i] = v1.vec[i] - v2.vec[i];
  }
  
  return result;

}

template<int VEC_DIM,int SEG_BUF_SIZE>
typename StpGroup<VEC_DIM, SEG_BUF_SIZE>::CMDVec StpGroup<VEC_DIM, SEG_BUF_SIZE>::add(CMDVec v1, CMDVec v2) {
  CMDVec result;
  for (int i = 0; i < VEC_DIM; i++) {
    result.vec[i] = v1.vec[i] + v2.vec[i];
  }
  return result;
}


template<int N, int S>
typename StpGroup<N, S>::CMDVec StpGroup<N, S>::lerp(const CMDVec& a, const CMDVec& b, float t) {
    CMDVec result;
    for (int i = 0; i < N; i++) {
        result.vec[i] = a.vec[i] + (b.vec[i] - a.vec[i]) * t;
    }
    return result;
}


template<int VEC_DIM,int SEG_BUF_SIZE>
float StpGroup<VEC_DIM, SEG_BUF_SIZE>::dotProduct(CMDVec v1, CMDVec v2) {
  float result = 0;
  for (int i = 0; i < VEC_DIM; i++) {
    result += v1.vec[i] * v2.vec[i];
  }
  return result;
}

template<int VEC_DIM,int SEG_BUF_SIZE>
float StpGroup<VEC_DIM, SEG_BUF_SIZE>::magnitude(CMDVec v1) {
  float sum = 0;
  for (int i = 0; i < VEC_DIM; i++) {
    sum += v1.vec[i] * v1.vec[i];
  }
  return sqrt(sum);
}

template<int VEC_DIM,int SEG_BUF_SIZE>
float StpGroup<VEC_DIM, SEG_BUF_SIZE>::distance(CMDVec v1, CMDVec v2) {
  return magnitude(sub(v1, v2));
}
