
#include <sp.hpp>
int createSharedMemory(std::string name, size_t maxSize, ShareMemoryInfo *ret_info)
{
	if (ret_info == NULL)
	{
		return -3;
	}

	ret_info->handle = NULL;
	ret_info->ptr = NULL;
	ret_info->name = "";

	HANDLE mapping = CreateFileMapping(
		INVALID_HANDLE_VALUE,
		NULL,
		PAGE_READWRITE,
		0,
		(int64_t)maxSize,
		name.c_str());

	if (mapping == nullptr)
	{
		return -1;
	}

	void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, (int64_t)maxSize);
	if (ptr == nullptr)
	{
		CloseHandle(mapping);
		return -2;
	}

	ret_info->handle = mapping;
	ret_info->ptr = ptr;

	ret_info->name = name;
	return 0;
}

int connSharedMemory(std::string name, size_t maxSize, ShareMemoryInfo *ret_info)
{

	if (ret_info == NULL)
	{
		return -3;
	}

	ret_info->handle = NULL;
	ret_info->ptr = NULL;
	ret_info->name = "";

	HANDLE mapping = OpenFileMapping(
		FILE_MAP_ALL_ACCESS, false,
		name.c_str());

	if (mapping == nullptr)
	{
		return -1;
	}

	void *ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (ptr == nullptr)
	{
		CloseHandle(mapping);
		return -2;
	}

	ret_info->handle = mapping;
	ret_info->ptr = ptr;
	ret_info->name = name;
	return 0;
}
bool deleteSharedMemory(ShareMemoryInfo info)
{

	return CloseHandle(info.handle);
}


SEM_HDL createSemaphore(std::string name)
{
	SEM_HDL m_hSemaphore = CreateSemaphoreA(
		NULL, 0,1, ("Global\\"+name).c_str());

	return m_hSemaphore;
}

SEM_HDL connMutex(std::string name)
{
	SEM_HDL m_hSemaphore = CreateSemaphoreA(
		NULL, 0,1, ("Global\\"+name).c_str());

	return m_hSemaphore;
}


bool IncSemaphore(SEM_HDL hdl)
{
	if (!ReleaseSemaphore(
			hdl,   // 要增加的信号量。
			1,	   // 增加1.
			NULL)) // 不想返回前一次信号量。
	{
		return false;
	}
	return true;
}


bool DecSemaphore(SEM_HDL hdl)
{
	WaitForSingleObject( hdl,INFINITE);
	return true;
}

