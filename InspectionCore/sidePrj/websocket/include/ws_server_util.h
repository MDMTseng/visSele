

#include <vector>
#include "websocket.h"

#ifdef __WIN32__
#include <winsock2.h>
#define socklen_t int
#else
#include <netinet/in.h>
#endif



class ws_conn;

class ws_protocol_callback{
};


class ws_conn{

  const int recvBufSizeInc=1024;
  int ws_state;
  std::vector <uint8_t> recvBuf;
  std::vector <uint8_t> sendBuf;
  size_t accBufDataLen;

  static int safeSend(int sock, const uint8_t *buffer, size_t bufferSize)
  {
      #ifdef PACKET_DUMP
      printf("out packet:\n");
      fwrite(buffer, 1, bufferSize, stdout);
      printf("\n");
      #endif
      if(sock<0)return -1;
      ssize_t written = send(sock, (const char*)buffer, bufferSize, 0);
      if (written == -1) {
          perror("send failed");
          return -1;
      }
      if (written != bufferSize) {
          perror("written not all bytes");
          return -1;
      }

      return 0;
  }
  int sock;
  struct sockaddr_in addr;
  char resource[128];
  public:


  ws_conn(){
    RESET();
  }

  void RESET()
  {
    cb=NULL;
    sock=-1;
    ws_state = WS_STATE_OPENING;
    memset(&addr,0,sizeof(addr));
    accBufDataLen = 0;
    if(recvBuf.size()<recvBufSizeInc)
      recvBuf.resize(recvBufSizeInc);

    sendBuf.resize(recvBufSizeInc);
  }


  int setSocket(int socket)
  {
    sock=socket;
  }

  int getSocket()
  {
    return sock;
  }


  int setAddr(struct sockaddr_in address)
  {
    addr=address;
  }
  struct sockaddr_in getAddr()
  {
    return addr;
  }

  bool isOccupied()
  {
    return sock!=-1;
  }

  void COPY_property(ws_conn *from)
  {
    sock = from->sock;
    ws_state = from->ws_state;
    addr = from->addr;
    accBufDataLen = from->accBufDataLen;
  }

  static int strcpy_m(char *dst, int dstMaxSize, char *src)
  {
    if( dstMaxSize<0 || src == NULL)return -1;
    dstMaxSize--;
    dst[dstMaxSize]='\0';
    int i;
    for(i=0 ; i<dstMaxSize && src[i]; i++)
    {
      dst[i]=src[i];
    }
    dst[i]=src[i];
    return i;
  }

  int doHandShake(void *buff, ssize_t buffLen)
  {
    ((char*)buff)[buffLen]='\0';
    struct handshake hs;
    nullHandshake(&hs);

    enum wsFrameType frameType = wsParseHandshake((unsigned char *)buff, buffLen, &hs);

    if (frameType != WS_OPENING_FRAME) {
      return -1;
    }
    strcpy_m(resource, sizeof(resource), hs.resource);
    printf("%s:%s\n",__func__,resource);

    // if resource is right, generate answer handshake and send it
    size_t frameSize=sendBuf.size();

    wsGetHandshakeAnswer(&hs, &sendBuf[0], &frameSize);
    freeHandshake(&hs);
    if (safeSend(sock, &sendBuf[0], frameSize) == EXIT_FAILURE)
    {
      doClosing();
      return -1;
    }

    return 0;
  }

  int doClosing()
  {
    if(isOccupied())
      close(sock);
    RESET();
    printf("%s\n",__func__);
    return 0;
  }

  int event_WsRECV(uint8_t *data, size_t dataSize, enum wsFrameType frameType, bool isFinal)
  {
    //BY default, echo
    size_t frameSize=sendBuf.size();
    int ret = wsMakeFrame2(data, dataSize, &(sendBuf[0]), &frameSize, frameType,isFinal);
    if(ret)
    {
      printf("wsMakeFrame2 error:%d\n",ret);
      //return -1;
    }
    else if (safeSend(sock, &sendBuf[0], frameSize) == EXIT_FAILURE)
    {
      printf("safeSend error\n");
      doClosing();
      //return -1;
    }

    return 0;
  }

