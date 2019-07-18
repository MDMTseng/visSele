#ifndef SOCK_Msg_Flow_HPP
#define SOCK_Msg_Flow_HPP
#include <thread>
#include <unistd.h>
#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h> /*htons*/
#endif


class SOCK_Msg_Flow
{
    protected:
    std::thread *recvThread;
    int sockfd;  
    char buf[100];
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    public:
    SOCK_Msg_Flow(char *host,int port) throw(int);
    virtual int start_RECV_Thread();
    virtual int getfd();
    virtual int send_data(uint8_t *data,int len);
    virtual int recv_data();

    virtual int recv_data_thread();
    ~SOCK_Msg_Flow();
};

#endif