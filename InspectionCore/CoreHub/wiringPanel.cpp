#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "DatCH_Image.hpp"
#include "DatCH_WebSocket.hpp"
#include "DatCH_BPG.hpp"
#include "DatCH_CallBack_WSBPG.hpp"
#include "acvImage_BasicTool.hpp"


#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>

#include <RingBuf.hpp>

std::timed_mutex mainThreadLock;


#define MT_LOCK(...) mainThreadLock_lock(__LINE__ VA_ARGS(__VA_ARGS__))
#define MT_UNLOCK(...) mainThreadLock_unlock(__LINE__ VA_ARGS(__VA_ARGS__))

int mainThreadLock_lock(int call_lineNumber,char* msg="",int try_lock_timeout_ms=0)
{

  if(try_lock_timeout_ms<=0)
  {
    //LOGI("%s_%d: Locking ",msg,call_lineNumber);
    mainThreadLock.lock();
  }
  else
  {
    using Ms = std::chrono::milliseconds;
    
    //LOGI("%s_%d: Locking %dms",msg,call_lineNumber,try_lock_timeout_ms);
    if(mainThreadLock.try_lock_for(Ms(try_lock_timeout_ms)))
    {
    }
    else
    {
      //LOGI("Lock failed");
      return -1;
    }
  }
  //LOGI("%s_%d: Locked ",msg,call_lineNumber);

  return 0;
}


int mainThreadLock_unlock(int call_lineNumber,char* msg="")
{

  //LOGI("%s_%d: unLocking ",msg,call_lineNumber);
  mainThreadLock.unlock();
  //LOGI("%s_%d: unLocked ",msg,call_lineNumber);

  return 0;
}

DatCH_WebSocket *websocket = NULL;
CameraLayer *gen_camera;
DatCH_CallBack_WSBPG callbk_obj;
DatCH_BPG1_0 *BPG_protocol = new DatCH_BPG1_0(NULL);

std::timed_mutex BPG_protocol_lock;

void BPG_protocol_send(DatCH_Data dat)
{
  //LOGI("SEND_LOCK");
  BPG_protocol_lock.lock();
  //LOGI("SEND_ING");
  BPG_protocol->SendData(dat);
  //LOGI("SEND_UNLOCK");
  BPG_protocol_lock.unlock();
  //LOGI("SEND_END");
}

int _argc;
char **_argv;

DatCH_CallBack_BPG *cb = new DatCH_CallBack_BPG(BPG_protocol);

typedef struct image_pipe_info
{
  CameraLayer *camLayer;
  int type;
  void *context;
  acvImage img;
  CameraLayer::frameInfo fi;
} image_pipe_info;

#define ImagePipeBufferSize 20
RingBuf<image_pipe_info> imagePipeBuffer(new image_pipe_info[ImagePipeBufferSize], ImagePipeBufferSize);
//lens1
//main.cpp  1067 main:v K: 1.00096 -0.00100092 -9.05316e-05 RNormalFactor:1296
//main.cpp  1068 main:v Center: 1295,971

//main.cpp  1075 main:v K: 0.999783 0.00054474 -0.000394607 RNormalFactor:1296
//main.cpp  1076 main:v Center: 1295,971

//lens2
//main.cpp  1061 main:v K: 0.989226 0.0101698 0.000896734 RNormalFactor:1296
//main.cpp  1062 main:v Center: 1295,971


// acvRadialDistortionParam calib_bacpac={
//     calibrationCenter:{1295,971},
//     RNormalFactor:1296,
//     K0:0.999783,
//     K1:0.00054474,
//     K2:-0.000394607,
//     //r = r_image/RNormalFactor
//     //C1 = K1/K0
//     //C2 = K2/K0
//     //r"=r'/K0
//     //Forward: r' = r*(K0+K1*r^2+K2*r^4)
//     //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
//     //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
//     //r/r'=r*K0/r"

//     ppb2b: 63.11896896362305,
//     mmpb2b:  0.630049821,
//     map: NULL
// };

CameraLayer *getCamera(int initCameraType); //0 for real First, then fake one, 1 for real camera only, 2 for fake only

typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst, size_t dstLen, uint8_t *src, size_t srcLen);

