#ifndef JSON_SEG_PARSER___HPP_
#define JSON_SEG_PARSER___HPP_




class json_seg_parser
{
    public:
    char pch;
    int jsonInStrState;
    int jsonStr_C;
    int jsonCurlyB_C;
    int jsonSquareB_C;
    json_seg_parser();
    virtual void reset();
    virtual int newChar(char ch);
};


#define JSON_SEG_PARSER_RESET      (1<<0)
#define JSON_SEG_PARSER_ERROR      (1<<1)
#define JSON_SEG_PARSER_SEG_END    (1<<2)
#define JSON_SEG_PARSER_SEG_START  (1<<3)

#define JSON_SEG_PARSER_NSEG_END   (1<<4)
#define JSON_SEG_PARSER_NSEG_START (1<<5)
#define JSON_SEG_PARSER_NSEG_SPACE (1<<6)


#endif