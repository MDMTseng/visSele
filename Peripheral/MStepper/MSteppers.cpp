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
char *int2bin(uint32_t a, int digits, char *buffer, int buf_size) {
    buffer += (buf_size - 1);

    for (int i = digits-1; i >= 0; i--) {
        *buffer-- = (a & 1) + '0';

        a >>= 1;
    }
    buffer++;
    return buffer;
}
char* toStr(const xVec &vec)
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

float totalTimeNeeded2(float V1,float a1,float VT, float V2, float a2,float D, float *ret_T1, float *ret_T2)
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


float DeAccTimeNeeded2(float VT, float V2, float a2,float *ret_D)
{
  float T = (V2-VT)/a2;
  if(ret_D)
  {
    *ret_D=T*(V2+VT)/2;
  }

  return T;
}



float findV1(float D, float a, float V2)
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

int32_t DeAccDistNeeded(int32_t VT, int32_t V2, int32_t a2)
{
  return ((int64_t)V2*V2-(int64_t)VT*VT)/a2/2;
  // int32_t resx2=((V2*V2-VT*VT)<<2)/a2;
  // return (resx2>>3)+((resx2&(1<<2))?1:0);//do round
}


float DeAccDistNeeded_f(float V1, float V2, float a2)
{
  return (V2*V2-V1*V1)/a2/2;
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
int Calc_JunctionNormCoeff(runBlock &blkA,runBlock &blkB,float &ret_blkB_coeff1)
{
  ret_blkB_coeff1=NAN;
  float BBsum=0;
  float ABsum=0;//basically a dot product
  float AAsum=0;//basically a dot product
  
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







  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    // float A=(float)preBlk->runvec.vec[i]/preBlk->steps;//normalize here, a bit slower
    // float B=(float)rb.runvec.vec[i]/rb.steps;

    float A=(float)blkA.runvec.vec[i];//X
    float B=(float)blkB.runvec.vec[i];//Y

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


  // AAsum/=blkA.steps;

  // rb.JunctionCoeff=dotp/BB;//normalize in the loop, a bit slower
  float coeff1 = (ABsum/blkA.steps)/(0.0001+BBsum/blkB.steps);
  ret_blkB_coeff1=coeff1;//forward way Xi => bYi

  // // 
  // float coeff2 = (AAsum/blkA.steps)/(0.0001+ABsum/blkB.steps);
  // ret_blkB_coeff2=coeff2;//backward way forward way cXi => Yi : b=1/c
  // __PRT_D_("coeff:%f %f\n",coeff1,coeff2);
  return 0;
}




int Calc_JunctionNormCoeff2(runBlock &blkA,runBlock &blkB,float &ret_blkB_coeff)
{



  //STEAGE1 start
  
  float coeff;
  {
    float BBsum=0;
    float ABsum=0;//basically a dot product
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







      for(int i=0;i<MSTP_VEC_SIZE;i++)
      {
        // float A=(float)preBlk->runvec.vec[i]/preBlk->steps;//normalize here, a bit slower
        // float B=(float)rb.runvec.vec[i]/rb.steps;

        float A=(float)blkA.runvec.vec[i];//X
        float B=(float)blkB.runvec.vec[i];//Y

        // if(AB<0 && AB<-junctionMaxSpeedJump)
        // {//The speed from +to-(or reverse) and the difference is too huge, then set target speed to zero
        //   dotp=0;
        //   BB=1;
        //   break;
        // }
        ABsum+=A*B;
        BBsum+=B*B;
      }
      // if(ABsum<0)
      // {
      //   // ret_blkB_coeff=NAN;
      //   return -1;
      // }


      // AAsum/=blkA.steps;

      // rb.JunctionCoeff=dotp/BB;//normalize in the loop, a bit slower
      coeff = (ABsum/blkA.steps)/(BBsum/blkB.steps);
  }

  //STEAGE1 end we found the rough coefficient
  //STEAGE2 start, find which line(element transision) is the highest at this rough coeff (which line that the maxDiff(coeff) minumum sits on)

  int MmL_index=-1;
  bool MmL_sec=-1;
  float maxAbsDiff=0;
  float NormCoeff=blkA.steps*coeff/blkB.steps;
  {

    // ret_MaxDiff=NAN;
    //Xi/Yi is for i axis run step count
    //Xc/Yc is the running steps count
    //target=> Max(Xi/Xc-b*Yi/Yc)
    //      =  Max(Xi-Yi*b*Xc/Yc)/Xc
    //NormCoeff= b*Xc/Yc

    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      float A=(float)blkA.runvec.vec[i];//pre extract steps
      float cB=(float)blkB.runvec.vec[i]*NormCoeff;
      float diff = A-cB;
      float abs_diff=diff<0?-diff:diff;
      if(maxAbsDiff<abs_diff)
      {
        MmL_index=i;
        MmL_sec=((diff>=0)^(A>0) )?0:1;
        maxAbsDiff=diff;
      }

    }
    maxAbsDiff/=blkA.steps;
    
  }

  //STEAGE2 ended we know which line that the minumum sits on

  //STEAGE3 start, accoding to the sec variable, we know where the minmax line is and what's the 


  /*
    hint:
    a1+b1 X=Y
    a2+b2 X=Y
    X=(a1-a2)/(b2-b1)
    Y=(b2a1-b1a2)/(b2-b1)
  */

  {//diff=A-coeff*B

    float MmL_A=(float)blkA.runvec.vec[MmL_index]/blkA.steps;//pre extract steps
    float MmL_B=(float)blkB.runvec.vec[MmL_index]/blkB.steps;
    //MmL_sec==0 =>  MmL_A-coeff*MmL_B >0  in this case  MmL_A>0 MmL_B>0
    //MmL_sec==1 =>  MmL_A-coeff*MmL_B <0


    if(MmL_sec==0)
    {
      if(MmL_A<0)
      {
        MmL_A=-MmL_A;
        MmL_B=-MmL_B;
      }
    }


    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      if(i==MmL_index)
      {
        continue;
      }
      float A=(float)blkA.runvec.vec[i]/blkA.steps;//pre extract steps
      float B=(float)blkB.runvec.vec[i]/blkB.steps;

      if(MmL_B==B)continue;//no solution, parellel
      /*
      



      
      */
    
     float Y=(MmL_B*A - MmL_A*B)/(MmL_B-B);


    }
  }




  // ret_blkB_coeff1=coeff1;//forward way Xi => bYi

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
int Calc_JunctionNormMaxDiff(runBlock &blk1,runBlock &blk2,float blk2_coeff,float &ret_MaxDiff)
{


  ret_MaxDiff=NAN;
  if( blk2_coeff!=blk2_coeff)
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


  float maxAbsDiff=0;
  __PRT_I_("steps:%d %d    NormCoeff:%f\n",blk1.steps,blk2.steps,NormCoeff);

  __PRT_I_("blk1.runvec:%s\n",toStr(blk1.runvec));
  __PRT_I_("blk2.runvec:%s\n",toStr(blk2.runvec));

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    float A=(float)blk1.runvec.vec[i];//pre extract steps
    float B=(float)blk2.runvec.vec[i]*NormCoeff;


    float diff = A-B;
    if(diff<0)diff=-diff;
    __PRT_D_("blk[%d]: %d,%d\n",i,blk1.runvec.vec[i],blk2.runvec.vec[i]);
    __PRT_I_("maxJump[%d]: coeff:%f  %f %f  diff:%f\n",i,blk2_coeff,A/blk1.steps,B/blk1.steps,diff);
    if(maxAbsDiff<diff)maxAbsDiff=diff;
  }
  maxAbsDiff/=blk1.steps;

  ret_MaxDiff=maxAbsDiff;
  __PRT_I_("ret_MaxDiff:%f\n",ret_MaxDiff);
  
  return 0;
}







