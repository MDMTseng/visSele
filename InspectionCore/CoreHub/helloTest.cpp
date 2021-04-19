#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <atomic>
#include <assert.h>
#include <thread>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#include <queue>
#include <RingBuf.hpp>
#include <TSQueue.hpp>












typedef struct TypeData{
  int XXX;
  int cameraID;
}TypeData;


resourcePool<TypeData> rpool(11);

TSQueue<TypeData*> tsQ(10);
std::atomic_bool stopF(false);

std::atomic_int64_t DatInSum(0);




int CD_Wait_ms=5000;


int GenId=0;
void thread_DataGen(){
  int id=GenId++;
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



    TypeData *fetch_data=rpool.fetchResrc_blocking();
    if(fetch_data==NULL)
    {
      printf("W: resource EMPTY!!!!\n");
      continue;
    }
    int randX=(rand()%10000)+1;
    counter++;

    int data=randX;
    fetch_data->XXX=data;
    fetch_data->cameraID=id;

    // while(true)
    {
      
      if(tsQ.push(fetch_data)) {
        DatInSum^=data;
        printf(">[%d]> data:%d DatInSum:%d  ptr:%p\n",id,fetch_data->XXX,(int)DatInSum,fetch_data);
        // break;
      } else {
        printf("%s  ptr:%p\n", "queue is full",fetch_data);
        // break;

        rpool.retResrc(fetch_data);//toss away
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
  while(1)
  {

    TypeData* data_out;
    
    if(tsQ.pop_blocking(data_out)) {
      DatInSum^=data_out->XXX;
      printf("<[%d]<  data:%d DResd:%d ptr:%p\n",
        id, data_out->XXX, (int)DatInSum,data_out);
      bool retOK = rpool.retResrc(data_out);

      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    else
    {
      printf("W: Queue empty\n");
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
      rpool.rest_size(),tsQ.size(),
      (int)DatInSum,(int)DatOutSum
      );
  }
}


int main(void) {


  // for(int i=0;i<10;i++)
  // {

  //   TypeData *fetch_data;
  //   int idx = rpool.fetchResrc(&fetch_data);
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
  //     rpool.retResrc(data_out.resId);
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
  for(int i=0;i<15;i++)
  {
    dataGen.push_back(new std::thread(thread_DataGen));
  }


  std::vector<std::thread *> dataDrain;

  for(int i=0;i<15;i++)
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