  int doNormalRecv(void *buff, size_t buffLen, size_t *ret_restLen, enum wsFrameType *ret_lastFrameType)
  {
      int h_padding = 0;
      enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
      while( buffLen >  h_padding )
      {
        size_t curPktLen;
        bool isFinal;

        uint8_t *data = NULL;
        size_t dataSize = 0;

        uint8_t* tmpD=(uint8_t*)buff+h_padding;
        frameType = wsParseInputFrame2(tmpD, buffLen-h_padding,
          &data, &dataSize, &curPktLen, &isFinal);
        //printf("frameType:%d    %02x %02x %02x\n",frameType,tmpD[0],tmpD[1],tmpD[2]);
        *ret_lastFrameType = frameType;

        if(frameType == WS_TEXT_FRAME || frameType == WS_BINARY_FRAME )
        {
          h_padding+=curPktLen;
          /*for(int i=0;i<dataSize;i++)
          {
            printf("%02x ",data[i]);
          }*/

          // printf("dataSize:%d isFinal:%d\n",dataSize,isFinal);
          event_WsRECV( data, dataSize, frameType, isFinal);

        }
        else if(frameType == WS_CONT_FRAME )
        {
          h_padding+=curPktLen;
          /*for(int i=0;i<dataSize;i++)
          {
            printf("%02x ",data[i]);
          }*/
          printf("CONT dataSize:%d\n",dataSize);

          event_WsRECV( data, dataSize, frameType, isFinal);
        }
        else if( frameType == WS_INCOMPLETE_FRAME )
        {
          //The packet is not finished, wait for receiving more
          break;
        }
        else if( frameType == WS_CLOSING_FRAME )
        {
          h_padding+=curPktLen;
          ws_state=WS_STATE_CLOSING;
          *ret_restLen=0;
          return doClosing();
        }
        else if( frameType == WS_ERROR_FRAME )
        {
          break;
        }
      }

      //The packet is incomplete/or error, remove finished packets
      //|finished|finished|incomplete| => |incomplete|
      //_______h_padding__^
      if( frameType == WS_INCOMPLETE_FRAME || frameType == WS_ERROR_FRAME )
      {
        ssize_t newLen=buffLen - h_padding;

        if(h_padding != 0)
        {
          memcpy(buff,(uint8_t*)buff+h_padding,newLen);
        }
        *ret_restLen = newLen;
      }
      else
      {
        *ret_restLen =0;
      }
      return 0;
  }

  enum wsFrameType lastPktType;
  int runLoop()
  {
    if( !isOccupied() )
    {
      return -1;
    }
    //printf("sock:%d size:%d\n",sock,recvBuf.size());

    if(recvBuf.size() == accBufDataLen)
    {
      printf("Buffer size(%d) is not enough, expend to %d\n",recvBuf.size(),recvBuf.size()+recvBufSizeInc);
      recvBuf.resize(recvBuf.size()+recvBufSizeInc);
    }
    ssize_t readed = recv(sock,(char*) (&(recvBuf[0])+accBufDataLen), recvBuf.size()-accBufDataLen, 0);
    if (!readed) {
      ws_state=WS_STATE_CLOSING;
      doClosing();
      return -1;
    }
    accBufDataLen+=readed;

    //printf("readed:%d\n",readed);

    /*if(accBufDataLen==recvBuf.size())
    {
      recvBuf.reserve(recvBuf.size()+recvBufSizeInc);
    }*/

    if(ws_state == WS_STATE_NORMAL)
    {
      if(doNormalRecv(&(recvBuf[0]), accBufDataLen, &accBufDataLen, &lastPktType) ==0 )
      {
      }

      if(lastPktType == WS_ERROR_FRAME && accBufDataLen == recvBuf.size())
      {
        printf(">>>>>ERROR QUIT\n");
      }
      return 0;
    }

    accBufDataLen = 0;//accBufDataLen is for receving accumulation, only useful in normal mode
    if(ws_state == WS_STATE_OPENING)
    {
      if(doHandShake(&(recvBuf[0]), readed) !=0 )
      {
        printf("Error:Hand shake failed...");
        ws_state=WS_STATE_CLOSING;
        doClosing();
      }
      else
      {
        ws_state = WS_STATE_NORMAL;
      }
      return 0;
    }

    if(ws_state == WS_STATE_CLOSING)
    {
        doClosing();
    }

  }


};


class ws_conn_entity_pool{

    std::vector <ws_conn> ws_conn_set;

    public:
    ws_conn *find(int sock)
    {
      	for(int i=0;i<ws_conn_set.size();i++)
      	{
      		if(ws_conn_set[i].getSocket() == sock)
      			return &(ws_conn_set[i]);
      	}
      	return NULL;
    }

