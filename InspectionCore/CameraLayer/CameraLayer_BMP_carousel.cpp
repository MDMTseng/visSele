#include "CameraLayer_BMP.hpp"
#include "acvImage_SpDomainTool.hpp"

#include <logctrl.h> 
#include <dirent.h> 
#include <thread>

#include<sys/time.h>



LOG_MODULE("cam.bmp");

CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context):CameraLayer_BMP(camInfo,misc,cb,context)
{
    if(camInfo.driver_name!=CameraLayer_BMP_carousel::getDriverName())
    {
      std::string excpMsg = "param_driver_name:" + camInfo.driver_name + " != "+CameraLayer_BMP_carousel::getDriverName();
      throw std::invalid_argument(excpMsg);
    }
    updateFolder(misc);


    connection_data.serial_number=misc;


    fileIdx=0;
    cameraThread=NULL;
    isThreadWorking=false;
    connection_data=camInfo;

    char buff[700];
    snprintf(buff, sizeof(buff),
    "{\
      \"type\":\"CameraLayer_BMP_carousel\",\
      \"name\":\"BMP_CAM\",\
      \"id\":\"%s\",\
      \"folder_name\":\"%s\"\
    }",misc.c_str(),misc.c_str());
    cam_json_info.assign(buff);
    LOGI(">>>%s",cam_json_info.c_str());
}


// CameraLayer_BMP_carousel::CameraLayer_BMP_carousel(CameraLayer_Callback cb,void* context,std::string folderName):
//     CameraLayer_BMP(cb,context)
// {
//     updateFolder(folderName);
//     fileIdx=0;
//     cameraThread=NULL;
//     isThreadWorking=false;


//     char buff[700];
//     snprintf(buff, sizeof(buff),
//     "{\
//       \"type\":\"CameraLayer_BMP_carousel\",\
//       \"name\":\"BMP_CAM\",\
//       \"id\":\"%s\",\
//       \"folder_name\":\"%s\"\
//     }",folderName.c_str(),folderName.c_str());
//     cam_json_info.assign(buff);
//     LOGI(">>>%s",cam_json_info.c_str());
// }

CameraLayer_BMP_carousel::~CameraLayer_BMP_carousel()
{
    LOGI("Descructor...");
    // The thread outlived the object it runs on. Nothing here stopped it, and
    // StopAquisition was the base-class no-op, so after `delete camera` the
    // acquisition thread kept calling LoadNext -> updateFolder on a freed
    // files_in_folder and kept handing frames to the pipeline through a
    // callback belonging to a destroyed layer. See the crash dumps behind
    // camera_lifetime_lock in wiringPanel.cpp: that lock bounds the INSPECTION
    // thread, it never had anything to say about this one.
    //
    // Joining here is what makes the rest safe: if the thread is inside
    // callback(...) when the destructor runs, the join simply waits for it to
    // return, and no member is touched after that point.
    StopThread();
}

// One stop path for everyone. TriggerMode's stop branch used to be the only
// place that knew how to do this.
void CameraLayer_BMP_carousel::StopThread()
{
    {
      std::lock_guard<std::mutex> lk(stop_m);
      ThreadTerminationFlag = 1;
    }
    stop_cv.notify_all();

    if(cameraThread == NULL)
      return;

    // A stop asked for from inside the thread itself (a callback that turns
    // the camera off) would be a self-join: std::system_error, i.e. a crash in
    // the code that exists to prevent one. Setting the flag is all we can do
    // and all that is needed -- the loop exits on its own, and whoever
    // destroys the layer later does the join.
    if(cameraThread->get_id() == std::this_thread::get_id())
      return;

    cameraThread->join();
    delete cameraThread;
    cameraThread = NULL;
}

// The loop's only sleep. Returns true when a stop was requested, so the caller
// leaves immediately instead of finishing the interval first.
bool CameraLayer_BMP_carousel::WaitOrStop(int ms)
{
    std::unique_lock<std::mutex> lk(stop_m);
    if(ms<=0)
      return ThreadTerminationFlag != 0;
    return stop_cv.wait_for(lk, std::chrono::milliseconds(ms),
                            [this]{ return ThreadTerminationFlag != 0; });
}



