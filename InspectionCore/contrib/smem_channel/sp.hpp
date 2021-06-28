#pragma once
#ifndef __SP_WIN_H__
#define __SP_WIN_H__
#define _TIMESPEC_DEFINED       1
#define NO_OLDNAMES
// #include <semaphore.h>

#ifdef _WIN32
    // windows code goes here
    #define pid_t _pid_t
    #include <windows.h>
    #include <string>
    #undef pid_t
    
    typedef HANDLE SHM_HDL;
    typedef HANDLE SEM_HDL;


#else
    #include <string>
    #include <sys/shm.h> 
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    #include <semaphore.h>
    #include <fcntl.h> 
    typedef int SHM_HDL;
    typedef sem_t* SEM_HDL;

#endif



struct ShareMemoryInfo
{
    std::string name;
    SHM_HDL handle;
	void* ptr;
};

// sem_t *sem_open(const char *name, int oflag);
int createSharedMemory(std::string name,size_t maxSize,ShareMemoryInfo *ret_info);
int connSharedMemory(std::string name,size_t maxSize,ShareMemoryInfo *ret_info);
bool deleteSharedMemory(ShareMemoryInfo *info);


SEM_HDL createSemaphore(std::string name);
bool IncSemaphore(SEM_HDL hdl);
bool DecSemaphore(SEM_HDL hdl);
bool deleteSemaphore(std::string name,SEM_HDL hdl);
#endif