
#include "DatCH_BPG.hpp"
#include "logctrl.h"


int DatCH_BPG_acvImage_Send(DatCH_BPG1_0 &dch,struct BPG_data *data,uint8_t *callbackInfo)
{
  if(callbackInfo==NULL)return -1;
  websock_data ws_data;
  ws_data.type = websock_data::DATA_FRAME;
  ws_data.data.data_frame.type=WS_DFT_CONT_FRAME;

  BPG_data_acvImage_Send_info *img_info = (BPG_data_acvImage_Send_info*)callbackInfo;

  acvImage *img=img_info->img;


  uint8_t header[]={
    0,0,
    img->GetWidth()>>8,
    img->GetWidth(),
    img->GetHeight()>>8,
    img->GetHeight(),

  };


  ws_data.data.data_frame.isFinal=false;
  ws_data.data.data_frame.raw=header;
  ws_data.data.data_frame.rawL=6;
  dch.SendData(ws_data);

  ws_data.data.data_frame.type=WS_DFT_CONT_FRAME;

  int rest_len =
    img->GetWidth()*
    img->GetHeight();

  uint8_t *img_pix_ptr=img->CVector[0];

  for(bool isKeepGoing=true;isKeepGoing && rest_len;)
  {
    int sendL = 0;
    for(int i=0;i<dch.buffer_size-4;i+=4,img_pix_ptr+=3)
    {
      dch.buffer[i]=img_pix_ptr[2];
      dch.buffer[i+1]=img_pix_ptr[1];
      dch.buffer[i+2]=img_pix_ptr[0];
      dch.buffer[i+3]=255;
      sendL+=4;
      rest_len--;
      if(rest_len==0)
      {
        isKeepGoing=false;
        ws_data.data.data_frame.isFinal=true;
        break;
      }
    }
    ws_data.data.data_frame.rawL=sendL;
    ws_data.data.data_frame.raw=dch.buffer;
    //printf("L:%d\n",ws_data.data.data_frame.rawL);

    dch.SendData(ws_data);
  }
  return 0;

}



DatCH_Data DatCH_BPG1_0::SendData(websock_data wsdata)
{
  DatCH_Data ret_data = GenMsgType(DatCH_Data::DataType_websock_data);
  websock_data ws_data;
  memset(&ws_data,0,sizeof(ws_data));
  wsdata.peer = peer;
  ret_data.data.p_websocket = &wsdata;
  int ret = cb_obj->callback(this, ret_data, callback_param);
  if(ret != 0)
    return GenMsgType(DatCH_Data::DataType_NAK);
  return GenMsgType(DatCH_Data::DataType_ACK);
}



DatCH_BPG1_0::DatCH_BPG1_0(ws_conn_data *conn): DatCH_BPG()
{
  version="1.0.0.alpha";
  peer = NULL;
  buffer = NULL;
  RESET(conn);
  BufferSetup(1000);
}
void DatCH_BPG1_0::BufferSetup(int buffer_size)
{
  if(buffer)
  {
    delete buffer;
  }
  this->buffer_size = buffer_size;
  if(buffer_size==0)
  {
    buffer=NULL;
  }
  else
  {
    buffer = new uint8_t[buffer_size];
  }
}
void DatCH_BPG1_0::RESET(ws_conn_data *peer)
{
  LOGV("RESET:this->peer:%p   peer:%p",this->peer,peer);
  if(cb_obj && this->peer)
  {//Tear down connection
      websock_data close_pkt;
      close_pkt.type = websock_data::CLOSING;
      close_pkt.peer = this->peer;
      this->peer = NULL;
      DatCH_Data ws_data=GenMsgType(DatCH_Data::DataType_websock_data);
      ws_data.data.p_websocket = &close_pkt;
      cb_obj->callback(this, ws_data, callback_param);
  }
  state = BPG_END;
  this->peer = peer;

}

