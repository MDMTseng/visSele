#pragma once
#include "Data_Layer_IF.hpp"

#include "json_seg_parser.hpp"
#include <stdint.h>
#include <cstddef>


class Data_JsonRaw_Layer:public Data_Layer_IF
{
  uint8_t dataBuff[500];
  int buffIdx=0;
  int JsonRawStatus=0;//-2 asking rawSupport, 0 means json only, 1 means support
  int packetID;
  json_seg_parser jsegp;
  int jsegpSum=0;
  int rawRECVL;

  const int base_headerLen=1+1+4+1;//0x1(start 1B)| resv&opcode(1B)| crc(4B) | base length field(1B (+2 or +8))
  const int crcFieldIdx=2;
  const int crcL=4;
  const int lenFieldIdx=crcFieldIdx+crcL;
  public:
  Data_JsonRaw_Layer();



  int askJsonRawSupport();

  int send_json_string(int head_room,uint8_t *data,int len,int leg_room);
  
  int send_raw_data(int opcode,int head_room,uint8_t *data,int len,int leg_room);
  int send_string(int head_room,uint8_t *data,int len,int leg_room);
  int send_binary(int head_room,uint8_t *data,int len,int leg_room);
  int send_data(int head_room,uint8_t *data,int len,int leg_room);

  virtual int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode)=0;
  enum RTYPE
  {
    INIT,
    JSON,
    JSONRAW,
    ERROR
  };
  RTYPE recvType=RTYPE::INIT;
  int jsonRawStrL=0;
  int recv_data(uint8_t *data,int len, bool is_a_packet=false);
  
  // int send_data(int head_room,uint8_t *data,int len,int leg_room)
  // {
  //   if(downlayer_df==NULL)return -1;
    
  //   return downlayer_df->send_data(head_room,data,len,leg_room);

  // }
};