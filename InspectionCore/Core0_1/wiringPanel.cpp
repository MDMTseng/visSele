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
#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>

#include <ctime>

std::timed_mutex mainThreadLock;

bool saveInspFailSnap = true;
bool saveInspNASnap = true;

cJSON *cache_deffile_JSON = NULL;

cJSON *cache_camera_param = NULL;

const std::string InspSampleSavePath_DEFAULT("data/SAMPLE");
std::string InspSampleSavePath = InspSampleSavePath_DEFAULT;

int resourcePoolSize = 30;
TSQueue<image_pipe_info *> inspQueue(10);
TSQueue<image_pipe_info *> actionQueue(10);
TSQueue<image_pipe_info *> inspSnapQueue(5);
#define MT_LOCK(...) mainThreadLock_lock(__LINE__ VA_ARGS(__VA_ARGS__))
#define MT_UNLOCK(...) mainThreadLock_unlock(__LINE__ VA_ARGS(__VA_ARGS__))

void setThreadPriority(std::thread &thread, int type, int priority)
{

  sched_param sch;
  int policy;
  pthread_getschedparam(thread.native_handle(), &policy, &sch);
  sch.sched_priority = priority;
  if (pthread_setschedparam(thread.native_handle(), type, &sch))
  {
    // std::cout << "Failed to setschedparam: " << std::strerror(errno) << '\n';
  }
}

