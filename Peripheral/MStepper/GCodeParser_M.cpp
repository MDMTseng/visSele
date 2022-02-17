#pragma once

#include "GCodeParser_M.hpp"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
using namespace std;



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
  // printf("unit:%f subdiv:%d  mm_per_rev:%f\n",unit,subdiv,mm_per_rev);
  return dist*pulses_per_mm;
}


float GCodeParser_M::unit2Pulse_conv(const char* code,float dist)
{
  if(code[0]=='Y')
  {
    return unit2Pulse(dist,1);
  }

  if(code[0]=='Z'&&code[0]=='1')
  {
    return unit2Pulse(dist,1);
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


int GCodeParser_M::ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,float *retVec)
{
  float num=NAN;
  int ret;
  ret = FindFloat("Z1_",line, blkIdxes,blkIdxesL,num);
  if(ret==0)retVec[0]=num;
  ret = FindFloat("Y",line, blkIdxes,blkIdxesL,num);
  if(ret==0)retVec[1]=num;
  return 0;
}

int GCodeParser_M::ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,xVec &retVec)
{
  float vecBuff[MSTP_VEC_SIZE];
  

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    vecBuff[i]=NAN;//set NAN as unset
  }

  ReadxVecData(line,blkIdxes,blkIdxesL,vecBuff);
  
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
int GCodeParser_M::ReadG1Data(char* line, int *blkIdxes,int blkIdxesL,xVec &vec,float &F)
{
  vec=isAbsLoc?MTPSYS_getLastLocInStepperSystem():(xVec){0};
  ReadxVecData(line, blkIdxes,blkIdxesL,vec);
  int didx=0;
  int j=0;
  F=latestF;

  float tmpF=latestF;
  int ret = FindFloat("F",line, blkIdxes,blkIdxesL,tmpF);
  float nF=unit2Pulse_conv("F",tmpF);
  if(nF==nF && nF>0)
  {
    F=latestF=nF;
  }

  return 0;
}


