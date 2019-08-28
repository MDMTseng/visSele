#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"
#include "DatCH_Image.hpp"
#include "DatCH_WebSocket.hpp"
#include "DatCH_BPG.hpp"
#include "DatCH_CallBack_WSBPG.hpp"

#include <compat_dirent.h> 
#include <limits.h> 
#include <sys/stat.h>

#include <main.h>
#include <playground.h>
#include <stdexcept>

#include <lodepng.h>
std::timed_mutex mainThreadLock;
DatCH_WebSocket *websocket=NULL;
MatchingEngine matchingEng;
CameraLayer *gen_camera;
DatCH_CallBack_WSBPG callbk_obj;
int CamInitStyle=0;

acvCalibMap* parseCM_info(PerifProt::Pak pakCM);

DatCH_BPG1_0 *BPG_protocol= new DatCH_BPG1_0(NULL);


DatCH_CallBack_BPG *cb = new DatCH_CallBack_BPG(BPG_protocol);

//lens1
//main.cpp  1067 main:v K: 1.00096 -0.00100092 -9.05316e-05 RNormalFactor:1296
//main.cpp  1068 main:v Center: 1295,971


//main.cpp  1075 main:v K: 0.999783 0.00054474 -0.000394607 RNormalFactor:1296
//main.cpp  1076 main:v Center: 1295,971


//lens2
//main.cpp  1061 main:v K: 0.989226 0.0101698 0.000896734 RNormalFactor:1296
//main.cpp  1062 main:v Center: 1295,971



acvRadialDistortionParam param_default={
    calibrationCenter:{1295,971},
    RNormalFactor:1296,
    K0:0.999783,
    K1:0.00054474,
    K2:-0.000394607,
    //r = r_image/RNormalFactor
    //C1 = K1/K0
    //C2 = K2/K0
    //r"=r'/K0
    //Forward: r' = r*(K0+K1*r^2+K2*r^4)
    //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
    //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
    //r/r'=r*K0/r"

    ppb2b: 63.11896896362305,
    mmpb2b:  0.630049821,
    map: NULL
};

char* ReadFile(char *filename);

CameraLayer *getCamera(int initCameraType);//0 for real First, then fake one, 1 for real camera only, 2 for fake only
int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,int repeatTime,char *jsonStr);

int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,int repeatTime,char *defFilename);

typedef size_t (*IMG_COMPRESS_FUNC)(uint8_t *dst,size_t dstLen,uint8_t *src,size_t srcLen);

int Save2PNG(uint8_t *data, int width, int height,int channelCount,const char* filePath)
{
    // we're going to encode with a state rather than a convenient function, because enforcing a color type requires setting options
    lodepng::State state;
    // input color type
    state.info_raw.colortype = LCT_GREY;
    switch(channelCount)
    {
      case 1:state.info_raw.colortype = LCT_GREY;break;
      case 2:state.info_raw.colortype = LCT_GREY_ALPHA;break;//Weird but what ever
      case 3:state.info_raw.colortype = LCT_RGB;break;
      case 4:state.info_raw.colortype = LCT_RGBA;break;
      default:return -1;
    }
    state.info_raw.bitdepth = 8;
    // output color type
    state.info_png.color.colortype = LCT_RGBA;
    state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 1; // without this, it would ignore the output color type specified above and choose an optimal one instead

    std::vector<unsigned char> buffer;
    unsigned error = lodepng::encode(buffer, data, width, height,state);
    if(error)
    {
      LOGI("encoder error %d : %s",error,lodepng_error_text(error));
      return error;
    }


    return lodepng::save_file(buffer,filePath);
}


int LoadPNGFile(acvImage *img,const  char *filename)
{
  
  std::vector<unsigned char> image;
  unsigned ret_width, ret_height;
  image.resize(0);
  
  unsigned error = lodepng::decode(image, ret_width, ret_height, filename,LCT_RGB,8);
  if(error!=0)return error;
  img->ReSize(ret_width,ret_height);

  for(int i=0;i<img->GetHeight();i++)
  {
    
    for(int j=0;j<img->GetWidth();j++)
    {
      img->CVector[i][j*3+0]=image[i*ret_width*3+j*3+2];
      img->CVector[i][j*3+1]=image[i*ret_width*3+j*3+1];
      img->CVector[i][j*3+2]=image[i*ret_width*3+j*3+0];//The order is reversed
    }
    //memcpy(img->CVector[i],&(image[i*ret_width*3]),ret_width*3);
  }
  
  return 0;
}

int LoadIMGFile(acvImage *ret_img,const  char *filename)
{
  const char *dot = strrchr(filename, '.');
  std::string fname_str(filename);

  int retVal;
  if(!dot)
  {
    retVal= LoadPNGFile(ret_img,(fname_str+".png").c_str());
    if(retVal==0)return 0;
    retVal= acvLoadBitmapFile(ret_img,(fname_str+".bmp").c_str());
    if(retVal==0)return 0;
    return -1;
  }

  if(strcmp(dot, ".bmp")==0)
  {
    return acvLoadBitmapFile(ret_img,filename);
  }
  if(strcmp(dot, ".png")==0)
  {
    return LoadPNGFile(ret_img,filename);
  }
  return -1;


}

int SavePNGFile(const char *filename, acvImage *img)
{
  LOGE("SavePNGFile:%s",filename);
  int w=img->GetWidth();
  std::vector<uint8_t> pix_arr(img->GetHeight()*img->GetWidth()*3);
  for(int i=0;i<img->GetHeight();i++)
  {
    
    for(int j=0;j<w;j++)
    {
      pix_arr[i*w*3+j*3+2]=img->CVector[i][j*3+0];
      pix_arr[i*w*3+j*3+1]=img->CVector[i][j*3+1];
      pix_arr[i*w*3+j*3+0]=img->CVector[i][j*3+2];
    }
    //memcpy(img->CVector[i],&(image[i*ret_width*3]),ret_width*3);
  }

  return Save2PNG(&(pix_arr[0]),  img->GetWidth(),  img->GetHeight(),3, filename);
}


int SaveIMGFile(const char *filename, acvImage *img)
{
  const char *dot = strrchr(filename, '.');

  LOGE("SaveIMGFile:%s",filename);
  int retVal;
  if(!dot)
  {
    std::string fname_str(filename);
    return SavePNGFile((fname_str+".png").c_str(),img);
  }

  if(strcmp(dot, ".bmp")==0)
  {
    return acvSaveBitmapFile(filename,img);
  }
  if(strcmp(dot, ".png")==0)
  {
    return SavePNGFile(filename,img);
  }
  return -1;

}

void ImageDownSampling(acvImage &dst,acvImage &src,int downScale,acvCalibMap *map)
{
  dst.ReSize(src.GetWidth()/downScale,src.GetHeight()/downScale);

  LOGI("map=%p",map);
  for(int i=0;i<dst.GetHeight();i++)
  {
    int src_i = i*downScale;
    for(int j=0;j<dst.GetWidth();j++)
    {
      int RSum=0,GSum=0,BSum=0;
      int src_j = j*downScale;

      if(map)
      {
        float coord[]={(float)src_j,(float)src_i};
        int ret = map->c2i(coord);
        
        if(ret==0)
        {
            int x=round(coord[0]);
            int y=round(coord[1]);
            if(x>=0 && x<src.GetWidth() && y>=0 && y<src.GetHeight())
            {
                BSum+=src.CVector[y][x*3];
                GSum+=src.CVector[y][x*3+1];
                RSum+=src.CVector[y][x*3+2];
            }
        }
      }
      else
      {
        for(int m=0;m<downScale;m++)
        {
            for(int n=0;n<downScale;n++)
            {
            BSum+=src.CVector[src_i+m][(src_j+n)*3];
            GSum+=src.CVector[src_i+m][(src_j+n)*3+1];
            RSum+=src.CVector[src_i+m][(src_j+n)*3+2];
            }
        }
        
        BSum/=(downScale*downScale);
        GSum/=(downScale*downScale);
        RSum/=(downScale*downScale);
      }
      dst.CVector[i][j*3+0]=BSum;
      dst.CVector[i][j*3+1]=GSum;
      dst.CVector[i][j*3+2]=RSum;
    }
  }
}



