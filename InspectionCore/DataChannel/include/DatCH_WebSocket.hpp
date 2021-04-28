#ifndef DATCH_WEBSOCKET_HPP
#define DATCH_WEBSOCKET_HPP

#include <DatCH.hpp>

#include "websocket_conn.hpp"


class DatCH_WebSocket: public DatCH_Interface, public ws_protocol_callback
{
protected:
    ws_server *server;
    websock_data tmp_ws_data;

public:
    ws_conn_data *default_peer;
    DatCH_WebSocket(int port);
    ~DatCH_WebSocket();
    int findMaxFd();
    int runLoop(struct timeval *tv);

    int ws_callback(websock_data data, void* param);
    DatCH_Data SendData(void* data, size_t dataL) override;
    DatCH_Data SendData(DatCH_Data) override;
    
    int disconnect(int sock);
};



#endif
