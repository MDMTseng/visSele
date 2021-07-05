#ifndef _BPG_IPC_INTERFACE_HPP
#define _BPG_IPC_INTERFACE_HPP
#include <BPG_Protocol.hpp>
#include <vector>
#include <string>
#include <regex>
#include "websocket_conn.hpp"
#include "smem_channel.hpp"
using namespace std;
class BPG_Protocol_Interface;
class BPG_Link_Interface
{
protected:
  BPG_Protocol_Interface *bpg_prot;

public:
  BPG_Link_Interface();

  void interfaceEvent();
  virtual int fromUpperLayer(uint8_t *dat, size_t len, bool FIN) = 0;
  virtual int toUpperLayer(uint8_t *dat, size_t len, bool FIN);
};

class BPG_Link_Interface_WebSocket : public BPG_Link_Interface, public ws_protocol_callback
{
protected:
  void init(int port);
  ws_server *server;
  websock_data tmp_ws_data;
  ws_conn_data *default_peer;

  bool isInContPktState;

public:
  BPG_Link_Interface_WebSocket(int port);
  BPG_Link_Interface_WebSocket();

  void setUpperLayer(BPG_Protocol_Interface *ulayer) { bpg_prot = ulayer; }

  int fromUpperLayer(uint8_t *dat, size_t len, bool FIN);

  ~BPG_Link_Interface_WebSocket();
  int findMaxFd();
  fd_set get_fd_set();
  int runLoop(fd_set *read_fds, struct timeval *tv);

  virtual int ws_callback(websock_data data, void *param);
  // DatCH_Data SendData(void* data, size_t dataL) override;
  // DatCH_Data SendData(DatCH_Data) override;

  int disconnect(int sock);
};



class BPG_Link_Interface_SharedMemoryChannel : public BPG_Link_Interface
{

  smem_channel* sendCh;
  smem_channel* recvCh;

  uint8_t* hold_data=NULL;
  void init(string name,size_t max_size);

  bool isInContPktState;

public:
  BPG_Link_Interface_SharedMemoryChannel(string name,size_t max_size);
  BPG_Link_Interface_SharedMemoryChannel();
  

  int fromUpperLayer(uint8_t *dat, size_t len, bool FIN);

  ~BPG_Link_Interface_SharedMemoryChannel();


  void recv_wait()
  {
    recvCh->r_wait();
    hold_data=(uint8_t*)recvCh->getPtr();
  }

  void recv_release()
  {
    hold_data=NULL;
    recvCh->r_release();
  }

};

#endif
