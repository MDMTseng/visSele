#include <napi.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <smem_channel.hpp>

smem_channel ch_server_recv("RECVXXX",40000,true);


Napi::Value CallEmit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Function emit = info[0].As<Napi::Function>();
  emit.Call({Napi::String::New(env, "start")});
  for (int i = 0; i < 3; i++) {
    std::this_thread::sleep_for(std::chrono::seconds(3));
    emit.Call(
        {Napi::String::New(env, "data"), Napi::String::New(env, "data ...")});

    // ch_server_recv.r_wait();
    // volatile uint8_t* data=(uint8_t*)ch_server_recv.getPtr();




    // auto buf = Napi::Uint8Array::New(env, 100*10000);
    // memcpy_s((void *)buf.Data(), 100, ptr, 100);
    // emit.Call({Napi::String::New(env, "data"), buf});








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