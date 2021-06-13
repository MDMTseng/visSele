#ifndef IMG_SAMPLER_HPP
#define IMG_SAMPLER_HPP


#include <acvImage.hpp>
#include <acvImage_BasicTool.hpp>
#include <acvImage_SpDomainTool.hpp>


typedef struct angledOffsetG{
  float angle_rad;
  acv_XY angle_vec;
  float offset;
}angledOffsetG;

class angledOffsetTable
{
  public:
  std::vector<angledOffsetG> table;
  bool sorted=false;
  float preOffset=0;

  public:
  int RESET();
  int size();

  int findRange(float angle_rad);

  int findRange(acv_XY Vec);
  float sampleAngleOffset(float angle_rad);

  float sampleAngleOffset(acv_XY Vec);
  int find(float angle_rad);
  
  void push_back(angledOffsetG aog);

  void makeSymmetic();
  
  void applyPreOffset(float pOffset);
  void sort();
  angledOffsetTable* CLONE();
  int CLONE(angledOffsetTable*);
  static int CLONE(angledOffsetTable* dst,angledOffsetTable* src);
};


typedef struct BGLightNodeInfo{
  struct index{
    int X;
    int Y;
  }index;
  acv_XY location;
  float sigma;
  float mean;
  int error;
  float samp_rate;
}BGLightNodeInfo;
class stageLightParam{
  acv_XY origin_offset;  
  public:
  std::vector <BGLightNodeInfo> BG_nodes;
  std::vector <BGLightNodeInfo> BG_exnodes;
  float exposure_us;
  int tarImgW,tarImgH;
  int idxW,idxH;
  int back_light_target;
  int RESET();
  void nodesIdxWHSetup();
  void CLONE(stageLightParam* obj){};
  stageLightParam* CLONE();
  float factorSampling(acv_XY xy);
  BGLightNodeInfo* fetchIdx(int X, int Y);
};




class acvCalibMapUtil
{
    public:

    static float NumRatio(float a,float b,float ratio);
    static int sample_vec(float* map,int width,int height,float mapfX,float mapfY,float sampleXY[2]);

    //map_vec the vec is to find the local xy axis in the map
    static int map_vec(float* map,int width,int height,float mapfX,float mapfY,float xyVec[4], float *opt_det=NULL);
    static float locateMapPosition(float* map,int width,int height,float tar_x,float tar_y,float mapSeed_ret[2],float maxError=0.01,float stepSize=1,int iterC=5);

};
class acvCalibMap
{
  protected:
  
    acv_XY origin_offset;  
    float* fwdMap;
    //(x',y')=fwdMap(x,y) 
    //x,y is in calibrated coord, ie semi-ideal map
    //x',y' is the location from oriignal image to sample
    //the size of fwdMap might be downsized
    int downSizedMapW,downSizedMapH;
    //iw & ih is the dimension of fwdMap

    float* invMap;//it's to quickly sample the inverse map
    //(x,y)=fwdMap(x',y') 
    //x,y is in calibrated coord, ie semi-ideal map
    //x',y' is the location from oriignal image to sample
    int iw,ih;
    //iw & ih is the dimension of invMap
    float map_loca_scale=1;
    int downScale=1;
    float reMapMtx[4];
    public:
    float calibPpB=1;
    float calibmmpB=1;
    int fullFrameW,fullFrameH;
    //fullFrameW,fullFrameH is the dimension of original image
    acvCalibMap(double *MX_data, double *MY_data, int fw_,int fh_,int fullW,int fullH);
    acvCalibMap();
    void RESET();
    void SET(double *MX_data, double *MY_data, int fw_,int fh_,int fullW,int fullH);
    void CLEAR();
    void generateInvMap(int iw_,int ih_);
    float* generateExtInvMap(int iw_,int ih_);
    int fwdMapDownScale(int dscale_idx);
    void deleteInvMap();
    ~acvCalibMap();
    float get_PpB_ideal();
    float get_mmpP_ideal();

    void reMap(int type);
    int i2c(float coord[2],bool useInvMap=true);
    int c2i(float coord[2]);
        
    int i2c(acv_XY &coord,bool useInvMap=true);
    int c2i(acv_XY &coord);
    
    acvCalibMap* CLONE();
    int CLONE(acvCalibMap* obj);
    static int CLONE(acvCalibMap* dst,acvCalibMap* src);
};


class ImageSampler
{
  protected:
  bool _ignoreCorrdCalib=false;
  bool _ignoreStageLightCalib=false;
  bool _ignoreAngleOffset=false;
  