cJSON *cJSON_DirFiles(const char* path,cJSON *jObj_to_W,int depth=0)
{
  if(path==NULL)return NULL;
              
  DIR *d = opendir(path);

  if (d==NULL) return NULL;


  cJSON *retObj =(jObj_to_W==NULL)?cJSON_CreateObject():jObj_to_W;
  struct dirent *dir;
  cJSON *dirFiles = cJSON_CreateArray();
  char buf[PATH_MAX + 1]; 
#ifdef _WIN32
  _fullpath(buf,path,PATH_MAX);
#else
  realpath (path, buf);
#endif

  cJSON_AddStringToObject(retObj, "path", buf);
  cJSON_AddItemToObject(retObj, "files", dirFiles);

  
  std::string folderPath(buf);

  while ((dir = readdir(d)) != NULL) {
    //if(dir->d_name[0]=='.')continue;
    cJSON *fileInfo =  cJSON_CreateObject();
    cJSON_AddItemToArray(dirFiles, fileInfo);
    cJSON_AddStringToObject(fileInfo, "name", dir->d_name);

    char *type=NULL;
    std::string fileName(dir->d_name);
    std::string filePath=folderPath+"/"+fileName;

    switch(dir->d_type)
    {
      case DT_REG:
        type="REG";break;
      case DT_DIR:
      {
        if(depth>0 && dir->d_name!=NULL && dir->d_name[0]!='\0' && dir->d_name[0]!='.')
        {
          cJSON *subFolderStruct = cJSON_DirFiles(filePath.c_str(),NULL,depth-1);
          if(subFolderStruct!=NULL)
          {
            cJSON_AddItemToObject(fileInfo,"struct", subFolderStruct);
          }

        }
        type="DIR";break;
      }
      // case DT_FIFO:
      // case DT_SOCK:
      // case DT_CHR:
      // case DT_BLK:
      // case DT_LNK:
      case DT_UNKNOWN:
      default:
        type="UNKNOWN";break;
    }
    cJSON_AddStringToObject(fileInfo, "type",type);
    
  
    struct stat st;
    if(stat(filePath.c_str(), &st) == 0) {
      cJSON_AddNumberToObject(fileInfo, "size_bytes",st.st_size);
      cJSON_AddNumberToObject(fileInfo, "mtime_ms",st.st_mtime*1000);
      cJSON_AddNumberToObject(fileInfo, "ctime_ms",st.st_ctime*1000);
      cJSON_AddNumberToObject(fileInfo, "atime_ms",st.st_atime*1000);
    }


  }
  closedir(d);
    
  return retObj;
}

machine_hash machine_h={0};
void AttachStaticInfo(cJSON *reportJson)
{
  if(reportJson==NULL)return;
  char tmpStr[128];

  {
    char *tmpStr_ptr=tmpStr;
    for(int i=0;i<sizeof(machine_h.machine);i++)
    {
      tmpStr_ptr+=sprintf(tmpStr_ptr,"%02X",machine_h.machine[i]);
    }
    cJSON_AddStringToObject(reportJson, "machine_hash", tmpStr);
  }

}


int jObject2acvRadialDistortionParam(cJSON *root,acvRadialDistortionParam *ret_param)
{
  
  if(ret_param==NULL)return -1;
  acvRadialDistortionParam param_default={
      calibrationCenter:{1295,971},
      RNormalFactor:1296,
      K0:0.999783,
      K1:0.00054474,
      K2:-0.000394607,
      //r = r_image/RNormalFactor
      //C1 = K1/K0
      //C2 = K2/K0
      //r"=r'/K0
      //Forward: r' = r*(K0+K1*r^2+K2*r^4)
      //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
      //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
      //r/r'=r*K0/r"

      ppb2b: 63.11896896362305,
      mmpb2b:  0.630049821,
      map:NULL
  };

  *ret_param = param_default;
  if(root ==NULL)return -1;
  acvRadialDistortionParam tmp_param;
  tmp_param.K0  = *JFetEx_NUMBER(root,"reports[0].K0");
  tmp_param.K1  = *JFetEx_NUMBER(root,"reports[0].K1");
  tmp_param.K2  = *JFetEx_NUMBER(root,"reports[0].K2");

  tmp_param.ppb2b  = *JFetEx_NUMBER(root,"reports[0].ppb2b");
  tmp_param.mmpb2b  = *JFetEx_NUMBER(root,"reports[0].mmpb2b");


  tmp_param.RNormalFactor  = *JFetEx_NUMBER(root,"reports[0].RNormalFactor");
  tmp_param.calibrationCenter.X  = *JFetEx_NUMBER(root,"reports[0].calibrationCenter.x");
  tmp_param.calibrationCenter.Y  = *JFetEx_NUMBER(root,"reports[0].calibrationCenter.y");



  {
    char default_CalibMapPath[]="data/CalibMap.bin";
    char* calibMapPath= JFetch_STRING(root,"reports[0].CalibMapPath");
    if(calibMapPath==NULL)
        calibMapPath = default_CalibMapPath;
    
     
    LOGE("calibMapPath:%s",calibMapPath);
    int datL=0;
    uint8_t* bDat =  ReadByte(calibMapPath,&datL);
    if(bDat)
    {
        int count = PerifProt::countValidArr(bDat,datL);
        LOGI("PerifProt::countValidArr  count:%d",count);
        if(count<0)
        {
            throw new std::runtime_error("ReadByte return NULL");
        }
        PerifProt::Pak p1 = PerifProt::parse(bDat);
        acvCalibMap* cm_=parseCM_info(p1);
        tmp_param.map=cm_;
        delete bDat;
    }
    else
    {
        throw new std::runtime_error("ReadByte return NULL");
        return -1;
    }

  }
  *ret_param = tmp_param;
  return 0;

}



int CameraSetup(CameraLayer &camera, cJSON &settingJson)
{
  double *val = JFetch_NUMBER(&settingJson,"exposure");
  int retV=-1;
  if(val)
  {
    camera.SetExposureTime(*val);
    LOGI("SetExposureTime:%f",*val);
    retV=0;
  }

  val = JFetch_NUMBER(&settingJson,"gain");
  if(val)
  {
    camera.SetAnalogGain((int)*val);
    LOGI("SetAnalogGain:%f",*val);
    retV=0;
  }

  
  val = JFetch_NUMBER(&settingJson,"framerate_mode");
  if(val)
  {
    *val=(int)*val;
    camera.SetFrameRateMode((int)*val);
    LOGI("framerate_mode:%f",*val);
    retV=0;
  }
  return 0;
}

int LoadCameraSetup(CameraLayer &camera, char * filename)
{
  char *fileStr = ReadText(filename);
  
  if(fileStr == NULL)
  {
    LOGE("Cannot read defFile from:%s",filename);
    return -1;
  }


  cJSON *json = cJSON_Parse(fileStr);
  
  free(fileStr);
  if(json == NULL)
  {
    LOGE("File:%s is not a JSON...",filename);
    return -1;
  }
  
  int ret = CameraSetup(camera, *json);
  cJSON_Delete(json);
  return ret;
}


int LoadCameraCalibrationFile(char * filename)
{
  acvRadialDistortionParam cam_param={0};
  char *fileStr = ReadText(filename);
  
  if(fileStr == NULL)
  {
    LOGE("Cannot read defFile from:%s",filename);
    return -1;
  }


  cJSON *json = cJSON_Parse(fileStr);
  
  free(fileStr);
  bool executionError=false;

  try{
    int ret = jObject2acvRadialDistortionParam(json,&cam_param);
    if(ret)
      executionError=true;
  }
  catch(const std::exception & ex)
  {
    LOGE("Exception:%s",ex.what());
    executionError=true;
  }

  LOGI("Read deffile:%s executionError:%d  K:%g %g %g",filename,executionError,cam_param.K0,cam_param.K1,cam_param.K2);
  if(param_default.map)
  {
      delete param_default.map;
      param_default.map=NULL;
  }
  param_default=cam_param;

  cJSON_Delete(json);

  if(executionError)return -1;
  return 0;
}

bool DoImageTransfer=true;



