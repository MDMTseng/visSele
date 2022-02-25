#ifndef SOCK_Msg_Flow_HPP
#define SOCK_Msg_Flow_HPP
#include <thread>
#include <unistd.h>
#include "XPlatAPI.h"
#include <mutex>
#include "simple_uart.h"


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


class Data_Layer_IF
{

    protected:
    Data_Layer_IF *uplayer_df;
    Data_Layer_IF *downlayer_df;
    public:
    Data_Layer_IF(){
      uplayer_df=downlayer_df=NULL;
    }

    
    int setDLayer(Data_Layer_IF *down)
    {
      if(downlayer_df)
      {
        // downlayer_df->close();//supposedly downlayer should call the disconnected at here
        delete downlayer_df;
      }
      downlayer_df=down;
      down->setULayer(this);
      return 0;
    }
    
    int setULayer(Data_Layer_IF *up)
    {
      if(uplayer_df)
      {
        uplayer_df->disconnected(this);
      }
      uplayer_df=up;

      return 0;
    }

    //we call these memenber
    virtual int close()=0;
    
    virtual int send_data(int head_room,uint8_t *data,int len,int leg_room){
      if(downlayer_df==NULL)return -1;
      return downlayer_df->send_data(head_room,data,len,leg_room);
    }

    //this should be called from downlayer
    virtual int recv_data(uint8_t *data,int len){
      if(uplayer_df==NULL)return -1;
      return uplayer_df->recv_data(data,len);
    }
    virtual void connected(Data_Layer_IF* ch)=0;
    virtual void disconnected(Data_Layer_IF* ch)=0;

    ~Data_Layer_IF()
    {
      if(downlayer_df)
      {
        delete downlayer_df;
      }
    }
};





class Data_TCP_Layer:public Data_Layer_IF
{

protected:

    std::thread *recvThread;
    int sockfd;  
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    std::timed_mutex sendLock;
public:
    Data_TCP_Layer(char *host,int port)throw(std::runtime_error);



    int send_data(int head_room,uint8_t *data,int len,int leg_room);
    int close();

    int recv_data(uint8_t *data,int len){}

    void connected(Data_Layer_IF* ch){}

    void disconnected(Data_Layer_IF* ch){}

    int recv_data_thread();

    ~Data_TCP_Layer();
};




class Data_UART_Layer:public Data_Layer_IF
{

protected:

    std::thread *recvThread;
	  struct simple_uart *uart;
    std::timed_mutex sendLock;
public:
    Data_UART_Layer(const char *name,int speed,const char *mode_string)throw(std::runtime_error);



    int send_data(int head_room,uint8_t *data,int len,int leg_room);
    int close();

    // int recv_data(uint8_t *data,int len){}

    void connected(Data_Layer_IF* ch){}

    void disconnected(Data_Layer_IF* ch){}

    int recv_data_thread();

    ~Data_UART_Layer();
};



#endif