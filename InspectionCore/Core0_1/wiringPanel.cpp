#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <CameraLayerManager.hpp>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>

#define _VERSION_ "1.2"
char* SNAP_FILE_EXTENSION="xreps";
char* SNAP_IMG_EXTENSION="jpg";
std::timed_mutex mainThreadLock;

std::mutex matchingEnglock;


bool SKIP_NA_DATA_VIEW=false;

int imageQueueSkipSize = 0;
int datViewQueueSkipSize = 0;
int DATA_VIEW_MAX_FPS=20;
bool DATA_VIEW_INSP_DATA_MUST_WITH_IMG=false;

float OK_MAX_FPS=6;
float NG_MAX_FPS=6;
float NA_MAX_FPS=6;

CameraLayerManager camLayerMan;
cJSON *cache_deffile_JSON = NULL;

cJSON *cache_camera_param = NULL;

bool img_transpose=false;
bool saveInspFailSnap = true;
bool saveInspNASnap = true;
int saveInspQFullSkipCount=0;
int save_snap_folder_full_delete_count=0;

const std::string InspSampleSavePath_DEFAULT("data/SAMPLE");
std::string InspSampleSavePath = InspSampleSavePath_DEFAULT;
int InspSampleSaveMaxCount = 1000;




const int resourcePoolSize = 30;

std::mutex lastDatViewCache_lock;
image_pipe_info *lastDatViewCache=NULL;
TSQueue<image_pipe_info *> inspQueue(10);
TSQueue<image_pipe_info *> datViewQueue(10);
TSQueue<image_pipe_info *> inspSnapQueue(5);
#define MT_LOCK(...) mainThreadLock_lock(__LINE__ VA_ARGS(__VA_ARGS__))
#define MT_UNLOCK(...) mainThreadLock_unlock(__LINE__ VA_ARGS(__VA_ARGS__))


int sendcJsonTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json);
int printfTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...);
int sendResultTo_perifCH(PerifChannel *perifCH,int uInspStatus, uint64_t timeStamp_100us);


void image_pipe_info_occupyFlag_set(image_pipe_info &pinfo,image_pipe_info_OccupyFIdx fidx)
{
  pinfo.occupyFlag|=(((typeof(pinfo.occupyFlag))1)<<fidx);
}
void image_pipe_info_occupyFlag_clr(image_pipe_info &pinfo,image_pipe_info_OccupyFIdx fidx)
{
  pinfo.occupyFlag&=~(((typeof(pinfo.occupyFlag))1)<<fidx);
}


bool image_pipe_info_gc(image_pipe_info &info,resourcePool<image_pipe_info> &pool)
{
  if(info.occupyFlag!=0)return false;
  pool.retResrc(&info);
  return true;
}
bool image_pipe_info_resendCache_swap_and_gc(image_pipe_info &info,resourcePool<image_pipe_info>&pool)
{
  if(lastDatViewCache==&info)return true;
  std::lock_guard<std::mutex> guard(lastDatViewCache_lock);
  image_pipe_info_occupyFlag_set(info,image_pipe_info_OccupyFIdx::resendCache);
  if(lastDatViewCache==NULL)
  {
    lastDatViewCache=&info;
    return true;
  }
  image_pipe_info *bk_Cache=lastDatViewCache;
  lastDatViewCache=&info;
  image_pipe_info_occupyFlag_clr(*bk_Cache,image_pipe_info_OccupyFIdx::resendCache);
  return image_pipe_info_gc(*bk_Cache,pool);
}

void InspResultAction_s(image_pipe_info *imgPipe, bool *skipInspDataTransfer, bool *skipImageTransfer, bool *inspSnap, bool *ret_pipe_pass_down, float datViewMaxFPS,bool pureSendImg);

void InspResultAction(image_pipe_info *imgPipe, bool *skipInspDataTransfer, bool *skipImageTransfer, bool *inspSnap, bool *ret_pipe_pass_down, float datViewMaxFPS=10)
{
  InspResultAction_s(imgPipe,skipInspDataTransfer,skipImageTransfer,inspSnap,ret_pipe_pass_down,datViewMaxFPS,false);
}
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
  return 0;
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

  return 0;
  //LOGI("%s_%d: unLocking ",msg,call_lineNumber);
  mainThreadLock.unlock();
  //LOGI("%s_%d: unLocked ",msg,call_lineNumber);

  return 0;
}




// int MicroInsp_FType::ev_on_close()
// {

//   //MT_LOCK(""); //the delete caller might come within main thread
//   int fd = getfd();
//   LOGE("fd:%d is disconnected  conn_pgID:%d", fd,conn_pgID);
//   char tmp[70];
//   sprintf(tmp, "{\"type\":\"DISCONNECT\",\"CONN_ID\":%d}", fd);

//   BPG_protocol_data bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("PD", tmp);
//   bpg_dat.pgID=conn_pgID;
//   BPG_protocol_send(bpg_dat);
//   bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("SS", "{}");
//   bpg_dat.pgID = conn_pgID;
//   BPG_protocol_send(bpg_dat);
//   //MT_UNLOCK("");

//   return 0;
// }



m_BPG_Protocol_Interface bpg_pi;

class PerifChannel:public Data_JsonRaw_Layer
{
  
  public:

  uint16_t conn_pgID;
  int pkt_count = 0;
  int ID;
  PerifChannel():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
  }
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
    
    if(opcode==1 )
    {

      char tmp[1024];
      sprintf(tmp, "{\"type\":\"MESSAGE\",\"msg\":%s,\"CONN_ID\":%d}", raw, ID);
      // LOGI("MSG:%s", tmp);
      BPG_protocol_data bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("PD", tmp);
      bpg_dat.pgID=conn_pgID;
      bpg_pi.fromUpperLayer(bpg_dat);

      bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("SS", "{}");
      bpg_dat.pgID = conn_pgID;
      bpg_pi.fromUpperLayer(bpg_dat);
      return 0;

    }
    printf(">>opcode:%d\n",opcode);
    return 0;


  }

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



void transpose(acvImage* dst,acvImage* src)
{
  dst->ReSize(src->GetHeight(),src->GetWidth());
  for(int i=0;i<src->GetHeight();i++)
  {
    for(int j=0;j<src->GetWidth();j++)
    {
      // dst->CVector[j][i*3+0]=src->CVector[i][j*3+0];
      // dst->CVector[j][i*3+1]=src->CVector[i][j*3+1];
      // dst->CVector[j][i*3+2]=src->CVector[i][j*3+2];
      memcpy(dst->CVector[j]+i*3,src->CVector[i]+j*3,3);
    }
  }
}



class ImageStackAddUp
{
  std::mutex lock;
public:
  int stackingC = 0;
  acvImage imgStacked;
  acvImage imgExtract;

  void acvImageAddUp_1CH(acvImage *imgOut, acvImage *imgIn)
  {
    std::lock_guard<std::mutex> guard(lock);
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
    std::lock_guard<std::mutex> guard(lock);
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
    std::lock_guard<std::mutex> guard(lock);
    memset((void *)imgOut->CVector[0], 0, imgOut->GetHeight() * imgOut->GetWidth() * 3);
  }

  void ReSize(acvImage *ref)
  {
    std::lock_guard<std::mutex> guard(lock);
    imgStacked.ReSize(ref);
    Reset();
  }

  void Reset()
  {
    std::lock_guard<std::mutex> guard(lock);
    stackingC = 0;
    //acvImageClear(&imgStacked);
  }

  void Add(acvImage *imgIn)
  {
    std::lock_guard<std::mutex> guard(lock);
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
    std::lock_guard<std::mutex> guard(lock);
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
    std::lock_guard<std::mutex> guard(lock);
    Export(&imgExtract);
  }

