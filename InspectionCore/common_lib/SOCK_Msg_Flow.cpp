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

#include <fcntl.h>
#include <assert.h>
#include <stdint.h> /* uint8_t */
#include <stdlib.h> /* strtoul */
#include <unistd.h>
#include <SOCK_Msg_Flow.hpp>


void socket_close(int *sock);

#ifdef __WIN32__
void socket_close(int *sock)
{
    // preserve current error code
    int err = WSAGetLastError();
    closesocket(*sock);
    *sock = INVALID_SOCKET;
    WSASetLastError(err);
}

int connect_nonb(int sock, const struct sockaddr *saptr, int salen, int sec)
{
    //return connect(sock,saptr,salen);
    // you really shouldn't be calling WSAStartup() here.
    // Call it at app startup instead...

    // put socked in non-blocking mode...
    u_long block = 1;
    if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR)
    {
        socket_close(&sock);
        return -1;
    }

    if (connect(sock, saptr, salen) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            // connection failed
            socket_close(&sock);
            return -1;
        }

        // connection pending

        fd_set setW, setE;

        FD_ZERO(&setW);
        FD_SET(sock, &setW);
        FD_ZERO(&setE);
        FD_SET(sock, &setE);

        timeval time_out = {0};
        time_out.tv_sec = sec;
        time_out.tv_usec = 0; 

        int ret = select(0, NULL, &setW, &setE, &time_out);
        if (ret <= 0)
        {
            // select() failed or connection timed out
            socket_close(&sock);
            if (ret == 0)
                WSASetLastError(WSAETIMEDOUT);
            return -1;
        }

        if (FD_ISSET(sock, &setE))
        {
            // connection failed
            char err = 0;
            int size =  sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &err,&size);
            socket_close(&sock);
            WSASetLastError(err);
            return -1;
        }
    }

    // connection successful

    // put socked in blocking mode...
    block = 0;
    if (ioctlsocket(sock, FIONBIO, &block) == SOCKET_ERROR)
    {
        socket_close(&sock);
        return -1;
    }

    return 0;
}

#else

void socket_close(int *sock)
{
  close(*sock);
}

int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec)
{
    int     flags, n, error;
    socklen_t   len;
    fd_set  rset, wset;
    struct timeval  tval;

    if ((flags = fcntl(sockfd, F_GETFL, 0)) == -1) {
        perror("fcntl F_GETFL");
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
    }

    error = 0;
    if ((n = connect(sockfd, saptr, salen)) < 0) {
        if (errno != EINPROGRESS) {
            return -1;
        }
    } else if (n == 0) {
        goto done;
    }

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);
    wset = rset;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;

    if ((n = select(sockfd+1, &rset, &wset, NULL, nsec ? &tval:NULL)) == 0) {
        socket_close(&sockfd);
        errno = ETIMEDOUT;
        return -1;
    } else if (n == -1) {
        socket_close(&sockfd);
        perror("select");
        return -1;
    }

    if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
        len = sizeof(error);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
            return -1;
        }
    } else {
        fprintf(stderr, "select error: socket not set");
    }


done:
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("fcntl");
    }

    if (error) {
        socket_close(&sockfd);
        errno = error;
        return -1;
    }

    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return 0;
}

#endif


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

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
      throw -3;
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
    {
      throw -3;
    }

    their_addr.sin_family = AF_INET;      /* host byte order */
    their_addr.sin_port = htons(port);    /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero),0, 8);     /* zero the rest of the struct */

    printf("c:sockfd:%d\n",sockfd);



    int ret_val;


    if ((ret_val=connect_nonb( sockfd,(struct sockaddr *)&their_addr,sizeof(struct sockaddr), 1))!=0) {
        //perror("connect");
        throw ret_val;
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
  
    sendLock.lock();
    int ret = send(sockfd, (char*)data, len, 0);
    sendLock.unlock();
    return ret;
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

void SOCK_Msg_Flow::DESTROY()
{
  //printf(">close(sockfd);>\n");
  socket_close(&sockfd);
  if(recvThread)
  {
      //printf(">recvThread->join()>\n");
      recvThread->join();
      //printf(">delete recvThread>\n");
      delete recvThread;
      recvThread = NULL;
  }
}

SOCK_Msg_Flow::~SOCK_Msg_Flow()
{
  DESTROY();
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


int SOCK_JSON_Flow::recv_json( char* json_str, int json_strL)
{

    printf("-----%s\n",json_str);
}

int SOCK_JSON_Flow::ev_on_close()
{
  printf("----\n");
  printf("----\n");
  printf("----\n");
  printf("----\n");
  return 1;
}

int SOCK_JSON_Flow::recv_data_thread()
{
    int recvL=0;
    int jsonBuff_w=0;
    printf("recv_data_thread:sockfd:%d",sockfd);
    //send_data((uint8_t*)">>>>>>>>>",8);
    while((recvL=recv(sockfd, (char*)buf, bufL, 0))>0)
    {
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
                recv_json(jsonBuff,jsonBuff_w);
                
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
    int ret =0;
    ret = ev_on_close();
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
  DESTROY();
}