bool DatCH_CallBack_BPG::checkTL(const char *TL,const BPG_data *dat)
{
    if(TL==NULL)return false;
    return (TL[0] == dat->tl[0] && TL[1] == dat->tl[1]);
}
uint16_t DatCH_CallBack_BPG::TLCode(const char *TL)
{
    return (((uint16_t)TL[0]<<8) |  TL[1]);
}
DatCH_CallBack_BPG::DatCH_CallBack_BPG(DatCH_BPG1_0 *self)
{
    this->self = self;
    cacheImage.ReSize(1,1);
}


void DatCH_CallBack_BPG::delete_Ext_Util_API()
{
    if(exApi)
    {
        delete exApi;
        exApi=NULL;
    }
}
void DatCH_CallBack_BPG::delete_MicroInsp_FType()
{
    
    if(mift)
    {
        delete mift;
        mift=NULL;
    }
}
BPG_data DatCH_CallBack_BPG::GenStrBPGData(char *TL, char* jsonStr)
{
    BPG_data BPG_dat={0};
    BPG_dat.tl[0]=TL[0];
    BPG_dat.tl[1]=TL[1];
    if(jsonStr ==NULL)
    {
        BPG_dat.size=0;
    }
    else
    {
        BPG_dat.size=strlen(jsonStr);
    }
    BPG_dat.dat_raw =(uint8_t*) jsonStr;

    return BPG_dat;
}
int DatCH_CallBack_BPG::callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
{
    //LOGI("DatCH_CallBack_BPG:%s_______type:%d________", __func__,data.type);
    switch(data.type)
    {
    case DatCH_Data::DataType_error:
    {
        LOGE("error code:%d..........",data.data.error.code);
    }
    break;

    //Connection layer of the BPG protocol
    case DatCH_Data::DataType_websock_data://App -(prot)>[here] WS //Final stage of outcoming data
    {
        DatCH_Data ret = websocket->SendData(data);
    }
    break;

    case DatCH_Data::DataType_BPG:// WS -(prot)>[here] App //Final stage of incoming data
    {
        BPG_data *dat = data.data.p_BPG_data;

        LOGI("DataType_BPG:[%c%c] pgID:%02X",dat->tl[0],dat->tl[1],
        dat->pgID);
        cJSON *json = cJSON_Parse((char*)dat->dat_raw);
        char err_str[100]="\0";
        bool session_ACK=false;
        char tmp[200];//For string construct json reply
        BPG_data bpg_dat;//Empty
        // {  
        //     sprintf(tmp,"{\"session_id\":%d, \"start\":true, \"PACKS\":[\"DF\",\"IM\"]}",session_id);
        //     bpg_dat=GenStrBPGData("SS", tmp);
        //     datCH_BPG.data.p_BPG_data=&bpg_dat;
        //     self->SendData(datCH_BPG);
        // }
        bpg_dat.pgID=dat->pgID;

        mainThreadLock.lock();
        if(checkTL("HR",dat))
        {
        LOGI("DataType_BPG>>>>%s",dat->dat_raw);

        LOGI("Hello ready.......");
        session_ACK=true;
        }
        else if(checkTL("SV",dat))//Data from UI to save file
        {
        LOGI("DataType_BPG>>STR>>%s",dat->dat_raw);

        if (json == NULL)
        {
            snprintf(err_str,sizeof(err_str),"JSON parse failed");
            LOGE("%s",err_str);
            break;
        }
        do{

            char* fileName =(char* )JFetch(json,"filename",cJSON_String);
            if (fileName == NULL)
            {
            snprintf(err_str,sizeof(err_str),"No entry:'filename' in it");
            LOGE("%s",err_str);
            break;
            }
            int strinL = strlen((char*)dat->dat_raw)+1;

            if(dat->size-strinL == 0 )
            {//No raw data, check "type" 

            char* type =(char* )JFetch(json,"type",cJSON_String);
            if (strcmp(type,"__CACHE_IMG__") == 0 )
            {
                LOGE("__CACHE_IMG__ %d x %d",cacheImage.GetWidth(),cacheImage.GetHeight());
                if(cacheImage.GetWidth()*cacheImage.GetHeight()>10)
                {
                SaveIMGFile(fileName,&cacheImage);

                }
                
                //cacheImage.ReSize(1,1);
            }
            }
            else
            {

            LOGI("DataType_BPG>>BIN>>%s",byteArrString(dat->dat_raw+strinL,dat->size-strinL));

            FILE *write_ptr;

            write_ptr = fopen(fileName,"wb");  // w for write, b for binary
            if(write_ptr==NULL)
            {
                snprintf(err_str,sizeof(err_str),"File open failed");
                LOGE("%s",err_str);
                break;
            }
            fwrite(dat->dat_raw+strinL,dat->size-strinL,1,write_ptr); // write 10 bytes from our buffer

            fclose (write_ptr);
            }


            session_ACK=true;
        }while(false);
        
        
        }
        else if(checkTL("FB",dat))//[F]ile [B]rowsing
        {

        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        do{
            
            if(json ==  NULL)
            {
            snprintf(err_str,sizeof(err_str),"JSON parse failed");
            LOGE("%s",err_str);
            break;
            }
            
            char* pathStr =(char* )JFetch(json,"path",cJSON_String);
            if(pathStr==NULL)
            {
            //ERROR
            snprintf(err_str,sizeof(err_str),"No 'path' entry in the JSON");
            LOGE("%s",err_str);
            break;
            }

            int depth=0;
            double* p_depth =JFetch_NUMBER(json,"depth");
            if(p_depth!=NULL)
            {
            depth=(int)*p_depth;
            }

            {
            cJSON * cjFileStruct = cJSON_DirFiles(pathStr,NULL,depth);

            char * fileStructStr  = NULL;
            
            if(cjFileStruct==NULL)
            { 
                cjFileStruct=cJSON_CreateObject();
                snprintf(err_str,sizeof(err_str),"File Structure is NULL");
                LOGI("W:%s",err_str);
                
                session_ACK=false;
            }
            else
            {
                
                session_ACK=true;
            }

            fileStructStr = cJSON_Print(cjFileStruct);
            

            bpg_dat=GenStrBPGData("FS", fileStructStr);//[F]older [S]truct
            bpg_dat.pgID=dat->pgID;
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);
            if(fileStructStr)free(fileStructStr);
            cJSON_Delete(cjFileStruct);

            }



        }while(false);

        }
        else if(checkTL("LD",dat))
        {
        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        do{
            

            
            char* filename =(char* )JFetch(json,"filename",cJSON_String);
            if(filename!=NULL)
            {
            try {
                char *fileStr = ReadText(filename);
                if(fileStr == NULL)
                {
                snprintf(err_str,sizeof(err_str),"Cannot read file from:%s",filename);
                LOGE("%s",err_str);
                break;
                }
                LOGV("Read deffile:%s",filename);
                bpg_dat=GenStrBPGData("FL", fileStr);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);
                free(fileStr);

            }
            catch (std::invalid_argument iaex) {
                snprintf(err_str,sizeof(err_str),"Caught an error! LINE:%04d",__LINE__);
                LOGE("%s",err_str);
            }

            break;
            }


            if (json == NULL)
            {
            snprintf(err_str,sizeof(err_str),"JSON parse failed LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            
            break;
            }
            char* imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
            if (imgSrcPath == NULL)
            {
            snprintf(err_str,sizeof(err_str),"No entry:imgSrcPath in it LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            break;
            }
            char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
            if (deffile == NULL)
            {
            snprintf(err_str,sizeof(err_str),"No entry:'deffile' in it LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            break;
            }

            acvImage *srcImg=NULL;
            if(imgSrcPath!=NULL)
            {
            
            int ret_val = LoadIMGFile(&tmp_buff,imgSrcPath);
            if(ret_val==0)
            {
                srcImg = &tmp_buff;
                cacheImage.ReSize(srcImg);
                acvCloneImage(srcImg,&cacheImage,-1);
            }
            }
            if(srcImg==NULL)
            {
            cacheImage.ReSize(1,1);
            break;
            }

            try {
                char *jsonStr = ReadText(deffile);
                if(jsonStr == NULL)
                {
                snprintf(err_str,sizeof(err_str),"Cannot read defFile from:%s LINE:%04d",deffile,__LINE__);
                LOGE("%s",err_str);
                break;
                }
                LOGV("Read deffile:%s",deffile);
                bpg_dat=GenStrBPGData("DF", jsonStr);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);
                free(jsonStr);
            }
            catch (std::invalid_argument iaex) {
                snprintf(err_str,sizeof(err_str),"Caught an error! LINE:%04d",__LINE__);
                LOGE("%s",err_str);
            }

            //TODO:HACK: 4X4 times scale down for transmission speed, bpg_dat.scale is not used for now
            bpg_dat=GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};

            


            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale,param_default.map);
            bpg_dat.callbackInfo = (uint8_t*)&iminfo;
            bpg_dat.callback=DatCH_BPG_acvImage_Send;
            
            bpg_dat.pgID=dat->pgID;
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);
            
            session_ACK=true;

        }while(false);


        }
        else if(checkTL("II",dat))//[I]mage [I]nspection
        {
        if (json == NULL)
        {
            snprintf(err_str,sizeof(err_str),"JSON parse failed LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            break;
        }
        
        do{
            
            char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
            if (deffile == NULL)
            {
            snprintf(err_str,sizeof(err_str),"No entry:'deffile' in it LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            break;
            }
            char* imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
            LOGI("Load Image from %s",imgSrcPath);
            acvImage *srcImg=NULL;
            if(imgSrcPath!=NULL)
            {
            
            int ret_val = LoadIMGFile(&tmp_buff,imgSrcPath);
            if(ret_val==0)
            {
                srcImg = &tmp_buff;
            }
            }

            if(srcImg==NULL)
            {
            mainThreadLock.unlock();
            LOGV("Do camera Fetch..");
            camera->TriggerMode(1);
            LOGV("LOCK...");
            mainThreadLock.lock();
            camera->Trigger();
            LOGV("LOCK BLOCK...");
            mainThreadLock.lock();
            
            LOGV( "unlock");
            mainThreadLock.unlock();
            srcImg = camera->GetImg();
            }

            if(srcImg==NULL)
            {
            snprintf(err_str,sizeof(err_str),"No Image from %s, exit... LINE:%04d",imgSrcPath,__LINE__);
            LOGE("%s",err_str);
            break;
            }

        


            DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);


            try {
                char *jsonStr = ReadText(deffile);
                if(jsonStr == NULL)
                {
                snprintf(err_str,sizeof(err_str),"Cannot read defFile from:%s LINE:%04d",deffile,__LINE__);
                LOGE("%s",err_str);
                break;
                }
                LOGV("Read deffile:%s",deffile);
                bpg_dat=GenStrBPGData("DF", jsonStr);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);

                int ret = ImgInspection_JSONStr(matchingEng,srcImg,1,jsonStr);
                free(jsonStr);
                //SaveIMGFile("data/buff.bmp",&test1_buff);

                const FeatureReport * report = matchingEng.GetReport();

                if(report!=NULL)
                {
                cJSON* jobj = matchingEng.FeatureReport2Json(report);
                AttachStaticInfo(jobj);
                char * jstr  = cJSON_Print(jobj);
                cJSON_Delete(jobj);

                //LOGI("__\n %s  \n___",jstr);
                bpg_dat=GenStrBPGData("RP", jstr);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);

                delete jstr;
                session_ACK=true;
                }
                else
                {
                session_ACK=false;
                }
            }
            catch (std::invalid_argument iaex) {
                snprintf(err_str,sizeof(err_str),"Caught an error! LINE:%04d",__LINE__);
                LOGE("%s",err_str);
                break;
            }

            bpg_dat=GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};
            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale,param_default.map);
            bpg_dat.callbackInfo = (uint8_t*)&iminfo;
            bpg_dat.callback=DatCH_BPG_acvImage_Send;
            bpg_dat.pgID=dat->pgID;
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            self->SendData(datCH_BPG);


            session_ACK=true;

        }while(false);


        }
        else if(checkTL("CI",dat))//[C]ontinuous [I]nspection
        {
        do{

            
            if (json == NULL)
            {
            snprintf(err_str,sizeof(err_str),"JSON parse failed LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            break;
            }
            
             cb->CI_pgID = dat->pgID;

            char* deffile =(char* )JFetch(json,"deffile",cJSON_String);
            if (deffile == NULL)
            {
            snprintf(err_str,sizeof(err_str),"No entry:'deffile' in it LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            cb->cameraFeedTrigger=false;
            
            camera->TriggerMode(1);
            break;
            }

            try {
            
                DatCH_Data datCH_BPG=
                BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

                char *jsonStr = ReadText(deffile);
                if(jsonStr == NULL)
                {
                snprintf(err_str,sizeof(err_str),"Cannot read defFile from:%s LINE:%04d",jsonStr,__LINE__);
                LOGE("%s",err_str);
                cb->cameraFeedTrigger=false;
                
                break;
                }

                LOGV("Read deffile:%s",deffile);
                bpg_dat=GenStrBPGData("DF", jsonStr);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);

                                
                matchingEng.ResetFeature();
                matchingEng.AddMatchingFeature(jsonStr);


                free(jsonStr);
                
                //TODO: HACK: this sleep is to wait for the gap in between def config file arriving and inspection result arriving.
                //If the inspection result arrives without def config file then webUI will generate(by design) an statemachine error event.
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                camera->TriggerMode(0);
                cb->cameraFeedTrigger=true;
                camera->Trigger();
                //SaveIMGFile("data/buff.bmp",&test1_buff);
        
                session_ACK=true;
            }
            catch (std::invalid_argument iaex) {
            
            snprintf(err_str,sizeof(err_str),"Caught an error! LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            }

        }while(false);

        LOGE("//////");

        }
        else if(checkTL("EX",dat))
        {
        LOGI("Trigger.......");

        {

            char* imgSrcPath=NULL; 
            if (json != NULL)
            {
            imgSrcPath =(char* )JFetch(json,"imgsrc",cJSON_String);
            if (imgSrcPath == NULL)
            {
                snprintf(err_str,sizeof(err_str),"No entry:imgSrcPath in it LINE:%04d",__LINE__);
                LOGE("%s",err_str);
            }
            }
            DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);


            
            acvImage *srcImg=NULL;
            if(imgSrcPath!=NULL)
            {
            int ret_val = LoadIMGFile(&tmp_buff,imgSrcPath);
            if(ret_val==0)
            {
                srcImg = &tmp_buff;
            }
            }


            if(srcImg==NULL)
            {
            mainThreadLock.unlock();
            LOGV("Do camera Fetch..");
            camera->TriggerMode(1);
            LOGV("LOCK...");
            mainThreadLock.lock();
            camera->Trigger();
            LOGV("LOCK BLOCK...");
            mainThreadLock.lock();
            
            LOGV( "unlock");
            mainThreadLock.unlock();
            srcImg = camera->GetImg();
            cacheImage.ReSize(srcImg);
            acvCloneImage(srcImg,&cacheImage,-1);
            //SaveIMGFile("data/test1.bmp",srcImg);
            }

            try {
                ImgInspection_DefRead(matchingEng,srcImg,1,"data/featureDetect.json");
                const FeatureReport * report = matchingEng.GetReport();

                if(report!=NULL)
                {
                cJSON* jobj = matchingEng.FeatureReport2Json(report);
                AttachStaticInfo(jobj);
                char * jstr  = cJSON_Print(jobj);
                cJSON_Delete(jobj);

                //LOGI("__\n %s  \n___",jstr);
                bpg_dat=GenStrBPGData("SG", jstr);//SG report : signature360
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);

                delete jstr;
                }
                else
                {
                sprintf(tmp,"{}");
                bpg_dat=GenStrBPGData("SG", tmp);
                bpg_dat.pgID=dat->pgID;
                datCH_BPG.data.p_BPG_data=&bpg_dat;
                self->SendData(datCH_BPG);
                }
            }
            catch (std::invalid_argument iaex) {
            snprintf(err_str,sizeof(err_str),"Caught an error! LINE:%04d",__LINE__);
            LOGE("%s",err_str);
            }



            bpg_dat=GenStrBPGData("IM", NULL);
            BPG_data_acvImage_Send_info iminfo={img:&dataSend_buff,scale:4};
            //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
            ImageDownSampling(dataSend_buff,*srcImg,iminfo.scale,param_default.map);
            bpg_dat.callbackInfo = (uint8_t*)&iminfo;
            bpg_dat.callback=DatCH_BPG_acvImage_Send;
            bpg_dat.pgID=dat->pgID;
            datCH_BPG.data.p_BPG_data=&bpg_dat;
            BPG_protocol->SendData(datCH_BPG);

        }
        
        session_ACK=true;
        }
        else if(checkTL("RC",dat))
        {

        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        
        char *target =(char* )JFetch(json,"target",cJSON_String);
        if(target==NULL)
        {

        }
        else if(strcmp(target,"camera_ez_reconnect") == 0 )
        {
            
            delete camera;
            camera=NULL;
            

            
            camera = getCamera(CamInitStyle);


            for(int i=0;camera==NULL;i++)
            {
            LOGV("Camera init retry[%d]...",i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            camera = getCamera(CamInitStyle);
            }
            LOGV("DatCH_BPG1_0:%p",camera);
            

            LOGV("DatCH_BPG1_0");
            this->camera = camera;
            callbk_obj.camera=camera;


        }
        
        session_ACK=true;
        }
        else if(checkTL("ST",dat))
        {
        
        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        void *target;
        int type = getDataFromJson(json,"DoImageTransfer",&target);
        if(type==cJSON_False)
        {
            DoImageTransfer=false;
            session_ACK=true;
        }
        else if( type ==cJSON_True)
        {
            DoImageTransfer=true;
            session_ACK=true;
        }

        char *path  = JFetch_STRING(json,"LoadCameraCalibration");
        if(path!=NULL)
        {
            int ret = LoadCameraCalibrationFile(path);
            if(ret)session_ACK=true;
        }


        LOGI("dat->dat_raw:%s",dat->dat_raw);
        LOGI("DoImageTransfer:%d",DoImageTransfer);
        cJSON *camSettingObj = JFetch_OBJECT(json,"CameraSetting");
        if(camera && camSettingObj)
        {
            CameraSetup(*camera, *camSettingObj);
        }
        
        }
        else if(checkTL("PR",dat))
        {
        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        void *target;
        char *IP  = JFetch_STRING(json,"ip");
        double *port_number  = JFetch_NUMBER(json,"port");
        if(IP!=NULL && port_number!=NULL)
        {
            try{
            delete_Ext_Util_API();
            LOGI("clean Ext_Util_API....");
            exApi=new Ext_Util_API(IP,*port_number);
            LOGI("new Ext_Util_API OK...");
            exApi->start_RECV_Thread();
            LOGI("start_RECV_Thread...");
            char* retJson =  exApi->SYNC_cmd_cameraCalib("*.jpg",7,9);
            LOGI("SYNC_cmd_cameraCalib...\n\n:%s",retJson);
            session_ACK=true;
            }
            catch(int errN)
            {
            sprintf(err_str,"[PR] Ext_Util_API init error:%d",errN);
            }
        }
        else if( exApi && IP==NULL && port_number==NULL)
        {
            delete_Ext_Util_API();
            session_ACK=true;
        }
        else
        {
            sprintf(err_str,"[PR] ip:%p port:%p",IP,port_number);
        }
        
        }
        else if(checkTL("PD",dat))
        {
        
        DatCH_Data datCH_BPG=
            BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

        void *target;
        char *IP  = JFetch_STRING(json,"ip");
        double *port_number  = JFetch_NUMBER(json,"port");
        if(IP!=NULL && port_number!=NULL)
        {
            try{
            delete_MicroInsp_FType();
            mift=new MicroInsp_FType(IP,*port_number);
            session_ACK=true;
            }
            catch(int errN)
            {
            sprintf(err_str,"[PR] MicroInsp_FType init error:%d",errN);
            }
        }
        else if( mift && IP==NULL && port_number==NULL)
        {
            delete_MicroInsp_FType();
            session_ACK=true;
        }
        else
        {
            sprintf(err_str,"[PR] ip:%p port:%p",IP,port_number);
        }
        
        }
        DatCH_Data datCH_BPG=
        BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);


        sprintf(tmp,"{\"start\":false,\"cmd\":\"%c%c\",\"ACK\":%s,\"errMsg\":\"%s\"}",
        dat->tl[0],dat->tl[1],(session_ACK)?"true":"false",err_str);
        bpg_dat=GenStrBPGData("SS", tmp);
        bpg_dat.pgID=dat->pgID;
        datCH_BPG.data.p_BPG_data=&bpg_dat;
        self->SendData(datCH_BPG);

        mainThreadLock.unlock();
        cJSON_Delete(json);

    }
    break;
    
    default:
        LOGI("type:%d, UNKNOWN type",data.type);
    }



    return 0;
}