  bool DiffBigger(acvImage *img2, float globalDiffThres, int localDiffThres, int skipSampling = 10)
  {
    std::lock_guard<std::mutex> guard(lock);
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

int imgStackingMaxCount=0;
ImageStackAddUp imstack;

m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;

MJPEG_Streamer *mjpegS;
MatchingEngine matchingEng;
CameraLayer *gen_camera;
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

std::timed_mutex BPG_protocol_lock;


int _argc;
char **_argv;

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
            
          //   LOGI("X:%d,%d bri:%f %f,%f=>%f,%f  samp:%p", j, i,bri,coord2[0]  ,coord2[1],coord[0],coord[1],sampler);
          // }else
          // {

          // }
          float bgr[3];
          sampler->sampleImage3_IdealCoord(&src, coord,bgr, doNearest);

          if ( bgr[0] > 255)
             bgr[0] = 255;
          if ( bgr[1] > 255)
             bgr[1] = 255;
          if ( bgr[2] > 255)
             bgr[2] = 255;
          BSum += bgr[0];
          GSum += bgr[1];
          RSum += bgr[2];
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
void AttachStaticInfo(cJSON *reportJson, m_BPG_Protocol_Interface *BPG_prot_if)
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

    if (BPG_prot_if && BPG_prot_if->cameraFramesLeft >= 0)
    {
      LOGI("BPG_prot_if->cameraFramesLeft:%d", BPG_prot_if->cameraFramesLeft);
      cJSON_AddNumberToObject(reportJson, "frames_left", BPG_prot_if->cameraFramesLeft);
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
      
      if (JFetEx_NUMBER(root, "reports[0].ppb2b") != NULL)
      {
        ret_param->getCalibMap()->calibPpB = *JFetEx_NUMBER(root, "reports[0].ppb2b");
        LOGI("Override calibPpB:%f", ret_param->getCalibMap()->calibPpB);
      }
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
    stageLightInfo->nodesUpdate();

  } while (false);
  return 0;
}


void downSampSetup(CameraLayer &camera, cJSON &settingJson)
{
  
  double *val = JFetch_NUMBER(&settingJson, "down_samp_level");
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
}

int CameraSetup(CameraLayer &camera, cJSON &settingJson)
{
  downSampSetup(camera, settingJson);
  camera.StopAquisition();
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


  {
    val = JFetch_NUMBER(&settingJson, "trigger_mode");
    if (val)
    {
      camera.TriggerMode((int)*val);
      retV = 0;
    }
  }


  {
    int type=getDataFromJson(&settingJson, "transpose", NULL);
    if(type==cJSON_True)
    {
      img_transpose=true;
    }

    if(type==cJSON_False)
    {
      img_transpose=false;
    }
  }


  {
    
    val = JFetch_NUMBER(&settingJson, "blacklevel");
    if (val)
    {
      CameraLayer::status ret=camera.SetBalckLevel(*val);
      LOGI("SetBalckLevel:%f  ret:%d", *val,ret);
      retV = 0;
    }
  }

  {
    
    val = JFetch_NUMBER(&settingJson, "gamma");
    if (val)
    {
      CameraLayer::status ret=camera.SetGamma(*val);
      LOGI("SetGamma:%f  ret:%d", *val,ret);
      retV = 0;
    }
  }
  val = JFetch_NUMBER(&settingJson, "framerate");
  if (val)
  {
    camera.SetFrameRate((float)*val);
    LOGI("framerate:%f", *val);
    retV = 0;
  }

  if (getDataFromJson(&settingJson, "set_once_WB", NULL) == cJSON_True)
  {
    CameraLayer::status st = camera.SetOnceWB();
    LOGI("SetOnceWB:%d", st);
    retV = 0;
  }


  {
    val = JFetch_NUMBER(&settingJson, "RGain");
    if (val)
    {
      camera.SetRGain(*val);
    }

    val = JFetch_NUMBER(&settingJson, "GGain");
    if (val)
    {
      camera.SetGGain(*val);
    }


    val = JFetch_NUMBER(&settingJson, "BGain");
    if (val)
    {
      camera.SetBGain(*val);
    }


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
    if (roi_x && roi_y && roi_w && roi_h && ((*roi_w) * (*roi_h))>100)
    {

      int x,y,w,h;
      if(img_transpose)
      {
        x = *roi_y;
        y = *roi_x;
        w = *roi_h;
        h = *roi_w;
      }
      else
      {
        x = *roi_x;
        y = *roi_y;
        w = *roi_w;
        h = *roi_h;
      }
      camera.SetROI(x,y,w,h, 0, 0);
      // LOGI("ROI v:%f %f %f %f", *roi_x, *roi_y, *roi_w, *roi_h);
      int ox, oy;
      camera.GetROI(&ox, &oy, NULL, NULL, NULL, NULL);
      
      // LOGI("ROI v:%d %d", ox, oy);
      // acv_XY offset_o = {(float)ox, (float)oy};
      // calib_bacpac.sampler->setOriginOffset(offset_o);
      //sampler
    }
    else
    {
    }
  }
  
  camera.StartAquisition();
  return 0;
}

int saveInspectionSample(cJSON *inspectionReport, cJSON *camera_param, cJSON *deffile, acvImage *image, const char *fileName,const char* filename_extension=SNAP_FILE_EXTENSION,const char* img_extension=SNAP_IMG_EXTENSION)
{

  cJSON *reportsList = JFetch_ARRAY(inspectionReport, "reports[0].reports");
  if (reportsList == NULL)
    return -10;

  cJSON *camera_param_data = JFetch_OBJECT(camera_param, "reports[0]");
  if (camera_param_data == NULL)
    return -11;

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
  int ret_write_Len = WriteBytesToFile((uint8_t *)jstr, strlen(jstr), (filePath+"." + (std::string)filename_extension).c_str());
  delete (jstr);
  if (ret_write_Len < 0)
  {
    return -1;
  }

  int saveErr = SaveIMGFile((filePath+"." + (std::string)img_extension).c_str(), image);
  if (saveErr != 0)
  {
    return -2;
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



BPG_protocol_data m_BPG_Protocol_Interface::GenStrBPGData(char *TL, char *jsonStr)
{
  BPG_protocol_data BPG_dat = {0};
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

bool DoImageTransfer = true;

bool m_BPG_Protocol_Interface::checkTL(const char *TL, const BPG_protocol_data *dat)
{
  if (TL == NULL)
    return false;
  return (TL[0] == dat->tl[0] && TL[1] == dat->tl[1]);
}
uint16_t m_BPG_Protocol_Interface::TLCode(const char *TL)
{
  return (((uint16_t)TL[0] << 8) | TL[1]);
}
m_BPG_Protocol_Interface::m_BPG_Protocol_Interface() : resPool(resourcePoolSize)
{
  cacheImage.ReSize(1, 1);
}

void m_BPG_Protocol_Interface::delete_PeripheralChannel()
{

  if (perifCH)
  {
    LOGI("DELETING");
    delete perifCH;
    perifCH = NULL;
  }
  LOGI("DELETED...");
}




std::vector<uint8_t> image_send_buffer(40000);
int m_BPG_Protocol_Interface::SEND_acvImage(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo)
{
  if(callbackInfo==NULL)return -1;
  BPG_protocol_data send_dat;
  BPG_protocol_data_acvImage_Send_info *img_info = (BPG_protocol_data_acvImage_Send_info*)callbackInfo;

  acvImage *img=img_info->img;

  uint8_t header[]={
    0,0,
    
    (uint8_t)(img_info->offsetX >>8),
    (uint8_t)(img_info->offsetX),
    (uint8_t)(img_info->offsetY >>8),
    (uint8_t)(img_info->offsetY),

    (uint8_t)(img->GetWidth()>>8),
    (uint8_t)(img->GetWidth()),
    (uint8_t)(img->GetHeight()>>8),
    (uint8_t)(img->GetHeight()),
    (uint8_t)(img_info->scale),
    
    (uint8_t)(img_info->fullWidth >>8),
    (uint8_t)(img_info->fullWidth),
    (uint8_t)(img_info->fullHeight >>8),
    (uint8_t)(img_info->fullHeight),
  };

  {
    image_send_buffer.resize(dch.getHeaderSize()+sizeof(header));
    dch.headerSetup(&image_send_buffer[0], image_send_buffer.size(), data);

    memcpy(&image_send_buffer[dch.getHeaderSize()], header, sizeof(header));
    dch.toLinkLayer(&image_send_buffer[0], dch.getHeaderSize()+sizeof(header), false);

  }

  const int headerOffset=10;
  image_send_buffer.resize(headerOffset+10000);
  

  int rest_len =
    img->GetWidth()*
    img->GetHeight();

  uint8_t *img_pix_ptr=img->CVector[0];

  for(bool isKeepGoing=true;isKeepGoing && rest_len;)
  {
    int imgBufferDataSize=image_send_buffer.size()-headerOffset;
    uint8_t* imgBufferDataPtr=&image_send_buffer[headerOffset];
    // LOGI(">img_pix_ptr,%d %d,%d",img_pix_ptr[0],img_pix_ptr[1],img_pix_ptr[2]);
    int sendL = 0;
    for(int i=0;i<imgBufferDataSize-4;i+=4,img_pix_ptr+=3)
    {
      imgBufferDataPtr[i]=img_pix_ptr[2];
      imgBufferDataPtr[i+1]=img_pix_ptr[1];
      imgBufferDataPtr[i+2]=img_pix_ptr[0];
      imgBufferDataPtr[i+3]=255;
      sendL+=4;
      rest_len--;
      if(rest_len==0)
      {
        isKeepGoing=false;
        break;
      }
    }
    // LOGI("b[..]:%d %d %d %d... sendL:%d isKeepGoing:%d"
    // ,image_send_buffer[0]
    // ,image_send_buffer[1]
    // ,image_send_buffer[2]
    // ,image_send_buffer[3]
    // ,sendL,isKeepGoing);

    //gives linklayer enough(according to linklayer's requirment 
    //can be much bigger(find possible maximum size header of all linklayer types))
    dch.toLinkLayer(imgBufferDataPtr, sendL, isKeepGoing==false,headerOffset,0);
  }
  return 0;

} 





CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj)
{  
  if (type != CameraLayer::EV_IMG)
    return CameraLayer::NAK;
  acvImage *img=(acvImage*)obj;

  CameraLayer::frameInfo finfo= cl_obj.GetFrameInfo();
  LOGI("finfo:WH:%d,%d  img_transpose:%d",finfo.width,finfo.height,img_transpose);

  acvImage *tmp_img=img_transpose?new acvImage():img;

  tmp_img->ReSize(finfo.width,finfo.height);
  auto ret=cl_obj.ExtractFrame(img->CVector[0],3,finfo.width*finfo.height);

  if(img_transpose==true)
  {
    transpose(img,tmp_img);

    delete tmp_img;
    tmp_img=NULL;
  }


  {//change BGR image to RRR
    for (int i = 0; i < img->GetHeight(); i++)
    {
      for (int j = 0; j < img->GetWidth(); j++)
      {

        int tmp = img->CVector[i][3 * j+2];
        img->CVector[i][3 * j] = img->CVector[i][3 * j + 1]=tmp;
      }
    }
  }

  return ret;
}

int getImage(CameraLayer *camera,acvImage *dst_img,int trig_type=0,int timeout_ms=-1)
{
  return (camera->SnapFrame(SNAP_Callback,(void*)dst_img,trig_type,timeout_ms) == CameraLayer::ACK)?0:-1;
}



int m_BPG_Protocol_Interface::toUpperLayer(BPG_protocol_data bpgdat) 
{
  //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);

    BPG_protocol_data *dat = &bpgdat;

    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //      dat->pgID);
    cJSON *json = cJSON_Parse((char *)dat->dat_raw);
    char err_str[1000] = "\0";
    bool session_ACK = false;
    char tmp[200];    //For string construct json reply
    BPG_protocol_data bpg_dat; //Empty
    // {
    //     sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"DF\",\"IM\"]}",session_id);
    //     bpg_dat=GenStrBPGData("SS", tmp);
    //     datCH_BPG.data.p_BPG_protocol_data=&bpg_dat;
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
    MT_LOCK("");
  do
  {

    // if (checkTL("GS", dat) == false)
    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //       dat->pgID);
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
              session_ACK=true;
            }
            else
            {
              session_ACK = false;
            }
          }
          else if (strcmp(type, "__LAST_DATA_VIEW_CACHE_IMG__") == 0)
          {
            if(lastDatViewCache==NULL)
            {
              session_ACK = false;
            }
            else
            {
              
              SaveIMGFile(fileName, &lastDatViewCache->img);
              session_ACK=true;
            }
            
          }
          else if (strcmp(type, "__START_STACKING_IMG__") == 0)
          {
            
            imstack.Reset();
            imgStackingMaxCount=1;

            
            double *stacking_count = JFetch_NUMBER(json, "stacking_count");
            if (stacking_count)
            {
              imgStackingMaxCount=(int)*stacking_count;
              if(imgStackingMaxCount<0)imgStackingMaxCount=-1;
            }

            
          }
          else if (strcmp(type, "__STACKING_IMG__") == 0)
          {
            int retry=10;
            for(;retry>0;retry--)
            {
              if(imstack.stackingC>=imgStackingMaxCount)break;
              std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            
            session_ACK = false;
            if(retry==0)
            {
              //failed
            }
            else
            {
              tmp_buff.ReSize(&(imstack.imgStacked));
              // acvCloneImage(&imstack.imgStacked,&tmp_buff,0);
              // 
              imstack.Export(&tmp_buff);
              
              // calib_bacpac.sampler->ignoreCalib(false); //First, make the cacheImage to be a calibrated full res image
              // ImageDownSampling(tmp_buff, imstack.imgExtract, 1, calib_bacpac.sampler, false);

              if (tmp_buff.GetWidth() * tmp_buff.GetHeight() > 10)//just a random check
              {
                LOGI("SAVE IMG:%s",fileName);
                SaveIMGFile(fileName, &tmp_buff);
                session_ACK = true;
              }
            }
            
          }
          else if (strcmp(type, "__LAST_DATA_VIEW_CACHE_INFO__") == 0)
          {
            char *report_extension = JFetch_STRING(json, "report_extension");
            char *img_extension = JFetch_STRING(json, "img_extension");


            lastDatViewCache_lock.lock();

            int err = saveInspectionSample(lastDatViewCache->datViewInfo.report_json, cache_camera_param, cache_deffile_JSON, &lastDatViewCache->img, fileName,
              report_extension!=NULL?report_extension:SNAP_FILE_EXTENSION,
              img_extension!=NULL?img_extension:SNAP_IMG_EXTENSION);
            
            if(err==0)
            {
              session_ACK=true;
            }
            lastDatViewCache_lock.unlock();
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
          session_ACK = true;
        }

      } while (false);
    }
    else if (checkTL("FB", dat)) //[F]ile [B]rowsing
    {

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
        LOGI("DEPTH:%d",depth);
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
          // LOGI("size:%d,raw=>\n%s",bpg_dat.size,bpg_dat.dat_raw);
          bpg_dat.pgID = dat->pgID;
          fromUpperLayer(bpg_dat);
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
          else if (strcmp(itemType, "precess_queue_status") == 0)
          {
            cJSON *robj = cJSON_CreateObject();
            cJSON_AddItemToObject(retArr,itemType,robj);
            {
              cJSON *info = cJSON_CreateObject();
              cJSON_AddItemToObject(robj,"inspQueue",info);
              cJSON_AddNumberToObject(info,"capacity",inspQueue.capacity());
              cJSON_AddNumberToObject(info,"size",inspQueue.size());
            }
            {
              cJSON *info = cJSON_CreateObject();
              cJSON_AddItemToObject(robj,"datViewQueue",info);
              cJSON_AddNumberToObject(info,"capacity",datViewQueue.capacity());
              cJSON_AddNumberToObject(info,"size",datViewQueue.size());
            }
            {
              cJSON *info = cJSON_CreateObject();
              cJSON_AddItemToObject(robj,"inspSnapQueue",info);
              cJSON_AddNumberToObject(info,"capacity",inspSnapQueue.capacity());
              cJSON_AddNumberToObject(info,"size",inspSnapQueue.size());
            }

          }
          else if (strcmp(itemType, "snap_queue_skip_count") == 0)
          {
            cJSON_AddNumberToObject(retArr,itemType,saveInspQFullSkipCount);
          }
          else if (strcmp(itemType, "save_snap_folder_full_delete_count") == 0)
          {
            cJSON_AddNumberToObject(retArr,itemType,save_snap_folder_full_delete_count);
          }
          else if (strcmp(itemType, "camera_info") == 0)
          {

            cJSON *cam_info_jarr = cJSON_CreateArray();
            string jInfo=camera->getCameraJsonInfo();
            // LOGI("CAM_INFO..\n%s",jInfo.c_str());
            cJSON *cam_1 = cJSON_Parse(jInfo.c_str());
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
        
        fromUpperLayer(bpg_dat);
        free(jstr);
      }
    }
    else if (checkTL("LD", dat))
    {
      
      session_ACK = true;
      LOGI("DataType_BPG:[%c%c] data:\n%s", dat->tl[0], dat->tl[1],(char *)dat->dat_raw);
      do
      {

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          session_ACK=false;
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
              session_ACK=false;
              break;
            }
            LOGI("Read deffile:%s", filename);
            bpg_dat = GenStrBPGData("FL", fileStr);
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);
            free(fileStr);
          }
          catch (std::invalid_argument iaex)
          {
            snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
            LOGE("%s", err_str);
            session_ACK=false;
            break;
          }
        }

        char *deffile = (char *)JFetch(json, "deffile", cJSON_String);
        if(deffile!=NULL)
        {
          try
          {
            char *jsonStr = ReadText(deffile);
            if (jsonStr != NULL)
            {
              LOGI("Read deffile:%s", deffile);
              bpg_dat = GenStrBPGData("DF", jsonStr);
              bpg_dat.pgID = dat->pgID;
              fromUpperLayer(bpg_dat);
              free(jsonStr);
            }
            else
            {
              session_ACK=false;
              break;
            }
          }
          catch (std::invalid_argument iaex)
          {
            snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
            LOGE("%s", err_str);
            session_ACK=false;
            break;
          }

        }

        char *imgSrcPath = (char *)JFetch(json, "imgsrc", cJSON_String);
        if (imgSrcPath != NULL)
        {

          int ret_val = LoadIMGFile(&tmp_buff, imgSrcPath);
          if (ret_val == 0)
          {
            acvImage *srcImg = NULL;
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
            BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)default_scale};

            iminfo.fullHeight = srcImg->GetHeight();
            iminfo.fullWidth = srcImg->GetWidth();
            //std::this_thread::sleep_for(std::chrono::milliseconds(4000));//SLOW load test
            //acvThreshold(srcImdg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff, *srcImg, iminfo.scale, NULL);
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;

            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;

            bpg_dat.pgID = dat->pgID;
            fromUpperLayer(bpg_dat);
          }
          else
          {
            
            session_ACK=false;
            break;
          }
        }


      } while (false);
    }
    else if (checkTL("II", dat)) //[I]mage [I]nspection
    {
      calib_bacpac.sampler->ignoreCalib(false);
      neutral_bacpac.sampler->ignoreCalib(true);
      FeatureManager_BacPac *select_bacpac = &neutral_bacpac;

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
          else if(strcmp(imgSrcPath, "__SNAP_TMP_IMG__") == 0)
          {
            int ret_val = getImage(camera,&tmp_buff);
            if (ret_val == 0)
            {
              srcImg = &tmp_buff;
            }
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
          this->cameraFramesLeft = 0;
          camera->TriggerMode(1);
          break;
        }

        bool isCalibNA = false;
        cJSON *img_property = JFetch_OBJECT(json, "img_property");

        char *jsonStr = NULL;
        if (defInfo)
        {
          jsonStr = cJSON_Print(defInfo);

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

        
        {
          
          cJSON *defObj = cJSON_Parse(jsonStr);
          if(defObj==NULL)
          {
            snprintf(err_str, sizeof(err_str), "defObj: is not Available  LINE:%04d", __LINE__);
            LOGE("%s", err_str);
            break;
          }
          neutral_bacpac.sampler->getCalibMap()->calibPpB=
            JFetch_NUMBER_ex(defObj,"featureSet[0].cam_param.ppb2b");
          neutral_bacpac.sampler->getCalibMap()->calibmmpB=
            JFetch_NUMBER_ex(defObj,"featureSet[0].cam_param.mmpb2b");
        } 
        


        // bpg_dat = GenStrBPGData("DF", jsonStr);
        // bpg_dat.pgID = dat->pgID;
        // datCH_BPG.data.p_BPG_protocol_data = &bpg_dat;
        // self->SendData(datCH_BPG);

        //SaveIMGFile("data/TMP__.png",srcImg);

        try
        {
          //SaveIMGFile("data/buff.bmp",&test1_buff);

          // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
          int ret = ImgInspection_JSONStr(matchingEng, srcImg, 1, jsonStr, select_bacpac);
          free(jsonStr);
          const FeatureReport *report = matchingEng.GetReport();

          if (report != NULL)
          {
            
            cJSON *jobj = matchingEng.FeatureReport2Json(report);
            AttachStaticInfo(jobj, this);
            char *jstr = cJSON_Print(jobj);
            cJSON_Delete(jobj);

            //LOGI("__\n %s  \n___",jstr);
            bpg_dat = GenStrBPGData("RP", jstr);
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);

            delete jstr;
            session_ACK = true;
          }
          else
          {
            session_ACK = false;
          }
          
          // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");
        }
        catch (std::invalid_argument iaex)
        {
          snprintf(err_str, sizeof(err_str), "Caught an error! LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          break;
        }

        if (img_property)
        {
          double *pscale = JFetch_NUMBER(img_property, "down_samp_level");
          if (pscale)
          {
            int _scale = 2;
            _scale = (int)*pscale;

            bpg_dat = GenStrBPGData("IM", NULL);
            BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)_scale};

            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70

            ImageSampler *sampler = isCalibNA ? NULL : select_bacpac->sampler;
            iminfo.fullHeight = srcImg->GetHeight();
            iminfo.fullWidth = srcImg->GetWidth();
            ImageDownSampling(dataSend_buff, *srcImg, iminfo.scale, sampler, 0);

            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);
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
        calib_bacpac.sampler->ignoreCalib(false); //First, make the cacheImage to be a calibrated full res image
        saveInspFailSnap = false;
        saveInspNASnap = false;
        SKIP_NA_DATA_VIEW=false;
        saveInspQFullSkipCount=0;

        
        OK_MAX_FPS=6;
        NG_MAX_FPS=6;
        NA_MAX_FPS=6;
        // save_snap_folder_full_delete_count=0;
        double *frame_count = JFetch_NUMBER(json, "frame_count");
        this->cameraFramesLeft = (frame_count != NULL) ? ((int)(*frame_count)) : -1;
        int frameCount = (int)this->cameraFramesLeft;
        LOGI("this->cameraFramesLeft:%d frame_count:%p", this->cameraFramesLeft, frame_count);

        if (json == NULL)
        {
          snprintf(err_str, sizeof(err_str), "JSON parse failed LINE:%04d", __LINE__);
          LOGE("%s", err_str);
          break;
        }

        this->CI_pgID = dat->pgID;

        char *deffile = (char *)JFetch(json, "deffile", cJSON_String);
        // if (deffile == NULL)
        // {
        // snprintf(err_str,sizeof(err_str),"No entry:'deffile' in it LINE:%04d",__LINE__);
        // LOGE("%s",err_str);
        // this->cameraFeedTrigger=false;

        // camera->TriggerMode(1);
        // break;
        // }

        cJSON *defInfo = JFetch_OBJECT(json, "definfo");

        if (deffile == NULL && defInfo == NULL)
        {

          LOGE("No entry:'deffile':%p OR 'definfo(json)':%p ", __LINE__, deffile, defInfo);

          this->cameraFramesLeft = 0;

          camera->TriggerMode(1);
          break;
        }

        try
        {
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
              this->cameraFramesLeft = 0;

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

          // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
          matchingEng.ResetFeature();
          matchingEng.AddMatchingFeature(jsonStr);

          // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");
          void *target;
          if (getDataFromJson(json, "get_deffile", &target) == cJSON_True)
          {
            bpg_dat = GenStrBPGData("DF", jsonStr);
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);
          }
          free(jsonStr);
          //TODO: HACK: this sleep is to wait for the gap in between def config file arriving and inspection result arriving.
          //If the inspection result arrives without def config file then webUI will generate(by design) an statemachine error event.
          // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

          imageQueueSkipSize=inspQueue.capacity();//it will never hit the skip size
          datViewQueueSkipSize=datViewQueue.capacity();
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
            imageQueueSkipSize=1;
            datViewQueueSkipSize=1;
          }
          else if (dat->tl[0] == 'F') //"FI" is for full inspection
          {                           //no manual trigger and process in thread
            camera->TriggerMode(2);
            doImgProcessThread = true;
            
            datViewQueueSkipSize=2;
          }


          if (this->cameraFramesLeft > 0)
          {

            MT_UNLOCK("SPACING LOCK");
            while (this->cameraFramesLeft > 0)
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


        if (cJSON_True == getDataFromJson(json, "IMG_ignore_calib", NULL))
        {
          calib_bacpac.sampler->ignoreCalib(true); 
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
          int trigger_type=0;
          int imageFetchTimeout=-1;

          {
            double *val = JFetch_NUMBER(json, "trigger_type");
            if(val)trigger_type=(int)*val;
          }

          {
            double *val = JFetch_NUMBER(json, "timeout");
            if(val)imageFetchTimeout=(int)*val;
          }

          int ret_val = getImage(camera,&tmp_buff,trigger_type,imageFetchTimeout);
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

            // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
            ImgInspection_DefRead(matchingEng, srcImg, 1, "data/featureDetect.json", &calib_bacpac);
            const FeatureReport *report = matchingEng.GetReport();

            if (report != NULL)
            {
              cJSON *jobj = matchingEng.FeatureReport2Json(report);
              AttachStaticInfo(jobj, this);

              char *jstr = cJSON_Print(jobj);
              cJSON_Delete(jobj);

              //LOGI("__\n %s  \n___",jstr);
              bpg_dat = GenStrBPGData("SG", jstr); //SG report : signature360
              bpg_dat.pgID = dat->pgID;
              
              fromUpperLayer(bpg_dat);

              delete jstr;
            }
            else
            {
              sprintf(tmp, "{}");
              bpg_dat = GenStrBPGData("SG", tmp);
              bpg_dat.pgID = dat->pgID;
              
              fromUpperLayer(bpg_dat);
            }
            
            // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");
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
            BPG_protocol_data_acvImage_Send_info iminfo = {img : &dataSend_buff, scale : (uint16_t)tar_down_samp_level};
            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70

            calib_bacpac.sampler->ignoreCalib(true);
            ImageDownSampling(dataSend_buff, cacheImage, iminfo.scale, calib_bacpac.sampler, true);

            iminfo.fullHeight = cacheImage.GetHeight();
            iminfo.fullWidth = cacheImage.GetWidth();
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);
          }
          calib_bacpac.sampler->ignoreCalib(false);
          session_ACK = true;
        }
      }
        

    }
    else if (checkTL("RC", dat)) //[R]e[C]onnect
    {
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
        calib_bacpac.cam = camera;
      }
      else if (strcmp(target, "camera_setting_refresh") == 0)
      {
        LOGV("DatCH_BPG1_0:%p", camera);

        CameraSettingFromFile(camera, "data/");

        LOGV("DatCH_BPG1_0");
        this->camera = camera;
        calib_bacpac.cam = camera;
      }
    }
    else if (checkTL("SC", dat)) //[S]pecial [C]MD
    {
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
      
      fromUpperLayer(bpg_dat);
      delete jstr;
      session_ACK = true;
    }
    else if (checkTL("ST", dat)) //[S]e[T]ting
    {
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
        
        // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
        cJSON *retInfo = matchingEng.SetParam(InspectionParam);


        // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");


        char *jstr = cJSON_Print(retInfo);
        cJSON_Delete(retInfo);

        bpg_dat = GenStrBPGData("DT", jstr); //Special Return from cmd
        bpg_dat.pgID = dat->pgID;
        fromUpperLayer(bpg_dat);
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


        
        {
          double *num = JFetch_NUMBER(ImTranseSetup, "OK_MAX_FPS");
          if(num)
          {
            OK_MAX_FPS=(float)*num;
            // InspSampleSaveMaxCount=(int)*num;
          }
        }
        {
          double *num = JFetch_NUMBER(ImTranseSetup, "NG_MAX_FPS");
          if(num)
          {
            NG_MAX_FPS=(float)*num;
            // InspSampleSaveMaxCount=(int)*num;
          }
        }
        {
          double *num = JFetch_NUMBER(ImTranseSetup, "NA_MAX_FPS");
          if(num)
          {
            NA_MAX_FPS=(float)*num;
            // InspSampleSaveMaxCount=(int)*num;
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


      downSampSetup(*camera, *json);

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


      {

        double *num = JFetch_NUMBER(json, "INSP_NG_SNAP_MAX_NUM");
        if(num)
        {
          InspSampleSaveMaxCount=(int)*num;
        }
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

      double *maxImgStFPS = JFetch_NUMBER(json, "IMG_STREAMING_MAX_FPS");
      if(maxImgStFPS)
      {
        DATA_VIEW_MAX_FPS=(int)*maxImgStFPS;
      }


      auto IMG_STREAMING_SKIP_NA = getDataFromJson(json, "IMG_STREAMING_SKIP_NA",NULL);
      if (IMG_STREAMING_SKIP_NA == cJSON_True)
      {
        SKIP_NA_DATA_VIEW = true;
      }
      else if (IMG_STREAMING_SKIP_NA == cJSON_False)
      {
        SKIP_NA_DATA_VIEW = false;
      }

      auto LAST_FRAME_RESEND = getDataFromJson(json, "LAST_FRAME_RESEND", NULL);
      if (LAST_FRAME_RESEND == cJSON_True)
      {  
        session_ACK=false;
        LOGI(">>>>>LAST_FRAME_RESEND>>>>>");
        if(inspQueue.size()!=0 || datViewQueue.size()!=0)
        {
          //No
        }
        else if(lastDatViewCache!=NULL)
        {
          lastDatViewCache_lock.lock();
          LOGI("IMG resend !!!!");
          bool skipInspDataTransfer=true;
          bool skipImageTransfer=false;
          bool inspSnap=false;



          InspResultAction_s(lastDatViewCache, &skipInspDataTransfer, &skipImageTransfer , &inspSnap,NULL,2,true);

          LOGI("IMG resend DONE....!!!!");
          lastDatViewCache_lock.unlock();
          session_ACK=true;
        }
        
      }


    }
    else if (checkTL("PR", dat)) //for external application
    {
   
    }
    else if (checkTL("PD", dat)) //Peripheral device
    {
      char *type = JFetch_STRING(json, "type");

      double *_CONN_ID = JFetch_NUMBER(json, "CONN_ID");
      int CONN_ID=-1;
      if(_CONN_ID)
      {
        CONN_ID=(int)*_CONN_ID;
      }


      do{
        if(strcmp(type, "CONNECT") == 0)
        {
          if(CONN_ID!=-1)
          {
            sprintf(err_str, "CONNECT should not have CONN_ID(%d)", CONN_ID);
            break;
          }

          
          delete_PeripheralChannel();
          // char *conn_type = JFetch_STRING(json, "type");

          // if(strcmp(conn_type, "uart") == 0)
          // {
            
          // }
          // else if(strcmp(conn_type, "IP") == 0 || conn_type==NULL)
          // {
            
          // }
          
          int avail_CONN_ID=714;
          Data_Layer_IF *PHYLayer=NULL;
          char *uart_name = NULL;
          
          char *IP = NULL;
          if ( (uart_name=JFetch_STRING(json, "uart_name")) !=NULL)
          {
            double *baudrate = JFetch_NUMBER(json, "baudrate");
            char *default_mode="8N1";
            char *mode = JFetch_STRING(json, "mode");
            if(mode==NULL)
            {
              mode=default_mode;
            }

            if(baudrate==NULL)
            {
              sprintf(err_str, "baudrate is not defined");
              break;
            }



            try{
              
              PHYLayer=new Data_UART_Layer(uart_name,(int)*baudrate, mode);


            }
            catch(std::runtime_error &e){
             
            }

          }
          else if( (IP=JFetch_STRING(json, "ip"))!=NULL)
          {

            double *port_number = JFetch_NUMBER(json, "port");
            if (port_number == NULL)
            {
              sprintf(err_str, "IP(%d) port_number(%d)", IP!=NULL,port_number!=NULL);
              break;
            }
          

            try{
              
              PHYLayer=new Data_TCP_Layer(IP,(int)*port_number);

            }
            catch(std::runtime_error &e){
            }



          }

          if(PHYLayer!=NULL)
          {
            perifCH=new PerifChannel();
            perifCH->ID=avail_CONN_ID;
            perifCH->conn_pgID=dat->pgID;
            perifCH->setDLayer(PHYLayer);

            perifCH->send_RESET();
            perifCH->send_RESET();
            perifCH->RESET();


            session_ACK = true;

            sprintf(tmp, "{\"type\":\"CONNECT\",\"CONN_ID\":%d}", avail_CONN_ID);
            bpg_dat = GenStrBPGData("PD", tmp);
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat);

          }
          else
          {
            session_ACK = false;

            LOGE("PHYLayer is not able to eatablish");
            sprintf(err_str, "PHYLayer is not able to eatablish");
          }

          // if(perifCH!=NULL)
          // {
          //   sprintf(err_str, "perifCH still in connected state");
          //   break;
          // }


        }
        else if(strcmp(type, "DISCONNECT") == 0)
        {

          if(perifCH==NULL || perifCH->ID != CONN_ID)
          {
            sprintf(err_str, "CONN_ID(%d)  perifCH exist:%p or current perifCH has different CONN_ID", CONN_ID, perifCH);
            break;
          }
          
          if(CONN_ID==-1 || perifCH->ID == CONN_ID)
          {//disconnect
            delete_PeripheralChannel();
            session_ACK = true;
          }
          else
          {
            sprintf(err_str, "CONN_ID(%d)  dose not match ", CONN_ID);
            break;
          }


        }
        else if(strcmp(type, "MESSAGE") == 0)
        {
          if(CONN_ID==-1 || perifCH==NULL ||perifCH->ID != CONN_ID)
          {
            sprintf(err_str, "CONN_ID(%d)  perifCH exist:%d or current perifCH has different CONN_ID", CONN_ID, perifCH!=NULL);
            break;
          }

          cJSON *msg_obj = JFetch_OBJECT(json, "msg");
          if (msg_obj)
          {
            uint8_t _buf[2000];
            int ret= sendcJsonTo_perifCH(perifCH,_buf, sizeof(_buf),true,msg_obj);
            session_ACK = (ret>=0);
          }
          else
          {
            session_ACK=true;//send nothing
          }


        }
      }while(false);


    }
    sprintf(tmp, "{\"start\":false,\"cmd\":\"%c%c\",\"ACK\":%s,\"errMsg\":\"%s\"}",
            dat->tl[0], dat->tl[1], (session_ACK) ? "true" : "false", err_str);
    bpg_dat = GenStrBPGData("SS", tmp);
    bpg_dat.pgID = dat->pgID;

    fromUpperLayer(bpg_dat);
    cJSON_Delete(json);
  }
  while(0);

  MT_UNLOCK("");
  // if (doExit)
  // {
  //   exit(0);
  // }

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
  if (total_status == FeatureReport_sig360_circle_line_single::STATUS_UNSET)
    return new_status;
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
    if(jrep[k].def->quality_essential==true)
    {
      int cur_stat=jrep[k].status;

      // LOGI(">>>NAG:%d NGA:%d  cur_stat:%d",jrep[k].def->NAasNG,jrep[k].def->NGasNA, cur_stat);
      if(jrep[k].def->NAasNG 
      && cur_stat==FeatureReport_sig360_circle_line_single::STATUS_NA)
      {
        cur_stat=FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
      }
      if(jrep[k].def->NGasNA 
      && cur_stat==FeatureReport_sig360_circle_line_single::STATUS_FAILURE)
      {
        cur_stat=FeatureReport_sig360_circle_line_single::STATUS_NA;
      }

      stat = InspStatusReducer(stat, cur_stat);

    }
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


  if(inspQueue.size()>imageQueueSkipSize)//for responsiveness
  {//skip image if the queue is more than imageQueueSkipSize
  
    LOGE("skip image, inspQueue.size():%d>imageQueueSkipSize:%d\n", inspQueue.size(),imageQueueSkipSize);
    return CameraLayer::NAK;
  }

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
  LOGI("=============== frameInterval:%fms \n", interval);
  LOGI("bpg_pi->cameraFramesLeft:%d", bpg_pi.cameraFramesLeft);
  CameraLayer &cl_GMV = *((CameraLayer *)&cl_obj);

  CameraLayer::frameInfo finfo = cl_GMV.GetFrameInfo();
  
  // LOGE("finfo.wh:%d,%d", finfo.width,finfo.height);
  

  image_pipe_info *headImgPipe = bpg_pi.resPool.fetchResrc_blocking();
  if (headImgPipe == NULL)
  {
    LOGE("HEAD IMG pipe is NULL");
    return CameraLayer::NAK;
  }

  headImgPipe->camLayer = &cl_obj;
  headImgPipe->type = type;
  headImgPipe->context = context;
  headImgPipe->fi = finfo;
  headImgPipe->occupyFlag=0;
  acvImage *tmp_img=&(headImgPipe->img);
  tmp_img->ReSize(finfo.width,finfo.height);
  auto ret=cl_obj.ExtractFrame(tmp_img->CVector[0],3,finfo.width*finfo.height);

  // acvImage *tmp_img=img_transpose?new acvImage():&(headImgPipe->img);

  // tmp_img->ReSize(finfo.width,finfo.height);
  // auto ret=cl_obj.ExtractFrame(tmp_img->CVector[0],3,finfo.width*finfo.height);

  // if(img_transpose==true)
  // {
  //   transpose(&(headImgPipe->img),tmp_img);

  //   delete tmp_img;
  //   tmp_img=NULL;
  // }


  // {//change BGR image to RRR
  //   for (int i = 0; i < headImgPipe->img.GetHeight(); i++)
  //   {
  //     for (int j = 0; j < headImgPipe->img.GetWidth(); j++)
  //     {

  //       int tmp = headImgPipe->img.CVector[i][3 * j+2];
  //       headImgPipe->img.CVector[i][3 * j] = headImgPipe->img.CVector[i][3 * j + 1]=tmp;
  //     }
  //   }
  // }

  headImgPipe->bacpac = &calib_bacpac;

  if (doImgProcessThread)
  {

    // LOGE("bpg_pi.resPool.rest_size:: %d", bpg_pi.resPool.rest_size());

    if (inspQueue.push_blocking(headImgPipe) == false)
    {
      LOGE("NO resource can be used.....");
      // imagePipeBuffer.clear();

      if (bpg_pi.perifCH)
      {
        LOGI("perifCH is here too!!");
        uint8_t buffx[200];
        
        int ret= printfTo_perifCH(bpg_pi.perifCH,buffx, sizeof(buffx),true,
                "{"
                "\"type\":\"inspRep\",\"status\":%d,"
                "\"idx\":%d"
                "}",
                -10001, 1);
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
      bpg_pi.resPool.retResrc(headImgPipe);
  }
  return CameraLayer::ACK;
}


int sendcJsonTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json)
{

  if (bpg_pi.perifCH==NULL)
  {
    return -1;
  }
  int buff_head_room=perifCH->max_head_room_size();
  int buffSize=bufL-buff_head_room;
  char *padded_buf=(char*)buf+buff_head_room;

  int ret= cJSON_PrintPreallocated(json, padded_buf, buffSize-perifCH->max_leg_room_size(), false);

  if(ret == false)
  {
    return -1;
  }

  int contentSize=strlen(padded_buf);
  if(directStringFormat)
  {
    ret = perifCH->send_json_string(buff_head_room,(uint8_t*)padded_buf,contentSize,buffSize-contentSize);
  }
  else
  {
    ret = perifCH->send_string(buff_head_room,(uint8_t*)padded_buf,contentSize,buffSize-contentSize);
  }
  return ret;
}




int printfTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...)
{

  if (bpg_pi.perifCH==NULL)
  {
    return -1;
  }

  int buff_head_room=perifCH->max_head_room_size();
  int buffSize=bufL-buff_head_room;
  uint8_t *padded_buf=buf+buff_head_room;

  va_list aptr;
  int ret;
  va_start(aptr, fmt);
  ret = vsnprintf ((char*)padded_buf, buffSize-perifCH->max_leg_room_size(), fmt, aptr);
  va_end(aptr); 

  if(ret<0)return ret;

  int contentSize=ret;
  
  if(directStringFormat)
  {
    ret = perifCH->send_json_string(buff_head_room,padded_buf,contentSize,buffSize-contentSize);
  }
  else
  {
    ret = perifCH->send_string(buff_head_room,padded_buf,contentSize,buffSize-contentSize);
  }
  return ret;
}


