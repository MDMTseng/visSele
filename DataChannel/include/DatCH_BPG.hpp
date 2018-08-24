#ifndef DATCH_BPG_HPP
#define DATCH_BPG_HPP

#include <DatCH.hpp>
#include <string>

#include "DatCH_WebSocket.hpp"

typedef struct BPG_data
{
    char type[2];//Two letter
    uint8_t prop;
    uint32_t size;//32bit
    uint8_t*     raw;
};




class DatCH_BPG: public DatCH_Interface
{
protected:
    std::string version;
    BPG_data data;
public:
    DatCH_BPG(){
      version="NULL";
      //data.type = DatCH_Data::DataType_BP;
    }
    ~DatCH_BPG()
    {
    }
    //DatCH_Data SendData(DatCH_Data) override;
    virtual DatCH_Data SendHR()=0;
};


class DatCH_BPG1_0: public DatCH_BPG
{
    enum{
      BPG_INIT,
      BPG_HANDSHAKE,
      BPG_OPERATIONAL,
      BPG_END,
    }state;
    ws_conn_data *conn;
protected:
    void RESET(ws_conn_data *conn);
public:
    DatCH_BPG1_0(ws_conn_data *conn);
    ~DatCH_BPG1_0();
    //DatCH_Data SendData(DatCH_Data) override;
    DatCH_Data SendHR();

};


#endif
