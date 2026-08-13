
#include "CameraLayer_BMP.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>   // cv::warpAffine
#include <cmath>

// Nearest-neighbor pixel fetch (single channel). OOB returns 0.
static inline uint8_t cvUnsignedMap1Sampling_Nearest(const cv::Mat &m, float x, float y, int channel)
{
    int rX = (int)std::round(x);
    int rY = (int)std::round(y);
    if (rX < 0 || rY < 0 || rX > m.cols - 1 || rY > m.rows - 1) return 0;
    return m.ptr<uint8_t>(rY)[rX * m.channels() + channel];
}

#include <logctrl.h> 
#include <dirent.h> 
LOG_MODULE("cam.bmp");
#include <thread>

#include<sys/time.h>
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

CameraLayer_BMP::CameraLayer_BMP(CameraLayer::BasicCameraInfo camInfo,std::string misc,CameraLayer_Callback cb,void* context):CameraLayer(camInfo,misc,cb,context)
{
    gaussianNoiseTable_M.resize(256);
    for(int i=0;i<gaussianNoiseTable_M.size();i++)
    {
        float u = rand() / (float)RAND_MAX;
        float v = rand() / (float)RAND_MAX;
        float x = sqrt(-2 * log(u)) * cos(2 * M_PI * v);
        gaussianNoiseTable_M[i]=(int)(x*1000000);//million scale up
    }
    // ROI_H=ROI_W=999999;
    a_gain=1;
    SetExposureTime(exp_time_100ExpUs);
}