CameraLayer::status CameraLayer_BMP_carousel::updateFolder(std::string folderName)
{
    // Scan into a local list and publish it in one move at the end. The
    // directory walk is the slow part and it needs no lock; doing it in place
    // meant a WebUI camera_info poll could be iterating files_in_folder while
    // the acquisition thread was between resize(0) and the last push_back.
    std::vector<std::string> found;
    DIR *d;
    struct dirent *dir;
    d = opendir(folderName.c_str());
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(dir->d_name[0]=='.')continue;
            std::string str(dir->d_name);
            // Only what imread can decode.
            //
            // The list used to be every entry in the directory, so pointing
            // the carousel at a folder that also holds a .json, a .hydef or a
            // README put those in the rotation, imread returned empty, and the
            // frame path had to cope with "no image" on a file the UI was
            // happily listing as frame 3 of 12. Filtering here means the list
            // the operator sees IS the list that can be shown.
            {
                size_t dot = str.find_last_of('.');
                if(dot == std::string::npos) continue;
                std::string ext = str.substr(dot+1);
                for(auto &ch : ext) ch = (char)tolower((unsigned char)ch);
                static const char *kImgExt[] = {
                    "bmp","png","jpg","jpeg","tif","tiff","pgm","ppm","webp",NULL };
                bool ok=false;
                for(int e=0; kImgExt[e]; e++)
                    if(ext == kImgExt[e]) { ok=true; break; }
                if(!ok) continue;
            }
            str = folderName+"/"+str;
            //LOGV("FILE::%s",str.c_str());
            found.push_back(str);
        }
        closedir(d);

        {
          std::lock_guard<std::mutex> lk(files_m);
          this->folderName = folderName;
          files_in_folder.swap(found);
        }
        return ACK;
    }
    else
    {
        // A folder that cannot be opened has no files, same as before: the
        // list is emptied rather than left showing the previous folder's.
        std::lock_guard<std::mutex> lk(files_m);
        this->folderName = folderName;
        files_in_folder.clear();
        return NAK;
    }
}

std::vector<std::string> CameraLayer_BMP_carousel::GetFileList()
{
    updateFolder(GetFolderName());
    std::lock_guard<std::mutex> lk(files_m);
    return files_in_folder;
}


CameraLayer::status CameraLayer_BMP_carousel::LoadNext(bool call_cb)
{
    updateFolder(GetFolderName());

    // Index arithmetic and the lookup belong to the same critical section:
    // both are only valid against the list length they were computed from,
    // and the other thread's `next`/`setfolder` can change that length
    // between the two. The path is copied out so LoadBMP's imread runs
    // unlocked.
    std::string path;
    {
      std::lock_guard<std::mutex> lk(files_m);
      if(files_in_folder.size()==0)return NAK;
      fileIdx++;
      // The <0 half is not new behaviour: the old comparison was int against
      // size_t, so a negative index converted to something enormous and landed
      // in the same reset. Written out, it stays true after the (int) cast.
      if(fileIdx<0 || fileIdx>=(int)files_in_folder.size())
      {
          fileIdx=0;
      }
      path = files_in_folder[fileIdx];
    }

    CameraLayer_BMP::status status=LoadBMP(path);
    if(status==ACK)
    {
      struct timeval tp;
      gettimeofday(&tp, NULL);
      uint64_t _100us = ((uint64_t)tp.tv_sec) * 1000000 + tp.tv_usec; //get current timestamp in milliseconds

      int newX,newY;
      int newW,newH;
      CalcROI(&newX,&newY,&newW,&newH);
      
      CameraLayer::frameInfo fi_={
        timeStamp_us:(uint64_t)_100us,
        width:(uint32_t)newW,
        height:(uint32_t)newH,
      };
      // Report the loaded file's channel count so a grayscale image actually
      // reaches the core as a 1-channel frame (see GetLoadedChannels).
      fi_.channelCount = GetLoadedChannels();
      fi = fi_;
      
      if(call_cb)
        callback(*this,CameraLayer::EV_IMG,context);
    }
    else
    {
      CameraLayer::frameInfo fi_={
        timeStamp_us:0,
        width:0,
        height:0,
      };
      fi_.channelCount = 0;
      fi = fi_;
      if(call_cb)
        callback(*this,CameraLayer::EV_ERROR,context);
    }
    return status;
}

