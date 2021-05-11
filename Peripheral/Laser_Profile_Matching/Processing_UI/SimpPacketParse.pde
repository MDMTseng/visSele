

class SimpPacketParse{
  protected int cur_size;
  public char []buffer;
  
  final char _START_='@';
  final char _END_='$';
  public SimpPacketParse(int size)
  {
    buffer=new char[size];
    cur_size=0;
  }
  
  public void add(char ch)
  {
    buffer[cur_size]=ch;
    cur_size++;
  }
  
  public int size()
  {
    return cur_size;
  }
  
  public void clean()
  {
    cur_size=0;
  }
  
  
  public boolean feed(char ch)
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
  
  
  
}