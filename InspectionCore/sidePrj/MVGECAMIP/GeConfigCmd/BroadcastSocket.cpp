#include "BroadcastSocket.h"
#include "log.h"

CBroadcastSocket::CBroadcastSocket(UINT iListenAddr, USHORT DestPort)
{
    m_iListenAddr = iListenAddr;
    m_DestPort = DestPort;
    m_SockRecv = 0;
    m_SockSend = 0;
    memset(&m_SaUdpServ, 0, sizeof(m_SaUdpServ));
    Init();
}

CBroadcastSocket::~CBroadcastSocket(void)
{
    if (m_SockSend > 0)
        closesocket(m_SockSend);
    if (m_SockRecv > 0)
        closesocket(m_SockRecv);
}

int CBroadcastSocket::recv(void *buf, int len)
{
	struct sockaddr_in saclient;

	int nSize = sizeof (struct sockaddr_in);
	int ret = 0;

    if (m_SockRecv == 0)
        return -1;

    timeval timeout;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(m_SockRecv, &readset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 200 * 1000;
    ret = select(m_SockRecv + 1, &readset, NULL, NULL, &timeout);

    if (ret > 0 )
    {
        ret = recvfrom (m_SockRecv, buf, len,0,(sockaddr*) &saclient,(socklen_t*)&nSize);
    }
	
	if (ret > 0 )
	{
		return ret;
	}
	else
	{
		ret = errno;
	}

	return 0;
}

int CBroadcastSocket::send(const void * msg, int len)
{
    if (m_SockSend == 0)
    {
        LOG("send socket err\n");
        return -1;
    }
	return sendto ( m_SockSend, msg, len, 0, (SOCKADDR *) &m_SaUdpServ, sizeof ( struct sockaddr_in ));
}

static USHORT RandPort()
{
    USHORT port;
    for (;;)
    {
        port = rand();
        if (port > 10000)
            break;
    }
    return port;
}

BOOL CBroadcastSocket::Init()
{
    struct sockaddr_in sin = { 0 };
    int nSize;
    int nError;
    int err;
    int socks[2] = { 0 };
    
    sin.sin_family = AF_INET;
    unsigned short port;
    for (int loop = 0; loop < 1000; ++loop)
    {
        if (socks[0] == 0)
        {
            for (int i = 0; i < 2; ++i)
            {
                socks[i] = socket(AF_INET,SOCK_DGRAM,0);

                int fBroadcast = 1;
                err = setsockopt(socks[i],SOL_SOCKET,SO_BROADCAST,(CHAR *)&fBroadcast,sizeof (fBroadcast));
                err = set_socket_nonblock(socks[i]);
            }
        }

        port  = RandPort();
        sin.sin_port = htons(port);
#ifdef __APPLE__
        sin.sin_addr.s_addr = htonl(0);
#else
        sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
#endif
        if(bind(socks[0], (SOCKADDR *)&sin, sizeof(sin))!=0)
        {
            LOG("bind broadcast addr err: %d\n", errno);
            continue;
        }

#ifdef __APPLE__
        int on = 1;
        err = setsockopt(socks[1],SOL_SOCKET,SO_REUSEADDR,(CHAR *)&on,sizeof (on));
#endif

        sin.sin_addr.s_addr = htonl(m_iListenAddr);
        if(bind(socks[1], (SOCKADDR *)&sin, sizeof(sin))==0)
        {
            m_SockRecv = socks[0];
            m_SockSend = socks[1];
            break;
        }
        else
        {
            LOG("bind eth addr err: %d\n", errno);
        }
        
        for (int i = 0; i < 2; ++i)
        {
            closesocket(socks[i]);
            socks[i] = 0;
        }
    }

    m_SaUdpServ.sin_family = AF_INET;
    m_SaUdpServ.sin_addr.s_addr = htonl ( INADDR_BROADCAST );
    m_SaUdpServ.sin_port = htons (m_DestPort);

    return TRUE;
}