int mainThreadLock_lock(int call_lineNumber, char *msg = "", int try_lock_timeout_ms = 0)
{

  if (try_lock_timeout_ms <= 0)
  {
    //LOGI("%s_%d: Locking ",msg,call_lineNumber);
    mainThreadLock.lock();
  }
  else
  {
    using Ms = std::chrono::milliseconds;

    //LOGI("%s_%d: Locking %dms",msg,call_lineNumber,try_lock_timeout_ms);
    if (mainThreadLock.try_lock_for(Ms(try_lock_timeout_ms)))
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

int mainThreadLock_unlock(int call_lineNumber, char *msg = "")
{

  //LOGI("%s_%d: unLocking ",msg,call_lineNumber);
  mainThreadLock.unlock();
  //LOGI("%s_%d: unLocked ",msg,call_lineNumber);

  return 0;
}

class ImageStackAddUp
{
public:
  int stackingC = 0;
  acvImage imgStacked;
  acvImage imgExtract;

  void acvImageAddUp_1CH(acvImage *imgOut, acvImage *imgIn)
  {
    for (int i = 0; i < imgOut->GetHeight(); i++)
    {
      for (int j = 0; j < imgOut->GetWidth(); j++)
      {
        // 3 channels will be used for number over flow
        _24BitUnion *pixU = (_24BitUnion *)(imgOut->CVector[i] + j * 3);
        pixU->_3Byte.Num += imgIn->CVector[i][j * 3];
      }
    }
  }

  void acvImageSet_1CH(acvImage *imgOut, acvImage *imgIn)
  {
    for (int i = 0; i < imgOut->GetHeight(); i++)
    {
      for (int j = 0; j < imgOut->GetWidth(); j++)
      {
        // 3 channels will be used for number over flow
        _24BitUnion *pixU = (_24BitUnion *)(imgOut->CVector[i] + j * 3);
        pixU->_3Byte.Num = imgIn->CVector[i][j * 3];
      }
    }
  }
  void acvImageClear(acvImage *imgOut)
  {
    memset((void *)imgOut->CVector[0], 0, imgOut->GetHeight() * imgOut->GetWidth() * 3);
  }

  void ReSize(acvImage *ref)
  {
    imgStacked.ReSize(ref);
    Reset();
  }

  void Reset()
  {
    stackingC = 0;
    //acvImageClear(&imgStacked);
  }

  void Add(acvImage *imgIn)
  {
    if (stackingC == 0)
    {
      acvImageSet_1CH(&imgStacked, imgIn);
      stackingC++;
      return;
    }
    if (stackingC < 100)
    {
      acvImageAddUp_1CH(&imgStacked, imgIn);
      stackingC++;
    }
  }

  void Export(acvImage *imgOut)
  {
    imgOut->ReSize(&imgStacked);
    for (int i = 0; i < imgOut->GetHeight(); i++)
    {
      for (int j = 0; j < imgOut->GetWidth(); j++)
      {
        // 3 channels will be used for number over flow

        _24BitUnion *pixU = (_24BitUnion *)(imgStacked.CVector[i] + j * 3);
        int pix = pixU->_3Byte.Num / stackingC;
        if (pix > 255)
          pix = 255;
        imgOut->CVector[i][j * 3] = pix;
        imgOut->CVector[i][j * 3 + 1] = pix;
        imgOut->CVector[i][j * 3 + 2] = pix;
      }
    }
  }

  void Export()
  {
    Export(&imgExtract);
  }

  bool DiffBigger(acvImage *img2, float globalDiffThres, int localDiffThres, int skipSampling = 10)
  {
    if (skipSampling < 1)
      skipSampling = 1;

    globalDiffThres *= globalDiffThres * (imgStacked.GetHeight() * imgStacked.GetWidth() / skipSampling / skipSampling);

    localDiffThres *= localDiffThres;

    // LOGI("%d %d   %d %d",imgStacked.GetHeight(),imgStacked.GetWidth(),img2->GetHeight(),img2->GetWidth());
    uint64_t diffSum = 0;
    int diffMax = 0;
    int count = 0;
    for (int i = 0; i < imgStacked.GetHeight(); i += skipSampling)
    {
      for (int j = 0; j < imgStacked.GetWidth(); j += skipSampling)
      {

        _24BitUnion *pixU = (_24BitUnion *)(imgStacked.CVector[i] + j * 3);
        // LOGI("im(%d,%d)",j,i);
        int pix = stackingC == 0 ? 0 : (pixU->_3Byte.Num / stackingC);

        count++;

        // LOGI("im(%d,%d)=%d",j,i,pix);
        int diff = pix - img2->CVector[i][j * 3];
        diff *= diff;
        diffSum += diff;
        if (diffSum > globalDiffThres)
        {
          return true;
        }
        if (diffMax < diff)
        {
          diffMax = diff;
          if (diffMax > localDiffThres)
          {
            return true;
          }
        }
      }
    }

    return false;
  }
};

ImageStackAddUp imstack;

DatCH_WebSocket *websocket = NULL;
MJPEG_Streamer *mjpegS;
MatchingEngine matchingEng;
CameraLayer *gen_camera;
DatCH_CallBack_WSBPG callbk_obj;
int CamInitStyle = 0;

int downSampLevel = 4;

int ImageCropX = 0;
int ImageCropY = 0;
int ImageCropW = 99999;
int ImageCropH = 99999;
bool downSampWithCalib = false;

bool doImgProcessThread = true;
bool doInspActionThread = true;
int parseCM_info(PerifProt::Pak pakCM, acvCalibMap *setObj);
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

//lens1
//main.cpp  1067 main:v K: 1.00096 -0.00100092 -9.05316e-05 RNormalFactor:1296
//main.cpp  1068 main:v Center: 1295,971

//main.cpp  1075 main:v K: 0.999783 0.00054474 -0.000394607 RNormalFactor:1296
//main.cpp  1076 main:v Center: 1295,971

//lens2
//main.cpp  1061 main:v K: 0.989226 0.0101698 0.000896734 RNormalFactor:1296
//main.cpp  1062 main:v Center: 1295,971

FeatureManager_BacPac calib_bacpac = {0};
FeatureManager_BacPac neutral_bacpac = {0};
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

int CameraSettingFromFile(CameraLayer *camera, char *path);

CameraLayer *getCamera(int initCameraType); //0 for real First, then fake one, 1 for real camera only, 2 for fake only
int ImgInspection_JSONStr(MatchingEngine &me, acvImage *test1, int repeatTime, char *jsonStr, FeatureManager_BacPac *bacpac);

int ImgInspection_DefRead(MatchingEngine &me, acvImage *test1, int repeatTime, char *defFilename, FeatureManager_BacPac *bacpac);

typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst, size_t dstLen, uint8_t *src, size_t srcLen);

void ImageDownSampling(acvImage &dst, acvImage &src, int downScale, ImageSampler *sampler, int doNearest = 1,
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
  // LOGI("X:%d Y:%d X2:%d Y2:%d", X, Y, X2, Y2);
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

          // if(i==(Y2/2) && j>200 && j< 400)
          // {
            
          //   float coord2[] = {coord[0], coord[1]};
          //   // int ret = sampler->ideal2img(coord2);
          //   bri = sampler->sampleImage_IdealCoord(&src, coord, doNearest);
            
          //   LOGI("X:%d,%d bri:%f %f,%f=>%f,%f  samp:%p", j, i,bri,coord2[0],coord2[1],coord[0],coord[1],sampler);
          // }else
          // {

          // }

          bri = sampler->sampleImage_IdealCoord(&src, coord, doNearest);

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

  {
    char default_CalibMapPath[] = "CalibMap.bin";
    char *calibMapPath = JFetch_STRING(root, "reports[0].CalibMapPath");
    if (calibMapPath == NULL)
      calibMapPath = default_CalibMapPath;
    char path[200];
    sprintf(path, "%s/%s", dirName, calibMapPath);
    calibMapPath = path;
    LOGE("calibMapPath:%s", calibMapPath);
    int datL = 0;
    uint8_t *bDat = ReadByte(calibMapPath, &datL);
    ret_param->getCalibMap()->RESET();
    if (bDat)
    {
      int count = PerifProt::countValidArr(bDat, datL);
      LOGI("PerifProt::countValidArr  count:%d", count);
      if (count < 0)
      {
        throw new std::runtime_error("ReadByte return NULL");
      }
      PerifProt::Pak p1 = PerifProt::parse(bDat);
      int pret = parseCM_info(p1, ret_param->getCalibMap());
      LOGI("parseCM_info::ret:%d", pret);

      delete bDat;
    }
    else
    {
      //throw new std::runtime_error("ReadByte return NULL");
      //return -1;
    }
  }

  //ret_param->mmpp = *JFetEx_NUMBER(root, "reports[0].mmpb2b") / (*JFetEx_NUMBER(root, "reports[0].ppb2b"));
  if (JFetEx_NUMBER(root, "reports[0].mmpb2b") != NULL)
  {
    ret_param->getCalibMap()->calibmmpB = *JFetEx_NUMBER(root, "reports[0].mmpb2b");
    LOGI("Override calibmmpB:%f", ret_param->getCalibMap()->calibmmpB);
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
      
      stageLightInfo->RESET();
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

    stageLightInfo->back_light_target = 200; //Default
    double *p_back_light_target = JFetch_NUMBER(sl_json, "cam_param.back_light_target");
    if (p_back_light_target)
    {
      stageLightInfo->back_light_target = *p_back_light_target;
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
    camera.SetAnalogGain(*val);
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

  if (getDataFromJson(&settingJson, "set_once_WB", NULL) == cJSON_True)
  {
    CameraLayer::status st = camera.SetOnceWB();
    LOGI("SetOnceWB:%d", st);
    retV = 0;
  }

  val = JFetch_NUMBER(&settingJson, "down_samp_level");
  if (val)
  {
    downSampLevel = (int)*val;
  }

  int type = getDataFromJson(&settingJson, "down_samp_w_calib", NULL);
  if (type == cJSON_False)
  {
    downSampWithCalib = false;
  }
  else if (type == cJSON_True)
  {
    downSampWithCalib = true;
  }
  else
  {
    downSampWithCalib = true;
  }

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
    if (roi_x && roi_y && roi_w && roi_h && ((*roi_w) * (*roi_h)) != 0)
    {
      camera.SetROI(*roi_x, *roi_y, *roi_w, *roi_h, 0, 0);
      // LOGI("ROI v:%f %f %f %f", *roi_x, *roi_y, *roi_w, *roi_h);
      int ox, oy;
      camera.GetROI(&ox, &oy, NULL, NULL, NULL, NULL);
      
      // LOGI("ROI v:%d %d", ox, oy);
      acv_XY offset_o = {(float)ox, (float)oy};
      calib_bacpac.sampler->setOriginOffset(offset_o);
      //sampler
    }
    else
    {
    }
  }
  return 0;
}

void saveInspectionSample(cJSON *inspectionReport, cJSON *camera_param, cJSON *deffile, acvImage *image, const char *fileName)
{

  cJSON *reportsList = JFetch_ARRAY(inspectionReport, "reports[0].reports");
  if (reportsList == NULL)
    return;

  cJSON *camera_param_data = JFetch_OBJECT(camera_param, "reports[0]");
  if (camera_param_data == NULL)
    return;

  std::string filePath(fileName);

  cJSON *infoJObj = cJSON_CreateObject();

  //somehow if I don't copy the data, the devil in
  //this function will remove the reportsList from inspectionReport in some weird way
  //TODO: find the bug, this causes extra data copy cost

  reportsList = cJSON_Duplicate(reportsList, true);
  camera_param_data = cJSON_Duplicate(camera_param_data, true);
  cJSON_AddItemToObject(infoJObj, "reports", reportsList);
  cJSON_AddItemToObject(infoJObj, "defInfo", deffile);
  cJSON_AddItemToObject(infoJObj, "camera_param", camera_param_data);
  cJSON_AddNumberToObject(infoJObj, "time_ms", current_time_ms());

  char *jstr = cJSON_Print(infoJObj);
  // cJSON_DetachItemViaPointer(infoJObj, reportsList); //since the data is copied let Delete to deal with it.
  cJSON_DetachItemViaPointer(infoJObj, deffile);

  cJSON_Delete(infoJObj);
  int ret_write_Len = WriteBytesToFile((uint8_t *)jstr, strlen(jstr), (filePath + ".xreps").c_str());
  delete (jstr);
  if (ret_write_Len < 0)
  {
    return;
  }

  int saveErr = SaveIMGFile((filePath + ".jpg").c_str(), image);
  if (saveErr != 0)
  {
  }
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

  if (cache_camera_param)
  {
    cJSON_Delete(cache_camera_param);
    cache_camera_param = NULL;
  }
  cache_camera_param = cJSON_Parse(fileStr);

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

void setup_machine_setting(cJSON *json_mac_setting)
{

  char *path = JFetch_STRING(json_mac_setting, "InspSampleSavePath");

  LOGE("setup_machine_setting::machine_setting.json path:%s", path);
  if (path != NULL)
  {
    LOGE("TRY to set InspSampleSavePath as %s", path);
    string path_str(path);
    if (rw_create_dir(path_str.c_str()) == true && access(path_str.c_str(), W_OK) == 0)
    {
      InspSampleSavePath = path_str;
    }
    else
    {
      LOGE("PATH:%s is not writable!! set to default", path_str.c_str());
      InspSampleSavePath = InspSampleSavePath_DEFAULT;
    }
    LOGE("InspSampleSavePath:%s is set!!", InspSampleSavePath.c_str());
  }
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
DatCH_CallBack_BPG::DatCH_CallBack_BPG(DatCH_BPG1_0 *self) : resPool(resourcePoolSize)
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



CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj)
{  
  if (type != CameraLayer::EV_IMG)
    return CameraLayer::NAK;
  acvImage *img=(acvImage*)obj;

  CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
  LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
  img->ReSize(finfo.width,finfo.height,3);

  return cl_obj.ExtractFrame(img->CVector[0],3,finfo.width*finfo.height);
}

int getImage(CameraLayer *camera,acvImage *dst_img)
{
  for (int i = 0;; i++)
  {
    if (camera->SnapFrame(SNAP_Callback,(void*)dst_img) == CameraLayer::ACK)
      break;
    if (i > 10)
      return -1;
  }
  return 0;
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
  bool doExit = false;
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
    // {
    //     sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"DF\",\"IM\"]}",session_id);
    //     bpg_dat=GenStrBPGData("SS", tmp);
    //     datCH_BPG.data.p_BPG_data=&bpg_dat;
    //     self->SendData(datCH_BPG);
    // }
    bpg_dat.pgID = dat->pgID;

    // using Ms = std::chrono::milliseconds;
    // for (int retryC=0;!mainThreadLock.try_lock_for(Ms(100));retryC++) //Lock and wait 100 ms
    // {
    //   LOGE("try lock:%d",retryC);
    //   //Still locked
    //   if (retryC > 1) //If the flag is closed then, exit
    //   {
    //     LOGE("retryC");
    //     exit(-1);
    //   }
    // }

    MT_LOCK();

    // if (checkTL("GS", dat) == false)
    //   LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //        dat->pgID);
    if (checkTL("HR", dat))
    {
      LOGI("DataType_BPG>>>>%s", dat->dat_raw);

      LOGI("Hello ready.......");
      session_ACK = true;
    }
    else if (checkTL("SV", dat)) //Data from UI to save file
    {
      LOGI("DataType_BPG>>STR>>%s", dat->dat_raw);

      if (json == NULL)
      {
        snprintf(err_str, sizeof(err_str), "JSON parse failed");
        LOGE("%s", err_str);
        break;
      }
      do
      {

        char *fileName = (char *)JFetch(json, "filename", cJSON_String);
        if (fileName == NULL)
        {
          snprintf(err_str, sizeof(err_str), "No entry:'filename' in it");
          LOGE("%s", err_str);
          break;
        }

        {

          char dirPath[200];
          strcpy(dirPath, fileName);
          char *dir = dirname(dirPath);
          bool dirExist = isDirExist(dir);

          if (dirExist == false && getDataFromJson(json, "make_dir", NULL) == cJSON_True)
          {
            int ret = cross_mkdir(dir);
            dirExist = isDirExist(dir);
          }
          if (dirExist == false)
          {
            snprintf(err_str, sizeof(err_str), "No Dir %s exist", dir);
            LOGE("%s", err_str);
            break;
          }
        }

        LOGE("fileName: %s", fileName);
        int strinL = strlen((char *)dat->dat_raw) + 1;

        if (dat->size - strinL == 0)
        { //No raw data, check "type"

          char *type = (char *)JFetch(json, "type", cJSON_String);
          if (strcmp(type, "__CACHE_IMG__") == 0)
          {
            LOGE("__CACHE_IMG__ %d x %d", cacheImage.GetWidth(), cacheImage.GetHeight());
            if (cacheImage.GetWidth() * cacheImage.GetHeight() > 10) //HACK: just a hacky way to make sure the cache image is there
            {
              SaveIMGFile(fileName, &cacheImage);
            }

            //cacheImage.ReSize(1,1);
          }
          else if (strcmp(type, "__STACKING_IMG__") == 0)
          {
            tmp_buff.ReSize(&(imstack.imgStacked));
            imstack.Export();
            calib_bacpac.sampler->ignoreCalib(false); //First, make the cacheImage to be a calibrated full res image
            ImageDownSampling(tmp_buff, imstack.imgExtract, 1, calib_bacpac.sampler, false);

            if (tmp_buff.GetWidth() * tmp_buff.GetHeight() > 10)
            {
              SaveIMGFile(fileName, &tmp_buff);
            }
          }
        }
        else
        {

          LOGI("DataType_BPG>>BIN>>%s", byteArrString(dat->dat_raw + strinL, dat->size - strinL));

          FILE *write_ptr;

          write_ptr = fopen(fileName, "wb"); // w for write, b for binary
          if (write_ptr == NULL)
          {
            snprintf(err_str, sizeof(err_str), "file:%s File open failed", fileName);
            LOGE("%s", err_str);
            break;
          }
          fwrite(dat->dat_raw + strinL, dat->size - strinL, 1, write_ptr); // write 10 bytes from our buffer

          fclose(write_ptr);
        }

        session_ACK = true;
      } while (false);
    }
    else if (checkTL("FB", dat)) //[F]ile [B]rowsing
    {

      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      do
      {

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed");
          LOGE("%s", err_str);
          break;
        }

        char *pathStr = (char *)JFetch(json, "path", cJSON_String);
        if (pathStr == NULL)
        {
          //ERROR
          snprintf(err_str, sizeof(err_str), "No 'path' entry in the JSON");
          LOGE("%s", err_str);
          break;
        }

        int depth = 1;
        double *p_depth = JFetch_NUMBER(json, "depth");
        if (p_depth != NULL)
        {
          depth = (int)*p_depth;
        }

        {
          cJSON *cjFileStruct = cJSON_DirFiles(pathStr, NULL, depth);

          char *fileStructStr = NULL;

          if (cjFileStruct == NULL)
          {
            cjFileStruct = cJSON_CreateObject();
            snprintf(err_str, sizeof(err_str), "File Structure is NULL");
            LOGI("W:%s", err_str);

            session_ACK = false;
          }
          else
          {

            session_ACK = true;
          }

          fileStructStr = cJSON_Print(cjFileStruct);

          bpg_dat = GenStrBPGData("FS", fileStructStr); //[F]older [S]truct
          bpg_dat.pgID = dat->pgID;
          datCH_BPG.data.p_BPG_data = &bpg_dat;
          self->SendData(datCH_BPG);
          if (fileStructStr)
            free(fileStructStr);
          cJSON_Delete(cjFileStruct);
        }

      } while (false);
    }
    else if (checkTL("GS", dat)) //[G]et [S]etting
    {

      cJSON *items = JFetch_ARRAY(json, "items");
      if (items == NULL)
      {
        session_ACK = false;
      }
      else
      {

        DatCH_Data datCH_BPG =
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        session_ACK = true;

        cJSON *retArr = cJSON_CreateObject();
        char chBuff[120];
        session_ACK = true;

        for (int k = 0;; k++)
        {
          sprintf(chBuff, "items[%d]", k);
          char *itemType = JFetch_STRING(json, chBuff);
          if (itemType == NULL)
            break;
          if (strcmp(itemType, "binary_path") == 0)
          {
            realfullPath(_argv[0], chBuff);
            cJSON_AddStringToObject(retArr, itemType, chBuff);
          }
          else if (strcmp(itemType, "data_path") == 0)
          {
            realfullPath("./", chBuff);
            cJSON_AddStringToObject(retArr, itemType, chBuff);
          }
          else if (strcmp(itemType, "XXXX") == 0)
          {
            cJSON_AddStringToObject(retArr, itemType, "XXXX");
          }
          else if (strcmp(itemType, "camera_info") == 0)
          {

            cJSON *cam_info_jarr = cJSON_CreateArray();

            //LOGI("CAM_INFO..\n%s",calib_bacpac.cam->getCameraJsonInfo());
            cJSON *cam_1 = cJSON_Parse(cb->camera->getCameraJsonInfo().c_str());
            if (cam_1 == NULL)
            {
              cam_1 = cJSON_CreateObject();
            }
            cJSON_AddNumberToObject(cam_1, "mmpp", calib_bacpac.sampler->mmpP_ideal());
            cJSON_AddNumberToObject(cam_1, "cur_width", calib_bacpac.sampler->getCalibMap()->fullFrameW);
            cJSON_AddNumberToObject(cam_1, "cur_height", calib_bacpac.sampler->getCalibMap()->fullFrameH);

            int M_gain, m_gain;
            CameraLayer::status g_ret = calib_bacpac.cam->isInOperation();
            cJSON_AddNumberToObject(cam_1, "cam_status", g_ret);

            cJSON_AddItemToArray(cam_info_jarr, cam_1);
            cJSON_AddItemToObject(retArr, itemType, cam_info_jarr);
          }
        }

        char *jstr = cJSON_Print(retArr);
        cJSON_Delete(retArr);
        bpg_dat = GenStrBPGData("GS", jstr);
        bpg_dat.pgID = dat->pgID;
        datCH_BPG.data.p_BPG_data = &bpg_dat;
        self->SendData(datCH_BPG);
        free(jstr);
      }
    }
    else if (checkTL("LD", dat))
    {
      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      do
      {

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
          LOGE("%s", err_str);

          break;
        }

        char *filename = (char *)JFetch(json, "filename", cJSON_String);
        if (filename != NULL)
        {
          try
          {
            char *fileStr = ReadText(filename);
            if (fileStr == NULL)
            {
              snprintf(err_str, sizeof(err_str), "Cannot read file from:%s", filename);
              LOGE("%s", err_str);
              break;
            }
            LOGI("Read deffile:%s", filename);
            bpg_dat = GenStrBPGData("FL", fileStr);
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
            free(fileStr);
          }
          catch (std::invalid_argument iaex)
          {
            snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
            LOGE("%s", err_str);
          }
        }

        char *deffile = (char *)JFetch(json, "deffile", cJSON_String);

        try
        {
          char *jsonStr = ReadText(deffile);
          if (jsonStr != NULL)
          {
            LOGI("Read deffile:%s", deffile);
            bpg_dat = GenStrBPGData("DF", jsonStr);
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
            free(jsonStr);
          }
        }
        catch (std::invalid_argument iaex)
        {
          snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
          LOGE("%s", err_str);
        }

        acvImage *srcImg = NULL;
        char *imgSrcPath = (char *)JFetch(json, "imgsrc", cJSON_String);
        if (imgSrcPath != NULL)
        {

          int ret_val = LoadIMGFile(&tmp_buff, imgSrcPath);
          if (ret_val == 0)
          {
            srcImg = &tmp_buff;
            cacheImage.ReSize(srcImg);
            acvCloneImage(srcImg, &cacheImage, -1);

            int default_scale = 2;

            double *DS_level = JFetch_NUMBER(json, "down_samp_level");
            if (DS_level)
            {
              default_scale = (int)*DS_level;
              if (default_scale <= 0)
                default_scale = 1;
            }
            //TODO:HACK: 4 times scale down for transmission speed, bpg_dat.scale is not used for now
            bpg_dat = GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)default_scale};

            iminfo.fullHeight = srcImg->GetHeight();
            iminfo.fullWidth = srcImg->GetWidth();
            //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
            //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff, *srcImg, iminfo.scale, NULL);
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = DatCH_BPG_acvImage_Send;

            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
          }
        }

        session_ACK = true;

      } while (false);
    }
    else if (checkTL("II", dat)) //[I]mage [I]nspection
    {
      calib_bacpac.sampler->ignoreCalib(false);
      neutral_bacpac.sampler->ignoreCalib(true);
      FeatureManager_BacPac *select_bacpac = &calib_bacpac;

      if (json == NULL)
      {
        snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
        LOGE("%s", err_str);
        break;
      }

      do
      {
        char *imgSrcPath = (char *)JFetch(json, "imgsrc", cJSON_String);
        LOGI("Load Image from %s", imgSrcPath);
        acvImage *srcImg = NULL;

        if (imgSrcPath != NULL)
        {
          if (strcmp(imgSrcPath, "__CACHE_IMG__") == 0)
          {
            tmp_buff.ReSize(&cacheImage);
            acvCloneImage(&cacheImage, &tmp_buff, -1);
            srcImg = &tmp_buff;
          }
          else
          {
            int ret_val = LoadIMGFile(&tmp_buff, imgSrcPath);
            if (ret_val == 0)
              srcImg = &tmp_buff;
          }
        }
        else if (srcImg == NULL)
        {
          int ret_val = getImage(camera,&tmp_buff);
          if (ret_val == 0)
          {
            srcImg = &tmp_buff;
            cacheImage.ReSize(srcImg);
            acvCloneImage(srcImg, &cacheImage, -1);
          }
        }

        if (srcImg == NULL)
        {
          snprintf(err_str, sizeof(err_str), "No Image from %s, exit... LINE:%04d", imgSrcPath, __LINE__);
          LOGE("%s", err_str);
          break;
        }
        // SaveIMGFile("data/test1.png",srcImg);

        char *deffile = (char *)JFetch(json, "deffile", cJSON_String);

        cJSON *defInfo = JFetch_OBJECT(json, "definfo");

        if (deffile == NULL && defInfo == NULL)
        {
          LOGE("No entry:'deffile':%p OR 'definfo(json)':%p ", __LINE__, deffile, defInfo);
          cb->cameraFramesLeft = 0;
          camera->TriggerMode(1);
          break;
        }

        bool isCalibNA = false;
        cJSON *img_property = JFetch_OBJECT(json, "img_property");
        if (img_property)
        {

          char *calibInfo_type = JFetch_STRING(img_property, "calibInfo.type");

          LOGI("calibInfo_type:%s", calibInfo_type);
          if (strcmp(calibInfo_type, "NA") == 0)
          {
            isCalibNA = true;
            select_bacpac = &neutral_bacpac;
            neutral_bacpac.sampler->getCalibMap()->calibPpB = 1;
            neutral_bacpac.sampler->getCalibMap()->calibmmpB = 1;
          }
          else if (strcmp(calibInfo_type, "neutral") == 0 || strcmp(calibInfo_type, "disable") == 0)
          {

            double *mmpp = JFetch_NUMBER(img_property, "calibInfo.mmpp"); //The mmpp has to be set
            if (mmpp)
            {
              select_bacpac = &neutral_bacpac;
              neutral_bacpac.sampler->getCalibMap()->calibPpB = (*mmpp);
              neutral_bacpac.sampler->getCalibMap()->calibmmpB = 1;
            }
            else
            {
              select_bacpac = NULL;

              break;
            }
          }
          else if (true || strcmp(calibInfo_type, "default") == 0) //since it's default...
          {
            select_bacpac = &calib_bacpac;
          }
        }

        char *jsonStr = NULL;
        if (defInfo)
        {
          jsonStr = cJSON_Print(defInfo);

          // {
          //   neutral_bacpac.sampler->getCalibMap()->calibPpB=NAN;
          //   double *tmpN=JFetch_NUMBER(defInfo,"featureSet[0].cam_param.ppb2b");
          //   if(tmpN)
          //     neutral_bacpac.sampler->getCalibMap()->calibPpB=*tmpN;
          // }

          // {
          //   neutral_bacpac.sampler->getCalibMap()->calibmmpB=NAN;
          //   double *tmpN=JFetch_NUMBER(defInfo,"featureSet[0].cam_param.mmpb2b");
          //   if(tmpN)
          //     neutral_bacpac.sampler->getCalibMap()->calibmmpB=*tmpN;
          // }
        }
        else
        {

          jsonStr = ReadText(deffile);
          if (jsonStr == NULL)
          {
            snprintf(err_str, sizeof(err_str), "Cannot read defFile from:%s LINE:%04d", deffile, __LINE__);
            LOGE("%s", err_str);
            break;
          }
          LOGI("Read deffile:%s", deffile);
        }

        DatCH_Data datCH_BPG =
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        // bpg_dat = GenStrBPGData("DF", jsonStr);
        // bpg_dat.pgID = dat->pgID;
        // datCH_BPG.data.p_BPG_data = &bpg_dat;
        // self->SendData(datCH_BPG);

        //SaveIMGFile("data/TMP__.png",srcImg);
        int ret = ImgInspection_JSONStr(matchingEng, srcImg, 1, jsonStr, select_bacpac);
        free(jsonStr);

        try
        {
          //SaveIMGFile("data/buff.bmp",&test1_buff);

          const FeatureReport *report = matchingEng.GetReport();

          if (report != NULL)
          {
            cJSON *jobj = matchingEng.FeatureReport2Json(report);
            AttachStaticInfo(jobj, cb);
            char *jstr = cJSON_Print(jobj);
            cJSON_Delete(jobj);

            //LOGI("__\n %s  \n___",jstr);
            bpg_dat = GenStrBPGData("RP", jstr);
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);

            delete jstr;
            session_ACK = true;
          }
          else
          {
            session_ACK = false;
          }
        }
        catch (std::invalid_argument iaex)
        {
          snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          break;
        }

        if (img_property)
        {
          double *pscale = JFetch_NUMBER(img_property, "scale");
          if (pscale)
          {
            int _scale = 2;
            _scale = (int)*pscale;

            bpg_dat = GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)_scale};

            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70

            ImageSampler *sampler = isCalibNA ? NULL : select_bacpac->sampler;
            iminfo.fullHeight = srcImg->GetHeight();
            iminfo.fullWidth = srcImg->GetWidth();
            ImageDownSampling(dataSend_buff, *srcImg, iminfo.scale, sampler, 0);

            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = DatCH_BPG_acvImage_Send;
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
          }
        }
        session_ACK = true;

      } while (false);

      calib_bacpac.sampler->ignoreCalib(false);
    }
    else if (checkTL("CI", dat) || checkTL("FI", dat)) //[C]ontinuous [I]nspection / [F]ull [I]nspection
    {
      do
      {
        saveInspFailSnap = false;
        saveInspNASnap = false;
        double *frame_count = JFetch_NUMBER(json, "frame_count");
        cb->cameraFramesLeft = (frame_count != NULL) ? ((int)(*frame_count)) : -1;
        int frameCount = (int)cb->cameraFramesLeft;
        LOGI("cb->cameraFramesLeft:%d frame_count:%p", cb->cameraFramesLeft, frame_count);

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          break;
        }

        cb->CI_pgID = dat->pgID;

        char *deffile = (char *)JFetch(json, "deffile", cJSON_String);
        // if (deffile == NULL)
        // {
        // snprintf(err_str,sizeof(err_str),"No entry:'deffile' in it LINE:%04d",__LINE__);
        // LOGE("%s",err_str);
        // cb->cameraFeedTrigger=false;

        // camera->TriggerMode(1);
        // break;
        // }

        cJSON *defInfo = JFetch_OBJECT(json, "definfo");

        if (deffile == NULL && defInfo == NULL)
        {

          LOGE("No entry:'deffile':%p OR 'definfo(json)':%p ", __LINE__, deffile, defInfo);

          cb->cameraFramesLeft = 0;

          camera->TriggerMode(1);
          break;
        }

        try
        {

          DatCH_Data datCH_BPG =
              BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

          char *jsonStr = NULL;

          if (defInfo != NULL)
          {
            jsonStr = cJSON_Print(defInfo);
            //jsonStr=jstr;
          }

          if (jsonStr == NULL)
          {
            jsonStr = ReadText(deffile);
            if (jsonStr == NULL)
            {
              snprintf(err_str, sizeof(err_str), "Cannot read defFile from:%s LINE:%04d", jsonStr, __LINE__);
              LOGE("%s", err_str);
              cb->cameraFramesLeft = 0;

              break;
            }
            LOGI("Read deffile:%s", deffile);
          }

          if (cache_deffile_JSON)
          {
            cJSON_Delete(cache_deffile_JSON);
            cache_deffile_JSON = NULL;
          }
          cache_deffile_JSON = cJSON_Parse(jsonStr);

          matchingEng.ResetFeature();
          matchingEng.AddMatchingFeature(jsonStr);

          void *target;
          if (getDataFromJson(json, "get_deffile", &target) == cJSON_True)
          {
            bpg_dat = GenStrBPGData("DF", jsonStr);
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
          }
          free(jsonStr);
          //TODO: HACK: this sleep is to wait for the gap in between def config file arriving and inspection result arriving.
          //If the inspection result arrives without def config file then webUI will generate(by design) an statemachine error event.
          // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

          if (dat->tl[0] == 'C')
          {
            if (false && frameCount == 1)
            {
              camera->TriggerMode(1); //Manual trigger
            }
            else
            {
              camera->TriggerMode(0);
            }

            doImgProcessThread = true;
          }
          else if (dat->tl[0] == 'F') //"FI" is for full inspection
          {                           //no manual trigger and process in thread
            camera->TriggerMode(2);
            doImgProcessThread = true;
          }

          if (cb->cameraFramesLeft > 0)
          {

            MT_UNLOCK("SPACING LOCK");
            while (cb->cameraFramesLeft > 0)
            {
              std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            MT_LOCK("SPACING LOCK");
            camera->TriggerMode(1);
          }
          //SaveIMGFile("data/buff.bmp",&test1_buff);

          session_ACK = true;
        }
        catch (std::invalid_argument iaex)
        {

          snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
          LOGE("%s", err_str);
        }

      } while (false);

      LOGE("//////");
    }
    else if (checkTL("EX", dat)) //feature EXtraction
    {
      LOGI("Trigger.......");
      calib_bacpac.sampler->ignoreCalib(false);

      {

        char *imgSrcPath = NULL;
        if (json != NULL)
        {
          imgSrcPath = (char *)JFetch_STRING(json, "imgsrc");
          if (imgSrcPath == NULL)
          {
            snprintf(err_str, sizeof(err_str), "No entry:imgSrcPath in it LINE:%04d", __LINE__);
            LOGE("%s", err_str);
          }
        }
        DatCH_Data datCH_BPG =
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        acvImage *srcImg = NULL;
        if (imgSrcPath != NULL)
        {
          int ret_val = LoadIMGFile(&tmp_buff, imgSrcPath);
          if (ret_val == 0)
          {
            srcImg = &tmp_buff;
          }
        }

        if (srcImg == NULL)
        {

          int ret_val = getImage(camera,&tmp_buff);
          if (ret_val == 0)
          {
            srcImg = &tmp_buff;
            
            cacheImage.ReSize(srcImg);
            //acvCloneImage(srcImg, &cacheImage, -1);
            calib_bacpac.sampler->ignoreCalib(false); //First, make the cacheImage to be a calibrated full res image
            ImageDownSampling(cacheImage, *srcImg, 1, calib_bacpac.sampler, false);

          }
        }

        if (srcImg == NULL)
        {
          
          session_ACK = false;

        }
        else
        {


          try
          {

            ImgInspection_DefRead(matchingEng, srcImg, 1, "data/featureDetect.json", &calib_bacpac);
            const FeatureReport *report = matchingEng.GetReport();

            if (report != NULL)
            {
              cJSON *jobj = matchingEng.FeatureReport2Json(report);
              AttachStaticInfo(jobj, cb);

              char *jstr = cJSON_Print(jobj);
              cJSON_Delete(jobj);

              //LOGI("__\n %s  \n___",jstr);
              bpg_dat = GenStrBPGData("SG", jstr); //SG report : signature360
              bpg_dat.pgID = dat->pgID;
              datCH_BPG.data.p_BPG_data = &bpg_dat;
              self->SendData(datCH_BPG);

              delete jstr;
            }
            else
            {
              sprintf(tmp, "{}");
              bpg_dat = GenStrBPGData("SG", tmp);
              bpg_dat.pgID = dat->pgID;
              datCH_BPG.data.p_BPG_data = &bpg_dat;
              self->SendData(datCH_BPG);
            }
          }
          catch (std::invalid_argument iaex)
          {
            snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
            LOGE("%s", err_str);
          }

          int tar_down_samp_level = 2;
          bool transfer_img = false;

          cJSON *img_property = JFetch_OBJECT(json, "img_property");
          if (img_property)
          {

            double *DS_level = JFetch_NUMBER(img_property, "down_samp_level");
            if (DS_level)
            {
              tar_down_samp_level = (int)*DS_level;
              if (tar_down_samp_level <= 0)
                tar_down_samp_level = 1;
            }

            transfer_img = true;
          }

          if (transfer_img)
          {
            //Down scale the calibrated cache image to make image transfer easier
            bpg_dat = GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)tar_down_samp_level};
            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70

            calib_bacpac.sampler->ignoreCalib(true);
            ImageDownSampling(dataSend_buff, cacheImage, iminfo.scale, calib_bacpac.sampler, true);

            iminfo.fullHeight = cacheImage.GetHeight();
            iminfo.fullWidth = cacheImage.GetWidth();
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = DatCH_BPG_acvImage_Send;
            bpg_dat.pgID = dat->pgID;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            BPG_protocol_send(datCH_BPG);
          }
          calib_bacpac.sampler->ignoreCalib(false);
          session_ACK = true;
        }
      }
        

    }
    else if (checkTL("RC", dat)) //[R]e[C]onnect
    {

      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      char *target = (char *)JFetch(json, "target", cJSON_String);
      if (target == NULL)
      {
      }
      else if (strcmp(target, "camera_ez_reconnect") == 0)
      {

        delete camera;
        camera = NULL;

        camera = getCamera(1);

        // for (int i = 0; camera == NULL; i++)
        // {
        //   LOGV("Camera init retry[%d]...", i);
        //   std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        //   camera = getCamera(CamInitStyle);
        // }
        if (camera != NULL)
        {
          session_ACK = true;
        }
        else
        {
          camera = getCamera(0); //Fallback BMP test folder
        }
        LOGV("DatCH_BPG1_0:%p", camera);

        CameraSettingFromFile(camera, "data/");

        LOGV("DatCH_BPG1_0");
        this->camera = camera;
        callbk_obj.camera = camera;
        calib_bacpac.cam = camera;
      }
      else if (strcmp(target, "camera_setting_refresh") == 0)
      {
        LOGV("DatCH_BPG1_0:%p", camera);

        CameraSettingFromFile(camera, "data/");

        LOGV("DatCH_BPG1_0");
        this->camera = camera;
        callbk_obj.camera = camera;
        calib_bacpac.cam = camera;
      }
    }
    else if (checkTL("SC", dat)) //[S]pecial [C]MD
    {
      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      cJSON *retObj = cJSON_CreateObject();

      char *cmd_type = JFetch_STRING(json, "type");
      if (strcmp(cmd_type, "exec") == 0)
      {
        char *cmd_ = JFetch_STRING(json, "cmd");
        if (cmd_)
        {
          std::string exec_ret = run_exe(cmd_);

          LOGI("CMD:%s", cmd_);
          LOGI("==>:%s", exec_ret.c_str());

          cJSON_AddStringToObject(retObj, "cmd", cmd_);
          cJSON_AddStringToObject(retObj, "output", exec_ret.c_str());
        }
      }
      if (strcmp(cmd_type, "files_existance_check") == 0)
      {
      }
      if (strcmp(cmd_type, "signature_files_matching") == 0)
      {
        cJSON *jo_signature = JFetch_OBJECT(json, "signature");

        //matching DefFiles

        if (jo_signature != NULL)
        {
          ContourSignature tar_sig(jo_signature);
          cJSON_AddNumberToObject(retObj, "mean", tar_sig.mean);
          cJSON_AddNumberToObject(retObj, "sigma", tar_sig.sigma);
          char keyBuff[] = "___________[XXXXXXXXXXX]";
          cJSON *retFileArr = cJSON_CreateArray();
          int k = 0;
          for (k = 0;; k++)
          {
            sprintf(keyBuff, "files[%d]", k);
            char *fileName = JFetch_STRING(json, keyBuff);
            if (fileName == NULL)
              break; //Meaning the array reaches the end

            char *fileStr = ReadText(fileName);
            cJSON *sig_m_report = NULL;
            if (fileStr != NULL)
            {
              sig_m_report = cJSON_CreateObject();
              cJSON *signatureX = NULL;

              {
                cJSON *fileJson = cJSON_Parse(fileStr);
                cJSON *obj0 = JFetch_OBJECT(fileJson, "featureSet[0].inherentfeatures[0]");
                if (obj0 != NULL)
                {
                  char *name = JFetch_STRING(fileJson, "name");
                  cJSON *tags_arr = cJSON_DetachItemFromObject(fileJson, "tag");

                  cJSON_AddNumberToObject(sig_m_report, "idx", k);
                  //signatureX = cJSON_DetachItemFromObject(obj0,"signature");
                  signatureX = JFetch_OBJECT(obj0, "signature");

                  ContourSignature cur_sig(signatureX);
                  bool ret_isInv;
                  float ret_angle = NAN;
                  float matching_Error;
                  matching_Error = tar_sig.match_min_error(cur_sig, 0, 360, 1, &ret_isInv, &ret_angle);

                  cJSON_AddStringToObject(sig_m_report, "FILE_N", fileName);
                  cJSON_AddNumberToObject(sig_m_report, "p_error", matching_Error);
                  cJSON_AddNumberToObject(sig_m_report, "p_angle", ret_angle);

                  matching_Error = tar_sig.match_min_error(cur_sig, 0, 360, -1, &ret_isInv, &ret_angle);

                  cJSON_AddNumberToObject(sig_m_report, "n_error", matching_Error);
                  cJSON_AddNumberToObject(sig_m_report, "n_angle", ret_angle);

                  cJSON_AddNumberToObject(sig_m_report, "mean", cur_sig.mean);
                  cJSON_AddNumberToObject(sig_m_report, "sigma", cur_sig.sigma);

                  cJSON_AddStringToObject(sig_m_report, "name", name);
                  cJSON_AddItemToObject(sig_m_report, "tags", tags_arr);
                }
                cJSON_Delete(fileJson);
              }

              delete fileStr;
            }
            else
            { //File reading error
              //Fill nothing to sig_m_report...
            }
            cJSON_AddItemToArray(retFileArr, sig_m_report);
          }
          if (k == 0)
          {
            cJSON_Delete(retFileArr);
          }
          else
          {
            cJSON_AddItemToObject(retObj, "files", retFileArr);
          }

          cJSON *retSignArr = cJSON_CreateArray();
          for (k = 0;; k++)
          {
            sprintf(keyBuff, "signatures[%d]", k);
            cJSON *sign_obj = JFetch_OBJECT(json, keyBuff);
            if (sign_obj == NULL)
              break; //Meaning the array reaches the end

            cJSON *sig_m_report = cJSON_CreateObject();

            {
              ContourSignature cur_sig(sign_obj);
              cJSON_AddNumberToObject(sig_m_report, "idx", k);
              bool ret_isInv;
              float ret_angle = NAN;
              float matching_Error;
              matching_Error = tar_sig.match_min_error(cur_sig, 0, 360, 1, &ret_isInv, &ret_angle);

              cJSON_AddNumberToObject(sig_m_report, "p_error", matching_Error);
              cJSON_AddNumberToObject(sig_m_report, "p_angle", ret_angle);

              matching_Error = tar_sig.match_min_error(cur_sig, 0, 360, -1, &ret_isInv, &ret_angle);

              cJSON_AddNumberToObject(sig_m_report, "n_error", matching_Error);
              cJSON_AddNumberToObject(sig_m_report, "n_angle", ret_angle);

              cJSON_AddNumberToObject(sig_m_report, "mean", cur_sig.mean);
              cJSON_AddNumberToObject(sig_m_report, "sigma", cur_sig.sigma);
            }

            cJSON_AddItemToArray(retSignArr, sig_m_report);
          }

          if (k == 0)
          {
            cJSON_Delete(retSignArr);
          }
          else
          {
            cJSON_AddItemToObject(retObj, "signatures", retSignArr);
          }
        }
        else
        {
          sprintf(err_str, "No signature info....");
        }
      }
      LOGI(">>>");

      char *jstr = cJSON_Print(retObj);
      cJSON_Delete(retObj);

      bpg_dat = GenStrBPGData("SR", jstr); //Special Return from cmd
      bpg_dat.pgID = dat->pgID;
      datCH_BPG.data.p_BPG_data = &bpg_dat;
      self->SendData(datCH_BPG);

      delete jstr;
      session_ACK = true;
    }
    else if (checkTL("ST", dat)) //[S]e[T]ting
    {

      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      void *target;
      int type = getDataFromJson(json, "DoImageTransfer", &target);
      if (type == cJSON_False)
      {
        DoImageTransfer = false;
        session_ACK = true;
      }
      else if (type == cJSON_True)
      {
        DoImageTransfer = true;
        session_ACK = true;
      }

      ImageCropX = 0;
      ImageCropY = 0;
      ImageCropW = 999999999;
      ImageCropH = 999999999;
      cJSON *InspectionParam = JFetch_ARRAY(json, "InspectionParam");
      if (InspectionParam)
      {
        cJSON *retInfo = matchingEng.SetParam(InspectionParam);

        char *jstr = cJSON_Print(retInfo);
        cJSON_Delete(retInfo);

        bpg_dat = GenStrBPGData("DT", jstr); //Special Return from cmd
        bpg_dat.pgID = dat->pgID;
        datCH_BPG.data.p_BPG_data = &bpg_dat;
        self->SendData(datCH_BPG);
        delete jstr;
      }
      cJSON *ImTranseSetup = JFetch_OBJECT(json, "ImageTransferSetup");
      if (ImTranseSetup)
      {

        int type = getDataFromJson(json, "enable", &target);
        if (type == cJSON_False)
        {
          DoImageTransfer = false;
        }
        else if (type == cJSON_True)
        {
          DoImageTransfer = true;
        }

        double *nX = JFetch_NUMBER(ImTranseSetup, "crop[0]");
        double *nY = JFetch_NUMBER(ImTranseSetup, "crop[1]");
        double *nW = JFetch_NUMBER(ImTranseSetup, "crop[2]");
        double *nH = JFetch_NUMBER(ImTranseSetup, "crop[3]");

        if (nX && nY && nW && nH)
        {
          ImageCropX = *nX;
          ImageCropY = *nY;
          ImageCropW = *nW;
          ImageCropH = *nH;

          if (ImageCropX < 0)
          {
            ImageCropW += ImageCropX;
            ImageCropX = 0;
          }
          if (ImageCropY < 0)
          {
            ImageCropH += ImageCropY;
            ImageCropY = 0;
          }

          if (ImageCropW < 0)
          {
            ImageCropW = 10;
          }
          if (ImageCropH < 0)
          {
            ImageCropH = 10;
          }
        }

        session_ACK = true;
      }

      char *path = JFetch_STRING(json, "CameraSettingFile");
      if (path != NULL)
      {
        int ret = CameraSettingFromFile(this->camera, path);

        if (ret)
          session_ACK = true;
      }

      LOGI("dat->dat_raw:%s", dat->dat_raw);
      LOGI("DoImageTransfer:%d", DoImageTransfer);
      cJSON *camSettingObj = JFetch_OBJECT(json, "CameraSetting");
      if (camera && camSettingObj)
      {
        CameraSetup(*camera, *camSettingObj);
      }

      if (getDataFromJson(json, "CameraTriggerShutter", NULL) == cJSON_True)
      {
        camera->Trigger();
      }

      cJSON *machSetting_JSON = JFetch_OBJECT(json, "MachineSetting");
      if (machSetting_JSON)
      {
        setup_machine_setting(machSetting_JSON);
      }

      auto INSP_NG_SNAP = getDataFromJson(json, "INSP_NG_SNAP", NULL);
      if (INSP_NG_SNAP == cJSON_True)
      {
        saveInspFailSnap = true;
      }
      else if (INSP_NG_SNAP == cJSON_False)
      {
        saveInspFailSnap = false;
      }

      auto INSP_NA_SNAP = getDataFromJson(json, "INSP_NA_SNAP", NULL);
      if (INSP_NA_SNAP == cJSON_True)
      {
        saveInspNASnap = true;
      }
      else if (INSP_NA_SNAP == cJSON_False)
      {
        saveInspNASnap = false;
      }
    }
    else if (checkTL("PR", dat)) //for external application
    {
      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      void *target;
      char *IP = JFetch_STRING(json, "ip");
      double *port_number = JFetch_NUMBER(json, "port");
      if (IP != NULL && port_number != NULL)
      {
        try
        {
          delete_Ext_Util_API();
          LOGI("clean Ext_Util_API....");
          exApi = new Ext_Util_API(IP, *port_number);
          LOGI("new Ext_Util_API OK...");
          exApi->start_RECV_Thread();
          LOGI("start_RECV_Thread...");
          char *retJson = exApi->SYNC_cmd_cameraCalib("*.jpg", 7, 9);
          LOGI("SYNC_cmd_cameraCalib...\n\n:%s", retJson);
          session_ACK = true;
        }
        catch (int errN)
        {
          sprintf(err_str, "[PR] Ext_Util_API init error:%d", errN);
        }
      }
      else if (exApi && IP == NULL && port_number == NULL)
      {
        delete_Ext_Util_API();
        session_ACK = true;
      }
      else
      {
        sprintf(err_str, "[PR] ip:%p port:%p", IP, port_number);
      }
    }
    else if (checkTL("PD", dat)) //Peripheral device
    {

      DatCH_Data datCH_BPG =
          BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

      cJSON *msg_obj = JFetch_OBJECT(json, "msg");
      if (msg_obj && mift)
      {
        char *msgjstr = cJSON_PrintUnformatted(msg_obj);
        int ret = mift->send_data((uint8_t *)msgjstr, strlen(msgjstr));
        // LOGI("mift->send_data:%d,msgjstr:%s", ret, msgjstr);
        delete msgjstr;
        session_ACK = true;
      }
      else
      {
        void *target;
        char *IP = JFetch_STRING(json, "ip");
        double *port_number = JFetch_NUMBER(json, "port");
        if (IP != NULL && port_number != NULL)
        {
          try
          {
            LOGI("delete_MicroInsp_FType()");
            delete_MicroInsp_FType();
            LOGI("delete_MicroInsp_FType() OK...");
            LOGI("CONN: %s:%d", IP, (int)*port_number);
            mift = new MicroInsp_FType(IP, (int)*port_number);

            LOGI("new MicroInsp_FType OK...");
            mift->start_RECV_Thread();
            LOGI("start_RECV_Thread...");

            int fd = mift->getfd();
            session_ACK = true;

            sprintf(tmp, "{\"type\":\"CONNECT\",\"CONN_ID\":%d}", fd);
            bpg_dat = GenStrBPGData("PD", tmp);
            bpg_dat.pgID = -1;
            datCH_BPG.data.p_BPG_data = &bpg_dat;
            self->SendData(datCH_BPG);
          }
          catch (int errN)
          {
            sprintf(err_str, "[PR] MicroInsp_FType init error:%d", errN);
            LOGE("%s", err_str);
          }
        }
        else if (mift && IP == NULL && port_number == NULL)
        {
          delete_MicroInsp_FType();
          session_ACK = true;
        }
        else
        {
          sprintf(err_str, "[PR] ip:%p port:%p", IP, port_number);
        }
      }
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

  if (doExit)
  {
    exit(0);
  }

  return 0;
}

