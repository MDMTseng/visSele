#ifndef MAIN_HPP
#define MAIN_HPP
#include <chrono>
#include <mutex>
#include <set>
#include <vector>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include <opencv2/core.hpp>
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


enum image_pipe_info_OccupyFIdx
{
  // imgInsp,
  // datView,
  snapSave,
  resendCache,
  // Not an occupant: the marker image_pipe_info_gc claims the slot with, so
  // that exactly one caller can hand it back. See image_pipe_info_gc.
  returnedToPool
};

typedef struct image_pipe_info
{

  // Read-modify-written by several threads: the datView thread sets/clears
  // snapSave and clears the previous cache slot's resendCache, while the
  // snap-save thread clears snapSave on the same slot. A lost |= left a slot
  // occupied forever (the pool bleeds one entry per lost update until "image
  // pool empty -> dropped a frame" becomes permanent and acquisition stalls);
  // a lost &= handed a slot back while someone was still writing into it.
  //
  // Guarded by occupyFlag_lock (wiringPanel.cpp) rather than being a
  // std::atomic, because resourcePool holds these in a std::vector<T> and
  // resize() needs the element type to stay movable. Only ever touched through
  // image_pipe_info_occupyFlag_set / _clr / image_pipe_info_gc.
  uint32_t occupyFlag;
  bool endLifeCycle;
  CameraLayer *camLayer;
  int type;
  void *context;
  cv::Mat img;     // phase 3a: was acvImage
  uint8_t *img_jpg_enc;
  size_t img_jpg_enc_L;

  CameraLayer::frameInfo fi;

  // Host-clock stamps for splitting the report latency. The board measures the
  // whole chain (trigger -> verdict processed) and that is ~479ms; the send
  // queue and serial write together account for 2.6ms of it, so the remaining
  // ~476ms is upstream of them and had no breakdown at all. These two turn
  // "upstream" into two answerable numbers: how long the frame waited to be
  // inspected, and how long the inspection itself took.
  uint64_t host_rx_us;     // pushed onto inspQueue (frame in hand)
  uint64_t host_insp_us;   // entered ImgPipeProcessCenter_imp
  // Same idea one queue further along, for the preview. The preview costs the
  // verdict path 66ms that is NOT queue blocking (that was removed) and the
  // only way to tell CPU contention from wire time is to split the send
  // itself. Stamped when the pipe is pushed onto datViewQueue.
  uint64_t dview_enq_us;   // pushed onto datViewQueue
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



// Image-send callback info. The image is supplied as a cv::Mat (3-channel BGR
// or 1-channel gray); SEND_acvImage will auto-detect grayscale (B==G==R for
// all px when 3ch, or 1-channel input) and emit a single-component JPEG when
// IMG_STREAMING_JPEG_QUALITY > 0, otherwise the legacy raw RGBA wire format.
typedef struct BPG_protocol_data_acvImage_Send_info
{
    cv::Mat* img;
    uint16_t scale;
    uint16_t offsetX,offsetY;
    uint16_t fullWidth,fullHeight;
    // Per-call JPEG quality override for SEND_acvImage. 0 = use the global
    // DataView_JPEG_quality (default). >0 forces a JPEG encode for THIS send
    // only (e.g. LD thumbnail reads want JPEG without flipping the live stream).
    int jpeg_quality = 0;

    // Pre-encoded JPEG for this frame, filled ONCE by the caller BEFORE the
    // subscriber fan-out. When set, SEND_acvImage skips imencode entirely and
    // just frames the bytes.
    //
    // Why it exists: the fan-out is `subscribersLock { for peer: fromUpperLayer
    // -> ... linkLayerLock -> SEND_acvImage }`, so the encode used to run
    // INSIDE both locks, once PER PEER. Measured on this machine: 6.5ms avg /
    // 16.2ms max per frame full-frame (441KB JPEG), 0.4ms at the production
    // ROI crop (31KB) -- and every millisecond of it parked every other
    // producer. Encoding before the fan-out costs nothing extra and takes the
    // whole thing out of the locks; a second subscriber then costs a memcpy
    // instead of another encode.
    //
    // jpg_q travels with the bytes on purpose: DataView_JPEG_quality can be
    // changed by an ST between the encode and the send, and the metadata
    // header must describe the bytes that are actually going out.
    const std::vector<uint8_t> *jpg = nullptr;
    uint8_t jpg_fmt = 0;   // 1 = 3-component BGR, 2 = 1-component grayscale
    int     jpg_q   = 0;   // quality the cached bytes were encoded at

}BPG_protocol_data_acvImage_Send_info;

class m_BPG_Protocol_Interface : public BPG_Protocol_Interface
{
public:

  m_BPG_Protocol_Interface();
  uint16_t CI_pgID;

  cv::Mat  tmp_buff;          // phase 3a (3 BPG image members all cv::Mat now)
  cv::Mat  cacheImage;        // phase 3a
  cv::Mat  dataSend_buff;     // phase 3a

  
  PerifChannel *perifCH= NULL;


  CameraLayer *camera = NULL;
  resourcePool<image_pipe_info> resPool;

  // Peers opted in to the live inspection stream (SS/RP/IM push). The default
  // (first) client is auto-subscribed for backward compat; others opt in via SB.
  std::set<void *> stream_subscribers;
  // Guards stream_subscribers AND the per-peer fromUpperLayer dispatch inside
  // pushToSubscribers. WS CLOSING/ERROR must acquire this BEFORE freeing the
  // peer so an in-flight push can never deref a dangling void* (MT_LOCK is a
  // no-op so we can't rely on it here).
  std::mutex subscribersLock;
  void subscribeStream(void *peer) {
    if (!peer) return;
    std::lock_guard<std::mutex> g(subscribersLock);
    stream_subscribers.insert(peer);
  }
  void unsubscribeStream(void *peer) {
    std::lock_guard<std::mutex> g(subscribersLock);
    stream_subscribers.erase(peer);
  }
  // How many peers a pushToSubscribers() call will actually reach. Worth having
  // at the send sites: the fan-out is silent, so "packet sent" to an empty list
  // looks exactly like a working stream in the logs.
  size_t streamSubscriberCount() {
    std::lock_guard<std::mutex> g(subscribersLock);
    return stream_subscribers.size();
  }
  // Push one packet to every subscribed peer (routes per-peer so framing stays
  // per-client; bulk image callbacks pick up the active peer under linkLayerLock).
  void pushToSubscribers(BPG_protocol_data bpg_dat)
  {
    std::lock_guard<std::mutex> g(subscribersLock);
    for (void *p : stream_subscribers)
      fromUpperLayer(bpg_dat, p);
  }
  // Push a MULTI-PACKET batch (e.g. SS-start / payload / SS-end) atomically
  // with respect to the subscriber set: three separate pushToSubscribers
  // calls each retake subscribersLock, so a peer subscribing between them
  // receives a torn batch (SS-end with no SS-start, or an unterminated
  // SS-start left open in its demux). Peer-major on purpose -- each peer
  // gets its complete, ordered batch before the next peer sees anything.
  // Lock order (subscribersLock -> linkLayerLock inside fromUpperLayer) is
  // the same as pushToSubscribers.
  void pushBatchToSubscribers(const BPG_protocol_data *bpg_dats, size_t count)
  {
    std::lock_guard<std::mutex> g(subscribersLock);
    for (void *p : stream_subscribers)
      for (size_t i = 0; i < count; i++)
        fromUpperLayer(bpg_dats[i], p);
  }

  int toUpperLayer(BPG_protocol_data bpgdat, void *peer) override;
  bool checkTL(const char *TL, const BPG_protocol_data *dat);
  uint16_t TLCode(const char *TL);
  void delete_PeripheralChannel();
  static BPG_protocol_data GenStrBPGData(char *TL,const char *jsonStr);
  
  static int SEND_acvImage(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);
};




class m_BPG_Link_Interface_WebSocket : public BPG_Link_Interface_WebSocket
{
  public:

  // All currently-connected peers. Shared core state (camera trigger, peripheral
  // channel) is only torn down when this becomes empty (last client left).
  std::set<ws_conn_data *> peers;

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