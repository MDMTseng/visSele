
#include "DatCH_WebSocket.hpp"

#include <ws_server_util.h>

DatCH_WebSocket::DatCH_WebSocket(int port): DatCH_Interface(),ws_protocol_callback(this)
{

    server = new ws_server(port,this);
}


int DatCH_WebSocket::runLoop(struct timeval *tv)
{
    server->runLoop(tv);
    return 0;
}

int DatCH_WebSocket::ws_callback(websock_data data, void* param)
{

    printf("%s:DatCH_WebSocket type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());

    printf("peer %s:%d\n",
           inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
    
    return 0;
}