void zlibDeflate_testX(acvImage *img, acvImage *buff, IMG_COMPRESS_FUNC collapse_func, IMG_COMPRESS_FUNC uncollapse_func)
{

  int tLen = img->GetHeight() * img->GetWidth();
  int imgc_Len = 3 * img->GetHeight() * img->GetWidth();

  if (collapse_func)
  {
    imgc_Len = collapse_func(img->CVector[0], 3 * img->GetHeight() * img->GetWidth(),
                             img->CVector[0], 3 * img->GetHeight() * img->GetWidth());
  }

  size_t compresSize = 3 * buff->GetHeight() * buff->GetWidth();

  compresSize = zlibDeflate(buff->CVector[0], 3 * buff->GetHeight() * buff->GetWidth(),
                            img->CVector[0], imgc_Len, 5);

  printf("Compressed size is: %lu/%lu:%.5f\n", compresSize, imgc_Len, (float)compresSize / (imgc_Len));

  size_t unCompresSize = zlibInflate(img->CVector[0], 3 * img->GetHeight() * img->GetWidth(),
                                     buff->CVector[0], compresSize);
  printf("Uncompressed size is: %lu\n", unCompresSize);

  imgc_Len = unCompresSize;
  if (uncollapse_func)
  {
    imgc_Len = uncollapse_func(img->CVector[0], 3 * img->GetHeight() * img->GetWidth(),
                               img->CVector[0], unCompresSize);
  }

  printf("imgc_Len size is: %lu\n", imgc_Len);
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

int CameraSettingFromFile(CameraLayer *camera, char *path)
{
  if (!camera)
    return -1;
  char tmpStr[200];

  sprintf(tmpStr, "%s/default_camera_setting.json", path);
  LOGI("Loading %s", tmpStr);
  int ret = LoadCameraSetting(*camera, tmpStr);
  LOGV("ret:%d", ret);

  if (ret)
    return ret;

  sprintf(tmpStr, "%s/default_camera_param.json", path);

  ret = LoadCameraCalibrationFile(tmpStr, calib_bacpac.sampler);

  if (ret)
  {
    LOGE("LoadCameraCalibrationFile ERROR");
    return ret;
    //throw new std::runtime_error("LoadCameraCalibrationFile ERROR");
  }
  neutral_bacpac.sampler->getCalibMap()->calibmmpB = calib_bacpac.sampler->getCalibMap()->calibmmpB;
  neutral_bacpac.sampler->getCalibMap()->calibPpB = calib_bacpac.sampler->getCalibMap()->calibPpB; //set mmpp

  if (calib_bacpac.sampler)
  {
    stageLightParam *stageLightInfo = calib_bacpac.sampler->getStageLightInfo();
    LOGI("SetExposureTime from bacpac:%f us", stageLightInfo->exposure_us);
    if(stageLightInfo->exposure_us==stageLightInfo->exposure_us)
      camera->SetExposureTime(stageLightInfo->exposure_us);
    //stageLightInfo->back_light_target=back_light_target;

    LOGI("mmpB:%f  calibPpB:%f", calib_bacpac.sampler->getCalibMap()->calibmmpB, calib_bacpac.sampler->getCalibMap()->calibPpB);
    LOGI("scaled ppb2b:%f", calib_bacpac.sampler->getCalibMap()->calibmmpB / calib_bacpac.sampler->mmpP_ideal());
    LOGI("mmpp:%.9f", calib_bacpac.sampler->mmpP_ideal());
  }

  return 0;
}

int ImgInspection_DefRead(MatchingEngine &me, acvImage *test1, int repeatTime, char *defFilename, FeatureManager_BacPac *bacpac)
{
  char *string = ReadText(defFilename);
  //printf("%s\n%s\n",string,defFilename);
  int ret = ImgInspection_JSONStr(me, test1, repeatTime, string, bacpac);
  free(string);
  return ret;
}

int ImgInspection(MatchingEngine &me, acvImage *test1, FeatureManager_BacPac *bacpac, CameraLayer *cam, int repeatTime = 1)
{

  LOGI("============w:%d h:%d====================cam:%p", test1->GetWidth(), test1->GetHeight(), cam);
  if (test1->GetWidth() * test1->GetHeight() == 0)
  {
    return -1;
  }
  clock_t t = clock();
  bacpac->cam = cam;
  for (int i = 0; i < repeatTime; i++)
  {
    me.setBacPac(bacpac);
    me.FeatureMatching(test1);
  }
  clock_t new_t = clock();
  LOGI("%fms \n", (double)(new_t - t) / CLOCKS_PER_SEC * 1000);
  t = new_t;

  return 0;
  //ContourFeatureDetect(test1,&test1_buff,tar_signature);
  //SaveIMGFile("data/target_buff.bmp",&test1_buff);
}

int ImgInspection_JSONStr(MatchingEngine &me, acvImage *test1, int repeatTime, char *jsonStr, FeatureManager_BacPac *bacpac)
{

  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);
  ImgInspection(me, test1, bacpac, bacpac->cam, repeatTime);
  return 0;
}

