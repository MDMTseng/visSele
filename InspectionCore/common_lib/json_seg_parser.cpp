#include "json_seg_parser.hpp"
#include "stdio.h"


json_seg_parser::json_seg_parser(){
    reset();
}
void json_seg_parser::reset()
{
    stackSize=0;
}



json_seg_parser::JSonState json_seg_parser::getStackHead(int idx)
{
  if(stackSize==0)return JSonState::NUL;
  if(idx>=stackSize)return  JSonState::ERR;
  return levelStack[stackSize-1-idx];
}
bool json_seg_parser::pushStackHead(JSonState st)
{
  if(stackSize==(sizeof(levelStack)/sizeof(levelStack[0])))return false;//full
  levelStack[stackSize++]=st;
  return true;
}
bool json_seg_parser::popStackHead()
{
  if(stackSize==0)return false;
  stackSize--;
  return true;
}

inline bool isSpace(char ch)
{
  return (ch==' ' || ch=='\n' ||ch=='\t' );
}

json_seg_parser::RESULT json_seg_parser::newChar(char ch){

  
  // printf("stack[%d]:",stackSize);
  // for(int i=0;i<stackSize;i++)
  // {
  //   printf("%d,",levelStack[i]);
  // }
  // printf("\n");

  switch(getStackHead())
  {
    case OBJ_KEY:
    {
      if(ch=='"')
      {
        popStackHead();
        pushStackHead(JSonState::STR);
        return RESULT::KEY_START;
      }
      else if(isSpace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR_SEC;
      }

    }
    case OBJ_SEP:
    {
      if(ch==':')
      {
        popStackHead();
        return RESULT::WAIT_NEXT;//KEY_END;
      }
      else if(isSpace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR_SEC;
      }
    }

    case NUL:
      if(ch=='{')
      {
        pushStackHead(JSonState::OBJ_END);
        pushStackHead(JSonState::DAT);
        pushStackHead(JSonState::OBJ_SEP);
        pushStackHead(JSonState::OBJ_KEY);
        return RESULT::OBJECT_START;
      }
      else if(ch=='[')
      {
        pushStackHead(JSonState::ARR_END);
        pushStackHead(JSonState::DAT);
        return RESULT::ARRAY_START;
      }
      else if(isSpace(ch) )
      {
        return RESULT::WAIT_NEXT;
      }
      else 
      {
        return RESULT::ERROR_SEC;
      }
    break;
    case DAT:
      if(isSpace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else if(ch=='{')
      {
        popStackHead();
        pushStackHead(JSonState::OBJ_END);
        pushStackHead(JSonState::DAT);
        pushStackHead(JSonState::OBJ_SEP);
        pushStackHead(JSonState::OBJ_KEY);
        return RESULT::OBJECT_START;
      }
      else if(ch=='[')
      {
        popStackHead();
        pushStackHead(JSonState::ARR_END);
        pushStackHead(JSonState::DAT);
        return RESULT::ARRAY_START;
      }
      else if(ch=='"')
      {
        popStackHead();
        pushStackHead(JSonState::STR);
        return RESULT::STR_START;
      }
      else 
      {
        popStackHead();
        pushStackHead(JSonState::VAL);
        return RESULT::VAL_START;
      }






    break;
    case OBJ_END:
      if(ch=='}')
      {
        popStackHead();
        return RESULT::OBJECT_COMPLETE;
      }
      else if(ch==',')
      {
        
        pushStackHead(JSonState::DAT);
        pushStackHead(JSonState::OBJ_SEP);
        pushStackHead(JSonState::OBJ_KEY);
        return RESULT::WAIT_NEXT;
      }
      else if(isSpace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR_SEC;
      }

    break;
    case ARR_END:
      if(ch==']')
      {
        popStackHead();
        return RESULT::ARRAY_COMPLETE;
      }
      else if(ch==',')
      {
        
        pushStackHead(JSonState::DAT);
        return RESULT::WAIT_NEXT;
      }
      else if(isSpace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR_SEC;
      }
    break;
    case STR:
      if(ch=='"')//end STR
      {
        popStackHead();
        if(getStackHead()==JSonState::OBJ_SEP)
        {
          return RESULT::KEY_END;
        }
        return RESULT::STR_END;
      }
      return RESULT::WAIT_NEXT;
    break;
    case VAL:

      if(ch=='}')
      {
        if(getStackHead(1)==JSonState::OBJ_END)
        {
          popStackHead();
          return newChar(ch);//instant run again
        }
        else
        {//shouldn't be here
          return RESULT::ERROR_SEC;
        }
      }
      else if(ch==']')
      {
        if(getStackHead(1)==JSonState::ARR_END)
        {
          popStackHead();
          return newChar(ch);//instant run again
        }
        else
        {//shouldn't be here
          return RESULT::ERROR_SEC;
        }
      }
      else if(ch==',')
      {


        if(getStackHead(1)==JSonState::ARR_END)//only if it's arr
        {
          popStackHead();
          
          pushStackHead(JSonState::DAT);//keep find next data
          return newChar(ch);//instant run again
        }
        if(getStackHead(1)==JSonState::OBJ_END)//only if it's arr
        {
          popStackHead();
          
          return  newChar(ch);
        }
        else
        {
          return RESULT::ERROR_SEC;
        }
      }
      else
      {
        return RESULT::WAIT_NEXT;
      }



    break;
  }

  return RESULT::ERROR_SEC;
}




