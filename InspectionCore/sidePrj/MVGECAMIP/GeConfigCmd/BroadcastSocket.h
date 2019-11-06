
#ifndef __BROADCAST_SOCKET_H__
#define __BROADCAST_SOCKET_H__

#include <sys/socket.h>
#include <netinet/in.h>
#include "GigeVisionDef.h"
#include "windef.h"

class CBroadcastSocket
{
public:

    CBroadcastSocket(UINT iListenAddr, USHORT DestPort);
	~CBroadcastSocket(void);

	int send(const void * msg, int len);
	int recv(void *buf, int len);

private:

	BOOL Init();

private:

    int              	m_SockSend;
	int					m_SockRecv;
	struct sockaddr_in  m_SaUdpServ;
    UINT                m_iListenAddr;
	USHORT				m_DestPort;
};

#endif
