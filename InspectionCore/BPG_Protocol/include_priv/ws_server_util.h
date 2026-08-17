

#include <vector>
#include <mutex>
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

  // Serializes every writer of (sendBuf, the send() syscall) on this
  // connection: worker sends arrive already serialized among themselves by
  // BPG's linkLayerLock, but the main WS thread also sends (PONG replies,
  // handshake answers) and tears down (doClosing) with no lock at all --
  // which is backlog 2.1: sendBuf resized under a sender's pointer, and an
  // fd closed (and reused by accept) while a sender is inside send().
  //
  // Teardown never BLOCKS on this mutex (a sender can sit in send() for up
  // to the 5s SO_SNDTIMEO; the select loop must not wait behind that):
  // doClosing() shuts the socket down -- which makes that in-flight send
  // fail fast -- and defers the close()+RESET() to tryFinalizeClose(),
  // which only proceeds when try_lock proves no sender is mid-send. The fd
  // therefore cannot be reused by a new connection while any sender still
  // holds it, because close() itself is what recycles the number.
  std::mutex sendMutex;
  int pendingCloseFd = -1;

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
  void triggerEV_OPENING();
  int doClosing();

  int event_WsRECV(uint8_t *data, size_t dataSize,
                   enum wsFrameType frameType, bool isFinal);

  int event_TCP_RECV(uint8_t *data, size_t dataSize);

  int doNormalRecv(void *buff, size_t buffLen,
                   size_t *ret_restLen, enum wsFrameType *ret_lastFrameType);

  enum wsFrameType lastPktType;
  int runLoop();
  uint8_t* request_data_buffer(size_t req_size);
  int send_pkt(websock_data *packet);
  int send_pkt(uint8_t *packet, size_t pkt_size, int type, bool isFinal,int extraHeaderRoom);
  // Complete a deferred teardown (close + RESET) if no sender is mid-send.
  // Returns true when nothing is pending anymore. Called from doClosing()
  // and retried from the server's runLoop each pass.
  bool tryFinalizeClose();
  bool closePending() const { return pendingCloseFd != -1; }
};

class ws_conn_entity_pool
{

  std::vector<ws_conn *> ws_conn_set;
  // find() runs on SENDER threads (ws_server::send_pkt) while the main WS
  // thread's find_avaliable_conn_info_slot() may push_back and REALLOCATE
  // the vector under it (backlog 2.1). Guard every cross-thread access.
  // getServers() stays raw: its callers are main-thread-only iterations,
  // and push_back only ever happens on that same thread.
  std::mutex poolLock;
  ws_conn *find_nolock(int sock);

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
