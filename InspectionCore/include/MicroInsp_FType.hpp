
#include <thread>
class MicroInsp_FType
{
    std::thread *recvThread;
    public:
    int sockfd, numbytes;  
    char buf[100];
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */
    MicroInsp_FType(char *host,int port) throw(int);
    int send_data(uint8_t *data,int len);
    int recv_data();

    int recv_data_thread();
    ~MicroInsp_FType();
};
