#include "BPG_Protocol.hpp"
#include "IPC_Channel.hpp"

#include <cstdlib>
#include <ctime>
#include <logctrl.h>
#include "websocket_conn.hpp"

#include <ws_server_util.h>
#include <exception>
#include <stdexcept>





LOG_MODULE("bpg.ws");

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

int BPG_Link_Interface_WebSocket::fromUpperLayer(uint8_t *dat, size_t len, bool FIN, void *peer, int extraHeaderRoom, int extraFooterRoom)
{
  // Route to the requested peer; NULL falls back to the default (first) peer
  // so legacy broadcast-style pushes keep working.
  ws_conn_data *target = (peer != NULL) ? (ws_conn_data *)peer : default_peer;
  if (target == NULL)
    return -1;

  websock_data packet;
  // websock_data has no constructor, so `type` is stack garbage unless set --
  // and ws_conn::send_pkt branches on it FIRST: garbage that equals CLOSING
  // (7) would run doClosing() on this SENDER thread, inside whatever locks it
  // holds (pushToSubscribers holds subscribersLock -> the CLOSING callback
  // retakes it -> self-deadlock), and would break the invariant that
  // teardown (pendingCloseFd, RESET) is main-WS-thread-only.
  packet.type = websock_data::DATA_FRAME;
  packet.peer = target;
  packet.data.data_frame.rawL = len;
  packet.data.data_frame.raw = dat;
  packet.data.data_frame.isFinal = FIN;
  packet.data.data_frame.type = (isInContPktState == false) ? WS_DFT_BINARY_FRAME : WS_DFT_CONT_FRAME;
  int maxWsHeaderSize=10;
  if(extraHeaderRoom>=maxWsHeaderSize)//bigger than maxmum header size
  {
    packet.data.data_frame.extraHeaderRoom=extraHeaderRoom;
  }
  else
  {
    packet.data.data_frame.extraHeaderRoom=0;
  }
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
      if (bpg_prot)
        bpg_prot->dropPeerState(data.peer);
      if (data.peer == default_peer)
        default_peer = NULL;
    }
    return 0;

    case websock_data::HAND_SHAKING_FINISHED:
    {
      // Multi-client: remember the first peer as the default broadcast target,
      // but do not reject additional peers.
      if (default_peer == NULL)
        default_peer = data.peer;
    }
    return 0;



    case websock_data::DATA_FRAME:

    {
      if (data.data.data_frame.raw == NULL)
        return -1;
      // No NUL is written at raw[rawL] any more.
      //
      // It served nothing: toUpperLayer takes an explicit length, and the BPG
      // reassembly layer copies exactly rawL bytes, so the terminator never
      // travelled with the payload. The one reader was the commented-out LOGI
      // below. Meanwhile it wrote one byte PAST the payload -- and when a
      // single recv() carries several pipelined WS frames, that byte is the
      // next frame's FIN/opcode. Zeroing it turns that frame into opcode 0,
      // a continuation frame, and the rest of the batch is misparsed.
      //
      // Payloads that reach cJSON_Parse are NUL-terminated by the sender (the
      // WebUI allocates body.length + 1); wiringPanel's toUpperLayer verifies
      // that with memchr and refuses the packet if it is missing, which is
      // exactly the behaviour this line was never actually providing.
      // LOGI(">>>>data raw:%.*s", data.data.data_frame.rawL, data.data.data_frame.raw);
      if (bpg_prot)
      {
        toUpperLayer(data.data.data_frame.raw, data.data.data_frame.rawL, data.data.data_frame.isFinal, data.peer);
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