void ImageDownSampling(acvImage &dst, acvImage &src, int downScale, ImageSampler *sampler,int doNearest=1,
                       int X = -1, int Y = -1, int W = -1, int H = -1)
{
  int X2 = src.GetWidth() - 1;
  int Y2 = src.GetHeight() - 1;

  if (X < 0)
    X = 0;
  else if (X >= X2)
    X = X2 - 1;

  if (Y < 0)
    Y = 0;
  else if (Y >= Y2)
    Y = Y2 - 1;

  if (W > 0)
  {
    X2 = X + W;
    if (X2 >= src.GetWidth())
    {
      X2 = src.GetWidth() - 1;
    }
  }

  if (H > 0)
  {
    Y2 = Y + H;
    if (Y2 >= src.GetHeight())
    {
      Y2 = src.GetHeight() - 1;
    }
  }
  X /= downScale;
  Y /= downScale;
  X2 /= downScale;
  Y2 /= downScale;
  dst.ReSize((X2 - X + 1), (Y2 - Y + 1));
  LOGI("X:%d Y:%d X2:%d Y2:%d", X, Y, X2, Y2);
  //LOGI("map=%p",map);
  for (int i = Y; i <= Y2; i++)
  {
    int src_i = i * downScale;
    for (int j = X; j <= X2; j++)
    {
      int RSum = 0, GSum = 0, BSum = 0;
      int src_j = j * downScale;

      {
        float bri = 0;
        if (sampler)
        {
          float coord[] = {(float)src_j, (float)src_i};
          
          bri = sampler->sampleImage_IdealCoord(&src, coord,doNearest);

          if (bri > 255)
            bri = 255;
          BSum += bri;
          GSum += bri;
          RSum += bri;
        }
        else
        {
          BSum = src.CVector[src_i][(src_j)*3 + 0];
          GSum = src.CVector[src_i][(src_j)*3 + 1];
          RSum = src.CVector[src_i][(src_j)*3 + 2];
        }
      }
      uint8_t *pix = &(dst.CVector[i - Y][(j - X) * 3 + 0]);
      pix[0] = BSum;
      pix[1] = GSum;
      pix[2] = RSum;
    }
  }
}


cJSON *cJSON_DirFiles(const char *path, cJSON *jObj_to_W, int depth = 0)
{
  if (path == NULL)
    return NULL;

  DIR *d = opendir(path);

  if (d == NULL)
    return NULL;

  cJSON *retObj = (jObj_to_W == NULL) ? cJSON_CreateObject() : jObj_to_W;
  struct dirent *dir;
  cJSON *dirFiles = cJSON_CreateArray();
  char buf[PATH_MAX + 1];
  realfullPath(path, buf);

  cJSON_AddStringToObject(retObj, "path", buf);
  cJSON_AddItemToObject(retObj, "files", dirFiles);

  std::string folderPath(buf);

  if (depth > 0)
    while ((dir = readdir(d)) != NULL)
    {
      //if(dir->d_name[0]=='.')continue;
      cJSON *fileInfo = cJSON_CreateObject();
      cJSON_AddItemToArray(dirFiles, fileInfo);
      cJSON_AddStringToObject(fileInfo, "name", dir->d_name);

      char *type = NULL;
      std::string fileName(dir->d_name);
      std::string filePath = folderPath + "/" + fileName;

      switch (dir->d_type)
      {
      case DT_REG:
        type = "REG";
        break;
      case DT_DIR:
      {
        if (depth > 0 && dir->d_name != NULL && dir->d_name[0] != '\0' && dir->d_name[0] != '.')
        {
          cJSON *subFolderStruct = cJSON_DirFiles(filePath.c_str(), NULL, depth - 1);
          if (subFolderStruct != NULL)
          {
            cJSON_AddItemToObject(fileInfo, "struct", subFolderStruct);
          }
        }
        type = "DIR";
        break;
      }
      // case DT_FIFO:
      // case DT_SOCK:
      // case DT_CHR:
      // case DT_BLK:
      // case DT_LNK:
      case DT_UNKNOWN:
      default:
        type = "UNKNOWN";
        break;
      }
      cJSON_AddStringToObject(fileInfo, "type", type);

      struct stat st;
      if (stat(filePath.c_str(), &st) == 0)
      {
        cJSON_AddNumberToObject(fileInfo, "size_bytes", st.st_size);
        cJSON_AddNumberToObject(fileInfo, "mtime_ms", st.st_mtime * 1000);
        cJSON_AddNumberToObject(fileInfo, "ctime_ms", st.st_ctime * 1000);
        cJSON_AddNumberToObject(fileInfo, "atime_ms", st.st_atime * 1000);
      }
    }
  closedir(d);

  return retObj;
}