int sendResultTo_perifCH(PerifChannel *perifCH,int uInspStatus, uint64_t timeStamp_100us,int count)
{
  uint8_t buffx[200];
  
  int ret= printfTo_perifCH(perifCH,buffx, sizeof(buffx),true,
    "{"
    "\"type\":\"inspRep\",\"status\":%d,"
    "\"idx\":%d,\"count\":%d,"
    "\"time_100us\":%lu"
    "}", uInspStatus, 1, count, timeStamp_100us);
  return ret;
}


          


float avgInterval=0;
uint64_t lastImgSendTime=0;
void InspResultAction_s(image_pipe_info *imgPipe, bool *skipInspDataTransfer, bool *skipImageTransfer, bool *inspSnap, bool *ret_pipe_pass_down, float datViewMaxFPS,bool pureSendImg)
{
  static int frameActionID = 0;
  if (ret_pipe_pass_down)
    *ret_pipe_pass_down = false;

  if (bpg_pi.cameraFramesLeft == 0)
  {
    // camera->TriggerMode(1);
    //MT_UNLOCK("");
    return;
  }
  if (bpg_pi.cameraFramesLeft > 0)
    bpg_pi.cameraFramesLeft--;
  
  MT_LOCK("InspResultAction lock");


  clock_t t = clock();
  
  uint64_t cur_ms = current_time_ms();
  float cur_Interval =cur_ms-lastImgSendTime;
  if(lastImgSendTime==0)
  {
    cur_Interval=1;
    lastImgSendTime=cur_ms;
  }

  float cur_avgInterval=avgInterval+(cur_Interval-avgInterval)*0.5;
  float cur_FPS=1000.0/cur_avgInterval;
  bool withinMinInterval=(cur_FPS)<datViewMaxFPS;

  // LOGI("cur_avgInterval:%0.2f cur_FPS:%0.2f datViewMaxFPS:%0.2f",cur_avgInterval,cur_FPS,datViewMaxFPS);
  
  // LOGI("skipRep:%d skipImg:%d cur_avgFPS:%0.2f",skipInspDataTransfer,skipImageTransfer,cur_avgFPS);
  if(withinMinInterval==false)//the interval is too short
  {
    // skipInspDataTransfer=
    *skipImageTransfer=true;
  }

  if(DoImageTransfer==false)
  {
    *skipImageTransfer=false;
  }

  if(DATA_VIEW_INSP_DATA_MUST_WITH_IMG)
  {
    if(*skipInspDataTransfer==false)
    {
      *skipInspDataTransfer=*skipImageTransfer;
    }
  }

  acvImage &capImg = imgPipe->img;
  FeatureManager_BacPac *bacpac = imgPipe->bacpac;
  CameraLayer::frameInfo &fi = imgPipe->fi;

  BPG_protocol_data bpg_dat;

  char tmp[200];

  if (*skipInspDataTransfer == false)
  do
  {
    // sendResultTo_perifCH(imgPipe->datViewInfo.uInspStatus,fi.timeStamp_100us);

    sprintf(tmp, "{\"start\":true}");
    bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("SS", tmp);
    bpg_dat.pgID = bpg_pi.CI_pgID;
    
    bpg_pi.fromUpperLayer(bpg_dat);

    try
    {
      // LOGI(">>>>");

      cJSON *jobj = imgPipe->datViewInfo.report_json;
      AttachStaticInfo(jobj, &bpg_pi);
      // double expTime = NAN;
      // if (CameraLayer::ACK == imgPipe->camLayer->GetExposureTime(&expTime))
      // {
      //   cJSON_AddNumberToObject(jobj, "exposure_time", expTime);
      // }
      char *jstr = cJSON_Print(jobj);

      // LOGI("__\n %s  \n___",jstr);
      bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("RP", jstr);
      bpg_dat.pgID = bpg_pi.CI_pgID;
      
      bpg_pi.fromUpperLayer(bpg_dat);

      delete jstr;
    }
    catch (std::invalid_argument iaex)
    {
      LOGE("Caught an error!");
    }
  } while (false);

  if (*skipImageTransfer == false)
  do
  {
    // LOGI(">>>>");
    clock_t img_t = clock();
    static acvImage test1_buff;

    BPG_protocol_data_acvImage_Send_info iminfo;
    bool sendJpg = false;

    if (sendJpg && mjpegS)
    {
      acvImage *sendImg = &capImg;

      if (sendImg == NULL)
      {
        sendImg = &test1_buff;
        iminfo = (BPG_protocol_data_acvImage_Send_info){img : &test1_buff, scale : 2};

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
    if ((!sendJpg))
    {
      int _downSampLevel=downSampLevel;

      {

        if (_downSampLevel <= 0)
        {
          _downSampLevel = 1;
        }
        // if(downSampLevel==7)
        //   downSampLevel=5;
        // else
        //   downSampLevel=7;
        iminfo = (BPG_protocol_data_acvImage_Send_info){img : &test1_buff, scale : (uint16_t)_downSampLevel};

        iminfo.offsetX = (ImageCropX / _downSampLevel) * _downSampLevel;
        iminfo.offsetY = (ImageCropY / _downSampLevel) * _downSampLevel;

        iminfo.fullHeight = capImg.GetHeight();
        iminfo.fullWidth = capImg.GetWidth();
        int cropW = ImageCropW;
        int cropH = ImageCropH;

        // LOGI(">>>>");
        ImageSampler *sampler = (true) ? bacpac->sampler : NULL;
        //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
        ImageDownSampling(test1_buff, capImg, _downSampLevel, sampler, 1,
                          iminfo.offsetX, iminfo.offsetY, cropW, cropH);
      }

      bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("IM", NULL);
      //BPG_protocol_data_acvImage_Send_info iminfo={img:&test1_buff,scale:4};
      iminfo.scale=_downSampLevel;
      // LOGI(">>>>");
      bpg_dat.callbackInfo = (uint8_t *)&iminfo;
      bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
      bpg_dat.pgID = bpg_pi.CI_pgID;
      
      bpg_pi.fromUpperLayer(bpg_dat);
      LOGI("img transfer(DL:%d) %fms \n", _downSampLevel, ((double)clock() - img_t) / CLOCKS_PER_SEC * 1000);
      
      lastImgSendTime=cur_ms;
      avgInterval=cur_avgInterval;
      if(pureSendImg==false)
        image_pipe_info_resendCache_swap_and_gc(*imgPipe,bpg_pi.resPool);
      // lastImgSendTime=t;
    }

  } while (false);
  
  if( *skipInspDataTransfer==false ||*skipImageTransfer==false)//if any of them are sent
  do
  {
    sprintf(tmp, "{\"start\":false, \"framesLeft\":%s,\"frameID\":%d,\"ACK\":true}", (bpg_pi.cameraFramesLeft) ? "true" : "false", frameActionID);
    bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("SS", tmp);
    bpg_dat.pgID = bpg_pi.CI_pgID;
    
    bpg_pi.fromUpperLayer(bpg_dat);

    //SaveIMGFile("data/MVCamX.bmp",&test1_buff);
    //exit(0);
    if (bpg_pi.cameraFramesLeft)
    {
      LOGV("bpg_pi.cameraFramesLeft:%d Get Next frame...", bpg_pi.cameraFramesLeft);
      //std::this_thread::sleep_for(std::chrono::milliseconds(100));
      //cl_GMV.Trigger();
    }
    else
    {
    }
  } while (false);

  if (*inspSnap==true)
  {
    image_pipe_info_occupyFlag_set(*imgPipe,image_pipe_info_OccupyFIdx::snapSave);
    if (inspSnapQueue.push(imgPipe))
    {
      if (ret_pipe_pass_down)//we passed the info into inspSnapQueue, so mark it
        *ret_pipe_pass_down = true;
    }
    else
    {
      image_pipe_info_occupyFlag_clr(*imgPipe,image_pipe_info_OccupyFIdx::snapSave);
      *inspSnap=false;
      LOGE("inspSnapQueue is full.... skip the save");
      saveInspQFullSkipCount++;
      if (ret_pipe_pass_down)//since the image doesn't pass down ( for now recycle it at this pipeline)
        *ret_pipe_pass_down = false;
    }
  }

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  MT_UNLOCK("");
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

bool isEndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}
bool isStartsWith(const char *str, const char *prefix)
{
  int p_len = strlen(prefix);
  int s_len = strlen(str);
  if(s_len<p_len)return false;
  return strncmp(str, prefix, p_len) == 0;
}


int getFileCountInFolder(const char* path,const char* ext)
{
  DIR *d = opendir(path);
  if (d) {
    int count=0;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
      if(dir->d_name[0]=='.')continue;//ignore all "." initial names(including . .. .xxxx)
      if(isEndsWith(dir->d_name,ext))
      {

        count++;
      }

    }
    closedir(d);
    return count;
  }
  return -1;
}


int removeOldestRep(const char* path,const char* ext)
{
  //the rep has a define file and some other axiliry files(for now there is a companion Image file)

  std::string path_ostr=path;

  DIR *d = opendir(path);
  if (d==NULL) {
    return -1;
  }

  
  std::string oldestFileName="";

  {

    time_t oldest_mtime=0;//oldest on latest modified time
    struct dirent *dir;
    int count=0;
    while ((dir = readdir(d)) != NULL) {
      
      
      // if(dir->d_type!=DT_REG)continue;//if not a file, next
      if(dir->d_name[0]=='.')continue;//ignore all "." initial names(including . .. .xxxx)
      
      if(isEndsWith(dir->d_name,ext))//focus on the certain type(extension) of file
      {
        
        std::string full_path=path_ostr+(std::string)dir->d_name;
      // LOGI("path_ostr:%s",full_path.c_str());
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0)continue;
      // LOGI("mt:%d",st.st_mtime);

        if(st.st_mtime<oldest_mtime||count==0)//is current file has mtime less than the record
        {
          oldestFileName=(std::string)dir->d_name;
          oldest_mtime=st.st_mtime;
        }
        count++;
      }

    }
    if(oldest_mtime==0)//there is zero target type file
    {
      closedir(d);
      d=NULL;
      return -2;
    }

  }

  
  rewinddir(d);
  

  std::string FILE_NAME = oldestFileName.substr (
    0,  oldestFileName.length()-1-strlen(ext)); //remove extention and the dot'.'

  // LOGI("oldestFileName:%s, FILE_NAME:%s",oldestFileName.c_str(),FILE_NAME.c_str());

  {
    int removeCount=0;
    struct dirent *dir;
    int count=0;
    while ((dir = readdir(d)) != NULL) {
      //if not a file, next
      // if(dir->d_type!=DT_REG)continue;//not a file
      if(dir->d_name[0]=='.')continue;//ignore all "." initial names(including . .. .xxxx)
      if(isStartsWith(dir->d_name,FILE_NAME.c_str())==false)continue;//not starts with [FILE_NAME]

      std::string full_path=path_ostr+(std::string)dir->d_name;
      // LOGI("DLE: FILE_NAME:%s",full_path.c_str());
      remove(full_path.c_str());//remove it
      removeCount++;
    }
    closedir(d);
    return removeCount;
  }



  return 0;
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
      // LOGI(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>report_json:%p",headImgPipe->datViewInfo.report_json);
      //report save
      //TODO: when need to save the inspection result run this, but there is a data saving latancy issue need to be solved
      {

        MT_LOCK("");
        std::string rootPath = InspSampleSavePath + SEP; //InspSampleSavePath might be changed by main thread
        MT_UNLOCK("");
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
        std::string folderPath = rootPath + extPath;

        int count =getFileCountInFolder(folderPath.c_str(),SNAP_FILE_EXTENSION);

        LOGI("folderPath::%s  ,count:%d",folderPath.c_str(),count);
        // while(count>=InspSampleSaveMaxCount)
        if(count>=InspSampleSaveMaxCount)//only deal with one
        {
          int ret = removeOldestRep(folderPath.c_str(),SNAP_FILE_EXTENSION);
          
          LOGI("removeOldestRep ret:%d",ret);
          count--;
          save_snap_folder_full_delete_count++;
        }
        std::string filePath = rootPath + extPath + std::to_string(current_time_ms());
        LOGI("SAVE::%s",filePath.c_str());

        saveInspectionSample(headImgPipe->datViewInfo.report_json, cache_camera_param, cache_deffile_JSON, &headImgPipe->img, filePath.c_str());
      }

      image_pipe_info_occupyFlag_clr(*headImgPipe,image_pipe_info_OccupyFIdx::snapSave);//clear the snap flag
      //possible occupation flag => resendCache, let image_pipe_info_resendCache_swap_and_gc handle it
      image_pipe_info_gc(*headImgPipe,bpg_pi.resPool);//try to gc the pointer, if there is no occupation
    }
  }
}


