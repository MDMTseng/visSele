#include "GevDiscovery.h"
#include "log.h"

static void* SendThread(LPVOID param)
{

	CGevDiscovery* pMainWin = (CGevDiscovery*)param;
    INT cnt = 0;
	while(pMainWin->m_bThreadStop == FALSE)
	{
		if(cnt >= 10)
        {
            pMainWin->SendDiscovery();
            cnt = 0;
        }
        
        Sleep(100);
        cnt++;
	}
    
    return 0;
}

static void* RevThread(LPVOID param)
{
    CGevDiscovery* pMainWin = (CGevDiscovery*)param;
    gvcp_discovery_ack_t sRevBuf;

    while(pMainWin->m_bThreadStop == FALSE)
    {
        if(pMainWin->DiscoveryReceive(&sRevBuf))
        {
            pMainWin->DeviceListAdd(sRevBuf);
        }
    }
    
    return 0;
}

static LPVOID ChkThread(LPVOID param)
{
    int cnt = 0;
    CGevDiscovery* pMainWin = (CGevDiscovery*)param;

    while(pMainWin->m_bThreadStop == FALSE)
    {
        if(cnt >= 10)
        {
            pMainWin->DeviceListChk();
            cnt = 0;
        }

        Sleep(100);
        cnt++;
    }

    return 0;
}

CGevDiscovery::CGevDiscovery(char const* pManuName,UINT iListenAddr, bool bAutoUpdate)
    : m_sock(iListenAddr, GVCP_UDP_PORT)
{
    memset(&m_sDevListInfo,0,sizeof(DEVLIST_INFOR));
    m_usCmdId = 0;
    m_pManuVendor = pManuName;
    m_bAutoUpdate = bAutoUpdate;

    m_bThreadStop = FALSE;
    pthread_create(&m_hThreadRev, NULL, RevThread, this);

    if (bAutoUpdate)
    {
        pthread_create(&m_hThreadSend, NULL, SendThread, this);
        pthread_create(&m_hThreadChk, NULL, ChkThread, this);
    }
}

CGevDiscovery::~CGevDiscovery(void)
{
    m_bThreadStop = TRUE;
    pthread_join(m_hThreadRev, NULL);

    if (m_bAutoUpdate)
    {
        pthread_join(m_hThreadSend, NULL);
        pthread_join(m_hThreadChk, NULL);
    }
}

int CGevDiscovery::BroadCastRecv(char *sBuf,int max_len)
{
	return m_sock.recv(sBuf, max_len);
}

int CGevDiscovery::BroadCastSend(unsigned char *sBuf,int len)
{
	return m_sock.send(sBuf, len);
}

//����cmd��
INT CGevDiscovery::MakeDiscoveryCmd(gvcp_cmd_head_t *buf)
{
	buf->status[0] = 0x42;
	buf->status[1] = 0x11;
	buf->answer[0] = DISCOVERY_CMD>>8;  
	buf->answer[1] = DISCOVERY_CMD&0xff;  

	buf->lenth[0] = 0;  
	buf->lenth[1] = 0;  

	m_usCmdId++;
	buf->id[0] = m_usCmdId >> 8;
	buf->id[1] = m_usCmdId & 0xff;

	return 0;
}

//����CMD
INT CGevDiscovery::SendDiscovery()
{
    if (!m_bAutoUpdate)
    {
        ::EnterCriticalSection(&m_csLock);
        memset(&m_sDevListInfo, 0, sizeof(m_sDevListInfo));
        ::LeaveCriticalSection(&m_csLock);
    }

    gvcp_cmd_head_t Buf;
    MakeDiscoveryCmd(&Buf);
    int err = BroadCastSend((unsigned char*)&Buf,sizeof(gvcp_cmd_head_t));
    if (err <= 0)
    {
        LOG("BroadCast send err: %d\n", err);
    }
    return 0;
}

BOOL CGevDiscovery::DiscoveryReceive(gvcp_discovery_ack_t *pDevInfo)
{
    if (BroadCastRecv((char*)pDevInfo,sizeof(gvcp_discovery_ack_t)) > 0)
    {
        if(pDevInfo->head.answer[0] == (DISCOVERY_ACK>>8)  && pDevInfo->head.answer[1] == (DISCOVERY_ACK & 0xff))
        {
            return 1;
        }
        
    }
    return 0;
}

//��ӵ��б���
void CGevDiscovery::DeviceListAdd(gvcp_discovery_ack_t DevInfo)
{
	
    INT iAddNeed = 1;

    //�������Ƚ�
    if(m_pManuVendor!=NULL)
    {
        if(strncmp((char*)&DevInfo.manufacturer_specific_information,m_pManuVendor,strlen(m_pManuVendor)) != 0) //�������ҵĹ��˵�
        {
            iAddNeed = 0;
            return;
        }
    }


    ::EnterCriticalSection( &m_csLock);

    //����������豸�б��У���ˢ��һ��
    for(INT i=0;i<m_sDevListInfo.list_num;i++)
    {
        if(m_sDevListInfo.is_valid[i]) 
        {
            if((memcmp(m_sDevList[i].mac_addr,DevInfo.mac_addr,6) == 0) &&
                (memcmp(m_sDevList[i].current_ip,DevInfo.current_ip,4) == 0))
            {
                m_sDevListInfo.create_time[i] = GetTickCount(); //ˢ����
                iAddNeed = 0; //����Ҫ��ӵ��豸�б���
                break;
            }
        }
    }

    //�·��ֵ��豸��ӵ��б�������
    if(iAddNeed)
    {
        //Խ����
        if(m_sDevListInfo.list_num >= DEVICE_LIST_MAX_NUM)
            return;

        memcpy(&m_sDevList[m_sDevListInfo.list_num],&DevInfo,sizeof(gvcp_discovery_ack_t));
        m_sDevListInfo.is_valid[m_sDevListInfo.list_num] = 1;
        m_sDevListInfo.create_time[m_sDevListInfo.list_num] = GetTickCount();
        m_sDevListInfo.list_num++; 
    }

    ::LeaveCriticalSection( &m_csLock);

	return;
}

void CGevDiscovery::DeviceListChk()
{
    UINT uTick;
    ::EnterCriticalSection( &m_csLock);
    uTick = GetTickCount();
    for(int i=0;i<m_sDevListInfo.list_num;i++)
    {
        if(m_sDevListInfo.is_valid[i])
        {
            if((uTick - m_sDevListInfo.create_time[i]) > 3000) //����3sδˢ�£���ɾ���豸
            {
                m_sDevListInfo.is_valid[i] = 0;
            }
        }
    }
    ::LeaveCriticalSection( &m_csLock);

}

INT CGevDiscovery::GetDiscoveryList(gvcp_discovery_ack_t *Dev)
{
    UINT iNum = 0;

	::EnterCriticalSection( &m_csLock);

    for(int i=0;i<m_sDevListInfo.list_num;i++)
    {
        if(m_sDevListInfo.is_valid[i])
        {
            memcpy(&Dev[iNum],&m_sDevList[i],sizeof(gvcp_discovery_ack_t));
            iNum++;
        }
        
    }
	::LeaveCriticalSection( &m_csLock);
    TRACE("GetDiscoveryList : %d - %d\r\n",m_sDevListInfo.list_num,iNum);
	return iNum;
}
