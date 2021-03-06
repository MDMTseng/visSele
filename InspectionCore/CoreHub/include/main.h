#ifndef MAIN_HPP
#define MAIN_HPP


#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "common_lib.h"
#include <stdexcept>
#include "CameraLayer_BMP.hpp"
#include "CameraLayer_GIGE_MindVision.hpp"
#include "acvImage_MophologyTool.hpp"

#include <MicroInsp_FType.hpp>
#include <Ext_Util_API.hpp>
#include <ImageSampler.h>


#include "DatCH_Image.hpp"
#include "DatCH_BPG.hpp"
#include "DatCH_CallBack_WSBPG.hpp"


#include "opencv2/opencv.hpp"
#include <TSQueue.hpp>

class MicroInsp_FType:public SOCK_JSON_Flow
{
    public:
    MicroInsp_FType(char *host,int port) throw(int):
      SOCK_JSON_Flow(host,port){};
      
    virtual ~MicroInsp_FType(){DESTROY();}
    
    virtual int recv_json( char* json_str, int json_strL);
    protected:
    int ev_on_close() override;
};

struct image_pipe_info;
typedef struct camera_channel_info
{
  CameraLayer *camera;
  resourcePool<struct image_pipe_info> *rp_ref;
  CameraLayer::frameInfo fi;
  void *context;
  int id;
} camera_channel_info;


typedef struct image_pipe_info
{
  camera_channel_info* camchinfo;
  acvImage img;
  cv::Mat cvImg;
} image_pipe_info;


class DatCH_CallBack_BPG : public DatCH_CallBack
{
  public:

  uint16_t CI_pgID;
  int cameraFramesLeft=0;

  DatCH_BPG1_0 *self;
  acvImage tmp_buff;
  acvImage cacheImage;  
  acvImage dataSend_buff;
  MicroInsp_FType *mift=NULL;
  Ext_Util_API *exApi=NULL;
  
  std::vector<camera_channel_info> cameraArray;
  std::vector<image_pipe_info> cameraArrayInfo;


  
  bool checkTL(const char *TL,const BPG_data *dat);
  uint16_t TLCode(const char *TL);
  DatCH_CallBack_BPG(DatCH_BPG1_0 *self);

  void delete_Ext_Util_API();
  void delete_MicroInsp_FType();
  static BPG_data GenStrBPGData(char *TL, char* jsonStr);
  
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param);
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
    }Pak;
    static uint64_t length_raw2real_ength(uint64_t x)
    {
        int i;
        for(i=0; i<4; i++)
        {
            uint64_t a = 0xFFull << (8*i);
            uint64_t b = 0xFF00000000000000ull >> (8*i);

            a = x&a;
            b = x&b;

            a=a<<8*(7-2*i);
            b=b>>8*(7-2*i);

            x=x&(~(0xFFull<<(8*i)));
            x=x&(~(0xFF00000000000000ull>>(8*i)));
            x=x|a;
            x=x|b;
        }

        return x;   // don't forget this
    }


    static Pak parse(uint8_t* raw)
    {
        Pak pak;
        pak.type=raw;
        pak.length_raw=(uint64_t *)(raw+2);
        pak.length=length_raw2real_ength( *pak.length_raw);
        pak.data=raw+10;
        pak.raw=raw;
        pak.next=pak.data+pak.length;
        return pak; 
    }

    static int countValidArr(uint8_t *dat,uint64_t len)
    {
        if(!dat)return -1;
        uint64_t rest_len=len;
        uint8_t* rawData = dat;
        int count=0;
        while(rest_len)
        {
            Pak iter=parse(rawData);
            uint64_t iterPakTotalLen= iter.next-iter.raw;
            if(rest_len<iterPakTotalLen)return -1;
            LOGI("[%c%c]:len:%d",iter.type[0],iter.type[1],iter.length);
            rest_len-=iterPakTotalLen;
            rawData = iter.next;
            count++;
        }
        return count;
    }
    static int countValidArr(Pak *pakArr)
    {
        if(!pakArr)return -1;
        uint64_t rest_len=pakArr->length;
        uint8_t* rawData = pakArr->data;
        return countValidArr(rawData,rest_len);
    }

    static int fetch(uint8_t *raw,uint64_t len,char *subPakType,Pak *subpak)
    {
        if(!raw || subpak==NULL)return -1;
        uint64_t rest_len=len;
        uint8_t* rawData = raw;
        int count=0;
        while(rest_len)
        {
            Pak iter=parse(rawData);
            uint64_t iterPakTotalLen= iter.next-iter.raw;
            if(rest_len<iterPakTotalLen)return -1;
            if(subPakType[0]==iter.type[0] && subPakType[1]==iter.type[1])
            {
                *subpak = iter;
                return 0;
            }
            rest_len-=iterPakTotalLen;
            rawData = iter.next;
            count++;
        }
        return -1;
    }
    static int fetch(Pak *pak,char *subPakType,Pak *subpak)
    {
        if(!pak || subpak==NULL)return -1;
        uint64_t rest_len=pak->length;
        uint8_t* rawData = pak->data;
        return fetch(rawData,rest_len,subPakType,subpak);
    }


};
int cp_main(int argc, char **argv);

#endif