void ImgPipeDatViewThread(bool *terminationflag)
{
  using Ms = std::chrono::milliseconds;
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

    while (datViewQueue.pop_blocking(headImgPipe))
    {

      bool doPassDown = false;
      bool saveToSnap = false;
          
      bool imgSendState=true;
      bool reportSendState=true;
      LOGI("vqSize:%d  datViewQueueSkipSize:%d",datViewQueue.size(),datViewQueueSkipSize);
      if(datViewQueue.size()>datViewQueueSkipSize)
      {
        imgSendState=false;
      }


      float maxFPS=10;

      if (headImgPipe->datViewInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_FAILURE)
      {
        maxFPS=NG_MAX_FPS;
        if(saveInspFailSnap==true)
          saveToSnap = true;
      }
      // LOGI("saveInspFailSnap:%d saveToSnap:%d  finspStatus:%d",saveInspFailSnap,saveToSnap,headImgPipe->datViewInfo.finspStatus);

      if (headImgPipe->datViewInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_SUCCESS)
      {
        maxFPS=OK_MAX_FPS;
      }

      if(headImgPipe->datViewInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_NA || headImgPipe->datViewInfo.finspStatus == FeatureReport_sig360_circle_line_single::STATUS_UNSET )
      {
        maxFPS=NA_MAX_FPS;
        if(saveInspNASnap)
        {
          saveToSnap = true;
        }

        if(SKIP_NA_DATA_VIEW)
        {
          // imgSendState=false;
          reportSendState=false;
        }
      }

      // LOGI("ONNGNA:%f %f %f",OK_MAX_FPS,NG_MAX_FPS,NA_MAX_FPS);




      // if(saveInspNASnap)
      // {
      //   if(headImgPipe->datViewInfo.finspStatus==FeatureReport_sig360_circle_line_single::STATUS_NA && inspSnapQueue.size()>1)//only when the queue is free
      //   {
      //     saveToSnap=true;
      //   }
      // }

      
      
      // imgSendState=true;

      
      bool skipInspDataTransfer=!reportSendState;
      bool skipImageTransfer= !imgSendState;
      bool inspSnap=saveToSnap;

      // LOGE("repSend:%d imgSend:%d inspSnap:%d",reportSendState,imgSendState,inspSnap);

      InspResultAction(headImgPipe,&skipInspDataTransfer , &skipImageTransfer , &inspSnap, &doPassDown,maxFPS);
      //possible occupationFlag snap save | resendCache
      image_pipe_info_gc(*headImgPipe,bpg_pi.resPool);//if all occupation flag cleared, it will gc the pointer    
      
    }
  }
}

