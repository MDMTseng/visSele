#ifndef DATCH_WEBSOCKET_HPP
#define DATCH_WEBSOCKET_HPP

#include <DatCH.hpp>

#include "websocket_conn.hpp"


class DatCH_WebSocket: public DatCH_Interface
{
protected:
public:
    ws_server *server;
    websock_data tmp_ws_data;
    DatCH_WebSocket(int port);
    int runLoop(struct timeval *tv);
};



#endif
