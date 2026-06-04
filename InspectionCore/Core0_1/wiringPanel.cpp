#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <CameraLayerManager.hpp>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "CvBridge.h"
#include "LensCalib.h"
#include "FieldCalib.h"
#include "BackLightFieldCalib.h"
#include <dirent.h>
#include <algorithm>

LOG_MODULE("core");

#define _VERSION_ "1.2"
char* SNAP_FILE_EXTENSION="xreps";
char* SNAP_IMG_EXTENSION="jpg";
std::timed_mutex mainThreadLock;

std::mutex matchingEnglock;


bool SKIP_NA_DATA_VIEW=false;

int imageQueueSkipSize = 0;
int datViewQueueSkipSize = 0;
int DATA_VIEW_MAX_FPS=20;
// SEND_acvImage compression mode. 0 = legacy raw RGBA (4 B / px) for back-compat
// with current WebUI; 1-100 = JPEG with that quality value. Set via the BPG
// settings command with key "IMG_STREAMING_JPEG_QUALITY". The format chosen is
// signalled to the receiver in the first byte of SEND_acvImage's 15-byte
// metadata sub-frame (0 = raw RGBA, 1 = JPEG); the JPEG quality used is in the
// second byte for diagnostics.
int DataView_JPEG_quality = 0;
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
// Number of snapshot saves that were SKIPPED because free disk space dropped
// below the hard floor (see saveInspectionSample call site).  Exposed via the
// `save_snap_folder_full_delete_count`-style query in the BPG status response
// (the field name is documented; this counter complements it).
int save_snap_disk_low_skip_count=0;
// Minimum free megabytes required on the snapshot filesystem before the
// daemon writes another snapshot.  Below this floor, the save is skipped (and
// loudly logged) rather than racing the kernel to ENOSPC and leaving a
// half-written file (cf. the round-fix on unchecked fwrite).
const int SNAP_MIN_FREE_MB = 500;

const std::string InspSampleSavePath_DEFAULT("data/SAMPLE");
std::string InspSampleSavePath = InspSampleSavePath_DEFAULT;
int InspSampleSaveMaxCount = 1000;




const int resourcePoolSize = 30;

std::mutex lastDatViewCache_lock;
image_pipe_info *lastDatViewCache=NULL;
// Monotonic counter incremented each time lastDatViewCache flips to a new
// frame. Used by field-calib capture to count *distinct* streamed frames.
std::atomic<uint64_t> g_view_frame_seq{0};
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
  g_view_frame_seq.fetch_add(1, std::memory_order_release);
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
      snprintf(tmp, sizeof(tmp), "{\"type\":\"MESSAGE\",\"msg\":%s,\"CONN_ID\":%d}", raw, ID);
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






class ImageStackAddUp
{
  std::mutex lock;
public:
  int stackingC = 0;
  // phase 3a: cv::Mat-backed (was acvImage).  imgStacked stores 24-bit
  // accumulator values via _24BitUnion stored as 3 bytes / pixel.
  cv::Mat imgStacked;
  cv::Mat imgExtract;

  void addUp_1CH(cv::Mat &accum, const cv::Mat &src)
  {
    std::lock_guard<std::mutex> guard(lock);
    for (int i = 0; i < accum.rows; i++)
    {
      uchar *aRow = accum.ptr<uchar>(i);
      const uchar *sRow = src.ptr<uchar>(i);
      for (int j = 0; j < accum.cols; j++)
      {
        _24BitUnion *pixU = (_24BitUnion *)(aRow + j * 3);
        pixU->_3Byte.Num += sRow[j * 3];
      }
    }
  }

  void set_1CH(cv::Mat &accum, const cv::Mat &src)
  {
    std::lock_guard<std::mutex> guard(lock);
    for (int i = 0; i < accum.rows; i++)
    {
      uchar *aRow = accum.ptr<uchar>(i);
      const uchar *sRow = src.ptr<uchar>(i);
      for (int j = 0; j < accum.cols; j++)
      {
        _24BitUnion *pixU = (_24BitUnion *)(aRow + j * 3);
        pixU->_3Byte.Num = sRow[j * 3];
      }
    }
  }

  void clear()
  {
    std::lock_guard<std::mutex> guard(lock);
    if (!imgStacked.empty()) imgStacked.setTo(cv::Scalar(0,0,0));
  }

  void ReSize(const cv::Mat &ref)
  {
    std::lock_guard<std::mutex> guard(lock);
    imgStacked.create(ref.rows, ref.cols, CV_8UC3);
    Reset();
  }

  void Reset()
  {
    std::lock_guard<std::mutex> guard(lock);
    stackingC = 0;
  }

  void Add(const cv::Mat &in)
  {
    std::lock_guard<std::mutex> guard(lock);
    if (stackingC == 0)        { set_1CH(imgStacked, in);   stackingC++; return; }
    if (stackingC < 100)       { addUp_1CH(imgStacked, in); stackingC++; }
  }

  void Export(cv::Mat &out)
  {
    std::lock_guard<std::mutex> guard(lock);
    out.create(imgStacked.rows, imgStacked.cols, CV_8UC3);
    for (int i = 0; i < out.rows; i++)
    {
      uchar *oRow = out.ptr<uchar>(i);
      const uchar *sRow = imgStacked.ptr<uchar>(i);
      for (int j = 0; j < out.cols; j++)
      {
        _24BitUnion *pixU = (_24BitUnion *)(sRow + j * 3);
        int pix = pixU->_3Byte.Num / (stackingC == 0 ? 1 : stackingC);
        if (pix > 255) pix = 255;
        oRow[j * 3] = oRow[j * 3 + 1] = oRow[j * 3 + 2] = pix;
      }
    }
  }

  void Export()
  {
    std::lock_guard<std::mutex> guard(lock);
    Export(imgExtract);
  }

