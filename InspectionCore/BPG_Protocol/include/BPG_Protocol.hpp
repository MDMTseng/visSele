#ifndef _BPG_Protocol_HPP
#define _BPG_Protocol_HPP

#include <vector>
#include <string>
#include <map>
#include <BPG_Link_Interface.hpp>
#include <regex>
#include <mutex>

using namespace std;
class BPG_Link_Interface;
typedef int (*BPG_protocol_data_feed_callback)(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);

struct BPG_protocol_data//Binary pack group
{
  char tl[2]; //Two letter
  uint8_t prop;
  uint16_t pgID;
  uint32_t size; //32bit
  uint8_t *dat_raw;
  BPG_protocol_data_feed_callback callback; //For bulk data transfer
  void *callbackInfo;
};

class BPG_Protocol_Interface
{
protected:
  // Per-peer inbound BPG reassembly buffers, keyed by the opaque peer handle.
  // (Interleaved fragments from concurrent clients must not share a buffer.)
  std::map<void *, vector<uint8_t>> recv_bufs;
  vector<uint8_t> cached_data_send;
  BPG_Link_Interface *linkCH;

  std::mutex bufferLock;
  std::mutex linkLayerLock;
  // Peer targeted by the in-progress send; valid while linkLayerLock is held,
  // so bulk-transfer callbacks (which can't take a peer arg) can route correctly.
  void *_active_peer = NULL;
  int _fromLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer);

public:
  static int getHeaderSize();
  static int headerSetup(uint8_t *buff, size_t len, BPG_protocol_data bpg_dat);
  BPG_Protocol_Interface();
  void setLink(BPG_Link_Interface *link) { linkCH = link; }
  uint8_t *requestSendingBuffer(size_t len);
  bool releaseSendingBuffer(uint8_t * buffer);
  // peer == NULL means "default/broadcast target" handled by the link layer.
  virtual int toUpperLayer(BPG_protocol_data bpgdat, void *peer) = 0;
  int fromUpperLayer(BPG_protocol_data bpgdat, void *peer);
  int fromUpperLayer(BPG_protocol_data bpgdat) { return fromUpperLayer(bpgdat, NULL); }
  int fromLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer);
  // Free a disconnected peer's reassembly buffer.
  void dropPeerState(void *peer) { recv_bufs.erase(peer); }
  // Peer targeted by the send currently in progress (for bulk-transfer callbacks).
  void *activePeer() { return _active_peer; }
  //enough header&footer room would allow lower layer use the buffer and fill the header in dat
  //[extraHeaderRoom][dat:len][extraFooterRoom]
  int toLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer, int extraHeaderRoom=0, int extraFooterRoom=0);
};

#endif
