#include <string>
#include <stdio.h>
#include <assert.h>
#include <memory>
#include <ifaddrs.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/ioctl.h> 
#include <arpa/inet.h>
#include <netinet/in.h>   
#include <netinet/if_ether.h>  
#include <net/if.h>  
#include <net/if_arp.h>  
#include <arpa/inet.h>  

#include "log.h"
#include "GevForceIp.h"
#include "GevDiscovery.h"
using namespace std;

#define MAX_ETH_NUM 32 //最多支持32块卡

typedef struct MV_GE_INTERFACE_INF_{
	char desc[256];
	unsigned char mac[6];
	unsigned char ip[4];
	unsigned char net_mask[4];
	unsigned char gate_way[4];
}MV_GE_INTERFACE_INF;

typedef struct MV_GE_DEV_INF_
{
	char ip[64];
	char net_mask[64];
	char gateway[64];
	char mac[64];
	char friendly_name[64];
	char model_name[64];
	char serial_number[64];
	char linkspeed[64];
	char access[64];
}MV_GE_DEV_INF;

class CIpConfig
{
public:

	CIpConfig();
	~CIpConfig();

	bool Init();

	int RefreshDevList();

	bool GetDevInfo(int iIndex, MV_GE_DEV_INF* dev_inf);
	int GetDevNum() const { return m_iDevNum; }

	bool IsDevIPValid(int iIndex);
	bool IsDevIDLE(int iIndex);

	bool SetIpAddr(int iIndex, char const* strIp, char const* strMask, char const* strGateway);
	bool AutoSetIpAddr(int iIndex);

private:

	int DeviceEnumeInit();
	int GetDeviceList(MV_GE_DISCOVERY_ACK *sList);

private:

	CGevDiscovery		*m_pCGevDiscovery[MAX_ETH_NUM];
	CGevForceIp			*m_pCForceIp;

	MV_GE_DISCOVERY_ACK m_sDevList[DEVICE_LIST_MAX_NUM*MAX_ETH_NUM];
	UINT                m_iDevInterface[DEVICE_LIST_MAX_NUM*MAX_ETH_NUM];
	int					m_iDevNum;

	MV_GE_INTERFACE_INF m_sInterfaceInfo[MAX_ETH_NUM];
	int                	m_iInterfaceNum;
};

CIpConfig::CIpConfig()
{
	memset(m_pCGevDiscovery, 0, sizeof(m_pCGevDiscovery));
	m_pCForceIp = NULL;
	memset(m_sDevList, 0, sizeof(m_sDevList));
	memset(m_iDevInterface, 0, sizeof(m_iDevInterface));
	m_iDevNum = 0;
	memset(&m_sInterfaceInfo, 0, sizeof(m_sInterfaceInfo));
	m_iInterfaceNum = 0;
}

CIpConfig::~CIpConfig()
{
	for(int i=0;i<MAX_ETH_NUM;i++)
	{
		if(m_pCGevDiscovery[i])
		{
			delete m_pCGevDiscovery[i];
			m_pCGevDiscovery[i] = NULL;
		}
	}

	delete m_pCForceIp;
	m_pCForceIp = NULL;
}

bool CIpConfig::Init()
{
	return DeviceEnumeInit() > 0;
}

static BOOL GetIpFromString(char const* strIp, BYTE ip[4])
{
	int tmp[4] = { -1, -1, -1, -1 };
	if (sscanf(strIp, "%d.%d.%d.%d", &tmp[0], &tmp[1], &tmp[2], &tmp[3]) != 4)
		return FALSE;

	for (int i = 0; i < 4; ++i)
	{
		if (tmp[i] < 0 || tmp[i] > 255)
			return FALSE;
	}
	for (int i = 0; i < 4; ++i)
	{
		ip[i] = tmp[i];
	}
	return TRUE;
}

