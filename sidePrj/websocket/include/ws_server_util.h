

#include <vector>
#include <netinet/in.h>
#include "websocket.h"

class ws_server{

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
      ssize_t written = send(sock, buffer, bufferSize, 0);
      if (written == -1) {
          close(sock);
          perror("send failed");
          return EXIT_FAILURE;
      }
      if (written != bufferSize) {
          close(sock);
          perror("written not all bytes");
          return EXIT_FAILURE;
      }
      
      return EXIT_SUCCESS;
  }
  public:
  int sock;
  struct sockaddr_in addr;
  char resource[128];
  ws_server(){
    RESET();
  }

  void RESET()
  {
    sock=0;
    ws_state = WS_STATE_OPENING;
    memset(&addr,0,sizeof(addr));
    accBufDataLen = 0;
    if(recvBuf.size()<recvBufSizeInc)
      recvBuf.resize(recvBufSizeInc);
    
    sendBuf.resize(recvBufSizeInc);
  }


  void COPY_property(ws_server *from)
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
    for(i=0 ; i<dstMaxSize && *src; i++)
    {
      dst[i]=src[i];
    }
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
      return -1;
    }
    return 0;
  }

  int doClosing()
  {
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

          printf("dataSize:%d isFinal:%d\n",dataSize,isFinal);
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
    if( sock == 0 )
    {
      return -1;
    }
    //printf("sock:%d size:%d\n",sock,recvBuf.size());

    if(recvBuf.size() == accBufDataLen)
    {
      printf("Buffer size(%d) is not enough, expend to %d\n",recvBuf.size(),recvBuf.size()+recvBufSizeInc);
      recvBuf.resize(recvBuf.size()+recvBufSizeInc);
    }
    ssize_t readed = recv(sock, &(recvBuf[0])+accBufDataLen, recvBuf.size()-accBufDataLen, 0);
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

    std::vector <ws_server> ws_conn_set;

    public:
    ws_server *find(int sock)
    {
      	for(int i=0;i<ws_conn_set.size();i++)
      	{
      		if(ws_conn_set[i].sock == sock)
      			return &(ws_conn_set[i]);
      	}
      	return NULL;
    }

    std::vector <ws_server>* getServers()
    {
      return &ws_conn_set;
    }

    int remove(int sock)
    {
        ws_server * torm = find(sock);
        if(torm == NULL)
          return -1;

        torm->sock = 0;
        return 0;
    }


    ws_server *find_avaliable_conn_info_slot()
    {
        for(int i=0;i<ws_conn_set.size();i++)
        {
          if(ws_conn_set[i].sock == 0)
            return &(ws_conn_set[i]);
        }
        ws_server empty;
        ws_conn_set.push_back(empty);

        return &(ws_conn_set[ws_conn_set.size()-1]);
    }

    ws_server* add(ws_server *info)
    {
        if(info == NULL)return NULL;
        if(info->sock == 0)
          return NULL;

        if(find(info->sock)!=NULL)
        {
          return NULL;
        }
        ws_server* tmp = find_avaliable_conn_info_slot();
        tmp->COPY_property(info);
      	return tmp;
    }

    int size()
    {
        int len=0;
        for(int i=0;i<ws_conn_set.size();i++)
        {
          if(ws_conn_set[i].sock)
          {
            len++;
          }
        }
        return len;
    }
};
