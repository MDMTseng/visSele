#pragma once

#ifndef __SMEM_CHANNEL_HPP__
#define __SMEM_CHANNEL_HPP__

#include <sp.hpp>
#include <string>

class smem_channel {
  ShareMemoryInfo info;
  SHM_HDL shm_id;
  void* ptr;
  SEM_HDL sem;
  std::string name;
  size_t memSize;

 public:
  smem_channel(const std::string name, size_t memSize, bool create_or_conn);
  ~smem_channel();
  size_t size();
  void* getPtr();
  void r_wait();
  void r_release();
  void s_post();
  void s_wait_remote();
};
#endif