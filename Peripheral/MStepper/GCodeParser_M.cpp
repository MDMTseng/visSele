#pragma once

#include "GCodeParser_M.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
using namespace std;

#include "LOG.h"



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


float GCodeParser_M::unit2Pulse_conv(const char* code,float dist)
{
  if(code[0]=='Y')
  {
    return unit2Pulse(dist,1);
  }

  if(code[0]=='Z'&&code[1]=='1')
  {
    return round(unit2Pulse(dist,1));
  }
  if(code[0]=='F')
  {
    return unit2Pulse(dist,1);
  }


  return NAN;
}

float parseFloat(char* str,int strL)
{
  char strBuf[20];
  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';
  return atof(strBuf);
}
long parseLong(char* str,int strL)
{
  char strBuf[20];
  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';
  return atol(strBuf);
}


int GCodeParser_M::ReadxVecData(char **blkIdxes,int blkIdxesL,float *retVec)
{
  float num=NAN;
  int ret;
  ret = FindFloat("Z1_",blkIdxes,blkIdxesL,num);
  if(ret==0)retVec[0]=num;
  ret = FindFloat("Y",blkIdxes,blkIdxesL,num);
  if(ret==0)retVec[1]=num;
  return 0;
}

int GCodeParser_M::ReadxVecData(char **blkIdxes,int blkIdxesL,xVec &retVec)
{
  float vecBuff[MSTP_VEC_SIZE];
  

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    vecBuff[i]=NAN;//set NAN as unset
  }

  ReadxVecData(blkIdxes,blkIdxesL,vecBuff);
  
  if(vecBuff[0]==vecBuff[0])
  {
    uint32_t ipos=unit2Pulse_conv("Z1",vecBuff[0]);
    
    retVec.vec[0]=ipos;
  }
  if(vecBuff[1]==vecBuff[1])
  {
    uint32_t ipos=unit2Pulse_conv("Y",vecBuff[1]);
      
    retVec.vec[1]=ipos;
  }

  
  return 0;
}
int GCodeParser_M::ReadG1Data(char **blkIdxes,int blkIdxesL,xVec &vec,float &F)
{
  vec=isAbsLoc?MTPSYS_getLastLocInStepperSystem():(xVec){0};
  ReadxVecData(blkIdxes,blkIdxesL,vec);
  int didx=0;
  int j=0;

  float tmpF=latestF;
  int ret = FindFloat("F",blkIdxes,blkIdxesL,tmpF);
  float nF=unit2Pulse_conv("F",tmpF);
  if(nF==nF && nF>0)
  {
    latestF=nF;
  }
  F=latestF;

  return 0;
}


int GCodeParser_M::FindFloat(char *prefix,char **blkIdxes,int blkIdxesL,float &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        retNum=parseFloat(blk+prefixL,len-prefixL);
        return retNum==retNum?0:-2;
      }
    }

  }
  return -3;
}
int GCodeParser_M::FindInt32(char *prefix,char **blkIdxes,int blkIdxesL,int32_t &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        retNum=(int32_t)parseLong(blk+prefixL,len-prefixL);
        return 0;
      }
    }

  }
  return -1;
}


int GCodeParser_M::FindGMEnd_idx(char **blkIdxes,int blkIdxesL)
{
  int j=0;
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    
    if(blk[0]=='G'||blk[0]=='M')
    {
      // __PRT_D_("j:%d\n",j);
      return j;
    }
  }
  return j;
}


bool GCodeParser_M::CheckHead(char *str1,char *str2)
{
  return strncmp(str1, str2, strlen(str2))==0;
}


int GCodeParser_M::MTPSYS_MachZeroRet(uint32_t index,int distance,int speed,void* context)
{
  return _mstp->MachZeroRet(index,distance,speed,context);
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
  return _mstp->minSpeed;
}


bool GCodeParser_M::MTPSYS_AddWait(uint32_t period_ms,int times, void* ctx,MSTP_segment_extra_info *exinfo)
{
  return _mstp->AddWait(period_ms,times,ctx,exinfo);
}


bool GCodeParser_M::MTPSYS_AddIOState(int32_t I,int32_t P, int32_t S,int32_t T)
{

  return false;
}


