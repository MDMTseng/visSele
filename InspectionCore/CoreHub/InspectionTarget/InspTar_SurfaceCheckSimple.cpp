
#include "InspTar_SurfaceCheckSimple.hpp"
#include "StageInfo_Orientation.hpp"
#include "StageInfo.hpp"
#include <iostream>
using namespace cv;

using namespace std;

template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_SurfaceCheckSimple::InspectionTarget_SurfaceCheckSimple(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
  :InspectionTarget(id,def,belongMan,local_env_path)
{
  type=InspectionTarget_SurfaceCheckSimple::TYPE();
}

// bool InspectionTarget_SurfaceCheckSimple::stageInfoFilter(shared_ptr<StageInfo> sinfo)
// {
//   // if(sinfo->typeName())



//   for(auto tag : sinfo->trigger_tags )
//   {
//     if(tag=="_STREAM_")
//     {
//       return false;
//     }
//     if(tag==id)return true;
//     if( matchTriggerTag(tag))
//       return true;
//   }
//   return false;
// }

future<int> InspectionTarget_SurfaceCheckSimple::futureInputStagePool()
{
  return async(launch::async,&InspectionTarget_SurfaceCheckSimple::processInputStagePool,this);
}

int InspectionTarget_SurfaceCheckSimple::processInputPool()
{
  int poolSize=input_pool.size();
  for(int i=0;i<poolSize;i++)
  {
    shared_ptr<StageInfo> curInput=input_pool[i];
    singleProcess(curInput);

    input_pool[i]=NULL;
  }
  input_pool.clear();


  return poolSize;//run all

}

cv::Scalar ImgRegionAveraging(Mat &img,cJSON *refRegion)
{
  if(refRegion==NULL)return cv::Scalar(NAN,NAN,NAN);

  cJSON *region= refRegion;

  int x=(int)JFetch_NUMBER_ex(region,"x");
  int y=(int)JFetch_NUMBER_ex(region,"y");
  int w=(int)JFetch_NUMBER_ex(region,"w");
  int h=(int)JFetch_NUMBER_ex(region,"h");


  LOGI("xywh:%d,%d,%d,%d",x,y,w,h);
  LOGI("img col row:%d,%d",img.cols,img.rows);


  if(x<0)
  {
    w+=x;
    x=0;
  }

  if(y<0)
  {
    h+=y;
    y=0;
  }

  if(w>img.cols)
  {
    w=img.cols;
  }
  if(h>img.rows)
  {
    h=img.rows;
  }



  int pixCSum=0;

  cv::Mat roi(img,cv::Rect(x,y,w,h));
  pixCSum+=w*h;
  cv::Scalar sum= cv::sum(roi);
  // sum/=(w*h);
  // LOGI("w:%d h:%d sum:[%f,%f,%f]",w,h,sum[0],sum[1],sum[2]);
  cv::Scalar avgPix=sum/pixCSum;
  LOGI("pixCSum:%d avgPix:[%f,%f,%f]",pixCSum,avgPix[0],avgPix[1],avgPix[2]);
  return avgPix;
}

cv::Scalar ImgRegionsAveraging(Mat &img,cJSON *refRegionArray)
{
  if(refRegionArray==NULL)return cv::Scalar(NAN,NAN,NAN);

  int regionCount=cJSON_GetArraySize(refRegionArray);
  cv::Scalar rsum={0};
  int pixCSum=0;

  for(int i=0;i<regionCount;i++)
  {
    
    cJSON *region= cJSON_GetArrayItem(refRegionArray,i);

    int x=(int)JFetch_NUMBER_ex(region,"x");
    int y=(int)JFetch_NUMBER_ex(region,"y");
    int w=(int)JFetch_NUMBER_ex(region,"w");
    int h=(int)JFetch_NUMBER_ex(region,"h");


    LOGI("xywh:%d,%d,%d,%d",x,y,w,h);
    LOGI("img col row:%d,%d",img.cols,img.rows);


    if(x<0)
    {
      w+=x;
      x=0;
    }

    if(y<0)
    {
      h+=y;
      y=0;
    }

    if(w>img.cols)
    {
      w=img.cols;
    }
    if(h>img.rows)
    {
      h=img.rows;
    }




    cv::Mat roi(img,cv::Rect(x,y,w,h));
    pixCSum+=w*h;
    cv::Scalar sum= cv::sum(roi);
    rsum+=sum;
    // sum/=(w*h);
    // LOGI("w:%d h:%d sum:[%f,%f,%f]",w,h,sum[0],sum[1],sum[2]);
  }
  cv::Scalar avgPix=rsum/pixCSum;
  LOGI("pixCSum:%d avgPix:[%f,%f,%f]",pixCSum,avgPix[0],avgPix[1],avgPix[2]);
  return avgPix;
}


bool InspectionTarget_SurfaceCheckSimple::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");


  if(type=="useExtParam")
  {

    if(extParam)//remove existing param
    {
      cJSON_Delete(extParam);
      extParam=NULL;
    }
    if(JFetch_FALSE(info,"enable"))
    {
      useExtParam=false;
    }
    else{
      useExtParam=true;
    }
    return true;
  }

  if(type=="extParam")
  {
    if(extParam)
    {
      cJSON_Delete(extParam);
      extParam=NULL;
    }
    extParam=cJSON_Duplicate(info,true);

    if(extParam && useExtParam)
    {
      if(cache_stage_info.get()==NULL)return false;
    
      belongMan->dispatch(cache_stage_info,this);

      while (belongMan->inspTarProcess())
      {
      }
    }


    return true;
  }

  if(type=="show_display_overlay")
  {
    show_display_overlay=!JFetch_FALSE(info,"enable");
    return true;
  }


  if(type=="extract_color")
  {
    if(result_cache_stage_info.get()==NULL)return false;

    acvImage* acv_img= result_cache_stage_info->img_show.get();
    Mat cv_img(acv_img->GetHeight(),acv_img->GetWidth(),CV_8UC3,acv_img->CVector[0]);

    



    cv::Scalar avgPix= ImgRegionAveraging(cv_img,JFetch_OBJECT(info,"region"));
    // LOGI("pixCSum:%d avgPix:[%f,%f,%f]",pixCSum,avgPix[0],avgPix[1],avgPix[2]);
    {
      cJSON* jrep=cJSON_CreateObject();
      cJSON* rep=cJSON_CreateObject();
      cJSON_AddItemToObject(jrep,"report",rep);
      cJSON_AddNumberToObject(rep,"r",avgPix[2]);
      cJSON_AddNumberToObject(rep,"g",avgPix[1]);
      cJSON_AddNumberToObject(rep,"b",avgPix[0]);
      act.send("RP",id,jrep);
      cJSON_Delete(jrep);jrep=NULL;
    }

    return true;
  }



  if(type=="extract_feature")
  {
    string path=JFetch_STRING_ex(info,"image_path");
    if(path=="")
    {
      return false;
    }
    Mat img = imread(path, IMREAD_COLOR);
    if (img.empty())
    {
      return false;
    }

    acvImage src_acvImg;
    src_acvImg.useExtBuffer((BYTE *)img.data,img.rows*img.cols*3,img.cols,img.rows);

    // acvImage src_acvImg(img.cols,img.rows,3);
    // for(int i=0;i<img.rows;i++)for(int j=0;j<img.cols;j++)
    // {
    //   src_acvImg.CVector[i][j*3]=0;
    //   src_acvImg.CVector[i][j*3+1]=0;
    //   src_acvImg.CVector[i][j*3+2]=0;
    // }
    int image_transfer_downsampling=(int)JFetch_NUMBER_ex(info,"image_transfer_downsampling",-1);
    if(image_transfer_downsampling>=1)
    {
      act.send("IM",id,&src_acvImg,image_transfer_downsampling);
    }

    cJSON *refRegions=JFetch_ARRAY(info,"colorExtractInfo.refRegions");
    if(refRegions)
    {
      
      cv::Scalar avgPix= ImgRegionsAveraging(img,refRegions);
      // LOGI("pixCSum:%d avgPix:[%f,%f,%f]",pixCSum,avgPix[0],avgPix[1],avgPix[2]);
      {
        cJSON* jrep=cJSON_CreateObject();
        cJSON* rep=cJSON_CreateObject();
        cJSON_AddItemToObject(jrep,"report",rep);
        cJSON_AddNumberToObject(rep,"R",avgPix[0]);
        cJSON_AddNumberToObject(rep,"G",avgPix[1]);
        cJSON_AddNumberToObject(rep,"B",avgPix[2]);
        act.send("RP",id,jrep);
        cJSON_Delete(jrep);jrep=NULL;
      }

      return true;

    }



    return true;

  }



  return false;
}


