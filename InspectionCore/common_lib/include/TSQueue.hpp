#ifndef __TS_Q_H__
#define __TS_Q_H__


#include <thread>
#include <mutex>
#include <queue>


#include <exception>
using namespace std;

struct TS_Termination_Exception : public exception {
   const char * what () const throw () {
      return "TS_Termination_Exception";
   }
};

template<typename T>
class TSQueue {

protected:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
 
  mutable std::mutex push_mutex_;
  mutable std::mutex pop_mutex_;
  bool termination=false;
  int maxDataCount;
  // Moved out of public interface to prevent races between this
  // and pop().
  bool empty();
  void termination_avalanche_and_throw_excption();
public:
  TSQueue(int maxCount=-1);
  size_t size();
  size_t capacity();
  bool is_terminated();
  void termination_trigger();
  bool resume_from_termination();
  bool pop(T& retDat);
  bool pop_blocking(T& retDat);
  bool peek(T& retDat);
  bool peek_blocking(T& retDat);
  bool push(const T &item);
  bool push_blocking(const T &item);
};

template<typename T>
void TSQueue<T>::termination_trigger(){
  termination=true;
  mutex_.unlock();
  push_mutex_.unlock();
  pop_mutex_.unlock();

}


template<typename T>
bool TSQueue<T>::is_terminated(){
  return termination;
}


template<typename T>
void TSQueue<T>::termination_avalanche_and_throw_excption(){
  termination_trigger();
  throw TS_Termination_Exception();
}

template<typename T>
bool TSQueue<T>::empty() {
  return queue_.empty();
}


template<typename T>
TSQueue<T>::TSQueue(int maxCount){
  maxDataCount=maxCount;
};

template<typename T>
size_t TSQueue<T>::size() {
  if(termination)termination_avalanche_and_throw_excption();
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination)termination_avalanche_and_throw_excption();
  return queue_.size();
}

template<typename T>
size_t TSQueue<T>::capacity() {
  return maxDataCount;
}

template<typename T>
bool TSQueue<T>::resume_from_termination()
{
  if(size()>0)return false;
  termination=false;
  return true;
}

template<typename T>
bool TSQueue<T>::pop(T& retDat) {
  if(termination)termination_avalanche_and_throw_excption();
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination)termination_avalanche_and_throw_excption();
  if (queue_.empty()) {
    return false;
  }
  retDat = queue_.front();
  queue_.pop();
  
  push_mutex_.unlock();
  return true;
}

template<typename T>
bool TSQueue<T>::peek(T& retDat) {
  if(termination)termination_avalanche_and_throw_excption();
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination)termination_avalanche_and_throw_excption();
  if (queue_.empty()) {
    return false;
  }
  retDat = queue_.front();
  push_mutex_.unlock();
  return true;
}

template<typename T>
bool TSQueue<T>::pop_blocking(T& retDat) {

  if(termination)termination_avalanche_and_throw_excption();
  while(pop(retDat)==false)
  {
    if(termination)termination_avalanche_and_throw_excption();
    // printf("pop_blocking :: locked\n");
    pop_mutex_.lock();
    // printf("pop_blocking :: unlocked\n");
  }

  return true;
}

template<typename T>
bool TSQueue<T>::peek_blocking(T& retDat) {

  if(termination)termination_avalanche_and_throw_excption();
  while(peek(retDat)==false)
  {
    if(termination)termination_avalanche_and_throw_excption();
    // printf("pop_blocking :: locked\n");
    pop_mutex_.lock();
    // printf("pop_blocking :: unlocked\n");
  }

  return true;
}
template<typename T>
bool TSQueue<T>::push(const T &item) {
  if(termination)termination_avalanche_and_throw_excption();
  std::lock_guard<std::mutex> lock(mutex_);

  if(termination)termination_avalanche_and_throw_excption();
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

  if(termination)termination_avalanche_and_throw_excption();
  while(push(item)==false)
  {
    // printf("push_blocking :: locked\n");
    if(termination)termination_avalanche_and_throw_excption();
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

  int fetchResrc(T** ret_data)
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
  bool _retResrc (int idx)
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
  bool retResrc (int idx)
  {

    std::lock_guard<std::mutex> lock(rsc_mutex);
    return _retResrc (idx);
  }  


  bool retResrc (T* dataPtr)
  {

    std::lock_guard<std::mutex> lock(rsc_mutex);

    int retIdx=-1;
    for(int i=0;i<pool.size();i++)
    {
      if( &(pool[i].data)==dataPtr)
      {
        return _retResrc (i);
      }
    }


    return false;
  }  
};




template<typename T>
class resourcePool
{
  protected:
  int _rest_size;
  std::mutex rsc_mutex;
  std::vector <T>pool;
  std::vector <T*>poolPtr;
  std::mutex fetch_mutex;
  bool termination=false;

  mutable std::mutex fetch_mutex_;

  public:
  resourcePool(int size)
  {
    RESIZE( size);
  }


  void RESIZE(int size)
  {
    pool.resize(size);
    poolPtr.resize(size);
    for(int i=0;i<pool.size();i++)
    {
      poolPtr[i]=&(pool[i]);
    }
    _rest_size=size;
  }


  void RESET()
  {
    std::lock_guard<std::mutex> lock(rsc_mutex);
    RESIZE(pool.size());
  }


  void termination_avalanche_and_throw_excption(){

    fetch_mutex.unlock();
    rsc_mutex.unlock();
    throw TS_Termination_Exception();
  }
  int rest_size(){return _rest_size;}
  T* fetchResrc()
  {
    if(termination)termination_avalanche_and_throw_excption();
    std::lock_guard<std::mutex> lock(rsc_mutex);
    if(termination)termination_avalanche_and_throw_excption();
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
    _rest_size--;
    return dat;
  }  

  T* fetchResrc_blocking(){
    if(termination)termination_avalanche_and_throw_excption();
    T* fdat=NULL;
    while((fdat=fetchResrc())==NULL)
    {
      fetch_mutex.lock();
      if(termination)termination_avalanche_and_throw_excption();
    }

    return fdat;
  }
  bool retResrc (T* ret_rsc)
  {
    if(termination)termination_avalanche_and_throw_excption();
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

    _rest_size++;
    fetch_mutex.unlock();
    //the ptr address is valid
    return true;
  }  

};






template<typename T>
class TSVector
{
  std::mutex _w_lock;
  std::vector <T>vec;

public:
  void push_back(T d)
  {
    
    std::lock_guard<std::mutex> lock(_w_lock);

    vec.push_back(d);
  }
  void w_lock()
  {
    _w_lock.lock();
  }
  void w_unlock()
  {
    _w_lock.unlock();
  }

  size_t size()
  {
    return vec.size();
  }


  T& operator[](int idx)
  {
    return vec[idx];
  }

  bool erase(int idx)
  {
    return vec.erase(vec.begin()+idx)!=vec.end();
  }

  void clear()
  {
    vec.clear();
  }

};

#endif