
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
  setInspDef(def);
}



int expEleRefSplit(string expEle,string &name,string &attr)
{
  if (expEle.rfind("N_", 0) == -1)return -1; 
  std::size_t ptPos = expEle.find(".");
  int nameEndLoc = ptPos;
  if(ptPos==std::string::npos)
  {
    nameEndLoc=expEle.size();
  }

  const string name_str = expEle.substr(2,nameEndLoc-2);
  string attr_str;
  if(ptPos!=std::string::npos)
  {
    attr_str=expEle.substr(ptPos+1);
  }

  name=name_str;
  attr=attr_str;

  return 0;
}

void InspectionTarget_SurfaceCheckSimple::setInspDef(cJSON *def)
{
  // LOGI("scriptTable.clear()");
  // scriptTable.clear();
  LOGI("scriptTable.clear()");


  for (auto& scriptv : scriptTable)
  {
    if(scriptv.second==NULL)  continue;
    delete scriptv.second;
    scriptv.second=NULL;
  }
  scriptTable.clear();







  InspectionTarget::setInspDef(def);

  //check if background_temp is loaded
  if(background_temp.empty())
    background_temp=imread(local_env_path+"/background_temp.png", IMREAD_COLOR);


  cJSON* jsub_regions = JFetch_ARRAY(def,"sub_regions");

  if(jsub_regions!=NULL)
  {
    int jsub_regions_L=cJSON_GetArraySize(jsub_regions);
    //find type==CALC from elements
    for(int i=0;i<jsub_regions_L;i++)
    {
      cJSON* jsub_region = cJSON_GetArrayItem(jsub_regions,i);
      string type=JFetch_STRING_ex(jsub_region,"type");
      string sr_name=JFetch_STRING_ex(jsub_region,"name");
      if(type=="CALC")
      {
        string script=JFetch_STRING_ex(jsub_region,"script");
        CompScript *cs=new CompScript();
        
        cJSON* variableArr = JFetch_ARRAY(jsub_region,"variables");
        if(variableArr){


          int variableArrL=cJSON_GetArraySize(variableArr);
          for (size_t j = 0; j < variableArrL; j++)
          {
            cJSON* vname = cJSON_GetArrayItem(variableArr,j);
            string full_name(vname->valuestring);


            string v_name;
            string v_attr;
            expEleRefSplit(full_name,v_name,v_attr);
            if(v_name.empty()) continue;

            bool isVarFound=false;
            for(int k=0;k<jsub_regions_L;k++)//check if the name is in sub_regions
            {
              cJSON* jsub_region = cJSON_GetArrayItem(jsub_regions,k);
              string sr_name=JFetch_STRING_ex(jsub_region,"name");
              if(sr_name==v_name)
              {
                isVarFound=true;
                break;
              }

            }

            if(isVarFound==false)continue;

            float n=NAN;
            cs->add_variable(full_name,n);
          }
          
        }

        {
          float n=NAN;
          cs->add_variable("nan",n);
        }



        cs->compile(script);
        scriptTable[sr_name]=cs;
        // LOGI(">>> sr_name:%s v:%f,%f",sr_name.c_str(),cs->value(),scriptTable[sr_name]->value());
      }
    }

  }


  LOGE("background_temp empty:%d",background_temp.empty());
  // featureInfo
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

cv::Scalar ImgRegionAveraging(Mat &img,cJSON *refRegion,int dowmSapleF=1)
{
  if(refRegion==NULL)return cv::Scalar(NAN,NAN,NAN);

  cJSON *region= refRegion;

  int x=(int)JFetch_NUMBER_ex(region,"x")/dowmSapleF;
  int y=(int)JFetch_NUMBER_ex(region,"y")/dowmSapleF;
  int w=(int)JFetch_NUMBER_ex(region,"w")/dowmSapleF;
  int h=(int)JFetch_NUMBER_ex(region,"h")/dowmSapleF;


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

cv::Scalar ImgRegionsAveraging(Mat &img,cJSON *refRegionArray,int dowmSapleF=1)
{
  if(refRegionArray==NULL)return cv::Scalar(NAN,NAN,NAN);

  int regionCount=cJSON_GetArraySize(refRegionArray);
  cv::Scalar rsum={0};
  int pixCSum=0;

  for(int i=0;i<regionCount;i++)
  {
    
    cJSON *region= cJSON_GetArrayItem(refRegionArray,i);

    int x=(int)JFetch_NUMBER_ex(region,"x")/dowmSapleF;
    int y=(int)JFetch_NUMBER_ex(region,"y")/dowmSapleF;
    int w=(int)JFetch_NUMBER_ex(region,"w")/dowmSapleF;
    int h=(int)JFetch_NUMBER_ex(region,"h")/dowmSapleF;


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
  
  if(type=="reload_background_temp")
  {
    background_temp=imread(local_env_path+"/background_temp.png", IMREAD_COLOR);
    return true;
  }

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


static void XYWH_clipping(int &X,int &Y,int &W,int &H, int MX,int MY,int MW,int MH)
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

float findBlobCount(Mat &m,float value)
{
  int crossCount=0;
  int width=m.size().width;
  bool inBlob=false;
  for(int i=0;i<width;i++)
  {
    int v=m.at<uint8_t>(0,i);


    bool cur_inBlob=(v>value)?true:false;

    if(inBlob==false && cur_inBlob==true)
    {
      crossCount++;
    }
    inBlob=cur_inBlob;
    // printf("%03d ",v);
    if(inBlob)
    {
      if(i==0)return -1;
    }
    else
    {

    }
  }
  if(inBlob)
  {
    return -2;
  }

  return crossCount;

}

template <class MAT_ValueType=uint8_t>
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
    MAT_ValueType v=m.at<MAT_ValueType>(0,i);
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



  for(int _i=0;_i<width;_i++)
  {
    int i=(reverseDir==false)?_i:width-_i-1;
    MAT_ValueType v=m.at<MAT_ValueType>(0,i);
    printf("%03f ",v);
  }
  printf("\n value:%f",value);


  return (idx2+idx1)/2;
}

float findCrossLoc_021(Mat &m,bool reverseDir=false)
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
    if(v==0)
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




class PostExpExe
{
  public:

  PostExpExe()
  {

  }

  int string_find_count(const char *str, char ch)
  {
    int count = 0;
    for (int i = 0; str[i]; i++)
    {
      if (str[i] == ch)
      {
        count++;
      }
    }
    return count;
  }
  int parse_CALC_Id(const char *post_exp)
  {
    if (post_exp[0] != '[')
      return -1;
    int idx = 0;
    for (int i = 1; post_exp[i]; i++)
    {
      char cc = post_exp[i];
      if ((cc < '0') || (cc > '9'))
        break;
      idx = idx * 10 + cc - '0';
    }
    return idx;
  }

  bool isParamsCache(const char *exp)
  {
    if (*exp != '$')
      return false;
    for (int i = 1; exp[i]; i += 2)
    {
      if (exp[i] != ',')
        return false;
      if (exp[i + 1] != '$')
        return false;
    }

    return true;
  }

  bool strMatchExact(const char *src, const char *pat)
  {
    for (int i = 0;; i++)
    {
      if (src[i] != pat[i])
        return false;
      if (src[i] == '\0')
        break;
    }
    return true;
  }
  
  
  double JfetchStrNUM(cJSON *obj, char *path)
  {
    double *num = JFetch_NUMBER(obj, path);
    if (num)
      return *num;

    char *str_num = JFetch_STRING(obj, path);
    if (str_num == NULL)
      return NAN;
    try
    {
      double dou = std::stod(str_num);

      return dou;
    }
    catch (...)
    {
    }
    LOGI("EXP.....");
    return NAN;
  }
  int functionExec_(const char *exp, float *params, int paramL, float *ret_result)
  {
    for (int i = 0; i < paramL; i++)
    {
      printf("[%d]:%f   ", i, params[i]);
    }
    printf("\n");
    if (ret_result)
      *ret_result = 0;
    if (strMatchExact(exp, "$+$"))
    {
      if (paramL != 2)
        return -1;
      *ret_result = params[0] + params[1];
      return 0;
    }
    else if (strMatchExact(exp, "$-$"))
    {

      if (paramL != 2)
        return -1;
      *ret_result = params[0] - params[1];
      return 0;
    }
    else if (strMatchExact(exp, "$*$"))
    {

      if (paramL != 2)
        return -1;
      *ret_result = params[0] * params[1];
      return 0;
    }
    else if (strMatchExact(exp, "$/$"))
    {

      if (paramL != 2)
        return -1;
      *ret_result = params[0] / params[1];
      return 0;
    }
    else if (strMatchExact(exp, "max$"))
    {
      float max = params[0];
      for (int i = 1; i < paramL; i++)
      {
        if (max < params[i])
          max = params[i];
      }
      *ret_result = max;
      return 0;
    }
    else if (strMatchExact(exp, "min$"))
    {
      float min = params[0];
      for (int i = 1; i < paramL; i++)
      {
        if (min > params[i])
          min = params[i];
      }
      *ret_result = min;
      return 0;
    }

    return -2;
  }


  float CALC(std::vector<string> postexp, vector<StageInfo_SurfaceCheckSimple::SubRegion_Info> &paramList,float *result)
  {
    if(result)*result=NAN;
    vector<float> calcStack;

    //funcParamHeadIdx indicates the function params starts from
    //exp: [5,64,11]
    int funcParamCount = 0;

    //"exp": "max(sin([3]*3),0)",
    //"post_exp": ["[3]","3","$*$","sin$","0","$,$","max$"]
    for (int i = 0; i < postexp.size(); i++)
    {
      const string post_exp = postexp[i];
      //LOGI("post_exp[%d]:%s",i,post_exp.c_str());


      LOGI("====%d===exp:%s",i,post_exp.c_str());
      if (post_exp.rfind("N_", 0) == 0) { //refrence param

        string name_str,attr_str;
        expEleRefSplit(post_exp,name_str,attr_str);
        
        if(name_str.empty())
        {
          return -81;
        }

        StageInfo_SurfaceCheckSimple::SubRegion_Info *p_info=NULL;
        for(int j=0;j<paramList.size();j++)
        {
          if(name_str==paramList[j].name)
          {
            p_info=&(paramList[j]);
            break;
          }
        }

        if(p_info==NULL)
        {
          return -80;
        }

        StageInfo_SurfaceCheckSimple::SubRegion_Info &info=*p_info;


        if(attr_str.empty()||attr_str=="v" ||attr_str=="val"||attr_str=="value" ||attr_str=="score")
        {
          calcStack.push_back(p_info->score);
        }
        else if(attr_str=="cat"||attr_str=="category")
        {
          calcStack.push_back(p_info->category);
        }
        else
        {
          calcStack.push_back(NAN);
        }

      }
      else
      { //if it's not an id refence
        int paramSymbolCount = string_find_count(post_exp.c_str(), '$');
        if (paramSymbolCount > 0)
        { //if it's a function (sin$, cos$, max$)

          if (isParamsCache(post_exp.c_str()))
          { //If it's  $,$,.... just save the funcParamCount

            //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
            funcParamCount = paramSymbolCount;
          }
          else
          {
            if (funcParamCount > 1)
            {
              paramSymbolCount = funcParamCount;
            }
            funcParamCount = 1;

            //LOGI("isParamsCache:%s>>>%d",post_exp.c_str(),paramSymbolCount);
            float res;
            int err_code =
                functionExec_(
                    post_exp.c_str(),
                    &(calcStack[calcStack.size() - paramSymbolCount]),
                    paramSymbolCount,
                    &res);

            //LOGI("%f, %d",res,err_code);
            if (err_code != 0)
            {
              return -50;
            }
            for (int k = 0; k < paramSymbolCount; k++)
            {
              calcStack.pop_back();
            }
            calcStack.push_back(res);
          }
        }
        else
        { //then it might be a number, try

          float val=NAN;
          try
          {
            val = std::stof(post_exp);
          }
          catch (...)
          {
            //val = NAN;
            return -3;
          }
          calcStack.push_back(val);
        }
      }


      for(int k=0;k<calcStack.size();k++)
      {
        printf("%0.5f,",calcStack[k]);
      }
      printf("\n");

    }

    // if (calcStack.size() != 1)
    // {
    //   if (ret_result)
    //     *ret_result = NAN;
    //   return -50;
    // }
    if(result)*result= calcStack[0];
    return 0;
  }

};


PostExpExe pee;

int PerformInsp(
  int idx,
  vector<StageInfo_SurfaceCheckSimple::SubRegion_Info> &regionResultList,
  map<string,CompScript*> &scriptTable,
  cJSON *jsub_regions,

  int downSampleF,
  bool show_display_overlay,
  Mat &_def_temp_img_ROI,
  Mat &_def_temp_img_ROI_BK,
  
  vector<Mat> &resultMarkOverlay,
  vector<Mat> &resultMarkRegion,
  vector<Mat> &resultImage,
  Mat &bg_img_ROI,
  int callDepth=0
  
  )
{
  if(regionResultList[idx].type!=StageInfo_SurfaceCheckSimple::id_UNSET || callDepth>10)
    return regionResultList[idx].type;

  
  LOGI("PerformInsp idx:%d",idx);

  int subregIdx=idx;

  cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,subregIdx);

  if(jsub_region==NULL)return StageInfo_SurfaceCheckSimple::id_UNSET;

  std::string subRegName=regionResultList[idx].name;//JFetch_STRING_ex(jsub_region,"name","");
  string subRegType=JFetch_STRING_ex(jsub_region,"type","HSVSeg");

  int SUBR_category=STAGEINFO_CAT_UNSET;
  StageInfo_SurfaceCheckSimple::SubRegion_Info sri;
  sri.name=subRegName;
  sri.type=StageInfo_SurfaceCheckSimple::id_UNSET;



  do{

  if(subRegType=="CALC")
  {

    sri.type=StageInfo_SurfaceCheckSimple::id_CALC;
    // LOGI(scriptTable.find(subRegName)== scriptTable.end());
    if(scriptTable.find(subRegName)== scriptTable.end())
    {
      sri.category=STAGEINFO_CAT_NA;
      break;
    }


    // script.set_variable("subrexgIdx",subregIdx);

    CompScript *c_script=scriptTable[subRegName];


    sri.calc_stat.p_compile_fail_info=&(c_script->failInfo);
    {

      cJSON* variableArr=JFetch_ARRAY(jsub_region,"variables");
      if(variableArr){
        int variableArrL=cJSON_GetArraySize(variableArr);
        for (size_t j = 0; j < variableArrL; j++)
        {
          cJSON* vname = cJSON_GetArrayItem(variableArr,j);

          string name(vname->valuestring);
          
          
          float xv=NAN;
          string subRegName;
          string attr_str;
          int err=expEleRefSplit(name,subRegName,attr_str);

          if(err || subRegName.empty())
          {
            c_script->set_variable(name,xv);
            continue;
          }



          StageInfo_SurfaceCheckSimple::SubRegion_Info *p_info=NULL;
          for(int k=0;k<regionResultList.size();k++)
          {
            if(subRegName==regionResultList[k].name)
            {
              // LOGI("subRegName:%s type:%d",subRegName.c_str(),regionResultList[k].type);
              if(regionResultList[k].type==StageInfo_SurfaceCheckSimple::id_UNSET)
              {
                PerformInsp(k,regionResultList,scriptTable,jsub_regions,downSampleF,show_display_overlay,_def_temp_img_ROI,_def_temp_img_ROI_BK,resultMarkOverlay,resultMarkRegion,resultImage,bg_img_ROI,callDepth+1);

                if(regionResultList[k].type==StageInfo_SurfaceCheckSimple::id_UNSET)
                {
                  break;
                }
              }

              // LOGI("secsubRegName:%s type:%d",subRegName.c_str(),regionResultList[k].type);

              p_info=&(regionResultList[k]);
              break;
            }
          }

          if(p_info==NULL)
          {
            c_script->set_variable(name,xv);
            continue;
          }

          StageInfo_SurfaceCheckSimple::SubRegion_Info &info=*p_info;


          if(attr_str.empty()||attr_str=="v" ||attr_str=="val"||attr_str=="value" ||attr_str=="score")
          {
            xv=p_info->score;
          }
          else if(attr_str=="cat"||attr_str=="category")
          {
            xv=p_info->category;
          }
          else
          {
            xv=NAN;
          }
          c_script->set_variable(name,xv);







        }
      }
    }

    float v=c_script->value();


    sri.score=v;

    LOGE("subRegName:%s v:%f",subRegName.c_str(),v);
    if(v!=v)
    { 
      sri.category=STAGEINFO_CAT_NA;
    }
    else
    {
      double rangeFrom=JFetch_NUMBER_ex(jsub_region,"rangeFrom",0);
      double rangeTo=JFetch_NUMBER_ex(jsub_region,"rangeTo",1000000);
      if(rangeFrom<=rangeTo)//inner section, within
      {
        if(v<rangeFrom || v>rangeTo)//not in the range, NG
        {
          sri.category=STAGEINFO_CAT_NG;
        }
        else
        {
          sri.category=STAGEINFO_CAT_OK;
        }
      }
      else//outter section, except
      {
        if(v>=rangeFrom || v<=rangeTo)//not in the range, OK
        {
          sri.category=STAGEINFO_CAT_OK;
        }
        else
        {
          sri.category=STAGEINFO_CAT_NG;
        }
      }
    } 
    
    break;

  }

  else
  {


    


    sri.type=StageInfo_SurfaceCheckSimple::id_HSVSeg;


    bool x_locating_mark =JFetch_TRUE(jsub_region,"x_locating_mark");
    bool y_locating_mark =JFetch_TRUE(jsub_region,"y_locating_mark");

    float x_f=JFetch_NUMBER_ex(jsub_region,"region.x",-1);
    float y_f=JFetch_NUMBER_ex(jsub_region,"region.y",-1);
    float w_f=JFetch_NUMBER_ex(jsub_region,"region.w",-1);
    float h_f=JFetch_NUMBER_ex(jsub_region,"region.h",-1);

    int srX=ceil(x_f/downSampleF);
    int srY=ceil(y_f/downSampleF);


    int srW=floor((x_f+w_f)/downSampleF)-srX;
    int srH=floor((y_f+h_f)/downSampleF)-srY;
    // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
    XYWH_clipping(srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
    if(srW<=1 || srH<=1)
    {
      //invalid number
    }





    // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);


    Mat sub_region_ROI_origin_img =_def_temp_img_ROI(Rect(srX, srY, srW, srH));
    

    resultMarkRegion[subregIdx]=sub_region_ROI_origin_img;
    Mat sub_region_ROI = sub_region_ROI_origin_img.clone();




    bool bgDiff=JFetch_TRUE(jsub_region,"bg_diff");
    if(bgDiff&& bg_img_ROI.empty()==false)
    {
      float brightness_comp_ratio=0;

      cv::Mat sreg_bg=bg_img_ROI(Rect(srX, srY, srW, srH));
      cv::Mat sreg_img=_def_temp_img_ROI_BK(Rect(srX, srY, srW, srH));;
      cv::Mat sreg_img_BK=sreg_img.clone();


      cJSON* ignore_regions = JFetch_ARRAY(jsub_region,"ignore_regions");
      int ignore_regions_L=ignore_regions==NULL?0:cJSON_GetArraySize(ignore_regions);
      for(int k=0;k<ignore_regions_L;k++)
      {
        cJSON *ig_reg= cJSON_GetArrayItem(ignore_regions,k);
        
        int padding=2;
        int x=padding+(int)JFetch_NUMBER_ex(ig_reg,"x")/downSampleF;
        int y=padding+(int)JFetch_NUMBER_ex(ig_reg,"y")/downSampleF;
        int w=-2*padding+(int)JFetch_NUMBER_ex(ig_reg,"w")/downSampleF;
        int h=-2*padding+(int)JFetch_NUMBER_ex(ig_reg,"h")/downSampleF;

        XYWH_clipping(x,y,w,h, 0,0,sreg_bg.cols,sreg_bg.rows);
        sreg_bg(Rect(x,y,w,h)) = 0;
        sreg_img_BK(Rect(x,y,w,h)) = 0;

        // auto igregion=_def_temp_img_ROI(Rect(x,y,w,h));
        

        // cv::Scalar ig_sum= cv::sum(igregion);

      }

      cv::Scalar bg_sum= cv::sum(sreg_bg);            
      cv::Scalar img_sum= cv::sum(sreg_img_BK);
      // LOGI("bg_sum:%f %f %f img_sum:%f %f %f  ",bg_sum[0],bg_sum[1],bg_sum[2],img_sum[0],img_sum[1],img_sum[2]);
      cv::Scalar compScalar;//=refAvgPix/avgPix;
      compScalar[0]=img_sum[0]/bg_sum[0];
      compScalar[1]=img_sum[1]/bg_sum[1];
      compScalar[2]=img_sum[2]/bg_sum[2];
      // LOGI("[%d]  compScalar:%f %f %f",i,compScalar[0],compScalar[1],compScalar[2]);

      // // def_temp_img_innerROI*=compScalar;
      multiply(sreg_bg,compScalar, sreg_bg);


      // if(i<30)
      // {
      //   cv::Mat dbgImg(Size(srW*2,srH),CV_8UC3);
      //   sreg_bg.copyTo(dbgImg(Rect(0, 0, srW, srH)));
      //   sreg_img_BK.copyTo(dbgImg(Rect(srW, 0, srW, srH)));

      //   imwrite("data/ZZA/dbgImg_"+to_string(i)+"_"+to_string(j)+".jpg",dbgImg); 
      // }


      {
        cv::Mat matTemp;
        cv::subtract(sreg_img,sreg_bg, matTemp, cv::noArray(), CV_32S);
        matTemp = cv::abs(matTemp);
        matTemp.convertTo(sub_region_ROI, CV_8U); // Or other precisions (depths).


        // sub_region_ROI=sreg_bg;

      }




    }


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
            
            int x=(int)JFetch_NUMBER_ex(ig_reg,"x")/downSampleF;
            int y=(int)JFetch_NUMBER_ex(ig_reg,"y")/downSampleF;
            int w=(int)JFetch_NUMBER_ex(ig_reg,"w")/downSampleF;
            int h=(int)JFetch_NUMBER_ex(ig_reg,"h")/downSampleF;

            XYWH_clipping(x,y,w,h, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);

            igRegs.push_back(cv::Rect(x,y,w,h));

          }
        }



        float comp_sigma0=NAN;
        Mat vectors=cv::Mat::zeros(cv::Size(), CV_8UC3);

        // Loop over rows and columns of the image
        for (int row = 0; row < sub_region_ROI.rows; ++row) {
            for (int col = 0; col < sub_region_ROI.cols; ++col) {
              bool isPixInRect=false;
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

        int pixCount=vectors.rows;
        if(pixCount>20)
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
        else
        {
          comp_sigma0=NAN;
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
          sri.category=STAGEINFO_CAT_NA;
        }
        // MATCH_REGION_score+=area_sum;
        // MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,sri.category);


        regionResultList[subregIdx]=sri;

      }



      else if(subRegType=="ScanPoint")
      {

        bool centerOrEdge=JFetch_TRUE(jsub_region,"centerOrEdge");
        float scanAngle=JFetch_NUMBER_ex(jsub_region,"scanAngle");
        bool sense0to1=JFetch_TRUE(jsub_region,"sense0to1");


        {

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

          Mat img_HSV_range;
          Mat img_HSV_threshold;
          inRange(img_HSV, rangeL, rangeH, img_HSV_range);


          double detect_detail=JFetch_NUMBER_ex(jsub_region,"detect_detail",100);

          if(sense0to1==false)
          {
            cv::blur(img_HSV_range,img_HSV_range,cv::Size(5,5));
            threshold(img_HSV_range, img_HSV_threshold, detect_detail, 255, THRESH_BINARY);

          }
          else
          {
            img_HSV_threshold=img_HSV_range;
          }



          if(JFetch_TRUE(jsub_region,"invert_detection")==false)
            bitwise_not(img_HSV_threshold , img_HSV_threshold);//need bit not by default, so the "invert" will be NOT to bit not



          resultMarkOverlay[subregIdx]=img_HSV_threshold.clone();
          if(show_display_overlay)
          {
            float resultOverlayAlpha = JFetch_NUMBER_ex(jsub_region,"resultOverlayAlpha",0);

            if(resultOverlayAlpha>0)
            {
              // cv::cvtColor(img_HSV_threshold,def_temp_img_innerROI,COLOR_GRAY2RGB);
              Mat img_HSV_threshold_rgb;
              cv::cvtColor(img_HSV_threshold,img_HSV_threshold_rgb,COLOR_GRAY2RGB);

              addWeighted( 
              img_HSV_threshold_rgb, resultOverlayAlpha, 
              sub_region_ROI, 1, 0, 
              sub_region_ROI);


              // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
              // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
            }
          }


          {
            Mat axisSum;
            int axisIdx=(scanAngle==0 || scanAngle==180)?0:1;
            cv::reduce(img_HSV_threshold, axisSum, axisIdx, REDUCE_AVG, CV_8U);
            if(axisIdx==1)
              axisSum=axisSum.t();

            if(sense0to1==false)
              cv::GaussianBlur(axisSum, axisSum, cv::Size(5, 1), 5);
            // LOGE("X . axisSum:%d . %d",axisSum.size().width,axisSum.size().height);

            Mat axisSum2;
            double otsu_threshold = (sense0to1)?1:cv::threshold(axisSum, axisSum2, 0 /*ignored value*/, 255, cv::THRESH_OTSU);
            // LOGE("otsu_threshold:%f",otsu_threshold);
            float xLoc1 = findCrossLoc(axisSum,otsu_threshold,false);
            float xLoc2 = findCrossLoc(axisSum,otsu_threshold,true);
            int blobCount=findBlobCount(axisSum,otsu_threshold);
            LOGE("id:%s xLoc1:%f xLoc2:%f blobCount:%d",subRegName.c_str(),xLoc1,xLoc2,blobCount);

            sri.category=STAGEINFO_CAT_OK;
            int NA_margin=2;
            if(axisIdx==0)
            {

              if(scanAngle==0 || centerOrEdge)
              {
                if(xLoc1<=NA_margin||xLoc1>=srW-NA_margin)
                {
                  sri.category=STAGEINFO_SCS_CAT_BASIC_reducer(sri.category,STAGEINFO_CAT_NA);
                }
              }
              if(scanAngle==180|| centerOrEdge)
              {
                if(xLoc2<=NA_margin||xLoc2>=srW-NA_margin)
                {
                  sri.category=STAGEINFO_SCS_CAT_BASIC_reducer(sri.category,STAGEINFO_CAT_NA);
                }
              }

              xLoc1+=srX;
              xLoc2+=srX;
              sri.score=centerOrEdge?(xLoc2+xLoc1)/2:(scanAngle==0?xLoc1:xLoc2);

            }
            else
            {

              if(scanAngle==90 || centerOrEdge)
              {
                if(xLoc1<=NA_margin||xLoc1>=srH-NA_margin)
                {
                  sri.category=STAGEINFO_SCS_CAT_BASIC_reducer(sri.category,STAGEINFO_CAT_NA);
                }
              }
              if(scanAngle==270|| centerOrEdge)
              {
                if(xLoc2<=NA_margin||xLoc2>=srH-NA_margin)
                {
                  sri.category=STAGEINFO_SCS_CAT_BASIC_reducer(sri.category,STAGEINFO_CAT_NA);
                }
              }


              xLoc1+=srY;
              xLoc2+=srY;
              sri.score=centerOrEdge?(xLoc2+xLoc1)/2:(scanAngle==90?xLoc1:xLoc2);
              sri.scanPoint_stat.blob_count=blobCount;
              // LOGE("xLoc1:%d xLoc2:%d",xLoc1,xLoc2);
              // sri.category=(xLoc1<=NA_margin||xLoc2<=NA_margin||
              // xLoc1>=srW-NA_margin||xLoc2>=srW-NA_margin||
              // )?STAGEINFO_CAT_NA:STAGEINFO_CAT_OK;
            }

            sri.type=StageInfo_SurfaceCheckSimple::id_ScanPoint;

            // MATCH_REGION_score+=area_sum;
            // MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,sri.category);


            regionResultList[subregIdx]=sri;
            
          }

        }

        resultImage[subregIdx]=sub_region_ROI;

      }




      else if(subRegType=="PassThru")
      {

        string scanDir=JFetch_STRING_ex(jsub_region,"scanDir");


        {

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

          Mat img_HSV_range;
          Mat img_HSV_threshold;
          inRange(img_HSV, rangeL, rangeH, img_HSV_range);


          double detect_detail=JFetch_NUMBER_ex(jsub_region,"detect_detail",100);
          cv::blur(img_HSV_range,img_HSV_range,cv::Size(5,5));

          threshold(img_HSV_range, img_HSV_threshold, detect_detail, 255, THRESH_BINARY);


          if(JFetch_TRUE(jsub_region,"invert_detection")==false)
            bitwise_not(img_HSV_threshold , img_HSV_threshold);//need bit not by default, so the "invert" will be NOT to bit not


          {
          

            Mat clonedImg=img_HSV_threshold.clone();

            int w=clonedImg.cols;
            int h=clonedImg.rows;

            {
              Point p_tl(0, 0), p_bl(0, h); 

              Point p_tr(w-1, 0), p_br(w-1, h); 


              int thickness = 1; 
              if(scanDir=="x" || scanDir=="-x")//draw RL side bar
              {
                line(clonedImg, p_tl, p_bl,Scalar(0, 0, 0),  thickness, LINE_4); 
                line(clonedImg, p_tr, p_br,Scalar(0, 0, 0),  thickness, LINE_4); 
              }

              if(scanDir=="y" || scanDir=="-y")//draw top bottom bar
              {
                line(clonedImg, p_tl, p_tr,Scalar(0, 0, 0),  thickness, LINE_4); 
                line(clonedImg, p_bl, p_br,Scalar(0, 0, 0),  thickness, LINE_4); 
              }

            }


            int floodVal=60;

            {//do flood fill
              Point seedPoint(0,0); // Coordinates (x: 100, y: 50)

              if(scanDir=="x"  || scanDir=="y" )seedPoint=Point(0,0);
              else if(scanDir=="-x" || scanDir=="-y")seedPoint=Point(w-1,h-1);


              Rect boundingRect; // The rectangle that flood fill will affect

              Scalar newVal(floodVal, floodVal, floodVal); // Fill color (white)
              floodFill(clonedImg, seedPoint, newVal, &boundingRect, 200, 0, 4);
              
            }

            {//check pixel value if flood to the oppsite side
              Point checkPoint(0,0); // Coordinates (x: 100, y: 50)

              if(scanDir=="x"  || scanDir=="y" )checkPoint=Point(w-1,h-1);
              else if(scanDir=="-x" || scanDir=="-y")checkPoint=Point(0,0);

              Scalar checkVal=clonedImg.at<uchar>(checkPoint);
              int val=checkVal[0];
              // LOGI("checkVal:%d",val);
              if(val==floodVal)
              {//panetrated
                sri.category=STAGEINFO_CAT_OK;
              }
              else
              {//blocked somewhere in the middle
                sri.category=STAGEINFO_CAT_NG;
              }

            }
            // {

            //   Point seedPoint(w-1,h-1); // Coordinates (x: 100, y: 50)
            //   Rect boundingRect; // The rectangle that flood fill will affect

            //   Scalar newVal(128, 128, 128); // Fill color (white)
            //   floodFill(clonedImg, seedPoint, newVal, &boundingRect, 200, 0, 4);
              
            // }




            // imwrite("data/ZZA/"+id+"_"+std::to_string(i)+"_"+std::to_string(j)+".jpg",img_HSV_threshold);
            // imwrite("data/ZZA/"+id+"_"+std::to_string(i)+"_"+std::to_string(j)+"flood.jpg",clonedImg);
          }


          {

            // sri.category=STAGEINFO_SCS_CAT_BASIC_reducer(sri.category,STAGEINFO_CAT_OK);
            sri.score=0;
            sri.type=StageInfo_SurfaceCheckSimple::id_PassThru;

            // MATCH_REGION_score+=area_sum;
            // MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,sri.category);


            regionResultList[subregIdx]=sri;
            
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

              addWeighted( 
              img_HSV_threshold_rgb, resultOverlayAlpha, 
              sub_region_ROI, 1, 0, 
              sub_region_ROI);


              // cv::GaussianBlur( img_HSV_threshold, img_HSV_threshold, Size( 11, 11), 0, 0 );
              // cv::threshold(img_HSV_threshold, img_HSV_threshold, *colorThres, 255, cv::THRESH_BINARY);
            }
          }


        }

        resultImage[subregIdx]=sub_region_ROI;

      }




      else if(subRegType=="DirectionalDiff")
      {


        float dirAngle=JFetch_NUMBER_ex(jsub_region,"dirAngle");
        float thres=JFetch_NUMBER_ex(jsub_region,"thres");
        float diffSupressThres=JFetch_NUMBER_ex(jsub_region,"diffSupressThres");



        Mat gray_img;
        cv::cvtColor(sub_region_ROI, gray_img, COLOR_BGR2GRAY);


        float diff=0;

        if(0)
        {
          // sobel filter with absolute value along the dirAngle
          
          Mat sobelDir;
          if(dirAngle==0 || dirAngle==180)
          {
            cv::Sobel(gray_img, sobelDir, CV_32F, 1, 0, 3);
          }
          else
          {
            cv::Sobel(gray_img, sobelDir, CV_32F, 0, 1, 3);
            sobelDir=sobelDir.t();

          }
          sobelDir=abs(sobelDir);
          Mat sobelMax;
          cv::reduce(sobelDir, sobelMax, 0, REDUCE_MAX, CV_32F);

          float maxEdge=0;
          float addEdge=0;
          int maxEdgeIdx=0;

          // printf("idx:%03d>>",i);
          for(int k=0;k<sobelMax.size().width;k++)
          {
            float edgeV=sobelMax.at<float>(0,k);
            printf("%f ",edgeV);


            if(edgeV>maxEdge)
            {
              maxEdge=edgeV;
              maxEdgeIdx=k;
            }
          }
          printf("\n");
          diff=maxEdge;
        }
        else
        {


          // //reduce img sub_region_ROI into a line
          Mat axisSum;
          int axisIdx=(dirAngle==0 || dirAngle==180)?0:1;
          cv::reduce(gray_img, axisSum, axisIdx, REDUCE_AVG, CV_32F);
          if(axisIdx==1)
            axisSum=axisSum.t();
          
          //on axisSum, do a gaussian blur
          // cv::GaussianBlur(axisSum, axisSum, cv::Size(5, 1), 5);

          //LOOP thru the axisSum, find the max diff
          float maxDiff=0;
          float addDiff=0;
          int maxDiffIdx=0;
          float prePixV=axisSum.at<float>(0,0);

          // printf("idx:%03d>>",i);
          for(int k=1;k<axisSum.size().width;k++)
          {
            float pix=axisSum.at<float>(0,k);
            float diff=(pix-prePixV);
            // printf("%03d ",diff);
            diff=abs(diff);
            if(diff>diffSupressThres)
            
              addDiff+=diff-diffSupressThres;


            if(diff>maxDiff)
            {
              maxDiff=diff;
              maxDiffIdx=k;
            }
            prePixV=pix;
          }
          // printf("\n");
          diff=addDiff;
        }





        sri.type=StageInfo_SurfaceCheckSimple::id_DirectionalDiff;
        sri.score=diff;
        if(diff==diff)
        {
          sri.category=(diff>thres)?STAGEINFO_CAT_NG:STAGEINFO_CAT_OK;
        }
        else
        {
          sri.category=STAGEINFO_CAT_NA;
        }


        // MATCH_REGION_score+=area_sum;
        // MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,sri.category);


      }

      break;
    }

    if(subRegType=="HSVSeg")
    {


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
            
            int x=(int)JFetch_NUMBER_ex(ig_reg,"x")/downSampleF;
            int y=(int)JFetch_NUMBER_ex(ig_reg,"y")/downSampleF;
            int w=(int)JFetch_NUMBER_ex(ig_reg,"w")/downSampleF;
            int h=(int)JFetch_NUMBER_ex(ig_reg,"h")/downSampleF;

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
          // MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category,
          // JFetch_TRUE(jsub_region,"color_compensation_diff_NG_as_NA")?
          // STAGEINFO_CAT_NA:STAGEINFO_CAT_NG);


          break;
        }

        // LOGI(">>>>>>>>>>>>colorBalancingDiff:%f",colorBalancingDiff);
        cv::Scalar compScalar;//=refAvgPix/avgPix;
        compScalar[0]=refAvgPix[0]/avgPix[0];
        compScalar[1]=refAvgPix[1]/avgPix[1];
        compScalar[2]=refAvgPix[2]/avgPix[2];
        // def_temp_img_innerROI*=compScalar;
        multiply(sub_region_ROI,compScalar, sub_region_ROI);



      }



      float sharpening_blurRad = JFetch_NUMBER_ex(jsub_region,"sharpening_blurRad",0);
      float sharpening_alpha = JFetch_NUMBER_ex(jsub_region,"sharpening_alpha",0);
      float sharpening_beta = JFetch_NUMBER_ex(jsub_region,"sharpening_beta",1+sharpening_alpha);
      float sharpening_gamma = JFetch_NUMBER_ex(jsub_region,"sharpening_gamma",0);
      if(sharpening_blurRad>1 && sharpening_alpha>0){

        Mat image(sub_region_ROI.rows,sub_region_ROI.cols,CV_8UC3);
        // cv::GaussianBlur(sub_region_ROI, image, cv::Size(0, 0), sharpening_blurRad);
        cv::blur(sub_region_ROI,image,cv::Size(sharpening_blurRad,sharpening_blurRad));
        cv::addWeighted(sub_region_ROI, 1+sharpening_alpha, image, -sharpening_alpha, sharpening_gamma, sub_region_ROI);
      }
      else if(sharpening_blurRad>1 && sharpening_alpha==0)
      {
        // cv::Mat channels[3];
        // cv::split(sub_region_ROI, channels);
        // for (int i = 0; i < 3; i++) {
        //   cv::Laplacian(channels[i], channels[i], CV_8U, sharpening_blurRad);
        // }
        int size=sharpening_blurRad*2+1;
        cv::Laplacian(sub_region_ROI, sub_region_ROI, CV_8U, size,0.1);
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
          
          int x=(int)JFetch_NUMBER_ex(ig_reg,"x")/downSampleF;
          int y=(int)JFetch_NUMBER_ex(ig_reg,"y")/downSampleF;
          int w=(int)JFetch_NUMBER_ex(ig_reg,"w")/downSampleF;
          int h=(int)JFetch_NUMBER_ex(ig_reg,"h")/downSampleF;

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





        findContours( img_HSV_threshold, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE );
        int area_sum=0;
        int element_max_area=0;
        int element_total_area=0;
        int line_max_length=0;
        int line_total_length=0;

        bool isNG=false;



        vector<int> cdpeth;
        cdpeth.resize(contours.size());
        for(int k=0;k<contours.size();k++)
        {
          cdpeth.push_back(-99999);
        }
        for(int k=0;k<contours.size();k++)
        {
          int pidx = hierarchy[k][3];
          if(pidx<0)
          {
            cdpeth[k]=0;
            continue;
          }

          cdpeth[k]=cdpeth[pidx]+1;
        }

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
          int area = contourArea(contours[k],false)*downSampleF*downSampleF;
          int a_area=area+contours[k].size()*downSampleF;
          bool isBlackContour=(cdpeth[k]&1)==0;
          // LOGI("idx:%d depth:%d  a_area:%d",k,cdpeth[k],a_area);
          area_sum+=a_area*(isBlackContour?1:-1);
          if(isBlackContour==false)continue;


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


            longestSide*=downSampleF;


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

        sri.category=SUBR_category;
        sri.score=area_sum; 


        {
          sri.hsvseg_stat.blob_area=area_sum;
          sri.hsvseg_stat.element_area=element_total_area;
          sri.hsvseg_stat.element_count=elementCount;
          sri.hsvseg_stat.max_line_length=line_max_length;
          
        }






      }
      
    }
  }


  }while(false);

  string NG_Map_To=JFetch_STRING_ex(jsub_region,"NG_Map_To","NG");




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




  regionResultList[subregIdx]=sri;

  return sri.category;


}


