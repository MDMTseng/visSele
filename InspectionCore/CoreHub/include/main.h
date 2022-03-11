#ifndef MAIN_HPP
#define MAIN_HPP
#include <chrono>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "FeatureManager.h"
#include "FeatureManager_sig360_circle_line.h"
#include "MatchingEngine.h"
#include "common_lib.h"
#include <stdexcept>
#include "CameraLayer_BMP.hpp"

#ifdef FEATURE_COMPILE_W_ARAVIS
#include "CameraLayer_Aravis.hpp"
#endif

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK
#include "MvCameraControl.h"
#include "CameraLayer_HikRobot_Camera.hpp"
#endif

#ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK

#include "CameraLayer_GIGE_MindVision.hpp"
#endif


#include "acvImage_MophologyTool.hpp"

#include <ImageSampler.h>

// #include "DatCH_BPG.hpp"
// #include "DatCH_CallBack_WSBPG.hpp"
#include "MatchingCore.h"
#include <TSQueue.hpp>

#include "MJPEG_Streamer.hpp"
#include "BPG_Protocol.hpp"

#include "Data_Layer_Protocol.hpp"
#include "Data_Layer_PHY.hpp"



int demomain(int argc, char **argv);
class MJPEG_Streamer2 : public MJPEG_Streamer
{

protected:
  void EVT(struct MJPEG_Streamer_EVT_DATA ev_data)
  {
    LOGI("ev type:%d", ev_data.ev_type);
    switch (ev_data.ev_type)
    {
    case MJPEG_Streamer_EVT_DATA::HTTP_INFO:
      LOGI("host:%s", ev_data.client->host.c_str());
      LOGI("res:%s", ev_data.client->resource.c_str());
      break;
    }
  }

public:
  // MJPEG_Streamer2
  using MJPEG_Streamer::MJPEG_Streamer;
  MJPEG_Streamer2(int port) : MJPEG_Streamer(port) {}
};
using namespace std::chrono;
static uint64_t current_time_ms(void)
{

  milliseconds ms = duration_cast< std::chrono::milliseconds >(
      system_clock::now().time_since_epoch()
  );
  return (uint64_t)ms.count();
}



class PerifChannel;



typedef struct image_pipe_info
{
  CameraLayer *camLayer;
  int type;
  void *context;
  acvImage img;
  uint8_t *img_jpg_enc;
  size_t img_jpg_enc_L;

  CameraLayer::frameInfo fi;
  //acvRadialDistortionParam cam_param;
  FeatureManager_BacPac *bacpac;

  struct datView_Info
  {
    int finspStatus;
    int uInspStatus;
    cJSON *report_json;
  } datViewInfo;
} image_pipe_info;


int BPG_prot_cb_acvImage_Send(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);
//typedef int (*BPG_protocol_data_feed_callback)(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);



typedef struct BPG_protocol_data_acvImage_Send_info
{
    acvImage* img;
    uint16_t scale;
    uint16_t offsetX,offsetY;
    uint16_t fullWidth,fullHeight;

}BPG_protocol_data_acvImage_Send_info;

class m_BPG_Protocol_Interface : public BPG_Protocol_Interface
{
public:

  m_BPG_Protocol_Interface();
  uint16_t CI_pgID;
  int cameraFramesLeft = 0;

  acvImage tmp_buff;
  acvImage cacheImage;
  acvImage dataSend_buff;

  
  PerifChannel *perifCH= NULL;


  CameraLayer *camera = NULL;
  resourcePool<image_pipe_info> resPool;
  int toUpperLayer(BPG_protocol_data bpgdat) override;
  bool checkTL(const char *TL, const BPG_protocol_data *dat);
  uint16_t TLCode(const char *TL);
  void delete_PeripheralChannel();
  static BPG_protocol_data GenStrBPGData(char *TL, char *jsonStr);
  
  static int SEND_acvImage(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);
};




class m_BPG_Link_Interface_WebSocket : public BPG_Link_Interface_WebSocket
{
  public:

  m_BPG_Link_Interface_WebSocket(int port):BPG_Link_Interface_WebSocket(port){};
  m_BPG_Link_Interface_WebSocket():BPG_Link_Interface_WebSocket(){};
  int ws_callback(websock_data data, void *param) override;
};



class PerifProt
{
public:
  typedef struct Pak
  {
    uint8_t *type;
    uint64_t *length_raw;
    uint64_t length;
    uint8_t *data;
    uint8_t *raw;
    uint8_t *next;
  } Pak;
  static uint64_t length_raw2real_ength(uint64_t x)
  {
    int i;
    for (i = 0; i < 4; i++)
    {
      uint64_t a = 0xFFull << (8 * i);
      uint64_t b = 0xFF00000000000000ull >> (8 * i);

      a = x & a;
      b = x & b;

      a = a << 8 * (7 - 2 * i);
      b = b >> 8 * (7 - 2 * i);

      x = x & (~(0xFFull << (8 * i)));
      x = x & (~(0xFF00000000000000ull >> (8 * i)));
      x = x | a;
      x = x | b;
    }

    return x; // don't forget this
  }

  static Pak parse(uint8_t *raw)
  {
    Pak pak;
    pak.type = raw;
    pak.length_raw = (uint64_t *)(raw + 2);
    pak.length = length_raw2real_ength(*pak.length_raw);
    pak.data = raw + 10;
    pak.raw = raw;
    pak.next = pak.data + pak.length;
    return pak;
  }

  static int countValidArr(uint8_t *dat, uint64_t len)
  {
    if (!dat)
      return -1;
    uint64_t rest_len = len;
    uint8_t *rawData = dat;
    int count = 0;
    while (rest_len)
    {
      Pak iter = parse(rawData);
      uint64_t iterPakTotalLen = iter.next - iter.raw;
      if (rest_len < iterPakTotalLen)
        return -1;
      LOGI("[%c%c]:len:%d", iter.type[0], iter.type[1], iter.length);
      rest_len -= iterPakTotalLen;
      rawData = iter.next;
      count++;
    }
    return count;
  }
  static int countValidArr(Pak *pakArr)
  {
    if (!pakArr)
      return -1;
    uint64_t rest_len = pakArr->length;
    uint8_t *rawData = pakArr->data;
    return countValidArr(rawData, rest_len);
  }

  static int fetch(uint8_t *raw, uint64_t len, char *subPakType, Pak *subpak)
  {
    if (!raw || subpak == NULL)
      return -1;
    uint64_t rest_len = len;
    uint8_t *rawData = raw;
    int count = 0;
    while (rest_len)
    {
      Pak iter = parse(rawData);
      uint64_t iterPakTotalLen = iter.next - iter.raw;
      if (rest_len < iterPakTotalLen)
        return -1;
      if (subPakType[0] == iter.type[0] && subPakType[1] == iter.type[1])
      {
        *subpak = iter;
        return 0;
      }
      rest_len -= iterPakTotalLen;
      rawData = iter.next;
      count++;
    }
    return -1;
  }
  static int fetch(Pak *pak, char *subPakType, Pak *subpak)
  {
    if (!pak || subpak == NULL)
      return -1;
    uint64_t rest_len = pak->length;
    uint8_t *rawData = pak->data;
    return fetch(rawData, rest_len, subPakType, subpak);
  }
};
int cp_main(int argc, char **argv);

#endif