#include "stdafx.h"
#include "windef.h"
#include "GevForceIp.h"



static BYTE	m_RecvTempBuf[576];


CGevForceIp::CGevForceIp(UINT iListenAddr) : m_sock(iListenAddr, GVCP_UDP_PORT)
{
    m_usCmdId = 0;
}



CGevForceIp::~CGevForceIp(void)
{	
	
}


int CGevForceIp::BroadCastRecv(char *sBuf,int max_len)
{
	return m_sock.recv(sBuf, max_len);
}

BOOL CGevForceIp::BroadCastSend(unsigned char *sBuf,int len)
{
	return m_sock.send(sBuf, len) > 0;
}

//����cmd��
void MakeGvcpHead(BYTE* pHead,USHORT cmd,USHORT len,BOOL bBoardCast,BOOL bNeedAck)
{
	static USHORT CmdID;
	pHead[0] = 0x42;

	pHead[1] = bNeedAck?0x01:0x0;	//flag

	if(bBoardCast)
		pHead[1] |= 0x10;

	pHead[2] = cmd>>8;
	pHead[3] = cmd&0xff;

	pHead[4] = len>>8;
	pHead[5] = len&0xff;

	CmdID++;
	pHead[6] = CmdID>>8;
	pHead[7] = CmdID&0xff;
}

INT CGevForceIp::GevSetForceIp(BYTE *mac,BYTE *ip,BYTE *mask,BYTE *gw,BOOL bNeedAck)
{
	int trys,retrys;
	int n;
	INT CmdLen;
	gvcp_forceip_t g_ForceIPPack;
	BYTE * pCommand = (BYTE *)&g_ForceIPPack;

	CmdLen = sizeof(gvcp_forceip_t) - sizeof(gvcp_cmd_head_t);
	MakeGvcpHead((BYTE *)&g_ForceIPPack, FORCEIP_CMD,CmdLen,TRUE,bNeedAck);
	memcpy(g_ForceIPPack.mac_addr,mac,6);
	memcpy(g_ForceIPPack.static_ip,ip,4);
	memcpy(g_ForceIPPack.static_netmask,mask,4);
	memcpy(g_ForceIPPack.static_gateway,gw,4);

	retrys = 3;
	do{

		if(BroadCastSend(pCommand,sizeof(gvcp_forceip_t)) != TRUE)
		{
			return FALSE;
		}

		if (bNeedAck)
		{

			trys = 200;
			while (trys--)
			{	

				n = BroadCastRecv((CHAR *)m_RecvTempBuf,sizeof(m_RecvTempBuf));
				Sleep(5);
				if (n > 0)
				{
					if (GET_GVCP_CMD(m_RecvTempBuf) != (FORCEIP_CMD + 1) || GET_GVCP_CMD_ID(m_RecvTempBuf) != GET_GVCP_CMD_ID(pCommand))//not ack
					{
						continue;
					}
					else
					{	
						if (GET_GVCP_ACK_STATUS(m_RecvTempBuf) != GEV_STATUS_SUCCESS)
							return FALSE; //GET_GVCP_ACK_STATUS(AckBuf);
						else
							return TRUE;
					}
				}

			}
		}
		else
		{
			return TRUE;
		}

	}while(retrys-- > 0);		

	return FALSE;
}


