
#include "InspTar_Orientation.hpp"


using namespace cv;

using namespace std;


template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_Orientation_ColorRegionOval::InspectionTarget_Orientation_ColorRegionOval(string id,cJSON* def,InspectionTargetManager* belongMan)
  :InspectionTarget(id,def,belongMan)
{
  type=InspectionTarget_Orientation_ColorRegionOval::TYPE();
}

bool InspectionTarget_Orientation_ColorRegionOval::stageInfoFilter(shared_ptr<StageInfo> sinfo)
{
  // if(sinfo->typeName())



  for(auto tag : sinfo->trigger_tags )
  {
    // if(tag=="_STREAM_")
    // {
    //   return false;
    // }
    if( matchTriggerTag(tag,def))
      return true;
  }
  return false;
}

future<int> InspectionTarget_Orientation_ColorRegionOval::futureInputStagePool()
{
  return async(launch::async,&InspectionTarget_Orientation_ColorRegionOval::processInputStagePool,this);
}

int InspectionTarget_Orientation_ColorRegionOval::processInputPool()
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

bool InspectionTarget_Orientation_ColorRegionOval::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");

  return false;
}


cJSON* InspectionTarget_Orientation_ColorRegionOval::genITIOInfo()
{


  
  cJSON* arr= cJSON_CreateArray();

  {
    cJSON* opt= cJSON_CreateObject();
    cJSON_AddItemToArray(arr,opt);

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "i",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Image::stypeName().c_str() ));
    }

    {
      cJSON* sarr= cJSON_CreateArray();
      
      cJSON_AddItemToObject(opt, "o",sarr );
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Orientation::stypeName().c_str() ));
    }




  }

  return arr;

}

