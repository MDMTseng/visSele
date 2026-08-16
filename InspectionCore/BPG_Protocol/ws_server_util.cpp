
#include "ws_server_util.h"

#include "logctrl.h"
#include "websocket.h"
#include <errno.h>
//////////////////////////////ws_server/////////////////////////////////////

LOG_MODULE("bpg.ws");

ws_server::ws_server(int port, ws_protocol_callback *cb) : ws_protocol_callback(this)
{
  this->cb = cb;

  listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket == -1)
  {
    printf("Error:create socket failed\n");
    return;
  }

  int enable = 1;
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int)) < 0)
  {
    //throw -3;
  }
#ifdef SO_REUSEPORT
  if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEPORT, (char *)&enable, sizeof(int)) < 0)
  {
    //throw -3;
  }
#endif

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port = htons(port);

  if (bind(listenSocket, (struct sockaddr *)&local, sizeof(local)) < 0)
  {
    printf("bind failed\n");
    close(listenSocket);
    listenSocket = -1;
    return;
  }

  if (listen(listenSocket, 8) < 0)
  {
    printf("listen failed\n");
    close(listenSocket);
    listenSocket = -1;
    return;
  }

  FD_ZERO(&evtSet);
  FD_SET(listenSocket, &evtSet);
  fdmax = listenSocket;

  printf("opened %s:%d  listenSocket:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port), listenSocket);
}

int ws_server::disconnect(int sock)
{
  ws_conn *conn = ws_conn_pool.find(sock);
  int ret = conn->doClosing();
  std::vector<ws_conn *> *servers = ws_conn_pool.getServers();

  for (int i = 0; i < (*servers).size(); i++)
  {
    printf("peer %s:%d  sock:%d\n",
           inet_ntoa((*servers)[i]->getAddr().sin_addr),
           ntohs((*servers)[i]->getAddr().sin_port), (*servers)[i]->getSocket());
  }
  return ret;
}

ws_server::~ws_server()
{
  //shutdown(listenSocket);
  std::vector<ws_conn *> *servers = ws_conn_pool.getServers();
  for (int i = 0; i < (*servers).size(); i++)
  {
    if ((*servers)[i]->isOccupied())
    {
      (*servers)[i]->doClosing();
    }
  }
  close(listenSocket);
}
int ws_server::get_socket()
{
  return listenSocket;
}

void ws_server::set_fd_set(fd_set *fdSet)
{
  FD_SET(listenSocket, fdSet);
  std::vector<ws_conn *> *servers = ws_conn_pool.getServers();
  for (int i = 0; i < (*servers).size(); i++)
  {
    if ((*servers)[i]->isOccupied())
    {
      FD_SET((*servers)[i]->getSocket(), fdSet);
    }
  }
}

fd_set ws_server::get_fd_set()
{
  fd_set newSet;
  FD_ZERO(&newSet);

  set_fd_set(&newSet);
  FD_SET(listenSocket, &newSet);
  evtSet = newSet;

  return evtSet;
}

