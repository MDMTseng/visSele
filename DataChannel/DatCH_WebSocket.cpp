
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

    switch(data.type)
    {
        case websock_data::eventType::OPENING:

            printf("OPENING peer %s:%d\n",
               inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
    
        break;
        case websock_data::eventType::DATA_FRAME:
            printf("DATA_FRAME >> frameType:%d frameL:%d data_ptr=%p\n",
                data.data.data_frame.type,
                data.data.data_frame.rawL,
                data.data.data_frame.raw
                );
            if(data.data.data_frame.raw)
            {
                data.data.data_frame.raw[data.data.data_frame.rawL]='\0';
                printf(">>>>>%s\n",
                    data.data.data_frame.raw
                    );
            }

        break;
        case websock_data::eventType::CLOSING:

            printf("CLOSING peer %s:%d\n",
               inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
    
        break;
    }

    return 0;
}