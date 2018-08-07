
#include "DatCH_WebSocket.hpp"

#include <ws_server_util.h>

DatCH_WebSocket::DatCH_WebSocket(int port): DatCH_Interface()
{
    server=new ws_server(port);
}



int DatCH_WebSocket::runLoop(struct timeval *tv)
{
	server->runLoop(tv);
	return 0;
}