
#include <RingBuf.hpp>
#include <stdint.h>


template <typename RB_Idx_Type=uint32_t>
RingBufIdxCounter<RB_Idx_Type>::RingBufIdxCounter()
{
  RESET(0);
}
template <typename RB_Idx_Type=uint32_t>
RingBufIdxCounter<RB_Idx_Type>::RingBufIdxCounter(RB_Idx_Type len)
{
  RESET(len);
}

template <typename RB_Idx_Type=uint32_t>
void RingBufIdxCounter<RB_Idx_Type>::RESET(RB_Idx_Type len)
{
  RBLen=len;
  headIdx=0;
  tailIdx=0;
  dataSize=0;
}

template <typename RB_Idx_Type=uint32_t>
void RingBufIdxCounter<RB_Idx_Type>::clear(){headIdx=0;tailIdx=0;dataSize=0;}
template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::getHead(){return headIdx;}
template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::getTail(){return tailIdx;}
template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::getTail(RB_Idx_Type idx){
  int real_idx=getTail()+idx;
  if(real_idx>=RBLen)real_idx-=RBLen;
  return real_idx;
}
template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::size(){return dataSize;}
template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::space(){return RBLen-dataSize;}

template <typename RB_Idx_Type=uint32_t>
RB_Idx_Type RingBufIdxCounter<RB_Idx_Type>::getNextHead(){
  RB_Idx_Type bk_headIdx=headIdx;
  if(bk_headIdx==RBLen-1)bk_headIdx=0;
  else bk_headIdx++;
  return bk_headIdx;
}


template <typename RB_Idx_Type=uint32_t>
int RingBufIdxCounter<RB_Idx_Type>::consumeTail(){
  if(dataSize==0)return -1;//Tail pass the head... ERROR
  if(tailIdx==RBLen-1)tailIdx=0;
  else tailIdx++;
  dataSize--;
  return 0;
}

template <typename RB_Idx_Type=uint32_t>
int RingBufIdxCounter<RB_Idx_Type>::pushHead(){
  if(dataSize==RBLen)//queue is full.... ERROR
  {
    return -1;
  }
  if(headIdx==RBLen-1)headIdx=0;
  else headIdx++;
  
  
  dataSize++;
  return 0;
}


RingBuf::RingBuf(RB_Type *buff,RB_Idx_Type len)
{
  this->buff=buff;
  RBC.RESET(len);
}

RingBuf::RB_Idx_Type size()
{
  counterLock.lock();
  int size=RBC.size();
  counterLock.unlock();
  return size;
}

void RingBuf::clear()
{
  counterLock.lock();
  RBC.clear();
  counterLock.unlock();
}


RB_Idx_Type RingBuf::getHead_Idx()
{
  
  counterLock.lock();
  int idx = RBC.getHead();
  counterLock.unlock();
  return idx;
}
RB_Idx_Type RingBuf::getTail_Idx()
{
  
  counterLock.lock();
  int idx = RBC.getTail();
  counterLock.unlock();
  return idx;
}

RB_Type* RingBuf::getHead()
{

  counterLock.lock();  
  RB_Type* t=NULL;  
  if(RBC.space()!=0)
    t =buff+RBC.getHead();
  counterLock.unlock();
  return t;
}

RB_Idx_Type RingBuf::getTail_Idx(uint32_t idx)
{
  counterLock.lock();
  RB_Idx_Type t = RBC.getTail(idx);
  counterLock.unlock();
  return t;
}
RB_Type* RingBuf::getTail(uint32_t idx)
{
  counterLock.lock();
  
  RB_Type* t=NULL;  
  if(RBC.size()!=0)
    t = buff+RBC.getTail(idx);
  counterLock.unlock();
  return t;
}

RB_Type* RingBuf::getTail()
{
  counterLock.lock();  
  RB_Type* t=NULL;  
  if(RBC.size()!=0)
    t = buff+RBC.getTail();
  counterLock.unlock();
  return t;
}


RB_Type* RingBuf::getTail_block(int timeout_ms)
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

int RingBuf::pushHead()
{
  counterLock.lock();
  int ret = RBC.pushHead();
  counterLock.unlock();
  tailLock.unlock();
  return ret;
}

int RingBuf::consumeTail()
{
  counterLock.lock();
  int ret = RBC.consumeTail();
  counterLock.unlock();

  return ret;
}