void zlibDeflate_testX(acvImage *img,acvImage *buff,IMG_COMPRESS_FUNC collapse_func, IMG_COMPRESS_FUNC uncollapse_func)
{


  int tLen=img->GetHeight()*img->GetWidth();
  int imgc_Len=3*img->GetHeight()*img->GetWidth();

  if(collapse_func)
  {
    imgc_Len= collapse_func(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
    img->CVector[0],3*img->GetHeight()*img->GetWidth());
  }


  size_t compresSize = 3*buff->GetHeight()*buff->GetWidth();

    compresSize = zlibDeflate(buff->CVector[0],3*buff->GetHeight()*buff->GetWidth(),
                img->CVector[0], imgc_Len,5);

  printf("Compressed size is: %lu/%lu:%.5f\n",  compresSize,imgc_Len,(float)compresSize/(imgc_Len));

  size_t unCompresSize = zlibInflate(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
                buff->CVector[0], compresSize);
  printf("Uncompressed size is: %lu\n",  unCompresSize);


  imgc_Len = unCompresSize;
  if(uncollapse_func)
  {
    imgc_Len= uncollapse_func(img->CVector[0],3*img->GetHeight()*img->GetWidth(),
    img->CVector[0],unCompresSize);
  }

  printf("imgc_Len size is: %lu\n",  imgc_Len);

}