int GCodeParser_M::FindFloat(char *prefix,char* line, int *blkIdxes,int blkIdxesL,float &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=line+blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // printf("j:%d\n",j);
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
int GCodeParser_M::FindInt32(char *prefix,char* line, int *blkIdxes,int blkIdxesL,int32_t &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=line+blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // printf("j:%d\n",j);
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


int GCodeParser_M::FindGMEnd_idx(char* line, int *blkIdxes,int blkIdxesL)
{
  int j=0;
  for(;j<blkIdxesL;j++)
  {
    char* blk=line+blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    
    if(blk[0]=='G'||blk[0]=='M')
    {
      // printf("j:%d\n",j);
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
  return _mstp->VecTo(vecAdd(VECTo,pos_offset),speed,ctx,exinfo);
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



GCodeParser::GCodeParser_Status GCodeParser_M::parseLine()
{
  
  printf("==========CallBack========\n");
  line[lineCharCount]='\0';
  printf(">>:%s\n",line);

  // for(int i=0;i<blockCount+1;i++)
  // {
  //   printf("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
  // }

  if(blockCount<1)return GCodeParser_Status::LINE_EMPTY;
  GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;


  {//print comment
  
    int commentIdx=0;
    for(int i=0;i<blockCount;i++)
    {
      char *cblk=line+blockInitial[i];
      int cblkL=blockInitial[i+1]-blockInitial[i];
      if(cblk[0]=='('||cblk[0]==';')
      {
        if(commentIdx==0)
        {
          printf("COMMENT========\n");
        }
        printf("[%d]:",commentIdx);
        for(int k=0;k<cblkL;k++)
        {
          printf("%c",cblk[k]);
        }
        printf("\n"); 
        commentIdx++;
      }
    }
  }
  for(int i=0;i<blockCount;i++)
  {
    if(retStatus<0)break;
    char *cblk=line+blockInitial[i];
    int cblkL=blockInitial[i+1]-blockInitial[i];
    // printf(">>head=>%c\n",cblk[0]);
    if(cblk[0]=='G')
    {
      if(CheckHead(cblk, "G28"))
      {
        printf("G28 GO HOME!!!:");
        
        int retErr=0;
        if(retErr==0)retErr=MTPSYS_MachZeroRet(1,50000,MTPSYS_getMinPulseSpeed()*2,NULL);
        if(retErr==0)retErr=MTPSYS_MachZeroRet(0,500*2,MTPSYS_getMinPulseSpeed(),NULL);
        retStatus=statusReducer(retStatus,(retErr==0)?GCodeParser_Status::TASK_OK:GCodeParser_Status::TASK_FAILED);
        printf("%s\n",retErr==0?"DONE":"FAILED");

      }
      else if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))//X Y Z A B C
      {
        printf("G1 baby!!!\n");
        int j=i+1;
        xVec vec;
        float F;
        ReadG1Data(line,blockInitial+j,blockCount-j,vec,F);


        printf("vec:%s F:%f\n",toStr(vec),F);
        if(isAbsLoc)
        {
          MTPSYS_VecTo(vecAdd(vec,pos_offset),F);
        }
        else
        {
          MTPSYS_VecAdd(vec,F);
        }
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G90"))
      {
        printf("G90 absolute pos\n");
        isAbsLoc=true;
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G91"))
      {
        printf("G91 relative pos\n");
        isAbsLoc=false;
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G04")||CheckHead(cblk, "G4"))
      {
        printf("G04 Pause\n");
        int j=i+1;
        int32_t P;
        int ret = FindInt32("P",line,blockInitial+j,blockCount-j,P);
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
        printf("G20 Use Inch\n");
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G21"))
      {
        unit_is_inch=false;
        printf("G21 Use mm\n");
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else if(CheckHead(cblk, "G92"))
      {//TODO: should do from mm instead of impulse?
        printf("G92 Set pos\n");

        int j=i+1;
        xVec vec;
        ReadxVecData(line,blockInitial+j,blockCount-j,vec);

        printf("vec:%s\n",toStr(vec));
        printf("sys last tar loc:%s\n",toStr(MTPSYS_getLastLocInStepperSystem()));

        pos_offset=vecSub(MTPSYS_getLastLocInStepperSystem(),vec);
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
      }
      else
      {
        printf("XX G block:");
        for(int k=0;k<cblkL;k++)
        {
          printf("%c",cblk[k]);
        }
        printf("\n"); 
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
      }

    }
    else if(line[blockInitial[i]]=='M')
    {

      if(CheckHead(cblk, "M42"))//M42 [I<bool>] [P<pin>] S<state> [T<0|1|2|3>] marlin M42 Set Pin State
      {
        printf("G04 Pause\n");
        int j=i+1;
        int32_t I,P,S,T;
        if(FindInt32("I",line,blockInitial+j,blockCount-j,I)!=0)I=-1;
        if(FindInt32("P",line,blockInitial+j,blockCount-j,P)!=0)P=-1;
        if(FindInt32("S",line,blockInitial+j,blockCount-j,S)!=0)S=-1;
        if(FindInt32("T",line,blockInitial+j,blockCount-j,T)!=0)T=-1;


        if(S>=0)
        {

          retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_OK);
        }
        else
        {
          retStatus=statusReducer(retStatus,GCodeParser_Status::GCODE_PARSE_ERROR);
        }
      }
      else
      {
        printf("XX M block:");
        for(int k=0;k<cblkL;k++)
        {
          printf("%c",cblk[k]);
        }
        printf("\n"); 
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
      }
    }
    else if(line[blockInitial[i]]!=';' &&line[blockInitial[i]]!='('  )
    {
      printf("XX block:");
      for(int k=0;k<cblkL;k++)
      {
        printf("%c",cblk[k]);
      }
      printf("\n"); 
      retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
    }

  }



  // for(int i=0;i<blockCount;i++)
  // {
  //   int startIdx = blockInitial[i];
  //   int endIdx = blockInitial[i+1];
  //   // printf("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
    




  //   for(int j=startIdx;j<endIdx;j++)
  //   {
  //     printf("%c",line[j]);
  //   }
  //   printf("\n");
  // }


  // for(int i=0;i<blockCount;i++)
  // {
  //   int startIdx = blockInitial[i];
  //   int endIdx = blockInitial[i+1];
  //   // printf("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
    




  //   for(int j=startIdx;j<endIdx;j++)
  //   {
  //     printf("%c",line[j]);
  //   }
  //   printf("\n");
  // }
  INIT();//if call INIT here then, the sync method would not work

  return retStatus;
}
void GCodeParser_M::onError(int code)
{

}