float acvImageDiff(acvImage *img1, acvImage *img2, float *ret_max_diff, int skipSampling)
{
  if (skipSampling < 1)
    skipSampling = 1;
  uint64_t diffSum = 0;
  int diffMax = 0;
  int count = 0;
  for (int i = 0; i < img1->GetHeight(); i += skipSampling)
  {
    for (int j = 0; j < img1->GetWidth(); j += skipSampling)
    {
      count++;
      int diff = img1->CVector[i][3 * j] - img2->CVector[i][3 * j];
      diff *= diff;
      diffSum += diff;
      if (diffMax < diff)
      {
        diffMax = diff;
      }
    }
  }
  if (ret_max_diff)
    *ret_max_diff = sqrt(diffMax);
  return sqrt((float)diffSum / (count));
}

void acvImageAve(acvImage *imgStackRes, acvImage *imgStack, int stackingN)
{
  for (int i = 0; i < imgStackRes->GetHeight(); i++)
  {
    for (int j = 0; j < imgStackRes->GetWidth(); j++)
    {
      int pixSum = 0;
      for (int k = 0; k < stackingN; k++)
      {
        pixSum += imgStack[k].CVector[i][3 * j];
      }
      imgStackRes->CVector[i][3 * j] =
          imgStackRes->CVector[i][3 * j + 1] =
              imgStackRes->CVector[i][3 * j + 2] = pixSum / stackingN;
    }
  }
}