machine_hash machine_h = {0};
void AttachStaticInfo(cJSON *reportJson, DatCH_CallBack_BPG *cb)
{
  if (reportJson == NULL)
    return;
  char tmpStr[128];

  {
    char *tmpStr_ptr = tmpStr;
    for (int i = 0; i < sizeof(machine_h.machine); i++)
    {
      tmpStr_ptr += sprintf(tmpStr_ptr, "%02X", machine_h.machine[i]);
    }
    cJSON_AddStringToObject(reportJson, "machine_hash", tmpStr);

    if (cb && cb->cameraFramesLeft >= 0)
    {
      LOGI("cb->cameraFramesLeft:%d", cb->cameraFramesLeft);
      cJSON_AddNumberToObject(reportJson, "frames_left", cb->cameraFramesLeft);
    }
  }
}
// int backPackLoad(FeatureManager_BacPac &calib_bacpac,cJSON *from)
// {
// }

// int backPackDump(FeatureManager_BacPac &calib_bacpac,cJSON *dumoTo)
// {
// }

BGLightNodeInfo extractInfoFromJson(cJSON *nodeRoot) //have exception
{
  if (nodeRoot == NULL)
  {
    char ExpMsg[100];
    sprintf(ExpMsg, "ERROR: extractInfoFromJson error, nodeRoot is NULL");
    throw std::runtime_error(ExpMsg);
  }

  BGLightNodeInfo info;
  info.location.X = *JFetEx_NUMBER(nodeRoot, "location.x");
  info.location.Y = *JFetEx_NUMBER(nodeRoot, "location.y");
  info.index.X = (int)*JFetEx_NUMBER(nodeRoot, "index.x");
  info.index.Y = (int)*JFetEx_NUMBER(nodeRoot, "index.y");

  info.sigma = *JFetEx_NUMBER(nodeRoot, "sigma");
  info.samp_rate = *JFetEx_NUMBER(nodeRoot, "samp_rate");
  info.mean = *JFetEx_NUMBER(nodeRoot, "mean");
  info.error = *JFetEx_NUMBER(nodeRoot, "error");

  return info;
}

