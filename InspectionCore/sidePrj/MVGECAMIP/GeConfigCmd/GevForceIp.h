
#ifndef __GEVFORCEIP_H__
#define __GEVFORCEIP_H__


#include "windef.h"
#include "GigeVisionDef.h"
#include "BroadcastSocket.h"


class CGevForceIp
{
public:

	CGevForceIp(UINT iListenAddr); 
	~CGevForceIp(void);

	INT GevSetForceIp(BYTE *mac,BYTE *ip,BYTE *mask,BYTE *gw,BOOL bNeedAck=1);
	INT GevSetForceIpCustom(BYTE *mac,BYTE *ip,BYTE *mask,BYTE *gw,BYTE alloc_mode,BYTE save,BOOL bNeedAck=1); //�Զ����FORCE IP
	INT DeviceEnterBootloader(BYTE *mac,BOOL bNeedAck=1);
    INT DeviceEnterAgingTest();

private:
	int BroadCastRecv(char *sBuf,int max_len);
	BOOL BroadCastSend(unsigned char *sBuf,int len);

	
	



private:

	CBroadcastSocket    m_sock;
	USHORT				m_usCmdId;





protected:


};

#endif