int ImgInspection_DefRead(MatchingEngine &me ,acvImage *test1,int repeatTime,char *defFilename)
{
  char *string = ReadText(defFilename);
  //printf("%s\n%s\n",string,defFilename);
  int ret = ImgInspection_JSONStr(me ,test1, repeatTime,string);
  free(string);
  return ret;
}


int ImgInspection(MatchingEngine &me ,acvImage *test1,acvRadialDistortionParam param,int repeatTime)
{

  LOGI("============w:%d h:%d====================",test1->GetWidth(),test1->GetHeight());
  if(test1->GetWidth()*test1->GetHeight()==0)
  {
      return -1;
  }
  clock_t t = clock();
  for(int i=0;i<repeatTime;i++)
  {
    me.setRadialDistortionParam(param);
    me.FeatureMatching(test1);
  }

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();

  return 0;
  //ContourFeatureDetect(test1,&test1_buff,tar_signature);
  //SaveIMGFile("data/target_buff.bmp",&test1_buff);

}



int ImgInspection_JSONStr(MatchingEngine &me ,acvImage *test1,int repeatTime,char *jsonStr)
{

  me.ResetFeature();
  me.AddMatchingFeature(jsonStr);
  ImgInspection(me,test1,param_default,repeatTime);
  return 0;

}

float  acvImageDiff(acvImage* img1,acvImage *img2,float *ret_max_diff,int skipSampling)
{
  if(skipSampling<1)skipSampling=1;
  uint64_t diffSum=0;
  int diffMax=0;
  int count=0;
  for(int i=0;i<img1->GetHeight();i+=skipSampling)
  {
    for(int j=0;j<img1->GetWidth();j+=skipSampling)
    {
      count++;
      int diff = img1->CVector[i][3*j] - img2->CVector[i][3*j];
      diff*=diff;
      diffSum+=diff;
      if(diffMax<diff)
      {
        diffMax=diff;
      }
    }
  }
  if(ret_max_diff)*ret_max_diff=sqrt(diffMax);
  return sqrt((float)diffSum/(count));
}