int loadCameraCalibParam(char *dirName, cJSON *root, ImageSampler *ret_param)
{

  if (ret_param == NULL)
    return -1;  
  if (root == NULL)
    return -1;

  cJSON *angledOffsetObj = JFetEx_OBJECT(root, "reports[0].angledOffset");
  ret_param->getAngOffsetTable()->RESET();
  if (angledOffsetObj != NULL)
  {

    float mult = 1;
    double *p_mult = JFetch_NUMBER(angledOffsetObj, "mult");
    if (p_mult)
    {
      mult = *p_mult;
    }

    ret_param->getAngOffsetTable()->RESET();
    cJSON *current_element = angledOffsetObj->child;

    for (cJSON *current_element = angledOffsetObj->child;
         current_element != NULL;
         current_element = current_element->next)
    {
      float tagNum;
      char *name = current_element->string;
      if (sscanf(name, "%f", &tagNum) != 1)
        continue;
      if (tagNum < 0 && tagNum >= 360)
        continue;
      double *val = JFetch_NUMBER(angledOffsetObj, name);
      if (val == NULL)
        continue;

      angledOffsetG newPair = {//angle in rad
                               angle_rad : (float)(tagNum * M_PI / 180),
                               offset : mult * (float)*val
      };
      ret_param->getAngOffsetTable()->push_back(newPair);
    }

    void *target;
    int type = getDataFromJson(angledOffsetObj, "symmetric", &target);
    if (type == cJSON_True)
    {
      ret_param->getAngOffsetTable()->makeSymmetic();
    }

    double *preOffset = JFetch_NUMBER(angledOffsetObj, "preOffset");
    if (preOffset)
    {
      ret_param->getAngOffsetTable()->applyPreOffset(*preOffset);
    }

    angledOffsetTable *angOffsetTable = ret_param->getAngOffsetTable();
    int testN = 100;

    LOGI("angOffsetTable.size:%d", angOffsetTable->size());
    // for (int i = 0; i < testN; i++)
    // {
    //   float angle = 2 * M_PI * i / testN;
    //   float offset = angOffsetTable->sampleAngleOffset(angle);
    //   LOGI("a:%f o:%f", angle * 180 / M_PI, offset);
    // }
    //exit(-1);
  }

  //ret_param->mmpp = *JFetEx_NUMBER(root, "reports[0].mmpb2b") / (*JFetEx_NUMBER(root, "reports[0].ppb2b"));
  if(JFetEx_NUMBER(root, "reports[0].mmpb2b")!=NULL)
  {
    ret_param->getCalibMap()->calibmmpB=*JFetEx_NUMBER(root, "reports[0].mmpb2b");
    LOGI("Override calibmmpB:%f",ret_param->getCalibMap()->calibmmpB);
  }

  do
  {

    stageLightParam *stageLightInfo = ret_param->getStageLightInfo();
    char default_SLCalibPath[] = "stageLightReport.json";
    char *SLCalibPath = JFetch_STRING(root, "reports[0].StageLightReportPath");
    if (SLCalibPath == NULL)
      SLCalibPath = default_SLCalibPath;
    char path[200];
    sprintf(path, "%s/%s", dirName, SLCalibPath);
    SLCalibPath = path;
    LOGE("SLCalibPath:%s", SLCalibPath);

    char *fileStr = ReadText(SLCalibPath);

    if (fileStr == NULL)
    {
      LOGE("Cannot read defFile from:%s", SLCalibPath);
      break;
    }

    //LOGE("fileStr:%s",fileStr);

    cJSON *sl_json = cJSON_Parse(fileStr);

    free(fileStr);
    if (sl_json == NULL)
    {
      LOGE("File:%s is not a JSON...", SLCalibPath);
      break;
    }

    stageLightInfo->RESET();
    double *expTime_us = JFetch_NUMBER(sl_json, "cam_param.exposure_time");

    if (expTime_us)
    {
      stageLightInfo->exposure_us = *expTime_us;
    }

    double *imDimW = JFetch_NUMBER(sl_json, "target_image_dim.x");
    double *imDimH = JFetch_NUMBER(sl_json, "target_image_dim.y");

    if (imDimW && imDimH)
    {
      stageLightInfo->tarImgW = *imDimW;
      stageLightInfo->tarImgH = *imDimH;
    }

    stageLightInfo->back_light_target=200;//Default
    double *p_back_light_target = JFetch_NUMBER(sl_json, "cam_param.back_light_target");
    if(p_back_light_target)
    {
      stageLightInfo->back_light_target=*p_back_light_target;
    }

    stageLightInfo->BG_nodes.clear();
    int idx = 0;
    while (true)
    {
      char jsonKey[100];
      sprintf(jsonKey, "grid_info[%d]", idx);
      idx++;
      cJSON *nodeObj = JFetch_OBJECT(sl_json, jsonKey);
      if (nodeObj == NULL)
        break;
      BGLightNodeInfo info;
      try
      {

        info = extractInfoFromJson(nodeObj);
      }
      catch (...)
      {
        stageLightInfo->BG_nodes.clear();
        break;
      }

      stageLightInfo->BG_nodes.push_back(info);

      // LOGI("node[%d]:mean:%f idx:%d:%d  xy:%f,%f  size:%d",
      //   idx,info.mean,idx,info.index.X,idx,info.index.Y,info.location.X,idx,info.location.Y,stageLightInfo->BG_nodes.size());
    }

    LOGE("exposure_time:");
    stageLightInfo->nodesIdxWHSetup();

  } while (false);
  return 0;
}

int CameraSetup(CameraLayer &camera, cJSON &settingJson)
{
  double *val = JFetch_NUMBER(&settingJson, "exposure");
  int retV = -1;
  if (val)
  {
    camera.SetExposureTime(*val);
    LOGI("SetExposureTime:%f", *val);
    retV = 0;
  }
  val = JFetch_NUMBER(&settingJson, "gain");
  if (val)
  {
    camera.SetAnalogGain((int)*val);
    LOGI("SetAnalogGain:%f", *val);
    retV = 0;
  }

  val = JFetch_NUMBER(&settingJson, "framerate_mode");
  if (val)
  {
    *val = (int)*val;
    camera.SetFrameRateMode((int)*val);
    LOGI("framerate_mode:%f", *val);
    retV = 0;
  }


  if ( getDataFromJson(&settingJson, "set_once_WB",NULL)==cJSON_True)
  {
    CameraLayer::status  st= camera.SetOnceWB();
    LOGI("SetOnceWB:%d", st);
    retV = 0;
  }


  int type = getDataFromJson(&settingJson, "down_samp_w_calib", NULL);


  cJSON *MIRROR = JFetch_ARRAY(&settingJson, "mirror");

  if (MIRROR)
  { //ROI set
    int mirrorX = 0;
    int mirrorY = 0;
    double *_mirrorX = JFetch_NUMBER(&settingJson, "mirror[0]");
    if (_mirrorX)
    {
      mirrorX = (int)*_mirrorX;
    }
    double *_mirrorY = JFetch_NUMBER(&settingJson, "mirror[1]");
    if (_mirrorY)
    {
      mirrorY = (int)*_mirrorY;
    }

    camera.SetMirror(0, mirrorX);
    camera.SetMirror(1, mirrorY);
  }

  if (JFetch_ARRAY(&settingJson, "ROI_mirror"))
  { //ROI set
    int mirrorX = 0;
    int mirrorY = 0;
    double *_mirrorX = JFetch_NUMBER(&settingJson, "ROI_mirror[0]");
    if (_mirrorX)
    {
      mirrorX = (int)*_mirrorX;
    }
    double *_mirrorY = JFetch_NUMBER(&settingJson, "ROI_mirror[1]");
    if (_mirrorY)
    {
      mirrorY = (int)*_mirrorY;
    }

    camera.SetROIMirror(0, mirrorX);
    camera.SetROIMirror(1, mirrorY);
  }


  cJSON *ROISetting = JFetch_ARRAY(&settingJson, "ROI");

  if (ROISetting)
  { //ROI set
    double *roi_x = JFetch_NUMBER(&settingJson, "ROI[0]");
    double *roi_y = JFetch_NUMBER(&settingJson, "ROI[1]");
    double *roi_w = JFetch_NUMBER(&settingJson, "ROI[2]");
    double *roi_h = JFetch_NUMBER(&settingJson, "ROI[3]");
    LOGI("ROI ptr:%p %p %p %p", roi_x, roi_y, roi_w, roi_h);
    if (roi_x && roi_y && roi_w && roi_h && ((*roi_w)* (*roi_h))!=0)
    {
      camera.SetROI(*roi_x, *roi_y, *roi_w, *roi_h, 0, 0);
      float ox,oy;
      camera.GetROI(&ox,&oy, NULL,NULL,NULL,NULL);
      acv_XY offset_o={ox,oy};
      //sampler
      
    }
    else
    {
    }
  }
  return 0;
}