    std::vector <ws_conn>* getServers()
    {
      return &ws_conn_set;
    }

    int remove(int sock)
    {
        ws_conn * torm = find(sock);
        if(torm == NULL)
          return -1;

        torm->doClosing();
        return 0;
    }


    ws_conn *find_avaliable_conn_info_slot()
    {
        for(int i=0;i<ws_conn_set.size();i++)
        {
          if(!ws_conn_set[i].isOccupied())
            return &(ws_conn_set[i]);
        }
        ws_conn empty;
        ws_conn_set.push_back(empty);

        return &(ws_conn_set[ws_conn_set.size()-1]);
    }

    ws_conn* add(ws_conn *info)
    {
        if(info == NULL)return NULL;
        if( !info->isOccupied())
          return NULL;

        if(find(info->getSocket())!=NULL)
        {
          return NULL;
        }
        ws_conn* tmp = find_avaliable_conn_info_slot();
        tmp->COPY_property(info);
      	return tmp;
    }

    int size()
    {
        int len=0;
        for(int i=0;i<ws_conn_set.size();i++)
        {
          if(ws_conn_set[i].isOccupied())
          {
            len++;
          }
        }
        return len;
    }
};




class ws_server{

  int listenSocket;
  fd_set evtSet;
  int fdmax;
public:
  ws_server(int port)
  {
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        printf("Error:create socket failed\n");
        return;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);
    if (bind(listenSocket, (struct sockaddr *) &local, sizeof(local)) == -1) {
        printf("bind failed\n");
        close(listenSocket);
        listenSocket=-1;
        return;
    }

    if (listen(listenSocket, 1) == -1) {
        printf("listen failed\n");
        close(listenSocket);
        listenSocket=-1;
        return;
    }

    FD_ZERO(&evtSet);
    FD_SET(listenSocket, &evtSet);
    fdmax=listenSocket;



    printf("opened %s:%d  listenSocket:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port),listenSocket);

  }
  ws_conn_entity_pool ws_conn_pool;

  int findMaxFd()
  {
    int max=listenSocket;

    std::vector <ws_conn>* servers = ws_conn_pool.getServers();
    for(int i=0;i<(*servers).size();i++)
    {
      if((*servers)[i].isOccupied() && (*servers)[i].getSocket() > max)
      {
        max = (*servers)[i].getSocket();
      }
    }

    return max;
  }

  int runLoop(struct timeval *tv)
  {
    if(listenSocket == -1)
    {
      return -1;
    }
    fd_set read_fds = evtSet;


    if (select(fdmax+1, &read_fds, NULL, NULL, tv) == -1) {
      perror("select");
      exit(4);
    }


    if(FD_ISSET(listenSocket, &read_fds))
    {
        struct sockaddr_in remote;
        socklen_t sockaddrLen = sizeof(remote);
        int NewSock = accept(listenSocket, (struct sockaddr*)&remote, &sockaddrLen);
        if (NewSock == -1) {
            printf("accept failed");
        }
        ws_conn* conn = ws_conn_pool.find_avaliable_conn_info_slot();
        conn->setSocket(NewSock);
        conn->setAddr(remote);
        printf("connected %s:%d\n",
        inet_ntoa(conn->getAddr().sin_addr), ntohs(conn->getAddr().sin_port));


        FD_SET(NewSock, &evtSet);
        if (NewSock > fdmax) {
          fdmax = NewSock;
        }

        printf("List size %d\n", ws_conn_pool.size());

    }
    else
    {
        std::vector <ws_conn>* servers = ws_conn_pool.getServers();
        bool evt_trigger=false;
        for(int i=0;i<(*servers).size();i++)
        {
            if((*servers)[i].isOccupied() && FD_ISSET((*servers)[i].getSocket(), &read_fds))
            {
              evt_trigger = true;
              int fd = (*servers)[i].getSocket();
              (*servers)[i].runLoop();
              if(!(*servers)[i].isOccupied())
              {
                  printf("List size %d\n", ws_conn_pool.size());
                  FD_CLR(fd, &evtSet);
                  fdmax = findMaxFd();
              }
              break;
            }
        }


        if(!evt_trigger)
        {
          printf("No matching event\n");
          return -2;
        }
    }
    return 0;

  }
};
