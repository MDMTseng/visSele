
#include "DatCH_BPG.hpp"

DatCH_BPG1_0::DatCH_BPG1_0(ws_conn_data *conn): DatCH_BPG()
{
  version="1.0.0.alpha";
  RESET(conn);
}

void DatCH_BPG1_0::RESET(ws_conn_data *peer)
{

  if(cb_obj && peer)
  {//Tear down connection
      websock_data close_pkt;
      close_pkt.type = websock_data::CLOSING;
      close_pkt.peer = peer;
      DatCH_Data ws_data;
      ws_data.type = DatCH_Data::DataType_websock_data;
      ws_data.data.p_websocket = &close_pkt;
      cb_obj->callback(this, ws_data, callback_param);
  }

  state = BPG_END;
  this->peer = peer;

}

DatCH_BPG1_0::~DatCH_BPG1_0()
{

}


DatCH_Data DatCH_BPG1_0::SendHR()
{

}

int DatCH_BPG1_0::MatchPeer(const ws_conn_data* ext_peer)
{
    /*if(ext_peer!=NULL && peer!=NULL)
    {
      return (ext_peer->getAddr() == peer->getAddr() );
    }*/
    return (peer == ext_peer);
}

DatCH_Data DatCH_BPG1_0::SendData(DatCH_Data data)
{
    if(cb_obj == NULL)
    {
        return GenErrorMsg(DatCH_Data_error::NO_CALLBACK_ERROR);
    }
    if(data.type==DatCH_Data::DataType_websock_data)//From websocket
    {
      return Process_websock_data(data.data.p_websocket);
    }
    else if(data.type==DatCH_Data::DataType_BPG)//From Application layer
    {

    }

    return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
}

DatCH_Data DatCH_BPG1_0::Process_websock_data(websock_data* p_websocket)
{
    if(p_websocket == NULL )
    {
      return GenErrorMsg(DatCH_Data_error::SEND_ERROR);
    }

    if(peer != NULL && p_websocket->peer != peer)
    {
      return GenErrorMsg(DatCH_Data_error::SEND_ERROR);
    }

    switch(p_websocket->type)
    {
      case websock_data::OPENING:
        RESET(p_websocket->peer);
      break;
      case websock_data::CLOSING:
        RESET(NULL);
      break;
      case websock_data::DATA_FRAME:

printf(">>>>>>>>>>>>>0\n");
        uint8_t* raw = p_websocket->data.data_frame.raw;
        size_t rawL = p_websocket->data.data_frame.rawL;
        if(cb_obj!=NULL && p_websocket->data.data_frame.isFinal && rawL >= 7)
        {
        printf(">>>>>>>>>>>>>1\n");
          BPG_data updata;
          updata.size =
            ((uint32_t)raw[3]<<24)|
            ((uint32_t)raw[4]<<16)|
            ((uint32_t)raw[5]<<8)|
            (uint32_t)raw[6];
          if(updata.size+7 != rawL)
          {
            printf(">>>>>>>>>>>>>2\n",updata.size,);
            break;
          }
          updata.tl[0] = raw[0];
          updata.tl[1] = raw[1];
          updata.prop = raw[2];
          updata.raw = &(raw[7]);
          DatCH_Data dch_data = GenMsgType(DatCH_Data::DataType_BPG);
          dch_data.data.p_BPG_data = &updata;
          cb_obj->callback(this, dch_data, callback_param);
                      printf(">>>>>>>>>>>>>3\n");
        }

      break;
    }

    return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
}
