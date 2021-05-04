#ifndef __XPLATAPI_H__
#define __XPLATAPI_H__


#ifdef __WIN32__
# include <winsock2.h>
#define socklen_t int
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


#endif

