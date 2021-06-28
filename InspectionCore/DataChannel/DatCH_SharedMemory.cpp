
#include "DatCH_SharedMemory.hpp"

#include <exception>
#include <stdexcept>

#include <unistd.h>
#include <stdio.h> 
#include <string.h> 
#include <thread>
#include <unistd.h>
#include <condition_variable>
// char buff[]="qwer\nasdf\n";

// int mainXX(){
//   int p[2]; _pipe(p);

//   if( !fork() ){
//     for( int buffsize=strlen(buff), len=0; buffsize>len; )
//       len+=_write( p[1], buff+len, buffsize-len );
//     return 0;
//   }

//   _close(p[1]);
//   FILE *f = fdopen( p[0], "r" );
//   char buff[100];
//   while( fgets(buff,100,f) ){
//     printf("from child: '%s'\n", buff );
//   }
//   puts("");
// }

using namespace std;
class DatCH_SharedMemory: public DatCH_Interface
{
protected:
    smem_channel* sendCh;
    smem_channel* recvCh;

    //Just to provide a fd styled recv event
    FILE *tmpFile=NULL;
    int tmpFilefd=-1;

    bool app_recved_flag;
    condition_variable app_recved_cond_var;

    //=========



    thread *recv_Thread;


    
    void _RECV_THREAD()
    {
        while(1)
        {
            recvCh->r_wait();
            uint8_t* data=(uint8_t*)recvCh->getPtr();
            _write(tmpFilefd, " ", 1);
            //the data should have length info to let following code know how long is this data
            
            // while (!ready) {
            //     app_recved_cond_var.wait(lock);
            // }
            recvCh->r_release();
        }
    }
public:         
  
    // void setFdset(fd_set *dst)
    // {
    
    //     FD_SET(sock, dst);
    //     for (int i = 0; i < clientTable.size(); i++)
    //     {
    //         FD_SET(clientTable[i].client_fd, dst);
    //     }
    // }

    
    // int runLoop(fd_set *read_fds,struct timeval *tv)
    // {
    //     return server->runLoop(read_fds,tv);
    // }


    DatCH_SharedMemory(std::string name,size_t max_size){
        tmpFile = tmpfile();
        tmpFilefd = fileno(tmpFile);
        app_recved_flag=false;


        sendCh=new smem_channel("s_"+name,max_size,true);
        recvCh=new smem_channel("r_"+name,max_size,true);
        recv_Thread=new thread(&_RECV_THREAD,this);
//         char dataInMemory[] = "This is some data in memory";
// FILE * fileDescriptor = fmemopen(dataInMemory, sizeof(dataInMemory), "r");
    }

    ~DatCH_SharedMemory()
    {
        if(tmpFile)
        {
            fclose(tmpFile);
            tmpFilefd=-1;
            tmpFile=NULL;
        }
        delete recvCh;

        recv_Thread->join();
        // recv_Thread->stop();
        delete recv_Thread;
    }



};

