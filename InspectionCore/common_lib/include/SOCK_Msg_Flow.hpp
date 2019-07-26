#ifndef SOCK_Msg_Flow_HPP
#define SOCK_Msg_Flow_HPP
#include <thread>
#include <unistd.h>
#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h> /*htons*/
#endif
#include <mutex>


class SOCK_Msg_Flow
{
    protected:
    std::thread *recvThread;
    int sockfd;  
    uint8_t* buf;
    int bufL;
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    public:
    SOCK_Msg_Flow(char *host,int port) throw(int);
    
    int buffLength(int length);
    virtual int start_RECV_Thread();
    virtual int getfd();
    virtual int send_data(uint8_t *data,int len);
    virtual int recv_data();

    virtual int recv_data_thread();
    virtual ~SOCK_Msg_Flow();
};


class json_seg_parser
{
    protected:
    char pch;
    int jsonInStrState;
    int jsonCurlyB_C;
    int jsonSquareB_C;

    public:
    json_seg_parser();
    void reset();
    int newChar(char ch);
};


class SOCK_JSON_Flow:public SOCK_Msg_Flow
{
    protected:
    json_seg_parser jsp;
    char jsonBuff[1024];
    char errorLock;
    public:

    std::timed_mutex syncLock;

    SOCK_JSON_Flow(char *host,int port) throw(int);

    int recv_data_thread();


    int cmd_cameraCalib(char* img_path, int board_w, int board_h);

    char* SYNC_cmd_cameraCalib(char* img_path, int board_w, int board_h);

    ~SOCK_JSON_Flow();
};

#endif