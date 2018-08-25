#ifndef DATCH_BPG_HPP
#define DATCH_BPG_HPP

#include <DatCH.hpp>
#include <string>

#include "DatCH_WebSocket.hpp"

typedef struct BPG_data
{
    char tl[2];//Two letter
    uint8_t prop;

    uint32_t size;//32bit
    uint8_t* dat_raw;
    acvImage* dat_img;//For bulk data transfer
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
    //DatCH_Data SendData(DatCH_Data) override;
    virtual DatCH_Data SendHR()=0;
    virtual int MatchPeer(const ws_conn_data* ext_peer)=0;
    virtual DatCH_Data Process_websock_data(websock_data* p_websocket)=0;
};


class DatCH_BPG1_0: public DatCH_BPG
{
    enum{
      BPG_INIT,
      BPG_HANDSHAKE,
      BPG_OPERATIONAL,
      BPG_END,
    }state;
    ws_conn_data *peer;
    size_t buffer_size;
    uint8_t *buffer;
protected:
    void RESET(ws_conn_data *conn);

    void BufferSetup(int buffer_size);
public:
    DatCH_BPG1_0(ws_conn_data *conn);
    ~DatCH_BPG1_0();
    //DatCH_Data SendData(DatCH_Data) override;
    DatCH_Data SendHR();
    DatCH_Data SendData(DatCH_Data);
    DatCH_Data SendData(BPG_data data);
    int MatchPeer(const ws_conn_data* ext_peer);
    DatCH_Data Process_websock_data(websock_data* p_websocket);
};


#endif
