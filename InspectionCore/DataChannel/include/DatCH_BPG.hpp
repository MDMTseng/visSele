#ifndef DATCH_BPG_HPP
#define DATCH_BPG_HPP

#include <DatCH.hpp>
#include <string>

#include "DatCH_WebSocket.hpp"


struct BPG_data;
class DatCH_BPG1_0;
typedef int (*BPG_data_feed_callback)(DatCH_BPG1_0 &dch,struct BPG_data *data,uint8_t *callbackInfo);

typedef struct BPG_data_acvImage_Send_info
{
    acvImage* img;
    float scale;
}BPG_data_acvImage_Send_info;


typedef struct BPG_data
{
    char tl[2];//Two letter
    uint8_t prop;

    uint32_t size;//32bit
    uint8_t* dat_raw;
    BPG_data_feed_callback callback;//For bulk data transfer
    uint8_t *callbackInfo;

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

int DatCH_BPG_acvImage_Send(DatCH_BPG1_0 &dch,struct BPG_data *data,uint8_t *callbackInfo);

class DatCH_BPG1_0: public DatCH_BPG
{
    enum{
      BPG_INIT,
      BPG_HANDSHAKE,
      BPG_OPERATIONAL,
      BPG_END,
    }state;
    ws_conn_data *peer;
protected:
    void RESET(ws_conn_data *conn);

    void BufferSetup(int buffer_size);
public:
    size_t buffer_size;
    uint8_t *buffer;
    DatCH_BPG1_0(ws_conn_data *conn);
    ~DatCH_BPG1_0();
    //DatCH_Data SendData(DatCH_Data) override;
    DatCH_Data SendHR();
    DatCH_Data SendData(DatCH_Data);
    DatCH_Data SendData(BPG_data data);
    
    DatCH_Data SendData(websock_data wsdata);
    int MatchPeer(const ws_conn_data* ext_peer);
    DatCH_Data Process_websock_data(websock_data* p_websocket);
};


#endif