bool PIN_DBG0_st=false;

MStp::MStp(RingBuf<runBlock> *_blocks, MSTP_setup *_axisSetup)
{

  blocks=_blocks;
  axisSetup=_axisSetup;
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
  axis_pul=0;
}

void MStp::StepperForceStop()
{
  
  blocks->clear();
  p_runBlk=NULL;
  lastTarLoc=curPos_c;
  T_next=0;
  axis_pul=axis_dir=0;
  delayResidue=0;
  pre_indexes=0;
  tskrun_state=0;
  isMidPulTrig=true;
  _axis_collectpul1=_axis_collectpul2=0;
  curPos_mod=curPos_residue=(xVec){0};
  
}


// void MStp::AxisZeroing(uint32_t index)
// {



// }



void MStp::Delay(int interval,int intervalCount)
{
  
}


void MStp::VecAdd(xVec VECAdd,float speed,void* ctx)
{
  VecTo(vecAdd(lastTarLoc,VECAdd),speed,ctx);
}


bool MStp::isQueueEmpty()
{
  return (p_runBlk==NULL)&&(blocks->size()==0);
}


void MStp::VecTo(xVec VECTo,float speed,void* ctx)
{

  if(blocks->space() <3)
  {
    return;
  }



  runBlock newBlk;

  newBlk=(runBlock){
    .ctx=ctx,
    .type=blockType::blk_line,
    .from = lastTarLoc,
    .to=VECTo,
    .runvec = vecSub(VECTo,lastTarLoc),
    .steps=vecMachStepCount(VECTo,lastTarLoc),
    .cur_step=0,
    .JunctionNormCoeff=0,
    .JunctionNormMaxDiff=NAN,
    .isInDeAccState=false,
    .vcur=0,
    .vcen=speed,
    .vto=0,
    .vto_JunctionMax=0,
  };

  __PRT_I_("\n");
  __PRT_I_("==========NEW runvec[%s]=========\n",toStr(newBlk.runvec));
  lastTarLoc=VECTo;




  if(newBlk.steps==0)
  {
    return;
  }



  // 
  // timerAlarmDisable(timer);

  runBlock *preBlk = NULL;
  
  int blkGard=2;
  if(blocks->size()>blkGard)//get previous block to calc junction info
  {
    preBlk = blocks->getHead(1);
  }
  if(preBlk!=NULL)
  {// you need to deal with the junction speed
    // preBlk->vec;
    // newBlk.vec;
    

    float coeff1=NAN;
    int coeffSt= Calc_JunctionNormCoeff(*preBlk,newBlk,coeff1);
    if(coeffSt<0)
    {
      newBlk.JunctionNormCoeff=0;
    }

    __PRT_I_("====coeff:%f    coeffSt:%d==\n",coeff1,coeffSt);
    newBlk.JunctionNormMaxDiff=99999999;
    if(coeffSt==0)
    {
      float maxDiff1=NAN;
      int retSt=0;
      retSt |= Calc_JunctionNormMaxDiff(*preBlk,newBlk,coeff1,maxDiff1);
      // retSt |= Calc_JunctionNormMaxDiff(*preBlk,newBlk,coeff2,maxDiff2);



      /*
        (1+coeff)/maxDiff
        1 is for the pre speed factor and coeff is for post speed factor

        so 1+coeff would make sure the conbined speed is the larger the better

        To devide maxDiff is to normalize the max difference number
      */




      newBlk.JunctionNormCoeff=coeff1;
      newBlk.JunctionNormMaxDiff=maxDiff1;
      __PRT_I_("====coeff:%f diff:%f==\n",newBlk.JunctionNormCoeff,newBlk.JunctionNormMaxDiff);
      if(retSt==0)
      {
        // newBlk.JunctionNormMaxDiff=maxDiff1;

        // __PRT_D_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n DIFF:",newBlk.JunctionNormMaxDiff,preBlk->vto,newBlk.vcur);

        if(newBlk.JunctionNormMaxDiff<0.01)
          newBlk.JunctionNormMaxDiff=0.01;//min diff cap to prevent value explosion


        //max allowed end speed of pre block, so that at junction the max speed jump is within the limit, this is fixed
        preBlk->vto_JunctionMax=junctionMaxSpeedJump/newBlk.JunctionNormMaxDiff;

        //vcur is the current speed, for un-executed block it's the starting speed
        newBlk.vcur=preBlk->vto_JunctionMax*newBlk.JunctionNormCoeff;

        {
          float vto_JunctionMax=preBlk->vto_JunctionMax;
          float JunctionNormMaxDiff=newBlk.JunctionNormMaxDiff;
          float vcur=newBlk.vcur;
          float vcen=newBlk.vcen;
          __PRT_I_("===JunctionMax:%f ndiff:%f vcur:%f  vcen:%f==\n",vto_JunctionMax,JunctionNormMaxDiff,vcur,vcen);
          
        }
        if(newBlk.vcur>newBlk.vcen)//check if the max initial speed is higher than target speed
        {
          newBlk.vcur=newBlk.vcen;//cap the speed

          preBlk->vto_JunctionMax=newBlk.vcur/newBlk.JunctionNormCoeff;//calc speed back to preBlk->vto

          {
            float vto_JunctionMax=preBlk->vto_JunctionMax;
            float vcur=newBlk.vcur;
            float vcen=newBlk.vcen;
            __PRT_I_("===JunctionMax:%f  vcur:%f  vcen:%f==\n",vto_JunctionMax,vcur,vcen);
            
          }
          
        }
        else 
        {

        }
        
        preBlk->vto=preBlk->vto_JunctionMax;


        {
          float vto_JunctionMax=preBlk->vto_JunctionMax;
          float vto=preBlk->vto;
          float vcur=newBlk.vcur;
          __PRT_I_("====CALC DIFF==JMax:%f  tvto:%f  rvcur:%f==\n",vto_JunctionMax,vto,vcur);
          
        }

        // for(int k=0;k<MSTP_VEC_SIZE;k++)
        // {
        //   float v1=(preBlk->vto*preBlk->runvec.vec[k]/preBlk->steps);
        //   float v2=(  newBlk.vcur*   newBlk.runvec.vec[k]/   newBlk.steps);
        //   float diff = v1-v2;
        //   __PRT_I_("(A(%f)-B(%f)=%04.2f )\n",v1,v2,diff);
        // }



      }
      else
      {
        newBlk.vcur=preBlk->vto=0;
      }
    }
    else
    {
      newBlk.vcur=preBlk->vto=0;
    }



    __PRT_D_("maxJump:%f coeff:%f  maxDiff:%f\n",junctionMaxSpeedJump, newBlk.JunctionNormCoeff, newBlk.JunctionNormMaxDiff);


    {
      float vto=preBlk->vto;
      float vcen=preBlk->vcen;
      float nvcur=newBlk.vcur;
      float nvcen=newBlk.vcen;
      __PRT_I_("pre vcen:%0.3f vto:%0.3f =>new vcur:%0.3f vcen:%0.3f\n",vcen,vto,nvcur,nvcen);
      
    }


    // newBlk.vcur=
    // preBlk->vto=0;//cosinSim*newBlk.vcen;
    







    // Serial.printf("preBlk vcur:%f  vcen:%f  vto:%f    newBlk:vcur:%f  vcen:%f  vto:%f   \n",preBlk->vcur,preBlk->vcen,preBlk->vto,   newBlk.vcur,newBlk.vcen,newBlk.vto );





  }
  else
  {
    // newBlk.vcur=100;
    // T_next=TICK2SEC_BASE/minSpeed;
    T_next=0;
  }
  runBlock* hrb=blocks->getHead();
  *hrb=newBlk;
  blocks->pushHead();



  if(1)
  {

    /*


               preblk |     curblk
           
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
              so the preblk need to reduce the vto speed, so curblk.vcur is low enough to safely de-accelerate to curblk.vto
                         
      example:curblk.steps=4                    
              curblk.vto is 0(stop), but without changing preblk.vto it's impossible

          
            _________V  preblk.vto 
          /          | \   
        /            |   \  
      /              |     \ 
    /                |      |
  /                  |      |
                      <-----> 
                      curblk.steps

            recalc preblk.vto'(lower the vto value) so that curblk can reach curblk.vto in the end
            ______   
          /        \   
        /            \V  preblk.vto'   
      /              | \ 
    /                |   \
  /                  |     \
                      <-----> 
                      curblk.steps




    CASE3 :the curblk has enough steps to de-accelerate to curblk.vto, so to change preblk is not needed 
            _________V___  preblk.vto 
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
    //{oldest blk}.....preblk, curblk, {newest blk}
    runBlock* curblk;
    runBlock* preblk = blocks->getHead(1);
    for(int i=1;(i+2)<blocks->size();i++)
    {//can only adjust vto
      curblk = preblk;
      preblk = blocks->getHead(1+i);


      // if(preblk==NULL)break;
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
          //so we don't need to change the speed vto of (preblk)
          break;
        }
      }


      if(cur_vfrom!=cur_vfrom)
      {
        //unset, ERROR
        break;
      }



      float preblk_vto_max=cur_vfrom/(curblk->JunctionNormCoeff+0.01);
      float new_preblk_vto=preblk_vto_max<preblk->vto_JunctionMax?preblk_vto_max:preblk->vto_JunctionMax;
      {
        float vcur=preblk->vcur;
        float vcen=preblk->vcen;
        __PRT_I_("[%d]:blk.steps:%d v:%f,%f,%f \n",i,preblk->steps,vcur,vcen,new_preblk_vto);
      }
      if(preblk->vto == new_preblk_vto)
      {//if the preblk vto is exactly the same then the following adjustment is not nessasary
        break;
      }
      preblk->vto=new_preblk_vto;
      // __PRT_D_("[%d]:v1:%f  ori_V1:%f Vdiff:%f\n",i,v1,ori_V1,Vdiff);




    }


  }
  startTimer();
  
  // timerAlarmEnable(timer);
  // 

}



void MStp::printBLKInfo()
{
  for(int i=0;i<blocks->size();i++)
  {
    runBlock& blk = *blocks->getTail(i);

    __PRT_I_("[%2d]:steps:%6d v:%05.2f, %05.2f, %05.2f coeff:(%0.2f,%0.2f) \n",i,blk.steps,
      blk.vcur,blk.vcen,blk.vto,
      blk.JunctionNormCoeff,
      blk.JunctionNormMaxDiff);
    __PRT_I_("     :%s\n",toStr(blk.runvec));

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


void MStp::BlockRunStep(runBlock &rb)
{
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  float a1=acc;
  float a2=-a1;
  int32_t D = (rb.steps-rb.cur_step);
  

  // int32_t deAccReqD=DeAccDistNeeded(vcur_int, vto_int,a2);
  int32_t deAccReqD=(int32_t)DeAccDistNeeded_f(rb.vcur, rb.vto, a2);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  // __PRT_D_("vcur_int:%d vcen_int:%d vto_int:%d a1:%d D:%d  deAccReqD:%d  T_next:%d\n",vcur_int,vcen_int,vto_int,a1,D,deAccReqD,T_next);


  float vcur=rb.vcur;

  if(vcur<minSpeed)
  {
    vcur=minSpeed;
  }
  int deAccBuffer=D-deAccReqD;
  if(deAccBuffer<4)
  {
    // float alpha=-((deAccBuffer)/4);
    float speedInc=a2/vcur;//*(1+alpha);

    rb.vcur+=speedInc;
    // __PRT_D_("a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",a2,T_next,TICK2SEC_BASE);

    if(rb.vcur<rb.vto)
    {
      rb.vcur=rb.vto;
    }

    if(rb.vcur<0)
    {
      rb.vcur=0;
    }

  // __PRT_D_("rb.vcur:%f a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",rb.vcur,a2,T_next,TICK2SEC_BASE);

  }
  else if(rb.vcur<rb.vcen )
  {
    if(rb.cur_step!=0)
    {
      float speedInc=a1/vcur;
      if(speedInc>maxSpeedInc)
      {
        speedInc=maxSpeedInc;
      }        
      rb.vcur+=(speedInc);
    }
    
    // __PRT_D_("a1:%d  rb.vcur:%f\n",a1,rb.vcur);
    if(rb.vcur>rb.vcen)
    {
      rb.vcur=rb.vcen;
    }
  }
  vcur=rb.vcur;
  if(vcur<minSpeed)
  {
    vcur=minSpeed;
  }


  // rb.vcur=rb.vcen;
  // {
  //   int space=3;
  //   if(rb.cur_step<space || rb.cur_step>=(rb.steps-space))
  //     __PRT_D_("cur_step:%d rb.vcur:%f  \n",rb.cur_step,rb.vcur);
  // }

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // uint32_t step_scal=(rb.cur_step+1)<<PULSE_ROUND_SHIFT;//100x is for round  +1 for predict next position

  // __PRT_D_("=%03d/%03d==: \n",step_scal,rb.steps);
  uint32_t _axis_pul=0;
  uint32_t steps_scal=rb.steps;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    curPos_mod.vec[k]+=posvec.vec[k];
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



  float T = TICK2SEC_BASE/vcur;
  this->T_next=(uint32_t)(T);

  delayResidue+=T-T_next;
  if(delayResidue>1)
  {
    delayResidue-=1;
    T_next+=1;
  }

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
}



void MStp::blockPlayer()
{
  
  if(p_runBlk!=NULL)
  {

    runBlock &blk=*p_runBlk;
    float vcur= blk.vcur;
    if(blk.cur_step==0)
    {

      uint32_t _axis_dir=0;
      xVec rvec=blk.runvec;
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

      // __PRT_D_("start=vcur:%f=vcen:%f==vto:%f==\n",vcur,blk.vcen,blk.vto);

      axis_dir=_axis_dir;
      // T_next=0;

      BlockInitEffect(p_runBlk);//flip direction

    }

    BlockRunStep(*p_runBlk);

    blk.cur_step++;
    // std::this_thread::sleep_for(std::chrono::milliseconds(sysInfo.T_next));
    
    // BlockRunEffect();
    if(blk.cur_step==blk.steps)
    {
      float vcur= blk.vcur;
      memset(&curPos_mod,0,sizeof(curPos_mod));
      memset(&curPos_residue,0,sizeof(curPos_residue));
      memset(&posvec,0,sizeof(posvec));
      
      __PRT_D_("EndSpeed:%f  T_next:%d\n",vcur,T_next);

      BlockEndEffect(p_runBlk);
      
      p_runBlk=NULL;
      blocks->consumeTail();

      p_runBlk= blocks->getTail();
      if(p_runBlk!=NULL)
      {
        p_runBlk->cur_step=0;
        p_runBlk->vcur= vcur*p_runBlk->JunctionNormCoeff;

        __PRT_D_("  =vcur:%f x coeff:%f=new_v %f,%f,%f==\n",vcur,p_runBlk->JunctionNormCoeff,p_runBlk->vcur,blk.vcen,blk.vto);


      }
      else
      {

        // __PRT_I_("No Tail\n");
      }
    }
    

    // __PRT_D_("\n\n\n");
  }
  else
  {
    BlockInitEffect(NULL);
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
    p_runBlk=blocks->getTail();
    if(p_runBlk!=NULL)
    {
    }      
    else
    {
      stopTimer();
      // __PRT_I_("No Tail\n");
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
    // __PRT_D_(">[%d]>%d\n",k,resd);
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
  if(tskrun_state==0)
  {

    // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
    blockPlayer();
    if(p_runBlk==NULL)
    {
      return 0;
    }
    pre_indexes=0;
    isMidPulTrig=false;
    axis_collectpul=0;

    if(T_next==0)
    {
      return 0;
    }
    // __PRT_D_(">>>>st0 T_next:%d\n",T_next);
    tskrun_state=1;

  }

  if(tskrun_state==1)
  {
    if(p_runBlk==NULL)
    {
      pre_indexes=0;
      tskrun_state=0;
      return 0;
    }
    if(isMidPulTrig==false)
    {
      uint32_t idxes=findMidIdx(axis_pul,p_runBlk->steps);
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


    if(p_runBlk->cur_step==1)
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
