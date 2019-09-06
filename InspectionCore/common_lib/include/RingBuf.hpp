#ifndef __RingBufX_H__
#define __RingBufX_H__


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

  
};

template <
  typename RB_Type,typename RB_Idx_Type=uint32_t
>
class RingBuf
{
  
  protected:
  std::timed_mutex ptrLock;
  public:
  RingBufIdxCounter<RB_Idx_Type> RBC;
  RB_Type *buff;
  RingBuf(RB_Type *buff,RB_Idx_Type len)
  {
    this->buff=buff;
    RBC.RESET(len);
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
  
  
  int pushHead()
  {
    ptrLock.lock();
    int ret = RBC.pushHead();
    ptrLock.unlock();
    return ret;
  }

  int consumeTail()
  {
    ptrLock.lock();
    int ret = RBC.consumeTail();
    ptrLock.unlock();
    return ret;
  }
};

#endif /* __RingBufX_H__ */
