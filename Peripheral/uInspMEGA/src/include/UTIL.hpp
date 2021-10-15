

#ifndef UTILXXX_H_
#define UTILXXX_H_
#include <stdarg.h>
#include <Arduino.h>
#define DEBUG_
#ifdef DEBUG_
#define DEBUG_print(A, ...) Serial.print(A,##__VA_ARGS__)
#define DEBUG_println(A, ...) Serial.println(A,##__VA_ARGS__)
#else
#define DEBUG_print(A, ...)
#define DEBUG_println(A, ...)
#endif

void DEBUG_printf(const char * format, ...);

typedef struct pipeLineInfo{
  uint32_t gate_pulse;
  uint32_t s_pulse;
  uint32_t e_pulse;
  uint32_t pulse_width;
  uint32_t trigger_pulse;
  int8_t stage;
  int8_t sent_stage;
  int8_t notifMark;
  int16_t insp_status;
}pipeLineInfo;






class buffered_print
{
  char* buf;
  int _capacity;
  int _size;
  public:
  buffered_print(int len)
  {
    buf=new char[len];
    _capacity=len;
    _size=0;
    resize(0);
  }

  int size()
  {
    return _size;
  }

  int capacity()
  {
    return _capacity;
  }
  int rest_capacity()
  {
    return _capacity-_size;
  }
  int resize(int size)
  {
    if(size>_capacity)
    {
      return -1;
    }
    _size=size;
    buf[_size+1]='\0';
    buf[_size]='\0';
    return 0;
  }

  char* buffer()
  {
    return buf;
  }
  
  char charAt(int idx)
  {
    if(idx>=0)
    {
      if(idx>=_size)
        return '\0';
      return buf[idx];
    }

    idx+=_size;
    if(idx>=0)
    {
      if(idx>=_size)
        return '\0';
      return buf[idx];
    }
    
    return '\0';
  }

  
  int printf(char *fmt, ...) 
  {    
    va_list argptr;
    va_start(argptr,fmt);
//    _size=snprintf(buf+_size,_capacity-_size,fmt,argptr);
    _size=vsnprintf(buf+_size,_capacity-_size,fmt,argptr);
    va_end(argptr);
    return (_size==_capacity)?-1:0;
  }
};

template <class T>
class mArray
{
  protected:
  int cur_size;
  int max_size;
  public:
  
  T* arr;
  
  mArray(int max_size=0)
  {
    RESET( max_size);
  }

  
  void RESET(int max_size)
  {
    cur_size=0;
    if(arr)
      delete(arr);
    
    if(max_size<=0)
    {
      arr=NULL;
      this->max_size=0;
      
      return;
    }

    
    arr=new T[max_size];
    this->max_size=max_size;
  }
  ~mArray()
  {
    RESET(0);
  }

  
  bool push_back(T d)
  {
    if(!resize(size()+1))
    {
      return false;
    }
    arr[size()-1]=d;
    return true;
  }


  bool resize(int _size)
  {
    if(_size<0)return false;
    if(_size>max_size)return false;
    cur_size=_size;
    return true;
  }

  
  int size()
  {
    return cur_size;
  }
  int maxSize()
  {
    return max_size;
  }
};


template <class T, unsigned N>
class mArray_Static:public mArray<T>
{
  public:
  T array[N];
  mArray_Static():mArray<T>(array,N)
  {

  }
};
uint32_t cmpCheckSum(int shift, int num_args, ...);

void HARD_RESET();
#endif
