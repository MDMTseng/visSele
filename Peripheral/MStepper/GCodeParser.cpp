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



bool GCodeParser::addChar(char c)
{
  line[lineCharCount]=c;
  do
  {
    if(isInSpace)
    {
      if(c!=' ')
      {
        blockInitial[blockCount++]=lineCharCount;
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
          blockInitial[blockCount]=lineCharCount;
          parseLine();
          return true;
        }

      }
      else
      {
        lineCharCount--;
      }

      break;
    }
    else
    {
      if(c==' ')
      {
        isInSpace=true;
        break;
      }

      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=lineCharCount;
        parseLine();
       
        return true;
      }
    }


    if(semicolonFound)
    {
      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=lineCharCount;
        parseLine();
        return true;
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
        blockInitial[blockCount]=lineCharCount;
        parseLine();
        return true;
      }
      break;
    }


  } while (0);

  // printf("[%d]:%c b:%d   ",lineCharCount,c,blockCount);
  // printf("p:%d s:%d  \n",headParenthesesFound,semicolonFound);
  lineCharCount++;
  return false;
}