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

#include "InspectionTarget.hpp"


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
  
  int fromUpperLayer_DATA(const char*TL,int pgID,cJSON* json);
  int fromUpperLayer_DATA(const char*TL,int pgID,BPG_protocol_data_acvImage_Send_info* imgInfo);
  int fromUpperLayer_DATA(const char*TL,int pgID,char* str);
  int fromUpperLayer_DATA(const char*TL,int pgID,InspectionTarget_EXCHANGE* excahngeInfo);
  int fromUpperLayer_SS(int pgID,bool isACK,const char*fromTL,const char* error_msg);
  void delete_PeripheralChannel();
  static BPG_protocol_data GenStrBPGData(const char *TL, char *jsonStr);
  
  static int SEND_acvImage(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo);
};




class m_BPG_Link_Interface_WebSocket : public BPG_Link_Interface_WebSocket
{
  public:

  m_BPG_Link_Interface_WebSocket(int port):BPG_Link_Interface_WebSocket(port){};
  m_BPG_Link_Interface_WebSocket():BPG_Link_Interface_WebSocket(){};
  int ws_callback(websock_data data, void *param) override;
};

class PerifChannel:public Data_JsonRaw_Layer
{
  
  public:

  uint16_t conn_pgID;
  int pkt_count = 0;
  int ID;
  PerifChannel():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
  }

  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode);
  int recv_RESET()
  {
    // printf("Get recv_RESET\n");
  }
  int recv_ERROR(ERROR_TYPE errorcode)
  {
    // printf("Get recv_ERROR:%d\n",errorcode);
  }
  
  void connected(Data_Layer_IF* ch){
    
    printf(">>>%X connected\n",ch);
  }

  void disconnected(Data_Layer_IF* ch){
    printf(">>>%X disconnected\n",ch);
  }

  ~PerifChannel()
  {
    close();
    printf("MData_uInsp DISTRUCT:%p\n",this);
  }

  // int send_data(int head_room,uint8_t *data,int len,int leg_room){
    
  //   // printf("==============\n");
  //   // for(int i=0;i<len;i++)
  //   // {
  //   //   printf("%d ",data[i]);
  //   // }
  //   // printf("\n");
  //   return recv_data(data,len, false);//LOOP back
  // }
};






int cp_main(int argc, char **argv);

cJSON *cJSON_DirFiles(const char *path, cJSON *jObj_to_W, int depth = 0);

int str_ends_with(const char *str, const char *suffix);


int sendcJSONTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json);


std::string getTimeStr(const char *timeFormat = "%d-%m-%Y %H:%M:%S");

bool isEndsWith(const char *str, const char *suffix);

bool isStartsWith(const char *str, const char *prefix);

int getFileCountInFolder(const char* path,const char* ext);
char* PatternRest(char *str, const char *pattern);


int printfTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...);

int sendcJSONTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json);

int sendResultTo_perifCH(PerifChannel *perifCH,int uInspStatus, uint64_t timeStamp_100us);

int removeOldestRep(const char* path,const char* ext);


CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj);
int getImage(CameraLayer *camera,acvImage *dst_img,int trig_type=0,int timeout_ms=-1);

void setThreadPriority(std::thread &thread, int type, int priority);




void ImageDownSampling(acvImage &dst, acvImage &src, int downScale, ImageSampler *sampler, int doNearest = 1,
                       int X = -1, int Y = -1, int W = -1, int H = -1);


BPG_protocol_data_acvImage_Send_info ImageDownSampling_Info(acvImage &dstBuff, acvImage &src, int downScale, ImageSampler *sampler, int doNearest,
                       int X, int Y, int W, int H);

#endif