void InspectionTarget_SurfaceCheckSimple::singleProcess(shared_ptr<StageInfo> sinfo)
{
  if(useExtParam==true && extParam==NULL )
  {
    
    cache_stage_info=sinfo;
    return;
  }

  int downSampleF=JFetch_NUMBER_ex(def,"down_sample_factor",1);

  int64 t0 = cv::getTickCount();
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Orientation *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  auto srcImg=d_sinfo->img;


  Mat _CV_srcImg(srcImg->GetHeight(),srcImg->GetWidth(),CV_8UC3,srcImg->CVector[0]);
  Mat CV_srcImg;

  if(downSampleF==1)
  {
    CV_srcImg=_CV_srcImg;
  }
  else
  {
    resize(_CV_srcImg,CV_srcImg,Size(_CV_srcImg.cols/downSampleF,_CV_srcImg.rows/downSampleF));
  }


  //check if background_temp is loaded and has the same size as CV_srcImg
  LOGE("background_temp empty:%d",background_temp.empty());
  if(background_temp.size()!=CV_srcImg.size())
  {
    if(!background_temp.empty())
      background_temp.release();
  }



  LOGE("background_temp empty:%d",background_temp.empty());

  // if()
  
  // cvtColor(def_temp_img, def_temp_img, COLOR_BayerGR2BGR);

  // cJSON *report=cJSON_CreateObject();
  // cJSON* rep_regionInfo=cJSON_CreateArray();
  
  // cJSON_AddStringToObject(report,"id",id.c_str());
  // cJSON_AddItemToObject(report,"regionInfo",rep_regionInfo);


  float X_offset_O=JFetch_NUMBER_ex(def,"x_offset",0);
  float Y_offset_O=JFetch_NUMBER_ex(def,"y_offset",0);

  float W_O=JFetch_NUMBER_ex(def,"w");
  float H_O=JFetch_NUMBER_ex(def,"h");


  float X_offset=X_offset_O/downSampleF;
  float Y_offset=Y_offset_O/downSampleF;

  float W=W_O/downSampleF;
  float H=H_O/downSampleF;





  float angle_offset=JFetch_NUMBER_ex(def,"angle_offset",0)*M_PI/180;
  int multi_target_column_count=(int)JFetch_NUMBER_ex(def,"multi_target_column_count",99999);


  float color_ch_mul_r=JFetch_NUMBER_ex(def,"color_ch_mul.r",1);
  float color_ch_mul_g=JFetch_NUMBER_ex(def,"color_ch_mul.g",1);
  float color_ch_mul_b=JFetch_NUMBER_ex(def,"color_ch_mul.b",1);

  // LOGE("color_ch_mul:%f %f %f",color_ch_mul_r,color_ch_mul_g,color_ch_mul_b);

  acvImage *retImage=NULL;

  shared_ptr<StageInfo_SurfaceCheckSimple> reportInfo(new StageInfo_SurfaceCheckSimple());
  
  int default_blurRadius=(int)JFetch_NUMBER_ex(def,"blur_radius",0)/downSampleF;
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

        orient.center.X=JFetch_NUMBER_ex(jorient,"center.x")/downSampleF;
        orient.center.Y=JFetch_NUMBER_ex(jorient,"center.y")/downSampleF;
        orienList_ext.push_back(orient);
      }
    }
    orienList=&orienList_ext;
  }


  auto &orientationList=*orienList;

  

  int bilateral_d= (int)JFetch_NUMBER_ex(def,"bilateral.d",-1);
  float bilateral_sigmaColor= JFetch_NUMBER_ex(def,"bilateral.sigmaColor",2);
  float bilateral_sigmaSpace=JFetch_NUMBER_ex(def,"bilateral.sigmaSpace",2);


  bool do_equalize_hist=false;//JFetch_TRUE(def,"equalize_hist");
  // LOGI("orientation info size:%d",orientationList.size());
  if(orientationList.size()>0)
  {  

    {
      int columnCount=orientationList.size()<multi_target_column_count?orientationList.size():multi_target_column_count;
      int rowCount=((orientationList.size()+multi_target_column_count-1)/multi_target_column_count);

      LOGE("columnCount:%d rowCount:%d",columnCount,rowCount);
      retImage=new acvImage(W*columnCount,H*rowCount,3);
    }
    
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

        int o_col=imgOrderIdx%multi_target_column_count;
        int o_row=imgOrderIdx/multi_target_column_count;

        Mat _def_temp_img_ROI = def_temp_img(Rect(o_col*W, o_row*H, W, H));



        float angle = orientation.angle;
        // if(angle>M_PI_2)angle-=M_PI;
        // if(angle<-M_PI_2)angle+=M_PI;
        angle+=angle_offset;

        bool xFlip=false;
        bool yFlip=orientation.flip;
        orientation.center.X/=downSampleF;
        orientation.center.Y/=downSampleF;

        Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2+X_offset,H/2+Y_offset},-angle,xFlip,yFlip);

        cv::warpAffine(CV_srcImg, _def_temp_img_ROI, rot,_def_temp_img_ROI.size());

        Mat _def_temp_img_ROI_BK;
        _def_temp_img_ROI.copyTo(_def_temp_img_ROI_BK);
        if(color_ch_mul_r!=1 || color_ch_mul_g!=1 || color_ch_mul_b!=1)
        {
          cv::Scalar compScalar(color_ch_mul_b,color_ch_mul_g,color_ch_mul_r);
          multiply(_def_temp_img_ROI,compScalar, _def_temp_img_ROI);
        }

        if(do_equalize_hist)
        {

          cv::Mat &image=_def_temp_img_ROI;
          cv::Mat yuvImage;
          cv::cvtColor(image, yuvImage, cv::COLOR_BGR2YUV);

          // Split the YUV image into separate channels
          std::vector<cv::Mat> channels;
          cv::split(yuvImage, channels);

          // Apply histogram equalization on the Y channel
          cv::equalizeHist(channels[0], channels[0]);

          // Merge the channels back into a YUV image
          cv::merge(channels, yuvImage);

          // Convert the YUV image back to the original color space
          cv::cvtColor(yuvImage, image, cv::COLOR_YUV2BGR);
        }





        cJSON* jsub_regions = JFetch_ARRAY(def,"sub_regions");
        
        int jsub_regions_L=jsub_regions==NULL?0:cJSON_GetArraySize(jsub_regions);

        vector<Mat> resultMarkOverlay;
        vector<Mat> resultImage;
        vector<Mat> resultMarkRegion;
        vector<int> indexArr_w_priority;
        indexArr_w_priority.resize(jsub_regions_L);






        float mult_bri_Count=0;
        cv::Scalar mult_bri=0;
        for(int j=0;j<jsub_regions_L;j++)
        {

          int subregIdx=j;
          cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,subregIdx);
          string subRegType=JFetch_STRING_ex(jsub_region,"type");
          if(subRegType!="BrightnessBalance")continue;

          if(JFetch_TRUE(jsub_region,"enable")==false)continue;

          int srW=(int)JFetch_NUMBER_ex(jsub_region,"region.w",-1)/downSampleF;
          int srH=(int)JFetch_NUMBER_ex(jsub_region,"region.h",-1)/downSampleF;

          int srX=(int)JFetch_NUMBER_ex(jsub_region,"region.x",-1)/downSampleF;
          int srY=(int)JFetch_NUMBER_ex(jsub_region,"region.y",-1)/downSampleF;
          // LOGI("%d %d %d %d   %d %d %d %d ",srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
          XYWH_clipping(srX,srY,srW,srH, 0,0,_def_temp_img_ROI.cols,_def_temp_img_ROI.rows);
          if(srW<=1 || srH<=1)
          {
            //invalid number
          }

          Mat sub_region_ROI_origin_img =_def_temp_img_ROI(Rect(srX, srY, srW, srH));
          
          cv::Scalar pixSum= cv::sum(sub_region_ROI_origin_img);  
          pixSum/=(srW*srH);



          double bTarR=JFetch_NUMBER_ex(jsub_region,"bTar.r",128);
          double bTarG=JFetch_NUMBER_ex(jsub_region,"bTar.g",128);
          double bTarB=JFetch_NUMBER_ex(jsub_region,"bTar.b",128);
          cv::Scalar pixTar={bTarB,bTarG,bTarR};

          LOGI("pixTar:%f %f %f  pixSum:%f %f %f",pixTar[0],pixTar[1],pixTar[2],pixSum[0],pixSum[1],pixSum[2]);
          cv::Scalar multTar;
          
          multTar[0]=pixTar[0]/pixSum[0];
          multTar[1]=pixTar[1]/pixSum[1];
          multTar[2]=pixTar[2]/pixSum[2];
          mult_bri_Count++;  
          mult_bri+=multTar;

        }

        if(mult_bri_Count==0)
        {
          mult_bri={1,1,1};
        }
        else
        {
          mult_bri/=mult_bri_Count;
          LOGI("mult_bri:%f %f %f",mult_bri[0],mult_bri[1],mult_bri[2]);
          // multiply(_def_temp_img_ROI,mult_bri, _def_temp_img_ROI);
        }
















        


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


            bool locating_type_z2o_or_auto = JFetch_TRUE(jsub_region,"locating_type_z2o_or_auto");


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


            int srW=(int)JFetch_NUMBER_ex(jsub_region,"region.w",-1)/downSampleF;
            int srH=(int)JFetch_NUMBER_ex(jsub_region,"region.h",-1)/downSampleF;

            int srX=(int)JFetch_NUMBER_ex(jsub_region,"region.x",-1)/downSampleF+xShift;
            int srY=(int)JFetch_NUMBER_ex(jsub_region,"region.y",-1)/downSampleF+yShift;
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
                  
                  int x=(int)JFetch_NUMBER_ex(ig_reg,"x")/downSampleF;
                  int y=(int)JFetch_NUMBER_ex(ig_reg,"y")/downSampleF;
                  int w=(int)JFetch_NUMBER_ex(ig_reg,"w")/downSampleF;
                  int h=(int)JFetch_NUMBER_ex(ig_reg,"h")/downSampleF;

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


            Mat img_HSV_threshold;
            if(1)
            {

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

              inRange(img_HSV, rangeL, rangeH, img_HSV_threshold);

              if(JFetch_TRUE(jsub_region,"invert_detection")==false)
                bitwise_not(img_HSV_threshold , img_HSV_threshold);//need bit not by default, so the "invert" will be NOT to bit not


            }
            else
            {

              cv::cvtColor(sub_region_ROI, img_HSV_threshold, cv::COLOR_BGR2GRAY);


            }



            if(x_locating_mark)
            {
              Mat axisSum_f;
              cv::reduce(img_HSV_threshold, axisSum_f, 0, REDUCE_AVG, CV_32F);



              float otsu_threshold=1;
              if(locating_type_z2o_or_auto==false)
              {

                cv::GaussianBlur(axisSum_f, axisSum_f, cv::Size(3, 1), 3);
                Mat axisSum;
                axisSum_f.convertTo(axisSum, CV_8U);
                // LOGE("X . axisSum:%d . %d",axisSum.size().width,axisSum.size().height);

                Mat axisSum2;
                otsu_threshold = cv::threshold(axisSum, axisSum2, 0 /*ignored value*/, 255, cv::THRESH_OTSU);

              }

              LOGE("XLOC::");
              int cL = findCrossLoc<float>(axisSum_f,otsu_threshold,JFetch_TRUE(jsub_region,"x_locating_dir"));
              // LOGE("cL:%d",cL);

              _xShift+=axisSum_f.size().width/2-cL;
              xAdjCount++;
            }


            if(y_locating_mark)
            {
              Mat axisSum_f;
              cv::reduce(img_HSV_threshold, axisSum_f, 1, REDUCE_AVG, CV_32F);
              axisSum_f=axisSum_f.t();



              float otsu_threshold=1;
              if(locating_type_z2o_or_auto==false)
              {

                cv::GaussianBlur(axisSum_f, axisSum_f, cv::Size(3, 1), 3);
                Mat axisSum;
                axisSum_f.convertTo(axisSum, CV_8U);
                // LOGE("X . axisSum:%d . %d",axisSum.size().width,axisSum.size().height);

                Mat axisSum2;
                otsu_threshold = cv::threshold(axisSum, axisSum2, 0 /*ignored value*/, 255, cv::THRESH_OTSU);

              }

              LOGE("YLOC::");
              int cL = findCrossLoc<float>(axisSum_f,otsu_threshold,JFetch_TRUE(jsub_region,"y_locating_dir"));
              // LOGE("cL:%d",cL);

              _yShift+=axisSum_f.size().width/2-cL;
              yAdjCount++;
            }
          } 

          _xShift/=xAdjCount+0.00001;//just to make sure when xAdjCount==0 the compute would still be ok
          _yShift/=yAdjCount+0.00001;
          xShift+=_xShift;
          yShift+=_yShift;

        }

        Mat bg_img_ROI;
        if(background_temp.empty()==false)
        {
          Mat rot= getRotTranMat( orientation.center,(acv_XY){W/2+X_offset+xShift,H/2+Y_offset+yShift},-angle,xFlip,yFlip);
          cv::warpAffine(background_temp, bg_img_ROI, rot,_def_temp_img_ROI.size());

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
          LOGE(">>>>>shift x:%f  y:%f",xShift,yShift);



          if(do_equalize_hist)
          {

            cv::Mat &image=_def_temp_img_ROI;
            cv::Mat yuvImage;
            cv::cvtColor(image, yuvImage, cv::COLOR_BGR2YUV);

            // Split the YUV image into separate channels
            std::vector<cv::Mat> channels;
            cv::split(yuvImage, channels);

            // Apply histogram equalization on the Y channel
            cv::equalizeHist(channels[0], channels[0]);

            // Merge the channels back into a YUV image
            cv::merge(channels, yuvImage);

            // Convert the YUV image back to the original color space
            cv::cvtColor(yuvImage, image, cv::COLOR_YUV2BGR);
          }


        }




        if(mult_bri_Count!=0)
        {
          multiply(_def_temp_img_ROI,mult_bri, _def_temp_img_ROI);
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
        for(int j=0;j<mri.subregions.size();j++)
        {
          mri.subregions[j].type=StageInfo_SurfaceCheckSimple::id_UNSET;
          mri.subregions[j].category=STAGEINFO_CAT_UNSET;

          cJSON *jsub_region= cJSON_GetArrayItem(jsub_regions,j);
          mri.subregions[j].name=JFetch_STRING_ex(jsub_region,"name","");



          
        }


        resultMarkOverlay.resize(jsub_regions_L);
        resultMarkRegion.resize(jsub_regions_L);
        resultImage.resize(jsub_regions_L);





        for(int j=0;j<mri.subregions.size();j++)
        {
          // PerformInsp(j,mri.subregions,jsub_regions,downSampleF,_def_temp_img_ROI);

          PerformInsp(
          j,
          mri.subregions,
          scriptTable,
          jsub_regions,

           downSampleF,
           show_display_overlay,
          _def_temp_img_ROI,
          _def_temp_img_ROI_BK,
          
          resultMarkOverlay,
          resultMarkRegion,
          resultImage,
          bg_img_ROI
          
          );
        }


        for(int j=0;j<mri.subregions.size();j++)
        {
          if(mri.subregions[j].name=="RESULT")
          {
            MATCH_REGION_category=mri.subregions[j].category;
            break;
          }
          MATCH_REGION_category=STAGEINFO_SCS_CAT_BASIC_reducer(MATCH_REGION_category, mri.subregions[j].category);

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
  for (auto & scriptv : scriptTable)
  {
    if(scriptv.second==NULL)  continue;
    delete scriptv.second;
    scriptv.second=NULL;
  }
  scriptTable.clear();




  if(extParam)
  {
    cJSON_Delete(extParam);
    extParam=NULL;
  }
}
