#pragma once

#include "GCodeParser_M.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
using namespace std;

#include "LOG.h"
#include "main.hpp"



GCodeParser_M::GCodeParser_M(MStp *mstp)
{
  _mstp=mstp;
}
float GCodeParser_M::unit2Pulse(float dist,float pulses_per_mm)
{//only mm for now
  if(unit_is_inch)
  {
    dist*=50.8;
  }
  // __PRT_D_("unit:%f subdiv:%d  mm_per_rev:%f\n",unit,subdiv,mm_per_rev);
  return dist*pulses_per_mm;
}

int GCodeParser_M::ReadxVecELE_toPulses(char **blkIdxes,int blkIdxesL,xVec &retVec,int axisIdx,const char* axisGIDX)
{

  float num;
  int ret = FindFloat(axisGIDX,blkIdxes,blkIdxesL,num);
  if(ret)return ret;
  retVec.vec[axisIdx]=(uint32_t)unit2Pulse_conv(axisIdx,num);
  return 0;
}

int GCodeParser_M::ReadG1Data(char **blkIdxes,int blkIdxesL,xVec &vec,float &F)
{
  vec=isAbsLoc?vecSub(MTPSYS_getLastLocInStepperSystem(),pulse_offset):(xVec){0};
  ReadxVecData(blkIdxes,blkIdxesL,vec);

  float tmpF=NAN;
  int ret = FindFloat(AXIS_GDX_FEEDRATE,blkIdxes,blkIdxesL,tmpF);
  if(tmpF<0)ret=-1;
  // if(tmpF>400)ret=-1;// HACK: TODO: speed cap
  // if(tmpF>400)tmpF=400;// HACK: TODO: speed cap
  if(ret==0)
  {
    tmpF=unit2Pulse_conv(AXIS_IDX_FEEDRATE,tmpF);

    if(tmpF>0)
    {
      latestF=tmpF;
    }
    else 
    {
      tmpF=latestF;
    }

  }
  else
  {
    tmpF=latestF;
  }
  F=tmpF;

  return 0;
}



int GCodeParser_M::ReadxVecData(char **blkIdxes,int blkIdxesL,xVec &retVec)
{
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_X,AXIS_GDX_X);

  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Y,AXIS_GDX_Y);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Z,AXIS_GDX_Z);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_A,AXIS_GDX_A);

  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Z1,AXIS_GDX_Z1);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_R1,AXIS_GDX_R1);
  
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Z2,AXIS_GDX_Z2);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_R2,AXIS_GDX_R2);

  
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Z3,AXIS_GDX_Z3);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_R3,AXIS_GDX_R3);

  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_Z4,AXIS_GDX_Z4);
  ReadxVecELE_toPulses(blkIdxes,blkIdxesL,retVec,AXIS_IDX_R4,AXIS_GDX_R4);

  return 0;
}


int axisGDX2IDX(char *GDXCode,int fallback)
{
  if(CheckHead(GDXCode,AXIS_GDX_X))
  {
    return AXIS_IDX_X;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_Y))
  {
    return AXIS_IDX_Y;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_Z))
  {
    return AXIS_IDX_Z;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_A))
  {
    return AXIS_IDX_A;
  }

  else if(CheckHead(GDXCode,AXIS_GDX_Z1))
  {
    return AXIS_IDX_Z1;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_R1))
  {
    return AXIS_IDX_R1;
  }

  else if(CheckHead(GDXCode,AXIS_GDX_Z2))
  {
    return AXIS_IDX_Z2;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_R2))
  {
    return AXIS_IDX_R2;
  }
  
  else if(CheckHead(GDXCode,AXIS_GDX_Z3))
  {
    return AXIS_IDX_Z3;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_R3))
  {
    return AXIS_IDX_R3;
  }

  else if(CheckHead(GDXCode,AXIS_GDX_Z4))
  {
    return AXIS_IDX_Z4;
  }
  else if(CheckHead(GDXCode,AXIS_GDX_R4))
  {
    return AXIS_IDX_R4;
  }

  if(CheckHead(GDXCode,AXIS_GDX_FEEDRATE))
  {
    return AXIS_IDX_FEEDRATE;
  }
  if(CheckHead(GDXCode,AXIS_GDX_ACCELERATION))
  {
    return AXIS_IDX_ACCELERATION;
  }
  if(CheckHead(GDXCode,AXIS_GDX_DEACCELERATION))
  {
    return AXIS_IDX_DEACCELERATION;
  }
  if(CheckHead(GDXCode,AXIS_GDX_FEED_ON_AXIS))
  {
    return AXIS_IDX_FEED_ON_AXIS;
  }

  return fallback;
}