bool CIpConfig::SetIpAddr(int iSelect, char const* strIp, char const* strMask, char const* strGateway)
{
	BYTE Ip[4],NetMask[4],GateWay[4];
	BYTE DeviceMac[6];

    if(iSelect<0)
	{
		return false;
	}

	if (!GetIpFromString(strIp, Ip) )
	{
		// MessageBox("Please enter valid ip");
		return false;
	}

	if (!GetIpFromString(strMask, NetMask) )
	{
		//MessageBox("Please enter valid mask");
		return false;
	}

	if (!GetIpFromString(strGateway, GateWay) )
	{
		//MessageBox("Please enter valid gateway");
		return false;
	}
	
    //先清除IP设置端口
    if(m_pCForceIp)
    {
        delete m_pCForceIp;
        m_pCForceIp = NULL;
    }

    //创建IP设置端口
    {
        UINT i = m_iDevInterface[iSelect];
        UINT uIpAddr = m_sInterfaceInfo[i].ip[0] << 24 |  m_sInterfaceInfo[i].ip[1] << 16 |  m_sInterfaceInfo[i].ip[2] << 8 |  m_sInterfaceInfo[i].ip[3];
        m_pCForceIp = new CGevForceIp(uIpAddr); 
    }

	memcpy(DeviceMac,m_sDevList[iSelect].mac_addr,6);
	BOOL bSaveip = TRUE;
	BOOL bAllocAuto = FALSE;
	if(m_pCForceIp->GevSetForceIpCustom(DeviceMac,Ip,NetMask,GateWay,bAllocAuto,bSaveip) )
	{
        //MessageBox("IP set successfully!");
		return true;
	}
	else
	{
		//MessageBox("Failed to set the IP of the camera! Check whether the camera is already opened by another application, or you can reset the power of the camera ,and try again !");
		return false;
	}
}

static uint32_t RandomIP(uint32_t HostIP, uint32_t HostIPMask)
{
	for (int Try = 0; Try < 100; ++Try)
	{
		uint32_t NewIP = 0;
		for (int i = 0; i < 4; ++i)
		{
			uint32_t b = (rand() % 245 + 10);
			NewIP |= (b << (i * 8));
		}
		NewIP = (NewIP & (~HostIPMask)) | (HostIP & HostIPMask);
		if (NewIP != HostIP)
			return NewIP;
	}
	return 0;
}

extern int arpping(u_int32_t yiaddr, u_int32_t ip, unsigned char *mac, char *interface);

bool CIpConfig::AutoSetIpAddr(int iSelect)
{
	BYTE DeviceMac[6];
	uint32_t interface_ip;
	uint32_t interface_mask;
	uint32_t interface_gateway = 0;
	unsigned char *interface_mac;
	char *interface_name;

    if (iSelect<0)
	{
		return false;
	}

	UINT i = m_iDevInterface[iSelect];
	interface_ip = m_sInterfaceInfo[i].ip[0] << 0 |  m_sInterfaceInfo[i].ip[1] << 8 |  m_sInterfaceInfo[i].ip[2] << 16 |  m_sInterfaceInfo[i].ip[3] << 24;
	interface_mask = m_sInterfaceInfo[i].net_mask[0] << 0 |  m_sInterfaceInfo[i].net_mask[1] << 8 |  m_sInterfaceInfo[i].net_mask[2] << 16 |  m_sInterfaceInfo[i].net_mask[3] << 24;
	interface_mac = m_sInterfaceInfo[i].mac;
	interface_name = m_sInterfaceInfo[i].desc;

	uint32_t newIP = 0;
	srand(GetTickCount());
	for (int i = 0; i < 5; ++i)
	{
		newIP = RandomIP(interface_ip, interface_mask);
		if (newIP == 0)
			continue;

		int r = arpping(newIP, interface_ip, interface_mac, interface_name);
		if (r == 0)
			continue;

		break;
	}
	
    //先清除IP设置端口
    if(m_pCForceIp)
    {
        delete m_pCForceIp;
        m_pCForceIp = NULL;
    }

    //创建IP设置端口
    m_pCForceIp = new CGevForceIp(htonl(interface_ip));

	memcpy(DeviceMac,m_sDevList[iSelect].mac_addr,6);
	BOOL bSaveip = TRUE;
	BOOL bAllocAuto = FALSE;
	if(m_pCForceIp->GevSetForceIpCustom(DeviceMac,(BYTE*)&newIP,(BYTE*)&interface_mask,(BYTE*)&interface_gateway,bAllocAuto,bSaveip) )
	{
        //MessageBox("IP set successfully!");
		return true;
	}
	else
	{
		//MessageBox("Failed to set the IP of the camera! Check whether the camera is already opened by another application, or you can reset the power of the camera ,and try again !");
		return false;
	}
}

