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

#include <assert.h>
#include <stdint.h> /* uint8_t */
#include <stdlib.h> /* strtoul */
#include <unistd.h>
#ifdef __WIN32__
#include <winsock2.h>
#else
#include <netinet/in.h> /*htons*/
#endif

#include <MicroInsp_FType.hpp>

MicroInsp_FType::MicroInsp_FType(char *host,int port) throw(int)
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

int MicroInsp_FType::send_data(uint8_t *data,int len)
{
    return send(sockfd, (char*)data, len, 0);
}

int MicroInsp_FType::recv_data()
{
    return recv(sockfd, buf, sizeof(buf), 0);
}

MicroInsp_FType::~MicroInsp_FType()
{
    close(sockfd);
}