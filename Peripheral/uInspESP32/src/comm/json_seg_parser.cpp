#include "comm/json_seg_parser.hpp"
#include "stdio.h"

#include <cstddef>

json_seg_parser::json_seg_parser(){
    reset();
}
void json_seg_parser::reset()
{
    stackSize=0;
    strEscapePending=false;
}



json_seg_parser::JSonState json_seg_parser::getStackHead(int idx)
{
  if(stackSize==0)return JSonState::NUL;
  if(idx<0)return JSonState::ERR;
  std::size_t offset=static_cast<std::size_t>(idx);
  if(offset>=stackSize)return  JSonState::ERR;
  return levelStack[stackSize-1-offset];
}
bool json_seg_parser::pushStackHead(JSonState st)
{
  if(stackSize>=kMaxStackDepth)return false;//full
  levelStack[stackSize++]=st;
  return true;
}
bool json_seg_parser::popStackHead()
{
  if(stackSize==0)return false;
  stackSize--;
  return true;
}

bool json_seg_parser::isWhitespace(char ch)
{
  switch (ch)
  {
    case ' ':
    case '\n':
    case '\t':
    case '\r':
    case '\f':
    case '\v':
      return true;
    default:
      return false;
  }
}

json_seg_parser::RESULT json_seg_parser::newChar(char ch){

  
  switch(getStackHead())
  {
    case OBJ_KEY:
    {
      if(ch=='"')
      {
        if(!popStackHead() || !pushStackHead(JSonState::STR))
        {
          return RESULT::ERROR;
        }
        strEscapePending=false;
        return RESULT::KEY_START;
      }
      else if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR;
      }

    }
    case OBJ_SEP:
    {
      if(ch==':')
      {
        if(!popStackHead())
        {
          return RESULT::ERROR;
        }
        return RESULT::WAIT_NEXT;//KEY_END;
      }
      else if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR;
      }
    }

    case NUL:
      if(ch=='{')
      {
        if(!pushStackHead(JSonState::OBJ_END) ||
           !pushStackHead(JSonState::DAT) ||
           !pushStackHead(JSonState::OBJ_SEP) ||
           !pushStackHead(JSonState::OBJ_KEY))
        {
          return RESULT::ERROR;
        }
        return RESULT::OBJECT_START;
      }
      else if(ch=='[')
      {
        if(!pushStackHead(JSonState::ARR_END) ||
           !pushStackHead(JSonState::DAT))
        {
          return RESULT::ERROR;
        }
        return RESULT::ARRAY_START;
      }
      else if(isWhitespace(ch) )
      {
        return RESULT::WAIT_NEXT;
      }
      else 
      {
        return RESULT::ERROR;
      }
    break;
    case DAT:
      if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else if(ch=='{')
      {
        if(!popStackHead() ||
           !pushStackHead(JSonState::OBJ_END) ||
           !pushStackHead(JSonState::DAT) ||
           !pushStackHead(JSonState::OBJ_SEP) ||
           !pushStackHead(JSonState::OBJ_KEY))
        {
          return RESULT::ERROR;
        }
        return RESULT::OBJECT_START;
      }
      else if(ch=='[')
      {
        if(!popStackHead() ||
           !pushStackHead(JSonState::ARR_END) ||
           !pushStackHead(JSonState::DAT))
        {
          return RESULT::ERROR;
        }
        return RESULT::ARRAY_START;
      }
      else if(ch=='"')
      {
        if(!popStackHead() || !pushStackHead(JSonState::STR))
        {
          return RESULT::ERROR;
        }
        strEscapePending=false;
        return RESULT::STR_START;
      }
      else 
      {
        if(!popStackHead() || !pushStackHead(JSonState::VAL))
        {
          return RESULT::ERROR;
        }
        return RESULT::VAL_START;
      }






    break;
    case OBJ_END:
      if(ch=='}')
      {
        if(!popStackHead())
        {
          return RESULT::ERROR;
        }
        return RESULT::OBJECT_COMPLETE;
      }
      else if(ch==',')
      {
        
        if(!pushStackHead(JSonState::DAT) ||
           !pushStackHead(JSonState::OBJ_SEP) ||
           !pushStackHead(JSonState::OBJ_KEY))
        {
          return RESULT::ERROR;
        }
        return RESULT::WAIT_NEXT;
      }
      else if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR;
      }

    break;
    case ARR_END:
      if(ch==']')
      {
        if(!popStackHead())
        {
          return RESULT::ERROR;
        }
        return RESULT::ARRAY_COMPLETE;
      }
      else if(ch==',')
      {
        
        if(!pushStackHead(JSonState::DAT))
        {
          return RESULT::ERROR;
        }
        return RESULT::WAIT_NEXT;
      }
      else if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::ERROR;
      }
    break;
    case STR:
      if(strEscapePending)
      {
        strEscapePending=false;
        return RESULT::WAIT_NEXT;
      }
      if(ch=='\\')
      {
        strEscapePending=true;
        return RESULT::WAIT_NEXT;
      }
      if(ch=='"')//end STR
      {
        if(!popStackHead())
        {
          return RESULT::ERROR;
        }
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
          if(!popStackHead())
          {
            return RESULT::ERROR;
          }
          return newChar(ch);//instant run again
        }
        else
        {//shouldn't be here
          return RESULT::ERROR;
        }
      }
      else if(ch==']')
      {
        if(getStackHead(1)==JSonState::ARR_END)
        {
          if(!popStackHead())
          {
            return RESULT::ERROR;
          }
          return newChar(ch);//instant run again
        }
        else
        {//shouldn't be here
          return RESULT::ERROR;
        }
      }
      else if(ch==',')
      {


        if(getStackHead(1)==JSonState::ARR_END)//only if it's arr
        {
          if(!popStackHead())
          {
            return RESULT::ERROR;
          }
          
          if(!pushStackHead(JSonState::DAT))//keep find next data
          {
            return RESULT::ERROR;
          }
          return newChar(ch);//instant run again
        }
        if(getStackHead(1)==JSonState::OBJ_END)//only if it's arr
        {
          if(!popStackHead())
          {
            return RESULT::ERROR;
          }
          
          return  newChar(ch);
        }
        else
        {
          return RESULT::ERROR;
        }
      }
      else if(isWhitespace(ch))
      {
        return RESULT::WAIT_NEXT;
      }
      else
      {
        return RESULT::WAIT_NEXT;
      }



    break;
  }

  return RESULT::ERROR;
}