int LoadCameraSetting(CameraLayer &camera, char *filename)
{
  char *fileStr = ReadText(filename);

  if (fileStr == NULL)
  {
    LOGE("Cannot read defFile from:%s", filename);
    return -1;
  }

  cJSON *json = cJSON_Parse(fileStr);

  free(fileStr);
  if (json == NULL)
  {
    LOGE("File:%s is not a JSON...", filename);
    return -1;
  }

  int ret = CameraSetup(camera, *json);
  cJSON_Delete(json);
  return ret;
}

int LoadCameraCalibrationFile(char *filename, ImageSampler *ret_cam_param)
{
  char *fileStr = ReadText(filename);

  if (fileStr == NULL)
  {
    LOGE("Cannot read defFile from:%s", filename);
    return -1;
  }
  LOGE("Read defFile from:%s", filename);

  cJSON *json = cJSON_Parse(fileStr);

  free(fileStr);
  bool executionError = false;

  try
  {
    char folder_name[200];
    int strLen = strlen(filename);
    strcpy(folder_name, filename);
    for (int i = strLen; i; i--) //Find folder name
    {
      if (folder_name[i] == '/')
      {
        folder_name[i + 1] = '\0';
        break;
      }
    }

    //json

    int ret = loadCameraCalibParam(folder_name, json, ret_cam_param);
    if (ret)
      executionError = true;
  }
  catch (const std::exception &ex)
  {
    LOGE("Exception:%s", ex.what());
    executionError = true;
  }
  cJSON_Delete(json);

  if (executionError)
    return -1;
  return 0;
}

bool DoImageTransfer = true;

int MicroInsp_FType::recv_json(char *json_str, int json_strL)
{
  MT_LOCK();
  int fd = getfd();

  DatCH_Data datCH_BPG =
      BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

  char tmp[1024];
  sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s,\"CONN_ID\":%d}", json_str, fd);
  LOGI("MSG:%s", tmp);

  BPG_data bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("PD", tmp);
  datCH_BPG.data.p_BPG_data = &bpg_dat;
  BPG_protocol_send(datCH_BPG);
  MT_UNLOCK();
  return 0;
}

int MicroInsp_FType::ev_on_close()
{
  
  //MT_LOCK(); //the delete caller might come within main thread
  int fd = getfd();
  LOGE("fd:%d is disconnected", fd);
  DatCH_Data datCH_BPG =
      BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

  char tmp[70];
  sprintf(tmp, "{\"type\":\"DISCONNECT\",\"CONN_ID\":%d}", getfd());

  BPG_data bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("PD", tmp);
  datCH_BPG.data.p_BPG_data = &bpg_dat;
  BPG_protocol_send(datCH_BPG);

  //MT_UNLOCK();

  return 0;
}

