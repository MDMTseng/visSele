#ifndef __TS_Q_H__
#define __TS_Q_H__


#include <thread>
#include <mutex>
#include <queue>

template<typename T>
class TSQueue {
  std::queue<T> queue_;
  mutable std::mutex mutex_;
 
  mutable std::mutex push_mutex_;
  mutable std::mutex pop_mutex_;
  int maxDataCount;
  // Moved out of public interface to prevent races between this
  // and pop().
  bool empty();
 public:
  TSQueue(int maxCount=-1);
  unsigned long size();
  bool pop(T& retDat);
  bool pop_blocking(T& retDat);
  bool push(const T &item);
  bool push_blocking(const T &item);
};



template<typename T>
bool TSQueue<T>::empty() {
  return queue_.empty();
}


template<typename T>
TSQueue<T>::TSQueue(int maxCount){
  maxDataCount=maxCount;
};

template<typename T>
unsigned long TSQueue<T>::size() {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}
template<typename T>
bool TSQueue<T>::pop(T& retDat) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return false;
  }
  retDat = queue_.front();
  queue_.pop();
  
  push_mutex_.unlock();
  return true;
}
template<typename T>
bool TSQueue<T>::pop_blocking(T& retDat) {

  while(pop(retDat)==false)
  {
    // printf("pop_blocking :: locked\n");
    pop_mutex_.lock();
    // printf("pop_blocking :: unlocked\n");
  }

  return true;
}
template<typename T>
bool TSQueue<T>::push(const T &item) {
  std::lock_guard<std::mutex> lock(mutex_);

  if(maxDataCount!=-1 && queue_.size()>=maxDataCount)
  {
    return false;
  }
  queue_.push(item);
  pop_mutex_.unlock();
  return true;
}

template<typename T>
bool TSQueue<T>::push_blocking(const T &item) {

  while(push(item)==false)
  {
    // printf("push_blocking :: locked\n");
    push_mutex_.lock();
    // printf("push_blocking :: unlocked\n");
  }

  return true;
}






template<typename T>
class resourcePool_naiive
{
  struct res_w_flag{
    uint8_t flag;
    T data;
  };
  public:
  int rest_size;
  std::mutex rsc_mutex;
  std::vector <struct res_w_flag>pool;
  resourcePool_naiive(int size)
  {
    pool.resize(size);
    rest_size=size;
  }

  int fetchSrc(T** ret_data)
  {
    std::lock_guard<std::mutex> lock(rsc_mutex);

    int retIdx=-1;
    for(int i=0;i<pool.size();i++)
    {
      if(pool[i].flag==0)
      {
        retIdx=i;
        break;
      }
    }
    if(ret_data)
        *ret_data=NULL;
    if(retIdx!=-1)
    {
      pool[retIdx].flag=1;
      rest_size--;
      if(ret_data)
        *ret_data=&(pool[retIdx].data);
    }
    return retIdx;
  }  

  protected:
  bool _retSrc (int idx)
  {
    bool ifOK=false;
    if(pool[idx].flag==1)
    {
      pool[idx].flag=0;
      rest_size++;
      ifOK=true;
    }
    return ifOK;
  }  

  public:
  bool retSrc (int idx)
  {

    std::lock_guard<std::mutex> lock(rsc_mutex);
    return _retSrc (idx);
  }  


  bool retSrc (T* dataPtr)
  {

    std::lock_guard<std::mutex> lock(rsc_mutex);

    int retIdx=-1;
    for(int i=0;i<pool.size();i++)
    {
      if( &(pool[i].data)==dataPtr)
      {
        return _retSrc (i);
      }
    }


    return false;
  }  
};




template<typename T>
class resourcePool
{
  public:
  int rest_size;
  std::mutex rsc_mutex;
  std::vector <T>pool;
  std::vector <T*>poolPtr;
  std::mutex fetch_mutex;

  mutable std::mutex fetch_mutex_;

  resourcePool(int size)
  {
    pool.resize(size);
    poolPtr.resize(size);
    for(int i=0;i<pool.size();i++)
    {
      poolPtr[i]=&(pool[i]);
    }
    rest_size=size;
  }

  T* fetchSrc()
  {
    std::lock_guard<std::mutex> lock(rsc_mutex);
    int retIdx=-1;
    for(int i=0;i<pool.size();i++)
    {
      if(poolPtr[i]!=NULL)
      {
        retIdx=i;
        break;
      }
    }

    if(retIdx==-1)
      return NULL;
    T* dat=poolPtr[retIdx];
    poolPtr[retIdx]=NULL;
    rest_size--;
    return dat;
  }  

  T* fetchSrc_blocking(){
    T* fdat=NULL;
    while((fdat=fetchSrc())==NULL)
    {
      fetch_mutex.lock();
    }

    return fdat;
  }
  bool retSrc (T* ret_rsc)
  {
    std::lock_guard<std::mutex> lock(rsc_mutex);
    //check ret_rsc is in the pool
    T* head=&(pool[0]);

    if(pool.size()==0)
    {
      // assert(0&& "0 size pool...");
      return false;
    }

    int resourceIdx=-1;
    if(pool.size()==1)
    {//kinda special case
      if(ret_rsc!=head)
      {
        return false;
      }
      resourceIdx=0;
    }
    else
    {

      T* tail=&(pool[pool.size()-1]);
      if(ret_rsc<head || ret_rsc>tail)
      {
        // assert(0&& "Out range");
        return false;
      }
      int addrDiff= (int)((uint8_t*)ret_rsc-(uint8_t*)head);
      int dataSize=((int)((uint8_t*)tail-(uint8_t*)head))/(pool.size()-1);//get real data spacing

      // printf("addrDiff:%d  dataSize:%d :%d\n",(int)addrDiff,(int)dataSize,sizeof(pool[0]));
      int residue=addrDiff%dataSize;
      if(residue!=0)
      {
        // assert(0&& "residue!=0");
        return false;
      }
      int idx=addrDiff/dataSize;
      if(poolPtr[idx]!=NULL)
      {
        // assert(0&& "poolPtr[idx]!=NULL");
        return false;
      }
      resourceIdx=idx;
      
    }
    poolPtr[resourceIdx]=&(pool[resourceIdx]);

    rest_size++;
    fetch_mutex.unlock();
    //the ptr address is valid
    return true;
  }  

};

#endif