DatCH_BPG1_0::~DatCH_BPG1_0()
{
  BufferSetup(0);
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

DatCH_Data DatCH_BPG1_0::SendData(BPG_data data)
{
    if(cb_obj == NULL)
    {
        return GenErrorMsg(DatCH_Data_error::NO_CALLBACK_ERROR);
    }
    uint8_t header[7];
    DatCH_Data ret_data = GenMsgType(DatCH_Data::DataType_websock_data);
    websock_data ws_data;
    ret_data.data.p_websocket = &ws_data;
    ws_data.peer = peer;
    ws_data.type = websock_data::DATA_FRAME;
    ws_data.data.data_frame.type=WS_DFT_BINARY_FRAME;



    header[0] = data.tl[0];
    header[1] = data.tl[1];

    header[2] = data.prop;
    uint32_t length=0;
    if(data.dat_raw)
    {
      length = data.size;
    }
    header[3] = length>>24;
    header[4] = length>>16;
    header[5] = length>>8;
    header[6] = length;

    ws_data.data.data_frame.raw=header;
    ws_data.data.data_frame.rawL=sizeof(header);
    //ws_data.data.data_frame.isFinal=(length==0);
    ws_data.data.data_frame.isFinal=(length==0 && data.callback==NULL);

    LOGV("TWOLETTER:  %c%c cb:%p",data.tl[0],data.tl[1],data.callback);

    cb_obj->callback(this, ret_data, callback_param);

    if(ws_data.data.data_frame.isFinal)
    {
      return GenMsgType(DatCH_Data::DataType_ACK);
    }

    ws_data.data.data_frame.type=WS_DFT_CONT_FRAME;
    ws_data.data.data_frame.isFinal=false;
    //The following data is continuous frame

    if(data.dat_raw)
    {
      int idxOffset;//data.size
      for(idxOffset = 0;
        (idxOffset+buffer_size)<=(data.size);
        idxOffset+=buffer_size)
      {
        ws_data.data.data_frame.rawL =buffer_size;
        ws_data.data.data_frame.raw  =data.dat_raw+idxOffset;
        int ret = cb_obj->callback(this, ret_data, callback_param);
        if(ret<0)break;
      }

      ws_data.data.data_frame.isFinal=true;
      ws_data.data.data_frame.rawL =data.size - idxOffset;
      ws_data.data.data_frame.raw  =data.dat_raw+idxOffset;
      int ret = cb_obj->callback(this, ret_data, callback_param);


    }
    else if(data.callback)
    {
      int ret_val;
      ret_val=data.callback(*this,&data,data.callbackInfo);
     

      if(ret_val!=0)
      {
        return GenMsgType(DatCH_Data::DataType_NAK);
      }

    }
    else
    {
      ws_data.data.data_frame.isFinal=true;
      ws_data.data.data_frame.rawL=0;
      ws_data.data.data_frame.raw=NULL;
      ret_data.data.p_websocket = &ws_data;
      cb_obj->callback(this, ret_data, callback_param);
    }


    //if()
    return GenMsgType(DatCH_Data::DataType_ACK);
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
      return SendData(*data.data.p_BPG_data);
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

    //printf(">>>>>>>>>>>>>switch\n");
    switch(p_websocket->type)
    {
      case websock_data::OPENING:
        LOGI(">>>>>>>>>>>>>OPENING");
        RESET(p_websocket->peer);
        //printf(">>>>>>>>>>>>>OPENING_RESET\n");
      break;
      case websock_data::CLOSING:
        LOGI(">>>>>>>>>>>>>CLOSING");
        RESET(NULL);
      break;
      case websock_data::DATA_FRAME:

        LOGI(">>>>>>>>>>>>>DATA_FRAME");
        uint8_t* raw = p_websocket->data.data_frame.raw;
        size_t rawL = p_websocket->data.data_frame.rawL;
        if(cb_obj!=NULL && p_websocket->data.data_frame.isFinal && rawL >= 7)
        {
          BPG_data updata;
          updata.size =
            ((uint32_t)raw[3]<<24)|
            ((uint32_t)raw[4]<<16)|
            ((uint32_t)raw[5]<<8)|
            (uint32_t)raw[6];
          if(updata.size+7 != rawL)
          {
            break;
          }
          updata.tl[0] = raw[0];
          updata.tl[1] = raw[1];
          updata.prop = raw[2];
          updata.dat_raw = &(raw[7]);
          DatCH_Data dch_data = GenMsgType(DatCH_Data::DataType_BPG);
          dch_data.data.p_BPG_data = &updata;
          cb_obj->callback(this, dch_data, callback_param);
        }

      break;
    }

    return GenErrorMsg(DatCH_Data_error::NOT_SUPPORTED);
}