void ImgPipeProcessCenter_imp(image_pipe_info *imgPipe, bool *ret_pipe_pass_down)
{

  LOGE("============DO INSP>> waterLvL: insp:%d/%d dview:%d/%d  snap:%d/%d   poolSize:%d",
       inspQueue.size(), inspQueue.capacity(),
       datViewQueue.size(), datViewQueue.capacity(),
       inspSnapQueue.size(), inspSnapQueue.capacity(),
       bpg_pi.resPool.rest_size());
  if (bpg_pi.cameraFramesLeft == 0)
  {
    // camera->TriggerMode(1);
    // MT_UNLOCK("");
    return;
  }
  clock_t t = clock();

  if(img_transpose==true)
  {
    acvImage tmp_img;
    transpose(&tmp_img,&imgPipe->img);
  // LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
    imgPipe->img.ReSize(tmp_img.GetWidth(),tmp_img.GetHeight());
    acvCloneImage(&tmp_img,&imgPipe->img,  2);
  }
  else
  {
    acvCloneImage(&imgPipe->img,&imgPipe->img,  2);
  }


  acvImage &capImg = imgPipe->img;
  FeatureManager_BacPac *bacpac = imgPipe->bacpac;
  CameraLayer::frameInfo &fi = imgPipe->fi;

  int ret = 0;

  // LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
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

  // if (0)
  // {
  //   if (imstack.imgStacked.GetHeight() != capImg.GetHeight() || imstack.imgStacked.GetWidth() != capImg.GetWidth())
  //   {
  //     imstack.ReSize(&capImg);
  //   }
  //   else if (imstack.DiffBigger(&capImg, 10, 30))
  //   {
  //     imstack.Reset();
  //   }

  //   LOGI("stackingC:%d", imstack.stackingC);
  //   imstack.Add(&capImg);
  //   // LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  // }
  // else
  // {
  //   if(imstack.stackingC<imgStackingMaxCount)
  //   {
  //     imstack.ReSize(&capImg);
  //     LOGI("Loading Image to imstack!!!!!");
  //     imstack.Add(&capImg);
  //   }
  // }

  // LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  {

    // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
    ret = ImgInspection(matchingEng, &capImg, bacpac, imgPipe->camLayer, 1);
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
            stat_sec = stat;//full insp status
          }
        }

    // LOGI(">>>>"); //overall status
    //     int agg_stat= FeatureReport_sig360_circle_line_single::STATUS_UNSET;
    //     for(int k=0;k<srep.size();k++)
    //     {
    // LOGI(">>>>");
    //       vector<FeatureReport_judgeReport> &jrep = *(srep[k].judgeReports);
    //       int stat = InspStatusReduce(jrep);
    //       if(stat==FeatureReport_sig360_circle_line_single::STATUS_NA)
    //       {
    //         continue;
    //       }
    //       agg_stat = InspStatusReducer(agg_stat, stat);
    // LOGI(">>>%d>%d",agg_stat,stat);
    //     }
    //     stat_sec=agg_stat;



      }
    }

    imgPipe->datViewInfo.uInspStatus = stat;
    imgPipe->datViewInfo.finspStatus = stat_sec;

    LOGI("stat:%d stat_sec:%d",stat,stat_sec);
    
    imgPipe->datViewInfo.report_json = matchingEng.FeatureReport2Json(report);
    // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");
  }

  LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);

  bool doPassDown = doInspActionThread;


  if (bpg_pi.perifCH!=NULL)
  {
    
    int ret = sendResultTo_perifCH(bpg_pi.perifCH,imgPipe->datViewInfo.uInspStatus, imgPipe->fi.timeStamp_us/100,bpg_pi.perifCH->pkt_count);
    if(ret>=0)
    {
      bpg_pi.perifCH->pkt_count++;
    }


  }
  cJSON_AddNumberToObject(imgPipe->datViewInfo.report_json, "uInspResult", imgPipe->datViewInfo.uInspStatus);
  //taking the short cut, perifCH(inspection machine) needs 100% of data
  // LOGI("timeStamp_us:%lu",imgPipe->fi.timeStamp_us);
  if (doPassDown)
  {
    if(datViewQueue.size()==datViewQueue.capacity())
    {
      //full, skip the most important data is send to perifCH(inspection machine)
      
      LOGI("SKIP datViewQueue!! info recycle");
      //recycle the resource here
      if (imgPipe->datViewInfo.report_json)
        cJSON_Delete(imgPipe->datViewInfo.report_json);
      imgPipe->datViewInfo.report_json = NULL;
      bpg_pi.resPool.retResrc(imgPipe);
      //do not wait here
      //TODO: make skip counter let data view queue know
    }
    else
    {
      datViewQueue.push_blocking(imgPipe);
    }
  }
  else
  {
    
    bool skipInspDataTransfer=false;
    bool skipImageTransfer=false;
    bool inspSnap=false;
    InspResultAction(imgPipe, &skipInspDataTransfer, &skipImageTransfer,&inspSnap, &doPassDown);

    if (!doPassDown) //then, we need to recycle the resource here
    {
      if (imgPipe->datViewInfo.report_json)
        cJSON_Delete(imgPipe->datViewInfo.report_json);
      imgPipe->datViewInfo.report_json = NULL;
      bpg_pi.resPool.retResrc(imgPipe);
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
        bpg_pi.resPool.retResrc(headImgPipe);
    }
  }
}


