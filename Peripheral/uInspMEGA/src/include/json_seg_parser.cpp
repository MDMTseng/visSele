#include "json_seg_parser.hpp"


json_seg_parser::json_seg_parser(){
    reset();
}
void json_seg_parser::reset()
{
    pch='\0';
    jsonCurlyB_C=0;
    jsonInStrState=0;
    jsonSquareB_C=0;
    jsonStr_C=0;
}


int json_seg_parser::newChar(char ch){
    int ret_val=0;
    if(ch=='"' && pch!='\\')
    {
        jsonInStrState=!jsonInStrState;
        ret_val|=(jsonInStrState)?JSON_SEG_PARSER_NSEG_START:JSON_SEG_PARSER_NSEG_END;
    }

    if( (ch=='{' || ch=='['))
    {
      if((pch=='}' || pch==']'))//End reset
      {
          ret_val|=JSON_SEG_PARSER_RESET;
          reset();
      }
      
      if(jsonCurlyB_C==0 && jsonSquareB_C==0 )
      {
          //start point
          ret_val|=JSON_SEG_PARSER_SEG_START;
      }
    }
    if(!jsonInStrState)
    {
        if(ch==' '|| ch=='\n'|| ch=='\r'||ch=='\0')
        {
          return JSON_SEG_PARSER_NSEG_SPACE;
        }
        switch(ch)
        {
            case '{':
                jsonCurlyB_C++;
                ret_val|=JSON_SEG_PARSER_NSEG_START;
            break;
            case '[':
                jsonSquareB_C++;
                ret_val|=JSON_SEG_PARSER_NSEG_START;
            break;
            case '}':
                jsonCurlyB_C--;

                ret_val|=JSON_SEG_PARSER_NSEG_END;
                if(jsonSquareB_C==0 && jsonCurlyB_C==0)
                {
                  
                    ret_val|=JSON_SEG_PARSER_SEG_END;
                }
                else if(jsonCurlyB_C<0 )//Error reset
                {
                    ret_val|=JSON_SEG_PARSER_ERROR;
                    reset();
                }
            break;
            case ']':
                jsonSquareB_C--;
                
                
                ret_val|=JSON_SEG_PARSER_NSEG_END;
                if(jsonSquareB_C==0 && jsonCurlyB_C==0)
                {
                    ret_val|=JSON_SEG_PARSER_SEG_END;
                }
                else if(jsonSquareB_C<0 )//Error reset
                {
                    ret_val|=JSON_SEG_PARSER_ERROR;
                    reset();
                }
            break;
        }
    }
    pch=ch;

    return ret_val;
}






