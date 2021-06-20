#include "smem_channel.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdexcept>





smem_channel::smem_channel(const std::string name,
                           size_t memSize,
                           bool create_or_conn) {
  this->memSize = memSize;

  this->name = name;
  // printf(">>>%d\n",__LINE__);
  if (create_or_conn)  // create
  {
    ShareMemoryInfo _info;
  // printf(">>>%d\n",__LINE__);
    int ret = createSharedMemory(name,memSize,&_info);
    if(ret!=0)
    {
      throw std::invalid_argument("createSharedMemory failed....");
    }
  // printf(">>>%d\n",__LINE__);
    shm_id = _info.handle;
    ptr = _info.ptr;
    info=_info;
  // printf(">>>%d\n",__LINE__);
    sem=createSemaphore(name);
  // printf(">>>%d\n",__LINE__);
    // sem = sem_open((name+"_sem").c_str(), O_CREAT, 0644, 0);
    // if (sem == SEM_FAILED) {
    //   throw std::invalid_argument("sem_open failed....");
    // }
  } else  // connect
  {
    
    // sem = sem_open((name+"_sem").c_str(), 0);
    // if (sem == SEM_FAILED) {
    //   throw std::invalid_argument("sem_open failed....");
    // }
    ShareMemoryInfo _info;
    int ret = connSharedMemory(name,memSize,&_info);
    if(ret!=0)
    {
      throw std::invalid_argument("connSharedMemory failed....");
    }
    shm_id = _info.handle;
    ptr = _info.ptr;
    info=_info;
    
    sem=createSemaphore(name);
  }
}
smem_channel::~smem_channel() {
  // sem_unlink((name+"_sem").c_str());
  deleteSharedMemory(info);
}
size_t smem_channel::size() {
  return memSize;
}

void* smem_channel::getPtr() {
  return ptr;
}

void smem_channel::r_wait() {
  // sem_wait(sem);
  DecSemaphore(sem);
}
void smem_channel::r_release() {
  // sem_post(sem);
  IncSemaphore(sem);
}

void smem_channel::s_post() {
  // sem_post(sem);
  IncSemaphore(sem);
}
void smem_channel::s_wait_remote() {
  // sem_wait(sem);
  DecSemaphore(sem);
}