int m_BPG_Link_Interface_WebSocket::ws_callback(websock_data data, void *param)
{
  // LOGI(">>>>data.type:%d",data.type);
  // printf("%s:BPG_Link_Interface_WebSocket type:%d sock:%d\n",__func__,data.type,data.peer->getSocket());



  switch(data.type)
  {
    case websock_data::OPENING:
      if (default_peer != NULL && default_peer != data.peer)
      {
        disconnect(data.peer->getSocket());
        return 1;
      }
    break;  
    case websock_data::CLOSING:
    case websock_data::ERROR_EV: 
    {
      if (data.peer == default_peer)
      {
        default_peer = NULL;
      LOGI("CLOSING peer %s:%d\n",
            inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));
      bpg_pi.cameraFramesLeft = 0;
      bpg_pi.camera->TriggerMode(1);
      bpg_pi.delete_PeripheralChannel();
    }


    }
    return 0;

    case websock_data::HAND_SHAKING_FINISHED:
    {
      
      LOGI("OPENING peer %s:%d  sock:%d\n",
           inet_ntoa(data.peer->getAddr().sin_addr),
           ntohs(data.peer->getAddr().sin_port), data.peer->getSocket());

      if (default_peer == NULL)
      {
        default_peer = data.peer;
        
        BPG_protocol_data bpg_dat = bpg_pi.GenStrBPGData("HR", "{\"version\":\"" _VERSION_ "\"}"); //[F]older [S]truct
        bpg_dat.pgID = 0xFF;
        bpg_pi.fromUpperLayer(bpg_dat);
      }
    }
    return 0;

    case websock_data::DATA_FRAME:
    {
      data.data.data_frame.raw[data.data.data_frame.rawL] = '\0';
      // LOGI(">>>>data raw:%s", data.data.data_frame.raw);
      if (bpg_prot)
      {
        toUpperLayer(data.data.data_frame.raw, data.data.data_frame.rawL, data.data.data_frame.isFinal);
      }
      else
      {
        return -1;
      }
    }
    return 0;

  }

  return -3;
}

