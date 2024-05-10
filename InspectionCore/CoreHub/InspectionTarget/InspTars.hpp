#pragma once

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

//#include "MLNN.hpp"
#include "cJSON.h"
#include "logctrl.h"

#include "MatchingCore.h"
#include "acvImage_BasicTool.hpp"
#include "mjpegLib.h"

#include <sys/stat.h>
#include <libgen.h>
#include <main.h>
#include <playground.h>
#include <stdexcept>
#include <compat_dirent.h>
#include <smem_channel.hpp>
#include <ctime>
#include "CameraLayerManager.hpp"

#include <opencv2/calib3d.hpp>
#include "opencv2/imgproc.hpp"
#include <opencv2/imgcodecs.hpp>
#include <InspTar_Orientation.hpp>
#include <InspTar_SurfaceCheckSimple.hpp>

using namespace cv;
// class InspectionTarget_s :public InspectionTarget
// {

// public:
//   vector <InspectionTarget_s*> *localGroup;
//   InspectionTarget_s(std::string id,cJSON* def,vector <InspectionTarget_s*> *localGroup):InspectionTarget(id)
//   {
//     this->localGroup=localGroup;
//     // setInspDef(def);
//     // report=NULL;
//   }

//   virtual bool checkRef()=0;

//   virtual acvImage* fetchImage(std::string id)=0;

//   virtual cJSON* fetchInspReport()=0;


//   virtual void setInspDef(cJSON* def)=0;

//   virtual void CAM_CallBack(image_pipe_info *pipe)=0;
  
//   virtual ~InspectionTarget_s()
//   {
//   }

// };


class InspectionTarget_TEST_IT :public InspectionTarget
{
  public:
  

  static std::string sTYPE()
  {return "TIT";}
  virtual std::string TYPE()
  {return sTYPE();}

  // void INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
  // {
  //   InspectionTarget::INIT(id,def,belongMan,local_env_path);
  // }

  
  virtual cJSON* genITIOInfo()
  {
    cJSON* info= genITInfo();
    
    {
      cJSON_AddItemToObject(info, "io",genIOInfo({StageInfo_Image::stypeName()},{}) );
    }
    return info;
  }

  virtual void run()
  {
    while(1)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      LOGI("TEST_IT thread_run....");
    }
  }

  
};



class StageInfo_SIGroup:public StageInfo
{
  public:
  static std::string stypeName(){return "Group";}
  virtual std::string typeName(){return this->stypeName();}
  std::vector< std::shared_ptr<StageInfo> >  group;
};




class InspectionTarget_StageInfoCollect_Base :public InspectionTarget
{
  
  protected:

  std::map<int,  shared_ptr<StageInfo_SIGroup>> input_stage_group_cand;
  
  cJSON * match_tag_sets;
  public:
  
  std::mutex input_group_matching_lock;

  virtual void INIT(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path)
  {
    InspectionTarget::INIT(id,def,belongMan,local_env_path);
  }


  static std::string sTYPE()
  {return "StageInfoCollect_Base";}
  virtual std::string TYPE()
  {return sTYPE();}

  // std::future<int> futureInputStagePool()
  // {
  //   return std::async(launch::async,&InspectionTarget_StageInfoCollect_Base::processInputStagePool,this);
  // }


  void setInspDef(cJSON* def)
  {
    InspectionTarget::setInspDef(def);
    // LOGE("akjdfjkaflkajsndfklasjkldj=============");
    match_tag_sets=NULL;
    if(def)
    {
      match_tag_sets=JFetch_ARRAY(this->def,"match_tag_sets");
    }
  }


  bool tagMatchingWhiteListIndex(int whiteListidx,vector<string> &tagArr)
  {
    if(match_tag_sets==NULL)return false;
    
    int set_size=cJSON_GetArraySize(match_tag_sets);
    if(whiteListidx>=set_size)return false;
    


    cJSON *tags = cJSON_GetArrayItem(match_tag_sets,whiteListidx);

    return tagMatching(tags,tagArr);
  }

