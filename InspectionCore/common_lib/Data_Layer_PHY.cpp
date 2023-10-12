
#include "Data_Layer_PHY.hpp"

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




static void socket_close(int *sock);

#ifdef __WIN32__
static void socket_close(int *sock)
{
    // preserve current error code
    int err = WSAGetLastError();
    closesocket(*sock);
    *sock = INVALID_SOCKET;
    WSASetLastError(err);
}

static int connect_nonb(int sock, const struct sockaddr *saptr, int salen, int sec)
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

static void socket_close(int *sock)
{
  close(*sock);
}

static int connect_nonb(int sockfd, const struct sockaddr *saptr, socklen_t salen, int nsec)
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


Data_TCP_Layer::Data_TCP_Layer(const char *host,int port)//throw(std::runtime_error)
{
    if ((he=gethostbyname(host)) == NULL) {  /* get the host info */
        //herror("gethostbyname");
        throw std::runtime_error("-1");
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        //perror("socket");
        throw std::runtime_error("-2");
    }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int)) < 0)
    {
      throw std::runtime_error("-3");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char*)&enable, sizeof(int)) < 0)
    {
      throw std::runtime_error("-4");
    }
#endif

    their_addr.sin_family = AF_INET;      /* host byte order */
    their_addr.sin_port = htons(port);    /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero),0, 8);     /* zero the rest of the struct */

    printf("c:sockfd:%d\n",sockfd);



    int ret_val;
    char EXP_INFO[100];

    if ((ret_val=connect_nonb( sockfd,(struct sockaddr *)&their_addr,sizeof(struct sockaddr), 1))!=0) {
      //perror("connect");
      sprintf(EXP_INFO,"ERROR ret:%d",ret_val);
      throw std::runtime_error(EXP_INFO);
    }
    // recvThread=NULL;
    isConnected=true;
    recvThread = new std::thread(&Data_TCP_Layer::recv_data_thread, this);
    // return (recvThread==NULL);

}

int Data_TCP_Layer::recv_data_thread()
{
    int recvL=0;
    
    uint8_t buffer[200];
    // printf(">th:sockfd:%d\n",sockfd);
    // send_data(0,(uint8_t*)">>>>>>>>>",8,0);
    while((recvL=recv(sockfd, (char*)buffer, sizeof(buffer), 0))>0)
    {
      recv_data(buffer,recvL,false);
    }
    // close();
    disconnected(this);
    printf(">recvL:%d\n",recvL);
    return recvL;
}

int Data_TCP_Layer::send_data(int head_room,uint8_t *data,int len,int leg_room)
{

    sendLock.lock();
    int ret = send(sockfd, (char*)data, len, 0);
    sendLock.unlock();
    return ret; 
}

int Data_TCP_Layer::close()
{

  printf(" TCP CLOSE!!!!\n");
  if(sockfd!=-1)
  {
    int tmp_fd=sockfd;
    sockfd=-1;
    socket_close(&tmp_fd);
  }
  if(recvThread)
  {
      //printf(">recvThread->join()>\n");
      recvThread->join();
      //printf(">delete recvThread>\n");
      delete recvThread;
      recvThread = NULL;
  }
  // Data_Layer_IF::close();
  return 0;
}
Data_TCP_Layer::~Data_TCP_Layer()
{
  printf(" TCP DESTRUCT!!!! \n");
  close();
  printf(" TCP DESTRUCT!!!! end \n");
}





////////////////////////////////////////
Data_UART_Layer::Data_UART_Layer(const char *name,int speed,const char *mode_string)//throw(std::runtime_error)
{

  printf("name:%s sped:%d mode_string:%s\n",name,speed,mode_string);
  uart = simple_uart_open(name, speed, mode_string);
  
  if( uart == NULL) {  /* get the host info */
      //herror("gethostbyname");
      throw std::runtime_error("-1");
  }


  uint8_t buffer[100];
  for(int k=0;;k++)//try to deplete buffer
  {
    int datLen = simple_uart_read_timed(uart, buffer, sizeof(buffer),30);
    if(datLen)
    {
      printf("WARN:: UART:%s channel still has residue data length:%d \n",name,datLen);

      printf("Text print blow\n");
      for(int i=0;i<datLen;i++)
      {
        printf("%c",buffer[i]);
      }
      printf("\n");
      continue;
    }
    break;
  }

  
  // recvThread=NULL;
  recvThread = new std::thread(&Data_UART_Layer::recv_data_thread, this);
  // return (recvThread==NULL);

}

int Data_UART_Layer::recv_data_thread()
{
  uint8_t buffer[100];
  for(int k=0;uart!=NULL;k++)
  {

    int datLen = simple_uart_read_timed(uart, buffer, sizeof(buffer),1000);

    if(datLen==0)continue;
    // printf(">>>>simple_uart datLen:%d\n",datLen);
    // for(int i=0;i<datLen;i++)
    // {
    //     printf("%c",buffer[i]);
    // }
    // printf("\n");


    if(datLen<0)break;
    recv_data(buffer, datLen,false);
  }
  
  disconnected(this);

  return 0;
}

int Data_UART_Layer::send_data(int head_room,uint8_t *data,int len,int leg_room)
{
    // printf(">>>>len%d\n",len);
    sendLock.lock();
    int ret = simple_uart_write(uart, (char*)data, len);
    sendLock.unlock();
    return ret; 
}

int Data_UART_Layer::close()
{
  printf("uart close\n");
  if(uart!=NULL)
  {
    simple_uart_close(uart);
  }
  uart=NULL;

  if(recvThread)
  {
    //printf(">recvThread->join()>\n");
    recvThread->join();
    //printf(">delete recvThread>\n");
    delete recvThread;
    recvThread = NULL;
  }
  printf("Data_Layer_IF::close\n");
  Data_Layer_IF::close();
  return 0;
}
Data_UART_Layer::~Data_UART_Layer()
{
  close();
  printf(" UART DESTRUCT!!!! \n");
  // close();
}





