#pragma once
#include "Data_Layer_IF.hpp"

#include "json_seg_parser.hpp"
#include <stdint.h>
#include <cstddef>
#include <stdarg.h>

class Data_JsonRaw_Layer:public Data_Layer_IF
{
  uint8_t dataBuff[20480];
  int buffIdx=0;
  int JsonRawStatus=0;//-2 asking rawSupport, 0 means json only, 1 means support
  int packetID;
  json_seg_parser jsegp;
  int jlevel=0;
  int rawRECVL;


  const int base_headerLen=1+1+4+1;//0x1(start 1B)| resv&opcode(1B)| crc(4B) | base length field(1B (+2 or +8))
  const int crcFieldIdx=2;
  const int crcL=4;
  const int lenFieldIdx=crcFieldIdx+crcL;
  const char *VERSION="0.0.1";
  const char RESET_PACKET[17]="{\"type\":\"RESET\"}";
  protected:
  char peerVERSION[20];
  public:
  Data_JsonRaw_Layer();



  int ask_JsonRaw_version();
  int rsp_JsonRaw_version();
  int send_RESET();
  virtual int RESET()
  {
    sprintf((char*)dataBuff,"");
    buffIdx=0;
    JsonRawStatus=0;
    packetID=0;
    jsegp.reset();
    jlevel=0;
    rawRECVL=0;
    recvType=RTYPE::INIT;
    errorCode=ERROR_TYPE::NONE;
    return 0;
  }


  int send_json_string(int head_room,uint8_t *data,int len,int leg_room);
  
  int send_printf(uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...);
  int send_raw_data(int opcode,int head_room,uint8_t *data,int len,int leg_room);
  int send_string(int head_room,uint8_t *data,int len,int leg_room);
  int send_binary(int head_room,uint8_t *data,int len,int leg_room);
  int send_data(int head_room,uint8_t *data,int len,int leg_room);

  virtual int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  enum RTYPE
  {
    INIT,
    JSON,
    JSONRAW,
    ERROR_SEC,
    TRAILER,   // between a completed JSON frame and its optional *HHHH\n CRC
  };
  
  enum ERROR_TYPE
  {
    NONE,
    INIT_CHAR_ERROR,
    JSON_FORMAT_ERROR,
    RECV_BUFFER_FULL,
    RAW_CRC_ERROR,
    RAW_DATA_OVERSIZE
  };
  RTYPE recvType=RTYPE::INIT;
  // Optional per-frame integrity trailer ("{...}*HHHH\n", CRC16-CCITT over
  // the JSON bytes) -- the uInspESP32 firmware appends it to every frame.
  // Frames without a trailer pass through (legacy peers); frames with a BAD
  // trailer are dropped and counted, never latched.
  // TX side is OPT-IN per channel: legacy peripherals (uInspMEGA...) latch
  // on stray trailer bytes, so only the ESP32 channel turns this on.
  bool tx_trailer=false;
  char trailerBuf[6];
  int trailerIdx=0;
  uint32_t rx_frames=0;
  uint32_t rx_crc_fail=0;
  uint32_t rx_crc_ok=0;
  static uint16_t crc16_ccitt(const uint8_t *d,int len);
  void finishJsonFrame(bool crc_present,bool crc_ok);
  ERROR_TYPE errorCode=ERROR_TYPE::NONE;
  int jsonRawStrL=0;
  int recv_data(uint8_t *data,int len, bool is_a_packet=false);
  
  virtual int recv_RESET()=0;
  virtual int recv_ERROR(ERROR_TYPE errorcode)=0;
  
  // int send_data(int head_room,uint8_t *data,int len,int leg_room)
  // {
  //   if(downlayer_df==NULL)return -1;
    
  //   return downlayer_df->send_data(head_room,data,len,leg_room);

  // }
};