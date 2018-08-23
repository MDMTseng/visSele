
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
    //printf("%s:DatCH_WebSocket type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());
    if(callback)
    {
        DatCH_Data ws_data;
        ws_data.type = DatCH_Data::DataType_websock_data;
        ws_data.data.p_websocket = &data;
        callback(this, ws_data, callback_param);
    }
    return 0;
}

DatCH_WebSocket::~DatCH_WebSocket()
{
    delete server;
}

DatCH_Data DatCH_WebSocket::SendData(DatCH_Data ws_data)
{
    DatCH_Data null_data;
    null_data.type = DatCH_Data::DataType_error;
    null_data.data.error.code=DatCH_Data_error::NOT_SUPPORTED;

    if(ws_data.type!=DatCH_Data::DataType_websock_data )
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }

    if(server->send_pkt(ws_data.data.p_websocket) != 0)
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }

    DatCH_Data ack_ret;
    ack_ret.type = DatCH_Data::DataType_ACK;
    return ack_ret;
}