bool DatCH_CallBack_BPG::checkTL(const char *TL, const BPG_data *dat)
{
  if (TL == NULL)
    return false;
  return (TL[0] == dat->tl[0] && TL[1] == dat->tl[1]);
}
uint16_t DatCH_CallBack_BPG::TLCode(const char *TL)
{
  return (((uint16_t)TL[0] << 8) | TL[1]);
}
DatCH_CallBack_BPG::DatCH_CallBack_BPG(DatCH_BPG1_0 *self)
{
  this->self = self;
  cacheImage.ReSize(1, 1);
}

void DatCH_CallBack_BPG::delete_Ext_Util_API()
{
  if (exApi)
  {
    delete exApi;
    exApi = NULL;
  }
}
void DatCH_CallBack_BPG::delete_MicroInsp_FType()
{

  if (mift)
  {
    LOGI("DELETING");
    delete mift;
    mift = NULL;
  }
  LOGI("DELETED...");
}


acvImage * getImage(CameraLayer *camera)
{
  for(int i=0;;i++)
  {
    if(camera->SnapFrame()==CameraLayer::ACK)break;
    if(i>10)return NULL;
  }
  return camera->GetFrame();
}


BPG_data DatCH_CallBack_BPG::GenStrBPGData(char *TL, char *jsonStr)
{
  BPG_data BPG_dat = {0};
  BPG_dat.tl[0] = TL[0];
  BPG_dat.tl[1] = TL[1];
  if (jsonStr == NULL)
  {
    BPG_dat.size = 0;
  }
  else
  {
    BPG_dat.size = strlen(jsonStr);
  }
  BPG_dat.dat_raw = (uint8_t *)jsonStr;

  return BPG_dat;
}
int DatCH_CallBack_BPG::callback(DatCH_Interface *from, DatCH_Data data, void *callback_param)
{
  //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);
  bool doExit=false;
  switch (data.type)
  {
  case DatCH_Data::DataType_error:
  {
    LOGE("error code:%d..........", data.data.error.code);
  }
  break;

  //Connection layer of the BPG protocol
  case DatCH_Data::DataType_websock_data: //App -(prot)>[here] WS //Final stage of outcoming data
  {
    DatCH_Data ret = websocket->SendData(data);
  }
  break;

  case DatCH_Data::DataType_BPG: // WS -(prot)>[here] App //Final stage of incoming data
  {
    BPG_data *dat = data.data.p_BPG_data;

    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //      dat->pgID);
    cJSON *json = cJSON_Parse((char *)dat->dat_raw);
    char err_str[100] = "\0";
    bool session_ACK = false;
    char tmp[200];    //For string construct json reply
    BPG_data bpg_dat; //Empty


    bpg_dat.pgID = dat->pgID;
    

    if(checkTL("GS", dat)==false)
      LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
          dat->pgID);
    if      (checkTL("HR", dat))
    {
      LOGI("DataType_BPG>>>>%s", dat->dat_raw);

      LOGI("Hello ready.......");
      session_ACK = true;
    }

    DatCH_Data datCH_BPG =
        BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

    sprintf(tmp, "{\"start\":false,\"cmd\":\"%c%c\",\"ACK\":%s,\"errMsg\":\"%s\"}",
            dat->tl[0], dat->tl[1], (session_ACK) ? "true" : "false", err_str);
    bpg_dat = GenStrBPGData("SS", tmp);
    bpg_dat.pgID = dat->pgID;
    datCH_BPG.data.p_BPG_data = &bpg_dat;
    self->SendData(datCH_BPG);

    MT_UNLOCK();
    cJSON_Delete(json);
  }
  break;

  default:
    LOGI("type:%d, UNKNOWN type", data.type);
  }

  if(doExit)
  {
    exit(0);
  }

  return 0;
}

int str_ends_with(const char *str, const char *suffix)
{
  if (!str || !suffix)
    return 0;
  size_t lenstr = strlen(str);
  size_t lensuffix = strlen(suffix);
  if (lensuffix > lenstr)
    return 0;
  return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

void CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void *context)
{
  
  LOGI("===============\n");
  if (type != CameraLayer::EV_IMG)
    return;
  
}

void ImgPipeProcessCenter_imp(image_pipe_info *imgPipe)
{
}

void ImgPipeProcessThread(bool *terminationflag)
{

}

