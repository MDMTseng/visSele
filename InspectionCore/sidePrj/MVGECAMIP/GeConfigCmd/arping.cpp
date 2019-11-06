#include <stdio.h>  
#include <stdlib.h>  
#include <unistd.h>   
#include <string.h>  
#include <errno.h>  
#include <time.h>  
  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/ioctl.h>  
                  
#include <netinet/in.h>   
#include <netinet/if_ether.h>  
#include <net/if.h>  
#include <net/if_arp.h>  
#include <arpa/inet.h>    
      
#ifndef __APPLE__

#define MAC_BCAST_ADDR      (unsigned char *) "\xff\xff\xff\xff\xff\xff"  

struct arpMsg {  
    struct ethhdr ethhdr;       /* Ethernet header */  
    u_short htype;              /* hardware type (must be ARPHRD_ETHER) */  
    u_short ptype;              /* protocol type (must be ETH_P_IP) */  
    u_char  hlen;               /* hardware address length (must be 6) */  
    u_char  plen;               /* protocol address length (must be 4) */  
    u_short operation;          /* ARP opcode */  
    u_char  sHaddr[6];          /* sender's hardware address */  
    u_char  sInaddr[4];         /* sender's IP address */  
    u_char  tHaddr[6];          /* target's hardware address */  
    u_char  tInaddr[4];         /* target's IP address */  
    u_char  pad[18];            /* pad for min. Ethernet payload (60 bytes) */  
};  

/*参数说明 目标IP地址，本机IP地址，本机mac地址，网卡名*/  
int arpping(u_int32_t yiaddr, u_int32_t ip, unsigned char *mac, char *interface)  
{  
    int timeout = 1;
    int optval = 1;  
    int s;                      /* socket */  
    int rv = 1;                 /* return value */  
    struct sockaddr addr;       /* for interface name */  
    struct arpMsg arp;  
    fd_set fdset;  
    struct timeval tm;  
    time_t prevTime;  
  
    /*socket发送一个arp包*/  
    if ((s = socket (PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP))) == -1) {  
        return -1;  
    }  
      
    /*设置套接口类型为广播，把这个arp包是广播到这个局域网*/  
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) == -1) {  
        close(s);  
        return -1;  
    }  
  
    /* 对arp设置，这里按照arp包的封装格式赋值即可，详见http://blog.csdn.net/wanxiao009/archive/2010/05/21/5613581.aspx */  
    memset(&arp, 0, sizeof(arp));  
    memcpy(arp.ethhdr.h_dest, MAC_BCAST_ADDR, 6);   /* MAC DA */  
    memcpy(arp.ethhdr.h_source, mac, 6);        /* MAC SA */  
    arp.ethhdr.h_proto = htons(ETH_P_ARP);      /* protocol type (Ethernet) */  
    arp.htype = htons(ARPHRD_ETHER);        /* hardware type */  
    arp.ptype = htons(ETH_P_IP);            /* protocol type (ARP message) */  
    arp.hlen = 6;                   /* hardware address length */  
    arp.plen = 4;                   /* protocol address length */  
    arp.operation = htons(ARPOP_REQUEST);       /* ARP op code */  
    *((u_int *) arp.sInaddr) = ip;          /* source IP address */  
    memcpy(arp.sHaddr, mac, 6);         /* source hardware address */  
    *((u_int *) arp.tInaddr) = yiaddr;      /* target IP address */  
  
    memset(&addr, 0, sizeof(addr));  
    strcpy(addr.sa_data, interface);  
    /*发送arp请求*/  
    if (sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0)  
        rv = 0;  
  
    /* 利用select函数进行多路等待*/  
    tm.tv_usec = 0;  
    time(&prevTime);  
    while (timeout > 0) {  
        FD_ZERO(&fdset);  
        FD_SET(s, &fdset);  
        tm.tv_sec = timeout;  
        if (select(s + 1, &fdset, (fd_set *) NULL, (fd_set *) NULL, &tm) < 0) {  
            //printf("Error on ARPING request: %s\n", strerror(errno));  
            if (errno != EINTR) rv = 0;  
        } else if (FD_ISSET(s, &fdset)) {  
            if (recv(s, &arp, sizeof(arp), 0) < 0 )   
                rv = 0;  
            /*如果条件 htons(ARPOP_REPLY) bcmp(arp.tHaddr, mac, 6) == 0 *((u_int *) arp.sInaddr) == yiaddr 三者都为真，则ARP应答有效,说明这个地址是已近存在的*/  
            if (arp.operation == htons(ARPOP_REPLY) &&  
                bcmp(arp.tHaddr, mac, 6) == 0 &&  
                *((u_int *) arp.sInaddr) == yiaddr) {  
                //printf("Valid arp reply receved for this address\n");  
                rv = 0;  
                break;  
            }  
        }  
        timeout -= time(NULL) - prevTime;  
        time(&prevTime);  
    }  
    close(s);  
    return rv;  
}  
  
#else

int arpping(u_int32_t yiaddr, u_int32_t ip, unsigned char *mac, char *interface)  
{
    return -1;
}

#endif

