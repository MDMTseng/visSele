
#include "InspTar_Empty.hpp"
#include "StageInfo_Orientation.hpp"
#include "StageInfo.hpp"
#include <iostream>
using namespace cv;

using namespace std;

template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}


void InspectionTarget_Empty::INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
{
  InspectionTarget::INIT(id,def,belongMan,local_env_path);
}

void InspectionTarget_Empty::setInspDef(cJSON *def)
{
  InspectionTarget::setInspDef(def);
}

void InspectionTarget_Empty::run()
{

  // acvImage cacheImage;
  while(true)
  {
    std::shared_ptr<StageInfo> curInput;
    // LOGI("<<<<<size():%d",datTransferQueue.size());
    // std::this_thread::sleep_for(std::chrono::milliseconds(500));//SLOW load test
          
    try{
      // LOGI("TryReadNew");
      if(input_queue.pop_blocking(curInput)==false)
      {
        LOGI("TryReadTailed");
        break;
      }

      singleProcess(curInput);
    }
    catch(TS_Termination_Exception e)
    {
      LOGI("TS_Termination_Exception");
      break;
    }
    
  }


}

bool InspectionTarget_Empty::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");
  

  return false;
}




int STAGEINFO_SCS_CAT_BASIC_reducer_(int sum_cat,int cat)
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

void InspectionTarget_Empty::singleProcess(shared_ptr<StageInfo> sinfo)
{
  LOGI("InspectionTarget_Empty::singleProcess");
}




InspectionTarget_Empty::~InspectionTarget_Empty()
{
 
}
