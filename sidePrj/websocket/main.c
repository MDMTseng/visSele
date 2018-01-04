/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ws_server_util.h>

#include "websocket.h"

#define PORT 5000
#define BUF_LEN 0xFFFF
//#define PACKET_DUMP

uint8_t gBuffer[BUF_LEN];
uint8_t goBuffer[BUF_LEN];
ws_conn_entity_pool ws_conn;


void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}


void clientWorker(ws_server* conn)
{
    while(conn->sock) 
    {
        conn->runLoop();
    }
} 

int main(int argc, char** argv)
{
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == -1) {
        error("create socket failed");
    }
    
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(PORT);
    if (bind(listenSocket, (struct sockaddr *) &local, sizeof(local)) == -1) {
        error("bind failed");
    }
    
    if (listen(listenSocket, 1) == -1) {
        error("listen failed");
    }

    fd_set evtSet,read_fds;
    int fdmax;
    FD_ZERO(&evtSet);
    FD_SET(listenSocket, &evtSet);
    fdmax=listenSocket;



    printf("opened %s:%d\n", inet_ntoa(local.sin_addr), ntohs(local.sin_port));
    
    while (TRUE) {

        read_fds = evtSet;


        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
          perror("select");
          exit(4);
        }


        if(FD_ISSET(listenSocket, &read_fds))
        {
            struct sockaddr_in remote;
            socklen_t sockaddrLen = sizeof(remote);
            int NewSock = accept(listenSocket, (struct sockaddr*)&remote, &sockaddrLen);
            if (NewSock == -1) {
                error("accept failed");
            }
            ws_server* conn = ws_conn.find_avaliable_conn_info_slot();
            conn->sock=NewSock;
            conn->addr=remote;
            printf("connected %s:%d\n", 
            inet_ntoa(conn->addr.sin_addr), ntohs(conn->addr.sin_port));


            FD_SET(NewSock, &evtSet); // 新增到 master set
            if (NewSock > fdmax) { // 持續追蹤最大的 fd
              fdmax = NewSock;
            }


            printf("List size %d\n", ws_conn.size());
            
        }
        else
        {
            std::vector <ws_server>* servers = ws_conn.getServers();
            for(int i=0;i<(*servers).size();i++)
            {

                if((*servers)[i].sock && FD_ISSET((*servers)[i].sock, &read_fds))
                {
                    int fd = (*servers)[i].sock;
                    (*servers)[i].runLoop();
                    if((*servers)[i].sock == 0)
                    {
                        printf("List size %d\n", ws_conn.size());
                        FD_CLR(fd, &evtSet);
                    }
                }
            }
        }

    }
    
    close(listenSocket);
    return EXIT_SUCCESS;
}

