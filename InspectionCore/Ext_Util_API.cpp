
#include <Ext_Util_API.hpp>

Ext_Util_API::Ext_Util_API(char *host,int port) throw(int): SOCK_Msg_Flow(host,port)
{
}


int Ext_Util_API::recv_data_thread()
{
    int recvL=0;
    
    printf("sockfd:%d",sockfd);
    //send_data((uint8_t*)">>>>>>>>>",8);
    while((recvL=recv(sockfd, buf, sizeof(buf), 0))>0)
    {
        printf("\n%d\n",recvL);
        for(int i=0;i<recvL;i++)
        {
            printf("%c",buf[i]);
        }
    }
    printf("END:%d",sockfd);
    return recvL;
}

//{"pgID":12442,"img_path":"*.jpg","board_dim":[7,9]}
int Ext_Util_API::cmd_cameraCalib(char* img_path, int board_w, int board_h)
{
    char bufStr[100];
    int len = sprintf(bufStr,
        "{\"type\":\"cameraCalib\","
        "\"img_path\":\"%s\","
        "\"board_dim\":[%d,%d]}",img_path,board_w,board_h);
    return send_data((uint8_t*)bufStr,len);
}


Ext_Util_API::~Ext_Util_API()
{
    
}