int DatCH_CallBack_WSBPG::DatCH_WS_callback(DatCH_Interface *ch_interface, DatCH_Data data, void *callback_param)
{
  //first stage of incoming data
  //and first stage of outcoming data if needed
  if (data.type != DatCH_Data::DataType_websock_data)
    return -1;
  DatCH_WebSocket *ws = (DatCH_WebSocket *)callback_param;
  websock_data ws_data = *data.data.p_websocket;
  // LOGI("SEND>>>>>>..websock_data..\n");
  if ((BPG_protocol->MatchPeer(NULL) || BPG_protocol->MatchPeer(ws_data.peer)))
  {
    // LOGI("SEND>>>>>>..MatchPeer..\n");
    
    BPG_protocol->SendData(data);
    //BPG_protocol_send(data); // WS [here]-(prot)> App
  }

  switch (ws_data.type)
  {
  case websock_data::eventType::OPENING:
    printf("OPENING peer %s:%d  sock:%d\n",
           inet_ntoa(ws_data.peer->getAddr().sin_addr),
           ntohs(ws_data.peer->getAddr().sin_port), ws_data.peer->getSocket());
    if (ws->default_peer == NULL)
    {
      ws->default_peer = ws_data.peer;
    }
    else
    {
    }
    break;

  case websock_data::eventType::HAND_SHAKING_FINISHED:

    LOGI("HAND_SHAKING: host:%s orig:%s key:%s res:%s\n",
         ws_data.data.hs_frame.host,
         ws_data.data.hs_frame.origin,
         ws_data.data.hs_frame.key,
         ws_data.data.hs_frame.resource);

    if (ws->default_peer == ws_data.peer)
    {
      LOGI("SEND>>>>>>..HAND_SHAKING_FINISHED..\n");
      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      LOGI("SEND>>>>>>..GenMsgType..\n");
      BPG_data BPG_dat;
      datCH_BPG.data.p_BPG_data = &BPG_dat;
      BPG_dat.tl[0] = 'H';
      BPG_dat.tl[1] = 'R';
      char tmp[] = "{\"AA\":5}";
      BPG_dat.size = sizeof(tmp) - 1;
      BPG_dat.dat_raw = (uint8_t *)tmp;
      //App [here]-(prot)> WS
      BPG_protocol_send(datCH_BPG);
    }
    else
    {
      ws->disconnect(ws_data.peer->getSocket());
    }
    break;
  case websock_data::eventType::DATA_FRAME:
    // printf("DATA_FRAME >> frameType:%d frameL:%d data_ptr=%p\n",
    //        ws_data.data.data_frame.type,
    //        ws_data.data.data_frame.rawL,
    //        ws_data.data.data_frame.raw);

    break;
  case websock_data::eventType::CLOSING:

    printf("CLOSING peer %s:%d\n",
           inet_ntoa(ws_data.peer->getAddr().sin_addr), ntohs(ws_data.peer->getAddr().sin_port));
    cb->cameraFramesLeft = 0;
    camera->TriggerMode(1);
    cb->delete_MicroInsp_FType();
    cb->delete_Ext_Util_API();

    break;
  default:
    return -1;
  }
  return 0;
}


int DatCH_CallBack_WSBPG::callback(DatCH_Interface *from, DatCH_Data data, void *callback_param)
{

  // LOGI("DatCH_CallBack_WSBPG:_______type:%d________", data.type);
  int ret_val = 0;
  switch (data.type)
  {
  case DatCH_Data::DataType_error:
  {
    LOGE("error code:%d..........", data.data.error.code);
  }
  break;
  case DatCH_Data::DataType_BMP_Read:
  {

    //acvImage *test1 = data.data.BMP_Read.img;

    //ImgInspection(matchingEng,test1,&test1_buff,1,"data/target.json");
  }
  break;

  case DatCH_Data::DataType_websock_data:
    ret_val = DatCH_WS_callback(from, data, callback_param);
    break;

  default:

    LOGI("type:%d, UNKNOWN type", data.type);
  }

  return ret_val;
}


int initCamera(CameraLayer_GIGE_MindVision *CL_GIGE)
{

  tSdkCameraDevInfo sCameraList[10];
  int retListL = sizeof(sCameraList) / sizeof(sCameraList[0]);
  CL_GIGE->EnumerateDevice(sCameraList, &retListL);

  if (retListL <= 0)
    return -1;
  for (int i = 0; i < retListL; i++)
  {
    printf("CAM:%d======\n", i);
    printf("acDriverVersion:%s\n", sCameraList[i].acDriverVersion);
    printf("acFriendlyName:%s\n", sCameraList[i].acFriendlyName);
    printf("acLinkName:%s\n", sCameraList[i].acLinkName);
    printf("acPortType:%s\n", sCameraList[i].acPortType);
    printf("acProductName:%s\n", sCameraList[i].acProductName);
    printf("acProductSeries:%s\n", sCameraList[i].acProductSeries);
    printf("acSensorType:%s\n", sCameraList[i].acSensorType);
    printf("acSn:%s\n", sCameraList[i].acSn);
    printf("\n\n\n\n");
  }

  if (CL_GIGE->InitCamera(&(sCameraList[0])) == CameraLayer::ACK)
  {
    return 0;
  }
  return -1;
}
int initCamera(CameraLayer_BMP_carousel *CL_bmpc)
{
  return CL_bmpc == NULL ? -1 : 0;
}