void  acvImageAve(acvImage* imgStackRes,acvImage *imgStack,int stackingN)
{
  for(int i=0;i<imgStackRes->GetHeight();i++)
  {
    for(int j=0;j<imgStackRes->GetWidth();j++)
    {
      int pixSum=0;
      for(int k=0;k<stackingN;k++)
      {
        pixSum+=imgStack[k].CVector[i][3*j];
      }
      imgStackRes->CVector[i][3*j]=
      imgStackRes->CVector[i][3*j+1]=
      imgStackRes->CVector[i][3*j+2]=pixSum/stackingN;
    }
  }
}

void  acvImageBlendIn(acvImage* imgOut,int* imgSArr,acvImage *imgB,int Num)
{
  for(int i=0;i<imgOut->GetHeight();i++)
  {
    for(int j=0;j<imgOut->GetWidth();j++)
    {
      int *pixSum=&(imgSArr[i*imgOut->GetWidth()+j]);
      if(Num==0)
      {
        *pixSum = imgB->CVector[i][3*j];
      }
      else
      {
        *pixSum += imgB->CVector[i][3*j];
      }
      imgOut->CVector[i][3*j]=
      imgOut->CVector[i][3*j+1]=
      imgOut->CVector[i][3*j+2]=(*pixSum/(Num+1));
    }
  }
}


clock_t pframeT;
acvImage proBG;
void CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void* context)
{
  static acvImage test1_buff;
  static int stackingC=0;
  static acvImage imgStackRes;


  clock_t t = clock();

  
  LOGI("frameInterval:%fms \n", ((double)t - pframeT) / CLOCKS_PER_SEC * 1000);
  pframeT=t;

  LOGV("cb->cameraFeedTrigger:%d",cb->cameraFeedTrigger); 
  if(!cb->cameraFeedTrigger)
  {
    LOGE( "unlock");
    mainThreadLock.unlock();
    return;
  }
  CameraLayer &cl_GMV=*((CameraLayer*)&cl_obj);
  
  acvImage &capImg=*cl_GMV.GetImg();
  imgStackRes.ReSize(&capImg);


  
  if(capImg.GetHeight()==proBG.GetHeight() &&capImg.GetWidth()==proBG.GetWidth()  )
  {
    for (int i = 0; i < capImg.GetHeight(); i++)
    {
      for (int j = 0; j < capImg.GetWidth(); j++)
      {
        int stdBG = proBG.CVector[i][j*3];
        int curBri = capImg.CVector[i][j*3];
        curBri=curBri*200/stdBG;
        if(curBri>255)curBri=255;
        capImg.CVector[i][j*3] = 
        capImg.CVector[i][j*3+1] = 
        capImg.CVector[i][j*3+2] = curBri;
      }
    }
  }


  int ret=0;

    //stackingC=0;

  if(0&&stackingC!=0)
  {
    float diffMax=0;
    float diff = acvImageDiff(&imgStackRes,&capImg,&diffMax,30);
    LOGV("diff:%f  max:%f",diff,diffMax);
    if(diff>7||diffMax>30)
    {
      stackingC=0;
    }
  }

  //if(stackingC!=0)return;
  if(0)
  {
    static vector <int>imgStackRes_deep;
    imgStackRes_deep.resize(capImg.GetWidth()*capImg.GetHeight());
      


    LOGV("stackingC:%d",stackingC);
    acvImageBlendIn(&imgStackRes,&(imgStackRes_deep[0]),&capImg,stackingC);

    LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);

    //acvImageAve(&imgStackRes,imgStack,pre_stackingIdx+1);

    ret = ImgInspection(matchingEng,&imgStackRes,param_default,1);
  }
  else
  {
    ret = ImgInspection(matchingEng,&capImg,param_default,1);
    if(stackingC==0)
    {
      
      acvCloneImage(&capImg,&imgStackRes,-1);

    }
  }
  stackingC++;

  LOGI("%fms \n---------------------", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);


  {

    using Ms = std::chrono::milliseconds;

    while(!mainThreadLock.try_lock_for(Ms(100)))//Lock and wait 100 ms
    {
      LOGE( "try lock");
      //Still locked
      if(!cb->cameraFeedTrigger)//If the flag is closed then, exit
      {
        LOGE( "cb->cameraFeedTrigger is off return..");
        return;
      }
    }
  }

  BPG_data bpg_dat;
  do{
    char tmp[100];
    DatCH_Data datCH_BPG=
      BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);


    sprintf(tmp,"{\"start\":true}");
    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    bpg_dat.pgID= cb->CI_pgID;
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);

    try {

        const FeatureReport * report = matchingEng.GetReport();

        int stat=FeatureReport_sig360_circle_line_single::STATUS_NA;
        if(report->type==FeatureReport::binary_processing_group)
        {
            vector<const FeatureReport*> &reports = 
            *(report->data.binary_processing_group.reports);
            if(reports.size()==1 && reports[0]->type==FeatureReport::sig360_circle_line)
            {
                vector<FeatureReport_sig360_circle_line_single> &srep=
                    *(reports[0]->data.sig360_circle_line.reports);
                
                int centerIdx=-1;
                float min_dist=99999999;

                for(int k=0;k<srep.size();k++)
                {
                    float dist = 
                        abs(srep[k].Center.X-capImg.GetWidth()/2)+
                        abs(srep[k].Center.Y-capImg.GetHeight()/2);
                    
                    if(min_dist>dist)
                    {
                        centerIdx=k;
                        min_dist=dist;
                    }
                }

                if(centerIdx!=-1)
                {
                    stat=FeatureReport_sig360_circle_line_single::STATUS_SUCCESS;
                    vector<FeatureReport_judgeReport> &jrep= *(srep[centerIdx].judgeReports);

                    for(int k=0;k<jrep.size();k++)
                    {
                        if(jrep[k].status==FeatureReport_sig360_circle_line_single::STATUS_NA)
                        {
                            stat=FeatureReport_sig360_circle_line_single::STATUS_NA;
                            break;
                        }
                        if(jrep[k].status==FeatureReport_sig360_circle_line_single::STATUS_FAILURE)
                        {
                            stat=FeatureReport_sig360_circle_line_single::STATUS_FAILURE;
                        }
                    }
                }

            }


        }
        
        if(cb->mift)
        {
            char buffx[100];
            int len = sprintf(buffx,"{\"type\":\"inspRep\",\"idx\":1,\"status\":%d}",stat);
            cb->mift->send_data((uint8_t*)buffx,len);
        }
    



        if(report!=NULL)
        {
          cJSON* jobj = matchingEng.FeatureReport2Json(report);
          AttachStaticInfo(jobj);
          char * jstr  = cJSON_Print(jobj);
          cJSON_Delete(jobj);

          //LOGI("__\n %s  \n___",jstr);
          bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("RP", jstr);
          bpg_dat.pgID= cb->CI_pgID;
          datCH_BPG.data.p_BPG_data=&bpg_dat;
          BPG_protocol->SendData(datCH_BPG);

          delete jstr;
        }
        else
        {
          sprintf(tmp,"{}");
          bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("RP", tmp);
          bpg_dat.pgID= cb->CI_pgID;
          datCH_BPG.data.p_BPG_data=&bpg_dat;
          BPG_protocol->SendData(datCH_BPG);
        }
    }
    catch (std::invalid_argument iaex) {
        LOGE( "Caught an error!");
    }

    //if(stackingC==0)
    if(DoImageTransfer){
      
      bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("IM", NULL);
      BPG_data_acvImage_Send_info iminfo={img:&test1_buff,scale:4};
      //acvThreshold(srcImg, 70);//HACK: the image should be the output of the inspection but we don't have that now, just hard code 70
      ImageDownSampling(test1_buff,capImg,iminfo.scale,param_default.map);
      bpg_dat.callbackInfo = (uint8_t*)&iminfo;
      bpg_dat.callback=DatCH_BPG_acvImage_Send;
      bpg_dat.pgID= cb->CI_pgID;
      datCH_BPG.data.p_BPG_data=&bpg_dat;
      BPG_protocol->SendData(datCH_BPG);

    }



    sprintf(tmp,"{\"start\":false, \"continue\":%s,\"ACK\":true}",(cb->cameraFeedTrigger)?"true":"false");
    bpg_dat=DatCH_CallBack_BPG::GenStrBPGData("SS", tmp);
    bpg_dat.pgID= cb->CI_pgID;
    datCH_BPG.data.p_BPG_data=&bpg_dat;
    BPG_protocol->SendData(datCH_BPG);

    //SaveIMGFile("data/MVCamX.bmp",&test1_buff);
    //exit(0);
    if(cb->cameraFeedTrigger)
    {
      LOGV("cb->cameraFeedTrigger:%d Get Next frame...",cb->cameraFeedTrigger);
      //std::this_thread::sleep_for(std::chrono::milliseconds(100));
      //cl_GMV.Trigger();
    }
  }while(false);

  LOGI("%fms \n", ((double)clock() - t) / CLOCKS_PER_SEC * 1000);
  t = clock();


  LOGE( "unlock");
  mainThreadLock.unlock();

}