int initCamera(CameraLayer_BMP_carousel *CL_bmpc)
{
  return CL_bmpc == NULL ? -1 : 0;
}

CameraLayer *getCamera(int initCameraType = 0)
{

  img_transpose=false;
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
  camLayerMan.discover();
  if(camLayerMan.camBasicInfo.size()>0)
  {
    CameraLayer::BasicCameraInfo BCamInfo = camLayerMan.camBasicInfo[0];
    if(BCamInfo.vender=="CameraLayer_BMP_carousel")
    {
      camera=camLayerMan.connectCamera(BCamInfo.driver_name,BCamInfo.id,"data/BMP_carousel_test",CameraLayer_Callback_GIGEMV, NULL);
    }
    else
    {
      LOGI(">>>name:%s  sn:%s  model:%s   ",BCamInfo.name.c_str(),BCamInfo.serial_number.c_str(),BCamInfo.model.c_str());
      camera=camLayerMan.connectCamera(BCamInfo.driver_name,BCamInfo.id,"",CameraLayer_Callback_GIGEMV, NULL);
    }
  }



  if (camera == NULL)
  {
    return NULL;
  }

  LOGV("TriggerMode(1)");
  camera->TriggerMode(2);
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
      ifwebsocket=new m_BPG_Link_Interface_WebSocket(port);

      //
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
  std::thread ActionThread(ImgPipeDatViewThread, &terminationFlag);
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
    bpg_pi.camera = camera;
  }
  LOGI("Camera:%p", bpg_pi.camera);

  {
    cJSON *json_mac_setting = ReadJson("data/machine_setting.json");
    if (json_mac_setting)
    {
      setup_machine_setting(json_mac_setting);
      cJSON_Delete(json_mac_setting);
    }
  }

  // while(1)
  // {
  //   try{
  //     clientSMEM_SEND_CH=new smem_channel("AACXXX",1000,false);
  //     break;
  //   }
  //   catch(std::exception &ex)
  //   {

  //   }
  //   LOGI(">>>");
  // }

  ifwebsocket->setUpperLayer(&bpg_pi);
  bpg_pi.setLink(ifwebsocket);
  // mjpegS = new MJPEG_Streamer2(7603);
  LOGI("SetEventCallBack is set...");

  int count=0;
  while (1)
  {


    // if(clientSMEM_SEND_CH)
    // {
    //   sprintf((char*)clientSMEM_SEND_CH->getPtr(),">>>%d",count++);
    //   clientSMEM_SEND_CH->s_post();
    //   clientSMEM_SEND_CH->s_wait_remote();
    // }
    // LOGI("GO RECV");
    // mjpegS->fdEventFetch(&fdset);

    // LOGI("WAIT..");
    fd_set fd_s = ifwebsocket->get_fd_set();
    int maxfd = ifwebsocket->findMaxFd();
    if (select(maxfd + 1, &fd_s, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }

    ifwebsocket->runLoop(&fd_s, NULL);
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
    delete ifwebsocket;
    
    terminationFlag = true;
    LOGE("SIGINT exit.... \n");
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
    AttachStaticInfo(jobj, &bpg_pi);
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
      AttachStaticInfo(jobj, &bpg_pi);
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


char* PatternRest(char *str, const char *pattern)
{
  for(;;str++,pattern++)
  { 
    if(*pattern=='\0')return str;//pattern ends... return
    if(*str != *pattern)return NULL;//if str NOT equal to pattern ( including *str=='\0')
    else  continue;
  }
  return NULL;
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
    bool doMatch = false;
    char *str = PatternRest(argv[i], "CamInitStyle=");//CamInitStyle={str}
    if(str)
    {
      
      if (strcmp(str, "0") == 0)
      {
        doMatch=true;
        CamInitStyle = 0;
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
        CamInitStyle = 1;
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
        CamInitStyle = 2;
      }
    }

    str = PatternRest(argv[i], "chdir=");
    if(str!=NULL)
    {
      LOGI("parse....   chdir=%s",str);
      doMatch=true;
      chdir(str);
    }

    if (doMatch)
    {
      LOGE("CMD param[%d]:%s ...OK", i, argv[i]);
    }
    else
    {
      LOGE("unknown param[%d]:%s", i, argv[i]);
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
      AttachStaticInfo(jobj, &bpg_pi);
      //cJSON_AddNumberToObject(jobj, "session_id", session_id);
      char *jstr = cJSON_Print(jobj);
      cJSON_Delete(jobj);

      LOGI("__\n %s  \n___", jstr);

      delete jstr;
    }
    return 0;
  }

  // if (0)
  // {
  //   char *imgName = "data/BMP_carousel_test/01-02-23-18-53-491.bmp";
  //   char *defName = "data/calib_test_line.hydef";

  //   //char *imgName="data/calib_cam1_surfaceGo.bmp";
  //   //char *defName = "data/cameraCalibration.json";
  //   //
  //   return simpleTest(imgName, defName);
  // }

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
