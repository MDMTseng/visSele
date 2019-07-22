
#include <Ext_Util_API.hpp>

Ext_Util_API::Ext_Util_API(char *host,int port) throw(int):
    SOCK_Msg_Flow(host,port),jsp()
{
}


int Ext_Util_API::recv_data_thread()
{
    int recvL=0;
    int jsonBuff_w=0;
    printf("sockfd:%d",sockfd);
    //send_data((uint8_t*)">>>>>>>>>",8);
    while((recvL=recv(sockfd, (char*)buf, bufL, 0))>0)
    {
        //printf("\n%d\n",recvL);
        for(int i=0;i<recvL;i++)
        {
            int ret_val = jsp.newChar(buf[i]);


            if(ret_val==1)//start
            {
                errorLock=0;
                jsonBuff_w=0;
                jsonBuff[jsonBuff_w++]=buf[i];
            }
            else if(ret_val==-1)
            {
                
                jsonBuff[jsonBuff_w++]=buf[i];
                if(errorLock)
                {
                    jsonBuff_w=0;
                    jsonBuff[jsonBuff_w]='\0';
                }
                else
                    jsonBuff[jsonBuff_w++]='\0';
                //printf("%s\n",jsonBuff);
                
                syncLock.unlock();
            }
            else if(!errorLock)
            {
                jsonBuff[jsonBuff_w++]=buf[i];
                if( jsonBuff_w >= (sizeof(jsonBuff)-1))
                {
                    errorLock=1;
                }
            }
            
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

char* Ext_Util_API::SYNC_cmd_cameraCalib(char* img_path, int board_w, int board_h)
{

    syncLock.lock();
    int ret_val = cmd_cameraCalib(img_path,  board_w,  board_h);
    
    using Ms = std::chrono::milliseconds;
    if(syncLock.try_lock_for(Ms(3000)))//Lock and wait 100 ms
    {
      //Still locked
        printf("errorLock:%d",errorLock);
        if(errorLock)
            return NULL;
        else
            return jsonBuff;
    }
    return NULL;


}
Ext_Util_API::~Ext_Util_API()
{
    
}