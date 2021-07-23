#ifndef _BPG_Protocol_HPP
#define _BPG_Protocol_HPP

#include <vector>
#include <string>
#include <BPG_Link_Interface.hpp>
#include <regex>
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
  vector<uint8_t> cached_data_recv;
  vector<uint8_t> cached_data_send;
  BPG_Link_Interface *linkCH;
  
  int _fromLinkLayer(uint8_t *dat, size_t len, bool FIN);
public:
  static int getHeaderSize();
  static int headerSetup(uint8_t *buff, size_t len, BPG_protocol_data bpg_dat);
  BPG_Protocol_Interface();
  void setLink(BPG_Link_Interface *link) { linkCH = link; }
  uint8_t *requestSendingBuffer(size_t len);
  virtual int toUpperLayer(BPG_protocol_data bpgdat) = 0;
  int fromUpperLayer(BPG_protocol_data bpgdat);
  int fromLinkLayer(uint8_t *dat, size_t len, bool FIN = true);
  //enough header&footer room would allow lower layer use the buffer and fill the header in dat
  //[extraHeaderRoom][dat:len][extraFooterRoom]
  int toLinkLayer(uint8_t *dat, size_t len, bool FIN = true,int extraHeaderRoom=0, int extraFooterRoom=0);
};

#endif
