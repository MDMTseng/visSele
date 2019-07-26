#ifndef MAIN_HPP
#define MAIN_HPP


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
#include "CameraLayer_GIGE_MindVision.hpp"
#include "acvImage_MophologyTool.hpp"

#include <MicroInsp_FType.hpp>
#include <Ext_Util_API.hpp>


class DatCH_CallBack_BPG : public DatCH_CallBack
{
  public:

  uint16_t CI_pgID;
  bool cameraFeedTrigger=false;

  DatCH_BPG1_0 *self;
  acvImage tmp_buff;
  acvImage cacheImage;  
  acvImage dataSend_buff;
  MicroInsp_FType *mift=NULL;
  Ext_Util_API *exApi=NULL;
  
  CameraLayer *camera=NULL;
  
  bool checkTL(const char *TL,const BPG_data *dat);
  uint16_t TLCode(const char *TL);
  DatCH_CallBack_BPG(DatCH_BPG1_0 *self);

  void delete_Ext_Util_API();
  void delete_MicroInsp_FType();
  static BPG_data GenStrBPGData(char *TL, char* jsonStr);
  
  int callback(DatCH_Interface *from, DatCH_Data data, void* callback_param);
};




#endif