CameraLayer::status CameraLayer_BMP::ExtractFrame(uint8_t* imgBuffer,int channelCount,size_t pixelCount)
{

      int newX,newY;
      int newW,newH;

      CalcROI(&newX,&newY,&newW,&newH);

      // LOGI("%f %f %f %f",tmpX,tmpY,tmpW,tmpH);
      if(pixelCount<newW*newH)
      {
        return NAK;
      }      

      if(exp_time_us==0)exp_time_us=exp_time_100ExpUs;
      float brightnessMult=1.0f;
      if (aug.brightness_jitter_en && aug.brightness_jitter_pct > 0) {
        float r = ((float)(rand()%2000)/1000)-1; //-1~1
        brightnessMult = r * (aug.brightness_jitter_pct/100.0f) + 1.0f;
      }

      // Full brightness unless exposure simulation is asked for. The file's
      // own exposure is already in its pixels; the real camera's exposure has
      // nothing to say about it.
      const float expoMult = aug.expo_sim_en
          ? (aug.expo_us * aug.expo_gain / exp_time_100ExpUs) : 1.0f;
      int tExp=(1<<13)*brightnessMult*expoMult;
      LOGI("tExp:%d",tExp);


      // img.ReSize(newW,newH);
      acv_XY rcenter((float)(newW/2), (float)(newH/2));


      if(1)
      {
        static float rotate=0;

        int noiseRange = aug.noise_en ? aug.noise_range : 0;

        // "Cropped" == the requested ROI is a sub-window of the loaded image
        // (there is surrounding source pixel data to rotate/wobble into).
        // A full-frame request (ROI == whole image) is NOT cropped, so we
        // skip rotation/offset and only apply brightness + noise -- rotating a
        // full frame would just reveal black borders.
        bool cropped = (newW < img_load.cols) || (newH < img_load.rows);

        if(!cropped)
        {
          rotate=0;
        }
        else if (aug.rotate_en && aug.rotate_step_deg > 0)
        {
          rotate += aug.rotate_step_deg * M_PI/180;
          if(rotate >= 2*M_PI) rotate -= 2*M_PI;   // keep angle bounded
          LOGI("ROTATE:%f",rotate*180/M_PI);
        }
        else
        {
          rotate = 0;
        }



        if(cropped && rotate!=0)
        {
          // Real rotation about the crop-window center, plus a vertical
          // wobble of amplitude y_offset_r (tied to the rotation phase).
          // warpAffine samples img_load directly -- the 2x3 matrix M maps a
          // destination (crop-local) pixel back to source coords
          // (WARP_INVERSE_MAP):  src = R(rotate)*(dst - cropCenter)
          //                            + srcCenter + (0, offsetY)
          const double c = std::cos(rotate);
          const double s = std::sin(rotate);
          const double cropCx = newW/2.0, cropCy = newH/2.0;
          const double srcCx  = newX + cropCx, srcCy = newY + cropCy;
          const double offsetY = aug.y_offset_en ? aug.y_offset_r*std::sin(rotate) : 0.0;

          cv::Mat M = (cv::Mat_<double>(2,3) <<
            c, -s, srcCx           - (c*cropCx - s*cropCy),
            s,  c, srcCy + offsetY - (s*cropCx + c*cropCy));

          cv::Mat rotated;
          cv::warpAffine(img_load, rotated, M, cv::Size(newW, newH),
                         cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                         cv::BORDER_CONSTANT, cv::Scalar(0,0,0));

          // Apply exposure + per-pixel noise on the rotated crop. Sampling and
          // bounds are already handled by warpAffine (OOB -> black border).
          const int src_ch = rotated.channels();
          for (int i = 0; i < newH; i++) {
            const uint8_t *src = rotated.ptr<uint8_t>(i);
            uint8_t *dst = imgBuffer + (i*newW)*channelCount;
            for (int j = 0; j < newW; j++) {
              const uint8_t *sp = src + j*src_ch;
              int N = 0;
              if (noiseRange > 0)
                N = (rand() % (2*noiseRange + 1)) - noiseRange;
              for (int ch = 0; ch < channelCount; ch++) {
                int pix = sp[ch < src_ch ? ch : 0];
                int d = N + ((int)((((uint64_t)pix) * (uint64_t)tExp) >> 12));
                if (d < 0) d = 0;
                else if (d > 255) d = 255;
                dst[j*channelCount + ch] = (uint8_t)d;
              }
            }
          }
        }
        else
        {
          // Row-by-row copy that honors ROI offset (newX, newY) and the
          // loaded image's actual stride (img_load.cols * src_channels).
          // The previous memcpy fast-path assumed the loaded image and the
          // ROI shared origin AND width, which is false for any PNG/BMP
          // whose dims don't match the requested ROI -- the row stride
          // mismatch produced the striped-corruption pattern.
          // imread(IMREAD_COLOR) always returns 3-channel BGR (grayscale
          // PNG is replicated to BGR), so src_ch is 3.
          const int src_ch = img_load.channels();
          for (int i = 0; i < newH; i++) {
            int li = i + newY;
            if (li < 0 || li >= img_load.rows) {
              memset(imgBuffer + (i*newW)*channelCount, 0, newW*channelCount);
              continue;
            }
            const uint8_t *src = img_load.ptr<uint8_t>(li);
            uint8_t *dst = imgBuffer + (i*newW)*channelCount;
            for (int j = 0; j < newW; j++) {
              int lj = j + newX;
              if (lj < 0 || lj >= img_load.cols) {
                for (int c = 0; c < channelCount; c++) dst[j*channelCount + c] = 0;
                continue;
              }
              const uint8_t *sp = src + lj*src_ch;
              int N = 0;
              if (noiseRange > 0)
                N = (rand() % (2*noiseRange + 1)) - noiseRange;
              for (int c = 0; c < channelCount; c++) {
                int pix = sp[c < src_ch ? c : 0];
                int d = N + ((int)((((uint64_t)pix) * (uint64_t)tExp) >> 12));
                if (d < 0) d = 0;
                else if (d > 255) d = 255;
                dst[j*channelCount + c] = (uint8_t)d;
              }
            }
          }
        }
      }
      else
      {      
        static float rotate=0;
      
        int noiseRange=15;
        if(newX==0&&newY==0)
        {
          rotate=0;
        }
        else
        {
          float baseAngle=0*M_PI/180;
          float endAngle= 360*M_PI/180;

          if(rotate<baseAngle)rotate=baseAngle;
          else if(rotate>endAngle)rotate=baseAngle;
          else
          {
            rotate+=0.1*M_PI/180;
          }
          // rotate+=1*M_PI/180;
          LOGI("ROTATE:%f",rotate*180/M_PI);
          // 
        }


        if(rotate!=0)
        {
          for(int i=0;i<newH;i++)//Add noise
          for(int j=0;j<newW;j++)
          {
            acv_XY pixCoord=acvVecSub(acv_XY((float)j, (float)i),rcenter);
            
            pixCoord = acvRotation(rotate,pixCoord);
            pixCoord=acvVecAdd(pixCoord,rcenter);
            pixCoord=acvVecAdd(pixCoord,acv_XY((float)newX, (float)newY));

            
            pixCoord=acvVecSub((acv_XY){
              MIRROR_X?(float)img_load.cols:pixCoord.x*2,
              MIRROR_Y?(float)img_load.rows:pixCoord.y*2},pixCoord);


            // for(int k=0;k<channelCount;k++)
            // {
            //   float pix= acvUnsignedMap1Sampling(&img_load, pixCoord, k);
              
            //   int N=0;
            //   if(noiseRange>0)
            //     N= (rand()%(2*noiseRange+1))-noiseRange;

            //   int d = N+ ((uint64_t)(pix*tExp))>>13;

            //   if(d<0)d=0;
            //   else if(d>255)d=255;
              
            //   imgBuffer[(i*newW+j)*channelCount+k]=d;

            // }

            // float pix= acvUnsignedMap1Sampling(&img_load, pixCoord, 0);
            float pix= cvUnsignedMap1Sampling_Nearest(img_load, pixCoord.x, pixCoord.y, 0);
            
            int N=0;
            if(noiseRange>0)
              N= (rand()%(2*noiseRange+1))-noiseRange;

            int d = N+ (((uint64_t)(pix*tExp))>>13);

            if(d<0)d=0;
            else if(d>255)d=255;
            
            imgBuffer[(i*newW+j)*channelCount+0]=
            imgBuffer[(i*newW+j)*channelCount+1]=
            imgBuffer[(i*newW+j)*channelCount+2]=d;

          }
        }
        else if(1)
        {
          
          // LOGI(">>>:::W:%d H:%d\n",img->GetWidth(),img->GetHeight());
          // if(1)
          // {
          //   memcpy(imgBuffer,img_load.ptr<uint8_t>(0),);
          //   acvCloneImage(&img_load,&img,-1);    

          // }  
          // else 
          for(int i=0;i<newH;i++)//exposure add
          {
            int li=i+newY;
            if(li<0 || li>=img_load.rows)continue;
            for(int j=0;j<newW;j++)
            {
              int lj=j+newX;
              if(lj<0 || lj>=img_load.cols)continue;
              int N=0;
              if(noiseRange>0)
                N= (rand()%(2*noiseRange+1))-noiseRange;
              int d =N+ ((img_load.ptr<uint8_t>(li)[lj*3]*tExp)>>13);
              
              if(d<0)d=0;
              else if(d>255)d=255;
              
              imgBuffer[(i*newW+j)*channelCount+0]=
              imgBuffer[(i*newW+j)*channelCount+1]=
              imgBuffer[(i*newW+j)*channelCount+2]=d;
              // img.CVector[i][j*3] = 
              // img.CVector[i][j*3+1] =
              // img.CVector[i][j*3+2] = d;

            }
          }
        }
      }

      // int noiseRange=5;
      // for(int i=0;i<newH;i++)//Add noise
      // {
      //     for(int j=0;j<newW;j++)
      //     {
      //         // int u = rand()%(gaussianNoiseTable_M.size());
      //         // int x = gaussianNoiseTable_M[u] * 3/1000000 + 0;
      //         int x=(rand()%(2*noiseRange))-noiseRange;
      //         int d = imgBuffer[(i*newW+j)*channelCount+0];
      //         d+=x;
      //         if(d<0)d=0;
      //         else if(d>255)d=255;
              
      //         imgBuffer[(i*newW+j)*channelCount+0] =
      //         imgBuffer[(i*newW+j)*channelCount+1] =
      //         imgBuffer[(i*newW+j)*channelCount+2] =d;

      //     }
      // }


  return ACK;
}

