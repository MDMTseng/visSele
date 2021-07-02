

#include <vector>
#include "websocket.h"

#include <unistd.h>

#include "websocket_conn.hpp"

class ws_conn_entity_pool;

class ws_server;

class ws_conn : public ws_conn_data
{

  std::vector<uint8_t> recvBuf;
  std::vector<uint8_t> sendBuf;
  ws_protocol_callback *cb;
  static int safeSend(int sock, const uint8_t *buffer, size_t bufferSize);

public:
  ws_conn();

  void RESET();

  websock_data genCallbackData(websock_data::eventType type);

  int setSocket(int socket);

  int setAddr(struct sockaddr_in address);

  void setCallBack(ws_protocol_callback *cb);

  void COPY_property(ws_conn *from);

  static int strcpy_m(char *dst, int dstMaxSize, char *src);
  int doHandShake(void *buff, ssize_t buffLen, struct handshake *p_hs);

  int doClosing();

  int event_WsRECV(uint8_t *data, size_t dataSize,
                   enum wsFrameType frameType, bool isFinal);

  int event_TCP_RECV(uint8_t *data, size_t dataSize);

  int doNormalRecv(void *buff, size_t buffLen,
                   size_t *ret_restLen, enum wsFrameType *ret_lastFrameType);

  enum wsFrameType lastPktType;
  int runLoop();

  int send_pkt(websock_data *packet);
  int send_pkt(const uint8_t *packet, size_t pkt_size, int type, bool isFinal);
};

class ws_conn_entity_pool
{

  std::vector<ws_conn *> ws_conn_set;

public:
  ws_conn *find(int sock);

  std::vector<ws_conn *> *getServers();

  int remove(int sock);

  ws_conn *find_avaliable_conn_info_slot();

  ws_conn *add(ws_conn *info);

  int size();
};

class ws_server : public ws_protocol_callback
{

  int listenSocket;
  fd_set evtSet;
  int fdmax;
  ws_protocol_callback *cb;
  ws_conn_entity_pool ws_conn_pool;

public:
  ws_server(int port, ws_protocol_callback *cb);

  fd_set get_fd_set();
  void set_fd_set(fd_set *fdSet);
  int findMaxFd();
  int get_socket();
  int runLoop(struct timeval *tv);
  int runLoop(fd_set *read_fds, struct timeval *tv);
  int ws_callback(websock_data data, void *param);
  int send_pkt(websock_data *packet);
  int send_pkt(void *packet, size_t pkt_size);
  int disconnect(int sock);
  ~ws_server();
};
