
#include <vector>
#include <compat_dirent.h>

#include <main.h>


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



BPG_protocol_data m_BPG_Protocol_Interface::GenStrBPGData(const char *TL, char *jsonStr)
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


m_BPG_Protocol_Interface::m_BPG_Protocol_Interface()
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





int m_BPG_Protocol_Interface::fromUpperLayer_DATA(const char*TL,int pgID,char* str)
{
  BPG_protocol_data bpg_dat = GenStrBPGData(TL, str);
  bpg_dat.pgID = pgID;
  return fromUpperLayer(bpg_dat);
}

int m_BPG_Protocol_Interface::fromUpperLayer_DATA(const char*TL,int pgID,cJSON* json)
{
  char *jstr = cJSON_Print(json);
  int ret = fromUpperLayer_DATA(TL,pgID,jstr);
  delete jstr;
  return ret;
}
int m_BPG_Protocol_Interface::fromUpperLayer_DATA(const char*TL,int pgID,BPG_protocol_data_acvImage_Send_info* imgInfo)
{
  BPG_protocol_data bpg_dat = GenStrBPGData(TL, NULL);
  bpg_dat.callbackInfo = (uint8_t *)imgInfo;
  bpg_dat.callback = m_BPG_Protocol_Interface::SEND_acvImage;
  bpg_dat.pgID = pgID;
  return fromUpperLayer(bpg_dat);
}
int m_BPG_Protocol_Interface::fromUpperLayer_SS(int pgID,bool isACK,const char*fromTL,const char* error_msg)
{


  char _buf[200];
  char *buf=_buf;
  buf+=sprintf(buf, "{\"start\":false,\"ACK\":%s",
           (isACK) ? "true" : "false");

  if(fromTL)
    buf+=sprintf(buf, ",\"cmd\":\"%c%c\"", fromTL[0], fromTL[1]);

          
  if(error_msg)
    buf+=sprintf(buf, ",\"errMsg\":\"%s\"", error_msg);

  
  buf+=sprintf(buf, "}");



  return fromUpperLayer_DATA("SS",pgID,_buf);
}

// int m_BPG_Protocol_Interface::fromUpperLayer_DATA(const char*jsonTL,int pgID,InspectionTarget_EXCHANGE* excahngeInfo)
// {
//   if(excahngeInfo==NULL)return -1;
//   if(excahngeInfo->info)
//   {
//     fromUpperLayer_DATA("CM",pgID,excahngeInfo->info);
//   }
  
//   if(excahngeInfo->imgInfo.img)
//   {
    
//     fromUpperLayer_DATA("IM",pgID,&excahngeInfo->imgInfo);
//   }
//   return 0;
// }


cJSON *cJSON_DirFiles(const char *path, cJSON *jObj_to_W, int depth)
{
  if (path == NULL)
    return NULL;

  DIR *d = opendir(path);

  if (d == NULL)
    return NULL;

  cJSON *retObj = (jObj_to_W == NULL) ? cJSON_CreateObject() : jObj_to_W;
  char buf[PATH_MAX + 1];
  realfullPath(path, buf);

  cJSON_AddStringToObject(retObj, "path", buf);


  if (depth > 0){

    cJSON *dirFiles = cJSON_CreateArray();
    cJSON_AddItemToObject(retObj, "files", dirFiles);
    std::string folderPath(buf);
    struct dirent *dir;
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
  }
  closedir(d);

  return retObj;
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



int sendcJSONTo_perifCH(Data_JsonRaw_Layer *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json)
{

  if (perifCH==NULL)
  {
    return -1;
  }
  int buff_head_room=perifCH->max_head_room_size();
  int buffSize=bufL-buff_head_room;
  char *padded_buf=(char*)buf+buff_head_room;

  int ret= cJSON_PrintPreallocated(json, padded_buf, buffSize-perifCH->max_leg_room_size(), false);
  LOGI(">>%s",padded_buf);
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





std::string getTimeStr(const char *timeFormat)
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


int sendcJsonTo_perifCH(Data_JsonRaw_Layer *perifCH,uint8_t* buf, int bufL, bool directStringFormat, cJSON* json)
{

  if (perifCH==NULL)
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



int printfTo_perifCH(Data_JsonRaw_Layer *perifCH,uint8_t* buf, int bufL, bool directStringFormat, const char *fmt, ...)
{

  if (perifCH==NULL)
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

int getImage(CameraLayer *camera,acvImage *dst_img,int trig_type,int timeout_ms)
{
  return (camera->SnapFrame(SNAP_Callback,(void*)dst_img,trig_type,timeout_ms) == CameraLayer::ACK)?0:-1;
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



typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst, size_t dstLen, uint8_t *src, size_t srcLen);

void ImageDownSampling(acvImage &dst, acvImage &src, int downScale, ImageSampler *sampler, int doNearest,
                       int X, int Y, int W, int H)
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




BPG_protocol_data_acvImage_Send_info ImageDownSampling_Info(acvImage &dstBuff, acvImage &src, int downScale, ImageSampler *sampler, int doNearest,
                       int X, int Y, int W, int H)
{
  BPG_protocol_data_acvImage_Send_info iminfo = {img : &dstBuff, scale : (uint16_t)downScale};
  //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70

  iminfo.offsetX = (X / downScale) * downScale;
  iminfo.offsetY = (X / downScale) * downScale;

  ImageDownSampling(dstBuff, src, downScale, sampler, doNearest,iminfo.offsetX,iminfo.offsetY,W,H);

  iminfo.fullHeight = src.GetHeight();
  iminfo.fullWidth = src.GetWidth();
  return iminfo;


}