
#include "ws_server_util.h"

#include "logctrl.h"
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


    int enable = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int)) < 0)
    {
      //throw -3;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEPORT, (char*)&enable, sizeof(int)) < 0)
    {
      //throw -3;
    }
#endif



    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(port);

    if (bind(listenSocket, (struct sockaddr *) &local, sizeof(local))<0) {
        printf("bind failed\n");
        close(listenSocket);
        listenSocket = -1;
        return;
    }

    if (listen(listenSocket, 1) <0) {
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
    std::vector <ws_conn*>* servers = ws_conn_pool.getServers();
    
    for (int i = 0; i < (*servers).size(); i++)
    {
        printf("peer %s:%d  sock:%d\n",
            inet_ntoa((*servers)[i]->getAddr().sin_addr),
            ntohs((*servers)[i]->getAddr().sin_port),(*servers)[i]->getSocket());
    }
    return ret;
}

ws_server::~ws_server()
{
    //shutdown(listenSocket);
    std::vector <ws_conn*>* servers = ws_conn_pool.getServers();
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



void ws_server::set_fd_set( fd_set *fdSet)
{
    FD_SET(listenSocket,fdSet);
    std::vector <ws_conn*>* servers = ws_conn_pool.getServers();
    for (int i = 0; i < (*servers).size(); i++)
    {
        if ((*servers)[i]->isOccupied())
        {
            FD_SET((*servers)[i]->getSocket(),fdSet);
        }
    }
}

fd_set ws_server::get_fd_set()
{
    fd_set newSet;
    FD_ZERO(&newSet);

    set_fd_set(&newSet);
    FD_SET(listenSocket,&newSet);
    evtSet=newSet;

    return evtSet;
}

int ws_server::findMaxFd()
{
    int max = listenSocket;

    std::vector <ws_conn*>* servers = ws_conn_pool.getServers();
    for (int i = 0; i < (*servers).size(); i++)
    {
        if ((*servers)[i]->isOccupied() && (*servers)[i]->getSocket() > max)
        {
            max = (*servers)[i]->getSocket();
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

  if(listenSocket == -1)
  {
    return -1;
  }
  fd_set read_fds = evtSet;


  if (select(fdmax+1, &read_fds, NULL, NULL, tv) == -1) {
    perror("select");
    exit(4);
  }
  return runLoop(&read_fds ,tv);
}
int ws_server::runLoop(fd_set *read_fds,struct timeval *tv)
{


    LOGV(">>>>>");
    if (FD_ISSET(listenSocket,read_fds))
    {
        FD_CLR(listenSocket, read_fds);
        LOGV("listenSocket");
        struct sockaddr_in remote;
        socklen_t sockaddrLen = sizeof(remote);
        LOGV("accept::");
        int NewSock = accept(listenSocket, (struct sockaddr*)&remote, &sockaddrLen);
        if (NewSock == -1) {
            
            LOGV("accept failed");
            printf("accept failed");
            //sleep(1000);
            return -2;
        }
        
        LOGV("Find slot");
        ws_conn* conn = ws_conn_pool.find_avaliable_conn_info_slot();
        LOGV("slot is here:%p",conn);
        conn->setSocket(NewSock);
        conn->setAddr(remote);
        
        LOGV("connected %s:%d sock:%d",
               inet_ntoa(conn->getAddr().sin_addr), ntohs(conn->getAddr().sin_port),conn->getSocket());

        conn->setCallBack(this);


        FD_SET(NewSock, &evtSet);
        if (NewSock > fdmax) {
            fdmax = NewSock;
        }

        printf("List size %d\n", ws_conn_pool.size());

    }
    else
    {
        LOGV("listenSocket else");
        std::vector <ws_conn*>* servers = ws_conn_pool.getServers();
        bool evt_trigger = false;
        for (int i = 0; i < (*servers).size(); i++)
        {
            int fd = (*servers)[i]->getSocket();
            if ((*servers)[i]->isOccupied() && FD_ISSET(fd, read_fds))
            {
                evt_trigger = true;
                FD_CLR(fd, read_fds);
                (*servers)[i]->runLoop();
                if (!(*servers)[i]->isOccupied())
                {
                    printf("List size %d\n", ws_conn_pool.size());
                    FD_CLR(fd, &evtSet);
                    fdmax = findMaxFd();
                }
                break;
            }
        }
        LOGV("listenSocket else end for");


        if (!evt_trigger)
        {
            LOGV("No matching event");
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
        if (ws_conn_set[i]->getSocket() == sock)
            return (ws_conn_set[i]);
    }
    return NULL;
}

std::vector <ws_conn*>* ws_conn_entity_pool::getServers()
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
        if (!ws_conn_set[i]->isOccupied())
            return (ws_conn_set[i]);
    }
    ws_conn_set.push_back(new ws_conn());

    return (ws_conn_set[ws_conn_set.size() - 1]);
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
    if(sock<0)return -1;
    ssize_t written = send(sock, (const char*)buffer, bufferSize, 0);
    if (written == -1) {
        perror("safeSend:send failed");
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

int ws_conn::doHandShake(void *buff, ssize_t buffLen,struct handshake *p_hs)
{
    ((char*)buff)[buffLen] = '\0';
    struct handshake &hs = *p_hs;
    nullHandshake(&hs);

    enum wsFrameType frameType = wsParseHandshake((unsigned char *)buff, buffLen, &hs);

    if (frameType != WS_OPENING_FRAME) {
        return -1;
    }
    strcpy_m(resource, sizeof(resource), hs.resource);
    //printf("%s:%s\n", __func__, resource);

    // if resource is right, generate answer handshake and send it
    size_t frameSize = sendBuf.size();

    wsGetHandshakeAnswer(&hs, &sendBuf[0], &frameSize);
    //freeHandshake(&hs);
    if (safeSend(sock, &sendBuf[0], frameSize) != 0)
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

    printf("%s:cb:%p sock:%d\n",__func__,cb,sock);
    sock=-1;
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


int ws_conn::event_TCP_RECV(uint8_t *data, size_t dataSize)
{
    //BY default, echo
    //size_t frameSize = sendBuf.size();
    //int ret = wsMakeFrame2(data, dataSize, &(sendBuf[0]), &frameSize, frameType, isFinal);

    if(cb!=NULL)
    {
      websock_data cb_data=genCallbackData(websock_data::eventType::DATA_FRAME_TCP);
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

            // printf("dataSize:%d isFinal:%d\n", dataSize, isFinal);
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
    if (readed<=0) {
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

    if(ws_state == WS_STATE_TCP)
    {
      event_TCP_RECV( &(recvBuf[0]), readed);
      return 0;
    }


    accBufDataLen = 0;//accBufDataLen is for receving accumulation, only useful in normal mode
    if (ws_state == WS_STATE_OPENING)
    {

        if(cb!=NULL)
        {
          cb->ws_callback(genCallbackData(websock_data::eventType::OPENING));
        }
        struct handshake hs;
        if (doHandShake(&(recvBuf[0]), readed, &hs) != 0 )
        {
            // printf("Error:Hand shake failed...");
            // ws_state = WS_STATE_CLOSING;
            // doClosing();
            
            websock_data ws_dat = genCallbackData(websock_data::eventType::TCP_CONNECTION_FINISHED);
            cb->ws_callback(ws_dat);

            ws_state=WS_STATE_TCP;
            event_TCP_RECV( &(recvBuf[0]), readed);

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
    if(packet == NULL || packet->peer==NULL)return -1;

    if(this!=packet->peer)return -20;
    if(packet->type == websock_data::CLOSING)
    {
        doClosing();
        return 0;
    }

    enum wsFrameType frameType = (enum wsFrameType)packet->data.data_frame.type;

    if(frameType==WS_CLOSING_FRAME)
    {

        doClosing();
        return 0;
    }

    if(frameType==TCP_BINARY_FRAME)
    {
      return safeSend(sock,packet->data.data_frame.raw, packet->data.data_frame.rawL);
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
