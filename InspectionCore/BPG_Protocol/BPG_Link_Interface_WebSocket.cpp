#include "BPG_Protocol.hpp"
#include "IPC_Channel.hpp"

#include <cstdlib>
#include <ctime>
#include <logctrl.h>
#include "websocket_conn.hpp"

#include <ws_server_util.h>
#include <exception>
#include <stdexcept>





BPG_Link_Interface_WebSocket::BPG_Link_Interface_WebSocket() : ws_protocol_callback(this)
{
  init(5714);
}
BPG_Link_Interface_WebSocket::BPG_Link_Interface_WebSocket(int port) : ws_protocol_callback(this)
{
  init(port);
}

void BPG_Link_Interface_WebSocket::init(int port)
{
  isInContPktState = false;
  server = new ws_server(port, this);
  if (server->get_socket() < 0)
  {
    throw std::invalid_argument("WS Server INIT Failed..");
  }

  default_peer = NULL;
}

BPG_Link_Interface_WebSocket::~BPG_Link_Interface_WebSocket()
{
  delete server;
}

int BPG_Link_Interface_WebSocket::findMaxFd()
{

  return server->findMaxFd();
}

fd_set BPG_Link_Interface_WebSocket::get_fd_set()
{
  return server->get_fd_set();
}

int BPG_Link_Interface_WebSocket::runLoop(fd_set *read_fds, struct timeval *tv)
{
  return server->runLoop(read_fds, tv);
}

int BPG_Link_Interface_WebSocket::fromUpperLayer(uint8_t *dat, size_t len, bool FIN)
{
  // if(isInContPktState)
  // {
  // }

  if (default_peer == NULL)
    return -1;

  websock_data packet;
  packet.peer = default_peer;
  packet.data.data_frame.rawL = len;
  packet.data.data_frame.raw = dat;
  packet.data.data_frame.isFinal = FIN;
  packet.data.data_frame.type = (isInContPktState == false) ? WS_DFT_BINARY_FRAME : WS_DFT_CONT_FRAME;

  // LOGI("<<<<FINAL:%d  type:%d>>>>",packet.data.data_frame.isFinal,packet.data.data_frame.type);
  server->send_pkt(&packet);

  isInContPktState = !FIN;
  return 0;
}

int BPG_Link_Interface_WebSocket::ws_callback(websock_data data, void *param)
{

  switch(data.type)
  {
    case websock_data::CLOSING:
    case websock_data::ERROR_EV: 
    {
      if (data.peer == default_peer)
        default_peer = NULL;
    }
    return 0;

    case websock_data::HAND_SHAKING_FINISHED:
    {
      if (default_peer != NULL && default_peer != data.peer)
      {
        disconnect(data.peer->getSocket());
        return 1;
      }
      
      if (default_peer == NULL)
        default_peer = data.peer;
    }
    return 0;



    case websock_data::DATA_FRAME:

    {
      data.data.data_frame.raw[data.data.data_frame.rawL] = '\0';
      // LOGI(">>>>data raw:%s", data.data.data_frame.raw);
      websock_data packet = data;
      packet.type = websock_data::DATA_FRAME;

      if (bpg_prot)
      {
        toUpperLayer(data.data.data_frame.raw, data.data.data_frame.rawL, data.data.data_frame.isFinal);
      }
      else
      {
        return -1;
      }
    }
    return 0;

  }

  return -3;
}

int BPG_Link_Interface_WebSocket::disconnect(int sock)
{
  return server->disconnect(sock);
}