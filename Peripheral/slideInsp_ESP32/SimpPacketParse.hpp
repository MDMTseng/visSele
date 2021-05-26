#ifndef SimpPacketParse_HPP___
#define SimpPacketParse_HPP___
class SimpPacketParse{
  int cur_size;

private:
  void init_ONCE(int size,char startCharSym,char endCharSym)
  {
    buffer=new char[size];
    cur_size=0;
    _START_=startCharSym;
    _END_=endCharSym;
  }
public:
  char _START_='@';
  char _END_='$';
  char *buffer;

  SimpPacketParse(int size,char startCharSym,char endCharSym)
  {
    init_ONCE(size,startCharSym,endCharSym);
  }
  SimpPacketParse(int size)
  {
    init_ONCE(size,'@','$');
  }
  char sChar(){return _START_;}
  char eChar(){return _END_;}
  void add(char ch)
  {
    buffer[cur_size]=ch;
    cur_size++;
  }
  
  int size()
  {
    return cur_size;
  }
  
  void clean()
  {
    cur_size=0;
  }

  int CMD_parse(SimpPacketParse *spp)
  {
    return 0;
  }
  
  bool feed(char ch,bool wCallbackStyle=false)
  {
    if(size()==0)//looking for $
    {
      if(ch==_START_)
      {
        add((char)ch);
      }
    }
    else//looking for @
    {
      if(ch==_END_)
      {
        add('\0');
        if(wCallbackStyle)
        {
          CMD_parse(this);
          clean();
        }
        else
          return true;
      }
      else
      {
        if(buffer[0]==_START_)
        {
          clean();
        }
        add((char)ch);
      }
    }
    return false;
  }


};





#endif