int DatCH_CallBack_WSBPG::DatCH_WS_callback(DatCH_Interface *ch_interface, DatCH_Data data, void* callback_param)
{
  //first stage of incoming data
  //and first stage of outcoming data if needed
  if(data.type!=DatCH_Data::DataType_websock_data)return -1;
  DatCH_WebSocket *ws=(DatCH_WebSocket*)callback_param;
  websock_data ws_data = *data.data.p_websocket;
  LOGI("SEND>>>>>>..websock_data..\n");
  if( (BPG_protocol->MatchPeer(NULL) || BPG_protocol->MatchPeer(ws_data.peer)))
  {
    LOGI("SEND>>>>>>..MatchPeer..\n");
    BPG_protocol->SendData(data);// WS [here]-(prot)> App
  }


  switch(ws_data.type)
  {
      case websock_data::eventType::OPENING:
          printf("OPENING peer %s:%d  sock:%d\n",
            inet_ntoa(ws_data.peer->getAddr().sin_addr),
            ntohs(ws_data.peer->getAddr().sin_port),ws_data.peer->getSocket());
          if(ws->default_peer == NULL){
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

          if(ws->default_peer == ws_data.peer )
          {
            LOGI("SEND>>>>>>..HAND_SHAKING_FINISHED..\n");
            DatCH_Data datCH_BPG=
              BPG_protocol->GenMsgType(DatCH_Data::DataType_BPG);

            LOGI("SEND>>>>>>..GenMsgType..\n");
            BPG_data BPG_dat;
            datCH_BPG.data.p_BPG_data=&BPG_dat;
            BPG_dat.tl[0]='H';
            BPG_dat.tl[1]='R';
            char tmp[]="{\"AA\":5}";
            BPG_dat.size=sizeof(tmp)-1;
            BPG_dat.dat_raw =(uint8_t*) tmp;
            //App [here]-(prot)> WS
            BPG_protocol->SendData(datCH_BPG);
          }
          else
          {
            ws->disconnect(ws_data.peer->getSocket());
          }
      break;
      case websock_data::eventType::DATA_FRAME:
          printf("DATA_FRAME >> frameType:%d frameL:%d data_ptr=%p\n",
              ws_data.data.data_frame.type,
              ws_data.data.data_frame.rawL,
              ws_data.data.data_frame.raw
              );


      break;
      case websock_data::eventType::CLOSING:

        printf("CLOSING peer %s:%d\n",
        inet_ntoa(ws_data.peer->getAddr().sin_addr), ntohs(ws_data.peer->getAddr().sin_port));
        cb->cameraFeedTrigger=false;
        camera->TriggerMode(1);
        cb->delete_MicroInsp_FType();
        cb->delete_Ext_Util_API();
          
      break;
      default:
        return -1;
  }
  return 0;

}
int DatCH_CallBack_WSBPG::callback(DatCH_Interface *from, DatCH_Data data, void* callback_param)
{

    LOGI("DatCH_CallBack_WSBPG:_______type:%d________",data.type);
    int ret_val=0;
    switch(data.type)
    {
      case DatCH_Data::DataType_error:
      {
        LOGE("error code:%d..........",data.data.error.code);
      }
      break;
      case DatCH_Data::DataType_BMP_Read:
      {

        //acvImage *test1 = data.data.BMP_Read.img;

        //ImgInspection(matchingEng,test1,&test1_buff,1,"data/target.json");
      }
      break;

      case DatCH_Data::DataType_websock_data:
        //LOGI("%s:type:DatCH_Data::DataType_websock_data", __func__);
        /*LOGV("lock");
        mainThreadLock.lock();*/
        ret_val =  DatCH_WS_callback(from, data, callback_param);
        /*LOGV("unlock");
        mainThreadLock.unlock();*/
      break;

      default:

        LOGI("type:%d, UNKNOWN type",data.type);
    }
    
    
    return ret_val;
}


int initCamera(CameraLayer_GIGE_MindVision *CL_GIGE)
{
  
  tSdkCameraDevInfo sCameraList[10];
  int retListL = sizeof(sCameraList)/sizeof(sCameraList[0]);
  CL_GIGE->EnumerateDevice(sCameraList,&retListL);
  
  if(retListL<=0)return -1;
	for (int i=0; i< retListL;i++)
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
  
  if(CL_GIGE->InitCamera(&(sCameraList[0]))==CameraLayer::ACK)
  {
    return 0;
  }
  return -1;
}
int initCamera(CameraLayer_BMP_carousel *CL_bmpc)
{
  return CL_bmpc==NULL?-1:0;
}


CameraLayer *getCamera(int initCameraType)
{

  CameraLayer *camera=NULL;
  if(initCameraType==0 || initCameraType==1)
  {
    CameraLayer_GIGE_MindVision *camera_GIGE;
    camera_GIGE=new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV,NULL);
    LOGV("initCamera");

    try{
      if(initCamera(camera_GIGE)==0)
      {
        camera=camera_GIGE;
      }
      else
      {
        delete camera;
        camera=NULL;
      }
    }
    catch(std::exception& e)
    {
      delete camera;
      camera=NULL;
    }

  }


  if(camera==NULL && (initCameraType ==0 || initCameraType==2) )
  {
    CameraLayer_BMP_carousel *camera_BMP;
    LOGV("CameraLayer_BMP_carousel");
    camera_BMP=new CameraLayer_BMP_carousel(CameraLayer_Callback_GIGEMV,NULL,"data/BMP_carousel_test");
    camera=camera_BMP;
  }

  if(camera==NULL)
  {
    return NULL;
  }
  
  LOGV("TriggerMode(1)");
  camera->TriggerMode(1);
  camera->SetExposureTime(12570.5110);
  camera->SetAnalogGain(2);

  
  LOGV("Loading data/default_camera_setting.json....");
  int ret = LoadCameraSetup(*camera, "data/default_camera_setting.json");
  LOGV("ret:%d",ret);
  return camera;
}

int mainLoop(bool realCamera=false)
{
  /**/
  
  printf(">>>>>\n" );
  bool pass=false;
  int retryCount=0;
  while(!pass)
  {
    try
    {
        websocket =new DatCH_WebSocket(4090);
        pass=true;
    }
    catch (exception& e) {
        retryCount++;
        int delaySec=5;
        LOGE("websocket server open retry:%d wait for %dsec",retryCount,delaySec);
        std::this_thread::sleep_for(std::chrono::milliseconds(delaySec*1000));
    }
  }
  printf(">>>>>\n" );
  
  {
    
    CameraLayer *camera = getCamera(CamInitStyle);
    
    
    for(int i=0;camera==NULL;i++)
    {
      LOGV("Camera init retry[%d]...",i);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      camera = getCamera(CamInitStyle);
    }
    LOGV("DatCH_BPG1_0:%p",camera);

    cb->camera = camera;
    callbk_obj.camera=camera;


    BPG_protocol->SetEventCallBack(cb,NULL);
  }


  websocket->SetEventCallBack(&callbk_obj,websocket);

  while(websocket->runLoop(NULL) == 0)
  {
    
  }

  return 0;
}


void sigroutine(int dunno) { /* 信號處理常式，其中dunno將會得到信號的值 */
  switch (dunno) {
    case SIGINT:
      LOGE("Get a signal -- SIGINT \n");
      LOGE("Tear down websocket.... \n");
      delete websocket;
    break;
  }
  return;
}


