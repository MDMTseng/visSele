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
    typedef union content
    {
        typedef struct _DATA_FRAME
        {
            enum FrameType
            {
                EMPTY_FRAME = 0xF0,
                ERROR_FRAME = 0xF1,
                INCOMPLETE_FRAME = 0xF2,
                CONT_FRAME = 0x00,
                TEXT_FRAME = 0x01,
                BINARY_FRAME = 0x02,
                PING_FRAME = 0x09,
                PONG_FRAME = 0x0A,
                OPENING_FRAME = 0xF3,
                CLOSING_FRAME = 0x08
            } type;
            uint8_t *raw;
        } data_frame;

        typedef struct _ERROR
        {
            int code;
        } error;
    } data;

};



class ws_conn_data {

protected:
    const int recvBufSizeInc = 1024;
    int ws_state;
    size_t accBufDataLen;
    int sock;
    struct sockaddr_in addr;
    char resource[128];

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
};



#endif
