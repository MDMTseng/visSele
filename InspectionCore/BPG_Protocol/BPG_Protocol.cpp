#include "BPG_Protocol.hpp"
#include "IPC_Channel.hpp"

#include <cstdlib>
#include <ctime>
#include <logctrl.h>

BPG_Link_Interface::BPG_Link_Interface()
{
}

// int BPG_Link_Interface::fromUpperLayer(uint8_t *dat,size_t len,bool FIN);
int BPG_Link_Interface::toUpperLayer(uint8_t *dat, size_t len, bool FIN)
{
  if (bpg_prot == NULL)
    return -1;
  return bpg_prot->fromLinkLayer(dat, len, FIN);
}

//st1 ok
static int getHeaderSize()
{
  return 9;
}

//st1 ok
static int headerSetup(uint8_t *buff, size_t len, BPG_protocol_data bpg_dat) //for protocol_data to uint8_t buffer for link layer transmission
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
uint8_t *BPG_Protocol_Interface::requestSendingBuffer(size_t len)
{
  cached_data_send.resize(getHeaderSize() + len);
  return &cached_data_send[getHeaderSize()];
}

//st1 ok
int BPG_Protocol_Interface::fromUpperLayer(BPG_protocol_data bpgdat)
{ //serialize the BPG_protocol_data as byte stream

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
    return toLinkLayer(&cached_data_send[0], cached_data_send.size(), true);
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
  if (bpgdat.size + 9 != len)
  {

    LOGI("<<<size:%d  len:%d<<<<<", bpgdat.size, len);

    BPG_protocol_data errDat = {0}; //TODO
    return errDat;
  }
  bpgdat.tl[0] = dat[0];
  bpgdat.tl[1] = dat[1];
  bpgdat.prop = dat[2];
  bpgdat.pgID = (dat[3] << 8) | (dat[4]);
  bpgdat.dat_raw = &(dat[9]);

  return bpgdat;
}

//st1 ok
int BPG_Protocol_Interface::fromLinkLayer(uint8_t *dat, size_t len, bool FIN)
{ //assemble to BPG_protocol_data

  if (FIN == true && cached_data_recv.size() == 0)
  {
    BPG_protocol_data bpgdat = convert(dat, len);
    if (bpgdat.tl[0] == 0)
    {
      return -1;
    }
    return toUpperLayer(bpgdat);
  }

  int headIdx = cached_data_recv.size();
  cached_data_recv.resize(cached_data_recv.size() + len);
  memcpy(&(cached_data_recv[headIdx]), dat, len);

  if (FIN == true)
  {
    BPG_protocol_data bpgdat = convert(&(cached_data_recv[0]), cached_data_recv.size());
    int ret = toUpperLayer(bpgdat);
    cached_data_recv.resize(0);
    return ret;
  }
  return 1;
}

//st1 ok
int BPG_Protocol_Interface::toLinkLayer(uint8_t *dat, size_t len, bool FIN)
{
  if (linkCH == NULL)
    return -1;

  LOGI("<<<<>>>>");
  return linkCH->fromUpperLayer(dat, len, FIN);
}