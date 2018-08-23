
#include "ws_server_util.h"

#include "websocket.h"
//////////////////////////////ws_server/////////////////////////////////////

ws_server::ws_server(int port,ws_protocol_callback *cb):ws_protocol_callback(this)
{
    this->cb = cb;
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
        listenSocket = -1;
        return;
    }

    if (listen(listenSocket, 1) == -1) {
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


ws_server::~ws_server()
{
    //shutdown(listenSocket);
    close(listenSocket);
}

int ws_server::findMaxFd()
{
    int max = listenSocket;

    std::vector <ws_conn>* servers = ws_conn_pool.getServers();
    for (int i = 0; i < (*servers).size(); i++)
    {
        if ((*servers)[i].isOccupied() && (*servers)[i].getSocket() > max)
        {
            max = (*servers)[i].getSocket();
        }
    }

    return max;
}
int ws_server::ws_callback(websock_data data, void* param)
{
  if(cb)
  {
    cb->ws_callback(data);
  }
  else
  {
    printf("%s: type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());

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


    if (select(fdmax + 1, &read_fds, NULL, NULL, tv) == -1) {
        perror("select");
        exit(4);
    }


    if (FD_ISSET(listenSocket, &read_fds))
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
        conn->setCallBack(this);

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
        bool evt_trigger = false;
        for (int i = 0; i < (*servers).size(); i++)
        {
            if ((*servers)[i].isOccupied() && FD_ISSET((*servers)[i].getSocket(), &read_fds))
            {
                evt_trigger = true;
                int fd = (*servers)[i].getSocket();
                (*servers)[i].runLoop();
                if (!(*servers)[i].isOccupied())
                {
                    printf("List size %d\n", ws_conn_pool.size());
                    FD_CLR(fd, &evtSet);
                    fdmax = findMaxFd();
                }
                break;
            }
        }


        if (!evt_trigger)
        {
            printf("No matching event\n");
            return -2;
        }
    }
    return 0;

}


int ws_server::send_pkt(websock_data *packet)
{
    if(packet == NULL || packet->peer==NULL)return -1;
    ws_conn *client = ws_conn_pool.find(packet->peer->getSocket());
    if(client!=packet->peer)return -20;
    return client->send_pkt(packet);
}
//////////////////////////////ws_conn_entity_pool/////////////////////////////////////

ws_conn *ws_conn_entity_pool::find(int sock)
{
    for (int i = 0; i < ws_conn_set.size(); i++)
    {
        if (ws_conn_set[i].getSocket() == sock)
            return &(ws_conn_set[i]);
    }
    return NULL;
}

std::vector <ws_conn>* ws_conn_entity_pool::getServers()
{
    return &ws_conn_set;
}

int ws_conn_entity_pool::remove(int sock)
{
    ws_conn * torm = find(sock);
    if (torm == NULL)
        return -1;

    torm->doClosing();
    return 0;
}


ws_conn *ws_conn_entity_pool::find_avaliable_conn_info_slot()
{
    for (int i = 0; i < ws_conn_set.size(); i++)
    {
        if (!ws_conn_set[i].isOccupied())
            return &(ws_conn_set[i]);
    }
    ws_conn empty;
    ws_conn_set.push_back(empty);

    return &(ws_conn_set[ws_conn_set.size() - 1]);
}

ws_conn* ws_conn_entity_pool::add(ws_conn *info)
{
    if (info == NULL)return NULL;
    if ( !info->isOccupied())
        return NULL;

    if (find(info->getSocket()) != NULL)
    {
        return NULL;
    }
    ws_conn* tmp = find_avaliable_conn_info_slot();
    tmp->COPY_property(info);
    return tmp;
}

int ws_conn_entity_pool::size()
{
    int len = 0;
    for (int i = 0; i < ws_conn_set.size(); i++)
    {
        if (ws_conn_set[i].isOccupied())
        {
            len++;
        }
    }
    return len;
}




//////////////////////////////ws_conn/////////////////////////////////////

int ws_conn::safeSend(int sock, const uint8_t *buffer, size_t bufferSize)
{
#ifdef PACKET_DUMP
    printf("out packet:\n");
    fwrite(buffer, 1, bufferSize, stdout);
    printf("\n");
#endif
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


ws_conn::ws_conn() {
    RESET();
}


void ws_conn::setCallBack(ws_protocol_callback* cb)
{
  this->cb = cb;
}


void ws_conn::RESET()
{
    cb = NULL;
    sock = -1;
    ws_state = WS_STATE_OPENING;
    memset(&addr, 0, sizeof(addr));
    accBufDataLen = 0;
    if (recvBuf.size() < recvBufSizeInc)
        recvBuf.resize(recvBufSizeInc);

    sendBuf.resize(recvBufSizeInc);
}


int ws_conn::setSocket(int socket)
{
    sock = socket;
}



int ws_conn::setAddr(struct sockaddr_in address)
{
    addr = address;
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
    if ( dstMaxSize < 0 || src == NULL)return -1;
    dstMaxSize--;
    dst[dstMaxSize] = '\0';
    int i;
    for (i = 0 ; i < dstMaxSize && src[i]; i++)
    {
        dst[i] = src[i];
    }
    dst[i] = src[i];
    return i;
}

int ws_conn::doHandShake(void *buff, ssize_t buffLen)
{
    ((char*)buff)[buffLen] = '\0';
    struct handshake hs;
    nullHandshake(&hs);

    enum wsFrameType frameType = wsParseHandshake((unsigned char *)buff, buffLen, &hs);

    if (frameType != WS_OPENING_FRAME) {
        return -1;
    }
    strcpy_m(resource, sizeof(resource), hs.resource);
    printf("%s:%s\n", __func__, resource);

    // if resource is right, generate answer handshake and send it
    size_t frameSize = sendBuf.size();

    wsGetHandshakeAnswer(&hs, &sendBuf[0], &frameSize);
    freeHandshake(&hs);
    if (safeSend(sock, &sendBuf[0], frameSize) == EXIT_FAILURE)
    {
        doClosing();
        return -1;
    }
    return 0;
}

int ws_conn::doClosing()
{
    if (isOccupied())
        close(sock);

    if(cb!=NULL)
    {
      cb->ws_callback(genCallbackData(websock_data::eventType::CLOSING));
    }
    RESET();
    printf("%s\n", __func__);
    return 0;
}

websock_data ws_conn::genCallbackData(websock_data::eventType type)
{
    websock_data data;
    data.peer=this;
    data.type =type;
    return data;
}
int ws_conn::event_WsRECV(uint8_t *data, size_t dataSize, enum wsFrameType frameType, bool isFinal)
{
    //BY default, echo
    //size_t frameSize = sendBuf.size();
    //int ret = wsMakeFrame2(data, dataSize, &(sendBuf[0]), &frameSize, frameType, isFinal);

    if(cb!=NULL)
    {
      websock_data cb_data=genCallbackData(websock_data::eventType::DATA_FRAME);
      cb_data.data.data_frame.type = frameType;
      cb_data.data.data_frame.raw = data;
      cb_data.data.data_frame.rawL = dataSize;
      cb_data.data.data_frame.isFinal = isFinal;
      cb->ws_callback(cb_data);
    }

    return 0;
}

int ws_conn::doNormalRecv(void *buff, size_t buffLen, size_t *ret_restLen, enum wsFrameType *ret_lastFrameType)
{
    int h_padding = 0;
    enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
    while ( buffLen >  h_padding )
    {
        size_t curPktLen;
        bool isFinal;

        uint8_t *data = NULL;
        size_t dataSize = 0;

        uint8_t* tmpD = (uint8_t*)buff + h_padding;
        frameType = wsParseInputFrame2(tmpD, buffLen - h_padding,
                                       &data, &dataSize, &curPktLen, &isFinal);
        //printf("frameType:%d    %02x %02x %02x\n",frameType,tmpD[0],tmpD[1],tmpD[2]);
        *ret_lastFrameType = frameType;

        if (frameType == WS_TEXT_FRAME || frameType == WS_BINARY_FRAME )
        {
            h_padding += curPktLen;
            /*for(int i=0;i<dataSize;i++)
            {
              printf("%02x ",data[i]);
            }*/

            printf("dataSize:%d isFinal:%d\n", dataSize, isFinal);
            event_WsRECV( data, dataSize, frameType, isFinal);

        }
        else if (frameType == WS_CONT_FRAME )
        {
            h_padding += curPktLen;
            /*for(int i=0;i<dataSize;i++)
            {
              printf("%02x ",data[i]);
            }*/
            printf("CONT dataSize:%d\n", dataSize);

            event_WsRECV( data, dataSize, frameType, isFinal);
        }
        else if ( frameType == WS_INCOMPLETE_FRAME )
        {
            //The packet is not finished, wait for receiving more
            break;
        }
        else if ( frameType == WS_CLOSING_FRAME )
        {
            h_padding += curPktLen;
            ws_state = WS_STATE_CLOSING;
            *ret_restLen = 0;
            return doClosing();
        }
        else if ( frameType == WS_ERROR_FRAME )
        {
            break;
        }
    }

    //The packet is incomplete/or error, remove finished packets
    //|finished|finished|incomplete| => |incomplete|
    //_______h_padding__^
    if ( frameType == WS_INCOMPLETE_FRAME || frameType == WS_ERROR_FRAME )
    {
        ssize_t newLen = buffLen - h_padding;

        if (h_padding != 0)
        {
            memcpy(buff, (uint8_t*)buff + h_padding, newLen);
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
    if ( !isOccupied() )
    {
        return -1;
    }
    //printf("sock:%d size:%d\n",sock,recvBuf.size());

    if (recvBuf.size() == accBufDataLen)
    {
        printf("Buffer size(%d) is not enough, expend to %d\n", recvBuf.size(), recvBuf.size() + recvBufSizeInc);
        recvBuf.resize(recvBuf.size() + recvBufSizeInc);
    }
    ssize_t readed = recv(sock, (char*) (&(recvBuf[0]) + accBufDataLen), recvBuf.size() - accBufDataLen, 0);
    if (!readed) {
        ws_state = WS_STATE_CLOSING;
        doClosing();
        return -1;
    }
    accBufDataLen += readed;

    //printf("readed:%d\n",readed);

    /*if(accBufDataLen==recvBuf.size())
    {
      recvBuf.reserve(recvBuf.size()+recvBufSizeInc);
    }*/

    if (ws_state == WS_STATE_NORMAL)
    {
        if (doNormalRecv(&(recvBuf[0]), accBufDataLen, &accBufDataLen, &lastPktType) == 0 )
        {
        }

        if (lastPktType == WS_ERROR_FRAME && accBufDataLen == recvBuf.size())
        {
            printf(">>>>>ERROR QUIT\n");
        }
        return 0;
    }

    accBufDataLen = 0;//accBufDataLen is for receving accumulation, only useful in normal mode
    if (ws_state == WS_STATE_OPENING)
    {

        if(cb!=NULL)
        {
          cb->ws_callback(genCallbackData(websock_data::eventType::OPENING));
        }
        if (doHandShake(&(recvBuf[0]), readed) != 0 )
        {
            printf("Error:Hand shake failed...");
            ws_state = WS_STATE_CLOSING;
            doClosing();
        }
        else
        {
            ws_state = WS_STATE_NORMAL;
        }
        return 0;
    }

    if (ws_state == WS_STATE_CLOSING)
    {
        doClosing();
    }

}

int ws_conn::send_pkt(websock_data *packet)
{
    if(packet == NULL || packet->peer==NULL)return -1;

    if(this!=packet->peer)return -20;
    enum wsFrameType frameType = (enum wsFrameType)packet->data.data_frame.type;

    if(frameType==WS_CLOSING_FRAME)
    {
        doClosing();
        return 0;
    }

    if(frameType!=WS_TEXT_FRAME && frameType!=WS_BINARY_FRAME
        && frameType!=WS_PING_FRAME&& frameType!=WS_PONG_FRAME
        && frameType!=WS_CONT_FRAME )
        return -3;

    return send_pkt(packet->data.data_frame.raw, packet->data.data_frame.rawL
      ,frameType,packet->data.data_frame.isFinal);
}

int ws_conn::send_pkt(const uint8_t *packet, size_t pkt_size,int type,bool isFinal)
{
    size_t frameSize=sendBuf.size();

    int saveSpaceMargin=150;
    if(frameSize < pkt_size+saveSpaceMargin)
    {
        int tmp = (pkt_size+saveSpaceMargin - frameSize)/recvBufSizeInc;

        sendBuf.resize(frameSize + (tmp+1)*recvBufSizeInc);
        frameSize=sendBuf.size();
    }


    int ret = wsMakeFrame2(packet, pkt_size,
        &(sendBuf[0]), &frameSize, (enum wsFrameType)type,isFinal);
    if(ret)
    {
      printf("wsMakeFrame2 error:%d\n",ret);
      //return -1;
    }
    return safeSend(sock, &sendBuf[0], frameSize);
}
