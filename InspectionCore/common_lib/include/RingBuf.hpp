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
  RB_Idx_Type getHead(RB_Idx_Type idx){
    int real_idx=getHead()-idx;
    if(real_idx<0)real_idx+=RBLen;
    return real_idx;
  }
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
  std::timed_mutex headLock;
  std::timed_mutex tailLock;
  std::timed_mutex counterLock;
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
    counterLock.lock();
    int size=RBC.size();
    counterLock.unlock();
    return size;
  }
  
  void clear()
  {
    counterLock.lock();
    RBC.clear();
    counterLock.unlock();
  }


  RB_Idx_Type getHead_Idx()
  {
    
    counterLock.lock();
    int idx = RBC.getHead();
    counterLock.unlock();
    return idx;
  }
  RB_Idx_Type getTail_Idx()
  {
    
    counterLock.lock();
    int idx = RBC.getTail();
    counterLock.unlock();
    return idx;
  }
  
  RB_Type* getHead()
  {

    counterLock.lock();  
    RB_Type* t=NULL;  
    if(RBC.space()!=0)
      t =buff+RBC.getHead();
    counterLock.unlock();
    return t;
  }

  RB_Idx_Type getTail_Idx(uint32_t idx)
  {
    counterLock.lock();
    RB_Idx_Type t = RBC.getTail(idx);
    counterLock.unlock();
    return t;
  }
  RB_Type* getTail(uint32_t idx)
  {
    counterLock.lock();
    
    RB_Type* t=NULL;  
    if(RBC.size()!=0)
      t = buff+RBC.getTail(idx);
    counterLock.unlock();
    return t;
  }

  RB_Type* getTail()
  {
    counterLock.lock();  
    RB_Type* t=NULL;  
    if(RBC.size()!=0)
      t = buff+RBC.getTail();
    counterLock.unlock();
    return t;
  }

  
  RB_Type* getTail_block(int timeout_ms)
  { 
    using Ms = std::chrono::milliseconds;
    
    tailLock.lock();

    
    if(size()==0 )
    {
      tailLock.try_lock_for(Ms(timeout_ms));
      if(size()==0)
      {
        tailLock.unlock();
        return NULL;
      }
    }
    tailLock.unlock();
    counterLock.lock();  
    RB_Type* t= buff+RBC.getTail();
    counterLock.unlock();
    return t;
  }
  
  int pushHead()
  {
    counterLock.lock();
    int ret = RBC.pushHead();
    counterLock.unlock();
    if(ret ==0 )
      tailLock.unlock();
    return ret;
  }

  int consumeTail()
  {
    counterLock.lock();
    int ret = RBC.consumeTail();
    counterLock.unlock();

    return ret;
  }
};


#endif /* __RingBufX_H__ */