int CIpConfig::GetDeviceList(MV_GE_DISCOVERY_ACK *sList)
{
	INT iNum[MAX_ETH_NUM];
	INT iNumAll = 0;

	//发送枚举包
	for(int i=0;i<m_iInterfaceNum;i++) //遍历每个接口
	{
		if (m_pCGevDiscovery[i] != NULL)
		{
			m_pCGevDiscovery[i]->SendDiscovery();
		}
	}

	// 等待设备返回枚举响应包
	Sleep(200);

	// 读取枚举结果
	for(int i=0;i<m_iInterfaceNum;i++) //遍历每个接口
	{
		iNum[i] = 0;
		//获取设备信息
		if(m_pCGevDiscovery[i] != NULL)
		{
			iNum[i] = m_pCGevDiscovery[i]->GetDiscoveryList((gvcp_discovery_ack_t*)&sList[iNumAll]); 
			for(int j=0;j<iNum[i];j++) //记录网卡号
			{
				m_iDevInterface[iNumAll+j] = i;
			}
			iNumAll += iNum[i];
		}
	}

	return iNumAll;
}

int CIpConfig::RefreshDevList()
{
	INT iUpdateNeed = 0;
	INT iNum;
	BYTE *byBuf = (BYTE*)malloc(MAX_ETH_NUM*DEVICE_LIST_MAX_NUM*sizeof(MV_GE_DISCOVERY_ACK));
	MV_GE_DISCOVERY_ACK *sDevList = (MV_GE_DISCOVERY_ACK *)byBuf;

   // MV_GE_DISCOVERY_ACK sDevList[DEVICE_LIST_MAX_NUM];

	iNum = GetDeviceList(sDevList); //获取设备列表

	//拷贝数据
	m_iDevNum = iNum;
	memcpy(&m_sDevList,sDevList,m_iDevNum*sizeof(gvcp_discovery_ack_t));

	free(byBuf);
	return m_iDevNum;
}

bool CIpConfig::GetDevInfo(int iIndex, MV_GE_DEV_INF* dev_inf)
{
	if (iIndex < 0 || iIndex >= m_iDevNum)
		return false;

	MV_GE_DISCOVERY_ACK *pDev = &m_sDevList[iIndex];
	
	//写上结束符
	pDev->device_friendly_name[sizeof(pDev->device_friendly_name)-1] = 0; 
	pDev->model_name[sizeof(pDev->model_name)-1] = 0; 
	pDev->device_version[sizeof(pDev->device_version)-1] = 0; 
	pDev->serial_number[sizeof(pDev->serial_number)-1] = 0; 

	memset(dev_inf, 0, sizeof(*dev_inf));

	strcpy(dev_inf->friendly_name, (char*)pDev->device_friendly_name);
	strcpy(dev_inf->model_name, (char*)pDev->model_name);
	strcpy(dev_inf->serial_number, (char*)pDev->serial_number);
	
	sprintf(dev_inf->mac, "%02X-%02X-%02X-%02X-%02X-%02X",pDev->mac_addr[0],pDev->mac_addr[1],pDev->mac_addr[2],pDev->mac_addr[3],pDev->mac_addr[4],pDev->mac_addr[5]);
	sprintf(dev_inf->ip, "%d.%d.%d.%d",pDev->current_ip[0],pDev->current_ip[1],pDev->current_ip[2],pDev->current_ip[3]);

	sprintf(dev_inf->net_mask, "%d.%d.%d.%d",pDev->current_net_mask[0],pDev->current_net_mask[1],pDev->current_net_mask[2],pDev->current_net_mask[3]);

	sprintf(dev_inf->gateway, "%d.%d.%d.%d",pDev->default_gateway[0],pDev->default_gateway[1],pDev->default_gateway[2],pDev->default_gateway[3]);

	strcpy(dev_inf->linkspeed, pDev->device_linkspeed == 'C' ? "1000M" : pDev->device_linkspeed == 'B' ? "100M" : "10M");
	strcpy(dev_inf->access, pDev->device_status == 'I' ? "IDLE" : "BUSY");
	return true;
}

bool CIpConfig::IsDevIPValid(int iIndex)
{
	if (iIndex < 0 || iIndex >= m_iDevNum)
		return false;

	uint32_t interface_ip;
	uint32_t interface_mask;

	UINT i = m_iDevInterface[iIndex];
	interface_ip = m_sInterfaceInfo[i].ip[0] << 0 |  m_sInterfaceInfo[i].ip[1] << 8 |  m_sInterfaceInfo[i].ip[2] << 16 |  m_sInterfaceInfo[i].ip[3] << 24;
	interface_mask = m_sInterfaceInfo[i].net_mask[0] << 0 |  m_sInterfaceInfo[i].net_mask[1] << 8 |  m_sInterfaceInfo[i].net_mask[2] << 16 |  m_sInterfaceInfo[i].net_mask[3] << 24;

	MV_GE_DISCOVERY_ACK *pDev = &m_sDevList[iIndex];
	uint32_t dev_ip = *(uint32_t*)pDev->current_ip;
	uint32_t dev_mask = *(uint32_t*)pDev->current_net_mask;

	if (interface_ip == dev_ip)
		return false;
	if ((interface_ip & interface_mask) != (dev_ip & dev_mask))
		return false;

	return true;
}