  int tagMatchingWhiteList(vector<string> &tagArr)
  {

    int set_size=cJSON_GetArraySize(match_tag_sets);
    for(int i=0;i<set_size;i++)
    {
      if(tagMatchingWhiteListIndex(i,tagArr))
      {
        return i;
      }
    }
    return -1;
  }



  virtual bool feedStageInfo(std::shared_ptr<StageInfo> sinfo)
  {
    std::lock_guard<std::mutex> _(input_group_matching_lock);
    int info_tid=sinfo->trigger_id;

    int matching_idx=tagMatchingWhiteList(sinfo->trigger_tags);

    if(matching_idx<0)return false;




    if (input_stage_group_cand.find(info_tid) == input_stage_group_cand.end()) //the trigger id is not in stage set
    { 
      shared_ptr<StageInfo_SIGroup> igi(new StageInfo_SIGroup());
      int set_size=cJSON_GetArraySize(match_tag_sets);
      for(int i=0;i<set_size;i++)
      {
        igi->group.push_back(NULL);//fill with empty slot
      }

      
      igi->trigger_id=info_tid;
      input_stage_group_cand.insert ( std::pair<int, shared_ptr<StageInfo_SIGroup> >(info_tid,igi) );

    }



    auto &stage_group=input_stage_group_cand[info_tid];

    stage_group->group[matching_idx]=sinfo;//put the stage info according to the specified index(from def file match_tag_sets)


    bool is_full_match=true;
    for(int i=0;i<stage_group->group.size();i++)
    {
      if(stage_group->group[i]==NULL)
      {
        is_full_match=false;
        break;
      }
    }
    if(is_full_match)
    {
      input_queue.push_blocking(stage_group);
      input_stage_group_cand.erase(info_tid);
    }
    return true;
  }



  
  
  virtual cJSON* genITIOInfo()
  {
    cJSON* info= genITInfo();
    
    {
      cJSON_AddItemToObject(info, "io",genIOInfo({StageInfo_SIGroup::stypeName()},{}) );
    }
    return info;
  }


  void run()
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

        singleGroupProcess(curInput);
      }
      catch(TS_Termination_Exception e)
      {
        LOGI("TS_Termination_Exception");
        break;
      }
      
    }


  }




  virtual void singleGroupProcess(shared_ptr<StageInfo> sinfo)=0;

  ~InspectionTarget_StageInfoCollect_Base()
  {
    
  }
};


/*

class InspectionTarget_ReduceCategorize :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  public:

  
  InspectionTarget_ReduceCategorize(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path)
    {}
  static std::string TYPE(){ return "ReduceCategorize"; }
  void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {
    

    shared_ptr<StageInfo_Category> reportInfo(new StageInfo_Category());

  
    reportInfo->source=this;
    reportInfo->source_id=id;
    
    reportInfo->trigger_id=trigger_id;
    reportInfo->trigger_tags.push_back(reportInfo->source_id);
    reportInfo->category=-909;
    reportInfo->jInfo=NULL;
    belongMan->dispatch(reportInfo);

  }

  ~InspectionTarget_ReduceCategorize()
  {
  }
  

};





class InspectionTarget_GroupResultSave :public InspectionTarget_StageInfoCollect_Base
{
  protected:
  TSQueue< std::vector< std::shared_ptr<StageInfo> > > datTransferQueue;


  std::thread runThread;
  public:

  
  InspectionTarget_GroupResultSave(std::string id,cJSON* def,InspectionTargetManager* belongMan,std::string local_env_path):
    InspectionTarget_StageInfoCollect_Base(id,def,belongMan,local_env_path),
    datTransferQueue(10),runThread(&InspectionTarget_GroupResultSave::thread_run,this)
    {}
  static std::string TYPE(){ return "GroupResultSave"; }
  void processGroup(int trigger_id,std::vector< std::shared_ptr<StageInfo> > group)
  {
    
    // if(datTransferQueue.push(group)==false)
    // {
    //   LOGI("datTransferQueue.push FAILED.....");
    // }

  }

  void thread_run();

  ~InspectionTarget_GroupResultSave()
  {
    datTransferQueue.termination_trigger();
    runThread.join();
    
  }
  

};


*/