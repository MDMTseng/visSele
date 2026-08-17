#ifndef __TS_Q_H__
#define __TS_Q_H__


#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>


#include <exception>
using namespace std;

struct TS_Termination_Exception : public exception {
   const char * what () const throw () {
      return "TS_Termination_Exception";
   }
};

// Bounded, thread-safe, blocking queue.
//
// Rewritten to use std::condition_variable. The previous implementation
// used a std::mutex as a semaphore -- locked in the consumer thread and
// unlocked in the producer thread (undefined behaviour: a mutex must be
// released by the thread that holds it) and with no count, so wakeups were
// lost when several push()es landed while a blocked consumer was between
// its pop() check and its lock(). That produced a stall-then-burst drain
// pattern on the inspection/data-view/camera queues. The cv version below
// keeps the exact same public API + TS_Termination_Exception semantics.
template<typename T>
class TSQueue {

protected:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_cv_;   // signalled on push
  std::condition_variable not_full_cv_;    // signalled on pop
  std::atomic<bool> termination{false};
  int maxDataCount;
  // Moved out of public interface to prevent races between this
  // and pop().
  bool empty();
  // Must be called with mutex_ held. Wakes every waiter so the
  // termination cascades, then throws.
  [[noreturn]] void throw_terminated_locked();
public:
  TSQueue(int maxCount=-1);
  void resize(int size);
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
  bool dump(T &item);

};

template<typename T>
void TSQueue<T>::termination_trigger(){
  {
    std::lock_guard<std::mutex> lock(mutex_);
    termination.store(true);
  }
  not_empty_cv_.notify_all();
  not_full_cv_.notify_all();
}


template<typename T>
bool TSQueue<T>::is_terminated(){
  return termination.load();
}


template<typename T>
void TSQueue<T>::throw_terminated_locked(){
  // mutex_ is held by the caller; notify outside the lock is fine but
  // notifying while holding it is also valid and keeps this simple.
  not_empty_cv_.notify_all();
  not_full_cv_.notify_all();
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
void TSQueue<T>::resize(int size) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    maxDataCount=size;
  }
  // Capacity may have grown -- wake any blocked pushers to re-check.
  not_full_cv_.notify_all();
}

template<typename T>
size_t TSQueue<T>::size() {
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination.load())throw_terminated_locked();
  return queue_.size();
}

template<typename T>
size_t TSQueue<T>::capacity() {
  return maxDataCount;
}

template<typename T>
bool TSQueue<T>::resume_from_termination()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if(!queue_.empty())return false;
  termination.store(false);
  return true;
}

template<typename T>
bool TSQueue<T>::pop(T& retDat) {
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination.load())throw_terminated_locked();
  if (queue_.empty()) {
    return false;
  }
  retDat = queue_.front();
  queue_.pop();
  not_full_cv_.notify_one();
  return true;
}

template<typename T>
bool TSQueue<T>::peek(T& retDat) {
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination.load())throw_terminated_locked();
  if (queue_.empty()) {
    return false;
  }
  retDat = queue_.front();
  return true;
}

template<typename T>
bool TSQueue<T>::pop_blocking(T& retDat) {
  std::unique_lock<std::mutex> lock(mutex_);
  not_empty_cv_.wait(lock, [this]{ return termination.load() || !queue_.empty(); });
  if(termination.load())throw_terminated_locked();
  retDat = queue_.front();
  queue_.pop();
  not_full_cv_.notify_one();
  return true;
}

template<typename T>
bool TSQueue<T>::peek_blocking(T& retDat) {
  std::unique_lock<std::mutex> lock(mutex_);
  not_empty_cv_.wait(lock, [this]{ return termination.load() || !queue_.empty(); });
  if(termination.load())throw_terminated_locked();
  retDat = queue_.front();
  return true;
}
template<typename T>
bool TSQueue<T>::push(const T &item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if(termination.load())throw_terminated_locked();
  if(maxDataCount!=-1 && queue_.size()>=(size_t)maxDataCount)
  {
    return false;
  }
  queue_.push(item);
  not_empty_cv_.notify_one();
  return true;
}

template<typename T>
bool TSQueue<T>::push_blocking(const T &item) {
  std::unique_lock<std::mutex> lock(mutex_);
  not_full_cv_.wait(lock, [this]{
    return termination.load() || maxDataCount==-1 || queue_.size()<(size_t)maxDataCount;
  });
  if(termination.load())throw_terminated_locked();
  queue_.push(item);
  not_empty_cv_.notify_one();
  return true;
}


template<typename T>
bool TSQueue<T>::dump(T &item) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return false;
  }
  item = queue_.front();
  queue_.pop();
  not_full_cv_.notify_one();
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
  bool termination=false;

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
    // No unlocks here. Unlocking a mutex the calling thread does not own is
    // undefined behavior (and this was called with neither mutex held); the
    // "avalanche" existed to wake fetchResrc_blocking, which is gone.
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

  // fetchResrc_blocking was deleted: it used fetch_mutex as a cross-thread
  // semaphore (locked here, unlocked by retResrc on another thread) -- the
  // exact UB pattern the TSQueue rewrite at the top of this file purged, and
  // it had no callers left. If a blocking fetch is ever needed again, build
  // it on a condition_variable with a predicate, like TSQueue does.
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
    // No fetch_mutex.unlock() here: this thread never locked it, and calling
    // unlock() on an unowned std::mutex is UB -- executed per frame on every
    // pool return until it was removed (winpthreads on the MinGW build does
    // not forgive it the way libc++ happened to).
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