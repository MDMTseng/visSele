#ifndef SOCK_Msg_Flow_HPP
#define SOCK_Msg_Flow_HPP
#include <thread>
#include <unistd.h>
#include "XPlatAPI.h"
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
    std::timed_mutex sendLock;

    public:
    SOCK_Msg_Flow(char *host,int port) throw(std::runtime_error);
    
    int buffLength(int length);
    virtual int start_RECV_Thread();
    virtual int getfd();
    virtual int send_data(uint8_t *data,int len);
    virtual int recv_data();

    virtual int recv_data_thread();
    virtual ~SOCK_Msg_Flow();
    virtual int ev_on_close(){return 0;};
    virtual void DESTROY();
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
    virtual void reset();
    virtual int newChar(char ch);
};


class SOCK_JSON_Flow:public SOCK_Msg_Flow
{
    protected:
    json_seg_parser jsp;
    char jsonBuff[1024];
    char errorLock;


    public:

    std::timed_mutex syncLock;

    SOCK_JSON_Flow(char *host,int port) throw(std::runtime_error);

    virtual int ev_on_close() override;
    virtual int recv_data_thread();
    virtual int recv_json( char* json_str, int json_strL);

    virtual int cmd_cameraCalib(char* img_path, int board_w, int board_h);

    virtual char* SYNC_cmd_cameraCalib(char* img_path, int board_w, int board_h);

    virtual ~SOCK_JSON_Flow();
};

#endif