CameraLayer::status CameraLayer_BMP_carousel::LoadPrev(bool call_cb)
{
    updateFolder(GetFolderName());
    int idx;
    {
      std::lock_guard<std::mutex> lk(files_m);
      if(files_in_folder.size()==0)return NAK;
      if(fileIdx<=0) fileIdx = (int)files_in_folder.size();
      fileIdx--;
      idx = fileIdx;
    }
    return LoadAt(idx, call_cb);
}

CameraLayer::status CameraLayer_BMP_carousel::ReloadCurrent(bool call_cb)
{
    updateFolder(GetFolderName());
    int idx;
    {
      std::lock_guard<std::mutex> lk(files_m);
      if(files_in_folder.size()==0)return NAK;
      if(fileIdx<0 || fileIdx>=(int)files_in_folder.size()) fileIdx=0;
      idx = fileIdx;
    }
    return LoadAt(idx, call_cb);
}

CameraLayer::status CameraLayer_BMP_carousel::LoadAt(int idx, bool call_cb)
{
    updateFolder(GetFolderName());

    std::string path;
    {
      std::lock_guard<std::mutex> lk(files_m);
      if(files_in_folder.size()==0)return NAK;
      if(idx<0) idx=0;
      if(idx>=(int)files_in_folder.size()) idx=(int)files_in_folder.size()-1;
      fileIdx = idx;
      path = files_in_folder[fileIdx];
    }

    CameraLayer_BMP::status status=LoadBMP(path);
    if(status==ACK)
    {
      struct timeval tp;
      gettimeofday(&tp, NULL);
      uint64_t _100us = ((uint64_t)tp.tv_sec) * 1000000 + tp.tv_usec;
      int newX,newY,newW,newH;
      CalcROI(&newX,&newY,&newW,&newH);
      CameraLayer::frameInfo fi_={
        timeStamp_us:(uint64_t)_100us,
        width:(uint32_t)newW,
        height:(uint32_t)newH,
      };
      // Report the loaded file's channel count so a grayscale image actually
      // reaches the core as a 1-channel frame (see GetLoadedChannels).
      fi_.channelCount = GetLoadedChannels();
      fi = fi_;
      if(call_cb) callback(*this,CameraLayer::EV_IMG,context);
    }
    else
    {
      CameraLayer::frameInfo fi_={ timeStamp_us:0, width:0, height:0 };
      fi_.channelCount = 0;
      fi = fi_;
      if(call_cb) callback(*this,CameraLayer::EV_ERROR,context);
    }
    return status;
}

CameraLayer::status CameraLayer_BMP_carousel::Trigger()
{
    imageTakingCount+=1;
    
    LOGI("imageTakingCount:%d",imageTakingCount);
    if(imageTakingCount==0)imageTakingCount=1;
    return TriggerMode(-1);
}

