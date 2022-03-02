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
}
int json_seg_parser::newChar(char ch){
    int ret_val=0;
    if(ch=='"' && pch!='\\')
    {
        jsonInStrState=!jsonInStrState;
    }

    if( (ch=='{' && pch=='}')||(ch=='[' && pch==']') )//End reset
    {
        reset();
        ret_val=0;
    }

    if(!jsonInStrState)
    {
        if( (ch=='{' || ch=='[') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //start point
            ret_val=1;
        }
        switch(ch)
        {
            case '{':
                jsonCurlyB_C++;
            break;
            case '[':
                jsonSquareB_C++;
            break;
            case '}':
                jsonCurlyB_C--;
            break;
            case ']':
                jsonSquareB_C--;
            break;
        }
        
        if( (ch=='}' || ch==']') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //end point
            ret_val=-1;
        }
        if(jsonCurlyB_C<0 || jsonSquareB_C<0 )//Error reset
        {
            reset();
            ret_val=0;
        }
    }
    pch=ch;
    return ret_val;
}




