
#include <sp.hpp>



SEM_HDL createSemaphore(std::string name)
{
  return sem_open(name.c_str(), O_CREAT | O_EXCL, 0644, 0);
}
bool IncSemaphore(SEM_HDL hdl)
{
  sem_post(hdl);
  return true;
}
bool DecSemaphore(SEM_HDL hdl)
{
  sem_wait(hdl);
  return true;
}

bool deleteSemaphore(std::string name,SEM_HDL hdl)
{
  sem_close(hdl);
  sem_unlink(name.c_str());
  return true;
}






int createSharedMemory(std::string name,size_t maxSize,ShareMemoryInfo *ret_info)
{
	if(ret_info==NULL)
	{
		return -3;
	}

	
	ret_info->handle=0;
	ret_info->ptr=NULL;
    ret_info->name="";
	

    SHM_HDL shm_id = shm_open(
        name.c_str(), O_RDWR | O_CREAT, 0644); /*第一步:建立共享記憶體區*/
    if (shm_id == -1) {
      return -2;
    }
    ftruncate(shm_id, maxSize);

    void *ptr = mmap(NULL,
                        maxSize,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        shm_id,
                        0); /*連線共享記憶體區*/

	ret_info->handle = shm_id;
	ret_info->ptr = ptr;
    ret_info->name=name;
	return 0;
}


int connSharedMemory(std::string name,size_t maxSize,ShareMemoryInfo *ret_info)
{
	

	if(ret_info==NULL)
	{
		return -3;
	}

	
	ret_info->handle=0;
	ret_info->ptr=NULL;
    ret_info->name="";
	

    SHM_HDL shm_id = shm_open(name.c_str(), O_RDWR, 0); /*第一步:開啟共享記憶體區*/
   
    if (shm_id == -1) {
      return -2;
    }
    void *ptr = mmap(NULL,
                        maxSize,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        shm_id,
                        0); /*連線共享記憶體區*/

	ret_info->handle = shm_id;
	ret_info->ptr = ptr;
  ret_info->name=name;
	return 0;
}
bool deleteSharedMemory(ShareMemoryInfo *info)
{
	
	return shm_unlink(info->name.c_str());
}