void CameraLayer_BMP_carousel::ContTriggerThread( )
{
  isThreadWorking=true;
    //The logic here is you always stay in the loop,
    //and execute LoadNext when imageTakingCount >0
    int idle_loop_interval_ms =100;
    while( ThreadTerminationFlag == 0)
    {
        LOGV("TTFlag:%d imageTakingCount:%d triggerMode:%d",
            ThreadTerminationFlag.load(),imageTakingCount,triggerMode);
        int delay_time=0;
        if(imageTakingCount>0 || triggerMode==0)
        {
            LOGV("imageTakingCount:%d,ThreadTerminationFlag:%d",imageTakingCount,ThreadTerminationFlag.load());
            for(int i=0;i<10;i++)
            {
              // Ten failing LoadNext calls are ten directory scans, ten imreads
              // and ten callbacks; whoever is waiting on the join should not
              // have to sit through the rest of them.
              if(ThreadTerminationFlag!=0)
              {
                break;
              }
              if(LoadNext()==ACK)
              {
                break;
              }
              //otherwise retry
            }
            imageTakingCount--;
            
            if(triggerMode==0)
            {
                imageTakingCount=0;
            }
        }
        else
        {//So we need a delay when imageTakingCount<=0 to prevent loop becoming too fast
            //std::this_thread::sleep_for(std::chrono::milliseconds(idle_loop_interval_ms));
            delay_time+=idle_loop_interval_ms;
        }

        if(imageTakingCount==0 && triggerMode!=0)
        {
            break;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(frameInterval_ms-delay_time>0)
        {
            if(WaitOrStop(frameInterval_ms-delay_time))
              break;
        }
        if(modeTriggerSim_sleep>0)
        {
            if(WaitOrStop(modeTriggerSim_sleep))
              break;
        }
    }
    //ThreadTerminationFlag = 0;

  isThreadWorking=false;
}


CameraLayer::status CameraLayer_BMP_carousel::SetFrameRate(float frame_rate)
{
  frameInterval_ms=1000/frame_rate;
  // frameInterval_ms=500;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP_carousel::TriggerCount(int count)
{
  imageTakingCount = count - 1;
  return Trigger();
}

CameraLayer::status CameraLayer_BMP_carousel::isInOperation()
{
    return CameraLayer::ACK;
}
CameraLayer::status CameraLayer_BMP_carousel::SnapFrame(CameraLayer_Callback snap_cb,void *cb_param)
{
  
  CameraLayer_Callback _callback=callback;
  void* _context=context;//replace the callback
  callback=snap_cb;
  context=cb_param;
  CameraLayer::status ret_s = LoadNext();

  callback=_callback;
  context=_context;//recover the callback
  return ret_s;
}

CameraLayer::status CameraLayer_BMP_carousel::TriggerMode(int mode)
{
  
    CameraLayer::TriggerMode(mode);
    modeTriggerSim_sleep=0;

    // if(mode==2)
    // {
    //   mode=0;
    //   modeTriggerSim_sleep=10000;
    // }
    if(mode>=0)
    {
      triggerMode = mode;
    }
    if(mode==0 || mode==-1 )
    {
      LOGI("ThreadTerminationFlag:%d cameraThread:%p>>>",ThreadTerminationFlag.load(),cameraThread);
        {
          std::lock_guard<std::mutex> lk(stop_m);
          ThreadTerminationFlag = 0;
        }
        if(isThreadWorking==false && cameraThread!=NULL)//Try to clean up the thread
        {
          cameraThread->join();
          delete cameraThread;
          cameraThread = NULL;
        }
        if(cameraThread == NULL)
        {
            // Marked working by the creator, not by the thread itself. The
            // thread sets this on its first line, but until it is scheduled
            // the flag still reads false -- and the join above, taken on a
            // thread that is in fact about to run forever in continuous mode,
            // never returns. StartAquisition() right after a TriggerMode(0)
            // walks straight into that window.
            isThreadWorking = true;
            cameraThread = new std::thread(&CameraLayer_BMP_carousel::ContTriggerThread, this);
        }
    }
    else
    {
        StopThread();
    }
    return ACK;
}

// Acquisition here is the carousel thread, nothing else. CameraSetup() brackets
// its writes with StopAquisition()/StartAquisition(), so the start side has to
// exist -- otherwise a settings refresh without a "trigger_mode" key would stop
// the fake camera for good.
CameraLayer::status CameraLayer_BMP_carousel::StopAquisition()
{
  StopThread();
  return ACK;
}

CameraLayer::status CameraLayer_BMP_carousel::StartAquisition()
{
  // Same start as TriggerMode's, deliberately: in trigger mode (triggerMode!=0
  // with nothing pending) the loop just exits again, which is what it did
  // before this override existed.
  return TriggerMode(-1);
}


int CameraLayer_BMP_carousel::listAddDevices(std::vector<CameraLayer::BasicCameraInfo> &devlist)
{
  int vlistL=5;
  for(int i=0;i<vlistL;i++)
  {
    CameraLayer::BasicCameraInfo info;
    
    info.name=
    info.id=
    info.model=
    info.serial_number="BMP_carousel_"+std::to_string(i);
    info.vender="CameraLayer_BMP_carousel";
    info.ctx=NULL;
    info.driver_name=getDriverName();
    devlist.push_back(info);
  }

  return vlistL;


}