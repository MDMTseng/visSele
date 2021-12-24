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


// inline float DeAccDistNeededf(float VT, float V2, float a2)
// {
//   return (V2*V2-VT*VT)/a2/2;
// }


inline int32_t DeAccDistNeeded(int32_t VT, int32_t V2, int32_t a2)
{
  return (V2*V2-VT*VT)/a2/2;
}


float accTo_DistanceNeeded(float Vc, float Vd, float ad, float *ret_Td=NULL)
{
  float T=(Vd-Vc)/ad;
  float D=(Vd*Vc)*T/2;

  if(ret_Td)*ret_Td=T;

  return D;
}


bool PIN_DBG0_st=false;

MStp::MStp(RingBuf<runBlock> *_blocks)
{

  IO_SET_DBG(PIN_DBG0, OUTPUT);
  curPos=lastTarLoc=preVec=(xVec){0};
  T_next=T_lapsed=0;
  minSpeed=2;
  acc=1;
  axis_pul=0;
  blocks=_blocks;
  axis_RUNState=1;
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

  if(rb.steps==0)
  {
    return;
  }

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
    for(int i=0;i<MSTP_VEC_SIZE;i++)
    {
      dotp+=tail->vec.vec[i]*rb.vec.vec[i];
      absA+=tail->vec.vec[i]*tail->vec.vec[i];
      absB+=rb.vec.vec[i]*rb.vec.vec[i];
    }
    float cosinSim=dotp*Q_rsqrt(absA*absB);
    if(cosinSim<0)cosinSim=0;
    tail->vto=0;//cosinSim*rb.vcen;
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
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  int32_t vcur_int=rb.vcur;
  int32_t vcen_int=rb.vcen;
  int32_t vto_int=rb.vto;

  int32_t a1=acc;
  int32_t a2=-a1;
  uint32_t D = (rb.steps-rb.cur_step);
  

  int32_t deAccReqD=DeAccDistNeeded(vcur_int, vto_int,a2);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);

  __PRT__("vcur_int:%d vcen_int:%d vto_int:%d a1:%d D:%d  deAccReqD:%d  \n",vcur_int,vcen_int,vto_int,a1,D,deAccReqD);
  if(D<deAccReqD+2)
  {
    rb.vcur+=(float)a2*T_next/TICK2SEC_BASE;
    if(rb.vcur<rb.vto)
    {
      rb.vcur=rb.vto;
    }
  // __PRT__("rb.vcur:%f a2:%d  T_next:%d  TICK2SEC_BASE:%d\n",rb.vcur,a2,T_next,TICK2SEC_BASE);

  }
  else if(vcur_int<vcen_int)
  {
    rb.vcur+=(float)a1*T_next/TICK2SEC_BASE;
    
    if(vcur_int>vcen_int)
    {
      rb.vcur=rb.vcen;
    }
  }






  // rb.vcur=rb.vcen;

  if(vcur_int<(uint32_t)minSpeed)//set min speed
  {
    rb.vcur=minSpeed;
  }

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // uint32_t step_scal=(rb.cur_step+1)<<PULSE_ROUND_SHIFT;//100x is for round  +1 for predict next position

  // __PRT__("=%03d/%03d==: \n",step_scal,rb.steps);
  uint32_t _axis_pul=0;
  uint32_t steps_scal=rb.steps;
  for(int k=0;k<MSTP_VEC_SIZE;k++)
  {
    curPos.vec[k]+=rb.vec.vec[k];
    if(curPos.vec[k]>=steps_scal)//a step forward
    {
      curPos.vec[k]-=steps_scal;
      curPos_residue.vec[k]=rb.steps-curPos.vec[k];
      _axis_pul|=1<<k;
    }
    else
    {
      curPos_residue.vec[k]=0;
    }

    // curPos.vec[k]+=rb.vec.vec[k];
  }

  axis_pul=_axis_pul;
  rb.cur_step++;

  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=1);
  // IO_WRITE_DBG(PIN_DBG0, PIN_DBG0_st=0);
}



void MStp::blockPlayer()
{
  
  if(blocks->size()>0)
  {

    runBlock &blk=*blocks->getTail();
    curBlk=&blk;
    if(blk.cur_step==0)
    {
      uint32_t _axis_dir=0;
      for(int k=0;k<MSTP_VEC_SIZE;k++)
      {
        if(blk.vec.vec[k]<0)
        {
          blk.vec.vec[k]=-blk.vec.vec[k];
          _axis_dir|=1<<k;
        }
      }

      axis_dir=_axis_dir;
      BlockDirEffect(axis_dir);
    }

    BlockRunStep(blk);

    float T = TICK2SEC_BASE/blk.vcur;
    T_next=(uint32_t)(T);

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
      // __PRT__("======vcur:%f=vto:%f===T_lapsed:%d===\n",vcur,blk.vto,T_lapsed);
      memset(&curPos,0,sizeof(curPos));
      memset(&curPos_residue,0,sizeof(curPos_residue));
      
      blocks->consumeTail();
      runBlock *new_blk=blocks->getTail();
      if(new_blk!=NULL)
      {
        new_blk->cur_step=0;
        new_blk->vcur= vcur;
        // curPos.vec[k]



      }
      T_lapsed=0;
    }
    

    // __PRT__("\n\n\n");
  }
  else
  {
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
    if(from_idxes&(1<<k)==0)continue;
    int resd=curPos_residue.vec[k];
    __PRT__(">[%d]>%d\n",k,resd);
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
    __PRT__(">>>>st0 T_next:%d\n",T_next);
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
