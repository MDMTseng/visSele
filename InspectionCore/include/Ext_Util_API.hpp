#ifndef Ext_Util_API_HPP
#define Ext_Util_API_HPP

#include <SOCK_Msg_Flow.hpp>

class Ext_Util_API:public SOCK_Msg_Flow
{
    public:
    Ext_Util_API(char *host,int port) throw(int);

    int recv_data_thread();


    int cmd_cameraCalib(char* img_path, int board_w, int board_h);

    ~Ext_Util_API();
};


#endif