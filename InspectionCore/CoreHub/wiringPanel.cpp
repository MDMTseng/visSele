#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string>

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


DatCH_BPG1_0 *BPG_protocol = new DatCH_BPG1_0(NULL);
DatCH_CallBack_BPG *cb = new DatCH_CallBack_BPG(BPG_protocol);
DatCH_CallBack_WSBPG callbk_obj;



DatCH_WebSocket *websocket = NULL;
cv::Mat frame;



TSQueue<image_pipe_info*> cam_(50);

TSQueue<image_pipe_info*> cam_Q2(50);
std::vector<std::thread *> stageThread1;
std::vector<std::thread *> stageThread2;




int _argc;
char **_argv;


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

 

void ImageClone(cv::Mat &dst,acvImage &src)
{
  if(src.GetHeight()!=dst.rows || src.GetWidth()!=dst.cols)
  {
    LOGI("$$$$$$$$$$$$$$$$    RESIZE     $$$$$$$$$$$$$$$$");
    dst.create(src.GetHeight(),src.GetWidth(), CV_8UC3); 
  }

  cv::Vec3b* ptr =dst.ptr<cv::Vec3b>(0);
  uint8_t *pix=&ptr[0][0];

  // memset(pix,128,dst.rows * dst.cols*3);

  memcpy(pix,src.CVector[0],dst.rows * dst.cols*3);
}


 
 
void CameraLayer_Callback_GIGEMV(CameraLayer &cl_obj, int type, void *context)
{

  LOGI("===============context:%p\n",context);
  if (type != CameraLayer::EV_IMG)
    return;
  CameraLayer &cl_GMV = *((CameraLayer *)&cl_obj);


  acvImage *img= cl_GMV.GetFrame();
  LOGI(">>>:::W:%d H:%d\n",img->GetWidth(),img->GetHeight());
  if((img->GetHeight()*img->GetWidth())==0)
  {
    // LOGI("EMPTY IMG:::W:%d H:%d\n",img->GetWidth(),img->GetHeight());
    return;
  }

  camera_channel_info *chInfo=(camera_channel_info *)context;
  LOGI("===============rp_ref:%p H:%d\n",chInfo->rp_ref,img->GetHeight());
  image_pipe_info *pipe = chInfo->rp_ref->fetchResrc_blocking();
  if(pipe!=NULL )
  {
    // pipe->img.ReSize(img);
    // if()
    ImageClone(pipe->cvImg,*img);
    
    // acvCloneImage(&pipe->img,img,-1);
    // pipe->camchinfo=chInfo;
    pipe->camchinfo=chInfo;
    cam_.push_blocking(pipe);
    // chInfo->rp_ref->retResrc(pipe);
  }

}


int T1ID=0;
void stageThread1_threads(){

  // std::thread::id tid = std::this_thread::get_id();
  int tid=T1ID++;
  while(1)
  {

    image_pipe_info *pinfo;
    if(cam_.pop_blocking(pinfo)!=true)
    {
      break;
    }
    LOGI("thread1:%d",tid);
    
    if(1)
    {
      cam_Q2.push_blocking(pinfo);
    }
    else
    {
      pinfo->camchinfo->rp_ref->retResrc(pinfo);//recycle
    }
  }

}


int T2ID=0;
void stageThread2_threads(){  
  int tid=T2ID++;
  // std::thread::id tid = std::this_thread::get_id();

  vector <image_pipe_info *> buffer;

  
  vector <image_pipe_info *> gatherSet;
  while(1)
  {

    image_pipe_info *pinfo;
    if(cam_Q2.pop_blocking(pinfo)!=true)
    {
      break;
    }
    LOGI("thread2:%d",tid);
    // buffer.push_back(pinfo);

    pinfo->camchinfo->rp_ref->retResrc(pinfo);//recycle
  }



}


void ImgPipeProcessCenter_imp(image_pipe_info *imgPipe)
{
  
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

CameraLayer *getCamera(int initCameraType=0,void * context=NULL,std::string bmpCPath="data/BMP_carousel_test")
{

  CameraLayer *camera = NULL;
  if (initCameraType == 0 || initCameraType == 1)
  {
    CameraLayer_GIGE_MindVision *camera_GIGE;
    camera_GIGE = new CameraLayer_GIGE_MindVision(CameraLayer_Callback_GIGEMV, context);
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
    LOGI("CameraLayer_BMP_carousel");
    camera_BMP = new CameraLayer_BMP_carousel(CameraLayer_Callback_GIGEMV, context, bmpCPath);
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

void BPG_protocol_send(DatCH_Data dat)
{
  BPG_protocol->SendData(dat);
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

void ImgPipeProcessThread(bool *terminationflag)
{

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
      int port = 1407;
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
  LOGI(">>>>>\n");

  {
    // image_pipe_info *c1=new image_pipe_info();

    cb->cameraArray.resize(4);
    for(int i=0;i<cb->cameraArray.size();i++)
    {
      camera_channel_info &ccinfo=cb->cameraArray[i];

      std::string path="data/testBMP";
      path+=std::to_string(i);
      LOGI("path:%s", path.c_str());
      ccinfo.camera= getCamera(0,&ccinfo,path.c_str());
      ccinfo.id=i;
      ccinfo.rp_ref=new resourcePool<image_pipe_info>(10);
      LOGI("[%d]:ccinfo.rp_ref:%p",i,ccinfo.rp_ref);
    }



    BPG_protocol->SetEventCallBack(cb, NULL);
  }
  // LOGI("Camera:%p", cb->camera1);

  websocket->SetEventCallBack(&callbk_obj, websocket);

  for(int i=0;i<5;i++)
  {
    stageThread1.push_back(new std::thread(stageThread1_threads));
  }

  
  for(int i=0;i<5;i++)
  {
    stageThread2.push_back(new std::thread(stageThread2_threads));
  }


  
  for(int i=0;i<cb->cameraArray.size();i++)
  {
    cb->cameraArray[i].camera->TriggerMode(0);

  }
  // cb->camera1->TriggerMode(0);
  // cb->camera2->TriggerMode(0);
  // cb->camera3->TriggerMode(0);
  // cb->camera4->TriggerMode(0);

  LOGI("SetEventCallBack is set...");
  while (websocket->runLoop(NULL) == 0)
  {
  }


  for(int i=0;i<stageThread1.size();i++)
  {
    stageThread1[i]->join();
    delete(stageThread1[i]);
  }
  for(int i=0;i<stageThread2.size();i++)
  {
    stageThread2[i]->join();
    delete(stageThread2[i]);
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
