

#include <vector>
#include "websocket.h"

#include <unistd.h>


#include "websocket_conn.hpp"


class ws_conn_entity_pool;

class ws_server;

class ws_conn: public ws_conn_data {

    std::vector <uint8_t> recvBuf;
    std::vector <uint8_t> sendBuf;

    static int safeSend(int sock, const uint8_t *buffer, size_t bufferSize);
public:


    ws_conn() ;

    void RESET();


    int setSocket(int socket);

    int setAddr(struct sockaddr_in address);

    void COPY_property(ws_conn *from);

    static int strcpy_m(char *dst, int dstMaxSize, char *src);

    int doHandShake(void *buff, ssize_t buffLen);

    int doClosing();

    int event_WsRECV(uint8_t *data, size_t dataSize, 
      enum wsFrameType frameType, bool isFinal);

    int doNormalRecv(void *buff, size_t buffLen, 
      size_t *ret_restLen, enum wsFrameType *ret_lastFrameType);

    enum wsFrameType lastPktType;
    int runLoop();

};


class ws_conn_entity_pool {

    std::vector <ws_conn> ws_conn_set;

public:
    ws_conn *find(int sock);

    std::vector <ws_conn>* getServers();

    int remove(int sock);


    ws_conn *find_avaliable_conn_info_slot();

    ws_conn* add(ws_conn *info);

    int size();

};



class ws_server {

    int listenSocket;
    fd_set evtSet;
    int fdmax;
public:
    ws_server(int port);
    ws_conn_entity_pool ws_conn_pool;

    int findMaxFd();
    int runLoop(struct timeval *tv);
};

