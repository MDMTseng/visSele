
#include "InspTar_SpriteDraw.hpp"


using namespace cv;

using namespace std;


template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_SpriteDraw::InspectionTarget_SpriteDraw(string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
  :InspectionTarget(id,def,belongMan,local_env_path)
{
  type=InspectionTarget_SpriteDraw::TYPE();
}


future<int> InspectionTarget_SpriteDraw::futureInputStagePool()
{
  return async(launch::async,&InspectionTarget_SpriteDraw::processInputStagePool,this);
}

int InspectionTarget_SpriteDraw::processInputPool()
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

bool InspectionTarget_SpriteDraw::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");

  return false;
}


cJSON* InspectionTarget_SpriteDraw::genITIOInfo()
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
      cJSON_AddItemToArray(sarr,cJSON_CreateString(StageInfo_Image::stypeName().c_str() ));
    }




  }

  return arr;

}


void InspectionTarget_SpriteDraw::singleProcess(shared_ptr<StageInfo> sinfo)
{
  int64 t0 = cv::getTickCount();
  LOGI("RUN:%s   from:%s dataType:%s ",id.c_str(),sinfo->source_id.c_str(),sinfo->typeName().c_str());
  

  auto d_sinfo = dynamic_cast<StageInfo_Orientation *>(sinfo.get());
  if(d_sinfo==NULL) {
    LOGE("sinfo type does not match.....");
    return;
  }
  auto srcImg=d_sinfo->img;

  vector<StageInfo_Orientation::orient> *orienList=&(d_sinfo->orientation);

  std::shared_ptr<StageInfo_Image> reportInfo(new StageInfo_Image());
  reportInfo->create_time_sysTick=d_sinfo->create_time_sysTick;
  reportInfo->source=this;
  reportInfo->source_id=id;
  reportInfo->trigger_id=d_sinfo->trigger_id;

  insertInputTagsWPrefix(reportInfo->trigger_tags,d_sinfo->trigger_tags,"s_");
  reportInfo->trigger_tags.push_back(id);
  if(orienList->size()==0)
  {

    reportInfo->img_show=d_sinfo->img_show;
    reportInfo->img_prop=d_sinfo->img_prop;
    reportInfo->img=d_sinfo->img;
    belongMan->dispatch(reportInfo);
    return;
  }


//   for(int i=0;i<orientationList.size();i++)
  
//   acvImage retImage=new acvImage(srcImg->GetWidth(),srcImg->GetHeight(),3);

  auto retImage=shared_ptr<acvImage>(new acvImage(srcImg->GetWidth(),srcImg->GetHeight(),3));
  acvCloneImage(srcImg.get(),retImage.get(),-1);
  Mat CV_srcImg(retImage->GetHeight(),retImage->GetWidth(),CV_8UC3,retImage->CVector[0]);

  
    
  
}

InspectionTarget_SpriteDraw::~InspectionTarget_SpriteDraw()
{
}
