#ifndef MicroInsp_FType_HPP
#define MicroInsp_FType_HPP
#include <SOCK_Msg_Flow.hpp>

class MicroInsp_FType:public SOCK_Msg_Flow
{
    public:
    MicroInsp_FType(char *host,int port) throw(int);
};

#endif