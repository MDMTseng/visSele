#include "BPG_Protocol.hpp"
#include "IPC_Channel.hpp"

#include <cstdlib>
#include <ctime>
#include <logctrl.h>
#include <thread>

LOG_MODULE("bpg");

BPG_Link_Interface::BPG_Link_Interface()
{
}

// int BPG_Link_Interface::fromUpperLayer(uint8_t *dat,size_t len,bool FIN);
int BPG_Link_Interface::toUpperLayer(uint8_t *dat, size_t len, bool FIN, void *peer)
{
  if (bpg_prot == NULL)
    return -1;
  return bpg_prot->fromLinkLayer(dat, len, FIN, peer);
}

//st1 ok
int BPG_Protocol_Interface::getHeaderSize()
{
  return 9;
}

//st1 ok
int BPG_Protocol_Interface::headerSetup(uint8_t *buff, size_t len, BPG_protocol_data bpg_dat) //for protocol_data to uint8_t buffer for link layer transmission
{

  buff[0] = bpg_dat.tl[0];
  buff[1] = bpg_dat.tl[1];

  buff[2] = bpg_dat.prop;

  buff[3] = bpg_dat.pgID >> 8;
  buff[4] = bpg_dat.pgID & 0xFF;

  uint32_t length = 0;
  if (bpg_dat.dat_raw)
  {
    length = bpg_dat.size;
  }
  buff[5] = length >> 24;
  buff[6] = length >> 16;
  buff[7] = length >> 8;
  buff[8] = length;

  return 0;
}

//st1 ok
static uint8_t *extractRawDataPtr(uint8_t *buff, size_t len, size_t *ret_rawDataL)
{
  if (len < 9)
  {
    if (ret_rawDataL)
      *ret_rawDataL = 0;
    return NULL;
  }
  if (ret_rawDataL)
    *ret_rawDataL = len - 9;
  return buff + 9;
}

BPG_Protocol_Interface::BPG_Protocol_Interface()
{
}
//st1 ok
// DEAD API, DO NOT ADOPT as-is: this resizes cached_data_send under
// bufferLock alone, while fromUpperLayer resizes the SAME vector under
// linkLayerLock alone -- any future caller races every normal send. Make it
// take linkLayerLock (and drop bufferLock) before reviving. Zero callers
// today (one commented-out reference in tmpCodes.cpp).
uint8_t *BPG_Protocol_Interface::requestSendingBuffer(size_t len)
{
  bufferLock.lock();
  cached_data_send.resize(getHeaderSize() + len);
  return &cached_data_send[getHeaderSize()];
}
bool BPG_Protocol_Interface::releaseSendingBuffer(uint8_t * buffer)
{
  if(&cached_data_send[getHeaderSize()]!=buffer)
  {
    return false;
  }

  bufferLock.unlock();
  return true;
}

//st1 ok
int BPG_Protocol_Interface::fromUpperLayer(BPG_protocol_data bpgdat, void *peer)
{ //serialize the BPG_protocol_data as byte stream

  const std::lock_guard<std::mutex> lock(linkLayerLock);
  _active_peer = peer; // valid for the duration of this locked send (incl. callbacks)
  // std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if (bpgdat.dat_raw != NULL || (bpgdat.dat_raw == NULL && bpgdat.size == 0 && bpgdat.callback == NULL)) //Normal case
  {
    cached_data_send.resize(bpgdat.size + getHeaderSize());
    headerSetup(&cached_data_send[0], cached_data_send.size(), bpgdat);

    //if dat_raw is here and exact on the cached_data_send buffer data position
    //usually work with requestSendingBuffer(size_t len)
    if (bpgdat.dat_raw && (bpgdat.dat_raw != &cached_data_send[getHeaderSize()]))
    {
      memcpy(&cached_data_send[getHeaderSize()], bpgdat.dat_raw, bpgdat.size);
    }
    return toLinkLayer(&cached_data_send[0], cached_data_send.size(), true, peer);
  }

  if (bpgdat.callback != NULL)
  {
    return bpgdat.callback(*this, bpgdat, bpgdat.callbackInfo);
  }
  return -1;
}