GCodeParser::GCodeParser_Status GCodeParser_M::parseLine()
{
  
  __PRT_D_("==========CallBack========\n");
  line[lineCharCount]='\0';
  __PRT_D_(">>:%s\n",line);

  // for(int i=0;i<blockCount+1;i++)
  // {
  //   __PRT_D_("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
  // }

  if(blockCount<1)return GCodeParser_Status::LINE_EMPTY;
  GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;


  {//print comment
  
    int commentIdx=0;
    for(int i=0;i<blockCount;i++)
    {
      char *cblk=blockInitial[i];
      int cblkL=blockInitial[i+1]-blockInitial[i];
      if(cblk[0]=='('||cblk[0]==';')
      {
        if(commentIdx==0)
        {
          __PRT_D_("COMMENT========\n");
        }
        __PRT_D_("[%d]:",commentIdx);
        for(int k=0;k<cblkL;k++)
        {
          __PRT_D_("%c",cblk[k]);
        }
        __PRT_D_("\n"); 
        commentIdx++;
      }
    }
  }
  for(int i=0;i<blockCount;)
  {
    if(retStatus<0)break;
    char *cblk=blockInitial[i];
    int cblkL=blockInitial[i+1]-blockInitial[i];
    // __PRT_D_(">>head=>%c\n",cblk[0]);
    if(cblk[0]=='G')
    {
      if(CheckHead(cblk, "G28"))
      {
        __PRT_D_("G28 GO HOME!!!:");
        
        int retErr=0;
        if(retErr==0)retErr=MTPSYS_MachZeroRet(1,50000,MTPSYS_getMinPulseSpeed()*2,NULL);
        if(retErr==0)retErr=MTPSYS_MachZeroRet(0,500*2,MTPSYS_getMinPulseSpeed(),NULL);
        retStatus=statusReducer(retStatus,(retErr==0)?GCodeParser_Status::TASK_OK:GCodeParser_Status::TASK_FAILED);
        __PRT_D_("%s\n",retErr==0?"DONE":"FAILED");

      }
      else if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))//X Y Z A B C
      {
        __PRT_D_("G1 baby!!!\n");
        int j=i+1;
        xVec vec;
        float F;
        ReadG1Data(blockInitial+j,blockCount-j,vec,F);

        MSTP_segment_extra_info exinfo={.acc=NAN,.deacc=NAN};

        {
          
          float tmpF=NAN;
          if(FindFloat("ACC",blockInitial+j,blockCount-j,tmpF)==0)
          {
            exinfo.acc=unit2Pulse_conv("ACC",tmpF);
          }
          tmpF=NAN;
          
          if(FindFloat("DEA",blockInitial+j,blockCount-j,tmpF)==0)
          {
            exinfo.deacc=unit2Pulse_conv("DEA",tmpF);
          }
        }


        __PRT_D_("vec:%s F:%f\n",toStr(vec),F);
        if(isAbsLoc)
        {
          MTPSYS_VecTo(vecAdd(vec,pos_offset),F,NULL,&exinfo);
        }
        else
        {
          MTPSYS_VecAdd(vec,F,NULL,&exinfo);
        }
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
        i=FindGMEnd_idx(blockInitial+j,blockCount-j);
      }
      else if(CheckHead(cblk, "G90"))
      {
        __PRT_D_("G90 absolute pos\n");
        isAbsLoc=true;
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G91"))
      {
        __PRT_D_("G91 relative pos\n");
        isAbsLoc=false;
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G04")||CheckHead(cblk, "G4"))
      {
        __PRT_D_("G04 Pause\n");
        int j=i+1;
        int32_t P;
        int ret = FindInt32("P",blockInitial+j,blockCount-j,P);
        if(ret==0)
        {
          if(MTPSYS_AddWait((uint32_t)P,1, NULL,NULL)==true)
          {
            retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_FAILED);
          }
          else
          {
            retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
          }
        }
        else
        {
          retStatus=statusReducer(retStatus,GCodeParser_Status::GCODE_PARSE_ERROR);
        }
      }
      else if(CheckHead(cblk, "G20"))
      {
        unit_is_inch=true;
        __PRT_D_("G20 Use Inch\n");
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G21"))
      {
        unit_is_inch=false;
        __PRT_D_("G21 Use mm\n");
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G92"))
      {//TODO: should do from mm instead of impulse?
        __PRT_D_("G92 Set pos\n");

        int j=i+1;
        xVec vec;
        ReadxVecData(blockInitial+j,blockCount-j,vec);

        pos_offset=vecSub(MTPSYS_getLastLocInStepperSystem(),vec);
        __PRT_D_("vec:%s ",toStr(vec));
        __PRT_D_("pos_offset:%s\n",toStr(pos_offset));
        __PRT_D_("sys last tar loc:%s\n",toStr(MTPSYS_getLastLocInStepperSystem()));

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

      if(CheckHead(cblk, "M42"))//M42 [I<bool>] [P<pin>] S<state> [T<0|1|2|3>] marlin M42 Set Pin State
      {
        int j=i+1;
        int32_t I,P,S,T;
        if(FindInt32("I",blockInitial+j,blockCount-j,I)!=0)I=-1;
        if(FindInt32("P",blockInitial+j,blockCount-j,P)!=0)P=-1;
        if(FindInt32("S",blockInitial+j,blockCount-j,S)!=0)S=-1;
        if(FindInt32("T",blockInitial+j,blockCount-j,T)!=0)T=-1;


        __PRT_D_("M42 I%d P%d S%d T%d\n",I,P,S,T);
        if(P>=0)
        {
          retStatus=MTPSYS_AddIOState(I,P,S,T)==true?
            statusReducer(retStatus,GCodeParser_Status::TASK_OK):
            statusReducer(retStatus,GCodeParser_Status::TASK_FAILED);
        } 
        else
        {
          retStatus=statusReducer(retStatus,GCodeParser_Status::GCODE_PARSE_ERROR);
        }
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
      __PRT_D_("XX block:");
      for(int k=0;k<cblkL;k++)
      {
        __PRT_D_("%c",cblk[k]);
      }
      __PRT_D_("\n"); 
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }

    i+=FindGMEnd_idx(blockInitial+i+1,blockCount-(i+1))+1;


  }



  // for(int i=0;i<blockCount;i++)
  // {
  //   int startIdx = blockInitial[i];
  //   int endIdx = blockInitial[i+1];
  //   // __PRT_D_("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
    




  //   for(int j=startIdx;j<endIdx;j++)
  //   {
  //     __PRT_D_("%c",line[j]);
  //   }
  //   __PRT_D_("\n");
  // }


  // for(int i=0;i<blockCount;i++)
  // {
  //   int startIdx = blockInitial[i];
  //   int endIdx = blockInitial[i+1];
  //   // __PRT_D_("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
    




  //   for(int j=startIdx;j<endIdx;j++)
  //   {
  //     __PRT_D_("%c",line[j]);
  //   }
  //   __PRT_D_("\n");
  // }
  INIT();//if call INIT here then, the sync method would not work

  return retStatus;
}
void GCodeParser_M::onError(int code)
{

}