cJSON* InspectionTarget_SurfaceCheckSimple::genITIOInfo()
{


  
  cJSON* arr= cJSON_CreateArray();

  {
    cJSON* opt= cJSON_CreateObject();
    cJSON_AddItemToArray(arr,opt);

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "i",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Orientation::stypeName().c_str() ));
    }

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "o",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_SurfaceCheckSimple::stypeName().c_str() ));
    }




  }

  return arr;

}


static Mat getRotTranMat(acv_XY pt1,acv_XY pt2,float theta,bool flipX=false,bool flipY=false)
{
  
  float s=sin(theta);
  float c=cos(theta);

  Point2f srcPoints[3];//原圖中的三點 ,一個包含三維點（x，y）的數組，其中x、y是浮點型數
  Point2f dstPoints[3];//目標圖中的三點  

  int yMult=flipY?-1:1;
    //第一種仿射變換的調用方式：三點法
  //三個點對的值,上面也說了，只要知道你想要變換後圖的三個點的座標，就可以實現仿射變換  
  srcPoints[0] = Point2f(pt1.X, pt1.Y);
  srcPoints[1] = Point2f(pt1.X+s, pt1.Y+c);
  srcPoints[2] = Point2f(pt1.X+c, pt1.Y-s);
	dstPoints[0] = Point2f(pt2.X, pt2.Y);
	dstPoints[1] = Point2f(pt2.X, pt2.Y+yMult);
	dstPoints[2] = Point2f(pt2.X+1, pt2.Y);

  Mat rotTranMat = getAffineTransform(srcPoints, dstPoints);

  // if(flipY)
  // {
  //   Mat flipMat = (Mat_<double>(3,3) << 1, 0, 0, 0, -1, 0, 0, 0, 1);
    
  //   std::cout << "rotTranMat: " << rotTranMat << std::endl;
  //   std::cout << " flipMat:"<< flipMat << std::endl;
  //   rotTranMat=rotTranMat*flipMat;
  //   std::cout << "rotTranMat: " << rotTranMat<< std::endl;
  // }
  // if(flipX)
  // {
  //   Mat flipMat = (Mat_<double>(2,3) << -1, 0, 0, 0, 1, 0);
  //   rotTranMat=flipMat*rotTranMat;
  // }

  return rotTranMat;
}

// def getRotTranMat(pt1,pt2,Theta):
//   s=math.sin(-Theta)
//   c=math.cos(-Theta)

//   pts1 = np.float32([pt1,[pt1[0]+s,pt1[1]+c],[pt1[0]+c,pt1[1]-s]])
//   pts2 = np.float32([pt2,[pt2[0],pt2[1]+1],[pt2[0]+1,pt2[1]]])
//   return cv2.getAffineTransform(pts1,pts2)


void XYWH_clipping(int &X,int &Y,int &W,int &H, int MX,int MY,int MW,int MH)
{
  if(X<MX)
  {
    W-=MX-X;
    X=MX;
  }

  if(Y<MY)
  {
    H-=MY-Y;
    Y=MY;
  }


  if(X>MX+MW)
  {
    X=MX+MW-1;
  }
  if(Y>MY+MH)
  {
    Y=MY+MH-1;
  }

  if(X+W>MX+MW)
  {
    W=MX+MW-X;
  }

  if(Y+H>MY+MH)
  {
    H=MY+MH-Y;
  }
}


void alphaBlend(Mat& img1, Mat&img2, Mat& mask, Mat& blended){
    // Blend img1 and img2 (of CV_8UC3) with mask (CV_8UC1)
    assert(img1.size() == img2.size() && img1.size() == mask.size());
    blended = cv::Mat(img1.size(), img1.type());
    for (int y = 0; y < blended.rows; ++y){
        for (int x = 0; x < blended.cols; ++x){
            float alpha = mask.at<unsigned char>(y, x)/255.0f;
            blended.at<cv::Vec3b>(y,x) = alpha*img1.at<cv::Vec3b>(y,x) + (1-alpha)*img2.at<cv::Vec3b>(y,x);
        }
    }
}





int STAGEINFO_SCS_CAT_BASIC_reducer(int sum_cat,int cat)
{

  switch(sum_cat)
  {
    case STAGEINFO_CAT_UNSET:
      sum_cat=cat;
    break;
    case STAGEINFO_CAT_OK:
      if(cat==STAGEINFO_CAT_NG2 || cat==STAGEINFO_CAT_NG||cat==STAGEINFO_CAT_NA)
        sum_cat=cat;

    break;
    case STAGEINFO_CAT_NG2:
      if(cat==STAGEINFO_CAT_NG|| cat==STAGEINFO_CAT_NA)
        sum_cat=cat;
    break;

    case STAGEINFO_CAT_NG:
      if( cat==STAGEINFO_CAT_NA)
        sum_cat=cat;
    break;

    default:
    case STAGEINFO_CAT_NA:
    case STAGEINFO_CAT_NOT_EXIST:

    break;
  }

  return sum_cat;
}


float findCrossLoc(Mat &m,float value,bool reverseDir=false)
{
  int idx1=-1;
  int val1;
  int idx2=-1;
  int val2;


  int width=m.size().width;
  for(int _i=0;_i<width;_i++)
  {
    int i=(reverseDir==false)?_i:width-_i-1;
    int v=m.at<uint8_t>(0,i);
    // printf("%03d ",v);
    if(v<value)
    {
      idx1=i;
      val1=v;
    }
    else
    {
      idx2=i;
      val2=v;
    }

    if(idx1!=-1 && idx2!=-1)break;
  }
  // printf("\n");


  return (idx2+idx1)/2;
}


// // example matrix
// Mat img = Mat::zeros(256, 128, CV_32FC3);

