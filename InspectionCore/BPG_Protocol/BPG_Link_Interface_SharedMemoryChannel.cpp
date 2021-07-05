#include "BPG_Protocol.hpp"
#include "IPC_Channel.hpp"

#include <cstdlib>
#include <ctime>
#include <logctrl.h>
#include "websocket_conn.hpp"

#include <ws_server_util.h>
#include <exception>
#include <stdexcept>

BPG_Link_Interface_SharedMemoryChannel::BPG_Link_Interface_SharedMemoryChannel()
{
  init("__BPG_LINK_IF_SharedMemory__",4096);
}
BPG_Link_Interface_SharedMemoryChannel::BPG_Link_Interface_SharedMemoryChannel(std::string smem_name,size_t max_size)
{
  init(smem_name,4096);
}

void BPG_Link_Interface_SharedMemoryChannel::init(std::string name,size_t max_size)
{

  sendCh=new smem_channel("s_"+name,max_size,true);
  recvCh=new smem_channel("r_"+name,max_size,true);
}

BPG_Link_Interface_SharedMemoryChannel::~BPG_Link_Interface_SharedMemoryChannel()
{
  delete sendCh;
  delete recvCh;
}

int BPG_Link_Interface_SharedMemoryChannel::fromUpperLayer(uint8_t *dat, size_t len, bool FIN)
{
  memcpy(sendCh->getPtr(),dat,len);
  sendCh->s_post();
  sendCh->s_wait_remote();
  // if(isInContPktState)
  // {
  // }

  // if (default_peer == NULL)
  //   return -1;

  // websock_data packet;
  // packet.peer = default_peer;
  // packet.data.data_frame.rawL = len;
  // packet.data.data_frame.raw = dat;
  // packet.data.data_frame.isFinal = FIN;
  // packet.data.data_frame.type = (isInContPktState == false) ? WS_DFT_BINARY_FRAME : WS_DFT_CONT_FRAME;

  // // LOGI("<<<<FINAL:%d  type:%d>>>>",packet.data.data_frame.isFinal,packet.data.data_frame.type);
  // server->send_pkt(&packet);

  // isInContPktState = !FIN;
  return 0;
}
