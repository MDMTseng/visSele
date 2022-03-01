#pragma once




class json_seg_parser
{
    protected:
    char pch;
    int jsonInStrState;
    int jsonCurlyB_C;
    int jsonSquareB_C;

    public:
    json_seg_parser();
    virtual void reset();
    virtual int newChar(char ch);
};
