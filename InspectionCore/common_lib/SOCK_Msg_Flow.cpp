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
#ifdef __WIN32__
#else
#include <netdb.h> 
#endif

#include <assert.h>
#include <stdint.h> /* uint8_t */
#include <stdlib.h> /* strtoul */
#include <unistd.h>
#include <SOCK_Msg_Flow.hpp>

SOCK_Msg_Flow::SOCK_Msg_Flow(char *host,int port) throw(int)
{

    sockfd=-1;
    this->bufL=100;
    this->buf=new uint8_t[this->bufL];
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

    printf("c:sockfd:%d\n",sockfd);
    if (connect(sockfd, (struct sockaddr *)&their_addr, \
                                            sizeof(struct sockaddr)) == -1) {
        //perror("connect");
        throw -1;
    }
    recvThread=NULL;
    
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

int SOCK_Msg_Flow::buffLength(int length)
{
    if(buf)
    {
        delete buf;
        buf=NULL;
        bufL=0;
    }
    buf=new uint8_t[length];
    bufL=length;

    return length;
}

int SOCK_Msg_Flow::send_data(uint8_t *data,int len)
{
    return send(sockfd, (char*)data, len, 0);
}

int SOCK_Msg_Flow::recv_data()
{
    return recv(sockfd, (char*)buf, bufL, 0);
}

int SOCK_Msg_Flow::getfd()
{
    return sockfd;
}

int SOCK_Msg_Flow::recv_data_thread()
{
    int recvL=0;
    
    printf("th:sockfd:%d\n",sockfd);
    send_data((uint8_t*)">>>>>>>>>",8);
    while((recvL=recv(sockfd, (char*)buf, bufL, 0))>0)
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




json_seg_parser::json_seg_parser(){
    reset();
}
void json_seg_parser::reset()
{
    pch='\0';
    jsonCurlyB_C=0;
    jsonInStrState=0;
    jsonSquareB_C=0;
}
int json_seg_parser::newChar(char ch){
    int ret_val=0;
    if(ch=='"' && pch!='\\')
    {
        jsonInStrState=!jsonInStrState;
    }

    if( (ch=='{' && pch=='}')||(ch=='[' && pch==']') )//End reset
    {
        reset();
        ret_val=0;
    }

    if(!jsonInStrState)
    {
        if( (ch=='{' || ch=='[') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //start point
            ret_val=1;
        }
        switch(ch)
        {
            case '{':
                jsonCurlyB_C++;
            break;
            case '[':
                jsonSquareB_C++;
            break;
            case '}':
                jsonCurlyB_C--;
            break;
            case ']':
                jsonSquareB_C--;
            break;
        }
        
        if( (ch=='}' || ch==']') && jsonCurlyB_C==0 && jsonSquareB_C==0 )
        {
            //end point
            ret_val=-1;
        }
        if(jsonCurlyB_C<0 || jsonSquareB_C<0 )//Error reset
        {
            reset();
            ret_val=0;
        }
    }
    pch=ch;
    return ret_val;
}





SOCK_JSON_Flow::SOCK_JSON_Flow(char *host,int port) throw(int):
    SOCK_Msg_Flow(host,port),jsp()
{
}


int SOCK_JSON_Flow::recv_data_thread()
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
                printf("-----%s\n",jsonBuff);
                
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
int SOCK_JSON_Flow::cmd_cameraCalib(char* img_path, int board_w, int board_h)
{
    char bufStr[100];
    int len = sprintf(bufStr,
        "{\"type\":\"cameraCalib\","
        "\"img_path\":\"%s\","
        "\"board_dim\":[%d,%d]}",img_path,board_w,board_h);
    return send_data((uint8_t*)bufStr,len);
}

char* SOCK_JSON_Flow::SYNC_cmd_cameraCalib(char* img_path, int board_w, int board_h)
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
SOCK_JSON_Flow::~SOCK_JSON_Flow()
{
    
}