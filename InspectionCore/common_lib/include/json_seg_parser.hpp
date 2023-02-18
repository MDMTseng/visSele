#pragma once




class json_seg_parser
{
    protected:

    
    enum JSonState
    {
      NUL,
      OBJ_KEY=1,
      OBJ_SEP=2,
      OBJ_VAL=3,
      OBJ_END=4,
      ARR_END=5,
      STR=6,
      VAL=7,
      DAT=8,

      ERR
    };
    JSonState levelStack[30];
    int stackSize=0;


    JSonState getStackHead(int idx=0);
    bool pushStackHead(JSonState st);
    bool popStackHead();

    public:
    
    enum RESULT
    {
      WAIT_NEXT=0,
      WAIT_NEXT_SPACE,
      OBJECT_START=2,
      OBJECT_COMPLETE,
      ARRAY_START=4,
      ARRAY_COMPLETE,

      KEY_START=6,
      KEY_END,

      VAL_START=8,
      // VAL_END,

      
      STR_START=10,
      STR_END,

      ERROR_SEC=-1000,
    };

    json_seg_parser();
    virtual void reset();
    virtual RESULT newChar(char ch);
};
