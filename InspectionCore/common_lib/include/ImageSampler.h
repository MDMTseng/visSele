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
  public:
  std::vector <BGLightNodeInfo> BG_nodes;
  std::vector <BGLightNodeInfo> BG_exnodes;
  float exposure_us;
  int tarImgW,tarImgH;
  int idxW,idxH;
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

    static int map_vec(float* map,int width,int height,float mapfX,float mapfY,float xyVec[4], float *opt_det=NULL);
    static float locateMapPosition(float* map,int width,int height,float tar_x,float tar_y,float mapSeed_ret[2],float maxError=0.01,int iterC=5);

};
class acvCalibMap
{
    float* fwdMap;
    //the xy idx is in corrected mapping
    //fwdMap(x,y) is the xy coord from original image
    int downSizedMapW,downSizedMapH;
    float* invMap;
    int iw,ih;

    int downScale=1;
    float fmapScale=1;
    public:
    int fullFrameW,fullFrameH;
    acv_XY origin_offset;  
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
  public:
  acvCalibMap *map;
  float mmpp;
  angledOffsetTable *angOffsetTable;
  stageLightParam *stageLightInfo;
  ImageSampler(){
    //map=new acvCalibMap();
    map=new acvCalibMap();
    angOffsetTable=new angledOffsetTable();
    stageLightInfo=new stageLightParam();
  }
  
  void RESET()
  { 
    map->RESET();
    angOffsetTable->RESET();
    stageLightInfo->RESET();
  }

  int img2ideal(acv_XY *distortedVec)
  {
    return map->i2c(*distortedVec);
  }



  int img2ideal(float distortedVec[2])
  {
    return map->i2c(distortedVec);
  }

  int ideal2img(acv_XY *idealVec)
  {
    return map->c2i(*idealVec);
  }

  int ideal2img(float idealVec[2])
  {
    return map->c2i(idealVec);
  }

  float sampleImage_IdealCoord(acvImage *img,float idealVec[2])
  {
    int ret = ideal2img(idealVec);
    acv_XY xy={X:idealVec[0],Y:idealVec[1]};
    float f=stageLightInfo->factorSampling(xy);
    return sampleImage_ImgCoord(img, idealVec)*200/f;
  }
  float sampleImage_IdealCoord(acvImage *img,acv_XY pos)
  {
    int ret = ideal2img(&pos);
    return sampleImage_ImgCoord(img, pos);
  }

  float sampleImage_ImgCoord(acvImage *img,acv_XY pos)
  {
    float bri = acvUnsignedMap1Sampling_Nearest(img, pos,0);

    return bri;
  }
  float sampleImage_ImgCoord(acvImage *img,float imgPos[2])
  {
    acv_XY pos={imgPos[0],imgPos[1]};
    return sampleImage_ImgCoord(img,pos);
  }


};


#endif