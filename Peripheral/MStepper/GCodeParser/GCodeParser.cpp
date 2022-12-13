#include "GCodeParser.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
using namespace std;


void GCodeParser::INIT()
{
	lineCharCount=0;
  blockCount=0;

  semicolonFound=false;
  headParenthesesFound=false;
  isInSpace=true;

}


GCodeParser::GCodeParser()
{
  INIT();
}

GCodeParser::GCodeParser_Status GCodeParser::statusReducer(GCodeParser::GCodeParser_Status st,GCodeParser::GCodeParser_Status new_st)
{
  if(st>new_st)
  {
    return new_st;
  }
  return st;
}

GCodeParser::GCodeParser_Status GCodeParser::runLine(const char *line)
{
  for(int i=0;;i++)
  {
    char c=line[i];
    GCodeParser_Status st = addChar(c);
    if(st!=GCodeParser_Status::LINE_INCOMPLETE)
      return st;
    if(c=='\0' || c=='\n')break;
  }
  return GCodeParser_Status::LINE_INCOMPLETE;
}

GCodeParser::GCodeParser_Status GCodeParser::addChar(char c)
{
  line[lineCharCount]=c;
  do
  {

    if(semicolonFound)//if there is a semi colon, skip to the end
    {
      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=line+lineCharCount+1;
        return parseLine();
      }
      break;
    }

    if(headParenthesesFound)
    {
      if(c==')')
      {
        headParenthesesFound=false;
        isInSpace=true;
      }
      else if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=line+lineCharCount+1;
        return parseLine();
      }
      break;
    }


    if(isInSpace)
    {
      if(c!=' ')
      {
        blockInitial[blockCount++]=line+lineCharCount;
        isInSpace=false;
        if(c=='(')
        {
          headParenthesesFound=true;
        }
        if(c==';')
        {
          semicolonFound=true;
        }
        if(c=='\0' || c=='\n')
        {
          blockInitial[blockCount]=line+lineCharCount+1;
          return parseLine();
        }

      }
      else//multiple spaces skip this one
      {
        lineCharCount--;
      }

      break;
    }
    else
    {
      // printf(">>%c\n",c);
      if(c==' ')
      {
        isInSpace=true;
        break;
      }
      if(c==';')
      {

        blockInitial[blockCount++]=line+lineCharCount;
        semicolonFound=true;
        break;
      }

      if(c=='(')
      {

        blockInitial[blockCount++]=line+lineCharCount;
        headParenthesesFound=true;
        break;
      }

      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=line+lineCharCount+1;
        return parseLine();
      }
    }


  } while (0);

  // printf("[%d]:%c b:%d   ",lineCharCount,c,blockCount);
  // printf("p:%d s:%d  \n",headParenthesesFound,semicolonFound);
  lineCharCount++;
  return GCodeParser_Status::LINE_INCOMPLETE;
}



int GCodeParser::FindGMEnd_idx(char **blkIdxes,int blkIdxesL)
{
  int j=0;
  for(;j<blkIdxesL;j++)
  {
    char* blk=blkIdxes[j];
    int len=blkIdxes[j+1]-blkIdxes[j]-1;
    
    if(blk[0]=='\n'||blk[0]=='\r')
    {
      // __PRT_D_("j:%d\n",j);
      return j;
    }
  }
  return j;
}


GCodeParser::GCodeParser_Status GCodeParser::parseLine()
{
  
  line[lineCharCount]='\0';

  // for(int i=0;i<blockCount+1;i++)
  // {
  //   __PRT_D_("blk[%d]:%d =>%c\n",i,blockInitial[i],line[blockInitial[i]]);
  // }

  if(blockCount<1)return GCodeParser_Status::LINE_EMPTY;

  GCodeParser_Status retStatus=GCodeParser_Status::LINE_EMPTY;

  // {//print comment
  
  //   int commentIdx=0;
  //   for(int i=0;i<blockCount;i++)
  //   {
  //     char *cblk=blockInitial[i];
  //     int cblkL=blockInitial[i+1]-blockInitial[i];
  //     if(cblk[0]=='('||cblk[0]==';')
  //     {
  //       if(commentIdx==0)
  //       {
  //         __PRT_D_("COMMENT========\n");
  //       }
  //       __PRT_D_("[%d]:",commentIdx);
  //       for(int k=0;k<cblkL;k++)
  //       {
  //         __PRT_D_("%c",cblk[k]);
  //       }
  //       __PRT_D_("\n"); 
  //       commentIdx++;
  //     }
  //   }
  // }
  for(int i=0;i<blockCount;)
  {
    if(retStatus<0)break;
    // char *cblk=blockInitial[i];
    // int cblkL=blockInitial[i+1]-blockInitial[i];
    // __PRT_D_(">>head=>%c\n",cblk[0]);
    // G_LOG(cblk);
    // char *cblk=blockInitial[i];
    // int cblkL=blockInitial[i+1]-blockInitial[i];
    // __PRT_D_(">>head=>%c\n",cblk[0]);
    
    retStatus= parseCMD(blockInitial+i, blockCount-i);

    i+=FindGMEnd_idx(blockInitial+i+1,blockCount-(i+1))+1;


    // G_LOG("BLK_END");
  }


  INIT();//if call INIT here then, the sync method would not work

  return retStatus;
}




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


  uint32_t num;
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






bool CheckHead(const char *str1,const char *str2)
{
  return strncmp(str1, str2, strlen(str2))==0;
}