#ifndef __WIN_DEF_H__
#define __WIN_DEF_H__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>

typedef unsigned int UINT;
typedef int BOOL;
typedef int INT;
typedef unsigned short USHORT;
typedef void* LPVOID;
typedef void* PVOID;
typedef char CHAR;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef sockaddr_in SOCKADDR_IN;
typedef int WSADATA;
typedef size_t DWORD_PTR;

#define SOCKADDR            sockaddr
#define WSAGetLastError()   errno
#define SOCKET_ERROR        -1
#define closesocket         close
#define TRACE(...)
#define SOCKET              int
#define WSACleanup(...)
#define MessageBox(...)

#ifndef TRUE
#define TRUE        1
#endif

#ifndef FALSE
#define FALSE       0
#endif

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define LOWORD(l)           ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l)           ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w)           ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w)           ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))

static void Sleep(int micro_seconds) { usleep(micro_seconds * 1000); }

static unsigned long GetTickCount()
{
	struct timeval ts;
    gettimeofday(&ts, NULL);
    return  (ts.tv_sec*1000 + ts.tv_usec/1000);
}

static int WSAStartup(WORD wVersionRequested, WSADATA *lpWSAData) { return 0; }

static int set_socket_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

class CCriticalSection
{
public:

    CCriticalSection()
    {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

        pthread_mutex_init(&m_mutex, &attr);

        pthread_mutexattr_destroy(&attr);
    }

    ~CCriticalSection()
    {
        pthread_mutex_destroy(&m_mutex);
    }

    void Lock() { pthread_mutex_lock(&m_mutex); }
    void Unlock() { pthread_mutex_unlock(&m_mutex); }

private:

    pthread_mutex_t m_mutex;

};

static void EnterCriticalSection(CCriticalSection *cs) { cs->Lock(); }
static void LeaveCriticalSection(CCriticalSection *cs) { cs->Unlock(); }


#endif