CameraLayer *getCamera(int initCameraType=0)
{

  CameraLayer *camera = NULL;
  if (initCameraType == 0 || initCameraType == 1)
  {
    CameraLayer_GIGE_MindVision *camera_GIGE;
    camera_GIGE = new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV, NULL);
    LOGV("initCamera");

    try
    {
      if (initCamera(camera_GIGE) == 0)
      {
        camera = camera_GIGE;
      }
      else
      {
        delete camera;
        camera = NULL;
      }
    }
    catch (std::exception &e)
    {
      delete camera;
      camera = NULL;
    }
  }
  LOGI("camera ptr:%p", camera);

  if (camera == NULL && (initCameraType == 0 || initCameraType == 2))
  {
    CameraLayer_BMP_carousel *camera_BMP;
    LOGV("CameraLayer_BMP_carousel");
    camera_BMP = new CameraLayer_BMP_carousel(CameraLayer_Callback_GIGEMV, NULL, "data/BMP_carousel_test");
    camera = camera_BMP;
  }

  if (camera == NULL)
  {
    return NULL;
  }

  LOGV("TriggerMode(1)");
  camera->TriggerMode(1);
  camera->SetExposureTime(12570.5110);
  camera->SetAnalogGain(1);

  // LOGV("Loading data/default_camera_setting.json....");
  // int ret = LoadCameraSetting(*camera, "data/default_camera_setting.json");
  // LOGV("ret:%d",ret);
  return camera;
}


bool terminationFlag=false;
int mainLoop(bool realCamera = false)
{
  /**/

  LOGI(">>>>>\n");
  bool pass = false;
  int retryCount = 0;
  while (!pass && !terminationFlag)
  {
    try
    {
      int port = 4090;
      LOGI("Try to open websocket... port:%d\n", port);
      websocket = new DatCH_WebSocket(port);
      pass = true;
    }
    catch (std::exception &e)
    {
      retryCount++;
      int delaySec = 5;
      LOGE("websocket server open retry:%d wait for %dsec", retryCount, delaySec);
      std::this_thread::sleep_for(std::chrono::milliseconds(delaySec * 1000));
    }
  }

  if (terminationFlag)
    return -1;
  std::thread mThread(ImgPipeProcessThread, &terminationFlag);
  LOGI(">>>>>\n");

  {

    CameraLayer *camera = getCamera(0);

    for (int i = 0; camera == NULL; i++)
    {
      LOGI("Camera init retry[%d]...", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      camera = getCamera(0);
    }
    LOGI("DatCH_BPG1_0 camera :%p", camera);

    cb->camera = camera;
    callbk_obj.camera = camera;

    BPG_protocol->SetEventCallBack(cb, NULL);
  }
  LOGI("Camera:%p", cb->camera);

  websocket->SetEventCallBack(&callbk_obj, websocket);

  LOGI("SetEventCallBack is set...");
  while (websocket->runLoop(NULL) == 0)
  {
  }

  return 0;
}


void sigroutine(int dunno)
{
  switch (dunno)
  {
  case SIGINT:
    LOGE("Get a signal -- SIGINT \n");
    LOGE("Tear down websocket.... \n");
    delete websocket;
    // terminationFlag = true;
    break;
  }
  return;
}

#include <vector>
int cp_main(int argc, char **argv)
{
  srand(time(NULL));

  // calib_bacpac.sampler = new ImageSampler();
  // neutral_bacpac.sampler = new ImageSampler();
#ifdef __WIN32__
  {
    WSADATA wsaData;
    int iResult;
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
      printf("WSAStartup failed with error: %d\n", iResult);
      return 1;
    }

    LOGI("WIN32 WSAStartup ret:%d", iResult);
  }
#endif

  _argc = argc;
  _argv = argv;
  for (int i = 0; i < argc; i++)
  {
    bool doMatch = true;
    {
      doMatch = false;
      LOGE("unknown param[%d]:%s", i, argv[i]);
    }

    if (doMatch)
    {
      LOGE("CMD param[%d]:%s ...OK", i, argv[i]);
    }
  }
  
  signal(SIGINT, sigroutine);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