CameraLayer_BMP::status CameraLayer_BMP::CalcROI(int* X,int* Y,int* W,int* H)
{
  
  int tmpW=ROI_W;
  int tmpH=ROI_H;
  int tmpX=ROI_X;
  int tmpY=ROI_Y;

  LOGI("%d %d %d %d",tmpX,tmpY,tmpW,tmpH);
  if(tmpX<0){
    tmpX=0;
  }
  if(tmpY<0){
    tmpY=0;
  }
  if(tmpX<1)
  {
    tmpX=(int)(tmpX*img_load.cols);
  }
  if(tmpY<1)
  {
    tmpY=(int)(tmpY*img_load.rows);
  }

  if(tmpX>=img_load.cols-5)
  {
    tmpX=img_load.cols-5-1;
  }
  if(tmpY>=img_load.rows-5)
  {
    tmpY=img_load.rows-5-1;
  }

  if(tmpW<0){
    tmpW=0;
  }
  if(tmpH<0){
    tmpH=0;
  }
  if(tmpW<1)
  {
    tmpW=(int)(tmpW*img_load.cols);
  }
  if(tmpH<1)
  {
    tmpH=(int)(tmpH*img_load.rows);
  }

  if(tmpW+tmpX>img_load.cols)
  {
    tmpW=img_load.cols-tmpX;
  }
  if(tmpH+tmpY>img_load.rows)
  {
    tmpH=img_load.rows-tmpY;
  }
  if(X)*X=tmpX;
  if(Y)*Y=tmpY;
  if(W)*W=tmpW;
  if(H)*H=tmpH;

  return ACK;
}
CameraLayer_BMP::status CameraLayer_BMP::LoadBMP(std::string fileName)
{
    status ret_status;
    m.lock();

    int ret = 0;
    
    //if(img.GetWidth()<100)//Just to skip image loading
    cacheUseCounter++;
    if(this->fileName.compare(fileName)!=0 || cacheUseCounter>20)//check if the name isn't equal
    {
      cacheUseCounter=0;
      this->fileName = fileName;
        LOGI("Loading:%s",fileName.c_str());
        img_load = cv::imread(fileName.c_str(), cv::IMREAD_COLOR);
        ret = img_load.empty() ? -1 : 0;
        if (ret == 0 && !img_load.isContinuous()) img_load = img_load.clone();
        LOGI("ret:%d",ret);
    }
    if(ret!=0)
    {
      ret_status=NAK;
      callback(*this,CameraLayer::EV_ERROR,context);
    }
    else
    {
      ret_status=ACK;
      // ret_status=NAK;

      // int X,Y,W,H;

      // CalcROI(&X,&Y,&W,&H);

      
      // CameraLayer::frameInfo fi_={
      //   timeStamp_us:0,
      //   width:(uint32_t)W,
      //   height:(uint32_t)H,
      // };

      // callback(*this,CameraLayer::EV_IMG,context);

    }
    m.unlock();
    return ret_status;
}



CameraLayer::status CameraLayer_BMP::SetROI(int x, int y, int w, int h,int zw,int zh)
{
  ROI_X=x;
  ROI_Y=y;
  ROI_W=w;
  ROI_H=h;
  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP::GetROI(int *x, int *y, int *w, int *h,int*zw,int *zh)
{
  return CalcROI(x,y,w,h);
}
CameraLayer::status CameraLayer_BMP::SetMirror(int Dir,int en)
{
  if(Dir==0)
  {
    MIRROR_X=en;
  }
  else if(Dir==1)
  {
    MIRROR_Y=en;
  }
  else
  {
    return CameraLayer::NAK;
  }
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetAnalogGain(float gain)
{
  a_gain=gain;
  return CameraLayer::ACK;
}


CameraLayer::status CameraLayer_BMP::SetExposureTime(float time_us)
{
  //default 5ms as 100% exposure
  exp_time_us=time_us;

  return CameraLayer::ACK;
}

CameraLayer::status CameraLayer_BMP::GetExposureTime(float *ret_time_us)
{
  if(ret_time_us)
  {
    *ret_time_us=exp_time_us;
    return CameraLayer::ACK;
  }
  return CameraLayer::NAK;
}
