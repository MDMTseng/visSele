
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
class DatCH_SharedMemory
{
protected:
    smem_channel* sendCh;
    smem_channel* recvCh;

    uint8_t* hold_data=NULL;
public:         

    DatCH_SharedMemory(std::string name,size_t max_size){
        sendCh=new smem_channel("s_"+name,max_size,true);
        recvCh=new smem_channel("r_"+name,max_size,true);
//         char dataInMemory[] = "This is some data in memory";
// FILE * fileDescriptor = fmemopen(dataInMemory, sizeof(dataInMemory), "r");
    }

    void recv_wait()
    {
      recvCh->r_wait();
      hold_data=(uint8_t*)recvCh->getPtr();
    }

    void recv_release()
    {
      hold_data=NULL;
      recvCh->r_release();
    }


    ~DatCH_SharedMemory()
    {
        delete recvCh;
        delete sendCh;

    }



};