void acvImageBlendIn(acvImage *imgOut, int *imgSArr, acvImage *imgB, int Num)
{
  for (int i = 0; i < imgOut->GetHeight(); i++)
  {
    for (int j = 0; j < imgOut->GetWidth(); j++)
    {
      int *pixSum = &(imgSArr[i * imgOut->GetWidth() + j]);
      if (Num == 0)
      {
        *pixSum = imgB->CVector[i][3 * j];
      }
      else
      {
        *pixSum += imgB->CVector[i][3 * j];
      }
      imgOut->CVector[i][3 * j] =
          imgOut->CVector[i][3 * j + 1] =
              imgOut->CVector[i][3 * j + 2] = (*pixSum / (Num + 1));
    }
  }
}

int InspStatusReducer(int total_status, int new_status)
{
  if (total_status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    return FeatureReport_sig360_circle_line_single::STATUS_NA;

  if (total_status == FeatureReport_sig360_circle_line_single::STATUS_FAILURE)
  {
    if (new_status == FeatureReport_sig360_circle_line_single::STATUS_NA)
    {
      return FeatureReport_sig360_circle_line_single::STATUS_NA;
    }
    else
    {
      return total_status;
    }
  }

  if (total_status == FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
  {
    return new_status;
  }
  return FeatureReport_sig360_circle_line_single::STATUS_NA;
}

int InspStatusReduce(vector<FeatureReport_judgeReport> &jrep)
{
  if (jrep.size() == 0)
    return FeatureReport_sig360_circle_line_single::STATUS_NA;
  int stat = FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;

  for (int k = 0; k < jrep.size(); k++)
  {
    stat = InspStatusReducer(stat, jrep[k].status);
  }
  return stat;
}

void ImgPipeProcessCenter_imp(image_pipe_info *imgPipe, bool *ret_pipe_pass_down = NULL);

CameraLayer::status CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void *context)
{
  if (type != CameraLayer::EV_IMG)
    return CameraLayer::NAK;
  static clock_t pframeT;
  clock_t t = clock();

  double interval = (double)(t - pframeT) / CLOCKS_PER_SEC * 1000;
  if (!doImgProcessThread)
  {
    int skip_int = 0;
    LOGI("frameInterval:%fms t:%d pframeT:%d", interval, t, pframeT);
    if (interval < skip_int)
    {
      LOGI("interval:%f is less than skip_int:%d ms", interval, skip_int);
      return CameraLayer::NAK; //if the interval less than 70ms then... skip this frame
    }
  }
  pframeT = t;
  LOGE("=============== frameInterval:%fms \n", interval);
  LOGI("cb->cameraFramesLeft:%d", cb->cameraFramesLeft);
  CameraLayer &cl_GMV = *((CameraLayer *)&cl_obj);


  

  // if(inspQueue.size()>0)//for responsiveness
  // {//toss image if the queue is not empty
  //   return;
  // }


  

  image_pipe_info *headImgPipe = cb->resPool.fetchResrc_blocking();
  if (headImgPipe == NULL)
  {
    LOGE("HEAD IMG pipe is NULL");
    return CameraLayer::NAK;
  }

  headImgPipe->camLayer = &cl_obj;
  headImgPipe->type = type;
  headImgPipe->context = context;
  CameraLayer::frameInfo finfo = cl_GMV.GetFrameInfo();
  headImgPipe->fi = finfo;
  
  LOGE("finfo.wh:%d,%d \n", finfo.width,finfo.height);
  headImgPipe->img.ReSize(finfo.width,finfo.height,3);
  cl_GMV.ExtractFrame(headImgPipe->img.CVector[0],3,finfo.width*finfo.height);

  headImgPipe->bacpac = &calib_bacpac;

  if (doImgProcessThread)
  {

    LOGE("cb->resPool.rest_size:: %d", cb->resPool.rest_size());

    if (inspQueue.push_blocking(headImgPipe) == false)
    {
      LOGE("NO resource can be used.....");
      // imagePipeBuffer.clear();

      if (cb->mift)
      {
        LOGI("mift is here too!!");
        char buffx[150];
        static int count = 0;
        int len = sprintf(buffx,
                          "{"
                          "\"type\":\"inspRep\",\"status\":%d,"
                          "\"idx\":%d"
                          "}",
                          -10001, 1);
        cb->mift->send_data((uint8_t *)buffx, len);
      }
    }
  }
  else
  {
    //
    //   while (imagePipeBuffer.size() == 0)
    //   { //Wait for ImgPipeProcessThread to complete
    //
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //   }
    //
    bool doPassDown = false;
    ImgPipeProcessCenter_imp(headImgPipe, &doPassDown);
    if (!doPassDown)
      cb->resPool.retResrc(headImgPipe);
  }
  return CameraLayer::ACK;
}

void sendResultTo_mift(int uInspStatus, uint64_t timeStamp)
{

  if (cb->mift)
  {
    LOGI("mift is here!!");
    char buffx[150];
    static int count = 0;
    int len = sprintf(buffx,
                      "{"
                      "\"type\":\"inspRep\",\"status\":%d,"
                      "\"idx\":%d,\"count\":%d,"
                      "\"time_100us\":%lu"
                      "}",
                      uInspStatus, 1, count, timeStamp);
    cb->mift->send_data((uint8_t *)buffx, len);
    count = (count + 1) & 0xFF;
    LOGI("%s", buffx);
  }
}

void InspResultAction(image_pipe_info *imgPipe, bool skipInspDataTransfer, bool skipImageTransfer, bool inspSnap, bool *ret_pipe_pass_down = NULL)
{
  static int frameActionID = 0;
  if (cb->cameraFramesLeft == 0)
  {
    // camera->TriggerMode(1);
    MT_UNLOCK();
    return;
  }
  if (cb->cameraFramesLeft > 0)
    cb->cameraFramesLeft--;

  {

    int counter = 0;
    while (MT_LOCK("ImgPipeProcessCenter_imp lock", 100) != 0) //Lock and wait 100 ms
    {
      LOGE("try lock");
      counter++;
      //Still locked
      if (counter > 1 || cb->cameraFramesLeft == 0) //If the flag is closed then, exit
      {
        LOGE("cb->cameraFramesLeft is off return..");
        return;
      }
    }
  }

  if (ret_pipe_pass_down)
    *ret_pipe_pass_down = false;

  clock_t t = clock();
  acvImage &capImg = imgPipe->img;
  FeatureManager_BacPac *bacpac = imgPipe->bacpac;
  CameraLayer::frameInfo &fi = imgPipe->fi;

  BPG_data bpg_dat;

  do
  {
    // sendResultTo_mift(imgPipe->actInfo.uInspStatus,fi.timeStamp_100us);

    if (skipInspDataTransfer == true)
    {
      break;
    }

    char tmp[100];
    DatCH_Data datCH_BPG =
        BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

    sprintf(tmp, "{\"start\":true}");
    bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    bpg_dat.pgID = cb->CI_pgID;
    datCH_BPG.data.p_BPG_data = &bpg_dat;
    BPG_protocol_send(datCH_BPG);

    try
    {
      // LOGI(">>>>");

      cJSON *jobj = imgPipe->actInfo.report_json;
      AttachStaticInfo(jobj, cb);
      // double expTime = NAN;
      // if (CameraLayer::ACK == imgPipe->camLayer->GetExposureTime(&expTime))
      // {
      //   cJSON_AddNumberToObject(jobj, "exposure_time", expTime);
      // }
      char *jstr = cJSON_Print(jobj);

      // LOGI("__\n %s  \n___",jstr);
      bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("RP", jstr);
      bpg_dat.pgID = cb->CI_pgID;
      datCH_BPG.data.p_BPG_data = &bpg_dat;
      BPG_protocol_send(datCH_BPG);

      delete jstr;
    }
    catch (std::invalid_argument iaex)
    {
      LOGE("Caught an error!");
    }

    // LOGI(">>>>");
    clock_t img_t = clock();
    static acvImage test1_buff;

    BPG_data_acvImage_Send_info iminfo;
    bool sendJpg = false;
    if (sendJpg && mjpegS && DoImageTransfer && skipImageTransfer == false)
    {
      acvImage *sendImg = &capImg;

      if (sendImg == NULL)
      {
        sendImg = &test1_buff;
        iminfo = (BPG_data_acvImage_Send_info){img : &test1_buff, scale : 2};

        iminfo.offsetX = (0 / iminfo.scale) * iminfo.scale;
        iminfo.offsetY = (0 / iminfo.scale) * iminfo.scale;

        iminfo.fullHeight = capImg.GetHeight();
        iminfo.fullWidth = capImg.GetWidth();
        int cropH = iminfo.fullHeight;
        int cropW = iminfo.fullWidth;

        // LOGI(">>>>");
        ImageSampler *sampler = (false) ? bacpac->sampler : NULL;
        //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
        ImageDownSampling(test1_buff, capImg, iminfo.scale, sampler, 1,
                          iminfo.offsetX, iminfo.offsetY, cropW, cropH);
      }
      frameActionID++;
      if (frameActionID >= 256)
        frameActionID = 0;
      int tmp = frameActionID;
      for (int i = 0; i < 8; i++)
      {
        if (tmp & 1)
        {
          sendImg->CVector[0][i * 3] =
              sendImg->CVector[0][i * 3 + 1] =
                  sendImg->CVector[0][i * 3 + 2] = 255;
        }
        else
        {
          sendImg->CVector[0][i * 3] =
              sendImg->CVector[0][i * 3 + 1] =
                  sendImg->CVector[0][i * 3 + 2] = 0;
        }
        tmp >>= 1;
      }

      uint8_t *encBuff = NULL;
      unsigned long encBuffL = 0;
      if (mjpecLib_enc((uint8_t *)sendImg->CVector[0], sendImg->GetWidth(), sendImg->GetHeight(), 85, &encBuff, &encBuffL) == 0)
      {
        int sendCount = mjpegS->SendFrame(std::string("/CAM1.mjpg"), encBuff, encBuffL);
        delete (encBuff);

        LOGI("ENC size:%d sendCount:%d", encBuffL, sendCount);
        encBuff = NULL;
        encBuffL = 0;
      }
    }
    //if(stackingC==0)
    if ((!sendJpg) && DoImageTransfer && skipImageTransfer == false)
    {

      {

        iminfo = (BPG_data_acvImage_Send_info){img : &test1_buff, scale : (uint16_t)downSampLevel};
        if (iminfo.scale == 0)
        {
          iminfo.scale = 1;
        }

        iminfo.offsetX = (ImageCropX / iminfo.scale) * iminfo.scale;
        iminfo.offsetY = (ImageCropY / iminfo.scale) * iminfo.scale;

        iminfo.fullHeight = capImg.GetHeight();
        iminfo.fullWidth = capImg.GetWidth();
        int cropW = ImageCropW;
        int cropH = ImageCropH;

        // LOGI(">>>>");
        ImageSampler *sampler = (true) ? bacpac->sampler : NULL;
        //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
        ImageDownSampling(test1_buff, capImg, iminfo.scale, sampler, 1,
                          iminfo.offsetX, iminfo.offsetY, cropW, cropH);
      }

      bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("IM", NULL);
      //BPG_data_acvImage_Send_info iminfo={img:&test1_buff,scale:4};

      // LOGI(">>>>");
      bpg_dat.callbackInfo = (uint8_t *)&iminfo;
      bpg_dat.callback = DatCH_BPG_acvImage_Send;
      bpg_dat.pgID = cb->CI_pgID;
      datCH_BPG.data.p_BPG_data = &bpg_dat;
      BPG_protocol_send(datCH_BPG);
      LOGI("img transfer(DL:%d) %fms \n", downSampLevel, ((double)clock() - img_t) / CLOCKS_PER_SEC * 1000);
    }

    sprintf(tmp, "{\"start\":false, \"framesLeft\":%s,\"frameID\":%d,\"ACK\":true}", (cb->cameraFramesLeft) ? "true" : "false", frameActionID);
    bpg_dat = DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    bpg_dat.pgID = cb->CI_pgID;
    datCH_BPG.data.p_BPG_data = &bpg_dat;
    BPG_protocol_send(datCH_BPG);

    //SaveIMGFile("data/MVCamX.bmp",&test1_buff);
    //exit(0);
    if (cb->cameraFramesLeft)
    {
      LOGV("cb->cameraFramesLeft:%d Get Next frame...", cb->cameraFramesLeft);
      //std::this_thread::sleep_for(std::chrono::milliseconds(100));
      //cl_GMV.Trigger();
    }
    else
    {
    }
  } while (false);

  if (inspSnap)
  {
    if (inspSnapQueue.push(imgPipe))
    {
      if (ret_pipe_pass_down)
        *ret_pipe_pass_down = true;
    }
    else
    {
      LOGE("inspSnapQueue is full.... skip the save");

      if (ret_pipe_pass_down)
        *ret_pipe_pass_down = false;
    }
  }

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  MT_UNLOCK();
}

std::string getTimeStr(const char *timeFormat = "%d-%m-%Y %H:%M:%S")
{
  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, sizeof(buffer), timeFormat, timeinfo);
  std::string str(buffer);
  return str;
}

