
#include "DatCH_WebSocket.hpp"

#include <ws_server_util.h>
#include <exception>
#include <stdexcept>

DatCH_WebSocket::DatCH_WebSocket(int port): DatCH_Interface(),ws_protocol_callback(this)
{

    server = new ws_server(port,this);
    if(server->get_socket()<0)
    {
        throw std::invalid_argument("WS Server INIT Failed..");
    }
    
    default_peer = NULL;
}


int DatCH_WebSocket::findMaxFd()
{
    
    return server->findMaxFd();
}

fd_set DatCH_WebSocket::get_fd_set()
{    
    return server->get_fd_set();
}

int DatCH_WebSocket:: runLoop(fd_set *read_fds,struct timeval *tv)
{
    return server->runLoop(read_fds,tv);
}
int DatCH_WebSocket::ws_callback(websock_data data, void* param)
{
    // printf("%s:DatCH_WebSocket type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());
    if(data.type == websock_data::CLOSING || data.type == websock_data::ERROR_EV)
    {
      if(data.peer == default_peer)
        default_peer = NULL;
    }


    if(cb_obj)
    {
        DatCH_Data ws_data;
        ws_data.type = DatCH_Data::DataType_websock_data;
        ws_data.data.p_websocket = &data;
        cb_obj->callback(this, ws_data, callback_param);
    }
    return 0;
}

DatCH_WebSocket::~DatCH_WebSocket()
{
    delete server;
}


DatCH_Data DatCH_WebSocket::SendData(void* data, size_t dataL)
{
    if(default_peer == NULL)
    {
        return GenErrorMsg(DatCH_Data_error::SEND_ERROR);
    }
    DatCH_Data data_pkt;
    websock_data ws_data;
    data_pkt.type=DatCH_Data::DataType_websock_data;
    ws_data.type=websock_data::DATA_FRAME;
    ws_data.peer = default_peer;
    ws_data.data.data_frame.type= WS_DFT_BINARY_FRAME;
    ws_data.data.data_frame.raw = (uint8_t *)data;
    ws_data.data.data_frame.rawL = dataL;
    ws_data.data.data_frame.isFinal=true;
    data_pkt.data.p_websocket = &ws_data;

    return SendData(data_pkt);
}


DatCH_Data DatCH_WebSocket::SendData(DatCH_Data ws_data)
{
    if(ws_data.type!=DatCH_Data::DataType_websock_data )
    {
        return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
    }
    int ret = server->send_pkt(ws_data.data.p_websocket);
    if( ret != 0)
    {
        return GenErrorMsg(DatCH_Data_error::SEND_ERROR);
    }

    return GenMsgType(DatCH_Data::DataType_ACK);
}


int DatCH_WebSocket::disconnect(int sock)
{
    return  server->disconnect(sock);
}