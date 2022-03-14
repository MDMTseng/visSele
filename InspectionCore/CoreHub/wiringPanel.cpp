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
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>

#define _VERSION_ "1.2"
std::timed_mutex mainThreadLock;


const int resourcePoolSize = 30;



int sendcJsonTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json);
int printfTo_perifCH(PerifChannel *perifCH,uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...);
int sendResultTo_perifCH(PerifChannel *perifCH,int uInspStatus, uint64_t timeStamp_100us);

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






m_BPG_Link_Interface_WebSocket *ifwebsocket=NULL;

int _argc;
char **_argv;

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
  LOGI("finfo:WH:%d,%d",finfo.width,finfo.height);
  img->ReSize(finfo.width,finfo.height,3);

  return cl_obj.ExtractFrame(img->CVector[0],3,finfo.width*finfo.height);
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
        }
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

CameraLayer::status CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void *context)
{
  if (type != CameraLayer::EV_IMG)
    return CameraLayer::NAK;
  // static clock_t pframeT;
  // clock_t t = clock();


  // if(inspQueue.size()>imageQueueSkipSize)//for responsiveness
  // {//skip image if the queue is more than imageQueueSkipSize
  
  //   LOGE("skip image, inspQueue.size():%d>imageQueueSkipSize:%d\n", inspQueue.size(),imageQueueSkipSize);
  //   return CameraLayer::NAK;
  // }

  // double interval = (double)(t - pframeT) / CLOCKS_PER_SEC * 1000;
  // if (!doImgProcessThread)
  // {
  //   int skip_int = 0;
  //   LOGI("frameInterval:%fms t:%d pframeT:%d", interval, t, pframeT);
  //   if (interval < skip_int)
  //   {
  //     LOGI("interval:%f is less than skip_int:%d ms", interval, skip_int);
  //     return CameraLayer::NAK; //if the interval less than 70ms then... skip this frame
  //   }
  // }
  // pframeT = t;
  // LOGI("=============== frameInterval:%fms \n", interval);
  // LOGI("bpg_pi->cameraFramesLeft:%d", bpg_pi.cameraFramesLeft);
  // CameraLayer &cl_GMV = *((CameraLayer *)&cl_obj);

  // CameraLayer::frameInfo finfo = cl_GMV.GetFrameInfo();
  
  // // LOGE("finfo.wh:%d,%d", finfo.width,finfo.height);
  

  // image_pipe_info *headImgPipe = bpg_pi.resPool.fetchResrc_blocking();
  // if (headImgPipe == NULL)
  // {
  //   LOGE("HEAD IMG pipe is NULL");
  //   return CameraLayer::NAK;
  // }

  // headImgPipe->camLayer = &cl_obj;
  // headImgPipe->type = type;
  // headImgPipe->context = context;
  // headImgPipe->fi = finfo;
  
  // headImgPipe->img.ReSize(finfo.width,finfo.height,3);
  // cl_GMV.ExtractFrame(headImgPipe->img.CVector[0],3,finfo.width*finfo.height);

  // headImgPipe->bacpac = &calib_bacpac;

  // if (doImgProcessThread)
  // {

  //   // LOGE("bpg_pi.resPool.rest_size:: %d", bpg_pi.resPool.rest_size());

  //   if (inspQueue.push_blocking(headImgPipe) == false)
  //   {
  //     LOGE("NO resource can be used.....");
  //     // imagePipeBuffer.clear();

  //     if (bpg_pi.perifCH)
  //     {
  //       LOGI("perifCH is here too!!");
  //       uint8_t buffx[200];
        
  //       int ret= printfTo_perifCH(bpg_pi.perifCH,buffx, sizeof(buffx),true,
  //               "{"
  //               "\"type\":\"inspRep\",\"status\":%d,"
  //               "\"idx\":%d"
  //               "}",
  //               -10001, 1);
  //     }
  //   }
  // }
  // else
  // {
  //   //
  //   //   while (imagePipeBuffer.size() == 0)
  //   //   { //Wait for ImgPipeProcessThread to complete
  //   //
  //   //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
  //   //   }
  //   //
  //   bool doPassDown = false;
  //   ImgPipeProcessCenter_imp(headImgPipe, &doPassDown);
  //   if (!doPassDown)
  //     bpg_pi.resPool.retResrc(headImgPipe);
  // }
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
      if(bpg_pi.camera!=NULL)
      {
        bpg_pi.camera->TriggerMode(1);
      }
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

  // calib_bacpac.sampler = new ImageSampler();
  // neutral_bacpac.sampler = new ImageSampler();


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
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
      }
      else if (strcmp(str, "1") == 0)
      {
        doMatch=true;
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

  signal(SIGINT, sigroutine);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
