
#include <MicroInsp_FType.hpp>

MicroInsp_FType::MicroInsp_FType(char *host,int port) throw(int):
    SOCK_JSON_Flow(host,port)
{
}
