
#ifndef __RingBufX_H__
#define __RingBufX_H__

// #include <Arduino.h>

/*
 * Set the integer size used to store the size of the buffer according of
 * the size given in the template instanciation. Thanks to Niklas Gürtler
 * to share his knowledge of C++ template meta programming.
 * https://niklas-guertler.de/
 *
 * If Index argument is true, the ring buffer has a size and an index
 * stored in an uint8_t (Type below) because its size is within [1,255].
 * Intermediate computation may need an uint16_t (BiggerType below).
 * If Index argument is false, the ring buffer has a size and an index
 * stored in an uint16_t (Type below) because its size is within [256,65535].
 * Intermediate computation may need an uint32_t (BiggerType below).
 */

template <typename RB_Idx_Type=uint32_t>
class RingBufIdxCounter
{
  RB_Idx_Type headIdx;
  RB_Idx_Type tailIdx;
  RB_Idx_Type dataSize;
  RB_Idx_Type RBLen;

  public:
  
  RingBufIdxCounter()
  {
    RESET(0);
  }
  RingBufIdxCounter(RB_Idx_Type len)
  {
    RESET(len);
  }

  void RESET(RB_Idx_Type len)
  {
    RBLen=len;
    headIdx=0;
    tailIdx=0;
    dataSize=0;
  }

  void clear(){headIdx=0;tailIdx=0;dataSize=0;}
  RB_Idx_Type getHead(){return headIdx;}
  RB_Idx_Type getTail(){return tailIdx;}
  RB_Idx_Type getTail(RB_Idx_Type idx){
    int real_idx=getTail()+idx;
    if(real_idx>=RBLen)real_idx-=RBLen;
    return real_idx;
  }
  RB_Idx_Type size(){return dataSize;}
  RB_Idx_Type space(){return RBLen-dataSize;}
  
  RB_Idx_Type getNextHead(){
    RB_Idx_Type bk_headIdx=headIdx;
    if(bk_headIdx==RBLen-1)bk_headIdx=0;
    else bk_headIdx++;
    return bk_headIdx;
  }
  
  
  int consumeTail(){
    if(dataSize==0)return -1;//Tail pass the head... ERROR
    if(tailIdx==RBLen-1)tailIdx=0;
    else tailIdx++;
    dataSize--;
    return 0;
  }

  int pushHead(){
    if(dataSize==RBLen)//queue is full.... ERROR
    {
      return -1;
    }
    if(headIdx==RBLen-1)headIdx=0;
    else headIdx++;
    
    
    dataSize++;
    return 0;
  }

  
  int pullHead(){
    if(dataSize==0)//queue is full.... ERROR
    {
      return -1;
    }
    if(headIdx==0)headIdx=RBLen-1;
    else headIdx--;
    
    
    dataSize--;
    return 0;
  }
};

template <
  typename RB_Type,typename RB_Idx_Type=uint32_t
>
class RingBuf
{
  public:
  RingBufIdxCounter<RB_Idx_Type> RBC;
  RB_Type *buff;
  RB_Type *new_buff;
  RingBuf(RB_Type *buff,RB_Idx_Type len)
  {
    RESET(buff,len);
  }
  
  RingBuf(RB_Idx_Type len)
  {
    new_buff=buff=new RB_Type[len];
    RBC.RESET(len);
  }
  
  RingBuf()
  {
    new_buff=buff=NULL;
    RBC.RESET(0);
  }
  void RESET(RB_Type *buff,RB_Idx_Type len)
  {
    if(new_buff)
    {
      delete(new_buff);
      new_buff=NULL;
    }
    this->buff=buff;
    RBC.RESET(len);
  }

  void RESET(RB_Idx_Type len)
  {
    if(new_buff)
    {
      delete(new_buff);
      new_buff=NULL;
    }
    RB_Type *tmpBuff=new RB_Type[len];
    RESET(tmpBuff,len);
    new_buff=tmpBuff;
  }
  
  ~RingBuf()
  {
    if(new_buff)
    {
      delete(new_buff);
      new_buff=NULL;
    }
    buff=NULL;
  }

  RB_Idx_Type size()
  {
    return RBC.size();
  }
  
  void clear()
  {
      RBC.clear();
  }


  RB_Idx_Type getHead_Idx()
  {
    return RBC.getHead();
  }
  RB_Idx_Type getTail_Idx()
  {
    return RBC.getTail();
  }
  
  RB_Type* getHead()
  {
    if(RBC.space()==0)return NULL;
    return buff+RBC.getHead();
  }

  RB_Idx_Type getTail_Idx(uint32_t idx)
  {
    return RBC.getTail(idx);
  }
  RB_Type* getTail(uint32_t idx)
  {
    if(idx>=RBC.size())return NULL;
    return buff+RBC.getTail(idx);
  }

  RB_Type* getTail()
  {
    if(RBC.size()==0)return NULL;
    return buff+RBC.getTail();
  }
  
  
  int pushHead(RB_Type dat)
  {
    RB_Type* head=getHead();
    if(head==NULL)
    {
       return -1;
    }
    *head=dat;
    return RBC.pushHead();
  }
  int pushHead()
  {
    return RBC.pushHead();
  }
  
  int pullHead()
  {
    return RBC.pullHead();
  }

  int consumeTail()
  {
    return RBC.consumeTail();
  }
};



template <
  typename RB_Type, unsigned N ,typename RB_Idx_Type=uint32_t
>
class RingBuf_Static:public RingBuf<RB_Type,RB_Idx_Type>
{

  public:
  RB_Type array[N];
  RingBuf_Static():RingBuf<RB_Type,RB_Idx_Type>(array,N)
  {

  }
};


#endif /* __RingBufX_H__ */
