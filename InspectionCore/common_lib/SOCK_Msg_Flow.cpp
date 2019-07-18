/*
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
//#include <netdb.h> 
#include <sys/types.h> 
//#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
*/
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 

#include <assert.h>
#include <stdint.h> /* uint8_t */
#include <stdlib.h> /* strtoul */
#include <unistd.h>
#include <SOCK_Msg_Flow.hpp>

SOCK_Msg_Flow::SOCK_Msg_Flow(char *host,int port) throw(int)
{

    if ((he=gethostbyname(host)) == NULL) {  /* get the host info */
        //herror("gethostbyname");
        throw -1;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        //perror("socket");
        throw -1;
    }

    their_addr.sin_family = AF_INET;      /* host byte order */
    their_addr.sin_port = htons(port);    /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero),0, 8);     /* zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&their_addr, \
                                            sizeof(struct sockaddr)) == -1) {
        //perror("connect");
        throw -1;
    }
    
}

int SOCK_Msg_Flow::start_RECV_Thread()
{
    if(!recvThread)
    {
        recvThread = new std::thread(&SOCK_Msg_Flow::recv_data_thread, this);
        return (recvThread==NULL);
    }
    return -1;
}

int SOCK_Msg_Flow::send_data(uint8_t *data,int len)
{
    return send(sockfd, (char*)data, len, 0);
}

int SOCK_Msg_Flow::recv_data()
{
    return recv(sockfd, buf, sizeof(buf), 0);
}

int SOCK_Msg_Flow::getfd()
{
    return sockfd;
}

int SOCK_Msg_Flow::recv_data_thread()
{
    int recvL=0;
    
    printf("sockfd:%d",sockfd);
    send_data((uint8_t*)">>>>>>>>>",8);
    while((recvL=recv(sockfd, buf, sizeof(buf), 0))>0)
    {
        printf("\n%d\n",recvL);
        for(int i=0;i<recvL;i++)
        {
            printf("%c",buf[i]);
        }
    }
    return recvL;
}
SOCK_Msg_Flow::~SOCK_Msg_Flow()
{
    close(sockfd);
    
    if(recvThread)
    {
        recvThread->join();
        delete recvThread;
        recvThread = NULL;
    }
}