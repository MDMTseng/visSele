/********************* Cubic Spline Interpolation **********************/
#include<iostream>
#include<math.h>
#include "RingBuf.hpp"
#include <initializer_list>
#include <thread>
#include "MSteppers.hpp"
using namespace std;


#define IO_WRITE_DBG(pinno,val) //digitalWrite(pinno,val)
#define IO_SET_DBG(pinno,val) //pinMode(pinno,val)


#define PIN_DBG0 18
inline char *int2bin(uint32_t a, int digits, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    for (int i = digits-1; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }
    buffer++;
    return buffer;
}
inline char* toStr(const MSTP_SEG_PREFIX xVec &vec)
{
  static char buff[(MSTP_VEC_SIZE)*(10+2)];//format 3433, 43432 ....
  char* ptr=buff;
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    ptr+=sprintf(ptr,"%d, ",vec.vec[i]);
  }
  return buff;
}

inline char *int2bin(uint32_t a, int digits) {
  static char binChar[64+1];
  binChar[sizeof(binChar)-1]='\0';
  return int2bin(a,digits, binChar, sizeof(binChar));
}

inline xVec vecSub(xVec v1,xVec v2)
{
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]-=v2.vec[i];
  }
  return v1;
}

inline xVec vecAdd(xVec v1,xVec v2)
{
  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    v1.vec[i]+=v2.vec[i];
  }
  return v1;
}

inline uint32_t vecMachStepCount(xVec vec)
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
inline uint32_t vecMachStepCount(xVec v1,xVec v2)
{
  return vecMachStepCount(vecSub(v1 ,v2));
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
  return ((int64_t)V2*V2-(int64_t)VT*VT)/a2/2;
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
inline int Calc_JunctionNormCoeff(MSTP_SEG_PREFIX MSTP_segment *blkA,MSTP_SEG_PREFIX MSTP_segment *blkB,float *ret_blkB_coeff1)
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

    // if(AB<0 && AB<-junctionMaxSpeedJump)
    // {//The speed from +to-(or reverse) and the difference is too huge, then set target speed to zero
    //   dotp=0;
    //   BB=1;
    //   break;
    // }
    // AAsum+=A*A;
    
    ABsum+=A*B;
    BBsum+=B*B;
  }
  if(ABsum<0)
  {
    // ret_blkB_coeff=NAN;
    // return -1;
    ABsum=0;
  }

  __PRT_I_("ABB:%f %f\n",ABsum,BBsum);
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
inline int Calc_JunctionNormMaxDiff(MSTP_SEG_PREFIX MSTP_segment *blk1,MSTP_SEG_PREFIX MSTP_segment *blk2,float blk2_coeff,float *ret_MaxDiff)
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
  __PRT_I_("steps:%d %d    NormCoeff:%f\n",blk1->steps,blk2->steps,NormCoeff);

  __PRT_I_("blk1.runvec:%s\n",toStr(blk1->runvec));
  __PRT_I_("blk2.runvec:%s\n",toStr(blk2->runvec));

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    float A=(float)blk1->runvec.vec[i];//pre extract steps
    float B=(float)blk2->runvec.vec[i]*NormCoeff;


    float diff = A-B;
    if(diff<0)diff=-diff;
    __PRT_D_("blk[%d]: %d,%d\n",i,blk1->runvec.vec[i],blk2->runvec.vec[i]);
    __PRT_I_("maxJump[%d]: A:%f B:%f  diff:%f\n",i,
      A,B, diff);
    if(maxAbsDiff<diff)maxAbsDiff=diff;
  }
  maxAbsDiff/=(float)blk1->steps;

  if(ret_MaxDiff)*ret_MaxDiff=maxAbsDiff;
  __PRT_I_("maxAbsDiff:%f,%f\n",maxAbsDiff,*ret_MaxDiff);
  
  return 0;
}




  void SystemClear();



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
  junctionMaxSpeedJump=300;

  maxSpeedInc=minSpeed;
  
  TICK2SEC_BASE=10*1000*1000;
  acc=1000
  IO_SET_DBG(PIN_DBG0, OUTPUT);
  SystemClear();
}


