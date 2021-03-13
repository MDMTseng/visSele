
#include <Ext_Util_API.hpp>

Ext_Util_API::Ext_Util_API(char *host,int port) throw(int):
    SOCK_JSON_Flow(host,port)
{
}

