
#include "InspTar_SurfaceCheckSimple.hpp"
#include "StageInfo_Orientation.hpp"

using namespace cv;

using namespace std;

template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_SurfaceCheckSimple::InspectionTarget_SurfaceCheckSimple(string id,cJSON* def,InspectionTargetManager* belongMan)
  :InspectionTarget(id,def,belongMan)
{
  type=InspectionTarget_SurfaceCheckSimple::TYPE();
  LOGI("sodkoskoskad;aks;dkas;dk");
}

bool InspectionTarget_SurfaceCheckSimple::stageInfoFilter(shared_ptr<StageInfo> sinfo)
{
  // if(sinfo->typeName())



  for(auto tag : sinfo->trigger_tags )
  {
    if(tag=="_STREAM_")
    {
      return false;
    }
    if(tag==id)return true;
    if( matchTriggerTag(tag,def))
      return true;
  }
  return false;
}

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



bool InspectionTarget_SurfaceCheckSimple::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");

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


Mat getRotTranMat(acv_XY pt1,acv_XY pt2,float theta)
{
  float s=sin(theta);
  float c=cos(theta);

  Point2f srcPoints[3];//原圖中的三點 ,一個包含三維點（x，y）的數組，其中x、y是浮點型數
	Point2f dstPoints[3];//目標圖中的三點  

    //第一種仿射變換的調用方式：三點法
	//三個點對的值,上面也說了，只要知道你想要變換後圖的三個點的座標，就可以實現仿射變換  
	srcPoints[0] = Point2f(pt1.X, pt1.Y);
	srcPoints[1] = Point2f(pt1.X+s, pt1.Y+c);
	srcPoints[2] = Point2f(pt1.X+c, pt1.Y-s);
	//映射後的三個座標值
	dstPoints[0] = Point2f(pt2.X, pt2.Y);
	dstPoints[1] = Point2f(pt2.X, pt2.Y+1);
	dstPoints[2] = Point2f(pt2.X+1, pt2.Y);

	return getAffineTransform(srcPoints, dstPoints);//由三個點對計算變換矩陣  
								
}

// def getRotTranMat(pt1,pt2,Theta):
//   s=math.sin(-Theta)
//   c=math.cos(-Theta)

//   pts1 = np.float32([pt1,[pt1[0]+s,pt1[1]+c],[pt1[0]+c,pt1[1]-s]])
//   pts2 = np.float32([pt2,[pt2[0],pt2[1]+1],[pt2[0]+1,pt2[1]]])
//   return cv2.getAffineTransform(pts1,pts2)

void InspectionTarget_SurfaceCheckSimple::singleProcess(shared_ptr<StageInfo> sinfo)
{
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Orientation *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type is not match.....");
    return;
  }
  auto srcImg=d_sinfo->imgSets["src"];


  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);



  LOGI("orientation info size:%d",d_sinfo->orientation.size());


  // if()
  
  // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);

  // cJSON *report=cJSON_CreateObject();
  // cJSON* rep_regionInfo=cJSON_CreateArray();
  
  // cJSON_AddStringToObject(report,"id",id.c_str());
  // cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);




  float W=JFetch_NUMBER_ex(def,"W");
  float H=JFetch_NUMBER_ex(def,"H");

  acvImage *retImage=NULL;

  if(d_sinfo->orientation.size()>0)
  {  
    retImage=new acvImage(W*d_sinfo->orientation.size(),H,3);
    Mat def_temp_img(retImage->GetHeight(),retImage->GetWidth(),CV_8UC3,retImage->CVector[0]);
    def_temp_img={0};
    for(int i=0;i<d_sinfo->orientation.size();i++)
    {
      // cJSON* idxRegion=JFetch_ARRAY(def,("[+"+std::to_string(i)+"+]").c_str());
      Mat def_temp_img_ROI = def_temp_img(Rect(i*W, 0, W, H));
      StageInfo_Orientation::orient orientation = d_sinfo->orientation[i];

      float angle = orientation.angle;
      if(angle!=angle)
      {
        continue;
      }
      if(angle>M_PI_2)angle-=M_PI;
      if(angle<-M_PI_2)angle+=M_PI;
      Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2,H/2},-angle);

      cv::warpAffine(CV_srcImg, def_temp_img_ROI, rot,def_temp_img_ROI.size());


      if(1){
        
        Mat img_HSV;
        cvtColor(def_temp_img_ROI, img_HSV, COLOR_BGR2HSV);


        double l_h=JFetch_NUMBER_ex(def,"hsv.rangel.h",0);
        double l_s=JFetch_NUMBER_ex(def,"hsv.rangel.s",0);
        double l_v=JFetch_NUMBER_ex(def,"hsv.rangel.v",0);

        double h_h=JFetch_NUMBER_ex(def,"hsv.rangeh.h",180);
        double h_s=JFetch_NUMBER_ex(def,"hsv.rangeh.s",255);
        double h_v=JFetch_NUMBER_ex(def,"hsv.rangeh.v",255);

        LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
        Scalar rangeH=Scalar(h_h,h_s,h_v);
        Scalar rangeL=Scalar(l_h,l_s,l_v);

        Mat img_HSV_threshold;
        inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);
        // cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);


        {
          double colorThres=JFetch_NUMBER_ex(def,"colorThres",0);
          if(colorThres>0)
          {
            GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
            threshold(img_HSV_threshold, img_HSV_threshold, colorThres, 255, THRESH_BINARY);

            GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
            threshold(img_HSV_threshold, img_HSV_threshold, 255-colorThres, 255, THRESH_BINARY);

          }
        }
        
            
        {
          double resultOverlayAlpha=JFetch_NUMBER_ex(def,"resultOverlayAlpha",0);
          {
            // cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);
            Mat img_HSV_threshold_rgb;
            cv::cvtColor(img_HSV_threshold,img_HSV_threshold_rgb,COLOR_GRAY2RGB);
            addWeighted( 
            img_HSV_threshold_rgb, resultOverlayAlpha, 
            def_temp_img_ROI, 1-resultOverlayAlpha, 0.0, 
            def_temp_img_ROI);
            // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
            // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
          }
        }
      }
    }


  }




  


  
  shared_ptr<StageInfo_SurfaceCheckSimple> reportInfo(new StageInfo_SurfaceCheckSimple());




  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->imgSets["img"]=shared_ptr<acvImage>(retImage);
  
  reportInfo->trigger_id=sinfo->trigger_id;
  // reportInfo->trigger_tags.push_back("InfoStream2UI");
  // reportInfo->trigger_tags.push_back("ToTestRule");
  reportInfo->trigger_tags.push_back("ImTran");

  
  reportInfo->trigger_tags.push_back(id);

  

  reportInfo->StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",2);
  LOGI("CHID:%d",reportInfo->StreamInfo.channel_id);

  reportInfo->jInfo=NULL;

  // attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

  belongMan->dispatch(reportInfo);

}

InspectionTarget_SurfaceCheckSimple::~InspectionTarget_SurfaceCheckSimple()
{
}
