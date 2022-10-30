
#include "InspTar_SurfaceCheckSimple.hpp"
#include "StageInfo_Orientation.hpp"
#include "StageInfo.hpp"

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
    if( matchTriggerTag(tag))
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


static Mat getRotTranMat(acv_XY pt1,acv_XY pt2,float theta)
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
  int64 t0 = cv::getTickCount();
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Orientation *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type is not match.....");
    return;
  }
  auto srcImg=d_sinfo->img;


  Mat CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);



  LOGI("orientation info size:%d",d_sinfo->orientation.size());


  // if()
  
  // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);

  // cJSON *report=cJSON_CreateObject();
  // cJSON* rep_regionInfo=cJSON_CreateArray();
  
  // cJSON_AddStringToObject(report,"id",id.c_str());
  // cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);



  float X_offset=JFetch_NUMBER_ex(def,"X_offset",0);
  float Y_offset=JFetch_NUMBER_ex(def,"Y_offset",0);

  float W=JFetch_NUMBER_ex(def,"W");
  float H=JFetch_NUMBER_ex(def,"H");

  acvImage *retImage=NULL;

  shared_ptr<StageInfo_SurfaceCheckSimple> reportInfo(new StageInfo_SurfaceCheckSimple());
  
  double resultOverlayAlpha=JFetch_NUMBER_ex(def,"resultOverlayAlpha",0);


  double l_h=JFetch_NUMBER_ex(def,"hsv.rangel.h",0);
  double l_s=JFetch_NUMBER_ex(def,"hsv.rangel.s",0);
  double l_v=JFetch_NUMBER_ex(def,"hsv.rangel.v",0);

  double h_h=JFetch_NUMBER_ex(def,"hsv.rangeh.h",180);
  double h_s=JFetch_NUMBER_ex(def,"hsv.rangeh.s",255);
  double h_v=JFetch_NUMBER_ex(def,"hsv.rangeh.v",255);
  double colorThres=JFetch_NUMBER_ex(def,"colorThres",0);
  bool img_order_reverse=JFetch_TRUE(def,"img_order_reverse");
  
  int area_thres=JFetch_NUMBER_ex(def,"area_thres",100);

  int point_area_thres=JFetch_NUMBER_ex(def,"point_area_thres",1000000000);
  int point_total_area_thres=JFetch_NUMBER_ex(def,"point_total_area_thres",1000000000);
  int line_length_thres=JFetch_NUMBER_ex(def,"line_length_thres",1000000000);
  int line_total_length_thres=JFetch_NUMBER_ex(def,"line_total_length_thres",1000000000);

  float angle_offset=JFetch_NUMBER_ex(def,"angle_offset",0)*M_PI/180;
  if(d_sinfo->orientation.size()>0)
  {  
    retImage=new acvImage(W*d_sinfo->orientation.size(),H,3);
    Mat def_temp_img(retImage->GetHeight(),retImage->GetWidth(),CV_8UC3,retImage->CVector[0]);
    def_temp_img={0};

    for(int i=0;i<d_sinfo->orientation.size();i++)
    {
      int SUB_category=STAGEINFO_CAT_NA;
      int cur_score=-1;
      StageInfo_Orientation::orient orientation = d_sinfo->orientation[i];

      StageInfo_SurfaceCheckSimple::SRegion_Info si;
      do{
      if(orientation.confidence<=0)
      {
        SUB_category=STAGEINFO_CAT_NOT_EXIST;
        break;
      }
      // cJSON* idxRegion=JFetch_ARRAY(def,("[+"+std::to_string(i)+"+]").c_str());
      int imgOrderIdx=img_order_reverse?(d_sinfo->orientation.size()-1-i):i;
      Mat def_temp_img_ROI = def_temp_img(Rect(imgOrderIdx*W, 0, W, H));

      float angle = orientation.angle;
      // if(angle>M_PI_2)angle-=M_PI;
      // if(angle<-M_PI_2)angle+=M_PI;
      angle+=angle_offset;
      Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2+X_offset,H/2+Y_offset},-angle);

      cv::warpAffine(CV_srcImg, def_temp_img_ROI, rot,def_temp_img_ROI.size());




      if(1){

        Mat image(def_temp_img_ROI.rows,def_temp_img_ROI.cols,CV_8UC3);
        cv::GaussianBlur(def_temp_img_ROI, image, cv::Size(0, 0), 3);
        float alpha=0.3;
        cv::addWeighted(def_temp_img_ROI, 1+alpha, image, -alpha, 0, def_temp_img_ROI);
      }



      if(1){
        
        Mat img_HSV;
        cvtColor(def_temp_img_ROI, img_HSV, COLOR_BGR2HSV);


        LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
        Scalar rangeH=Scalar(h_h,h_s,h_v);
        Scalar rangeL=Scalar(l_h,l_s,l_v);

        Mat img_HSV_threshold;
        inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);
        // cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);


        {
          if(colorThres>0)
          {
            GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
            threshold(img_HSV_threshold, img_HSV_threshold, colorThres, 255, THRESH_BINARY);

            GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 5, 5), 0, 0 );
            threshold(img_HSV_threshold, img_HSV_threshold, 255-colorThres, 255, THRESH_BINARY);

          }
        }
        
            
        {
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

        {

          vector<vector<Point> > contours;
          vector<Vec4i> hierarchy;
          bitwise_not(img_HSV_threshold , img_HSV_threshold);
          findContours( img_HSV_threshold, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE );
          int area_sum=0;




          int point_max_area=0;
          int point_total_area=0;
          int line_max_length=0;
          int line_total_length=0;

          bool isNG=false;
          for(int k=0;k<contours.size();k++)
          {
            int area = contourArea(contours[k],false);
            int a_area=area+contours[k].size();
            area_sum+=a_area;



            StageInfo_SurfaceCheckSimple::Ele_info einfo;
            einfo.category=STAGEINFO_CAT_NA;
            einfo.perimeter=contours.size();
            einfo.angle=NAN;

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
              
              einfo.x=rrect.center.x+imgOrderIdx*W;
              einfo.y=rrect.center.y;


              einfo.h=rrect.size.height;
              einfo.w=rrect.size.width;

              int longestSide=einfo.h>einfo.w?einfo.h:einfo.w;
              int shortestSide=einfo.h<einfo.w?einfo.h:einfo.w;
              einfo.angle=rrect.angle*M_PI/180;

              if(line_length_thres<longestSide)
              {
                einfo.category=STAGEINFO_CAT_SCS_LINE_OVER_LEN;
                isNG=true;
              }
              else
              {
              }

              if((float)longestSide/shortestSide>2.5){//consider as a line
                line_total_length+=longestSide;
                if(line_max_length<longestSide)
                {
                  line_max_length=longestSide;
                }
                isALine=true;
              }

            }


            if(isALine==false)
            {
              point_total_area+=a_area;
              if(point_max_area<a_area)
              {
                point_max_area=a_area;
              }

              if(point_area_thres<a_area)
              {
                einfo.category=STAGEINFO_CAT_SCS_PT_OVER_SIZE;
                isNG=true;
              }
              else
              {
              }
            }

            if(einfo.category==STAGEINFO_CAT_NA)
            {
              einfo.category=STAGEINFO_CAT_OK;
            }

            // LOGI(">>[%d]>siz:%d area:%f",k,contours[k].size(),area);
            einfo.area=a_area;
            si.elements.push_back(einfo);
          }
          // LOGI(">>>>>>>>> area_sum:%d",area_sum);


          if(isNG ||
          area_sum>area_thres ||
          // point_max_area>point_area_thres ||
          // line_max_length>line_length_thres ||
          point_total_area>point_total_area_thres ||
          line_total_length>line_total_length_thres)
                                 SUB_category=STAGEINFO_CAT_NG;
          else                   SUB_category=STAGEINFO_CAT_OK;
          cur_score=area_sum;

        }
      }
    
    
    
    
      }while (0);
      si.category=SUB_category;
      si.score=cur_score;
      reportInfo->sreg_info.push_back(si);
    }


  }
  int category=0;
  for(auto catInfo:reportInfo->sreg_info)
  {
    if(category>catInfo.category || category==0)
    {
      category=catInfo.category;
    }
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

  reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",1);
  LOGI("CHID:%d category:%d",reportInfo->img_prop.StreamInfo.channel_id,category);

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

  cache_stage_info=sinfo;
}

InspectionTarget_SurfaceCheckSimple::~InspectionTarget_SurfaceCheckSimple()
{
}
