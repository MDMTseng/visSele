#ifndef JSON_SEG_PARSER___HPP_
#define JSON_SEG_PARSER___HPP_




class json_seg_parser
{
    public:
    char pch;
    int jsonInStrState;
    int jsonCurlyB_C;
    int jsonSquareB_C;
    json_seg_parser();
    virtual void reset();
    virtual int newChar(char ch);
};




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
#define JSON_SEG_PARSER_RESET      (1<<0)
#define JSON_SEG_PARSER_ERROR      (1<<1)
#define JSON_SEG_PARSER_SEG_END    (1<<2)
#define JSON_SEG_PARSER_SEG_START  (1<<3)

#define JSON_SEG_PARSER_NSEG_END   (1<<4)
#define JSON_SEG_PARSER_NSEG_START (1<<5)
#define JSON_SEG_PARSER_NSEG_SPACE (1<<6)
int json_seg_parser::newChar(char ch){
    int ret_val=0;
    if(ch=='"' && pch!='\\')
    {
        jsonInStrState=!jsonInStrState;
        ret_val|=(jsonInStrState)?JSON_SEG_PARSER_NSEG_START:JSON_SEG_PARSER_NSEG_END;
    }

    if(!jsonInStrState)
    {
        if(ch==' '|| ch=='\n'|| ch=='\r')
        {
          return JSON_SEG_PARSER_NSEG_SPACE;
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








#endif