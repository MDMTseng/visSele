#ifndef DatCH_CallBack_WSBPG_HPP
#define DatCH_CallBack_WSBPG_HPP



// V DatCH_BPG1_0::SendData(BPG_data data)                 $$ Application layer $$  DatCH_CallBack_BPG.callback({type:DataType_BPG}) 
//                                                     $$ BPG_protocol(DatCH_CallBack_BPG) $$     DatCH_BPG1_0::Process_websock_data^
//DatCH_CallBack_BPG.callback({type:DataType_websock_data})                         
//                websocket->SendData(data);       $$ DatCH_WebSocket(DatCH_CallBack_WSBPG) $$  DatCH_CallBack_WSBPG::BPG_protocol.SendData({type:DataType_websock_data})^         

#include <main.h>

class DatCH_CallBack_WSBPG : public DatCH_CallBack
{
  public:
  CameraLayer *camera;
  int DatCH_WS_callback(DatCH_Interface *ch_interface, DatCH_Data data, void* callback_param);
  
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param);
};



#endif