void MStp::SystemClear()
{
  
  curPos_c=curPos_mod=curPos_residue=lastTarLoc=(xVec){0};
  T_next=0;
  minSpeed=2;
  acc=1;
}

void MStp::StepperForceStop()
{
  
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
  return segBufL;
}

int MStp::SegQ_Space() MSTP_SEG_PREFIX
{
  return SegQ_Capacity()-SegQ_Size();
}


MSTP_SEG_PREFIX MSTP_segment* MStp::SegQ_Head(int idx) MSTP_SEG_PREFIX
{
  if(SegQ_IsFull())return NULL;
  int rIdx=((segBufHeadIdx-idx)+segBufL)%segBufL;
  return segBuf+rIdx;
}
bool MStp::SegQ_Head_Push() MSTP_SEG_PREFIX
{
  if(SegQ_IsFull())return false;
  int newIdx=(segBufHeadIdx+1)%segBufL;
  segBufHeadIdx=newIdx;
  return true;
}



MSTP_SEG_PREFIX MSTP_segment* MStp::SegQ_Tail(int idx) MSTP_SEG_PREFIX
{
  if(SegQ_IsEmpty())return NULL;
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


bool MStp::VecAdd(xVec VECAdd,float speed,void* ctx)
{
  return VecTo(vecAdd(lastTarLoc,VECAdd),speed,ctx);
}



bool MStp::VecTo(xVec VECTo,float speed,void* ctx)
{

  if(SegQ_Space() <=5)
  {
    return false;
  }


  MSTP_SEG_PREFIX MSTP_segment* hrb=SegQ_Head();
  MSTP_SEG_PREFIX MSTP_segment &newSeg=*hrb;
  newSeg.ctx=ctx;
  newSeg.vcur=0;
  newSeg.vcen=speed;
  newSeg.vto=0;
  newSeg.type=blockType::blk_line;
  vecAssign(newSeg.from,lastTarLoc);
  vecAssign(newSeg.to,VECTo);
  vecAssign(newSeg.runvec,vecSub(VECTo,lastTarLoc));
  newSeg.steps=vecMachStepCount(VECTo,lastTarLoc);
  newSeg.cur_step=0;
  newSeg.JunctionNormCoeff=0;
  newSeg.JunctionNormMaxDiff=NAN;
  newSeg.vto_JunctionMax=0;

  __PRT_I_("\n");
  __PRT_I_("==========NEW runvec[%s]======idx: h:%d t:%d===\n",toStr(newSeg.runvec),segBufHeadIdx,segBufTailIdx);
  lastTarLoc=VECTo;




  if(newSeg.steps==0)
  {
    return true;
  }



  // 
  // timerAlarmDisable(timer);

  MSTP_SEG_PREFIX MSTP_segment *preSeg = NULL;
  
  if(SegQ_Size()>0)//get previous block to calc junction info
  {
    preSeg = SegQ_Head(1);
  }
  if(preSeg!=NULL)
  {// you need to deal with the junction speed
    // preSeg->vec;
    // newSeg.vec;
    

    float coeff1=NAN;
    int coeffSt= Calc_JunctionNormCoeff(preSeg,&newSeg,&coeff1);
    if(coeffSt<0)
    {
      newSeg.JunctionNormCoeff=0;
    }

    __PRT_I_("====coeff:%f    coeffSt:%d==\n",coeff1,coeffSt);
    newSeg.JunctionNormMaxDiff=99999999;
    if(coeffSt==0)
    {
      float maxDiff1=NAN;
      int retSt=0;
      retSt |= Calc_JunctionNormMaxDiff(preSeg,&newSeg,coeff1,&maxDiff1);
      // retSt |= Calc_JunctionNormMaxDiff(*preSeg,newSeg,coeff2,maxDiff2);



      /*
        (1+coeff)/maxDiff
        1 is for the pre speed factor and coeff is for post speed factor

        so 1+coeff would make sure the conbined speed is the larger the better

        To devide maxDiff is to normalize the max difference number
      */




      newSeg.JunctionNormCoeff=coeff1;
      newSeg.JunctionNormMaxDiff=maxDiff1;
      __PRT_I_("====coeff:%f,%f diff:%f==\n",newSeg.JunctionNormCoeff,coeff1,newSeg.JunctionNormMaxDiff);
      if(retSt==0)
      {
        // newSeg.JunctionNormMaxDiff=maxDiff1;

        // __PRT_D_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n DIFF:",newSeg.JunctionNormMaxDiff,preSeg->vto,newSeg.vcur);

        if(newSeg.JunctionNormMaxDiff<0.01)
          newSeg.JunctionNormMaxDiff=0.01;//min diff cap to prevent value explosion


        //max allowed end speed of pre block, so that at junction the max speed jump is within the limit, this is fixed
        preSeg->vto_JunctionMax=junctionMaxSpeedJump/newSeg.JunctionNormMaxDiff;

        //vcur is the current speed, for un-executed block it's the starting speed
        newSeg.vcur=preSeg->vto_JunctionMax*newSeg.JunctionNormCoeff;

        {
          float vto_JunctionMax=preSeg->vto_JunctionMax;
          float JunctionNormMaxDiff=newSeg.JunctionNormMaxDiff;
          float vcur=newSeg.vcur;
          float vcen=newSeg.vcen;
          __PRT_I_("===JunctionMax:%f ndiff:%f vcur:%f  vcen:%f==\n",vto_JunctionMax,JunctionNormMaxDiff,vcur,vcen);
          
        }
        if(newSeg.vcur>newSeg.vcen)//check if the max initial speed is higher than target speed
        {
          newSeg.vcur=newSeg.vcen;//cap the speed

          preSeg->vto_JunctionMax=newSeg.vcur/newSeg.JunctionNormCoeff;//calc speed back to preSeg->vto

          {
            float vto_JunctionMax=preSeg->vto_JunctionMax;
            float vcur=newSeg.vcur;
            float vcen=newSeg.vcen;
            __PRT_I_("===JunctionMax:%f  vcur:%f  vcen:%f==\n",vto_JunctionMax,vcur,vcen);
            
          }
          
        }
        else 
        {

        }
        
        preSeg->vto=preSeg->vto_JunctionMax;


        {
          float vto_JunctionMax=preSeg->vto_JunctionMax;
          float vto=preSeg->vto;
          float vcur=newSeg.vcur;
          __PRT_I_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n",vto_JunctionMax,vto,vcur);
          
        }

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



    __PRT_D_("maxJump:%f coeff:%f  maxDiff:%f\n",junctionMaxSpeedJump, newSeg.JunctionNormCoeff, newSeg.JunctionNormMaxDiff);


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


      CASE2:  curblk is not long enough to able to de-accelerate from curblk.vcen(v center max speed) to curblk.vto
              so the preSeg need to reduce the vto speed, so curblk.vcur is low enough to safely de-accelerate to curblk.vto
                         
      example:curblk.steps=4                    
              curblk.vto is 0(stop), but without changing preSeg.vto it's impossible

          
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
        int32_t minDistNeeded= DeAccDistNeeded_f(curblk->vcen,curblk->vto, -acc);
        if( curDeAccSteps <= minDistNeeded )
        {
          cur_vfrom = findV1(curDeAccSteps, -acc, curblk->vto);
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
        __PRT_I_("[%d]:blk.steps:%d v:%f,%f,%f \n",i,preSeg->steps,vcur,vcen,new_preSeg_vto);
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

    __PRT_I_("[%2d]:steps:%6d v:%05.2f, %05.2f, %05.2f coeff:(%0.2f,%0.2f) \n",i,seg->steps,
      seg->vcur,seg->vcen,seg->vto,
      seg->JunctionNormCoeff,
      seg->JunctionNormMaxDiff);
    __PRT_I_("     :%s\n",toStr(seg->runvec));

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


void MStp::BlockRunStep(MSTP_SEG_PREFIX MSTP_segment *seg) MSTP_SEG_PREFIX
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  float a1=acc;
  float a2=-a1;
  int32_t D = (seg->steps-seg->cur_step);
  

  // int32_t deAccReqD=DeAccDistNeeded(vcur_int, vto_int,a2);
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
    
    seg->vcur+=speedInc;
    // __PRT_D_("a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",a2,T_next,TICK2SEC_BASE);

    if(seg->vcur<seg->vto)
    {
      seg->vcur=seg->vto;
    }

    if(seg->vcur<0)
    {
      seg->vcur=0;
    }

  // __PRT_D_("seg->vcur:%f a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",seg->vcur,a2,T_next,TICK2SEC_BASE);

  }
  else if(seg->vcur<seg->vcen )
  {
    if(seg->cur_step!=0)
    {
      float speedInc=a1/vcur;
      if(speedInc>maxSpeedInc)
      {
        speedInc=maxSpeedInc;
      }        
      seg->vcur+=(speedInc);
    }
    
    // __PRT_D_("a1:%d  seg->vcur:%f\n",a1,seg->vcur);
    if(seg->vcur>seg->vcen)
    {
      seg->vcur=seg->vcen;
    }
  }

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


  uint32_t _axis_pul=0;
  uint32_t steps_scal=seg->steps;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    curPos_mod.vec[k]+=posvec.vec[k];
    if(curPos_mod.vec[k]>=steps_scal)//a step forward
    {
      curPos_mod.vec[k]-=steps_scal;
      curPos_residue.vec[k]=seg->steps-curPos_mod.vec[k];
      _axis_pul|=1<<k;
    }
    else
    {
      curPos_residue.vec[k]=0;
    }

  }

  axis_pul=_axis_pul;
}



void MStp::SegPlayer() MSTP_SEG_PREFIX
{
  if(p_runSeg==NULL)
  {
    // BlockInitEffect(NULL);
    T_next=0;
    // cout << "This is the first thread "<< endl;
    // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    axis_pul=0;

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      curPos_residue.vec[i]=0;

    }
    // blocks->consumeTail();
    // __PRT_I_("Empty Q\n");
    p_runSeg=SegQ_Tail();
    if(p_runSeg!=NULL)
    {
    }      
    else
    {
      stopTimer();
      // __PRT_I_("No Tail\n");
      return;
    }

  }


    
  if(p_runSeg!=NULL)
  {

    if(p_runSeg->cur_step==p_runSeg->steps)
    {
      float vcur= p_runSeg->vcur;
      xVec vec0=(xVec){0};
      vecAssign(curPos_mod,vec0);
      vecAssign(curPos_residue,vec0);
      vecAssign(posvec,vec0);
      
      __PRT_D_("EndSpeed:%f  T_next:%d\n",vcur,T_next);

      BlockEndEffect(p_runSeg);
      
      SegQ_Tail_Pop();

      p_runSeg=SegQ_Tail();
      if(p_runSeg!=NULL)
      {
        p_runSeg->cur_step=0;
        p_runSeg->vcur= vcur*p_runSeg->JunctionNormCoeff;

        // __PRT_D_("  =vcur:%f x coeff:%f=new_v %f,%f,%f==\n",vcur,p_runSeg->JunctionNormCoeff,p_runSeg->vcur,blk.vcen,blk.vto);


      }
      else
      {

        // __PRT_I_("No Tail\n");
      }
    }
    if(p_runSeg==NULL)
    {
      stopTimer();
      return;
    }

    
    float vcur= p_runSeg->vcur;
    if(p_runSeg->cur_step==0)
    {

      uint32_t _axis_dir=0;
      xVec rvec;
      vecAssign_ref(rvec,p_runSeg->runvec);
      for(int k=0;k<MSTP_VEC_SIZE;k++)
      {
        if(rvec.vec[k]==0)
        {
          _axis_dir|=axis_dir&(1<<k);//if no movement use the old info
        }
        if(rvec.vec[k]<0)
        {
          posvec.vec[k]=-rvec.vec[k];
          _axis_dir|=1<<k;
        }
        else
        {
          posvec.vec[k]=rvec.vec[k];
        }
      }

      // __PRT_D_("start=vcur:%f=vcen:%f==vto:%f==\n",vcur,p_runSeg->vcen,p_runSeg->vto);

      axis_dir=_axis_dir;
      // T_next=0;

      BlockInitEffect(p_runSeg);//flip direction

    }

    BlockRunStep(p_runSeg);



    vcur=p_runSeg->vcur;
    if(vcur<minSpeed)
    {
      vcur=minSpeed;
    }



    float T = TICK2SEC_BASE/vcur;
    this->T_next=(uint32_t)(T);

    delayResidue+=T-T_next;
    if(delayResidue>1)
    {
      delayResidue-=1;
      T_next+=1;
    }






    p_runSeg->cur_step++;
    // std::this_thread::sleep_for(std::chrono::milliseconds(sysInfo.T_next));
    
    // BlockRunEffect();


    // __PRT_D_("\n\n\n");
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
    // __PRT_D_(">[%d]>%d\n",k,resd);
    if(resd!=0 && resd<=midP)
    {
      idxes|=1<<k;
    }
  }
  return idxes;
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
  if(tskrun_state==0)
  {

    __PRT_D_("tskrun_state:%d isMidPulTrig:%d\n",tskrun_state,isMidPulTrig);
    // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
    SegPlayer();

    pre_indexes=0;
    isMidPulTrig=false;
    axis_collectpul=0;
    __PRT_D_("p_runSeg:%p T_next:%d\n",p_runSeg,T_next);
    if(p_runSeg==NULL)
    {
      return 0;
    }
    if(T_next==0)
    {
      return 0;
    }
    // __PRT_D_(">>>>st0 T_next:%d\n",T_next);
    tskrun_state=1;

  }

  __PRT_D_("tskrun_state:%d isMidPulTrig:%d \n",tskrun_state,isMidPulTrig);
  if(tskrun_state==1)
  {
    if(p_runSeg==NULL)
    {
      pre_indexes=0;
      tskrun_state=0;
      return 0;
    }
    __PRT_D_("cur_step:%d \n",p_runSeg->cur_step);
    if(isMidPulTrig==false)
    {
      uint32_t idxes=findMidIdx(axis_pul,p_runSeg->steps);
      // Serial.printf("=curBlk->steps:%d==idxes:%s  resd:%d\n",curBlk->steps,int2bin(idxes,MSTP_VEC_SIZE),curPos_residue.vec[0]);
      if(idxes==0)
      {
        IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=!PIN_DBG0_st);
      }
      isMidPulTrig=true;
      axis_pul&=~idxes;//surpress current

      pre_indexes=idxes;
      axis_collectpul=_axis_collectpul1;
      _axis_collectpul1=pre_indexes;
      
      if(p_runSeg->cur_step==1)
      {
        pre_indexes=0;
        axis_collectpul=~0;
      }
      return T_next/2;
    }


    if(p_runSeg->cur_step==1)
    {
      BlockDirEffect(axis_dir);
    }
    tskrun_state=0;

    pre_indexes=axis_pul;
    axis_collectpul=_axis_collectpul1;
    _axis_collectpul1=pre_indexes;
    return T_next/2;
  }
  
  return 0;
}
