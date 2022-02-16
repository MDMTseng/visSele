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

    if(semicolonFound)//if there is a semi colon, skip to the end
    {
      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=lineCharCount+1;
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
        blockInitial[blockCount]=lineCharCount+1;
        parseLine();
        return true;
      }
      break;
    }


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
          blockInitial[blockCount]=lineCharCount+1;
          parseLine();
          return true;
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

        blockInitial[blockCount++]=lineCharCount;
        semicolonFound=true;
        break;
      }

      if(c=='(')
      {

        blockInitial[blockCount++]=lineCharCount;
        headParenthesesFound=true;
        break;
      }

      if(c=='\0' || c=='\n')
      {
        blockInitial[blockCount]=lineCharCount+1;
        parseLine();
       
        return true;
      }
    }


  } while (0);

  // printf("[%d]:%c b:%d   ",lineCharCount,c,blockCount);
  // printf("p:%d s:%d  \n",headParenthesesFound,semicolonFound);
  lineCharCount++;
  return false;
}