  acvCalibMap *map;
  angledOffsetTable *angOffsetTable;
  stageLightParam *stageLightInfo;
  acv_XY origin_offset={0,0};
  public:
  ImageSampler(){
    //map=new acvCalibMap();
    map=new acvCalibMap();
    angOffsetTable=new angledOffsetTable();
    stageLightInfo=new stageLightParam();
  }
  stageLightParam *getStageLightInfo(){return stageLightInfo;}
  angledOffsetTable *getAngOffsetTable(){return angOffsetTable;}
  acvCalibMap *getCalibMap(){return map;}
  void ignoreCalib(bool d)
  {
    ignoreCoordCalib(d);
    ignoreStageLightCalib(d);
    ignoreAngleOffset(d);
  }
  void ignoreCoordCalib(bool d)
  {
    _ignoreCorrdCalib=d;
  }
  void ignoreStageLightCalib(bool d)
  {
    _ignoreStageLightCalib=d;
  }
  void ignoreAngleOffset(bool d)
  {
    _ignoreAngleOffset=d;
  }

  float mmpP_ideal()
  {
    return map->get_mmpP_ideal();
  }

  void setOriginOffset(acv_XY _origin_offset)
  {
    origin_offset=_origin_offset;
  }
  
  void RESET()
  { 
    ignoreCalib(false);
    map->RESET();
    angOffsetTable->RESET();
    stageLightInfo->RESET();
  }

  int img2ideal(acv_XY *distortedVec)
  {
    if(_ignoreCorrdCalib)return 0;
    distortedVec->X += origin_offset.X;
    distortedVec->Y += origin_offset.Y;
    int ret = map->c2i(*distortedVec);
    distortedVec->X -= origin_offset.X;
    distortedVec->Y -= origin_offset.Y;
    return ret;
  }



  int img2ideal(float distortedVec[2])
  {
    if(_ignoreCorrdCalib)return 0;
    distortedVec[0] += origin_offset.X;
    distortedVec[1] += origin_offset.Y;
    int ret = map->c2i(distortedVec);
    distortedVec[0] -= origin_offset.X;
    distortedVec[1] -= origin_offset.Y;
    return ret;
  }

  int ideal2img(acv_XY *idealVec)
  {
    
    if(_ignoreCorrdCalib)return 0;
    idealVec->X += origin_offset.X;
    idealVec->Y += origin_offset.Y;
    int ret =  map->c2i(*idealVec);
    idealVec->X -= origin_offset.X;
    idealVec->Y -= origin_offset.Y;
    return ret;
  }

  int ideal2img(float idealVec[2])
  {
    if(_ignoreCorrdCalib)return 0;
    
    idealVec[0] += origin_offset.X;
    idealVec[1] += origin_offset.Y;

    int ret = map->i2c(idealVec);
    idealVec[0] -= origin_offset.X;
    idealVec[1] -= origin_offset.Y;
    return ret;
  }
  float sampleBackLightFactor_ImgCoord(acv_XY pos)
  {
    if(_ignoreStageLightCalib || stageLightInfo->exposure_us!=stageLightInfo->exposure_us)
      return 1;

    return stageLightInfo->back_light_target/stageLightInfo->factorSampling(pos);
  }


  float sampleImage_IdealCoord(acvImage *img,float idealVec[2],int doNearest=1)
  {
    int ret = ideal2img(idealVec);
    float sampPix=sampleImage_ImgCoord(img, idealVec,doNearest);
    return sampPix;
  }
  float sampleImage_IdealCoord(acvImage *img,acv_XY pos,int doNearest=1)
  {
    int ret = ideal2img(&pos);
    return sampleImage_ImgCoord(img, pos,doNearest);
  }

  float sampleImage_ImgCoord(acvImage *img,acv_XY pos,int doNearest=1)
  {
    //float bri = acvUnsignedMap1Sampling(img, pos,0);
    float bri;
    
    if(doNearest==1)
      bri= acvUnsignedMap1Sampling_Nearest(img, pos,0);
    else if (doNearest==0)
      bri= acvUnsignedMap1Sampling(img, pos,0);
    // if(CCC%3001==0)
    // {
    //   LOGI("pos:%f,%f offset:%f,%f",pos.X,pos.Y,origin_offset.X,origin_offset.Y);
    // }CCC++;
    pos=acvVecAdd(pos,origin_offset);
    return bri*sampleBackLightFactor_ImgCoord(pos);
  }
  float sampleImage_ImgCoord(acvImage *img,float imgPos[2],int doNearest=1)
  {
    acv_XY pos={imgPos[0],imgPos[1]};
    return sampleImage_ImgCoord(img,pos,doNearest);
  }

  float sampleAngleOffset(float angle)
  {
    if(_ignoreAngleOffset)return 0;
    return angOffsetTable->sampleAngleOffset(angle);
  }
};


#endif