const char* axisIDX2GDX(int IDXCode)
{
  switch(IDXCode)
  {

    case AXIS_IDX_X:return AXIS_GDX_X;
    case AXIS_IDX_Y:return AXIS_GDX_Y;
    case AXIS_IDX_Z:return AXIS_GDX_Z;
    case AXIS_IDX_A:return AXIS_GDX_A;
    case AXIS_IDX_Z1:return AXIS_GDX_Z1;
    case AXIS_IDX_R1:return AXIS_GDX_R1;
    case AXIS_IDX_Z2:return AXIS_GDX_Z2;
    case AXIS_IDX_R2:return AXIS_GDX_R2;
    case AXIS_IDX_Z3:return AXIS_GDX_Z3;
    case AXIS_IDX_R3:return AXIS_GDX_R3;
    case AXIS_IDX_Z4:return AXIS_GDX_Z4;
    case AXIS_IDX_R4:return AXIS_GDX_R4;

    case AXIS_IDX_FEEDRATE:return AXIS_GDX_FEEDRATE;
    case AXIS_IDX_ACCELERATION:return AXIS_GDX_ACCELERATION;
    case AXIS_IDX_DEACCELERATION:return AXIS_GDX_DEACCELERATION;
    case AXIS_IDX_FEED_ON_AXIS:return AXIS_GDX_FEED_ON_AXIS;
  }
  return NULL;
}



int GCodeParser_M::MTPSYS_MachZeroRet(uint32_t axis_index,uint32_t sensor_pin,int distance,int speed,void* context)
{
  return _mstp->MachZeroRet(axis_index,sensor_pin,distance,speed,context);
}