  bool DiffBigger(const cv::Mat &img2, float globalDiffThres, int localDiffThres, int skipSampling = 10)
  {
    std::lock_guard<std::mutex> guard(lock);
    if (skipSampling < 1) skipSampling = 1;

    globalDiffThres *= globalDiffThres * (imgStacked.rows * imgStacked.cols / skipSampling / skipSampling);
    localDiffThres *= localDiffThres;

    uint64_t diffSum = 0; int diffMax = 0; int count = 0;
    for (int i = 0; i < imgStacked.rows; i += skipSampling)
    {
      const uchar *sRow = imgStacked.ptr<uchar>(i);
      const uchar *src2Row = img2.ptr<uchar>(i);
      for (int j = 0; j < imgStacked.cols; j += skipSampling)
      {

        _24BitUnion *pixU = (_24BitUnion *)(sRow + j * 3);
        int pix = stackingC == 0 ? 0 : (pixU->_3Byte.Num / stackingC);
        count++;
        int diff = pix - src2Row[j * 3];
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
int ws_port = 4090;

MJPEG_Streamer *mjpegS;
MatchingEngine matchingEng;
CameraLayer *gen_camera;
int CamInitStyle = 0;

int downSampLevel = 1;

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

// Persistent calib data loaded from disk at startup and re-loaded after a
// successful in-app calibrate. Owned here, pointed-at by calib_bacpac. Default
// applyXxx flags stay false -- consumer code gates on its own toggle.
static LensCalibResult  g_lens_calib;
static FieldCalibResult g_field_calib;
// Calib files are not hard-coded anymore -- WebUI tells core which path to
// load / save / reload via explicit RPC payload fields. Core does NOT
// auto-load anything at startup; bacpac.lensCalib / .fieldCal stay null
// until the UI requests a load.

// Per-side capture buffer staged between field_calib_capture calls. Finalize
// folds these into a FieldCalibResult and writes the JSON.
static FieldGrid g_pending_bright;
static FieldGrid g_pending_dark;
static int g_pending_img_w = 0, g_pending_img_h = 0;

// Capture n_frames distinct streamed frames from the live camera CI stream
// (caller must have CI registered + trigger_mode:0). Builds an M×N grid of
// (mean, std) over the cells. Returns true on success.
static bool field_calib_capture_grid(int rows, int cols, int n_frames,
                                     int timeout_ms, FieldGrid &out_grid)
{
  if (rows <= 0 || cols <= 0 || n_frames <= 0) return false;
  const int ncells = rows * cols;
  std::vector<double> sum(ncells, 0.0), sumsq(ncells, 0.0);
  int captured = 0;
  uint64_t last_seq = g_view_frame_seq.load(std::memory_order_acquire);
  auto t0 = std::chrono::steady_clock::now();
  int last_W = 0, last_H = 0;
  while (captured < n_frames) {
    uint64_t cur = g_view_frame_seq.load(std::memory_order_acquire);
    if (cur == last_seq) {
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count() > timeout_ms) {
        LOGE("field_calib_capture: timeout after %d frame(s)", captured);
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }
    last_seq = cur;
    // Snapshot frame under lock.
    cv::Mat frame;
    {
      std::lock_guard<std::mutex> guard(lastDatViewCache_lock);
      if (!lastDatViewCache || lastDatViewCache->img.empty()) continue;
      lastDatViewCache->img.copyTo(frame);
    }
    cv::Mat gray;
    if (frame.channels() == 1) gray = frame;
    else cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    last_W = gray.cols; last_H = gray.rows;
    // Per-cell mean over this frame, then accumulate into running sums.
    for (int r = 0; r < rows; r++) {
      int y0 = (int)((long long)r * gray.rows / rows);
      int y1 = (int)((long long)(r + 1) * gray.rows / rows);
      for (int c = 0; c < cols; c++) {
        int x0 = (int)((long long)c * gray.cols / cols);
        int x1 = (int)((long long)(c + 1) * gray.cols / cols);
        cv::Rect roi(x0, y0, x1 - x0, y1 - y0);
        double m = cv::mean(gray(roi))[0];
        sum[r * cols + c]   += m;
        sumsq[r * cols + c] += m * m;
      }
    }
    captured++;
    t0 = std::chrono::steady_clock::now();
  }
  out_grid.rows = rows; out_grid.cols = cols; out_grid.n_frames = captured;
  out_grid.mean.assign(ncells, 0.0);
  out_grid.std.assign(ncells, 0.0);
  for (int i = 0; i < ncells; i++) {
    double m = sum[i] / captured;
    double v = sumsq[i] / captured - m * m;
    if (v < 0) v = 0;
    out_grid.mean[i] = m;
    out_grid.std[i]  = std::sqrt(v);
  }
  g_pending_img_w = last_W; g_pending_img_h = last_H;
  LOGI("field_calib_capture: %dx%d grid over %d frame(s), img=%dx%d",
       rows, cols, captured, last_W, last_H);
  return true;
}

// Push lens-calib's px/mm into the sampler's calibMap so mmpP_ideal() reflects
// the latest lens calibration (otherwise downstream measure code keeps using
// whatever calibPpB/calibmmpB the camera setup loaded -- ignoring the lens
// recalibration). Telecentric: m = px/mm; mmpP = 1/m.
static void push_mmpp_to_sampler()
{
  if (!g_lens_calib.ok) return;
  double m_px_per_mm = g_lens_calib.tele.m;
  if (m_px_per_mm <= 0) return;
  if (calib_bacpac.sampler) {
    auto *cm = calib_bacpac.sampler->getCalibMap();
    if (cm) { cm->calibPpB = m_px_per_mm; cm->calibmmpB = 1.0; }
  }
  if (neutral_bacpac.sampler) {
    auto *cm = neutral_bacpac.sampler->getCalibMap();
    if (cm) { cm->calibPpB = m_px_per_mm; cm->calibmmpB = 1.0; }
  }
  LOGI("push_mmpp_to_sampler: m=%.6f px/mm -> mmpp=%.9f mm/px",
       m_px_per_mm, 1.0 / m_px_per_mm);
}

static bool load_lens_calib(const char *path)
{
  if (!path || !*path) return false;
  FILE *f = fopen(path, "rb");
  if (!f) { LOGE("load_lens_calib: cannot open %s", path); return false; }
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  std::vector<char> buf(n + 1, 0); fread(buf.data(), 1, n, f); fclose(f);
  g_lens_calib = lens_calib_from_json(buf.data());
  calib_bacpac.lensCalib   = &g_lens_calib;
  neutral_bacpac.lensCalib = &g_lens_calib;
  push_mmpp_to_sampler();
  LOGI("load_lens_calib: %s ok=%d rms=%.4f m=%.4f", path,
       g_lens_calib.ok, g_lens_calib.overall_rms_px, g_lens_calib.tele.m);
  return true;
}

static bool load_field_calib(const char *path)
{
  if (!path || !*path) return false;
  FieldCalibResult tmp = field_calib_load_file(path);
  if (tmp.bright.rows == 0 && tmp.dark.rows == 0) {
    LOGE("load_field_calib: %s missing or empty", path);
    return false;
  }
  g_field_calib = tmp;
  calib_bacpac.fieldCal   = &g_field_calib;
  neutral_bacpac.fieldCal = &g_field_calib;
  LOGI("load_field_calib: %s bright %dx%d(n=%d) dark %dx%d(n=%d) uniformity=%.1f%%",
       path,
       g_field_calib.bright.rows, g_field_calib.bright.cols, g_field_calib.bright.n_frames,
       g_field_calib.dark.rows, g_field_calib.dark.cols, g_field_calib.dark.n_frames,
       g_field_calib.uniformity_pct);
  return true;
}
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
int ImgInspection_JSONStr(MatchingEngine &me, cv::Mat &test1_cv, int repeatTime, char *jsonStr, FeatureManager_BacPac *bacpac);

int ImgInspection_DefRead(MatchingEngine &me, cv::Mat &test1_cv, int repeatTime, char *defFilename, FeatureManager_BacPac *bacpac);

typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst, size_t dstLen, uint8_t *src, size_t srcLen);

// cv::Mat-native ImageDownSampling. Walks the dst grid, samples src at
// downScale*step with the calib-aware ImageSampler when provided (else direct
// pixel fetch). Mirrors the acvImage body bytewise on the no-sampler branch.
void ImageDownSampling(cv::Mat &dst, const cv::Mat &src, int downScale,
                       ImageSampler *sampler, int doNearest = 1,
                       int X = -1, int Y = -1, int W = -1, int H = -1)
{
  if (src.empty() || src.type() != CV_8UC3) return;
  cv::Mat src_c = src.isContinuous() ? src : src.clone();
  int X2 = src_c.cols - 1, Y2 = src_c.rows - 1;
  int xx = (X < 0) ? 0 : (X >= X2 ? X2 - 1 : X);
  int yy = (Y < 0) ? 0 : (Y >= Y2 ? Y2 - 1 : Y);
  if (W > 0) { X2 = xx + W; if (X2 >= src_c.cols)  X2 = src_c.cols - 1;  }
  if (H > 0) { Y2 = yy + H; if (Y2 >= src_c.rows)  Y2 = src_c.rows - 1;  }
  int sxStart = xx / downScale, syStart = yy / downScale;
  int sxEnd   = X2 / downScale, syEnd   = Y2 / downScale;
  int dstW = sxEnd - sxStart + 1;
  int dstH = syEnd - syStart + 1;
  dst.create(dstH, dstW, CV_8UC3);

  for (int i = syStart; i <= syEnd; i++)
  {
    int src_i = i * downScale;
    uint8_t *dRow = dst.ptr<uint8_t>(i - syStart);
    for (int j = sxStart; j <= sxEnd; j++)
    {
      int src_j = j * downScale;
      int BSum = 0, GSum = 0, RSum = 0;
      if (sampler)
      {
        float coord[2] = { (float)src_j, (float)src_i };
        float bgr[3];
        sampler->sampleImage3_IdealCoord(src_c, coord, bgr, doNearest);
        if (bgr[0] > 255) bgr[0] = 255;
        if (bgr[1] > 255) bgr[1] = 255;
        if (bgr[2] > 255) bgr[2] = 255;
        BSum = (int)bgr[0]; GSum = (int)bgr[1]; RSum = (int)bgr[2];
      }
      else
      {
        const uint8_t *sPix = src_c.ptr<uint8_t>(src_i) + src_j * 3;
        BSum = sPix[0]; GSum = sPix[1]; RSum = sPix[2];
      }
      uint8_t *dPix = dRow + (j - sxStart) * 3;
      dPix[0] = BSum; dPix[1] = GSum; dPix[2] = RSum;
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
  info.location.x = *JFetEx_NUMBER(nodeRoot, "location.x");
  info.location.y = *JFetEx_NUMBER(nodeRoot, "location.y");
  info.index.x = (int)*JFetEx_NUMBER(nodeRoot, "index.x");
  info.index.y = (int)*JFetEx_NUMBER(nodeRoot, "index.y");

  info.sigma = *JFetEx_NUMBER(nodeRoot, "sigma");
  info.samp_rate = *JFetEx_NUMBER(nodeRoot, "samp_rate");
  info.mean = *JFetEx_NUMBER(nodeRoot, "mean");
  info.error = *JFetEx_NUMBER(nodeRoot, "error");

  return info;
}



void downSampSetup(CameraLayer &camera, cJSON &settingJson)
{
  // Opt-out: with IGNORE_DYNAMIC_VIEW=1 the core ignores canvas-driven
  // down_samp_level updates entirely so the stream stays at full res
  // (paired with IGNORE_DYNAMIC_VIEW handling in ImageTransferSetup).
  static const bool ignoreDyn = (getenv("IGNORE_DYNAMIC_VIEW") != NULL);
  double *val = JFetch_NUMBER(&settingJson, "down_samp_level");
  if (val && !ignoreDyn)
  {
    downSampLevel = (int)*val;
  }
  downSampLevel=1;

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

int saveInspectionSample(cJSON *inspectionReport, cJSON *camera_param, cJSON *deffile, const cv::Mat &image, const char *fileName, const char *filename_extension=SNAP_FILE_EXTENSION, const char *img_extension=SNAP_IMG_EXTENSION)
{
  if (image.empty()) return -1;

  cJSON *reportsList = JFetch_ARRAY(inspectionReport, "reports[0].reports");
  if (reportsList == NULL)
    return -10;

  cJSON *camera_param_data = JFetch_OBJECT(camera_param, "reports[0]");
  if (camera_param_data == NULL)
    return -11;

  std::string filePath(fileName);

  cJSON *infoJObj = cJSON_CreateObject();

  // copy to dodge a still-unsolved aliasing bug that would otherwise strip
  // reportsList from inspectionReport mid-print.
  reportsList = cJSON_Duplicate(reportsList, true);
  camera_param_data = cJSON_Duplicate(camera_param_data, true);
  cJSON_AddItemToObject(infoJObj, "reports", reportsList);
  cJSON_AddItemToObject(infoJObj, "defInfo", deffile);
  cJSON_AddItemToObject(infoJObj, "camera_param", camera_param_data);
  cJSON_AddNumberToObject(infoJObj, "time_ms", current_time_ms());

  char *jstr = cJSON_Print(infoJObj);
  cJSON_DetachItemViaPointer(infoJObj, deffile);

  cJSON_Delete(infoJObj);
  int ret_write_Len = WriteBytesToFile((uint8_t *)jstr, strlen(jstr), (filePath+"." + (std::string)filename_extension).c_str());
  delete (jstr);
  if (ret_write_Len < 0)
    return -1;

  if (!cv::imwrite((filePath+"." + (std::string)img_extension).c_str(), image))
    return -2;

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



BPG_protocol_data m_BPG_Protocol_Interface::GenStrBPGData(char *TL, const char *jsonStr)
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
  cacheImage.create(1, 1, CV_8UC3);   // phase 3a: cv::Mat init
  // NOTE: no auto-load of any calib files here. WebUI explicitly calls
  // RC{target:"calib_files_load", ...} with the paths it wants.
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
// Heuristic: is this 3-channel BGR image actually grayscale content (B=G=R
// across every pixel)?  Used to auto-pick mode 2 (1-component grayscale JPEG)
// even when the caller passed a BGR Mat. Fast-rejecting via a 1-row sample
// (~2-3 us for 5 MP) keeps the auto-detect cheap when the answer is "no".
static bool _looks_grayscale(const cv::Mat &m)
{
  if (m.channels() == 1) return true;
  if (m.channels() != 3) return false;
  int H = m.rows, W = m.cols;
  if (H == 0 || W == 0) return false;
  // 1-row sample first; bail out as soon as we see a colored pixel.
  int probe_y = H / 2;
  const uchar *p = m.ptr<uchar>(probe_y);
  for (int x = 0; x < W; x++) {
    if (p[3*x] != p[3*x+1] || p[3*x+1] != p[3*x+2]) return false;
  }
  // Probe row was uniform.  Now sample a sparse grid across the image to
  // catch the case where only a small region carries colour.
  int step = std::max(1, H / 32);
  for (int y = 0; y < H; y += step) {
    const uchar *q = m.ptr<uchar>(y);
    int xstep = std::max(1, W / 64);
    for (int x = 0; x < W; x += xstep) {
      if (q[3*x] != q[3*x+1] || q[3*x+1] != q[3*x+2]) return false;
    }
  }
  return true;
}

int m_BPG_Protocol_Interface::SEND_acvImage(BPG_Protocol_Interface &dch, struct BPG_protocol_data data, void *callbackInfo)
{
  if(callbackInfo==NULL)return -1;
  BPG_protocol_data send_dat;
  BPG_protocol_data_acvImage_Send_info *img_info = (BPG_protocol_data_acvImage_Send_info*)callbackInfo;

  cv::Mat *img = img_info->img;
  if (img == NULL || img->empty()) return -1;
  int W = img->cols, H = img->rows;

  uint8_t header[]={
    0,0,

    (uint8_t)(img_info->offsetX >>8),
    (uint8_t)(img_info->offsetX),
    (uint8_t)(img_info->offsetY >>8),
    (uint8_t)(img_info->offsetY),

    (uint8_t)(W>>8),
    (uint8_t)(W),
    (uint8_t)(H>>8),
    (uint8_t)(H),
    (uint8_t)(img_info->scale),

    (uint8_t)(img_info->fullWidth >>8),
    (uint8_t)(img_info->fullWidth),
    (uint8_t)(img_info->fullHeight >>8),
    (uint8_t)(img_info->fullHeight),
  };

  // JPEG path: encode once via cv::imencode, mark format in metadata header
  // byte 0, and stream the encoded bytes. Default-off (DataView_JPEG_quality
  // = 0) keeps the legacy raw-RGBA wire format for back-compat with the
  // current WebUI.
  //
  //   header[0] = 1  -> 3-component (BGR) JPEG     (CV_8UC3 colour content)
  //   header[0] = 2  -> 1-component grayscale JPEG (CV_8UC1, or BGR detected
  //                     as B==G==R for every probe pixel -- the daemon stores
  //                     gray sources as replicated BGR, so this is the common
  //                     case and the auto-detect picks it up for free).
  if (DataView_JPEG_quality > 0)
  {
    cv::Mat encode_src;
    uint8_t fmt;
    if (_looks_grayscale(*img))
    {
      // Pull a single channel out for encoding -- cv::imencode on CV_8UC1
      // writes a 1-component grayscale JPEG (SOF0 components=1), ~16% smaller
      // and ~17% faster than the 3-component path on the same content.
      if (img->channels() == 1) encode_src = *img;
      else                       cv::extractChannel(*img, encode_src, 0);
      fmt = 2;
    }
    else
    {
      encode_src = *img;
      fmt = 1;
    }
    std::vector<uint8_t> _jpeg;
    std::vector<int> _params = { cv::IMWRITE_JPEG_QUALITY, DataView_JPEG_quality };
    cv::imencode(".jpg", encode_src, _jpeg, _params);

    header[0] = fmt;
    header[1] = (uint8_t)DataView_JPEG_quality;

    image_send_buffer.resize(dch.getHeaderSize() + sizeof(header));
    dch.headerSetup(&image_send_buffer[0], image_send_buffer.size(), data);
    memcpy(&image_send_buffer[dch.getHeaderSize()], header, sizeof(header));
    dch.toLinkLayer(&image_send_buffer[0],
                    dch.getHeaderSize() + sizeof(header),
                    false, dch.activePeer());

    const int  jpegHeaderOffset = 10;
    const size_t JPEG_CHUNK = 32 * 1024;
    for (size_t pos = 0; pos < _jpeg.size(); pos += JPEG_CHUNK)
    {
      size_t n = std::min(JPEG_CHUNK, _jpeg.size() - pos);
      bool   isLast = (pos + n >= _jpeg.size());
      image_send_buffer.resize(jpegHeaderOffset + n);
      memcpy(&image_send_buffer[jpegHeaderOffset], &_jpeg[pos], n);
      dch.toLinkLayer(&image_send_buffer[jpegHeaderOffset], n, isLast,
                      dch.activePeer(), jpegHeaderOffset, 0);
    }
    return 0;
  }

  // Legacy raw-RGBA wire format. Header[0] stays 0 (format = raw RGBA).
  {
    image_send_buffer.resize(dch.getHeaderSize()+sizeof(header));
    dch.headerSetup(&image_send_buffer[0], image_send_buffer.size(), data);

    memcpy(&image_send_buffer[dch.getHeaderSize()], header, sizeof(header));
    dch.toLinkLayer(&image_send_buffer[0], dch.getHeaderSize()+sizeof(header), false, dch.activePeer());
  }

  const int headerOffset=10;
  image_send_buffer.resize(headerOffset+10000);

  // Walk the cv::Mat row-by-row.  For 3-ch BGR we emit (R,G,B,255); for 1-ch
  // gray we replicate (g,g,g,255) so the receiver's RGBA decode is unchanged.
  int rest_len = W * H;
  const int channels = img->channels();
  size_t cur_x = 0, cur_y = 0;
  while (rest_len > 0)
  {
    int imgBufferDataSize=image_send_buffer.size()-headerOffset;
    uint8_t* imgBufferDataPtr=&image_send_buffer[headerOffset];
    int sendL = 0;
    int max_px = (imgBufferDataSize - 4) / 4;
    while (max_px > 0 && rest_len > 0)
    {
      const uchar *row = img->ptr<uchar>((int)cur_y);
      const uchar *px  = row + cur_x * channels;
      if (channels == 3) {
        imgBufferDataPtr[sendL    ] = px[2];   // R
        imgBufferDataPtr[sendL + 1] = px[1];   // G
        imgBufferDataPtr[sendL + 2] = px[0];   // B
      } else {  // 1-channel gray: replicate
        imgBufferDataPtr[sendL    ] = px[0];
        imgBufferDataPtr[sendL + 1] = px[0];
        imgBufferDataPtr[sendL + 2] = px[0];
      }
      imgBufferDataPtr[sendL + 3] = 255;
      sendL += 4;
      rest_len--;
      max_px--;
      if (++cur_x >= (size_t)W) { cur_x = 0; cur_y++; }
    }
    dch.toLinkLayer(imgBufferDataPtr, sendL, rest_len == 0, dch.activePeer(), headerOffset, 0);
  }
  return 0;
}





CameraLayer::status SNAP_Callback(CameraLayer &cl_obj, int type, void* obj)
{
  if (type != CameraLayer::EV_IMG)
    return CameraLayer::NAK;
  cv::Mat *dst = (cv::Mat *)obj;

  CameraLayer::frameInfo finfo = cl_obj.GetFrameInfo();
  LOGI("finfo:WH:%d,%d  img_transpose:%d", finfo.width, finfo.height, img_transpose);

  // Allocate a contiguous BGR buffer at the camera's native orientation; the
  // camera writes into it via ExtractFrame.
  cv::Mat raw(finfo.height, finfo.width, CV_8UC3);
  auto ret = cl_obj.ExtractFrame(raw.data, 3, finfo.width * finfo.height);

  cv::Mat oriented;
  if (img_transpose) cv::transpose(raw, oriented);
  else oriented = raw;

  // BGR -> RRR (replicate R channel across all 3 BGR slots), matching the
  // legacy callback's per-pixel rewrite.
  cv::Mat r;
  cv::extractChannel(oriented, r, 2);
  cv::cvtColor(r, *dst, cv::COLOR_GRAY2BGR);

  return ret;
}

int getImage(CameraLayer *camera, cv::Mat &dst, int trig_type=0, int timeout_ms=-1)
{
  return (camera->SnapFrame(SNAP_Callback, (void *)&dst, trig_type, timeout_ms) == CameraLayer::ACK) ? 0 : -1;
}



int m_BPG_Protocol_Interface::toUpperLayer(BPG_protocol_data bpgdat, void *peer)
{
  //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);

    BPG_protocol_data *dat = &bpgdat;

    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //      dat->pgID);
    cJSON *json = cJSON_Parse((char *)dat->dat_raw);
    // RAII cleanup: this BPG message handler is a ~1600-line do/while with
    // 20+ inner `break` paths, none of which previously called
    // cJSON_Delete(json) before bailing out -> one cJSON tree leak per
    // malformed / early-exit packet, unbounded in a 24/7 daemon. Wrap json
    // in a stack-RAII guard that frees on every exit path. The trailing
    // `cJSON_Delete(json)` at the bottom of this block was removed.
    struct _CJsonGuard {
      cJSON *p;
      ~_CJsonGuard() { if (p) cJSON_Delete(p); }
    } _json_guard{json};
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

    // Probe: log core state on every BPG msg. CORE_STATE_PROBE=1 to enable.
    // Skips chatty per-frame types (GS=settings poll, IM=image push back to
    // peer) to keep the output readable when the bug repros.
    {
      static const bool dbgProbe = (getenv("CORE_STATE_PROBE") != NULL);
      if (dbgProbe && !checkTL("GS", dat)) {
        fprintf(stderr,
          "[STATEPROBE] msg=[%c%c] DoImageTransfer=%d JPEGq=%d cameraFramesLeft=%d "
          "cacheImage=%dx%d ImageCrop=(%d,%d,%d,%d)\n",
          dat->tl[0], dat->tl[1],
          (int)DoImageTransfer, DataView_JPEG_quality,
          (int)cameraFramesLeft,
          cacheImage.cols, cacheImage.rows,
          ImageCropX, ImageCropY, ImageCropW, ImageCropH);
      }
    }

    // if (checkTL("GS", dat) == false)
    // LOGI("DataType_BPG:[%c%c] pgID:%02X", dat->tl[0], dat->tl[1],
    //       dat->pgID);
    if (checkTL("HR", dat))
    {
      LOGI("DataType_BPG>>>>%s", dat->dat_raw);

      LOGI("Hello ready.......");
      session_ACK = true;
    }
    else if (checkTL("SB", dat)) //[S]u[B]scribe to the live inspection stream
    {
      // {"stream": true|false}; default true if omitted.
      void *target;
      int type = getDataFromJson(json, "stream", &target);
      if (type == cJSON_False)
        unsubscribeStream(peer);
      else
        subscribeStream(peer);
      LOGI("SB stream subscribe=%d peer=%p subscribers=%zu",
           (type != cJSON_False), peer, stream_subscribers.size());
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
          snprintf(dirPath, sizeof(dirPath), "%s", fileName);
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

        // Defensive write for the __CACHE_IMG__ family. Three reasons:
        //   1. cv::imwrite throws cv::Exception when it can't pick an
        //      encoder from the file extension; uncaught it terminates the
        //      whole process (libc++abi). We catch + log instead.
        //   2. WebUI sometimes sends extensionless cache paths (e.g.
        //      ".../arc_test"); auto-append `.png` so the write still
        //      succeeds rather than failing soft.
        //   3. Empty/garbage images shouldn't get out — caller already
        //      checks cols*rows; we re-check defensively.
        auto safe_imwrite_cache = [](const char *path, const cv::Mat &img) -> bool {
          if (path == nullptr || path[0] == '\0') {
            LOGE("safe_imwrite_cache: empty path; skipping write");
            return false;
          }
          if (img.empty() || img.cols <= 0 || img.rows <= 0) {
            LOGE("safe_imwrite_cache: empty image (%dx%d); skipping write %s",
                 img.cols, img.rows, path);
            return false;
          }
          std::string p = path;
          // Has a recognized extension? OpenCV dispatches on .png/.jpg/.jpeg/.bmp/.tif/.tiff.
          auto dot = p.find_last_of('.');
          auto slash = p.find_last_of("/\\");
          bool hasExt = (dot != std::string::npos) &&
                        (slash == std::string::npos || dot > slash) &&
                        (dot + 1 < p.size());
          if (!hasExt) { p += ".png"; LOGW("imwrite: no ext on %s → using %s", path, p.c_str()); }
          try {
            return cv::imwrite(p, img);
          } catch (const cv::Exception &ex) {
            LOGE("imwrite failed for %s: %s", p.c_str(), ex.what());
            return false;
          }
        };

        int strinL = strlen((char *)dat->dat_raw) + 1;

        if (dat->size - strinL == 0)
        { //No raw data, check "type"

          char *type = (char *)JFetch(json, "type", cJSON_String);
          if (strcmp(type, "__CACHE_IMG__") == 0)
          {
            LOGE("__CACHE_IMG__ %d x %d", cacheImage.cols, cacheImage.rows);
            if (cacheImage.cols * cacheImage.rows > 10) //HACK: just a hacky way to make sure the cache image is there
            {
              // SaveIMGFile dispatched on file extension; cv::imwrite does the
              // same thing (png/jpg/bmp) on a cv::Mat directly. safe_imwrite_cache
              // adds .png if missing and catches cv::Exception so an extensionless
              // filename doesn't terminate the process.
              session_ACK = safe_imwrite_cache(fileName, cacheImage);
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
              
              session_ACK = safe_imwrite_cache(fileName, lastDatViewCache->img);
            }
            
          }
          else if (strcmp(type, "__DELETE_FILE__") == 0)
          {
            // Only allow deletes under ./data to avoid stray paths.
            session_ACK = false;
            if (strstr(fileName, "..") != NULL) {
              LOGE("__DELETE_FILE__: rejecting path with '..': %s", fileName);
            } else if (strncmp(fileName, "data/", 5) != 0 && strncmp(fileName, "./data/", 7) != 0) {
              LOGE("__DELETE_FILE__: refuse outside data/: %s", fileName);
            } else {
              int r = remove(fileName);
              if (r == 0) { session_ACK = true; LOGI("deleted %s", fileName); }
              else LOGE("delete failed %s: %s", fileName, strerror(errno));
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
              // cv::Mat Export overload allocates tmp_buff itself.
              imstack.Export(tmp_buff);

              if (tmp_buff.cols * tmp_buff.rows > 10)//just a random check
              {
                LOGI("SAVE IMG:%s",fileName);
                session_ACK = safe_imwrite_cache(fileName, tmp_buff);
              }
            }
            
          }
          else if (strcmp(type, "__LAST_DATA_VIEW_CACHE_INFO__") == 0)
          {
            char *report_extension = JFetch_STRING(json, "report_extension");
            char *img_extension = JFetch_STRING(json, "img_extension");


            lastDatViewCache_lock.lock();

            int err = saveInspectionSample(lastDatViewCache->datViewInfo.report_json, cache_camera_param, cache_deffile_JSON, lastDatViewCache->img, fileName,
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
          // Check fwrite return so disk-full / I/O failures don't silently
          // produce a half-written file with session_ACK=true.
          size_t _nw = fwrite(dat->dat_raw + strinL, dat->size - strinL, 1, write_ptr);
          fclose(write_ptr);
          if (_nw < 1)
          {
            snprintf(err_str, sizeof(err_str),
                     "file:%s fwrite failed (disk full / I/O error)", fileName);
            LOGE("%s", err_str);
            break;
          }
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
          fromUpperLayer(bpg_dat, peer);
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
          else if (strcmp(itemType, "save_snap_disk_low_skip_count") == 0)
          {
            cJSON_AddNumberToObject(retArr,itemType,save_snap_disk_low_skip_count);
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
        
        fromUpperLayer(bpg_dat, peer);
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
            
            fromUpperLayer(bpg_dat, peer);
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
              fromUpperLayer(bpg_dat, peer);
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

          // tmp_buff is now cv::Mat; cv::imread directly (same loader the
          // validated loadImageCv primitive uses).
          tmp_buff = cv::imread(imgSrcPath, cv::IMREAD_COLOR);
          if (!tmp_buff.isContinuous()) tmp_buff = tmp_buff.clone();
          int ret_val = tmp_buff.empty() ? -1 : 0;
          if (ret_val == 0)
          {
            cv::Mat *srcImg = NULL;
            srcImg = &tmp_buff;
            srcImg->copyTo(cacheImage);

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
            ImageDownSampling(dataSend_buff, *srcImg, default_scale, NULL);
            BPG_protocol_data_acvImage_Send_info iminfo = { &dataSend_buff, (uint16_t)default_scale };

            iminfo.fullHeight = srcImg->rows;
            iminfo.fullWidth = srcImg->cols;
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;

            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;

            bpg_dat.pgID = dat->pgID;
            fromUpperLayer(bpg_dat, peer);
          }
          else
          {
            
            session_ACK=false;
            break;
          }
        }


      } while (false);
    }
    else if (checkTL("LB", dat)) //[L]oad [B]inary -- raw file bytes (e.g. PNG thumb)
    {
      session_ACK = false;
      do
      {
        if (json == NULL) { snprintf(err_str, sizeof(err_str), "LB: JSON parse failed"); break; }
        char *filename = (char *)JFetch(json, "filename", cJSON_String);
        if (filename == NULL) { snprintf(err_str, sizeof(err_str), "LB: no 'filename'"); break; }
        int blen = 0;
        uint8_t *bytes = ReadByte(filename, &blen);
        if (bytes == NULL || blen <= 0) {
          snprintf(err_str, sizeof(err_str), "LB: ReadByte failed for %s", filename);
          if (bytes) free(bytes);
          break;
        }
        BPG_protocol_data bd = {0};
        bd.tl[0] = 'B'; bd.tl[1] = 'L';
        bd.dat_raw = bytes;
        bd.size = blen;
        bd.pgID = dat->pgID;
        fromUpperLayer(bd, peer);
        free(bytes);
        session_ACK = true;
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
        cv::Mat *srcImg = NULL;

        if (imgSrcPath != NULL)
        {
          if (strcmp(imgSrcPath, "__CACHE_IMG__") == 0)
          {
            cacheImage.copyTo(tmp_buff);
            srcImg = &tmp_buff;
          }
          else if(strcmp(imgSrcPath, "__SNAP_TMP_IMG__") == 0)
          {
            if (getImage(camera, tmp_buff) == 0) srcImg = &tmp_buff;
          }
          else
          {
            tmp_buff = cv::imread(imgSrcPath, cv::IMREAD_COLOR);
            if (!tmp_buff.empty())
            {
              if (!tmp_buff.isContinuous()) tmp_buff = tmp_buff.clone();
              srcImg = &tmp_buff;
            }
          }
        }
        else if (srcImg == NULL)
        {
          if (getImage(camera, tmp_buff) == 0)
          {
            srcImg = &tmp_buff;
            srcImg->copyTo(cacheImage);
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
          int ret = ImgInspection_JSONStr(matchingEng, *srcImg, 1, jsonStr, select_bacpac);
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
            
            fromUpperLayer(bpg_dat, peer);

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

            ImageSampler *sampler = isCalibNA ? NULL : select_bacpac->sampler;
            ImageDownSampling(dataSend_buff, *srcImg, _scale, sampler, 0);
            BPG_protocol_data_acvImage_Send_info iminfo = { &dataSend_buff, (uint16_t)_scale };
            iminfo.fullHeight = srcImg->rows;
            iminfo.fullWidth = srcImg->cols;

            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat, peer);
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

          // Parse-then-swap (see cache_camera_param above).
          if (cJSON *_new_def = cJSON_Parse(jsonStr))
          {
            if (cache_deffile_JSON) cJSON_Delete(cache_deffile_JSON);
            cache_deffile_JSON = _new_def;
          }
          else
          {
            LOGE("cache_deffile_JSON: malformed JSON, keeping previous cached value");
          }

          // LOGI("==>>");matchingEnglock.lock();LOGI("==>>");
          matchingEng.ResetFeature();
          matchingEng.AddMatchingFeature(jsonStr);

          // LOGI("==<<");matchingEnglock.unlock();LOGI("==<<");
          void *target;
          if (getDataFromJson(json, "get_deffile", &target) == cJSON_True)
          {
            bpg_dat = GenStrBPGData("DF", jsonStr);
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat, peer);
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
      calib_bacpac.sampler->ignoreCalib(true);

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
        cv::Mat *srcImg = NULL;
        if (imgSrcPath != NULL)
        {
          tmp_buff = cv::imread(imgSrcPath, cv::IMREAD_COLOR);
          if (!tmp_buff.empty())
          {
            if (!tmp_buff.isContinuous()) tmp_buff = tmp_buff.clone();
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

          if (getImage(camera, tmp_buff, trigger_type, imageFetchTimeout) == 0)
          {
            srcImg = &tmp_buff;
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
            ImgInspection_DefRead(matchingEng, *srcImg, 1, "data/featureDetect.json", &calib_bacpac);
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
              
              fromUpperLayer(bpg_dat, peer);

              delete jstr;
            }
            else
            {
              sprintf(tmp, "{}");
              bpg_dat = GenStrBPGData("SG", tmp);
              bpg_dat.pgID = dat->pgID;
              
              fromUpperLayer(bpg_dat, peer);
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

            calib_bacpac.sampler->ignoreCalib(true);
            ImageDownSampling(dataSend_buff, cacheImage, tar_down_samp_level, calib_bacpac.sampler, true);
            BPG_protocol_data_acvImage_Send_info iminfo = { &dataSend_buff, (uint16_t)tar_down_samp_level };
            iminfo.fullHeight = cacheImage.rows;
            iminfo.fullWidth  = cacheImage.cols;
            bpg_dat.callbackInfo = (uint8_t *)&iminfo;
            bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
            bpg_dat.pgID = dat->pgID;
            
            fromUpperLayer(bpg_dat, peer);
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
      else if (strcmp(target, "lens_calibrate") == 0)
      {
        // Collect chess_*.png from `dir`, run lens calibration, write JSON.
        char *dir = JFetch_STRING(json, "dir");
        char *out = JFetch_STRING(json, "out");
        char *modelStr = JFetch_STRING(json, "lens_model");
        double *sq = JFetch_NUMBER(json, "square_mm");
        if (!dir || !out || !modelStr || !sq) {
          LOGE("lens_calibrate: missing dir/out/lens_model/square_mm");
        } else {
          std::vector<std::string> imgs;
          DIR *d = opendir(dir);
          if (d) {
            struct dirent *de;
            while ((de = readdir(d))) {
              std::string n = de->d_name;
              if (n.rfind("chess_", 0) == 0 &&
                  (n.size() >= 4 && (n.rfind(".png") == n.size()-4 ||
                                     n.rfind(".PNG") == n.size()-4))) {
                imgs.push_back(std::string(dir) + "/" + n);
              }
            }
            closedir(d);
            std::sort(imgs.begin(), imgs.end());
          }
          LOGI("lens_calibrate: %zu image(s) in %s, square_mm=%.4f, model=%s",
               imgs.size(), dir, *sq, modelStr);
          if (imgs.empty()) {
            LOGE("lens_calibrate: no chess_*.png in %s", dir);
          } else {
            LensModel model = lens_model_from_string(modelStr);
            LensCalibResult r = lens_calib_run_from_images(imgs, *sq, model, out);
            LOGI("lens_calibrate: ok=%d RMS=%.4f px, m=%.4f px/mm, u0=%.2f v0=%.2f -> %s",
                 r.ok ? 1 : 0, r.overall_rms_px, r.tele.m, r.tele.u0, r.tele.v0, out);
            session_ACK = r.ok;
            if (r.ok) load_lens_calib(out);
            // Emit result JSON as RP so UI can display stats without re-fetching.
            char *jstr = lens_calib_to_json(r);
            if (jstr) {
              // Augment with image count.
              char wrap[2048];
              snprintf(wrap, sizeof(wrap),
                "{\"report_type\":\"lens_calib\",\"image_count\":%zu,"
                "\"out_path\":\"%s\",\"calib\":%s}",
                imgs.size(), out, jstr);
              BPG_protocol_data rp = GenStrBPGData("RP", wrap);
              rp.pgID = dat->pgID;
              fromUpperLayer(rp, peer);
              free(jstr);
            }
          }
        }
      }
      else if (strcmp(target, "field_calib_capture") == 0)
      {
        // Capture N>0 distinct streamed frames into an M×N bright OR dark grid.
        // Caller must have CI registered and trigger_mode:0 so frames flow.
        // Payload: { which:"bright"|"dark", rows, cols, n_frames, timeout_ms? }
        char *which = JFetch_STRING(json, "which");
        double *rs = JFetch_NUMBER(json, "rows");
        double *cs = JFetch_NUMBER(json, "cols");
        double *nf = JFetch_NUMBER(json, "n_frames");
        double *to = JFetch_NUMBER(json, "timeout_ms");
        if (!which || !rs || !cs || !nf || *rs <= 0 || *cs <= 0 || *nf <= 0) {
          LOGE("field_calib_capture: missing/invalid params");
        } else {
          FieldGrid g;
          int tmo = to ? (int)*to : 8000;
          bool ok = field_calib_capture_grid((int)*rs, (int)*cs, (int)*nf, tmo, g);
          if (ok) {
            bool is_bright = strcmp(which, "bright") == 0;
            (is_bright ? g_pending_bright : g_pending_dark) = g;
            session_ACK = true;
            // Per-cell stats wrapper for the UI to display this side immediately.
            double lo = g.mean.empty() ? 0 : g.mean[0], hi = lo, s = 0;
            for (double x : g.mean) { s += x; if (x < lo) lo = x; if (x > hi) hi = x; }
            double m = g.mean.empty() ? 0 : s / g.mean.size();
            double medStd = 0;
            if (!g.std.empty()) {
              std::vector<double> v = g.std;
              std::nth_element(v.begin(), v.begin() + v.size()/2, v.end());
              medStd = v[v.size()/2];
            }
            char buf[8192];
            char *gj = field_calib_to_json({ true, g_pending_img_w, g_pending_img_h,
                                             g_pending_bright, g_pending_dark });
            snprintf(buf, sizeof(buf),
              "{\"report_type\":\"field_calib_capture\",\"which\":\"%s\","
              "\"rows\":%d,\"cols\":%d,\"n_frames\":%d,"
              "\"image_size\":[%d,%d],"
              "\"cell_mean\":%.4f,\"cell_min\":%.4f,\"cell_max\":%.4f,"
              "\"cell_std_median\":%.4f,\"current\":%s}",
              which, g.rows, g.cols, g.n_frames,
              g_pending_img_w, g_pending_img_h, m, lo, hi, medStd,
              gj ? gj : "null");
            if (gj) free(gj);
            BPG_protocol_data rp = GenStrBPGData("RP", buf);
            rp.pgID = dat->pgID;
            fromUpperLayer(rp, peer);
          }
        }
      }
      else if (strcmp(target, "field_calib_clear_pending") == 0)
      {
        // UI calls this after removing the last per-side capture from history
        // so that finalize falls back to the disk-loaded grid for that side
        // instead of silently reusing a stale pending capture.
        char *which = JFetch_STRING(json, "which");
        if (!which) {
          g_pending_bright = FieldGrid(); g_pending_dark = FieldGrid();
        } else if (strcmp(which, "bright") == 0) {
          g_pending_bright = FieldGrid();
        } else if (strcmp(which, "dark") == 0) {
          g_pending_dark = FieldGrid();
        }
        session_ACK = true;
        LOGI("field_calib_clear_pending: which=%s", which ? which : "(all)");
      }
      else if (strcmp(target, "field_calib_finalize") == 0)
      {
        // Combine pending bright+dark into a FieldCalibResult, save JSON, reload
        // bacpac. UI must specify `out`.
        char *out = JFetch_STRING(json, "out");
        if (!out || !*out) {
          LOGE("field_calib_finalize: missing 'out' path");
          break;
        }
        const char *path = out;
        // Fall back to whatever the disk-loaded calib has for any side not
        // captured this session (g_field_calib was populated by
        // reload_calib_into_bacpac at startup / after the last save).
        FieldGrid eff_bright = g_pending_bright.rows > 0 ? g_pending_bright : g_field_calib.bright;
        FieldGrid eff_dark   = g_pending_dark.rows   > 0 ? g_pending_dark   : g_field_calib.dark;
        if (eff_bright.rows == 0 && eff_dark.rows == 0) {
          LOGE("field_calib_finalize: no pending or saved grids -- capture first");
        } else {
          FieldCalibResult fr;
          fr.ok = true;
          fr.img_w = (g_pending_img_w > 0) ? g_pending_img_w : g_field_calib.img_w;
          fr.img_h = (g_pending_img_h > 0) ? g_pending_img_h : g_field_calib.img_h;
          fr.bright = eff_bright;
          fr.dark   = eff_dark;
          // (1) Vignette mask: cells far darker than the median are lens-limited
          //     (the lens physically can't deliver light there). Mark them
          //     invalid so the consumer skips equalization rather than wildly
          //     amplifying them. Threshold = vignette_frac * median(bright).
          // (2) Robust-clean (fit quadratic + reject scratches/dust) runs only
          //     on non-vignette cells -- they're zeroed first so robustClean's
          //     "non-positive == inactive" rule excludes them from the fit.
          // bright.valid[] is persisted in the JSON for the consumer to read.
          double *vfp = JFetch_NUMBER(json, "vignette_frac");
          double vignette_frac = vfp ? *vfp : 0.5;
          if (fr.bright.rows > 0 && fr.bright.cols > 0) {
            const int N = (int)fr.bright.mean.size();
            std::vector<double> sorted = fr.bright.mean;
            std::sort(sorted.begin(), sorted.end());
            double med = sorted[N / 2];
            double thr = vignette_frac * med;
            fr.bright.valid.assign(N, 1);
            int vig = 0;
            std::vector<float> g(N);
            for (int i = 0; i < N; i++) {
              if (fr.bright.mean[i] < thr) { fr.bright.valid[i] = 0; vig++; g[i] = 0; }
              else g[i] = (float)fr.bright.mean[i];
            }
            int rej = backLightField_robustClean(g.data(), fr.bright.cols, fr.bright.rows);
            for (int i = 0; i < N; i++) {
              // Keep vignette cells flagged invalid; restore their mean to 0.
              fr.bright.mean[i] = fr.bright.valid[i] ? g[i] : 0.0;
            }
            fr.bright_rejected_cells = rej;
            fr.bright_vignette_cells = vig;
            LOGI("field_calib_finalize: bright median=%.1f vignette_thr=%.1f "
                 "(frac=%.2f) -> %d vignette + %d scratch-cleaned",
                 med, thr, vignette_frac, vig, rej);
          }
          // Dark grid keeps a trivial all-valid mask (no vignette concept).
          if (fr.dark.mean.size() > 0)
            fr.dark.valid.assign(fr.dark.mean.size(), 1);
          if (field_calib_save_file(fr, path)) {
            load_field_calib(path);
            session_ACK = true;
            char *rep = field_calib_to_json(fr);
            if (rep) {
              // Wrap so UI can branch on report_type like lens calib does.
              char wrap[16384];
              snprintf(wrap, sizeof(wrap),
                "{\"report_type\":\"field_calib\",\"out_path\":\"%s\",\"calib\":%s}",
                path, rep);
              BPG_protocol_data rp = GenStrBPGData("RP", wrap);
              rp.pgID = dat->pgID;
              fromUpperLayer(rp, peer);
              free(rep);
            }
            LOGI("field_calib_finalize: -> %s (uniformity=%.1f%%, hot=%d, dyn=%.1f)",
                 path, g_field_calib.uniformity_pct, g_field_calib.hot_cells,
                 g_field_calib.dynamic_range);
          } else {
            LOGE("field_calib_finalize: write failed %s", path);
          }
        }
      }
      else if (strcmp(target, "camera_setting_refresh") == 0 ||
               strcmp(target, "calib_files_load") == 0)
      {
        // UI-driven: every path comes in the payload. Missing keys skip that
        // file (so the UI can refresh just one piece without disturbing the
        // others). Payload:
        //   { camera_setting_dir, lens_calib_path, field_calib_path }
        char *cam_dir = JFetch_STRING(json, "camera_setting_dir");
        char *lens_p  = JFetch_STRING(json, "lens_calib_path");
        char *field_p = JFetch_STRING(json, "field_calib_path");
        if (cam_dir && *cam_dir && camera) {
          CameraSettingFromFile(camera, cam_dir);
          this->camera = camera;
          calib_bacpac.cam = camera;
        }
        bool ok = true;
        if (lens_p  && *lens_p)  ok = load_lens_calib(lens_p)   && ok;
        if (field_p && *field_p) ok = load_field_calib(field_p) && ok;
        session_ACK = ok;
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
      
      fromUpperLayer(bpg_dat, peer);
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
        fromUpperLayer(bpg_dat, peer);
        delete jstr;
      }
      cJSON *ImTranseSetup = JFetch_OBJECT(json, "ImageTransferSetup");
      // IGNORE_DYNAMIC_VIEW=1 skips ONLY the crop-application sub-step
      // below; `enable`, OK/NG/NA_MAX_FPS etc. on the same setup must
      // still apply or the streamer goes silent.
      static const bool ignoreDynView = (getenv("IGNORE_DYNAMIC_VIEW") != NULL);
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

        if (nX && nY && nW && nH && !ignoreDynView)
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

      // JPEG compression for per-event image transfers.  0 keeps the legacy
      // raw-RGBA wire format; 1-100 switches to JPEG with that quality.  The
      // format chosen is signalled in the first byte of the metadata sub-frame
      // (0 = raw, 1 = JPEG); the WebUI must check this byte and decode either.
      double *jpegQ = JFetch_NUMBER(json, "IMG_STREAMING_JPEG_QUALITY");
      if (jpegQ)
      {
        int q = (int)*jpegQ;
        if (q < 0) q = 0;
        if (q > 100) q = 100;
        DataView_JPEG_quality = q;
        LOGI("IMG_STREAMING_JPEG_QUALITY=%d (0=raw RGBA)", DataView_JPEG_quality);
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
            
            fromUpperLayer(bpg_dat, peer);

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

    fromUpperLayer(bpg_dat, peer);
    // (json cleanup handled by _json_guard RAII added at the top of this block)
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

  // Calib params (mmpp, distortion coeffs, bright/dark grids) come from
  // data/lens_calib.json + data/field_calib.json via load_lens_calib /
  // load_field_calib, triggered by the WebUI's calib_files_load RPC --
  // not from the legacy default_camera_param.json + stageLightReport.json
  // pair that used to live here.
  return 0;
}

int ImgInspection_DefRead(MatchingEngine &me, cv::Mat &test1_cv, int repeatTime, char *defFilename, FeatureManager_BacPac *bacpac)
{
  char *string = ReadText(defFilename);
  int ret = ImgInspection_JSONStr(me, test1_cv, repeatTime, string, bacpac);
  free(string);
  return ret;
}

int ImgInspection(MatchingEngine &me, cv::Mat &test1_cv, FeatureManager_BacPac *bacpac, CameraLayer *cam, int repeatTime = 1)
{
  LOGI("============w:%d h:%d====================cam:%p", test1_cv.cols, test1_cv.rows, cam);
  if (test1_cv.empty()) return -1;
  clock_t t = clock();
  bacpac->cam = cam;
  for (int i = 0; i < repeatTime; i++)
  {
    me.setBacPac(bacpac);
    me.FeatureMatching(test1_cv);
  }
  clock_t new_t = clock();
  LOGI("%fms \n", (double)(new_t - t) / CLOCKS_PER_SEC * 1000);
  return 0;
}

int ImgInspection_JSONStr(MatchingEngine &me, cv::Mat &test1_cv, int repeatTime, char *jsonStr, FeatureManager_BacPac *bacpac)
{
  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);
  ImgInspection(me, test1_cv, bacpac, bacpac->cam, repeatTime);
  return 0;
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
  cv::Mat *tmp_img=&(headImgPipe->img);
  tmp_img->create(finfo.height, finfo.width, CV_8UC3);
  auto ret=cl_obj.ExtractFrame(tmp_img->data, 3, finfo.width*finfo.height);

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

  cv::Mat &capImg = imgPipe->img;
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

    bpg_pi.pushToSubscribers(bpg_dat);

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

      bpg_pi.pushToSubscribers(bpg_dat);

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
    static cv::Mat test1_buff;  // phase 3a: was acvImage

    BPG_protocol_data_acvImage_Send_info iminfo;
    bool sendJpg = false;

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
        iminfo = (BPG_protocol_data_acvImage_Send_info){ NULL, (uint16_t)_downSampLevel };

        iminfo.offsetX = (ImageCropX / _downSampLevel) * _downSampLevel;
        iminfo.offsetY = (ImageCropY / _downSampLevel) * _downSampLevel;

        iminfo.fullHeight = capImg.rows;
        iminfo.fullWidth = capImg.cols;
        int cropW = ImageCropW;
        int cropH = ImageCropH;

        ImageSampler *sampler = (true) ? bacpac->sampler : NULL;
        ImageDownSampling(test1_buff, capImg, _downSampLevel, sampler, 1,
                          iminfo.offsetX, iminfo.offsetY, cropW, cropH);
      }
      iminfo.img = &test1_buff;

      bpg_dat = m_BPG_Protocol_Interface::GenStrBPGData("IM", NULL);
      //BPG_protocol_data_acvImage_Send_info iminfo={img:&test1_buff,scale:4};
      iminfo.scale=_downSampLevel;
      // LOGI(">>>>");
      bpg_dat.callbackInfo = (uint8_t *)&iminfo;
      bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
      bpg_dat.pgID = bpg_pi.CI_pgID;

      bpg_pi.pushToSubscribers(bpg_dat);
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

    bpg_pi.pushToSubscribers(bpg_dat);

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
      saveInspQFullSkipCount++;
      // Sustained-drop visibility: silently dropping frames is fine for
      // realtime, but if the queue is saturated for hundreds of frames the
      // operator needs to know.  Loud-log once per 100 cumulative skips so
      // log volume stays bounded.
      if (saveInspQFullSkipCount % 100 == 1)
        LOGE("inspSnapQueue full -> dropping save (cumulative drops: %d)", saveInspQFullSkipCount);
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

        // Hard disk-space floor: avoid the silent-truncation cascade where
        // the snapshot fopen succeeds, the kernel hits ENOSPC mid-write, and
        // the daemon has no telemetry that anything went wrong.  Check the
        // filesystem of the target folder; skip + loudly log if low.
        bool _disk_ok = true;
        {
          struct statvfs _sv;
          if (statvfs(folderPath.c_str(), &_sv) == 0)
          {
            unsigned long long _free_mb =
              ((unsigned long long)_sv.f_bavail * (unsigned long long)_sv.f_frsize) / (1024ULL * 1024ULL);
            if ((long long)_free_mb < SNAP_MIN_FREE_MB)
            {
              _disk_ok = false;
              save_snap_disk_low_skip_count++;
              LOGE("SAVE skipped: free %llu MB < floor %d MB on %s "
                   "(skipped %d so far; raise floor or free space)",
                   _free_mb, SNAP_MIN_FREE_MB, folderPath.c_str(),
                   save_snap_disk_low_skip_count);
            }
          }
        }
        if (_disk_ok)
          saveInspectionSample(headImgPipe->datViewInfo.report_json, cache_camera_param, cache_deffile_JSON, headImgPipe->img, filePath.c_str());
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

  // acvCloneImage(.., 2) extracted the R channel and replicated to BGR ->
  // cv::extractChannel + cvtColor GRAY2BGR.
  if(img_transpose==true)
  {
    cv::Mat tmp_img;
    cv::transpose(imgPipe->img, tmp_img);
    cv::Mat r;
    cv::extractChannel(tmp_img, r, 2);
    cv::cvtColor(r, imgPipe->img, cv::COLOR_GRAY2BGR);
  }
  else
  {
    cv::Mat r;
    cv::extractChannel(imgPipe->img, r, 2);
    cv::cvtColor(r, imgPipe->img, cv::COLOR_GRAY2BGR);
  }


  cv::Mat &capImg = imgPipe->img;
  FeatureManager_BacPac *bacpac = imgPipe->bacpac;
  CameraLayer::frameInfo &fi = imgPipe->fi;

  int ret = 0;

  // LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  //stackingC=0;
  if(0){
    
    acv_XY offset = {
      fi.offset_x,
      fi.offset_y
    };
    LOGI("offset:%f,%f", offset.x,offset.y);
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
    ret = ImgInspection(matchingEng, capImg, bacpac, imgPipe->camLayer, 1);
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
      // Multi-client: accept every peer (no single-client rejection).
      break;

    case websock_data::CLOSING:
    case websock_data::ERROR_EV:
    {
      LOGI("CLOSING peer %s:%d\n",
           inet_ntoa(data.peer->getAddr().sin_addr), ntohs(data.peer->getAddr().sin_port));

      bpg_pi.dropPeerState(data.peer); // free this peer's inbound reassembly buffer
      bpg_pi.unsubscribeStream(data.peer);
      peers.erase(data.peer);

      // If the default broadcast target left, promote another peer (or none).
      if (data.peer == default_peer)
        default_peer = peers.empty() ? NULL : *peers.begin();

      // Only tear down shared core state when the LAST client disconnects.
      if (peers.empty())
      {
        bpg_pi.cameraFramesLeft = 0;
        if (bpg_pi.camera)
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

      peers.insert(data.peer);
      if (default_peer == NULL)
      {
        default_peer = data.peer;
        bpg_pi.subscribeStream(data.peer); // primary client streams by default
      }

      // Greet every new client with HR so each can initialize independently.
      BPG_protocol_data bpg_dat = bpg_pi.GenStrBPGData("HR", "{\"version\":\"" _VERSION_ "\"}");
      bpg_dat.pgID = 0xFF;
      bpg_pi.fromUpperLayer(bpg_dat, data.peer);
    }
    return 0;

    case websock_data::DATA_FRAME:
    {
      data.data.data_frame.raw[data.data.data_frame.rawL] = '\0';
      // LOGI(">>>>data raw:%s", data.data.data_frame.raw);
      if (bpg_prot)
      {
        toUpperLayer(data.data.data_frame.raw, data.data.data_frame.rawL, data.data.data_frame.isFinal, data.peer);
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
      int port = ws_port;
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
      if (errno == EINTR)
        continue; // interrupted by a signal; just retry
      perror("select");
      continue; // transient error: keep serving instead of killing the process
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

  cv::Mat newImg = cv::imread(imgName, cv::IMREAD_COLOR);
  int ret = newImg.empty() ? -1 : 0;
  if (ret)
  {
    LOGE("LoadBMP failed: ret:%d", ret);
    return -1;
  }
  if (!newImg.isContinuous()) newImg = newImg.clone();
  ImgInspection_DefRead(matchingEng, newImg, 1, defName, &calib_bacpac);

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
    // Calib comes from data/lens_calib.json (loaded via WebUI's
    // calib_files_load RPC), not from the legacy default_camera_param.json.
    LOGI("mmpB:%f  calibPpB:%f", calib_bacpac.sampler->getCalibMap()->calibmmpB, calib_bacpac.sampler->getCalibMap()->calibPpB);
    LOGI("mmpp:%.9f", calib_bacpac.sampler->mmpP_ideal());
    acv_XY loca = {1000, 10};
    LOGI("0__ %f  %f ___", loca.x, loca.y);
    calib_bacpac.sampler->img2ideal(&loca);
    LOGI("1__ %f  %f ___", loca.x, loca.y);
    calib_bacpac.sampler->ideal2img(&loca);
    LOGI("2__ %f  %f ___", loca.x, loca.y);
    char *string = ReadText("data/FM_gen.json");
    matchingEng.ResetFeature();
    matchingEng.AddMatchingFeature(string);

    cv::Mat bw_img = cv::imread("data/gen_TEST/B.BMP", cv::IMREAD_COLOR);
    if (!bw_img.isContinuous()) bw_img = bw_img.clone();
    int ret = bw_img.empty() ? -1 : 0;
    ret = ImgInspection(matchingEng, bw_img, &calib_bacpac, calib_bacpac.cam, 1);
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
  // Old LoadCameraCalibrationFile() used to RESET() the calib map as its first
  // step. With that load gone, the sampler's calibMap/angOffsetTable/
  // stageLightInfo would stay in their default-constructed state -- which left
  // RNormalFactor / fullFrameW / fullFrameH at zero and made ImageDownSampling
  // produce all-black frames in Insp mode (CalibUI's preview path uses
  // IMG_ignore_calib so it dodged the issue). RESET initialises to identity.
  calib_bacpac.sampler->RESET();
  neutral_bacpac.sampler->RESET();

  // Headless golden-sample inspection loopback (for caliper-vs-contour testing
  // without the WebUI):  visSele --insp <image.png> <def.hydef> <out.json>
  // Runs the def on the image and writes the report JSON, then exits.
  for (int ai = 1; ai + 3 < argc + 1 && ai < argc; ai++)
  {
    if (strcmp(argv[ai], "--insp") != 0) continue;
    if (ai + 3 >= argc) { LOGE("--insp needs <image> <def> <out.json>"); return 2; }
    char *imgPath = argv[ai + 1], *defPath = argv[ai + 2], *outPath = argv[ai + 3];
    // Reject non-regular def files (FIFO/socket/dir/char-device). ReadText() on
    // a FIFO blocks indefinitely waiting for a writer/EOF -> hang.
    {
      struct stat st;
      if (stat(defPath, &st) == 0 && !S_ISREG(st.st_mode))
      {
        LOGE("--insp: def file %s is not a regular file", defPath);
        return 4;
      }
    }
    cv::Mat cvSrc;
    if (loadImageCv(imgPath, cvSrc) != 0)
    { LOGE("--insp: cannot load image %s", imgPath); return 3; }
    // Reject degenerate-size images that the sig360 / labeling pipeline assumes
    // are at least sample/down-sampling-friendly. A 1x1 image SIGSEGVs deep in
    // the matching pipeline; bounce it as a controlled load failure.
    {
      const int MIN_INSP_IMG_DIM = 32;
      if (cvSrc.cols < MIN_INSP_IMG_DIM || cvSrc.rows < MIN_INSP_IMG_DIM)
      {
        LOGE("--insp: image too small (%dx%d, min %dx%d)",
             cvSrc.cols, cvSrc.rows, MIN_INSP_IMG_DIM, MIN_INSP_IMG_DIM);
        return 3;
      }
    }
    // mirror the live single-inspection handler: it uses neutral_bacpac with
    // calibPpB/calibmmpB taken from the def (wiringPanel CI handler ~1999/2100).
    // First fully init the sampler's calib map (RESET + load) like live startup
    // does (~4814) -- otherwise img2ideal divides by an uninit RNormalFactor and
    // returns NaN, poisoning every edge refine (lines/circles/search points).
    // Legacy LoadCameraCalibrationFile removed -- sampler->calibMap is now
    // primed by load_lens_calib (triggered by the WebUI's calib_files_load
    // RPC). The def's cam_param.ppb2b / mmpb2b override below still applies
    // when present for backward compat with old hydef files.
    {
      char *ds = ReadText(defPath);
      if (ds) { cJSON *dj = cJSON_Parse(ds);
        if (dj) {
          neutral_bacpac.sampler->getCalibMap()->calibPpB  = JFetch_NUMBER_ex(dj, "featureSet[0].cam_param.ppb2b");
          neutral_bacpac.sampler->getCalibMap()->calibmmpB = JFetch_NUMBER_ex(dj, "featureSet[0].cam_param.mmpb2b");
          cJSON_Delete(dj);
        }
        free(ds);
      }
    }
    // acv -> cv: full cv::Mat path through the engine entry (the acvImage `img`
    // shim above is now only kept for the `bacpac` calibration side-effects
    // upstream; the engine receives cvSrc directly).
    // Profiling tap: INSP_LOOP_N>1 reruns the inspection in-process so a
    // sampler can attach.  Default 1 = legacy single-shot behavior.
    int loopN = 1;
    if (const char *e = std::getenv("INSP_LOOP_N")) {
      int n = std::atoi(e);
      if (n > 0) loopN = n;
    }
    for (int li = 0; li < loopN; ++li) {
      ImgInspection_DefRead(matchingEng, cvSrc, 1, defPath, &neutral_bacpac);
    }
    const FeatureReport *report = matchingEng.GetReport();
    if (report == NULL) { LOGE("--insp: null report"); return 4; }
    cJSON *jobj = matchingEng.FeatureReport2Json(report);
    AttachStaticInfo(jobj, &bpg_pi);
    char *jstr = cJSON_Print(jobj);
    FILE *fp = fopen(outPath, "wb");
    if (fp) {
      size_t _len = strlen(jstr);
      size_t _nw = fwrite(jstr, 1, _len, fp);
      fclose(fp);
      if (_nw < _len) { LOGE("--insp: short write %zu/%zu to %s (disk full?)", _nw, _len, outPath); }
      else            { LOGE("--insp: wrote %s", outPath); }
    }
    else LOGE("--insp: cannot write %s", outPath);
    cJSON_Delete(jobj);
    free(jstr);
    return 0;
  }

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

    str = PatternRest(argv[i], "port=");
    if(str!=NULL)
    {
      ws_port = atoi(str);
      LOGI("parse....   port=%d", ws_port);
      doMatch=true;
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

    cv::Mat calibImage = cv::imread("data/calibImg.BMP", cv::IMREAD_COLOR);
    if (calibImage.empty()) return -1;
    if (!calibImage.isContinuous()) calibImage = calibImage.clone();
    ImgInspection_DefRead(matchingEng, calibImage, 1, "data/cameraCalibration.json", &calib_bacpac);

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

  if (0) //GenBG map -- experimental background-feature extraction probe.
  {       // dilate (window-max) -> 4 passes of 20x20 box filter to smooth out
          // the illumination field, then |Ori - filtered - 5| * 3 saturated to
          // highlight darker-than-bg features for inspection of a static
          // backlight target.  Kept as a one-shot debug tool.
    cv::Mat BGImage = cv::imread("data/BG.BMP", cv::IMREAD_COLOR);
    if (BGImage.empty()) return -1;
    if (!BGImage.isContinuous()) BGImage = BGImage.clone();

    cv::Mat BGImage_Ori = BGImage.clone();
    cv::Mat BuffImage;

    // acvWindowMax with size=5 -> 5x5 max filter == cv::dilate with rect kernel.
    cv::dilate(BGImage, BGImage,
               cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

    // 4 passes of 20x20 box filter -- big smoothing to approximate the
    // illumination field.  Ping-pong BGImage <-> BuffImage so the final
    // smoothed result ends up in BGImage.
    for (int p = 0; p < 4; p++)
    {
      cv::boxFilter(BGImage, BuffImage, -1, cv::Size(20, 20));
      std::swap(BGImage, BuffImage);
    }

    if (BGImage_Ori.size() == BGImage.size())
    {
      // |BG_Ori - BG - 5| * 3 saturated to uchar.  Done per-pixel because the
      // original logic mixes a signed subtraction with the -5 bias then takes
      // absolute value -- not exactly cv::absdiff semantics.
      cv::Mat result(BGImage.rows, BGImage.cols, CV_8UC3);
      for (int i = 0; i < BGImage.rows; i++)
      {
        const uchar *oriRow = BGImage_Ori.ptr<uchar>(i);
        const uchar *bgRow  = BGImage.ptr<uchar>(i);
        uchar *resRow = result.ptr<uchar>(i);
        for (int j = 0; j < BGImage.cols; j++)
        {
          int diff = (int)oriRow[3*j] - (int)bgRow[3*j] - 5;
          if (diff < 0) diff = -diff;
          diff *= 3;
          if (diff > 255) diff = 255;
          resRow[3*j] = resRow[3*j + 1] = resRow[3*j + 2] = (uchar)diff;
        }
      }
      BGImage_Ori = result;
    }
    cv::imwrite("data/BGImage_OriX.bmp", BGImage_Ori);
    cv::imwrite("data/proBG.bmp", BGImage);

    return 0;
  }
  signal(SIGINT, sigroutine);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
