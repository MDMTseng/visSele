#include "GCodeParserUtil.hpp"





float parseFloat(char* str,int strL)
{
  if(str==NULL || strL<=0)return NAN;

  char strBuf[30];

  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';



  char *ep = NULL;
  double f = strtod (strBuf, &ep);

  if (!ep  ||  *ep)
      return NAN;  // has non-floating digits after number, if any

  return f;
}
long parseLong(char* str,int strL, bool *isOK)
{
  if(isOK)*isOK=false;
  char strBuf[30];
  if(strL>(sizeof(strBuf)-1))return 0;
  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';

  char testChar=strBuf[0];
  if(testChar!='-'&&testChar!='+' && (testChar<'0' || testChar>'9'))return 0;//head is wrong

  for(int i=1;i<strL;i++)
  {
    testChar=strBuf[i];
    if((testChar<'0' || testChar>'9'))return 0;//not a number
  }


  if(isOK)*isOK=true;
  return atol(strBuf);
}

uint32_t parseUlong(char* str,int strL, bool *isOK)
{
  if(isOK)*isOK=false;
  char strBuf[30];
  memcpy(strBuf,str,strL);
  strBuf[strL]='\0';

  char testChar=strBuf[0];


  uint32_t num=0;
  for(int i=0;i<strL;i++)
  {
    testChar=strBuf[i];
    if((testChar<'0' || testChar>'9'))return 0;//not a number
    num*=10;
    num+=testChar-'0';
  }


  if(isOK)*isOK=true;
  return num;
}


int FindFloat(const char *prefix,char **blkIdxes,int blkIdxesL,float &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    // for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
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
int FindInt32(const char *prefix,char **blkIdxes,int blkIdxesL,int32_t &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    // for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        bool isOK=false;
        retNum=(int32_t)parseLong(blk+prefixL,len-prefixL,&isOK);
        return isOK?0:-1;
      }
    }

  }
  return -1;
}

int FindUint32(const char *prefix,char **blkIdxes,int blkIdxesL,uint32_t &retNum)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    // for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        bool isOK=false;
        retNum=parseUlong(blk+prefixL,len-prefixL,&isOK);
        return isOK?0:-1;
      }
    }

  }
  return -1;
}

bool FindExist(const char *prefix,char **blkIdxes,int blkIdxesL)
{
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return false;
    }
    
    // for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        if(blk[prefixL]==' ' || blk[prefixL]=='\n' || blk[prefixL]=='\0' )
          return true;
        return false;
      }
    }

  }
  return false;
}


int FindStr(const char *prefix,char **blkIdxes,int blkIdxesL,char* retStr)
{
  retStr[0]='\0';
  int j=0;
  int prefixL=strlen(prefix);
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    if(blk[0]=='('||blk[0]==';')continue;//skip comment
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return -1;
    }
    
    // for(int k=0;k<len;k++)//single block G21 or P4355 or X-34
    {
      if(strncmp(blk, prefix, prefixL)==0)
      {
        memcpy(retStr,blk+prefixL,len-prefixL);
        retStr[len-prefixL]='\0';
        return 0;
      }
    }

  }
  return -1;
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



int ReadxVecELE_toPulses(char **blkIdxes,int blkIdxesL,xVec_f &retVec,int axisIdx,const char* axisGIDX)
{

  float num;
  auto &v=retVec.vec[axisIdx];
  // v=NAN;
  int ret = FindFloat(axisGIDX,blkIdxes,blkIdxesL,num);
  if(ret)return ret;
  v=num;
  return 0;
}


int ReadxVec_fData(char **blkIdxes,int blkIdxesL,xVec_f &retVec)
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

MSTP_segment_extra_info ReadSegment_extra_info(char **blkIdxes,int blkIdxesL)
{

  MSTP_segment_extra_info sei={0};
  sei.speed=NAN;
  sei.acc=NAN;
  sei.deacc=NAN;
  sei.speedOnAxisIdx=-1;

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_FEEDRATE,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      sei.speed=tmpF;
    }
  }

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_ACCELERATION,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      sei.acc=tmpF;
    }
  }


  if(sei.acc<0)
  {
    sei.acc=-sei.acc;
  }

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_DEACCELERATION,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      sei.deacc=tmpF;
    }
  }

  if(sei.deacc!=sei.deacc)
  {
    sei.deacc=sei.acc;
  }

  if(sei.deacc>0)
  {
    sei.deacc=-sei.deacc;
  }



  {

    char AxisCode[10];
    if(FindStr(AXIS_GDX_FEED_ON_AXIS,blkIdxes,blkIdxesL,AxisCode)==0)
    {
      sei.speedOnAxisIdx=axisGDX2IDX(AxisCode,-1);
    }
  }


  {
    float tmpF=NAN;
    int ret = FindFloat("RCor",blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      if(tmpF>0 && tmpF<=1)
      {
        sei.cornorR=tmpF;
      }
    }
    else
    {
      tmpF=NAN;
      ret = FindFloat("RCmm",blkIdxes,blkIdxesL,tmpF);
      if(ret==0)
      {
        if(tmpF>1)
        {
          sei.cornorR=tmpF;
        }
      }
    }
  }



  return sei;
}

int ReadGVecData(char **blkIdxes,int blkIdxesL,xVec_f &vec,MSTP_segment_extra_info *moveInfo)
{

  // vec=vecSub(MTPSYS_getLastLocInStepperSystem(),pulse_offset);
  ReadxVec_fData(blkIdxes,blkIdxesL,vec);

  if(moveInfo==NULL)return 0;

  moveInfo->speed=NAN;
  moveInfo->acc=NAN;
  moveInfo->deacc=NAN;
  moveInfo->speedOnAxisIdx=-1;

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_FEEDRATE,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      moveInfo->speed=tmpF;
    }
  }

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_ACCELERATION,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      moveInfo->acc=tmpF;
    }
  }

  {
    float tmpF=NAN;
    int ret = FindFloat(AXIS_GDX_DEACCELERATION,blkIdxes,blkIdxesL,tmpF);
    if(ret==0)
    {
      moveInfo->acc=tmpF;
    }
  }


  {

    char AxisCode[10];
    if(FindStr(AXIS_GDX_FEED_ON_AXIS,blkIdxes,blkIdxesL,AxisCode)==0)
    {
      moveInfo->speedOnAxisIdx=axisGDX2IDX(AxisCode,-1);
    }
  }



  return 0;
}



