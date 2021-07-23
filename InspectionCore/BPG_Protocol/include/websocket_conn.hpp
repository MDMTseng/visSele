#ifndef WEBSOCKET_CONNECTION_HPP
#define WEBSOCKET_CONNECTION_HPP

#ifdef __WIN32__
#include <winsock2.h>
#define socklen_t int
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

class ws_server;
class ws_conn_data;

enum wsDataFrameType
{ // errors starting from 0xF0
  WS_DFT_EMPTY_FRAME = 0xF0,
  WS_DFT_ERROR_FRAME = 0xF1,
  WS_DFT_INCOMPLETE_FRAME = 0xF2,
  WS_DFT_CONT_FRAME = 0x00,
  WS_DFT_TEXT_FRAME = 0x01,
  WS_DFT_BINARY_FRAME = 0x02,
  WS_DFT_PING_FRAME = 0x09,
  WS_DFT_PONG_FRAME = 0x0A,
  WS_DFT_OPENING_FRAME = 0xF3,
  WS_DFT_CLOSING_FRAME = 0x08
};
typedef struct websock_data
{
  enum eventType
  {
    OPENING,
    MSG_1st,
    HAND_SHAKING,
    HAND_SHAKING_FINISHED,
    TCP_CONNECTION_FINISHED,
    DATA_FRAME,
    DATA_FRAME_TCP,
    CLOSING,
    ERROR_EV,
  } type;

  ws_conn_data *peer;
  union content
  {
    struct _DATA_FRAME
    {
      int type;
      uint8_t *raw;
      size_t rawL;
      bool isFinal;
      int extraHeaderRoom;
    } data_frame;

    struct _HS_FRAME
    {
      char *host;
      char *origin;
      char *key;
      char *resource;
    } hs_frame;

    typedef struct _ERROR
    {
      int code;
    } error;
  } data;
};

class ws_protocol_callback
{
public:
  void *param;
  ws_protocol_callback(void *param)
  {
    this->param = param;
  }
  virtual int ws_callback(websock_data data, void *param)
  {
    return 0;
  }
  virtual int ws_callback(websock_data data)
  {
    return ws_callback(data, param);
  }
};

class ws_conn_data
{

protected:
  const int recvBufSizeInc = 1024*10;
  int ws_state;
  size_t accBufDataLen;
  int sock;
  struct sockaddr_in addr;
  char resource[128];
  ws_protocol_callback *cb;

public:
  int getSocket()
  {
    return sock;
  }

  struct sockaddr_in getAddr() const
  {
    return addr;
  }

  bool isOccupied() const
  {
    return sock != -1;
  }
  const char *getResource() const
  {
    return resource;
  }
};

#endif