void InspSnapSaveThread(bool *terminationflag)
{
  using Ms = std::chrono::milliseconds;
  int delayStartCounter = 10000;

  std::string SEP = std::string(1, systemPathSEP());
  while (terminationflag && *terminationflag == false)
  {

    //   if(delayStartCounter>0)
    //   {
    //     delayStartCounter--;
    //   }
    //   else
    //   {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //   }
    image_pipe_info *headImgPipe = NULL;

    while (inspSnapQueue.pop_blocking(headImgPipe))
    {
      // LOGI(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>report_json:%p",headImgPipe->actInfo.report_json);
      //report save
      //TODO: when need to save the inspection result run this, but there is a data saving latancy issue need to be solved
      {

        MT_LOCK();
        std::string rootPath = InspSampleSavePath + SEP; //InspSampleSavePath might be changed by main thread
        MT_UNLOCK();
        //root/Date/Name/ms.xxx
        std::string extPath = getTimeStr("%Y%m%d") + SEP; //Date
        {

          char *name = JFetch_STRING(cache_deffile_JSON, "name");
          if (name != NULL && name[0] != '\0')
          {
            extPath += std::string(name) + SEP;
          }
          else
          {
            extPath += std::string("_NoName_") + SEP;
          }

          std::string _path1 = rootPath + extPath;
          LOGE("create DIR", _path1.c_str());
          if (rw_create_dir(_path1.c_str()) == false) //recursive create folder if failed
          {
            LOGE("the path:%s cannot be created", _path1.c_str());
            rootPath = InspSampleSavePath_DEFAULT;      //try the default one
            if (rw_create_dir(_path1.c_str()) == false) //should always work
            {
              std::string _path_d = _path1;
              LOGE("the default path:%s cannot be created.... exit", _path_d.c_str());
              exit(-100);
              //TODO: critical
            }
          }
          // if(access((rootPath+extPath).c_str(),W_OK)==0)//should work
          // {
          //   std::string _path_d = rootPath+extPath;
          //   LOGE("the path:%s is not accecible.... exit",_path_d.c_str());
          //   exit(-101);
          //   //TODO: critical
          // }
        }

        // rootPath
        // std::string timeStamp= getTimeStr("%H:%M:%S") ;
        std::string filePath = rootPath + extPath + std::to_string(current_time_ms());

        saveInspectionSample(headImgPipe->actInfo.report_json, cache_camera_param, cache_deffile_JSON, &headImgPipe->img, filePath.c_str());
      }

      cJSON_Delete(headImgPipe->actInfo.report_json);
      headImgPipe->actInfo.report_json = NULL;

      cb->resPool.retResrc(headImgPipe);
    }
  }
}

void ImgPipeActionThread(bool *terminationflag)
{
  using Ms = std::chrono::milliseconds;
  int delayStartCounter = 10000;
  bool imgSendState=true;
  while (terminationflag && *terminationflag == false)
  {

    //   if(delayStartCounter>0)
    //   {
    //     delayStartCounter--;
    //   }
    //   else
    //   {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //   }
    image_pipe_info *headImgPipe = NULL;

    while (actionQueue.pop_blocking(headImgPipe))
    {

      bool doPassDown = false;
      bool saveToSnap = false;

      if (saveInspFailSnap)
      {
        if (headImgPipe->actInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_FAILURE ||
            headImgPipe->actInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_NA)
        {
          saveToSnap = true;
        }
      }

      // if(saveInspNASnap)
      // {
      //   if(headImgPipe->actInfo.finspStatus==FeatureReport_sig360_circle_line_single::STATUS_NA && inspSnapQueue.size()>1)//only when the queue is free
      //   {
      //     saveToSnap=true;
      //   }
      // }

      if(imgSendState==false)
      {
        if(actionQueue.size() <3)
          imgSendState=true;
      }
      else
      {
        if(actionQueue.size() >5)
          imgSendState=false;
      }

      InspResultAction(headImgPipe, false, !imgSendState , saveToSnap, &doPassDown);

      //delayStartCounter=10000;
      if (!doPassDown)
      { //there is the end, recycle the resource

        cJSON_Delete(headImgPipe->actInfo.report_json);
        headImgPipe->actInfo.report_json = NULL;
        cb->resPool.retResrc(headImgPipe);
      }
    }
  }
}