void InspectionTarget_Orientation_ColorRegionOval::singleProcess(shared_ptr<StageInfo> sinfo)
{

  LOGI("RUN:%s   from:%s",id.c_str(),sinfo->source_id.c_str());
  cJSON *report=cJSON_CreateObject();
  auto srcImg=sinfo->imgSets["img"];


  acvImage *copyImg=new acvImage();
  copyImg->ReSize(srcImg.get());
  acvCloneImage(srcImg.get(),copyImg,-1);
  Mat def_temp_img(copyImg->GetHeight(),copyImg->GetWidth(),CV_8UC3,copyImg->CVector[0]);

  
  // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);
  cJSON* rep_regionInfo=cJSON_CreateArray();
  
  cJSON_AddStringToObject(report,"id",id.c_str());
  cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);




  shared_ptr<StageInfo_Orientation> reportInfo(new StageInfo_Orientation());
  for(int i=0;;i++)
  {
    string key="regionInfo["+to_string(i)+"]";
    
    LOGI("key:%s",key.c_str());
    cJSON *regionInfo=JFetch_OBJECT(def,key.c_str());
    if(regionInfo==NULL)break;


    cJSON *region_report=cJSON_CreateObject();

    
    cJSON_AddItemToArray(rep_regionInfo,region_report);


    cJSON_AddNumberToObject(region_report,"idx",i);

    try{
      int X=(int)*JFetEx_NUMBER(regionInfo,"region[0]");
      int Y=(int)*JFetEx_NUMBER(regionInfo,"region[1]");
      int W=(int)*JFetEx_NUMBER(regionInfo,"region[2]");
      int H=(int)*JFetEx_NUMBER(regionInfo,"region[3]");


      Mat def_temp_img_ROI = def_temp_img(Rect(X, Y, W, H));


      

      if(1){
        
        Mat img_HSV;
        cvtColor(def_temp_img_ROI, img_HSV, COLOR_BGR2HSV);


        double l_h=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.h",0);
        double l_s=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.s",0);
        double l_v=JFetch_NUMBER_ex(regionInfo,"hsv.rangel.v",0);

        double h_h=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.h",180);
        double h_s=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.s",255);
        double h_v=JFetch_NUMBER_ex(regionInfo,"hsv.rangeh.v",255);

        LOGI("%f %f %f     %f %f %f",l_h,l_s,l_v,  h_h,h_s,h_v);
        Scalar rangeH=Scalar(h_h,h_s,h_v);
        Scalar rangeL=Scalar(l_h,l_s,l_v);

        Mat img_HSV_threshold;
        inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);
        // cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);


        {
          double *colorThres=JFetch_NUMBER(regionInfo,"colorThres");
          if(colorThres)
          {
            GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
            threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, THRESH_BINARY);
          }
        }
        
            
        {
          double *resultOverlayAlpha=JFetch_NUMBER(regionInfo,"resultOverlayAlpha");
          if(resultOverlayAlpha && *resultOverlayAlpha>0 && *resultOverlayAlpha<=1)
          {
            // cv::cvtColor(img_HSV_threshold,def_temp_img_ROI,COLOR_GRAY2RGB);
            Mat img_HSV_threshold_rgb;
            cv::cvtColor(img_HSV_threshold,img_HSV_threshold_rgb,COLOR_GRAY2RGB);
            addWeighted( 
            img_HSV_threshold_rgb, *resultOverlayAlpha, 
            def_temp_img_ROI, 1-*resultOverlayAlpha, 0.0, 
            def_temp_img_ROI);
            // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
            // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
          }
        }

        vector<vector<Point> > contours;
        vector<Vec4i> hierarchy;
        findContours( img_HSV_threshold, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE );



        double contourLenH=JFetch_NUMBER_ex(regionInfo,"contour.lengthh",INFINITY);
        double contourLenL=JFetch_NUMBER_ex(regionInfo,"contour.lengthl",0);
        double contourAreaH=JFetch_NUMBER_ex(regionInfo,"contour.areah",INFINITY);
        double contourAreaL=JFetch_NUMBER_ex(regionInfo,"contour.areal",0);

        float maxScore=0;
        float taridx=-1;
        float tarArea=0;
        for( size_t i = 0; i< contours.size(); i++ )
        {
          int contourL=contours[i].size();

          LOGI("[%d] L:%d",i,contourL);
          if(contourL<contourLenL || contourL>contourLenH )continue;

          auto &approx_contours=contours[i];
          // vector<cv::Point>approx_contours;
          // float epsilon = 0.001*contourL;
          // if(epsilon<2)epsilon=2;
          // approxPolyDP(contours[i], approx_contours,epsilon, true);



          double area = contourArea(approx_contours,false);
          LOGI("[%d] L:%d epsi:%d nL:%d area:%f",i,contourL,approx_contours.size(),area);
          int iarea=area;
          if(iarea<contourAreaL || iarea>contourAreaH )continue;


          // int sqArea=contourL/4;
          // float cratio=area/(sqArea*sqArea);
          


          if(maxScore<area)
          {
            maxScore=area;
            tarArea=area;
            taridx=i;


            LOGI("%d>>contourL:%d iarea:%d",i,contourL,iarea);


          }

        }

        StageInfo_Orientation::orient orie;
        orie.angle=NAN;
        orie.center=(acv_XY){NAN,NAN};
        if(taridx!=-1)
        {


          vector<Point>  hull;
          convexHull(Mat(contours[taridx]), hull);


          RotatedRect rrect=fitEllipse(hull);

          orie.center.X=rrect.center.x+X;
          orie.center.Y=rrect.center.y+Y;
          orie.flip=false;
          orie.angle=rrect.angle*3.14159/180;

          LOGI("center:%f %f  angle:%f",orie.center.X,orie.center.Y,orie.angle*180/3.14159);
          cJSON_AddNumberToObject(region_report,"contour_length",hull.size());

          {

            cJSON *center=cJSON_CreateObject();

            
            cJSON_AddItemToObject(region_report,"center",center);

            cJSON_AddNumberToObject(center,"x",orie.center.X);
            cJSON_AddNumberToObject(center,"y",orie.center.Y);

          }
          cJSON_AddNumberToObject(region_report,"area",tarArea);

          cJSON_AddNumberToObject(region_report,"angle",orie.angle);
          cJSON_AddBoolToObject(region_report,"flip",orie.flip);
        }
        // if(maxScore>0)
        reportInfo->orientation.push_back(orie);


      }



    }catch(...)
    {
      
      LOGE("....ERROR....");
    }
    
    // char* defStr=cJSON_Print(regionInfo);
    // LOGI(">>>%d\n%s",i,defStr);
    // delete defStr;

    
  }
  

  reportInfo->sharedInfo.push_back(sinfo);
  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->imgSets["img"]=shared_ptr<acvImage>(copyImg);
  reportInfo->imgSets["src"]=srcImg;
  
  reportInfo->trigger_id=sinfo->trigger_id;
  // reportInfo->trigger_tags.push_back("InfoStream2UI");
  // reportInfo->trigger_tags.push_back("ToTestRule");
  reportInfo->trigger_tags.push_back("ImTran");
  // reportInfo->trigger_tags.push_back("regionInspection");

  
  reportInfo->trigger_tags.push_back(id);

  

  reportInfo->img_prop.StreamInfo.channel_id=JFetch_NUMBER_ex(additionalInfo,"stream_info.stream_id",0);
  reportInfo->img_prop.StreamInfo.downsample=JFetch_NUMBER_ex(additionalInfo,"stream_info.downsample",10);
  LOGI("CHID:%d",reportInfo->img_prop.StreamInfo.channel_id);

  
  cJSON *freport=cJSON_CreateObject();
  cJSON_AddItemToObject(freport,"report",rep_regionInfo);
  reportInfo->jInfo=freport;

  attachSstaticInfo(reportInfo->jInfo,reportInfo->trigger_id);

  belongMan->dispatch(reportInfo);
}

InspectionTarget_Orientation_ColorRegionOval::~InspectionTarget_Orientation_ColorRegionOval()
{
}
