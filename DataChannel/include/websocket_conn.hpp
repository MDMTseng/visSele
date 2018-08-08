#ifndef WEBSOCKET_CONNECTION_HPP
#define WEBSOCKET_CONNECTION_HPP

#ifdef __WIN32__
# include <winsock2.h>
#define socklen_t int
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif


class ws_server;
class ws_conn_data;

typedef struct websock_data
{
    enum eventType
    {
        OPENING,
        HAND_SHAKING,
        DATA_FRAME,
        CLOSING,
        ERROR,
    } type;

    ws_conn_data* peer;
    union content
    {
        struct _DATA_FRAME
        {
            int type;
            uint8_t *raw;
            size_t rawL;
            bool isFinal;
        } data_frame;

        typedef struct _ERROR
        {
            int code;
        } error;
    } data;

};


class ws_protocol_callback{
public:
    void* param;
    ws_protocol_callback(void* param)
    {
        this->param=param;
    }
    virtual int ws_callback(websock_data data, void* param)
    {
        return 0;
    }
    virtual int ws_callback(websock_data data)
    {
        return ws_callback(data,param);
    }
};

class ws_conn_data {

protected:
    const int recvBufSizeInc = 1024;
    int ws_state;
    size_t accBufDataLen;
    int sock;
    struct sockaddr_in addr;
    char resource[128];
    ws_protocol_callback *cb;

public:

    int getSocket()
    {
        return sock;
    }

    struct sockaddr_in getAddr()
    {
        return addr;
    }

    bool isOccupied()
    {
        return sock != -1;
    }
    const char* getResource()
    {
        return resource;
    }
};



#endif