void CameraLayer_Callback_BMP(CameraLayer &cl_obj, int type, void* context)
{
  CameraLayer_BMP &clBMP=*((CameraLayer_BMP*)&cl_obj);
  LOGV("Called.... %d, filename:%s",type,clBMP.GetCurrentFileName().c_str());
}

int simpleTest(char *imgName, char *defName)
{
  //return testGIGE();;
  CameraLayer_BMP cl_BMP(CameraLayer_Callback_BMP,NULL);

  CameraLayer::status ret = cl_BMP.LoadBMP(imgName);
  if(ret != CameraLayer::ACK)
  {
    LOGE("LoadBMP failed: ret:%d",ret);
    return -1;
  }
  ImgInspection_DefRead(matchingEng,cl_BMP.GetImg(),1,defName);

  const FeatureReport * report = matchingEng.GetReport();

  if(report!=NULL)
  {
    cJSON* jobj = matchingEng.FeatureReport2Json(report);
    AttachStaticInfo(jobj);
    char * jstr  = cJSON_Print(jobj);
    cJSON_Delete(jobj);
    LOGI("...\n%s\n...",jstr);
    
  }
  printf("Start to send....\n");


  return 0;
}


acvCalibMap* parseCM_info(PerifProt::Pak pakCM)
{
  int count=-1;
  count = PerifProt::countValidArr(&pakCM);
  if(count<=0)
  {
      return NULL;
  }
  PerifProt::Pak p2 = PerifProt::parse(pakCM.data);
  PerifProt::Pak IF_pak,DM_pak,MX_pak,MY_pak,DS_pak;
  int ret;

  ret = PerifProt::fetch(&pakCM,"IF",&IF_pak);//if(ret<0)return ret;
  ret = PerifProt::fetch(&pakCM,"DM",&DM_pak);if(ret<0)return NULL;
  ret = PerifProt::fetch(&pakCM,"DS",&DS_pak);if(ret<0)return NULL;
  ret = PerifProt::fetch(&pakCM,"MX",&MX_pak);if(ret<0)return NULL;
  ret = PerifProt::fetch(&pakCM,"MY",&MY_pak);if(ret<0)return NULL;

  uint64_t *dim=(uint64_t *)DM_pak.data;//the original dimension
  uint64_t *dimS=(uint64_t *)DS_pak.data;//Downscaled dimension(the forwardCalibMap)
  double *MX_data=(double *)MX_pak.data;
  double *MY_data=(double *)MY_pak.data;

  
  acvCalibMap *cm_x=new acvCalibMap(MX_data,MY_data,dimS[0],dimS[1],dim[0],dim[1]);
  //cm_x.generateInvMap(dim[0],dim[1]);
  for(int i=0;i<7;i++)
  {
    float coord[]={1017,  377};
    cm_x->i2c(coord);
    LOGI("i2c:={%f,%f}",coord[0],coord[1]);
    cm_x->c2i(coord);
    LOGI("c2i:={%f,%f}",coord[0],coord[1]);
    //cm_x->fwdMapDownScale(1);
    //cm_x.generateInvMap(dim[0],dim[1]);
  }
  //exit(0);
  return cm_x;
  
}

int testCode()
{
  return 0;

  acvImage img;
  
  acvImage bw_img;
  int ret = LoadPNGFile(&img,"data/B5G-25X45X60.png");
  bw_img.ReSize(&img);
  acvThreshold(&bw_img, 80, 0);
  ret = SavePNGFile("data/B5G-25X45X60__.png", &bw_img);

  return -1;
}
#include <vector>
int main(int argc, char** argv)
{
  if(testCode()!=0)return -1;


  machine_h= get_machine_hash();

  srand(time(NULL));
  /*auto lambda = []() { LOGV("Hello, Lambda"); };
  lambda();*/
  #ifdef __WIN32__
  {
      WSADATA wsaData;
      int iResult;
      // Initialize Winsock
      iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
      if (iResult != 0) {
          printf("WSAStartup failed with error: %d\n", iResult);
          return 1;
      }
      
  }
  #endif

  for(int i=0;i<argc;i++)
  {
    bool doMatch=true;
    if (strcmp(argv[i],"CamInitStyle=0") == 0 )
    {
      CamInitStyle=0;
    }
    else if (strcmp(argv[i],"CamInitStyle=1") == 0 )
    {
      CamInitStyle=1;
    }
    else if (strcmp(argv[i],"CamInitStyle=2") == 0 )
    {
      CamInitStyle=2;
    }
    else
    {
      doMatch=false;
      LOGE("unknown param[%d]:%s",i,argv[i]);
    }

    if(doMatch)
    {
      LOGE("CMD param[%d]:%s ...OK",i,argv[i]);
    }
  }

  if(0){


    acvImage calibImage;
    acvImage test_buff;
    int ret_val = LoadIMGFile(&calibImage,"data/calibImg.BMP");
    if(ret_val!=0)return -1;
    ImgInspection_DefRead(matchingEng,&calibImage,1,"data/cameraCalibration.json");

    
    const FeatureReport * report = matchingEng.GetReport();

    if(report!=NULL)
    {
      cJSON* jobj = matchingEng.FeatureReport2Json(report);
      AttachStaticInfo(jobj);
      //cJSON_AddNumberToObject(jobj, "session_id", session_id);
      char * jstr  = cJSON_Print(jobj);
      cJSON_Delete(jobj);

      LOGI("__\n %s  \n___",jstr);

      delete jstr;
    }
    return 0;
  }


  if(0)
  {
    char *imgName="data/test1.BMP";
    char *defName = "data/cache_def.json";

    LoadIMGFile(&proBG,"data/proBG.BMP");
    //char *imgName="data/calib_cam1_surfaceGo.bmp";
    //char *defName = "data/cameraCalibration.json";
    //
    return simpleTest(imgName,defName);
  }

  int ret = LoadCameraCalibrationFile("data/default_camera_param.json");
  if(ret)
  {
      LOGE("LoadCameraCalibrationFile ERROR");
      throw new std::runtime_error("LoadCameraCalibrationFile ERROR");
  }


  if(0)//GenBG map
  {
    
    acvImage BuffImage;
    acvImage BGImage;
    acvImage BGImage_Ori;
    int ret_val = LoadIMGFile(&BGImage,"data/BG.BMP");
    if(ret_val)return ret_val;
    BuffImage.ReSize(&BGImage);
    BGImage_Ori.ReSize(&BGImage);
    acvCloneImage(&BGImage,&BGImage_Ori,0);

    acvWindowMax(&BGImage, 5);

    acvBoxFilter(&BuffImage,&BGImage,20);
    acvBoxFilter(&BuffImage,&BGImage,20);
    acvBoxFilter(&BuffImage,&BGImage,20);
    acvBoxFilter(&BuffImage,&BGImage,20);
    
    acvCloneImage(&BGImage,&BGImage,0);
    

    if(BGImage_Ori.GetHeight() == BGImage.GetHeight() && BGImage_Ori.GetWidth() == BGImage.GetWidth() )
    {
      for(int i=0;i<BGImage_Ori.GetHeight();i++)
      {
        for(int j=0;j<BGImage_Ori.GetWidth();j++)
        {
          int BG_Ori = BGImage_Ori.CVector[i][3*j];
          int BG = BGImage.CVector[i][3*j];
          int diff = BG_Ori-BG-5;
          if(diff<0)diff=-diff;
          diff*=3;
          if(diff>255)diff=255;

          BGImage_Ori.CVector[i][3*j]=diff;
          BGImage_Ori.CVector[i][3*j+1]=diff;
          BGImage_Ori.CVector[i][3*j+2]=diff;
        }
      }
    }
    SaveIMGFile("data/BGImage_OriX.bmp",&BGImage_Ori);
    SaveIMGFile("data/proBG.bmp",&BGImage);

    return 0;
  }
  LoadIMGFile(&proBG,"data/proBG.BMP");


  signal(SIGINT, sigroutine);
  //printf(">>>>>>>BPG_END: callbk_BPG_obj:%p callbk_obj:%p \n",&callbk_BPG_obj,&callbk_obj);
  return mainLoop(true);
}