bool CIpConfig::IsDevIDLE(int iIndex)
{
	if (iIndex < 0 || iIndex >= m_iDevNum)
		return false;

	MV_GE_DISCOVERY_ACK *pDev = &m_sDevList[iIndex];
	return pDev->device_status == 'I';
}

#ifdef SIOCGIFHWADDR

static int get_interface_mac(unsigned char mac[6], char const* interface_name)
{  
    int                 sockfd;
    struct sockaddr_in  sin;
    struct ifreq        ifr;
	int					r = -1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);  
    if (sockfd < 0) {  
        return -1;
    }  
      
    strncpy(ifr.ifr_name, interface_name, IFNAMSIZ);      //Interface name

    if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) == 0) {  //SIOCGIFHWADDR 获取hardware address  
        memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
		r = 0;
    }  
     
    close(sockfd);
    return r;
}

#else

#include <net/if_dl.h>

static int get_interface_mac(unsigned char mac[6], char const* interface_name)
{
    ifaddrs* iflist;
    bool found = false;
    if (getifaddrs(&iflist) == 0) {
        for (ifaddrs* cur = iflist; cur; cur = cur->ifa_next) {
            if ((cur->ifa_addr->sa_family == AF_LINK) &&
                    (strcmp(cur->ifa_name, interface_name) == 0) &&
                    cur->ifa_addr) {
                sockaddr_dl* sdl = (sockaddr_dl*)cur->ifa_addr;
                memcpy(mac, LLADDR(sdl), sdl->sdl_alen);
                found = true;
                break;
            }
        }

        freeifaddrs(iflist);
    }
    return found ? 0 : -1;
}

#endif

int CIpConfig::DeviceEnumeInit()
{
	int i;
	m_iInterfaceNum = 0;

	for(i=0;i<MAX_ETH_NUM;i++)
	{
		if(m_pCGevDiscovery[i])
		{
			delete m_pCGevDiscovery[i];
			m_pCGevDiscovery[i] = NULL;
		}
	}

	i = 0;
	UINT uIp[4];
	struct sockaddr_in *sin = NULL;
	struct ifaddrs *ifa = NULL, *ifList;
	if (getifaddrs(&ifList) < 0) return FALSE;
	for (ifa = ifList; ifa != NULL; ifa = ifa->ifa_next)
	{
		if(ifa->ifa_addr == NULL
			|| ifa->ifa_addr->sa_family != AF_INET)
			continue;
		
		memset(&m_sInterfaceInfo[i], 0, sizeof(m_sInterfaceInfo[i]));
		strcpy(m_sInterfaceInfo[i].desc, ifa->ifa_name);

		get_interface_mac(m_sInterfaceInfo[i].mac, ifa->ifa_name);

		sin = (struct sockaddr_in *)ifa->ifa_addr;
		sscanf(inet_ntoa(sin->sin_addr), "%u.%u.%u.%u", &uIp[0],&uIp[1],&uIp[2],&uIp[3]);
		m_sInterfaceInfo[i].ip[0] = uIp[0];
		m_sInterfaceInfo[i].ip[1] = uIp[1];
		m_sInterfaceInfo[i].ip[2] = uIp[2];
		m_sInterfaceInfo[i].ip[3] = uIp[3];

		sin = (struct sockaddr_in *)ifa->ifa_netmask;
		sscanf(inet_ntoa(sin->sin_addr), "%u.%u.%u.%u", &uIp[0],&uIp[1],&uIp[2],&uIp[3]);
		m_sInterfaceInfo[i].net_mask[0] = uIp[0];
		m_sInterfaceInfo[i].net_mask[1] = uIp[1];
		m_sInterfaceInfo[i].net_mask[2] = uIp[2];
		m_sInterfaceInfo[i].net_mask[3] = uIp[3];

		// invalid IP不添加进数组
		if (m_sInterfaceInfo[i].ip[0] == 0
			|| m_sInterfaceInfo[i].ip[0] == 255
			|| (m_sInterfaceInfo[i].ip[0] == 127 && m_sInterfaceInfo[i].ip[1] == 0 && m_sInterfaceInfo[i].ip[2] == 0 && m_sInterfaceInfo[i].ip[3] == 1)
			|| (m_sInterfaceInfo[i].ip[0] == 224 && m_sInterfaceInfo[i].ip[1] == 0 && m_sInterfaceInfo[i].ip[2] == 0 && m_sInterfaceInfo[i].ip[3] == 1)
		)
		{
			continue;
		}

        LOG("%s, %u.%u.%u.%u, %u.%u.%u.%u, %x-%x-%x-%x-%x-%x\n",
                m_sInterfaceInfo[i].desc,
                m_sInterfaceInfo[i].ip[0],
                m_sInterfaceInfo[i].ip[1],
                m_sInterfaceInfo[i].ip[2],
                m_sInterfaceInfo[i].ip[3],
                m_sInterfaceInfo[i].net_mask[0],
                m_sInterfaceInfo[i].net_mask[1],
                m_sInterfaceInfo[i].net_mask[2],
                m_sInterfaceInfo[i].net_mask[3],
                m_sInterfaceInfo[i].mac[0],
                m_sInterfaceInfo[i].mac[1],
                m_sInterfaceInfo[i].mac[2],
                m_sInterfaceInfo[i].mac[3],
                m_sInterfaceInfo[i].mac[4],
                m_sInterfaceInfo[i].mac[5]);

		if(++i >= MAX_ETH_NUM)
			break;
	}
	freeifaddrs(ifList);
	m_iInterfaceNum = i;

	for(i=0;i<m_iInterfaceNum;i++)
	{
		if (!m_pCGevDiscovery[i])
		{
			UINT uIpAddr = m_sInterfaceInfo[i].ip[0] << 24 |  m_sInterfaceInfo[i].ip[1] << 16 |  m_sInterfaceInfo[i].ip[2] << 8 |  m_sInterfaceInfo[i].ip[3];
			m_pCGevDiscovery[i] = new CGevDiscovery("F622", uIpAddr, false);
		}
	}

	return m_iInterfaceNum;
}

