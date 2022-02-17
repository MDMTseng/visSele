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


float GCodeParser_M::unit2Pulse_axis(int axis,float dist)
{
  switch(axis)
  {
    case 0:return unit2Pulse(dist,SUBDIV/mm_PER_REV);
    case 1:return unit2Pulse(dist,SUBDIV/mm_PER_REV);

  }
  return NAN;
}

float GCodeParser_M::parseFloat(char* str,int strL)
{
  char strBuf[20];
  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';
  return atof(strBuf);
}


int GCodeParser_M::ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,float *retVec)
{
  int didx=0;
  int j=0;
  for(;j<blkIdxesL;j++)
  {
    char* blk=line+blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // printf("j:%d\n",j);
      return j;
    }
    
    // printf("[%d]:",didx++);
    for(int k=0;k<len;k++)
    {
      // printf("%c",blk[k]);
      if(CheckHead(blk, "Y"))
      {
        blk+=1;len-=1;
        
        retVec[0] = parseFloat(blk,len);
      }
      else if(CheckHead(blk, "Z1_"))
      {
        blk+=3;len-=3;
        retVec[1] = parseFloat(blk,len);
      }
    }

  }
  return j;
}

int GCodeParser_M::ReadxVecData(char* line, int *blkIdxes,int blkIdxesL,xVec &retVec)
{
  float vecBuff[MSTP_VEC_SIZE];
  

  for(int i=0;i<MSTP_VEC_SIZE;i++)
  {
    vecBuff[i]=NAN;//set NAN as unset
  }

  int retj = ReadxVecData(line,blkIdxes,blkIdxesL,vecBuff);
  
  if(vecBuff[0]==vecBuff[0])
  {
    uint32_t ipos=unit2Pulse_axis(0,vecBuff[0]);
    
    retVec.vec[0]=ipos;
  }
  if(vecBuff[1]==vecBuff[1])
  {
    uint32_t ipos=unit2Pulse_axis(1,vecBuff[1]);
      
    retVec.vec[1]=ipos;
  }

  
  return retj;
}
int GCodeParser_M::ReadG1Data(char* line, int *blkIdxes,int blkIdxesL,xVec &vec,float &F)
{
  vec=isAbsLoc?_mstp->lastTarLoc:(xVec){0};
  ReadxVecData(line, blkIdxes,blkIdxesL,vec);
  int didx=0;
  int j=0;
  F=latestF;
  for(;j<blkIdxesL;j++)
  {
    char* blk=line+blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='G'||blk[0]=='M')
    {
      // printf("j:%d\n",j);
      return j;
    }
    
    // printf("[%d]:",didx++);
    for(int k=0;k<len;k++)
    {
      if(CheckHead(blk, "F"))
      {
        blk+=1;len-=1;
        float nF=unit2Pulse(parseFloat(blk,len),SUBDIV/mm_PER_REV);
        if(nF==nF && nF>0)
        {
          latestF = nF;
          F=latestF;
        }
      }
      // printf("%c",blk[k]);
    }
    // printf("<\n");

  }
  // printf("F:%f\n",F);
  // printf("j:%d\n",j);
  return j;
}


bool GCodeParser_M::CheckHead(char *str1,char *str2)
{
  return strncmp(str1, str2, strlen(str2))==0;
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
        if(retErr==0)retErr=_mstp->MachZeroRet(1,50000,_mstp->minSpeed*2,NULL);
        if(retErr==0)retErr=_mstp->MachZeroRet(0,500*2,_mstp->minSpeed,NULL);
        retStatus=statusReducer(retStatus,(retErr==0)?GCodeParser_Status::TASK_OK:GCodeParser_Status::TASK_FAILED);
        printf("%s\n",retErr==0?"DONE":"FAILED");

      }
      else if(CheckHead(cblk, "G01 ")||CheckHead(cblk, "G1 "))//X Y Z A B C
      {
        printf("G1 baby!!!\n");
        int j=i+1;
        xVec vec;
        float F;
        i=ReadG1Data(line,blockInitial+j,blockCount-j,vec,F);
        printf("vec:%s F:%f\n",toStr(vec),F);
        if(isAbsLoc)
        {
          _mstp->VecTo(vecAdd(vec,pos_offset),F);
        }
        else
        {
          _mstp->VecAdd(vec,F);
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
        retStatus=statusReducer(retStatus,GCodeParser_Status::TASK_UNSUPPORTED);
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
        i= ReadxVecData(line,blockInitial+j,blockCount-j,vec);

        printf("vec:%s\n",toStr(vec));
        printf("sys last tar loc:%s\n",toStr(_mstp->lastTarLoc));

        pos_offset=vecSub(_mstp->lastTarLoc,vec);
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