// // get the pointer (cast to data type of Mat)
// float *pImgData = (float *)img.data;

// // loop through rows, columns and channels
// for (int row = 0; row < img.rows; ++row)
// {
//     for (int column = 0; column < img.cols; ++column)
//     {
//         for (int channel = 0; channel < img.channels(); ++channel)
//         {
//             float value = pImgData[img.channels() * (img.cols * row + column) + channel];
//         }
//     }
// }


          // inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);


void inRangeV2(cv::Mat src, Scalar rangeFrom,Scalar rangeTo,Scalar w,int add_gamma, cv::Mat &dst){

//For each channel
//if in range the output is the distance to the nearest threshold
//if out of range the output is the distance to the nearest threshold in negative
//the range could be warped around
//Exp: T_f=10 T_t=200 => range is 10 11 ....199 200
//v=50 => output is 40
//v=100 => output is 90
//v=190 => output is 10

//v=0 => output is -10
//v=255 => output is -11
//v=220 => output is -20

//Exp: T_f=200 T_t=10 => range is 200 201 202....254 255 0 1....9 10
//v=50 => output is -40
//v=100 => output is -90
//v=190 => output is -10

//v=0 => output is 10
//v=255 => output is 11
//v=220 => output is 20



  dst = Mat(src.size(), CV_8UC1);
  for(int i = 0; i < src.rows; i++) 
  for(int j = 0; j < src.cols; j++)
  {

    cv::Vec3b &srcP=src.at<cv::Vec3b>(i,j);
    
    Scalar score={0};

    for(int k=0;k<3;k++)
    {
      int v = srcP[k];
      int from=rangeFrom[k];
      int to=rangeTo[k];
      bool needsReverse=(from>to);

      if(needsReverse)//reverse the from-to
      {
        int from=rangeTo[k];
        int to=rangeFrom[k];
      }

      int dist1 = v-from;
      int dist2 = to-v;
      int res_v;


      if(dist1>=0 && dist2>=0)//in range
      {
        if(to>=255)
        {
          res_v=dist1;
        }
        if(from<=0)
        {
          res_v=dist2;
        }
        else
        {
          res_v=(dist1<dist2)?dist1:dist2;
        }

      }
      else//out of range
      {
        if(dist1>0)dist1-=255;
        if(dist2>0)dist2-=255;
        res_v=(dist1>dist2)?dist1:dist2;
      }

      if(needsReverse)res_v=-res_v;//reverse the sign
      score[k]=res_v*w[k];
    }
    // score[0]*=w[0];
    int finalScore=0;
    if(score[0]<0 || score[1]<0 || score[2]<0 )
    {
      if(score[0]>0)score[0]=0;
      if(score[1]>0)score[1]=0;
      if(score[2]>0)score[2]=0;

      finalScore=score[0]+score[1]+score[2];
    }
    else
    {
      finalScore=(score[0]>score[1])?score[0]:score[1];

      if(score[2]>finalScore)
        finalScore=score[2];

    }
    finalScore+=add_gamma;
    if(finalScore<0)finalScore=0;
    else if (finalScore>255)finalScore=255;
    

    uchar &dstP=dst.at<uchar>(i,j);
    dstP=finalScore;

  }



}


