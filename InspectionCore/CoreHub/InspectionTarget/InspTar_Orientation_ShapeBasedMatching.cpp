
#include "InspTar_Orientation.hpp"


using namespace cv;

using namespace std;


template<typename Base, typename T> inline bool instanceof(const T) {
   return is_base_of<Base, T>::value;
}

InspectionTarget_Orientation_ShapeBasedMatching::InspectionTarget_Orientation_ShapeBasedMatching(string id,cJSON* def,InspectionTargetManager* belongMan)
  :InspectionTarget(id,def,belongMan)
{
  type=InspectionTarget_Orientation_ShapeBasedMatching::TYPE();


  sbm=new SBM_if(60, {4,6,12},30,80);

}

bool InspectionTarget_Orientation_ShapeBasedMatching::stageInfoFilter(shared_ptr<StageInfo> sinfo)
{
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

future<int> InspectionTarget_Orientation_ShapeBasedMatching::futureInputStagePool()
{
  return async(launch::async,&InspectionTarget_Orientation_ShapeBasedMatching::processInputStagePool,this);
}

int InspectionTarget_Orientation_ShapeBasedMatching::processInputPool()
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


cJSON* TemplateFeature2Json(line2Dup::Feature &feat)
{
  cJSON* jfeat=cJSON_CreateObject();
  cJSON_AddNumberToObject(jfeat,"label",feat.label);
  cJSON_AddNumberToObject(jfeat,"theta",feat.theta);
  cJSON_AddNumberToObject(jfeat,"x",feat.x);
  cJSON_AddNumberToObject(jfeat,"y",feat.y);

  return jfeat;
}

cJSON* Template2Json(line2Dup::Template &templ)
{
  cJSON* jtempl=cJSON_CreateObject();
  cJSON_AddNumberToObject(jtempl,"angle",templ.angle);
  cJSON_AddNumberToObject(jtempl,"tl_x",templ.tl_x);
  cJSON_AddNumberToObject(jtempl,"tl_y",templ.tl_y);
  cJSON_AddNumberToObject(jtempl,"height",templ.height);
  cJSON_AddNumberToObject(jtempl,"width",templ.width);
  cJSON_AddNumberToObject(jtempl,"pyramid_level",templ.pyramid_level);

  cJSON* jfeats=cJSON_CreateArray();
  cJSON_AddItemToObject(jtempl,"features",jfeats);
  for(int i=0;i<templ.features.size();i++)
  {
    cJSON_AddItemToArray(jfeats,TemplateFeature2Json(templ.features[i]));
  }
  return jtempl;
}

cJSON* TemplatePyramid2Json(line2Dup::TemplatePyramid &tp)
{
  cJSON* jtpArr=cJSON_CreateArray();
  for(int i=0;i<tp.size();i++)
  {
    cJSON_AddItemToArray(jtpArr,Template2Json(tp[i]));
  }

  return jtpArr;
}





line2Dup::TemplatePyramid Json2TemplatePyramid(cJSON* json)
{
  
}


bool InspectionTarget_Orientation_ShapeBasedMatching::exchangeCMD(cJSON* info,int id,exchangeCMD_ACT &act)
{
  //LOGI(">>>>>>>>>>>>");
  bool ret = InspectionTarget::exchangeCMD(info,id,act);//apply framework layer exchange
  if(ret)return ret;
  string type=JFetch_STRING_ex(info,"type");

  if(type=="extract_feature")
  {
    string path=JFetch_STRING_ex(info,"image_path");
    if(path=="")
    {
      return NULL;
    }
    Mat img = imread(path, IMREAD_COLOR);
    if (img.empty())
    {
      return NULL;
    }


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
      acvImage src_acvImg;
      src_acvImg.useExtBuffer((BYTE *)img.data,img.rows*img.cols*3,img.cols,img.rows);
      act.send("IM",id,&src_acvImg,10);
    }

    int feature_count= JFetch_NUMBER_ex(info,"feature_count",60);

    if(feature_count>0)
    {

      Mat _mask;
      cJSON *mask_regions=JFetch_ARRAY(info,"mask_regions");
      if(mask_regions)
      {

        int reg_size=cJSON_GetArraySize(mask_regions);
        if(reg_size==0)
        {
          _mask = Mat(img.size(), CV_8UC1,{255});//default all white region
        }
        else
        {
          _mask = Mat::zeros(img.rows, img.cols, CV_8UC1);//Mat(img.size(), CV_8UC1,{0});//default all black region
          for (int i = 0 ; i <reg_size ; i++)
          {
            cJSON * regionInfo = cJSON_GetArrayItem(mask_regions, i);
            bool isBlackRegion=JFetch_TRUE(regionInfo,"isBlackRegion");


            double x=JFetch_NUMBER_ex(regionInfo, "x");
            double y=JFetch_NUMBER_ex(regionInfo, "y");
            double w=JFetch_NUMBER_ex(regionInfo, "w");
            double h=JFetch_NUMBER_ex(regionInfo, "h");

            LOGI("ROI: %f,%f,%f,%f<<<<",x,y,w,h);
            if(x==x && y==y && w==w && h==h)
            {
              _mask(Rect(x,y,w,h)) = isBlackRegion?0:255;
            }
          }
        }

      }
      else
      {
        _mask = Mat(img.size(), CV_8UC1,{255});//default all white region
      }


      

      
      line2Dup::TemplatePyramid tp;
      //1087,231   1596,666
      sbm->TemplateFeatureExtraction(img,_mask,JFetch_NUMBER_ex(info,"feature_count",60),tp);


      cJSON* jtp=TemplatePyramid2Json(tp);

      act.send("RP",id,jtp);
      cJSON_Delete(jtp);jtp=NULL;
    }

    return true;

  }

  return false;
}


cJSON* InspectionTarget_Orientation_ShapeBasedMatching::genITIOInfo()
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

void InspectionTarget_Orientation_ShapeBasedMatching::singleProcess(shared_ptr<StageInfo> sinfo)
{


  LOGI(">>>>>>>>InspectionTarget_Orientation_ShapeBasedMatching>>>>>>>>");
}

InspectionTarget_Orientation_ShapeBasedMatching::~InspectionTarget_Orientation_ShapeBasedMatching()
{
  delete sbm;
}
