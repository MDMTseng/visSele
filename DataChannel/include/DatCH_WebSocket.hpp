#ifndef DATCH_WEBSOCKET_HPP
#define DATCH_WEBSOCKET_HPP

#include <DatCH.hpp>


class ws_server;

class DatCH_WebSocket: public DatCH_Interface
{
protected:
public:
	ws_server *server;
	DatCH_WebSocket(int port);
	int runLoop(struct timeval *tv);
};



#endif
