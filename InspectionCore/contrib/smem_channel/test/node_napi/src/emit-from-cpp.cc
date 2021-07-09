#include <napi.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <smem_channel.hpp>

smem_channel ch_server_recv("RECVXXX",40000,true);

void finalizer__(Napi::Env /*env*/, uint8_t* data)
{

    printf("finalizer__>>>\n");
}

// uint8_t tmpArr_RECV[10000]={0};
uint8_t tmpArr_SEND[10000]={0};

Napi::Value CallEmit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Function emit = info[0].As<Napi::Function>();
  emit.Call({Napi::String::New(env, "start")});


  auto buf_RECV = Napi::Buffer<uint8_t>::New(env, (uint8_t *)ch_server_recv.getPtr(),ch_server_recv.size());
  emit.Call({Napi::String::New(env, "buffer_RECV"), buf_RECV});

  {
    auto buf = Napi::Buffer<uint8_t>::New(env, tmpArr_SEND,sizeof(tmpArr_SEND),finalizer__);
    emit.Call({Napi::String::New(env, "buffer_SEND"), buf});
  }

  for (int i = 0; i < 100; i++) {
    // std::this_thread::sleep_for(std::chrono::seconds(3));
    // emit.Call({Napi::String::New(env, "data"), Napi::String::New(env, "data ...")});

    // ch_server_recv.r_wait();
    // volatile uint8_t* data=(uint8_t*)ch_server_recv.getPtr();


    uint8_t * data = buf_RECV.Data();
    data[0]=i;
    emit.Call({Napi::String::New(env, "data"), Napi::String::New(env, "data ...")});
    data[0]=0;
    printf(">%d>>\n",i);






    // ch_server_recv.r_release();




    
  }
  emit.Call({Napi::String::New(env, "end")});
  return Napi::String::New(env, "OK");
}

// Init
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "callEmit"),
              Napi::Function::New(env, CallEmit));
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init);