void InspectionTarget_SurfaceCheckSimple::singleProcess(shared_ptr<StageInfo> sinfo)
{
  if(useExtParam==true && extParam==NULL )
  {
    
    cache_stage_info=sinfo;
    return;
  }


  int64 t0 = cv::getTickCount();
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Orientation *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  auto srcImg=d_sinfo->img;


  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);





  // if()
  
  // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);

  // cJSON *report=cJSON_CreateObject();
  // cJSON* rep_regionInfo=cJSON_CreateArray();
  
  // cJSON_AddStringToObject(report,"id",id.c_str());
  // cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);



  float X_offset=JFetch_NUMBER_ex(def,"x_offset",0);
  float Y_offset=JFetch_NUMBER_ex(def,"y_offset",0);

  float W=JFetch_NUMBER_ex(def,"w");
  float H=JFetch_NUMBER_ex(def,"h");
  float angle_offset=JFetch_NUMBER_ex(def,"angle_offset",0)*M_PI/180;


  float color_ch_mul_r=JFetch_NUMBER_ex(def,"color_ch_mul.r",1);
  float color_ch_mul_g=JFetch_NUMBER_ex(def,"color_ch_mul.g",1);
  float color_ch_mul_b=JFetch_NUMBER_ex(def,"color_ch_mul.b",1);

  // LOGE("color_ch_mul:%f %f %f",color_ch_mul_r,color_ch_mul_g,color_ch_mul_b);

  acvImage *retImage=NULL;

  shared_ptr<StageInfo_SurfaceCheckSimple> reportInfo(new StageInfo_SurfaceCheckSimple());
  
  int default_blurRadius=(int)JFetch_NUMBER_ex(def,"blur_radius",0);
  vector<StageInfo_Orientation::orient> *orienList=&(d_sinfo->orientation);


  vector<StageInfo_Orientation::orient> orienList_ext;

  bool img_order_reverse=JFetch_TRUE(def,"order_reverse");
  
  if(useExtParam==true)
  {
    orienList=&orienList_ext;
  }
  if(useExtParam==true && extParam)
  {
    cJSON *orienJList = JFetch_ARRAY(extParam,"orientation");
    if(orienJList)
    {
      int listL=cJSON_GetArraySize(orienJList);
      for(int i=0;i<listL;i++)
      {
        cJSON *jorient= cJSON_GetArrayItem(orienJList,i);
        StageInfo_Orientation::orient orient;


        orient.flip=JFetch_TRUE(jorient,"flip")==true;
        orient.angle=JFetch_NUMBER_ex(jorient,"angle",0);
        orient.confidence=JFetch_NUMBER_ex(jorient,"confidence",0);

        orient.center.X=JFetch_NUMBER_ex(jorient,"center.x");
        orient.center.Y=JFetch_NUMBER_ex(jorient,"center.y");
        orienList_ext.push_back(orient);
      }
    }
    orienList=&orienList_ext;
  }


  auto &orientationList=*orienList;

  

  int bilateral_d= (int)JFetch_NUMBER_ex(def,"bilateral.d",-1);
  float bilateral_sigmaColor= JFetch_NUMBER_ex(def,"bilateral.sigmaColor",2);
  float bilateral_sigmaSpace=JFetch_NUMBER_ex(def,"bilateral.sigmaSpace",2);

  // LOGI("orientation info size:%d",orientationList.size());
  if(orientationList.size()>0)
  {  
    retImage=new acvImage(W*orientationList.size(),H,3);
    Mat def_temp_img(retImage->GetHeight(),retImage->GetWidth(),CV_8UC3,retImage->CVector[0]);
    


    def_temp_img={0};

    for(int i=0;i<orientationList.size();i++)
    {
      int MATCH_REGION_category=STAGEINFO_CAT_UNSET;
      int MATCH_REGION_score=-1;
      StageInfo_Orientation::orient orientation = orientationList[i];

      StageInfo_SurfaceCheckSimple::MatchRegion_Info mri;
      do{
        if(orientation.confidence<=0)
        {
          MATCH_REGION_category=STAGEINFO_CAT_NOT_EXIST;
          break;
        }
        // cJSON* idxRegion=JFetch_ARRAY(def,("[+"+std::to_string(i)+"+]").c_str());
        int imgOrderIdx=img_order_reverse?(orientationList.size()-1-i):i;
        Mat _def_temp_img_ROI = def_temp_img(Rect(imgOrderIdx*W, 0, W, H));

        float angle = orientation.angle;
        // if(angle>M_PI_2)angle-=M_PI;
        // if(angle<-M_PI_2)angle+=M_PI;
        angle+=angle_offset;

        bool xFlip=false;
        bool yFlip=orientation.flip;
        Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2+X_offset,H/2+Y_offset},-angle,xFlip,yFlip);

        cv::warpAffine(CV_srcImg, _def_temp_img_ROI, rot,_def_temp_img_ROI.size());

        if(color_ch_mul_r!=1 || color_ch_mul_g!=1 || color_ch_mul_b!=1)
        {
          cv::Scalar compScalar(color_ch_mul_b,color_ch_mul_g,color_ch_mul_r);
          multiply(_def_temp_img_ROI,compScalar, _def_temp_img_ROI);
        }







        cJSON* jsub_regions = JFetch_ARRAY(def,"sub_regions");
        
        int jsub_regions_L=jsub_regions==NULL?0:cJSON_GetArraySize(jsub_regions);

        vector<Mat> resultMarkOverlay;
        vector<Mat> resultImage;
        vector<Mat> resultMarkRegion;
        vector<int> indexArr_w_priority;
        indexArr_w_priority.resize(jsub_regions_L);


        float xShift=0;
        float yShift=0;

        for(int i=0;i<1;i++)//2 iteration
        {//locating adjustment

          float _xShift=0;
          float _yShift=0;
          int xAdjCount=0;
          int yAdjCount=0;
          int h1=0;
          int h2=jsub_regions_L-1;
          for(int j=0;j<jsub_regions_L;j++)
          {

            cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,j);


            bool x_locating_mark =JFetch_TRUE(jsub_region,"x_locating_mark");
            bool y_locating_mark =JFetch_TRUE(jsub_region,"y_locating_mark");
            bool x_mirror_mark =JFetch_TRUE(jsub_region,"x_mirror_mark");
            bool y_mirror_mark =JFetch_TRUE(jsub_region,"y_mirror_mark");
            if(x_locating_mark==false && y_locating_mark==false)
            {
              indexArr_w_priority[h1++]=j;
              continue;
            }

            if(JFetch_STRING_ex(jsub_region,"type","HSVSeg")!="HSVSeg")
            {
              continue;
            }
            indexArr_w_priority[h2--]=j;


            int srW=(int)JFetch_NUMBER_ex(jsub_region,"region.w",-1);
            int srH=(int)JFetch_NUMBER_ex(jsub_region,"region.h",-1);

            int srX=(int)JFetch_NUMBER_ex(jsub_region,"region.x",-1)+xShift;
            int srY=(int)JFetch_NUMBER_ex(jsub_region,"region.y",-1)+yShift;
            // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
            XYWH_clipping(srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
            if(srW<=1 || srH<=1)
            {
              //invalid number
              continue;
            }

            Mat sub_region_ROI = _def_temp_img_ROI(Rect(srX, srY, srW, srH)).clone();
            if(show_display_overlay && JFetch_TRUE(jsub_region,"color_compensation_enable"))
            {
              cv::Scalar sum= cv::sum(sub_region_ROI);



              int pixCount=srW*srH;
              LOGI(">>>>>>>>>>>>");


                  
              {

                cv::Scalar igs_sum(0);
                int igs_PixCount=0;
                cJSON* ignore_regions = JFetch_ARRAY(jsub_region,"ignore_regions");
                int ignore_regions_L=ignore_regions==NULL?0:cJSON_GetArraySize(ignore_regions);
                for(int k=0;k<ignore_regions_L;k++)
                {
                  cJSON *ig_reg= cJSON_GetArrayItem(ignore_regions,k);
                  
                  int x=(int)JFetch_NUMBER_ex(ig_reg,"x");
                  int y=(int)JFetch_NUMBER_ex(ig_reg,"y");
                  int w=(int)JFetch_NUMBER_ex(ig_reg,"w");
                  int h=(int)JFetch_NUMBER_ex(ig_reg,"h");

                  XYWH_clipping(x,y,w,h, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);

                  auto igregion=_def_temp_img_ROI(Rect(x,y,w,h));

                  cv::Scalar ig_sum= cv::sum(igregion);

                  igs_sum+=ig_sum;
                  igs_PixCount+=w*h;


                }
                sum-=igs_sum;
                pixCount-=igs_PixCount;

              }



              cv::Scalar avgPix= sum/pixCount;



              int cct_r=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.r",-1);
              int cct_g=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.g",-1);
              int cct_b=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.b",-1);

              cv::Scalar refAvgPix=cv::Scalar(cct_b,cct_g,cct_r);
              int color_compensation_diff_thres=JFetch_NUMBER_ex(jsub_region,"color_compensation_diff_thres",-1);

              float color_compensation_diff= norm(refAvgPix,avgPix,NORM_L2);

              // LOGI(">>>>>>>>>>>>colorBalancingDiff:%f",colorBalancingDiff);
              cv::Scalar compScalar;//=refAvgPix/avgPix;
              compScalar[0]=refAvgPix[0]/avgPix[0];
              compScalar[1]=refAvgPix[1]/avgPix[1];
              compScalar[2]=refAvgPix[2]/avgPix[2];
              // def_temp_img_innerROI*=compScalar;
              multiply(sub_region_ROI,compScalar, sub_region_ROI);

            }


            {

            }



            double l_h=JFetch_NUMBER_ex(jsub_region,"rangel.h",0);
            double l_s=JFetch_NUMBER_ex(jsub_region,"rangel.s",0);
            double l_v=JFetch_NUMBER_ex(jsub_region,"rangel.v",0);

            double h_h=JFetch_NUMBER_ex(jsub_region,"rangeh.h",180);
            double h_s=JFetch_NUMBER_ex(jsub_region,"rangeh.s",255);
            double h_v=JFetch_NUMBER_ex(jsub_region,"rangeh.v",255);
            Mat img_HSV;
            cvtColor(sub_region_ROI, img_HSV, COLOR_BGR2HSV);
            // LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
            Scalar rangeH=Scalar(h_h,h_s,h_v);
            Scalar rangeL=Scalar(l_h,l_s,l_v);

            Mat img_HSV_threshold;
            inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);

            if(x_locating_mark)
            {
              Mat axisSum;
              cv::reduce(img_HSV_threshold, axisSum, 0, REDUCE_AVG, CV_8U);

              cv::GaussianBlur(axisSum, axisSum, cv::Size(5, 1), 5);
              // LOGE("X . axisSum:%d . %d",axisSum.size().width,axisSum.size().height);

              Mat axisSum2;
              double otsu_threshold = cv::threshold(axisSum, axisSum2, 0 /*ignored value*/, 255, cv::THRESH_OTSU);
              // LOGE("otsu_threshold:%f",otsu_threshold);
              int cL = findCrossLoc(axisSum,otsu_threshold,JFetch_TRUE(jsub_region,"x_locating_dir"));
              // LOGE("cL:%d",cL);

              _xShift+=axisSum.size().width/2-cL;
              xAdjCount++;
            }


            if(y_locating_mark)
            {
              Mat axisSum;
              cv::reduce(img_HSV_threshold, axisSum, 1, REDUCE_AVG, CV_8U);
              axisSum=axisSum.t();
              cv::GaussianBlur(axisSum, axisSum, cv::Size(5, 1), 5);
              int factor = img_HSV_threshold.size().width;
              // LOGE("Y . axisSum:%d . %d . factor:%d",axisSum.size().width,axisSum.size().height,factor);

              Mat axisSum2;
              double otsu_threshold = cv::threshold(axisSum, axisSum2, 0 /*ignored value*/, 255, cv::THRESH_OTSU);
              // LOGE("otsu_threshold:%f",otsu_threshold);
              int cL = findCrossLoc(axisSum,otsu_threshold,JFetch_TRUE(jsub_region,"y_locating_dir"));
              // LOGE("cL:%d",cL);

              _yShift+=axisSum.size().width/2-cL;
              yAdjCount++;
            }
          } 

          _xShift/=xAdjCount+0.00001;//just to make sure when xAdjCount==0 the compute would still be ok
          _yShift/=yAdjCount+0.00001;
          xShift+=_xShift;
          yShift+=_yShift;

        }

        if(xShift!=0 || yShift!=0)
        {
          Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2+X_offset+xShift,H/2+Y_offset+yShift},-angle,xFlip,yFlip);
          cv::warpAffine(CV_srcImg, _def_temp_img_ROI, rot,_def_temp_img_ROI.size());

          if(color_ch_mul_r!=1 || color_ch_mul_g!=1 || color_ch_mul_b!=1)
          {
            cv::Scalar compScalar(color_ch_mul_b,color_ch_mul_g,color_ch_mul_r);
            multiply(_def_temp_img_ROI,compScalar, _def_temp_img_ROI);
          }
        }


        {
          if(bilateral_d>1)
          {
            Mat filterRes=_def_temp_img_ROI.clone();
            // LOGE("bilateral d:%d . sC:%f sS:%f",d,sigmaColor,sigmaSpace);
            cv::bilateralFilter(filterRes,_def_temp_img_ROI,
              bilateral_d,bilateral_sigmaColor,bilateral_sigmaSpace);
          }
        }



        mri.subregions.resize(jsub_regions_L);

        resultMarkOverlay.resize(jsub_regions_L);
        resultMarkRegion.resize(jsub_regions_L);
        resultImage.resize(jsub_regions_L);
        for(int j=0;j<jsub_regions_L;j++)
        {
          int SUBR_category=STAGEINFO_CAT_UNSET;
          StageInfo_SurfaceCheckSimple::SubRegion_Info sri;
          int subregIdx=indexArr_w_priority[j];
          
          cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,subregIdx);


          sri.type=StageInfo_SurfaceCheckSimple::id_HSVSeg;

          string NG_Map_To=JFetch_STRING_ex(jsub_region,"NG_Map_To","NG");


          bool x_locating_mark =JFetch_TRUE(jsub_region,"x_locating_mark");
          bool y_locating_mark =JFetch_TRUE(jsub_region,"y_locating_mark");


          int srW=(int)JFetch_NUMBER_ex(jsub_region,"region.w",-1);
          int srH=(int)JFetch_NUMBER_ex(jsub_region,"region.h",-1);

          int srX=(int)JFetch_NUMBER_ex(jsub_region,"region.x",-1);
          int srY=(int)JFetch_NUMBER_ex(jsub_region,"region.y",-1);
          // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
          XYWH_clipping(srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
          if(srW<=1 || srH<=1)
          {
            //invalid number
          }



          // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);


          Mat sub_region_ROI_origin_img = _def_temp_img_ROI(Rect(srX, srY, srW, srH));
          resultMarkRegion[subregIdx]=sub_region_ROI_origin_img;
          Mat sub_region_ROI = sub_region_ROI_origin_img.clone();


          string subRegType=JFetch_STRING_ex(jsub_region,"type","HSVSeg");

          if(subRegType!="HSVSeg")
          {
            if(subRegType=="SigmaThres")
            {


              float colorSigma=JFetch_NUMBER_ex(jsub_region,"colorSigma");


              LOGE("SigmaThres is HERE....colorSigma:%f",colorSigma);



              vector<cv::Rect> igRegs;
              {

                cv::Scalar igs_sum(0);
                int igs_PixCount=0;
                cJSON* ignore_regions = JFetch_ARRAY(jsub_region,"ignore_regions");
                int ignore_regions_L=ignore_regions==NULL?0:cJSON_GetArraySize(ignore_regions);
                for(int k=0;k<ignore_regions_L;k++)
                {
                  cJSON *ig_reg= cJSON_GetArrayItem(ignore_regions,k);
                  
                  int x=(int)JFetch_NUMBER_ex(ig_reg,"x");
                  int y=(int)JFetch_NUMBER_ex(ig_reg,"y");
                  int w=(int)JFetch_NUMBER_ex(ig_reg,"w");
                  int h=(int)JFetch_NUMBER_ex(ig_reg,"h");

                  XYWH_clipping(x,y,w,h, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);

                  igRegs.push_back(cv::Rect(x,y,w,h));

                }
              }



              float comp_sigma0=NAN;
              Mat vectors=cv::Mat::zeros(cv::Size(), CV_8UC3);
          
              // Loop over rows and columns of the image
              for (int row = 0; row < sub_region_ROI.rows; ++row) {
                  for (int col = 0; col < sub_region_ROI.cols; ++col) {
                    bool isPixInRect;
                    for(int k=0;k<igRegs.size();k++){
                      isPixInRect=igRegs[k].contains(cv::Point(col,row));
                      if(isPixInRect)break;
                    }
                    if(isPixInRect)continue;

                    // Get pixel value at (row, col)
                    cv::Vec3b pixel = sub_region_ROI.at<cv::Vec3b>(row, col);

                    vectors.push_back(pixel);
                  }
              }
              vectors=vectors.reshape(1, vectors.rows * vectors.cols);

              // std::cout << "vectors size " << std::endl << vectors.size() << std::endl;
              // LOGE("????>>>>>> ");

              if(1)
              {//PCA

                vectors.convertTo(vectors, CV_32F);
                // Perform PCA analysis on the vectors
                PCA pca_analysis(vectors, Mat(), PCA::DATA_AS_ROW);

                // Print the eigenvectors and eigenvalues
                std::cout << "Eigenvectors: " << std::endl << pca_analysis.eigenvectors << std::endl;

                auto eigenvalues=pca_analysis.eigenvalues;
                std::cout << "Eigenvalues: " << std::endl << eigenvalues << std::endl;


                float sigma0 = sqrt(eigenvalues.at<float>(0,0));
                // float sigma1 = sqrt(eigenvalues.at<float>(1,0));
                // float sigma2 = sqrt(eigenvalues.at<float>(2,0));


                comp_sigma0=sigma0;

              }

              if(0)
              {
                Mat A = (Mat_<double>(3, 1) << 1, 2, 3); // Define vector A
                Mat B = (Mat_<double>(3, 1) << 10, 11, 12);

                Mat result = A.cross(B); // Calculate cross product of A with each vector in B

                std::cout << "Cross product result:\n" << result << std::endl;

              }




              
              if(JFetch_TRUE(jsub_region,"brightnessCompensation"))
              {
                Scalar avgPixel = mean(sub_region_ROI);
                float absAvg=norm(avgPixel);
                comp_sigma0/=absAvg;
                comp_sigma0*=128;

              }
              // std::cout << "Eigenvalues: " << std::endl << sigma0<< ",comp:"<<comp_sigma0 << std::endl;

              sri.type=StageInfo_SurfaceCheckSimple::id_SigmaThres;
              sri.score=comp_sigma0;
              if(comp_sigma0==comp_sigma0)
              {
                sri.category=(comp_sigma0>colorSigma)?STAGEINFO_CAT_NG:STAGEINFO_CAT_OK;
              }
              else
              {
                sri.category=STAGEINFO_CAT_NG;
              }



              if(sri.category==STAGEINFO_CAT_NG)
              {

                if(NG_Map_To=="NG2")
                {
                  sri.category=STAGEINFO_CAT_NG2;
                }
                else if(NG_Map_To=="NA")
                {
                  sri.category=STAGEINFO_CAT_NA;
                }
                else //if(NG_Map_To=="NG")
                {

                }

              }

              LOGE("NG_Map_To:%s",NG_Map_To.c_str());

              // MATCH_REGION_score+=area_sum;
              MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,sri.category);


              mri.subregions[subregIdx]=sri;

            }






            continue;
          }




          if(show_display_overlay && JFetch_TRUE(jsub_region,"color_compensation_enable"))
          {
            cv::Scalar sum= cv::sum(sub_region_ROI);
            int pixCount=srW*srH;
            // LOGI(">>>>>>>>>>>>");



            {

              cv::Scalar igs_sum(0);
              int igs_PixCount=0;
              cJSON* ignore_regions = JFetch_ARRAY(jsub_region,"ignore_regions");
              int ignore_regions_L=ignore_regions==NULL?0:cJSON_GetArraySize(ignore_regions);
              for(int k=0;k<ignore_regions_L;k++)
              {
                cJSON *ig_reg= cJSON_GetArrayItem(ignore_regions,k);
                
                int x=(int)JFetch_NUMBER_ex(ig_reg,"x");
                int y=(int)JFetch_NUMBER_ex(ig_reg,"y");
                int w=(int)JFetch_NUMBER_ex(ig_reg,"w");
                int h=(int)JFetch_NUMBER_ex(ig_reg,"h");

                XYWH_clipping(x,y,w,h, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);

                auto igregion=_def_temp_img_ROI(Rect(x,y,w,h));

                cv::Scalar ig_sum= cv::sum(igregion);

                igs_sum+=ig_sum;
                igs_PixCount+=w*h;


              }
              // LOGE("sum:%f %f %f . count=%d",sum[0],sum[1],sum[2],pixCount);
              // LOGE("igs:%f %f %f . count=%d",igs_sum[0],igs_sum[1],igs_sum[2],igs_PixCount);
              sum-=igs_sum;
              pixCount-=igs_PixCount;

            }




            cv::Scalar avgPix= sum/pixCount;
            // LOGE("avgPix:%f %f %f",avgPix[0],avgPix[1],avgPix[2]);

            int cct_r=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.r",-1);
            int cct_g=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.g",-1);
            int cct_b=JFetch_NUMBER_ex(jsub_region,"color_compensation_target.b",-1);

            cv::Scalar refAvgPix=cv::Scalar(cct_b,cct_g,cct_r);
            int color_compensation_diff_thres=JFetch_NUMBER_ex(jsub_region,"color_compensation_diff_thres",-1);

            float color_compensation_diff= norm(refAvgPix,avgPix,NORM_L2);


            if(color_compensation_diff>color_compensation_diff_thres)
            {
              SUBR_category=STAGEINFO_CAT_SCS_COLOR_CORRECTION_THRES_LIMIT;
              sri.category=SUBR_category;
              sri.score=color_compensation_diff;
              mri.subregions[subregIdx]=sri;


              if(NG_Map_To=="NG2")
              {
                SUBR_category=STAGEINFO_CAT_NG2;
              }
              else if(NG_Map_To=="NA")
              {
                SUBR_category=STAGEINFO_CAT_NA;
              }
              else //if(NG_Map_To=="NG")
              {

              }

              MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,
              JFetch_TRUE(jsub_region,"color_compensation_diff_NG_as_NA")?
              STAGEINFO_CAT_NA:STAGEINFO_CAT_NG);



              continue;
            }

            // LOGI(">>>>>>>>>>>>colorBalancingDiff:%f",colorBalancingDiff);
            cv::Scalar compScalar;//=refAvgPix/avgPix;
            compScalar[0]=refAvgPix[0]/avgPix[0];
            compScalar[1]=refAvgPix[1]/avgPix[1];
            compScalar[2]=refAvgPix[2]/avgPix[2];
            // def_temp_img_innerROI*=compScalar;
            multiply(sub_region_ROI,compScalar, sub_region_ROI);



          }



          float sharpening_blurRad = JFetch_NUMBER_ex(jsub_region,"sharpening_blurRad",10);
          float sharpening_alpha = JFetch_NUMBER_ex(jsub_region,"sharpening_alpha",0);
          float sharpening_beta = JFetch_NUMBER_ex(jsub_region,"sharpening_beta",1+sharpening_alpha);
          float sharpening_gamma = JFetch_NUMBER_ex(jsub_region,"sharpening_gamma",0);
          if(sharpening_blurRad>1 && sharpening_alpha>0){

            Mat image(sub_region_ROI.rows,sub_region_ROI.cols,CV_8UC3);
            // cv::GaussianBlur(sub_region_ROI, image, cv::Size(0, 0), sharpening_blurRad);
            cv::blur(sub_region_ROI,image,cv::Size(sharpening_blurRad,sharpening_blurRad));
            cv::addWeighted(sub_region_ROI, 1+sharpening_alpha, image, -sharpening_alpha, sharpening_gamma, sub_region_ROI);
          }




          // {

          //   int blurRadius=(int)JFetch_NUMBER_ex(jsub_region,"blurRadius",default_blurRadius);
          //   if(blurRadius>0)
          //     cv::GaussianBlur(sub_region_ROI, sub_region_ROI, cv::Size(blurRadius, blurRadius), blurRadius);

          // }

          float line_length_thres=JFetch_NUMBER_ex(jsub_region,"line_length_thres",99999);
          float point_area_thres = JFetch_NUMBER_ex(jsub_region,"point_area_thres",99999);

          float point_total_area_thres=JFetch_NUMBER_ex(jsub_region,"point_total_area_thres",1000000000);
          float line_total_length_thres=JFetch_NUMBER_ex(jsub_region,"line_total_length_thres",1000000000);


          float area_thres = JFetch_NUMBER_ex(jsub_region,"area_thres",99999);
          resultImage[subregIdx]=sub_region_ROI;
          Mat img_HSV;
          cvtColor(sub_region_ROI, img_HSV, COLOR_BGR2HSV);

          // imwrite("data/ZZA/"+id+"_"+std::to_string(j)+".jpg",img_HSV); 
          double l_h=JFetch_NUMBER_ex(jsub_region,"rangel.h",0);
          double l_s=JFetch_NUMBER_ex(jsub_region,"rangel.s",0);
          double l_v=JFetch_NUMBER_ex(jsub_region,"rangel.v",0);

          double h_h=JFetch_NUMBER_ex(jsub_region,"rangeh.h",180);
          double h_s=JFetch_NUMBER_ex(jsub_region,"rangeh.s",255);
          double h_v=JFetch_NUMBER_ex(jsub_region,"rangeh.v",255);
          // LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
          Scalar rangeH=Scalar(h_h,h_s,h_v);
          Scalar rangeL=Scalar(l_h,l_s,l_v);

          Mat img_HSV_range;
          Mat img_HSV_threshold;
          // inRangeV2(img_HSV, rangeL, rangeH,{0,0,1},100,img_HSV_range);

          inRange(img_HSV, rangeL, rangeH, img_HSV_range);

          // imwrite("data/ZZA/"+id+"_range_"+std::to_string(j)+".jpg",img_HSV_range); 
          // cvtColor(img_HSV_threshold,def_temp_img_innerROI,COLOR_GRAY2RGB);

          // if(blackRegions)
          // {

          //   int regionCount=cJSON_GetArraySize(blackRegions);
          //   for(int i=0;i<regionCount;i++)
          //   {
              
          //     cJSON *region= cJSON_GetArrayItem(blackRegions,i);
              
          //     int x=(int)JFetch_NUMBER_ex(region,"x");
          //     int y=(int)JFetch_NUMBER_ex(region,"y");
          //     int w=(int)JFetch_NUMBER_ex(region,"w");
          //     int h=(int)JFetch_NUMBER_ex(region,"h");


          //     XYWH_clipping(x,y,w,h, 0,0,img_HSV_threshold.cols,img_HSV_threshold.rows);
          //     if(srW>1 && srH>1)
          //     {
          //       img_HSV_threshold(Rect(x,y,w,h)) = 255;
          //     }
          //   }
          // }

          // {
          //   if(colorThres>0)
          //   {
          //     GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
          //     threshold(img_HSV_threshold, img_HSV_threshold, colorThres, 255, THRESH_BINARY);

          //     GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
          //     threshold(img_HSV_threshold, img_HSV_threshold, 255-colorThres, 255, THRESH_BINARY);

          //   }
          // }

          double detect_detail=JFetch_NUMBER_ex(jsub_region,"detect_detail",100);
          cv::blur(img_HSV_range,img_HSV_range,cv::Size(5,5));

          threshold(img_HSV_range, img_HSV_threshold, detect_detail, 255, THRESH_BINARY);


          if(JFetch_TRUE(jsub_region,"invert_detection")==false)
            bitwise_not(img_HSV_threshold , img_HSV_threshold);//need bit not by default, so the "invert" will be NOT to bit not



          // if(detect_detail!=128)
          // {

          //   cv::blur(img_HSV_threshold,img_HSV_threshold,cv::Size(5,5));
          //   threshold(img_HSV_threshold, img_HSV_threshold, detect_detail, 255, THRESH_BINARY);

          // }


          {
            cJSON* ignore_regions = JFetch_ARRAY(jsub_region,"ignore_regions");
            int ignore_regions_L=ignore_regions==NULL?0:cJSON_GetArraySize(ignore_regions);
            for(int k=0;k<ignore_regions_L;k++)
            {
              cJSON *ig_reg= cJSON_GetArrayItem(ignore_regions,k);
              
              int x=(int)JFetch_NUMBER_ex(ig_reg,"x");
              int y=(int)JFetch_NUMBER_ex(ig_reg,"y");
              int w=(int)JFetch_NUMBER_ex(ig_reg,"w");
              int h=(int)JFetch_NUMBER_ex(ig_reg,"h");

              XYWH_clipping(x,y,w,h, 0,0,img_HSV_threshold.cols,img_HSV_threshold.rows);
              img_HSV_threshold(Rect(x,y,w,h)) = 0;


            }

          }



          resultMarkOverlay[subregIdx]=img_HSV_threshold.clone();
          if(show_display_overlay)
          {
            float resultOverlayAlpha = JFetch_NUMBER_ex(jsub_region,"resultOverlayAlpha",0);

            if(resultOverlayAlpha>0)
            {
              // cv::cvtColor(img_HSV_threshold,def_temp_img_innerROI,COLOR_GRAY2RGB);
              Mat img_HSV_threshold_rgb;
              cv::cvtColor(img_HSV_threshold,img_HSV_threshold_rgb,COLOR_GRAY2RGB);

              if(x_locating_mark||y_locating_mark)
                multiply(img_HSV_threshold_rgb,cv::Scalar(0,1,0), img_HSV_threshold_rgb);
              else
                multiply(img_HSV_threshold_rgb,cv::Scalar(0,0,1), img_HSV_threshold_rgb);


              addWeighted( 
              img_HSV_threshold_rgb, resultOverlayAlpha, 
              sub_region_ROI, 1, 0, 
              sub_region_ROI);


              // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
              // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
            }
          }

          {

            vector<vector<Point> > contours;
            vector<Vec4i> hierarchy;




            findContours( img_HSV_threshold, contours, hierarchy, RETR_LIST, CHAIN_APPROX_SIMPLE );
            int area_sum=0;
            int element_max_area=0;
            int element_total_area=0;
            int line_max_length=0;
            int line_total_length=0;

            bool isNG=false;

            // if(colorBalancingDiff>colorBalancingDiffThres)
            // {elementCount

            //   StageInfo_SurfaceCheckSimple::Ele_info einfo={0};
            //   einfo.category=STAGEINFO_CAT_SCS_COLOR_CORRECTION_THRES_LIMIT;
            //   einfo.area=colorBalancingDiff;
            //   si.elements.push_back(einfo);
            //   isNG=true;
            // }

            int elementCount=0;

            for(int k=0;k<contours.size();k++)
            {
              int area = contourArea(contours[k],false);
              int a_area=area+contours[k].size();
              area_sum+=a_area;



              StageInfo_SurfaceCheckSimple::Ele_info einfo;
              einfo.category=STAGEINFO_CAT_UNSET;
              bool isALine=false;


              {
                RotatedRect rrect;
                bool isEllipse=false;
                try
                {
                  if(contours[k].size()>6)
                    rrect=fitEllipse(contours[k]);
                }
                catch(const cv::Exception& e)
                {
                }

                if(isEllipse==false)
                {
                  rrect=minAreaRect(contours[k]);
                  // rrect.center.x-=rrect.size.width/2;
                  // rrect.center.y-=rrect.size.height/2;
                }
                
                int rrecth=rrect.size.height;
                int rrectw=rrect.size.width;
                int longestSide=rrecth>rrectw?rrecth:rrectw;
                int shortestSide=rrecth<rrectw?rrecth:rrectw;
                if((float)longestSide/shortestSide>2.5){//consider as a line
                  line_total_length+=longestSide;
                  if(line_max_length<longestSide)
                  {
                    line_max_length=longestSide;
                  }
                  isALine=true;




                  if(line_length_thres<longestSide)
                  {
                    einfo.category=STAGEINFO_CAT_SCS_LINE_OVER_LEN;


                    einfo.data.line.perimeter=contours.size();
                    einfo.data.line.x=rrect.center.x+srX;
                    einfo.data.line.y=rrect.center.y+srY;


                    einfo.data.line.h=rrecth;
                    einfo.data.line.w=rrectw;

                    einfo.data.line.area=a_area;
                    einfo.data.line.angle=rrect.angle*M_PI/180;

                    isNG=true;
                  }
                  else
                  {
                  }
                }


                if(point_area_thres<a_area)
                {//oversized element
                  element_total_area+=a_area;
                  elementCount++;

                  if(einfo.category==STAGEINFO_CAT_UNSET)
                  {
                    einfo.category=STAGEINFO_CAT_SCS_PT_OVER_SIZE;


                    einfo.data.point.perimeter=contours.size();

                    einfo.data.point.x=rrect.center.x+srX;
                    einfo.data.point.y=rrect.center.y+srY;


                    einfo.data.point.area=a_area;

                    einfo.data.point.h=rrect.size.height;
                    einfo.data.point.w=rrect.size.width;
                    einfo.data.point.angle=rrect.angle*M_PI/180;



                  }
                  // isNG=true;
                }
                else
                {
                }





              }




              if(einfo.category==STAGEINFO_CAT_UNSET)
              {
                einfo.category=STAGEINFO_CAT_OK;
              }

              if(einfo.category==STAGEINFO_CAT_OK)
              {
                continue;
              }

              sri.elements.push_back(einfo);
            }



            int element_count_thres = JFetch_NUMBER_ex(jsub_region,"element_count_thres",0);

            if(element_count_thres<elementCount)
            {
              isNG=true;

              
            }


            int element_area_thres = JFetch_NUMBER_ex(jsub_region,"element_area_thres",-1);
            if(element_area_thres>0 && element_area_thres<element_total_area)
            {
              isNG=true;
              
            }


            if(area_sum>area_thres)
            {
              isNG=true;
              
            }



            // LOGI(">>>>>>>>> area_sum:%d",area_sum);


            if(isNG ||
            element_total_area>point_total_area_thres ||
            line_total_length>line_total_length_thres)
                                  SUBR_category=STAGEINFO_CAT_NG;
            else                   SUBR_category=STAGEINFO_CAT_OK;

            if(SUBR_category==STAGEINFO_CAT_NG)
            {

              if(NG_Map_To=="NG2")
              {
                SUBR_category=STAGEINFO_CAT_NG2;
              }
              else if(NG_Map_To=="NA")
              {
                SUBR_category=STAGEINFO_CAT_NA;
              }
              else //if(NG_Map_To=="NG")
              {

              }

            }

            LOGE("NG_Map_To:%s",NG_Map_To.c_str());

            MATCH_REGION_score+=area_sum;
            MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,SUBR_category);

            sri.category=SUBR_category;
            sri.score=area_sum; 


            {
              sri.hsvseg_stat.blob_area=area_sum;
              sri.hsvseg_stat.element_area=element_total_area;
              sri.hsvseg_stat.element_count=elementCount;
              sri.hsvseg_stat.max_line_length=line_max_length;
              
            }





            mri.subregions[subregIdx]=sri;



          }
        }

        if(show_display_overlay)//draw the overlay on original images(the ROI of original image)
        {


        for(int j=0;j<jsub_regions_L;j++)
        {

          cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,j);
          if(JFetch_TRUE(jsub_region,"show_processed_image"))
          {
            resultImage[j].copyTo(resultMarkRegion[j]);
          }

         
        }
        for(int j=0;j<jsub_regions_L;j++)
        {
         
          if(resultMarkOverlay[j].empty()==true)continue;
          cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,j);
          float resultOverlayAlpha = JFetch_NUMBER_ex(jsub_region,"resultOverlayAlpha",0);
          
          if(resultOverlayAlpha<=0)continue;

          float overlay_r = JFetch_NUMBER_ex(jsub_region,"overlayColor.r",255);
          float overlay_g = JFetch_NUMBER_ex(jsub_region,"overlayColor.g",0);
          float overlay_b = JFetch_NUMBER_ex(jsub_region,"overlayColor.b",0);

          {
            // cv::cvtColor(img_HSV_threshold,def_temp_img_innerROI,COLOR_GRAY2RGB);
            Mat img_HSV_threshold_rgb;
            cv::cvtColor(resultMarkOverlay[j],img_HSV_threshold_rgb,COLOR_GRAY2RGB);

            // if(x_locating_mark||y_locating_mark)
            //   multiply(img_HSV_threshold_rgb,cv::Scalar(0,1,0), img_HSV_threshold_rgb);
            // else
            multiply(img_HSV_threshold_rgb,cv::Scalar(overlay_b/255,overlay_g/255,overlay_r/255), img_HSV_threshold_rgb);


            addWeighted( 
            img_HSV_threshold_rgb, resultOverlayAlpha, 
            resultMarkRegion[j], 1, 0, 
            resultMarkRegion[j]);


            // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
            // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
          }
        }
      
        }
      
    
      }while (0);
      mri.category=MATCH_REGION_category;
      mri.score=MATCH_REGION_score;
      reportInfo->match_reg_info.push_back(mri);
    }


  }
  int category=STAGEINFO_CAT_UNSET;
  for(auto catInfo:reportInfo->match_reg_info)
  {
    category=STAGEINFO_SCS_CAT_BASIC_reducer(category,catInfo.category);
  }

  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->img_show=shared_ptr<acvImage>(retImage);
  reportInfo->img=srcImg;
  
  reportInfo->trigger_id=sinfo->trigger_id;
  reportInfo->sharedInfo.push_back(sinfo);
  reportInfo->trigger_tags.push_back(id);

  insertInputTagsWPrefix(reportInfo->trigger_tags,sinfo->trigger_tags,"s_");

  reportInfo->category=category;


  reportInfo->pixel_size=sinfo->img_prop.fi.pixel_size_mm;

  reportInfo->img_prop=sinfo->img_prop;
  reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",10);
  LOGI("CHID:%d category:%d . id:%s",reportInfo->img_prop.StreamInfo.channel_id,category,id.c_str());

  // reportInfo->jInfo=NULL;
  // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

  {
    int64 t1 = cv::getTickCount();
    double secs_us = 1000000*(t1-t0)/cv::getTickFrequency();
    reportInfo->process_time_us=secs_us;
    reportInfo->create_time_sysTick=t1;
    // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

    LOGI(">>>>>>>>process_time_us:%f",secs_us);
  }

  reportInfo->genJsonRepTojInfo();
  belongMan->dispatch(reportInfo);
  result_cache_stage_info=reportInfo;
  cache_stage_info=sinfo;
}




InspectionTarget_SurfaceCheckSimple::~InspectionTarget_SurfaceCheckSimple()
{
  if(extParam)
  {
    cJSON_Delete(extParam);
    extParam=NULL;
  }
}