INT CGevForceIp::GevSetForceIpCustom(BYTE *mac,BYTE *ip,BYTE *mask,BYTE *gw,BYTE alloc_mode,BYTE save,BOOL bNeedAck)
{
	int trys,retrys;
	int n;
	INT CmdLen;
	gvcp_custom_forceip_t g_ForceIPPack;
	BYTE * pCommand = (BYTE *)&g_ForceIPPack;

	CmdLen = sizeof(gvcp_custom_forceip_t) - sizeof(gvcp_cmd_head_t);
	MakeGvcpHead((BYTE *)&g_ForceIPPack, IPCONFIG_CMD,CmdLen,TRUE,bNeedAck);
	memcpy(g_ForceIPPack.mac_addr,mac,6);
	memcpy(g_ForceIPPack.static_ip,ip,4);
	memcpy(g_ForceIPPack.static_netmask,mask,4);
	memcpy(g_ForceIPPack.static_gateway,gw,4);
	g_ForceIPPack.ipsave = save;
	g_ForceIPPack.alloc_mode = alloc_mode;

	retrys = 3;
	do{

		if(BroadCastSend(pCommand,sizeof(gvcp_custom_forceip_t)) != TRUE)
		{
			return FALSE;
		}

		if (bNeedAck)
		{

			trys = 200;
			while (trys--)
			{	

				n = BroadCastRecv((CHAR *)m_RecvTempBuf,sizeof(m_RecvTempBuf));
				Sleep(5);
				if (n > 0)
				{
					if (GET_GVCP_CMD(m_RecvTempBuf) != (IPCONFIG_CMD + 1) || GET_GVCP_CMD_ID(m_RecvTempBuf) != GET_GVCP_CMD_ID(pCommand))//not ack
					{
						continue;
					}
					else
					{	
						if (GET_GVCP_ACK_STATUS(m_RecvTempBuf) != GEV_STATUS_SUCCESS)
							return FALSE; //GET_GVCP_ACK_STATUS(AckBuf);
						else
							return TRUE;
					}
				}

			}
		}
		else
		{
			return TRUE;
		}

	}while(retrys-- > 0);		

	return FALSE;
}


//�������bootloaderģʽ
INT CGevForceIp::DeviceEnterBootloader(BYTE *mac,BOOL bNeedAck)
{

	int trys,retrys;
	int n;
	INT CmdLen;
	gvcp_custom_enterbootloader_t g_Pack;
	BYTE * pCommand = (BYTE *)&g_Pack;

	CmdLen = sizeof(gvcp_custom_enterbootloader_t) - sizeof(gvcp_cmd_head_t);
	MakeGvcpHead((BYTE *)&g_Pack, ENTER_BOOTLOADER_CMD,CmdLen,TRUE,bNeedAck);
	memcpy(g_Pack.mac_addr,mac,6);
	memcpy(g_Pack.chk_string,ENTERBOOTLOADER_STR,sizeof(ENTERBOOTLOADER_STR));

	retrys = 3;
	do{

		if(BroadCastSend(pCommand,sizeof(gvcp_custom_enterbootloader_t)) != TRUE)
		{
			return FALSE;
		}

		if (bNeedAck)
		{

			trys = 200;
			while (trys--)
			{	

				n = BroadCastRecv((CHAR *)m_RecvTempBuf,sizeof(m_RecvTempBuf));
				Sleep(5);
				if (n > 0)
				{
					if (GET_GVCP_CMD(m_RecvTempBuf) != (ENTER_BOOTLOADER_CMD + 1) || GET_GVCP_CMD_ID(m_RecvTempBuf) != GET_GVCP_CMD_ID(pCommand))//not ack
					{
						continue;
					}
					else
					{	
						if (GET_GVCP_ACK_STATUS(m_RecvTempBuf) != GEV_STATUS_SUCCESS)
							return FALSE; //GET_GVCP_ACK_STATUS(AckBuf);
						else
							return TRUE;
					}
				}

			}
		}
		else
		{
			return TRUE;
		}

	}while(retrys-- > 0);		

	return FALSE;
}


//��������ϻ�����ģʽ
INT CGevForceIp::DeviceEnterAgingTest(void)
{

    INT CmdLen;
    gvcp_custom_enteragingtest_t g_Pack;
    BYTE * pCommand = (BYTE *)&g_Pack;

    CmdLen = sizeof(gvcp_custom_enteragingtest_t) - sizeof(gvcp_cmd_head_t);
    MakeGvcpHead((BYTE *)&g_Pack, ENTER_AGING_TEST_CMD,CmdLen,TRUE,0);
    memcpy(g_Pack.chk_string,ENTERAGINGTESTSTR,sizeof(ENTERAGINGTESTSTR));

    if(BroadCastSend(pCommand,sizeof(gvcp_custom_enteragingtest_t)) != TRUE)
    {
        return FALSE;
    }
    return TRUE;
}