#include "GCodeParser.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



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