bool GCodeParser_M::MTPSYS_VecTo(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
{
  return _mstp->VecTo(VECTo,speed,ctx,exinfo);
}
bool GCodeParser_M::MTPSYS_VecAdd(xVec VECTo,float speed,void* ctx,MSTP_segment_extra_info *exinfo)
{
  return _mstp->VecAdd(VECTo,speed,ctx,exinfo);
}

xVec GCodeParser_M::MTPSYS_getLastLocInStepperSystem()
{
  return _mstp->lastTarLoc;
}

float GCodeParser_M::MTPSYS_getMinPulseSpeed()
{
  return NAN;//_mstp->minSpeed;
}


bool GCodeParser_M::MTPSYS_AddWait(uint32_t period_ms,int times, void* ctx,MSTP_segment_extra_info *exinfo)
{
  return _mstp->AddWait(period_ms,times,ctx,exinfo);
}


GCodeParser::GCodeParser_Status GCodeParser_M::parseCMD(char **blks, char blkCount)
{

  bool isMTPLocked=( _mstp->MTP_INIT_Lock ||_mstp->endStopHitLock || _mstp->fatalErrorCode!=0);
  GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;
  char *cblk=blks[0];
  int cblkL=blks[1]-blks[0];

  blks++;
  blkCount--;

  
  if(cblk[0]=='G')
  {
    if(CheckHead(cblk, "G04 ")||CheckHead(cblk, "G4 "))//G04 P10; pause
    {
      if(isMTPLocked)
      {
        return GCodeParser_Status::TASK_FATAL_FAILED;
      }
      __PRT_D_("G04 Pause\n");
      int32_t P;
      int ret = FindInt32("P",blks,blkCount,P);
      if(ret==0)
      {
        // G_LOG("run MTPSYS_AddWait");
        if(MTPSYS_AddWait((uint32_t)P,1, NULL,NULL)==true)
        {
          retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
        }
        else
        {
          retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_FAILED);
        }
      }
      else
      {
        retStatus=statusReducer(retStatus,GCodeParser_Status::GCODE_PARSE_ERROR);
      }
    }
    else if(CheckHead(cblk, "G92 "))//G92 Set pos
    {
      if(isMTPLocked)
      {
        return GCodeParser_Status::TASK_FATAL_FAILED;
      }
      __PRT_D_("G92 Set pos\n");

      xVec xvLast = MTPSYS_getLastLocInStepperSystem();
      xVec vec=xvLast;
      ReadxVecData(blks,blkCount,vec);
      xVec vdiff=vecSub(xvLast,vec);

      xVec tmp=pulse_offset;
      for(int i=0;i<MSTP_VEC_SIZE;i++)
      {
        if(vdiff.vec[i]!=0)
          tmp.vec[i]=vdiff.vec[i];
      }

      pulse_offset=tmp;
      __PRT_D_("vec:%s ",toStr(vec));
      __PRT_D_("pulse_offset:%s\n",toStr(pulse_offset));
      __PRT_D_("sys last tar loc:%s\n",toStr(MTPSYS_getLastLocInStepperSystem()));

      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    else if(CheckHead(cblk, "G92.SYNC "))//G92 Set pos
    {

      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }
    else if(CheckHead(cblk, "G28"))//G28 GO HOME!!!:
    {
      _mstp->MTP_INIT_Lock=false;
      if(_mstp->MTP_INIT_Lock && isMTPLocked)
      {
        return GCodeParser_Status::TASK_FATAL_FAILED;
      }
      __PRT_D_("G28 GO HOME!!!:");
      int retErr=0;
      int hSp=20;
      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z1,PIN_Z1_SEN2,unit2Pulse_conv(AXIS_IDX_Z1,2000),unit2Pulse_conv(AXIS_IDX_Z1,hSp),NULL);
      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z2,PIN_Z2_SEN2,unit2Pulse_conv(AXIS_IDX_Z2,2000),unit2Pulse_conv(AXIS_IDX_Z2,hSp),NULL);
      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z3,PIN_Z3_SEN2,unit2Pulse_conv(AXIS_IDX_Z3,2000),unit2Pulse_conv(AXIS_IDX_Z3,hSp),NULL);
      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z4,PIN_Z4_SEN2,unit2Pulse_conv(AXIS_IDX_Z4,2000),unit2Pulse_conv(AXIS_IDX_Z4,hSp),NULL);
      

      int xyhSp=70;

      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_X,PIN_X_SEN1,unit2Pulse_conv(AXIS_IDX_X,-2000),unit2Pulse_conv(AXIS_IDX_X,xyhSp),NULL);
      if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Y,PIN_Y_SEN1,unit2Pulse_conv(AXIS_IDX_Y,-2000),unit2Pulse_conv(AXIS_IDX_Y,xyhSp),NULL);
      

      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z1,PIN_Z1_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z2,PIN_Z2_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z3,PIN_Z3_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_Z4,PIN_Z4_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);

      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_R1,PIN_R1_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_R2,PIN_R2_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_R3,PIN_R3_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);
      // if(retErr==0)retErr=MTPSYS_MachZeroRet(AXIS_IDX_R4,PIN_R4_SEN1,50000,MTPSYS_getMinPulseSpeed()*10,NULL);



      pulse_offset=(xVec){0};
      retStatus=statusReducer(retStatus,(retErr==0)?GCodeParser_Status::TASK_OK:GCodeParser_Status::TASK_FAILED);
      __PRT_D_("%s\n",retErr==0?"DONE":"FAILED");

    }
    else if(false && CheckHead(cblk, "G90"))//G90 absolute pos
    {
      __PRT_D_("G90 absolute pos\n");
      isAbsLoc=true;
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    else if(false && CheckHead(cblk, "G91"))//G91 relative pos
    {
      __PRT_D_("G91 relative pos\n");
      isAbsLoc=false;
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    else if(false && CheckHead(cblk, "G20"))//G20 Use Inch
    {
      unit_is_inch=true;
      __PRT_D_("G20 Use Inch\n");
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    else if(false && CheckHead(cblk, "G21"))//G21 Use mm
    {
      unit_is_inch=false;
      __PRT_D_("G21 Use mm\n");
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    
    else
    {
      __PRT_D_("XX G block:");
      for(int k=0;k<cblkL;k++)
      {
        __PRT_D_("%c",cblk[k]);
      }
      __PRT_D_("\n"); 
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }

  }
  else if(cblk[0]=='M')
  {
    //M42 [I<bool>] [P<pin>] S<state> [T<0|1|2|3>] marlin M42 Set Pin State
    //M42 [I<bool>] [P<pin>] S<state> [T<0|1|2|3>] CID_{string} TTAG_{string} TID_{int} //addtional info for trigger info replay
    if(     CheckHead(cblk, "M42 "))//PIN ctrl
    {//S<state> 0 input 1 output 2 INPUT_PULLUP 3 INPUT_PULLDOWN
      //M42 P2 S1  //set output
      //M42 P2 T1  //Set one
      
      if(isMTPLocked)
      {
        return GCodeParser_Status::TASK_FATAL_FAILED;
      }
      uint32_t PORT,S;
      int32_t P,T;
      if(FindUint32("PORT",blks,blkCount,PORT)!=0)PORT=0;
      if(FindUint32("S",blks,blkCount,S)!=0)S=0;
      if(FindInt32("P",blks,blkCount,P)!=0)P=-1;
      if(FindInt32("T",blks,blkCount,T)!=0)T=-1;
      

      char *CID_Ptr=NULL;
      char CID_BUF[50];
      if(FindStr("CID_",blks,blkCount,CID_BUF)==0)
      {
        CID_Ptr=CID_BUF;
      }
      char *TTAG_Ptr=NULL;
      char TTAG_BUF[100];
      if(FindStr("TTAG_",blks,blkCount,TTAG_BUF)==0)
      {
        TTAG_Ptr=TTAG_BUF;
      }

      int TID;
      if(FindInt32("TID_",blks,blkCount,TID)!=0)TID=-1;



      __PRT_D_("M42 PORT%d P%d S%d T%d CID:%s TTAG:%s\n",PORT,P,S,T,CID_Ptr,TTAG_Ptr);
      if(CID_Ptr!=NULL || P>=0 || PORT)
      {
        retStatus=MTPSYS_AddIOState(PORT,P,S,T,CID_Ptr,TTAG_Ptr,TID)==true?
          statusReducer(retStatus,GCodeParser_Status::TASK_OK):
          statusReducer(retStatus,GCodeParser_Status::TASK_FAILED);
      } 
      else
      {
        retStatus=statusReducer(retStatus,GCodeParser_Status::GCODE_PARSE_ERROR);
      }
    }
    else if(CheckHead(cblk, "M114"))//Get/reply current position
    {//
    
      if(p_jnote)
      {

        xVec curPulseLoc= _mstp->curPos_c;
        auto &jnote=*p_jnote;
        auto jobjM144 = jnote.createNestedObject("M114");
        // auto jobjStp = jobjM144.createNestedObject("step");
        // auto jobjUnit = jobjM144.createNestedObject("unit");
        for(int i=0;i<MSTP_VEC_SIZE;i++)
        {
          const char* gdx = axisIDX2GDX(i);
          if(gdx==NULL)continue;
          auto stepLoc=curPulseLoc.vec[i]-pulse_offset.vec[i];
          // jobjStp[gdx]=stepLoc;
          float unitLoc =Pulse2Unit_conv(i,stepLoc);
          
          jobjM144[gdx]=unitLoc;


        }
        retStatus=GCodeParser_Status::TASK_OK;
      }

    }
    else if(CheckHead(cblk, "M400.1"))//Wait for motion stops,blocking style
    {//TODO

      if(isMTPLocked)
      {
        return GCodeParser_Status::TASK_FATAL_FAILED;
      }
      while(1)
      {//wait
        if(_mstp->SegQ_IsEmpty())
        {
          retStatus=GCodeParser_Status::TASK_OK;
          break;
        }
        else
        {//to let variable updates between main thread and 
          yield();
          // delay(10);
        }
      }
    }
    else if(CheckHead(cblk, "M119"))//get input (includes Endstop) States as integer (state are in binary form)
    {

      if(p_jnote)
      {
        auto &jnote=*p_jnote;
        auto jobjRet = jnote.createNestedObject("M119");

        jobjRet["state"]=_mstp->latest_input_pins;
        jobjRet["last_hit"]=_mstp->endstopPins_hit;
        jobjRet["lock"]=_mstp->endStopHitLock;
        jobjRet["in_detection"]=_mstp->endStopDetection;

        jobjRet["endstop_pins"]=_mstp->endstopPins;
        jobjRet["endstop_pins_ns"]=_mstp->endstopPins_normalState;
        retStatus=GCodeParser_Status::TASK_OK;
      }


    }
    else if(CheckHead(cblk, "M120 "))//enable end stop
    {
      // uint32_t PORT;
      // if(FindUint32("PORT",blks,blkCount,PORT)==0)
      // {
      //   _mstp->endstopPins= PORT;
      // yield();
      // }
      // uint32_t PNS;
      // if(FindUint32("PNS",blks,blkCount,PNS)==0)
      // {
      //   _mstp->endstopPins_normalState= PNS;
      // yield();
      // }
      // yield();
      _mstp->endStopDetection=true;

      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);

    }
    else if(CheckHead(cblk, "M121"))//disable end stop
    {
      _mstp->endStopDetection=false;
      _mstp->endStopHitLock=false;
      _mstp->endstopPins_hit=0;
      
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
    }
    else if(false && CheckHead(cblk, "M226 "))//Wait for Pin State,M226 P<pin> [S<state>] [T<timeout>]
    {//TODO
    }
    else
    {
      __PRT_D_("XX M block:");
      for(int k=0;k<cblkL;k++)
      {
        __PRT_D_("%c",cblk[k]);
      }
      __PRT_D_("\n"); 
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }
  }
  else if(cblk[0]!=';' &&cblk[0]!='('  )
  {
    // __PRT_D_("XX block:");
    // for(int k=0;k<cblkL;k++)
    // {
    //   __PRT_D_("%c",cblk[k]);
    // }
    // __PRT_D_("\n"); 
    retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
  }

  return retStatus;

}


void GCodeParser_M::onError(int code)
{

}