int ws_server::findMaxFd()
{
  int max = listenSocket;

  std::vector<ws_conn *> *servers = ws_conn_pool.getServers();
  for (int i = 0; i < (*servers).size(); i++)
  {
    if ((*servers)[i]->isOccupied() && (*servers)[i]->getSocket() > max)
    {
      max = (*servers)[i]->getSocket();
    }
  }

  return max;
}
int ws_server::ws_callback(websock_data data, void *param)
{
  if (cb)
  {
    cb->ws_callback(data);
  }
  else
  {
    printf("%s: type:%d sock:%d\n", __func__, data.type, data.peer->getSocket());

    printf("peer %s:%d\n",
           inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
  }
  return 0;
}

int ws_server::runLoop(struct timeval *tv)
{

  if (listenSocket == -1)
  {
    return -1;
  }
  fd_set read_fds = evtSet;

  if (select(fdmax + 1, &read_fds, NULL, NULL, tv) == -1)
  {
    if (errno == EINTR)
      return 0; // interrupted by a signal; just retry on the next iteration
    perror("select");
    return -1; // transient error: report it, but never kill the process
  }
  return runLoop(&read_fds, tv);
}
int ws_server::runLoop(fd_set *read_fds, struct timeval *tv)
{

  LOGV(">>>>>");

  // 1) Accept every pending incoming connection (drain the backlog).
  if (FD_ISSET(listenSocket, read_fds))
  {
    FD_CLR(listenSocket, read_fds);
    LOGV("listenSocket");
    struct sockaddr_in remote;
    socklen_t sockaddrLen = sizeof(remote);
    int NewSock = accept(listenSocket, (struct sockaddr *)&remote, &sockaddrLen);
    if (NewSock == -1)
    {
      LOGV("accept failed");
    }
    else
    {
      // Send timeout. Every send in this server is a blocking send() on the
      // client's socket, issued from shared threads -- so ONE client that
      // stops reading (a background tab, a laptop lid, a paused debugger)
      // used to wedge the whole WS layer for EVERY client: measured RP
      // 27/s -> 0/s within 6 seconds of pausing a second client's socket,
      // GS unanswered, recovery only when that client resumed. The sorter
      // kept running (verified against a fake board); the UI layer froze.
      // With a timeout the stuck send errors out, safeSend shuts the
      // connection down, and everyone else stays live.
#ifdef _WIN32
      DWORD _sndTo = 5000;
      setsockopt(NewSock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&_sndTo, sizeof(_sndTo));
#else
      struct timeval _sndTo = { 5, 0 };
      setsockopt(NewSock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&_sndTo, sizeof(_sndTo));
#endif
      ws_conn *conn = ws_conn_pool.find_avaliable_conn_info_slot();
      conn->setSocket(NewSock);
      conn->setAddr(remote);
      LOGV("connected %s:%d sock:%d",
           inet_ntoa(conn->getAddr().sin_addr), ntohs(conn->getAddr().sin_port), conn->getSocket());
      conn->setCallBack(this);
      conn->triggerEV_OPENING();

      FD_SET(NewSock, &evtSet);
      if (NewSock > fdmax)
        fdmax = NewSock;

      printf("List size %d\n", ws_conn_pool.size());
    }
  }

  // 2) Service ALL ready client connections this round (fair servicing).
  std::vector<ws_conn *> *servers = ws_conn_pool.getServers();
  bool any_closed = false;
  // Retry any close that doClosing() had to defer because a sender was
  // mid-send. The shutdown() already fired, so the sender's send() has
  // returned by now and the try_lock succeeds; this is a no-op otherwise.
  for (size_t i = 0; i < (*servers).size(); i++)
  {
    if ((*servers)[i]->closePending())
      (*servers)[i]->tryFinalizeClose();
  }
  for (size_t i = 0; i < (*servers).size(); i++)
  {
    int fd = (*servers)[i]->getSocket();
    if ((*servers)[i]->isOccupied() && FD_ISSET(fd, read_fds))
    {
      FD_CLR(fd, read_fds);
      (*servers)[i]->runLoop();
      if (!(*servers)[i]->isOccupied())
      {
        printf("List size %d\n", ws_conn_pool.size());
        FD_CLR(fd, &evtSet);
        any_closed = true;
      }
    }
  }
  if (any_closed)
    fdmax = findMaxFd();

  return 0;
}

int ws_server::send_pkt(websock_data *packet)
{
  if (packet == NULL || packet->peer == NULL)
    return -1;
  ws_conn *client = ws_conn_pool.find(packet->peer->getSocket());
  if (client != packet->peer)
    return -20;
  return client->send_pkt(packet);
}
//////////////////////////////ws_conn_entity_pool/////////////////////////////////////

ws_conn *ws_conn_entity_pool::find_nolock(int sock)
{
  for (int i = 0; i < ws_conn_set.size(); i++)
  {
    if (ws_conn_set[i]->getSocket() == sock)
      return (ws_conn_set[i]);
  }
  return NULL;
}

ws_conn *ws_conn_entity_pool::find(int sock)
{
  // Runs on sender threads while the main thread's
  // find_avaliable_conn_info_slot() may push_back (and reallocate) under it.
  std::lock_guard<std::mutex> _pool_guard(poolLock);
  return find_nolock(sock);
}

std::vector<ws_conn *> *ws_conn_entity_pool::getServers()
{
  // Raw access; main-thread-only iterations (select loop, fd sets,
  // destructor). Cross-thread lookups must go through find().
  return &ws_conn_set;
}

int ws_conn_entity_pool::remove(int sock)
{
  ws_conn *torm = find(sock);
  if (torm == NULL)
    return -1;

  torm->doClosing();
  return 0;
}

ws_conn *ws_conn_entity_pool::find_avaliable_conn_info_slot()
{
  std::lock_guard<std::mutex> _pool_guard(poolLock);
  for (int i = 0; i < ws_conn_set.size(); i++)
  {
    // A slot whose close is still deferred (a sender was mid-send when the
    // connection died) still owns its sendBuf and its not-yet-closed fd --
    // handing it to a new connection would stack two lifetimes on one slot.
    if (!ws_conn_set[i]->isOccupied() && !ws_conn_set[i]->closePending())
      return (ws_conn_set[i]);
  }
  ws_conn_set.push_back(new ws_conn());

  return (ws_conn_set[ws_conn_set.size() - 1]);
}

ws_conn *ws_conn_entity_pool::add(ws_conn *info)
{
  if (info == NULL)
    return NULL;
  if (!info->isOccupied())
    return NULL;

  if (find(info->getSocket()) != NULL)
  {
    return NULL;
  }
  ws_conn *tmp = find_avaliable_conn_info_slot();
  tmp->COPY_property(info);
  return tmp;
}

int ws_conn_entity_pool::size()
{
  int len = 0;
  for (int i = 0; i < ws_conn_set.size(); i++)
  {
    if (ws_conn_set[i]->isOccupied())
    {
      len++;
    }
  }
  return len;
}

//////////////////////////////ws_conn/////////////////////////////////////
//#define PACKET_DUMP
int ws_conn::safeSend(int sock, const uint8_t *buffer, size_t bufferSize)
{
#ifdef PACKET_DUMP
  printf("=================out packet:\n");
  fwrite(buffer, 1, bufferSize, stdout);
  printf("\n");
#endif
  if (sock < 0)
    return -1;
  ssize_t written = send(sock, (const char *)buffer, bufferSize, 0);
  if (written == -1 || written != bufferSize)
  {
    // Timeout (SO_SNDTIMEO, set at accept) or a genuine error. A partial
    // write is fatal too: the peer now has half a WS frame and can never
    // resynchronise. Either way this connection is done -- but do NOT
    // close() here: sends run on several threads while the main loop owns
    // the fd, and closing from the wrong thread frees a slot the select
    // loop is still watching. shutdown() is thread-safe, makes the fd
    // readable-with-EOF, and the main loop then does the real teardown on
    // its own thread.
    perror(written == -1 ? "safeSend:send failed" : "safeSend:partial write");
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
#else
    shutdown(sock, SHUT_RDWR);
#endif
    return -1;
  }

  return 0;
}

ws_conn::ws_conn()
{
  RESET();
}

void ws_conn::setCallBack(ws_protocol_callback *cb)
{
  this->cb = cb;
}
void ws_conn::triggerEV_OPENING()
{
  if (cb != NULL)
  {
    cb->ws_callback(genCallbackData(websock_data::eventType::OPENING));
  }
}
void ws_conn::RESET()
{
  cb = NULL;
  sock = -1;
  ws_state = WS_STATE_OPENING;
  memset(&addr, 0, sizeof(addr));
  accBufDataLen = 0;
  // INVARIANT: recvBuf is always allocated with ONE spare byte past the
  // usable region. Only recvBuf.size()-1 bytes are ever received into; the
  // last byte exists purely so doHandShake()/the link layer can drop a '\0'
  // terminator at buff[buffLen] without writing past the allocation.
  if (recvBuf.size() < (size_t)recvBufSizeInc + 1)
    recvBuf.resize((size_t)recvBufSizeInc + 1);

  sendBuf.resize(recvBufSizeInc);
}

int ws_conn::setSocket(int socket)
{
  sock = socket;
  return 0;
}

int ws_conn::setAddr(struct sockaddr_in address)
{
  addr = address;
  return 0;
}

void ws_conn::COPY_property(ws_conn *from)
{
  sock = from->sock;
  ws_state = from->ws_state;
  addr = from->addr;
  accBufDataLen = from->accBufDataLen;
}

int ws_conn::strcpy_m(char *dst, int dstMaxSize, char *src)
{
  if (dstMaxSize < 0 || src == NULL)
    return -1;
  dstMaxSize--;
  dst[dstMaxSize] = '\0';
  int i;
  for (i = 0; i < dstMaxSize && src[i]; i++)
  {
    dst[i] = src[i];
  }
  dst[i] = src[i];
  return i;
}

int ws_conn::doHandShake(void *buff, ssize_t buffLen, struct handshake *p_hs)
{
  if (buff == NULL || buffLen < 0)
    return -1;
  // Safe: the caller passes recvBuf, which by invariant (see RESET()) has one
  // byte allocated past the region recv() is allowed to fill, so buffLen can
  // be at most recvBuf.size()-1 and this write stays inside the allocation.
  ((char *)buff)[buffLen] = '\0';
  struct handshake &hs = *p_hs;
  nullHandshake(&hs);

  enum wsFrameType frameType = wsParseHandshake((unsigned char *)buff, buffLen, &hs);

  if (frameType != WS_OPENING_FRAME)
  {
    return -1;
  }
  strcpy_m(resource, sizeof(resource), hs.resource);
  //printf("%s:%s\n", __func__, resource);

  // if resource is right, generate answer handshake and send it
  {
    // No worker can send to a conn that has not finished its handshake, but
    // take the lock anyway: it is uncontended here and keeps the invariant
    // "sendBuf is only ever touched under sendMutex" unconditional.
    std::lock_guard<std::mutex> _send_guard(sendMutex);
    size_t frameSize = sendBuf.size();

    wsGetHandshakeAnswer(&hs, &sendBuf[0], &frameSize);
    //freeHandshake(&hs);
    if (safeSend(sock, &sendBuf[0], frameSize) != 0)
    {
      doClosing();
      return -1;
    }
  }
  return 0;
}

int ws_conn::doClosing()
{
  if (isOccupied())
  {
    // shutdown() first, close() later (tryFinalizeClose). A sender may be
    // sitting inside send() on this fd right now; shutdown makes that send
    // fail fast and is safe from any thread, while close() would recycle
    // the fd number for the next accept() while the sender still uses it.
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
#else
    shutdown(sock, SHUT_RDWR);
#endif
    pendingCloseFd = sock;
  }

  printf("%s:cb:%p sock:%d\n", __func__, cb, sock);
  sock = -1;   // no NEW sender passes the in-lock check from here on
  if (cb != NULL)
  {
    cb->ws_callback(genCallbackData(websock_data::eventType::CLOSING));
  }
  tryFinalizeClose();   // usually immediate; retried from ws_server::runLoop
  printf("%s\n", __func__);
  return 0;
}

bool ws_conn::tryFinalizeClose()
{
  if (pendingCloseFd == -1)
    return true;
  // try_lock, never lock: a sender blocked in send() can hold sendMutex for
  // up to the 5s SO_SNDTIMEO, and this runs on the select loop's thread.
  // The shutdown() in doClosing() already made that send return quickly, so
  // the retry from runLoop lands almost immediately.
  if (!sendMutex.try_lock())
    return false;
  close(pendingCloseFd);
  pendingCloseFd = -1;
  RESET();   // sendBuf resize -- must not race a sender, hence under the lock
  sendMutex.unlock();
  return true;
}

websock_data ws_conn::genCallbackData(websock_data::eventType type)
{
  websock_data data;
  data.peer = this;
  data.type = type;
  return data;
}
int ws_conn::event_WsRECV(uint8_t *data, size_t dataSize, enum wsFrameType frameType, bool isFinal)
{
  //BY default, echo
  //size_t frameSize = sendBuf.size();
  //int ret = wsMakeFrame2(data, dataSize, &(sendBuf[0]), &frameSize, frameType, isFinal);

  if (cb != NULL)
  {
    websock_data cb_data = genCallbackData(websock_data::eventType::DATA_FRAME);
    cb_data.data.data_frame.type = frameType;
    cb_data.data.data_frame.raw = data;
    cb_data.data.data_frame.rawL = dataSize;
    cb_data.data.data_frame.isFinal = isFinal;
    cb->ws_callback(cb_data);
  }

  return 0;
}

int ws_conn::event_TCP_RECV(uint8_t *data, size_t dataSize)
{
  //BY default, echo
  //size_t frameSize = sendBuf.size();
  //int ret = wsMakeFrame2(data, dataSize, &(sendBuf[0]), &frameSize, frameType, isFinal);

  if (cb != NULL)
  {
    websock_data cb_data = genCallbackData(websock_data::eventType::DATA_FRAME_TCP);
    cb_data.data.data_frame.type = TCP_BINARY_FRAME;
    cb_data.data.data_frame.raw = data;
    cb_data.data.data_frame.rawL = dataSize;
    cb_data.data.data_frame.isFinal = true;
    cb->ws_callback(cb_data);
  }

  return 0;
}

int ws_conn::doNormalRecv(void *buff, size_t buffLen, size_t *ret_restLen, enum wsFrameType *ret_lastFrameType)
{
  int h_padding = 0;
  enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
  while (buffLen > h_padding)
  {
    size_t curPktLen;
    bool isFinal;

    uint8_t *data = NULL;
    size_t dataSize = 0;

    uint8_t *tmpD = (uint8_t *)buff + h_padding;
    frameType = wsParseInputFrame2(tmpD, buffLen - h_padding,
                                   &data, &dataSize, &curPktLen, &isFinal);
    //printf("frameType:%d    %02x %02x %02x\n",frameType,tmpD[0],tmpD[1],tmpD[2]);
    *ret_lastFrameType = frameType;

    if (frameType == WS_TEXT_FRAME || frameType == WS_BINARY_FRAME)
    {
      h_padding += curPktLen;
      /*for(int i=0;i<dataSize;i++)
            {
              printf("%02x ",data[i]);
            }*/

      // printf("dataSize:%d isFinal:%d\n", dataSize, isFinal);
      event_WsRECV(data, dataSize, frameType, isFinal);
    }
    else if (frameType == WS_CONT_FRAME)
    {
      h_padding += curPktLen;
      /*for(int i=0;i<dataSize;i++)
            {
              printf("%02x ",data[i]);
            }*/

      event_WsRECV(data, dataSize, frameType, isFinal);
    }
    else if (frameType == WS_INCOMPLETE_FRAME)
    {
      //The packet is not finished, wait for receiving more
      break;
    }
    else if (frameType == WS_CLOSING_FRAME)
    {
      h_padding += curPktLen;
      ws_state = WS_STATE_CLOSING;
      *ret_restLen = 0;
      return doClosing();
    }
    else if (frameType == WS_PING_FRAME)
    {
      h_padding += curPktLen;
      // RFC6455 says answer with a PONG carrying the same payload. This is not
      // an exotic path: python-websockets sends a keepalive ping every 20s by
      // default, and until now nothing here consumed it -- h_padding never
      // advanced, so the while condition never changed and the core spun at
      // 100% with the select loop never coming back.
      send_pkt(data, dataSize, WS_PONG_FRAME, true, 0);
    }
    else if (frameType == WS_PONG_FRAME)
    {
      h_padding += curPktLen;   // unsolicited pong: consume it, nothing to do
    }
    else if (frameType == WS_ERROR_FRAME)
    {
      break;
    }
    else
    {
      // Anything we do not know how to consume would leave h_padding where it
      // was and spin forever. Bail out instead of hanging the whole daemon.
      break;
    }
  }

  //The packet is incomplete/or error, remove finished packets
  //|finished|finished|incomplete| => |incomplete|
  //_______h_padding__^
  if (frameType == WS_INCOMPLETE_FRAME || frameType == WS_ERROR_FRAME)
  {
    ssize_t newLen = buffLen - h_padding;

    if (h_padding != 0)
    {
      memcpy(buff, (uint8_t *)buff + h_padding, newLen);
    }
    *ret_restLen = newLen;
  }
  else
  {
    *ret_restLen = 0;
  }
  return 0;
}

int ws_conn::runLoop()
{
  if (!isOccupied())
  {
    return -1;
  }
  //printf("sock:%d size:%d\n",sock,recvBuf.size());

  // See RESET(): recvBuf always holds one spare byte past the usable region,
  // reserved for a '\0' terminator. Never receive into it.
  if (recvBuf.size() < 1)
    recvBuf.resize((size_t)recvBufSizeInc + 1);
  size_t recvCap = recvBuf.size() - 1;
  if (recvCap == accBufDataLen)
  {
    printf("Buffer size(%d) is not enough, expend to %d\n", (int)recvCap, (int)(recvCap + recvBufSizeInc));
    recvBuf.resize(recvBuf.size() + recvBufSizeInc);
    recvCap = recvBuf.size() - 1;
  }
  ssize_t readed = recv(sock, (char *)(&(recvBuf[0]) + accBufDataLen), recvCap - accBufDataLen, 0);
  if (readed <= 0)
  {
    ws_state = WS_STATE_CLOSING;
    doClosing();
    return -1;
  }
  accBufDataLen += readed;


  /*if(accBufDataLen==recvBuf.size())
    {
      recvBuf.reserve(recvBuf.size()+recvBufSizeInc);
    }*/

  if (ws_state == WS_STATE_NORMAL)
  {
    if (doNormalRecv(&(recvBuf[0]), accBufDataLen, &accBufDataLen, &lastPktType) == 0)
    {
    }

    if (lastPktType == WS_ERROR_FRAME && accBufDataLen == recvCap)
    {
      printf(">>>>>ERROR QUIT\n");
    }
    return 0;
  }

  if (ws_state == WS_STATE_TCP)
  {
    event_TCP_RECV(&(recvBuf[0]), readed);
    return 0;
  }

  accBufDataLen = 0; //accBufDataLen is for receving accumulation, only useful in normal mode
  if (ws_state == WS_STATE_OPENING)
  {

    if (cb != NULL)
    {
      cb->ws_callback(genCallbackData(websock_data::eventType::MSG_1st));
    }
    struct handshake hs;
    if (doHandShake(&(recvBuf[0]), readed, &hs) != 0)
    {
      // printf("Error:Hand shake failed...");
      // ws_state = WS_STATE_CLOSING;
      // doClosing();

      websock_data ws_dat = genCallbackData(websock_data::eventType::TCP_CONNECTION_FINISHED);
      cb->ws_callback(ws_dat);

      ws_state = WS_STATE_TCP;
      event_TCP_RECV(&(recvBuf[0]), readed);
    }
    else
    {
      websock_data ws_dat = genCallbackData(websock_data::eventType::HAND_SHAKING_FINISHED);
      ws_dat.data.hs_frame.host = hs.host;
      ws_dat.data.hs_frame.origin = hs.origin;
      ws_dat.data.hs_frame.key = hs.key;
      ws_dat.data.hs_frame.resource = hs.resource;
      cb->ws_callback(ws_dat);
      ws_state = WS_STATE_NORMAL;
    }
    freeHandshake(&hs);
    return 0;
  }

  if (ws_state == WS_STATE_CLOSING)
  {
    doClosing();
    return -1;
  }

  return -1;
}

int ws_conn::send_pkt(websock_data *packet)
{
  if (packet == NULL || packet->peer == NULL)
    return -1;

  if (this != packet->peer)
    return -20;
  if (packet->type == websock_data::CLOSING)
  {
    doClosing();
    return 0;
  }

  enum wsFrameType frameType = (enum wsFrameType)packet->data.data_frame.type;

  if (frameType == WS_CLOSING_FRAME)
  {

    doClosing();
    return 0;
  }

  if (frameType == TCP_BINARY_FRAME)
  {
    std::lock_guard<std::mutex> _send_guard(sendMutex);
    const int s = sock;
    if (s < 0)
      return -1;
    return safeSend(s, packet->data.data_frame.raw, packet->data.data_frame.rawL);
  }

  if (frameType != WS_TEXT_FRAME && frameType != WS_BINARY_FRAME && frameType != WS_PING_FRAME && frameType != WS_PONG_FRAME && frameType != WS_CONT_FRAME)
    return -3;

  return send_pkt(packet->data.data_frame.raw, packet->data.data_frame.rawL, frameType, packet->data.data_frame.isFinal,packet->data.data_frame.extraHeaderRoom);
}
#define WS_MAX_HEADERSIZE 10
uint8_t* ws_conn::request_data_buffer(size_t req_size)
{
  size_t frameSize = sendBuf.size();
  if (frameSize < WS_MAX_HEADERSIZE+req_size)
  {
    sendBuf.resize(WS_MAX_HEADERSIZE+req_size);
    frameSize = sendBuf.size();
  }
  return &(sendBuf[WS_MAX_HEADERSIZE]);
}

int ws_conn::send_pkt(uint8_t *packet, size_t pkt_size, int type, bool isFinal,int extraHeaderRoom)
{
  // Worker sends are serialized among themselves by BPG's linkLayerLock, but
  // the main WS thread's PONG replies come through here too, and teardown
  // (close + sendBuf RESET) is gated on this same mutex -- see the header.
  std::lock_guard<std::mutex> _send_guard(sendMutex);
  const int s = sock;
  if (s < 0)
    return -1;   // doClosing already ran; the fd may be gone or recycled

  uint8_t* frameBuffer=NULL;

  size_t frameSize = -1;

  // printf(">pkt_size:%d MHSize:%d> extraHeaderRoom:%d\n",pkt_size,WS_MAX_HEADERSIZE,extraHeaderRoom);
  if(extraHeaderRoom>=WS_MAX_HEADERSIZE)
  {
  // printf(">\n");
    frameBuffer=packet-WS_MAX_HEADERSIZE;
    frameSize = pkt_size+WS_MAX_HEADERSIZE;
  }
  else
  {
    frameSize = sendBuf.size();

    if (frameSize < WS_MAX_HEADERSIZE+pkt_size)
    {
      sendBuf.resize(WS_MAX_HEADERSIZE+pkt_size);
      frameSize = sendBuf.size();
    }
    frameBuffer=&(sendBuf[0]);

  }
    // printf("sendBuf.size:%d\n", sendBuf.size());


  uint8_t * sendData=wsMakeFrame_HeaderBack(packet, pkt_size,
                         frameBuffer, &frameSize, (enum wsFrameType)type, isFinal);
  if(sendData==NULL)
  {
    printf("wsMakeFrame2 error:\n");
    return -1;
  }

  return safeSend(s, sendData, frameSize);

  
  // int ret = wsMakeFrame2(packet, pkt_size,
  //                        &(sendBuf[0]), &frameSize, (enum wsFrameType)type, isFinal);
  // if (ret)
  // {
  //   printf("wsMakeFrame2 error:%d\n", ret);
  //   return -1;
  // }
  // return safeSend(sock, &sendBuf[0], frameSize);
}