void ImgPipeProcessCenter_imp(image_pipe_info *imgPipe, bool *ret_pipe_pass_down)
{

  LOGE("============DO INSP>> waterLvL: insp:%d/%d act:%d/%d  snap:%d/%d   poolSize:%d",
       inspQueue.size(), inspQueue.capacity(),
       actionQueue.size(), actionQueue.capacity(),
       inspSnapQueue.size(), inspSnapQueue.capacity(),
       cb->resPool.rest_size());
  if (cb->cameraFramesLeft == 0)
  {
    // camera->TriggerMode(1);
    MT_UNLOCK();
    return;
  }
  clock_t t = clock();

  acvImage &capImg = imgPipe->img;
  FeatureManager_BacPac *bacpac = imgPipe->bacpac;
  CameraLayer::frameInfo &fi = imgPipe->fi;

  int ret = 0;

  //stackingC=0;
  if(0){
    
    acv_XY offset = {
      X : fi.offset_x,
      Y : fi.offset_y
    };
    LOGI("offset:%f,%f", offset.X,offset.Y);
    bacpac->sampler->setOriginOffset(offset);
  }

  //if(stackingC!=0)return;

  if (0)
  {
    if (imstack.imgStacked.GetHeight() != capImg.GetHeight() || imstack.imgStacked.GetWidth() != capImg.GetWidth())
    {
      imstack.ReSize(&capImg);
    }
    else if (imstack.DiffBigger(&capImg, 10, 30))
    {
      imstack.Reset();
    }

    LOGI("stackingC:%d", imstack.stackingC);
    imstack.Add(&capImg);
    // LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  }

  {
    ret = ImgInspection(matchingEng, &capImg, bacpac, imgPipe->camLayer, 1);
  }

  {

    const FeatureReport *report = matchingEng.GetReport();

    int stat = FeatureReport_sig360_circle_line_single::STATUS_NA;

    int stat_sec = FeatureReport_sig360_circle_line_single::STATUS_UNSET;

    if (report->type == FeatureReport::binary_processing_group)
    {
      vector<const FeatureReport *> &reports =
          *(report->data.binary_processing_group.reports);

      vector<acv_LabeledData> *ldat = report->data.binary_processing_group.labeledData;

      if (reports.size() == 1 && reports[0]->type == FeatureReport::sig360_circle_line)
      {
        vector<FeatureReport_sig360_circle_line_single> &srep =
            *(reports[0]->data.sig360_circle_line.reports);
        stat = FeatureReport_sig360_circle_line_single::STATUS_NA;

        if (srep.size() == 1) //only one detected objects in scence is allowed
        {
          int insp_tar_area = (*ldat)[srep[0].labeling_idx].area;

          int totalArea = 0;
          for (int i = 1; i < ldat->size(); i++)
          {
            totalArea += (*ldat)[i].area;
          }
          float extra_area_ratio = (float)(totalArea - insp_tar_area) / totalArea;
          LOGI("totalArea:%d insp_tar_area:%d extra_area_ratio:%f", totalArea, insp_tar_area, extra_area_ratio);
          if (extra_area_ratio < 0.1)
          {
            vector<FeatureReport_judgeReport> &jrep = *(srep[0].judgeReports);
            stat = InspStatusReduce(jrep);
            stat_sec = stat;
          }
        }
      }
    }

    imgPipe->actInfo.uInspStatus = stat;
    imgPipe->actInfo.finspStatus = stat_sec;

    imgPipe->actInfo.report_json = matchingEng.FeatureReport2Json(report);
  }

  LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);

  bool doPassDown = doInspActionThread;

  //taking the short cut
  sendResultTo_mift(imgPipe->actInfo.uInspStatus, imgPipe->fi.timeStamp_us);
  if (doPassDown)
  {
    actionQueue.push_blocking(imgPipe);
  }
  else
  {
    InspResultAction(imgPipe, false, false, &doPassDown);

    if (!doPassDown) //then, we need to recycle the resource here
    {
      if (imgPipe->actInfo.report_json)
        cJSON_Delete(imgPipe->actInfo.report_json);
      imgPipe->actInfo.report_json = NULL;
      cb->resPool.retResrc(imgPipe);
    }
  }
  if (ret_pipe_pass_down)
    *ret_pipe_pass_down = doPassDown;

  //std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void ImgPipeProcessThread(bool *terminationflag)
{
  using Ms = std::chrono::milliseconds;
  int delayStartCounter = 10000;
  while (terminationflag && *terminationflag == false)
  {

    //   if(delayStartCounter>0)
    //   {
    //     delayStartCounter--;
    //   }
    //   else
    //   {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //   }
    image_pipe_info *headImgPipe = NULL;

    while (inspQueue.pop_blocking(headImgPipe))
    {

      // LOGI("============POP");
      //delayStartCounter=10000;
      bool doPassDown = false;
      ImgPipeProcessCenter_imp(headImgPipe, &doPassDown);
      if (!doPassDown)
        cb->resPool.retResrc(headImgPipe);
    }
  }
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

    break;

  case websock_data::eventType::HAND_SHAKING_FINISHED:

    LOGI("HAND_SHAKING: host:%s orig:%s key:%s res:%s\n",
         ws_data.data.hs_frame.host,
         ws_data.data.hs_frame.origin,
         ws_data.data.hs_frame.key,
         ws_data.data.hs_frame.resource);
    if (ws->default_peer == NULL)
    {
      ws->default_peer = ws_data.peer;
    }
    else
    {
    }
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

#ifdef FEATURE_COMPILE_W_MINDVISION_CAMERA_SDK
CameraLayer_GIGE_MindVision *initCamera_MindVision(std::string targetIdContains = "")
{

  tSdkCameraDevInfo sCameraList[10];
  int retListL = sizeof(sCameraList) / sizeof(sCameraList[0]);
  CameraLayer_GIGE_MindVision::EnumerateDevice(sCameraList, &retListL);

  if (retListL <= 0)
    return NULL;
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
  CameraLayer_GIGE_MindVision *CL_GIGE = new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV, NULL);
  if (CL_GIGE->InitCamera(&(sCameraList[0])) == CameraLayer::ACK)
  {
    return CL_GIGE;
  }
  delete CL_GIGE;
  return NULL;
}
#else
CameraLayer *initCamera_MindVision(std::string targetIdContains = "")
{
  LOGE("NO driver.... skip");
  return NULL;
}
#endif

#ifdef FEATURE_COMPILE_W_ARAVIS
CameraLayer_Aravis *initCamera_Aravis(std::string targetIdContains = "")
{
  vector<CameraLayer_Aravis::cam_info> infoList;
  CameraLayer_Aravis::listDevices(infoList, true);
  if (infoList.size() > 0)
  {
    for (int i = 0; i < infoList.size(); i++)
    {
      if (targetIdContains.length() > 0 && infoList[i].id.find(targetIdContains) == string::npos)
      {
        continue;
      }

      try
      {
        if (infoList[i].available)
        { //try to find first available camera

          LOGI("=======CAM OPEN========");
          CameraLayer_Aravis *camera = new CameraLayer_Aravis(infoList[i].id.c_str(), CameraLayer_Callback_GIGEMV, NULL);
          CameraLayer_Aravis::cam_info ci = camera->getCameraInfo();
          LOGI("=======CAM OPEN========");
          LOGI("id:%s", ci.id.c_str());
          LOGI("physical_id:%s", ci.physical_id.c_str());
          LOGI("address:%s", ci.address.c_str());
          LOGI("model:%s", ci.model.c_str());
          LOGI("protocol:%s", ci.protocol.c_str());
          LOGI("serial_nbr:%s", ci.serial_nbr.c_str());
          LOGI("vendor:%s", ci.vendor.c_str());
          LOGI("available:%d", ci.available);
          return camera;
        }
      }
      catch (const std::exception &ex)
      {
      }
    }
  }

  return NULL;
}

#else
CameraLayer *initCamera_Aravis(std::string targetIdContains = "")
{
  LOGE("NO driver.... skip");
  return NULL;
}
#endif

#ifdef FEATURE_COMPILE_W_HIKROBOT_CAMERA_SDK

bool PrintDeviceInfo(MV_CC_DEVICE_INFO *pstMVDevInfo)
{
  if (NULL == pstMVDevInfo)
  {
    printf("The Pointer of pstMVDevInfo is NULL!\n");
    return false;
  }
  if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
  {
    printf("chUserDefinedName: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    printf("chModelName: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
    printf("chDeviceVersion: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chDeviceVersion);
    printf("chManufacturerSpecificInfo: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chManufacturerSpecificInfo);
    printf("chManufacturerName: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chManufacturerName);
    printf("Serial Number: [%s]\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chSerialNumber);
  }
  else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
  {
    printf("chUserDefinedName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    printf("chVendorName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chVendorName);
    printf("chModelName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
    printf("chFamilyName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chFamilyName);
    printf("chDeviceVersion: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chDeviceVersion);
    printf("chManufacturerName: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chManufacturerName);
    printf("Serial Number: [%s]\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chSerialNumber);
  }
  else
  {
    printf("Not support.\n");
  }

  return true;
}

CameraLayer_HikRobot_Camera *initCamera_HikRobot_Camera(std::string targetIdContains = "")
{
  MV_CC_DEVICE_INFO_LIST stDeviceList;
  CameraLayer_HikRobot_Camera::listDevices(&stDeviceList);

  MV_CC_DEVICE_INFO *tar_pstMVDevInfo = NULL;
  for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++)
  {
    printf("[device %d]:\n", i);
    MV_CC_DEVICE_INFO *pstMVDevInfo = stDeviceList.pDeviceInfo[i];
    if (NULL == pstMVDevInfo)
    {
      continue;
    }

    PrintDeviceInfo(pstMVDevInfo);
    std::string UserDefinedName;
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
      UserDefinedName = std::string((const char *)pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
      UserDefinedName = std::string((const char *)pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    if (UserDefinedName.find(targetIdContains) == string::npos)
    {
      continue;
    }
    tar_pstMVDevInfo = pstMVDevInfo;
    // void *handle = NULL;
    // nRet = MV_CC_CreateHandle(&handle, pstMVDevInfo);
    // if (MV_OK == nRet)
    // {
    //   cameraHandles.push_back(handle);
    // }
  }

  if (tar_pstMVDevInfo == NULL)
    return NULL;

  CameraLayer_HikRobot_Camera *extractCam = NULL;
  try
  {
    return new CameraLayer_HikRobot_Camera(tar_pstMVDevInfo, CameraLayer_Callback_GIGEMV, NULL);
  }
  catch (const std::exception &e)
  {
    LOGE("catch an E:%s", e.what());
  }
  return NULL;
}

#else
CameraLayer *initCamera_HikRobot_Camera(std::string targetIdContains = "")
{
  LOGE("NO driver.... skip");
  return NULL;
}
#endif





int initCamera(CameraLayer_BMP_carousel *CL_bmpc)
{
  return CL_bmpc == NULL ? -1 : 0;
}

CameraLayer *getCamera(int initCameraType = 0)
{

  CameraLayer *camera = NULL;
  // if (initCameraType == 0 || initCameraType == 1)
  // {
  //   CameraLayer_GIGE_MindVision *camera_GIGE;
  //   camera_GIGE = new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV, NULL);
  //   LOGV("initCamera");

  //   try
  //   {
  //     if (initCamera(camera_GIGE) == 0)
  //     {
  //       camera = camera_GIGE;
  //     }
  //     else
  //     {
  //       delete camera;
  //       camera = NULL;
  //     }
  //   }
  //   catch (std::exception &e)
  //   {
  //     delete camera;
  //     camera = NULL;
  //   }
  // }

  std::string target_name_part = "";
  if (camera == NULL)
  {
    camera = initCamera_MindVision(target_name_part);
  }
  if (camera == NULL)
  {
    LOGI(">>>>>\n");
    camera = initCamera_Aravis(target_name_part);
  }
  if (camera == NULL)
  {
    LOGI(">>>>>\n");
    camera = initCamera_HikRobot_Camera(target_name_part);
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

bool terminationFlag = false;
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
    catch (exception &e)
    {
      retryCount++;
      int delaySec = 5;
      LOGE("websocket server open retry:%d wait for %dsec", retryCount, delaySec);
      std::this_thread::sleep_for(std::chrono::milliseconds(delaySec * 1000));
    }
  }

  if (terminationFlag)
    return -1;
  std::thread InspThread(ImgPipeProcessThread, &terminationFlag);
  setThreadPriority(InspThread, SCHED_RR, -20);
  std::thread ActionThread(ImgPipeActionThread, &terminationFlag);
  setThreadPriority(ActionThread, SCHED_RR, 0);
  LOGI(">>>>>\n");

  std::thread _inspSnapSaveThread(InspSnapSaveThread, &terminationFlag);
  setThreadPriority(_inspSnapSaveThread, SCHED_RR, 19);

  {

    CameraLayer *camera = getCamera(CamInitStyle);

    for (int i = 0; camera == NULL; i++)
    {
      LOGI("Camera init retry[%d]...", i);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      camera = getCamera(CamInitStyle);
    }
    calib_bacpac.cam = camera;
    LOGI("DatCH_BPG1_0 camera :%p", camera);

    CameraSettingFromFile(camera, "data/");

    LOGI("CameraSettingFromFile OK");
    cb->camera = camera;
    callbk_obj.camera = camera;

    BPG_protocol->SetEventCallBack(cb, NULL);
  }
  LOGI("Camera:%p", cb->camera);

  {
    cJSON *json_mac_setting = ReadJson("data/machine_setting.json");
    if (json_mac_setting)
    {
      setup_machine_setting(json_mac_setting);
      cJSON_Delete(json_mac_setting);
    }
  }

  websocket->SetEventCallBack(&callbk_obj, websocket);
  mjpegS = new MJPEG_Streamer2(7603);
  LOGI("SetEventCallBack is set...");
  while (1)
  {

    int maxfd = websocket->findMaxFd();
    int ms_maxfd = mjpegS->GetMaxfd();
    if (maxfd < ms_maxfd)
    {
      maxfd = ms_maxfd;
    }

    fd_set fdset = websocket->get_fd_set();
    mjpegS->setFdset(&fdset);

    if (select(maxfd + 1, &fdset, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }
    // LOGI("GO RECV");
    websocket->runLoop(&fdset, NULL);
    mjpegS->fdEventFetch(&fdset);
  }

  return 0;
}
void sigroutine(int dunno)
{ /* 信號處理常式，其中dunno將會得到信號的值 */
  switch (dunno)
  {
  case SIGINT:
    LOGE("Get a signal -- SIGINT \n");
    LOGE("Tear down websocket.... \n");
    delete websocket;
    terminationFlag = true;
    break;
  }
  return;
}

void CameraLayer_Callback_BMP(CameraLayer &cl_obj, int type, void *context)
{
  CameraLayer_BMP &clBMP = *((CameraLayer_BMP *)&cl_obj);
  LOGV("Called.... %d, filename:%s", type, clBMP.GetCurrentFileName().c_str());
}

int simpleTest(char *imgName, char *defName)
{
  //return testGIGE();;

  acvImage newImg;
  int ret = LoadIMGFile(&newImg, imgName);
  if (ret)
  {
    LOGE("LoadBMP failed: ret:%d", ret);
    return -1;
  }
  ImgInspection_DefRead(matchingEng, &newImg, 1, defName, &calib_bacpac);

  const FeatureReport *report = matchingEng.GetReport();

  if (report != NULL)
  {
    cJSON *jobj = matchingEng.FeatureReport2Json(report);
    AttachStaticInfo(jobj, cb);
    char *jstr = cJSON_Print(jobj);
    cJSON_Delete(jobj);
    LOGI("...\n%s\n...", jstr);
  }
  printf("Start to send....\n");

  return 0;
}

int parseCM_info(PerifProt::Pak pakCM, acvCalibMap *setObj)
{
  int count = -1;
  count = PerifProt::countValidArr(&pakCM);
  if (count <= 0)
  {
    return -1;
  }
  PerifProt::Pak p2 = PerifProt::parse(pakCM.data);
  PerifProt::Pak IF_pak, DM_pak, MX_pak, MY_pak, DS_pak;
  int ret;

  ret = PerifProt::fetch(&pakCM, "IF", &IF_pak); //if(ret<0)return ret;
  ret = PerifProt::fetch(&pakCM, "DM", &DM_pak);
  if (ret < 0)
    return -2;
  ret = PerifProt::fetch(&pakCM, "DS", &DS_pak);
  if (ret < 0)
    return -3;
  ret = PerifProt::fetch(&pakCM, "MX", &MX_pak);
  if (ret < 0)
    return -4;
  ret = PerifProt::fetch(&pakCM, "MY", &MY_pak);
  if (ret < 0)
    return -5;

  PerifProt::Pak CB_pak, OB_pak;
  PerifProt::Pak MB_pak;
  {
    ret = PerifProt::fetch(&pakCM, "CB", &CB_pak);
    if (ret < 0)
      return -6;
    ret = PerifProt::fetch(&pakCM, "OB", &OB_pak);
    if (ret < 0)
      return -7;

    ret = PerifProt::fetch(&pakCM, "MB", &MB_pak);
    if (ret < 0)
      return -7;
    LOGI("CB:%f  OB:%f", ((double *)CB_pak.data)[0], ((double *)OB_pak.data)[0]);
    LOGI("MB:%f ", ((double *)MB_pak.data)[0]);
  }

  //The dimension of image
  uint32_t dim[2]; //the original dimension
  {
    int bytes_per_data = DM_pak.length / 2;

    if (bytes_per_data == 4)
    {
      uint32_t *tmp = (uint32_t *)DM_pak.data;
      dim[0] = tmp[0];
      dim[1] = tmp[1];
    }
    else if (bytes_per_data == 8)
    {
      uint64_t *tmp = (uint64_t *)DM_pak.data;
      dim[0] = tmp[0];
      dim[1] = tmp[1];
    }
  }

  //The dimension of calib map
  //Downscaled dimension(the forwardCalibMap)
  uint32_t dimS[2];
  {
    int bytes_per_data = DS_pak.length / 2;

    if (bytes_per_data == 4)
    {
      uint32_t *tmp = (uint32_t *)DS_pak.data;
      dimS[0] = tmp[0];
      dimS[1] = tmp[1];
    }
    else if (bytes_per_data == 8)
    {
      uint64_t *tmp = (uint64_t *)DS_pak.data;
      dimS[0] = tmp[0];
      dimS[1] = tmp[1];
    }
  }

  double *MX_data = (double *)MX_pak.data;
  double *MY_data = (double *)MY_pak.data;

  setObj->RESET();
  LOGI("MX_data:%p  MY_data:%p dimS:%d %d dim:%d %d..", MX_data, MY_data, dimS[0], dimS[1], dim[0], dim[1]);
  setObj->SET(MX_data, MY_data, dimS[0], dimS[1], dim[0], dim[1]);
  
  LOGI("CB:%f  MB:%f", ((double *)CB_pak.data)[0],((double *)MB_pak.data)[0]);
  setObj->calibPpB = 1/((double *)CB_pak.data)[0];
  setObj->calibmmpB = ((double *)MB_pak.data)[0];

  LOGI("calibmmpB:%f", setObj->calibmmpB);
  return 0;
}

int testCode()
{
  {

    CameraLayer *cam = getCamera(0);
    int ret = LoadCameraCalibrationFile("data/default_camera_param.json", calib_bacpac.sampler);

    LOGI("mmpB:%f  calibPpB:%f", calib_bacpac.sampler->getCalibMap()->calibmmpB, calib_bacpac.sampler->getCalibMap()->calibPpB);
    LOGI("mmpp:%.9f", calib_bacpac.sampler->mmpP_ideal());
    acv_XY loca = {X : 1000, Y : 10};
    LOGI("0__ %f  %f ___", loca.X, loca.Y);
    calib_bacpac.sampler->img2ideal(&loca);
    LOGI("1__ %f  %f ___", loca.X, loca.Y);
    calib_bacpac.sampler->ideal2img(&loca);
    LOGI("2__ %f  %f ___", loca.X, loca.Y);
    char *string = ReadText("data/FM_gen.json");
    matchingEng.ResetFeature();
    matchingEng.AddMatchingFeature(string);

    acvImage bw_img;
    ret = LoadIMGFile(&bw_img, "data/gen_TEST/B.BMP");

    ret = ImgInspection(matchingEng, &bw_img, &calib_bacpac, calib_bacpac.cam, 1);
    const FeatureReport *report = matchingEng.GetReport();
    delete (string);

    if (report != NULL)
    {
      cJSON *jobj = matchingEng.FeatureReport2Json(report);
      AttachStaticInfo(jobj, cb);
      //cJSON_AddNumberToObject(jobj, "session_id", session_id);
      char *jstr = cJSON_Print(jobj);
      cJSON_Delete(jobj);

      LOGI("__\n %s  \n___", jstr);

      delete jstr;
    }

    return 1;
  }

  return 0;
}
#include <vector>
int cp_main(int argc, char **argv)
{
  // {

  //   tmpMain();
  // }

  srand(time(NULL));

  // for(int i=0;i<10;i++)
  // {
  //   float f[]={0.3,0.2,0.4,0.7,1.1,1.5,1.8,2,2.3,2.6};

  //   const int Len=sizeof(f)/sizeof(f[0]);
  //   for(int i=0;i<Len;i++)
  //   {
  //     float noise=(float)(rand()%10000)/10000;
  //     f[i]+=noise*0.1;
  //   }

  //   float gMG;
  //   float gMIdx = findGradMaxIdx_spline(f,Len,&gMG);

  //   float df[Len];
  //   for(int i=1;i<Len-1;i++)
  //   {
  //     df[i]=f[i+1]-f[i-1];
  //   }
  //   df[0]=df[1];
  //   df[Len-1]=df[Len-2];

  //   float MG;
  //   float MIdx = findMaxIdx_spline(df,Len,&MG);

  //   float mean=NAN,sigma=NAN;
  //   calc_pdf_mean_sigma(df,Len,&mean,&sigma);

  //   // LOGI("gMIdx:%f gMG:%f",gMIdx,gMG);
  //   // LOGI(" MIdx:%f  MG:%f",MIdx,MG);

  //   LOGI("mean:%f sigma:%f",mean,sigma);

  // }
  // return -1;
  calib_bacpac.sampler = new ImageSampler();
  neutral_bacpac.sampler = new ImageSampler();

  // int sret = LoadCameraCalibrationFile("data/default_camera_param.json",calib_bacpac.sampler);
  // acv_XY xy={20,30};
  // float ddd = calib_bacpac.sampler->getStageLightInfo()->factorSampling(xy);
  // LOGI("ddd:%f",ddd);
  // return 0;
  // if(testCode()!=0)return -1;

/*auto lambda = []() { LOGV("Hello, Lambda"); };
  lambda();*/
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

  char buffer[512]; //force output run with buffer mode(print when buffer is full) instead of line buffered mode
  //this speeds up windows print dramaticlly
  setvbuf(stdout, buffer, _IOFBF, sizeof(buffer));

#endif

  _argc = argc;
  _argv = argv;
  for (int i = 0; i < argc; i++)
  {
    bool doMatch = true;
    if (strcmp(argv[i], "CamInitStyle=0") == 0)
    {
      CamInitStyle = 0;
    }
    else if (strcmp(argv[i], "CamInitStyle=1") == 0)
    {
      CamInitStyle = 1;
    }
    else if (strcmp(argv[i], "CamInitStyle=2") == 0)
    {
      CamInitStyle = 2;
    }
    else
    {
      doMatch = false;
      LOGE("unknown param[%d]:%s", i, argv[i]);
    }

    if (doMatch)
    {
      LOGE("CMD param[%d]:%s ...OK", i, argv[i]);
    }
  }

  if (0)
  {

    acvImage calibImage;
    acvImage test_buff;
    int ret_val = LoadIMGFile(&calibImage, "data/calibImg.BMP");
    if (ret_val != 0)
      return -1;
    ImgInspection_DefRead(matchingEng, &calibImage, 1, "data/cameraCalibration.json", &calib_bacpac);

    const FeatureReport *report = matchingEng.GetReport();

    if (report != NULL)
    {
      cJSON *jobj = matchingEng.FeatureReport2Json(report);
      AttachStaticInfo(jobj, cb);
      //cJSON_AddNumberToObject(jobj, "session_id", session_id);
      char *jstr = cJSON_Print(jobj);
      cJSON_Delete(jobj);

      LOGI("__\n %s  \n___", jstr);

      delete jstr;
    }
    return 0;
  }

  if (0)
  {
    char *imgName = "data/BMP_carousel_test/01-02-23-18-53-491.bmp";
    char *defName = "data/calib_test_line.hydef";

    //char *imgName="data/calib_cam1_surfaceGo.bmp";
    //char *defName = "data/cameraCalibration.json";
    //
    return simpleTest(imgName, defName);
  }

  if (0) //GenBG map
  {

    acvImage BuffImage;
    acvImage BGImage;
    acvImage BGImage_Ori;
    int ret_val = LoadIMGFile(&BGImage, "data/BG.BMP");
    if (ret_val)
      return ret_val;
    BuffImage.ReSize(&BGImage);
    BGImage_Ori.ReSize(&BGImage);
    acvCloneImage(&BGImage, &BGImage_Ori, 0);

    acvWindowMax(&BGImage, 5);

    acvBoxFilter(&BuffImage, &BGImage, 20);
    acvBoxFilter(&BuffImage, &BGImage, 20);
    acvBoxFilter(&BuffImage, &BGImage, 20);
    acvBoxFilter(&BuffImage, &BGImage, 20);

    acvCloneImage(&BGImage, &BGImage, 0);

    if (BGImage_Ori.GetHeight() == BGImage.GetHeight() && BGImage_Ori.GetWidth() == BGImage.GetWidth())
    {
      for (int i = 0; i < BGImage_Ori.GetHeight(); i++)
      {
        for (int j = 0; j < BGImage_Ori.GetWidth(); j++)
        {
          int BG_Ori = BGImage_Ori.CVector[i][3 * j];
          int BG = BGImage.CVector[i][3 * j];
          int diff = BG_Ori - BG - 5;
          if (diff < 0)
            diff = -diff;
          diff *= 3;
          if (diff > 255)
            diff = 255;

          BGImage_Ori.CVector[i][3 * j] = diff;
          BGImage_Ori.CVector[i][3 * j + 1] = diff;
          BGImage_Ori.CVector[i][3 * j + 2] = diff;
        }
      }
    }
    SaveIMGFile("data/BGImage_OriX.bmp", &BGImage_Ori);
    SaveIMGFile("data/proBG.bmp", &BGImage);

    return 0;
  }
  signal(SIGINT, sigroutine);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