int RepairAllCameraIP()
{
	std::auto_ptr<CIpConfig> ip_config(new CIpConfig() );

	if (!ip_config->Init() )
	{
		return -1;
	}

	if (ip_config->RefreshDevList() < 1)
	{
		return 0;
	}

	int succ_count = 0;
	for (int i = 0; i < ip_config->GetDevNum(); ++i)
	{
		if (ip_config->IsDevIDLE(i))
		{
			if (ip_config->AutoSetIpAddr(i) )
			{
				++succ_count;
			}
		}
	}
	return succ_count;
}

void printCameraList(CIpConfig* ip_config)
{
	printf("%d cameras found.\n", ip_config->RefreshDevList() );
	for (int i = 0; i < ip_config->GetDevNum(); ++i)
	{
		MV_GE_DEV_INF Dev = { 0 };
		ip_config->GetDevInfo(i, &Dev);

		printf("\nDevName: %s\n", Dev.friendly_name);
		printf("ModelName: %s\n", Dev.model_name);
		printf("ip: %s\n", Dev.ip);
		printf("net_mask: %s\n", Dev.net_mask);
		printf("gateway: %s\n", Dev.gateway);
		printf("mac: %s\n", Dev.mac);
		printf("serial_number: %s\n", Dev.serial_number);
		printf("linkspeed: %s\n", Dev.linkspeed);
		printf("access: %s\n", Dev.access);
		printf("IP Valid: %s\n", ip_config->IsDevIPValid(i) ? "true" : "false");
	}
}

int main(int argc, char **argv)
{
	std::auto_ptr<CIpConfig> ip_config(new CIpConfig() );

	if (!ip_config->Init() )
	{
		printf("No network card found!\n");
		return -1;
	}

	printCameraList(ip_config.get());

	for (;;)
	{
		char buf[32];

		printf("p(Print Camera List) r(Repair All Cameras) q(Quit) Input:");
		fgets(buf, sizeof(buf), stdin);

		int ch = buf[0];
		if (ch == 'p')
		{
			printCameraList(ip_config.get());
		}
		else if (ch == 'r')
		{
			int count = RepairAllCameraIP();
			if (count >= 0)
				printf("Repair %d Cameras\n", count);
			else
				printf("Repair Failed!!!\n");
		}
		else if (ch == 'q')
		{
			break;
		}
	}
	
	return 0;
}