//st1 ok
static BPG_protocol_data convert(uint8_t *dat, size_t len)
{
  BPG_protocol_data bpgdat; //TODO

  bpgdat.size =
      ((uint32_t)dat[5] << 24) |
      ((uint32_t)dat[6] << 16) |
      ((uint32_t)dat[7] << 8) |
      (uint32_t)dat[8];
  // bpgdat.size= len-9;
  
  // if (bpgdat.size + 9 != len)
  // {

  //   LOGI("<<<size:%d  len:%d<<<<<", bpgdat.size, len);

  //   BPG_protocol_data errDat = {0}; //TODO
  //   return errDat;
  // }
  bpgdat.tl[0] = dat[0];
  bpgdat.tl[1] = dat[1];
  bpgdat.prop = dat[2];
  bpgdat.pgID = (((uint16_t)dat[3]) << 8) | (dat[4]);
  bpgdat.dat_raw = &(dat[9]);

  return bpgdat;
}

int BPG_Protocol_Interface::_fromLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer)
{
  vector<uint8_t> &recv_buf = recv_bufs[peer];

  // Drain every complete packet sitting in this peer's reassembly buffer.
  //
  // This used to recurse into itself once per decoded packet and, before each
  // recursion, shift the remaining bytes down to offset 0 one byte at a time.
  // A single WS frame packed with many tiny BPG packets (e.g. 1.2MB of 9-byte
  // empty packets) therefore blew the stack, and even short of that the
  // byte-by-byte shifting was O(n^2) and pinned the main thread.
  //
  // Same decisions, same order, same return value -- just a loop, and the
  // consumed prefix is dropped once at the end instead of per packet.
  const size_t headerSize = (size_t)getHeaderSize();
  size_t readOff = 0;
  int ret = 1;

  while (true)
  {
    size_t avail = recv_buf.size() - readOff;

    if (avail < headerSize)
    {
      // Not enough bytes for even the header. If the link-layer FIN flag is
      // set we have an end-of-message frame that didn't deliver a full header
      // -> corrupt; drop the partial buffer so the next clean packet syncs.
      // Otherwise wait for more bytes.
      if (FIN) { readOff = recv_buf.size(); ret = -1; }
      break;
    }

    BPG_protocol_data bpgdat = convert(&(recv_buf[readOff]), avail);

    // Hard cap on the claimed payload size. Even malicious peers can't make
    // the daemon allocate / wait for more than this.
    const uint32_t MAX_BPG_PAYLOAD = 16u * 1024u * 1024u;
    if (bpgdat.size > MAX_BPG_PAYLOAD)
    {
      readOff = recv_buf.size(); // == recv_buf.clear() below
      ret = -1;
      break;
    }

    size_t packetOffset = headerSize + (size_t)bpgdat.size;
    if (avail < packetOffset) //the packet content is not complete
    {
      // Same FIN trick: if the peer told us this is the last frame and the
      // claimed payload still hasn't arrived, the size field is lying ->
      // drop and resync. Otherwise wait for more bytes.
      if (FIN) { readOff = recv_buf.size(); ret = -1; }
      break;
    }
    // LOGI("<<<size:%d  len:%d<<<<<", bpgdat.size, avail);
    ret = toUpperLayer(bpgdat, peer);

    readOff += packetOffset;
    if (readOff >= recv_buf.size()) //buffer fully consumed
      break;
    //Still have dat in buffer, try decode
  }

  if (readOff >= recv_buf.size())
    recv_buf.clear();
  else if (readOff > 0)
    recv_buf.erase(recv_buf.begin(), recv_buf.begin() + readOff);

  return ret;
}

//st1 ok
int BPG_Protocol_Interface::fromLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer)
{ //assemble to BPG_protocol_data

  //Just accumulate into this peer's reassembly buffer
  vector<uint8_t> &recv_buf = recv_bufs[peer];
  int headIdx = recv_buf.size();
  recv_buf.resize(recv_buf.size() + len);
  memcpy(&(recv_buf[headIdx]), dat, len);

  return _fromLinkLayer(dat, len, FIN, peer);//kick event
}
uint8_t *tmp;
bool pre_FIN;
int skipLogCount;
//st1 ok
int BPG_Protocol_Interface::toLinkLayer(uint8_t *dat, size_t len, bool FIN, void *peer, int extraHeaderRoom, int extraFooterRoom)
{
  if (linkCH == NULL)
    return -1;

  // if(tmp!=dat || pre_FIN!=FIN)
  // {
  //   LOGI("DAT_PTR:%p LEN:%d FIN:%d skipLogCount:%d",dat,len,FIN,skipLogCount);
  //   skipLogCount=0;
  // }
  // skipLogCount++;
  // tmp=dat;
  // pre_FIN=FIN;
  return linkCH->fromUpperLayer(dat, len, FIN, peer, extraHeaderRoom, extraFooterRoom);
}