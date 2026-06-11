#pragma once
#include <thread>
#include <unistd.h>
#include "XPlatAPI.h"
#include <mutex>
#include <atomic>
#include <exception>
#include "simple_uart.h"
#include "Data_Layer_IF.hpp"


class Data_TCP_Layer:public Data_Layer_IF
{

protected:

    std::thread *recvThread;
    int sockfd;  
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    std::timed_mutex sendLock;
public:
    Data_TCP_Layer(const char *host,int port);//throw(std::runtime_error);



    int send_data(int head_room,uint8_t *data,int len,int leg_room);
    int close();

    void connected(Data_Layer_IF* ch){}


    int recv_data_thread();

    ~Data_TCP_Layer();
};




class Data_UART_Layer:public Data_Layer_IF
{

protected:

    std::thread *recvThread;
	  struct simple_uart *uart;
    std::timed_mutex sendLock;
    // Set in close() to tell recv_data_thread to exit. close() joins the thread
    // BEFORE freeing the handle (no read-after-close -> no err=6 flap), and the
    // thread suppresses its disconnected() notify when stopRecv is set so an
    // intentional teardown doesn't masquerade as a device drop to the WebUI.
    std::atomic<bool> stopRecv{false};
public:
    Data_UART_Layer(const char *name,int speed,const char *mode_string);//throw(std::runtime_error);



    int send_data(int head_room,uint8_t *data,int len,int leg_room);
    virtual int close();

    // int recv_data(uint8_t *data,int len){}

    void connected(Data_Layer_IF* ch){}


    int recv_data_thread();

    ~Data_UART_Layer();
};



class Data_LOOPBACK_Layer:public Data_Layer_IF
{

public:
    Data_LOOPBACK_Layer()
    {

    }



    int send_data(int head_room,uint8_t *data,int len,int leg_room)
    {
      return recv_data(data,len,false);
    }

    // int recv_data(uint8_t *data,int len){}

    void connected(Data_Layer_IF* ch){}
};

