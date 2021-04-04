#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <wfqueue.h>
#include <iostream>
#include <vector>
#include <RingBuf.hpp>





template<typename T>
class resourcePool
{
  struct res_w_flag{
    uint8_t flag;
    T data;
  };
  public:
  int rest_size;
  std::mutex rsc_mutex;
  std::vector <struct res_w_flag>pool;
  resourcePool(int size)
  {
    pool.resize(size);
    rest_size=size;
  }

  int fetchSrc(T** ret_data)
  {
    rsc_mutex.lock();

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
    rsc_mutex.unlock();
    return retIdx;
  }  



  bool retSrc (int idx)
  {
    rsc_mutex.lock();
    bool ifOK=false;
    if(pool[idx].flag==1)
    {
      pool[idx].flag=0;
      rest_size++;
      ifOK=true;
    }
    rsc_mutex.unlock();
    return ifOK;
  }  

};


typedef struct TypeData{
  int XXX;
  int cameraID;
}TypeData;
typedef struct TypeX{
  TypeData *data;
  int resId;
}TypeX;

resourcePool<TypeData> rpool(30);


tWaitFree::Queue<TypeX> myqueue(5);


std::atomic_bool stopF(false);

std::atomic_int64_t DatInSum(0);


int CD_Wait_ms=5000;


int GenId=0;
void thread_DataGen(){
  int id=GenId++;
  tWaitFree::WfqEnqCtx<TypeX> enqCtx; // init 1 time in a thread only
  int counter=0;
  while(1)
  {
    if(stopF)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(CD_Wait_ms));//cool down
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = t2 - t1;
    // std::cout << "Waited " << elapsed.count() << " ms\n";



    TypeData *fetch_data;
    int idx = rpool.fetchSrc(&fetch_data);
    if(idx<0)
    {
      printf("W: resource EMPTY!!!!\n");
      break;
    }
    int randX=(rand()%10000)+1;
    counter++;

    int data=randX;
    fetch_data->XXX=data;
    fetch_data->cameraID=id;
    TypeX tmp={resId:idx,data:fetch_data};

    while(true)
    {

      if(myqueue.tryEnq(tmp, enqCtx)) {
        DatInSum^=data;
        printf(">[%d]> id:%d  data:%d DatInSum:%d  ptr:%p\n",id,tmp.resId,tmp.data->XXX,(int)DatInSum,tmp.data);
        break;
      } else {
        printf("%s  ptr:%p\n", "queue is full",tmp.data);
        // break;

        // rpool.retSrc(idx);//toss away
        stopF=true;
        std::this_thread::sleep_for(std::chrono::milliseconds(CD_Wait_ms));//cool down
        stopF=false;
      }
    }


  }
}


std::atomic_int64_t DatOutSum(0);
int DrainId=0;
void thread_DataDrain(){
  int id=DrainId++;
  tWaitFree::WfqDeqCtx<TypeX> deqCtx; // init 1 time in a thread only
  while(1)
  {

    TypeX data_out;
    if(myqueue.tryDeq(data_out, deqCtx)) {

      DatInSum^=data_out.data->XXX;
      printf("<[%d]<  id:%d data:%d DatInSum:%d  ptr:%p \n",
        id, data_out.resId, data_out.data->XXX, (int)DatInSum,data_out.data);
      bool retOK = rpool.retSrc(data_out.resId);
      // printf("retOK:%d \n",retOK);


      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    else
    {
      // printf("W: Queue empty\n");
      // break;

      auto t1 = std::chrono::high_resolution_clock::now();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      auto t2 = std::chrono::high_resolution_clock::now();
      std::chrono::duration<double, std::milli> elapsed = t2 - t1;
      // std::cout << "Waited " << elapsed.count() << " ms\n";


    }
  }
}


void thread_DataWatch()
{
  while(1)
  {
    auto t1 = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = t2 - t1;
    // std::cout << "Waited " << elapsed.count() << " ms\n";
  

    printf("poolSize:%d  QSize:%d   DatInSum:%d  DatOutSum:%d\n",
      rpool.rest_size,myqueue.getSize(),
      (int)DatInSum,(int)DatOutSum
      );
  }
}


int main(void) {


  // for(int i=0;i<10;i++)
  // {

  //   TypeData *fetch_data;
  //   int idx = rpool.fetchSrc(&fetch_data);
  //   if(idx<0)
  //   {
  //     printf("W: resource FULL!!!!\n");
  //     break;
  //   }
  //   fetch_data->XXX=i;

  //   TypeX tmp={resId:idx,data:fetch_data};
  //   if(myqueue.tryEnq(tmp, enqCtx)) {
  //     printf("enque Done  id:%d  data:%d\n",tmp.resId,tmp.data->XXX);
  //   } else {
  //     printf("%s\n", "queue is full, please try again to re-enqueue ");
  //     break;
  //   }
  // }
  // // wrap in to thread
  // // please use deq to guarantee dequeue.


  // while(true)
  // {
  //   TypeX data_out;
  //   if(myqueue.tryDeq(data_out, deqCtx)) {
  //     printf("deque Done  id:%d  data:%d\n",data_out.resId,data_out.data->XXX);
  //     rpool.retSrc(data_out.resId);
  //   }
  //   else
  //   {
  //     printf("W: Queue empty\n");
  //     break;
  //   }
  // }
  // printf("Hello! C World!\n");
  std::vector<std::thread *> dataGen;
  // std::thread *dataGen=new std::thread[5];
  for(int i=0;i<1;i++)
  {
    dataGen.push_back(new std::thread(thread_DataGen));
  }


  std::vector<std::thread *> dataDrain;

  for(int i=0;i<1;i++)
  {
    dataDrain.push_back(new std::thread(thread_DataDrain));
  }



  std::thread Third (thread_DataWatch); 

  for(int i=0;i<dataGen.size();i++)
  {
    dataGen[i]->join();
    delete(dataGen[i]);
  }
  for(int i=0;i<dataDrain.size();i++)
  {
    dataDrain[i]->join();
    delete(dataDrain[i]);
  }

  Third.join();               // 等「second」thread執行結束

  return 0;
}