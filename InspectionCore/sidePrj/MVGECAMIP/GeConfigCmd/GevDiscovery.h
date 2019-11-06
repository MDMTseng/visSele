
#ifndef __GEVDISCOVERY_H__
#define __GEVDISCOVERY_H__


#include <sys/socket.h>
#include <netinet/in.h>
#include "GigeVisionDef.h"
#include "windef.h"
#include "BroadcastSocket.h"


#define DEVICE_LIST_MAX_NUM  250


typedef struct MV_GE_DISCOVERY_ACK_{
	gvcp_cmd_head_t head;
	unsigned char spec_version_major[2];
	unsigned char spec_version_minor[2];
	unsigned char device_mode[4];
	unsigned char reserved0[2];
	unsigned char mac_addr[6];
	unsigned char ip_config_options[4];
	unsigned char ip_config_current[4];
	unsigned char reserved1[12];
	unsigned char current_ip[4];
	unsigned char reserved2[12];
	unsigned char current_net_mask[4];
	unsigned char reserved3[12];
	unsigned char default_gateway[4];
	unsigned char manufacturer_name [32];
	unsigned char model_name [32];
	unsigned char device_version [32];
	//unsigned char manufacturer_specific_information[48];
	unsigned char device_pidvid[8];
	unsigned char device_status;
	unsigned char device_linkspeed;
	unsigned char device_ip_alloc_mode;
	unsigned char reserved4[1];
	unsigned char device_friendly_name[36];

	unsigned char serial_number [16];
	unsigned char user_defined_name [16];
}MV_GE_DISCOVERY_ACK;

typedef struct DEVLIST_INFOR_{
    unsigned int list_num;
    unsigned int is_valid[DEVICE_LIST_MAX_NUM];
    unsigned int create_time[DEVICE_LIST_MAX_NUM];

}DEVLIST_INFOR;

class CGevDiscovery
{
public:

    CGevDiscovery(char const* pManuName, UINT iListenAddr, bool bAutoUpdate);
	~CGevDiscovery(void);

private:

    CBroadcastSocket	m_sock;
    CCriticalSection    m_csLock;
    USHORT				m_usCmdId;
    char const*         m_pManuVendor;
	bool				m_bAutoUpdate;
    
    int BroadCastRecv(char *sBuf,int max_len);
	int BroadCastSend(unsigned char *sBuf,int len);

	INT MakeDiscoveryCmd(gvcp_cmd_head_t *buf);

	gvcp_discovery_ack_t m_sDevList[DEVICE_LIST_MAX_NUM];
    DEVLIST_INFOR       m_sDevListInfo;
	
public:

    pthread_t          	m_hThreadSend;
    pthread_t           m_hThreadRev;
    pthread_t           m_hThreadChk;
	BOOL				m_bThreadStop;

    INT SendDiscovery();
    BOOL DiscoveryReceive(gvcp_discovery_ack_t *pDevInfo);
    void DeviceListAdd(gvcp_discovery_ack_t DevInfo);
    void DeviceListChk();

    INT GetDiscoveryList(gvcp_discovery_ack_t *Dev);

};

#endif
