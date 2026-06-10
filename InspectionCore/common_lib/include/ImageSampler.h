#ifndef IMG_SAMPLER_HPP
#define IMG_SAMPLER_HPP


#include <acvImage.hpp>
#include <acvImage_BasicTool.hpp>
#include "vis_geom.h"
#include <opencv2/core.hpp>
#include <cmath>


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
    int x;
    int y;
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
  void nodesUpdate();
  void CLONE(stageLightParam* obj){};
  stageLightParam* CLONE();
  float factorSampling(acv_XY xy);
  BGLightNodeInfo* fetchIdx(int X, int Y);
  float calcInterpolation(int X,int Y,BGLightNodeInfo *newNodeInfo);
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
    bool isPresent();
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
  acv_XY getOriginOffset() const { return origin_offset; }
  
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
    distortedVec->x += origin_offset.x;
    distortedVec->y += origin_offset.y;
    int ret = map->c2i(*distortedVec);
    distortedVec->x -= origin_offset.x;
    distortedVec->y -= origin_offset.y;
    return ret;
  }



  int img2ideal(float distortedVec[2])
  {
    if(_ignoreCorrdCalib)return 0;
    distortedVec[0] += origin_offset.x;
    distortedVec[1] += origin_offset.y;
    int ret = map->c2i(distortedVec);
    distortedVec[0] -= origin_offset.x;
    distortedVec[1] -= origin_offset.y;
    return ret;
  }

  int ideal2img(acv_XY *idealVec)
  {
    
    if(_ignoreCorrdCalib)return 0;
    idealVec->x += origin_offset.x;
    idealVec->y += origin_offset.y;
    int ret =  map->c2i(*idealVec);
    idealVec->x -= origin_offset.x;
    idealVec->y -= origin_offset.y;
    return ret;
  }

  int ideal2img(float idealVec[2])
  {
    if(_ignoreCorrdCalib)return 0;
    
    idealVec[0] += origin_offset.x;
    idealVec[1] += origin_offset.y;

    int ret = map->i2c(idealVec);
    idealVec[0] -= origin_offset.x;
    idealVec[1] -= origin_offset.y;
    return ret;
  }
  float sampleBackLightFactor_ImgCoord(acv_XY pos)
  {
    if(_ignoreStageLightCalib || stageLightInfo->exposure_us!=stageLightInfo->exposure_us)
      return 1;

    return stageLightInfo->back_light_target/stageLightInfo->factorSampling(pos);
  }

  // True when a sample reduces to a plain pixel fetch -- no distortion remap and
  // no backlight scaling -- so a downsample can use cv::resize instead of the
  // per-pixel sampler. Mirrors the early-outs in ideal2img + sampleBackLightFactor.
  // Non-const because acvCalibMap::isPresent() is non-const.
  bool samplingIsIdentity()
  {
    bool coordRemap = !_ignoreCorrdCalib && map && map->isPresent();
    bool backlight  = !_ignoreStageLightCalib && stageLightInfo &&
                      (stageLightInfo->exposure_us == stageLightInfo->exposure_us); // not NaN
    return !coordRemap && !backlight;
  }


  // cv::Mat-native samplers. ideal2img coord transform + bilinear/nearest
  // sample + backlight factor.
  float sampleImage_ImgCoord_cv(const cv::Mat &m, acv_XY pos, int doNearest=1, int ch=0)
  {
    float bri = 0;
    int rX = (int)pos.x, rY = (int)pos.y;
    if (doNearest == 1)
    {
      rX = (int)std::round(pos.x); rY = (int)std::round(pos.y);
      if (rX < 0 || rY < 0 || rX > m.cols - 1 || rY > m.rows - 1) bri = 0;
      else bri = m.ptr<uint8_t>(rY)[rX * m.channels() + ch];
    }
    else
    {
      float resX = pos.x - rX, resY = pos.y - rY;
      if (rX < 0 || rY < 0 || rX + 1 > m.cols - 1 || rY + 1 > m.rows - 1) bri = std::nan("");
      else
      {
        const int chN = m.channels();
        const uint8_t *r0 = m.ptr<uint8_t>(rY), *r1 = m.ptr<uint8_t>(rY + 1);
        float c00 = r0[rX * chN + ch], c01 = r0[(rX + 1) * chN + ch];
        float c10 = r1[rX * chN + ch], c11 = r1[(rX + 1) * chN + ch];
        c00 += resX * (c01 - c00);
        c10 += resX * (c11 - c10);
        c00 += resY * (c10 - c00);
        bri = c00;
      }
    }
    pos = acvVecAdd(pos, origin_offset);
    return bri * sampleBackLightFactor_ImgCoord(pos);
  }
  int sampleImage3_IdealCoord(const cv::Mat &m, float imgPos[2], float ret_RGB[3], int doNearest=1)
  {
    ideal2img(imgPos);
    acv_XY pos = { imgPos[0], imgPos[1] };
    ret_RGB[0] = sampleImage_ImgCoord_cv(m, pos, doNearest, 0);
    ret_RGB[1] = sampleImage_ImgCoord_cv(m, pos, doNearest, 1);
    ret_RGB[2] = sampleImage_ImgCoord_cv(m, pos, doNearest, 2);
    return 0;
  }
  // Single-channel variant: one ideal2img + one bilinear+backlight sample.
  // For a gray working image this is ~3x less work than sampleImage3.
  float sampleImage1_IdealCoord(const cv::Mat &m, float imgPos[2], int doNearest=1)
  {
    ideal2img(imgPos);
    acv_XY pos = { imgPos[0], imgPos[1] };
    return sampleImage_ImgCoord_cv(m, pos, doNearest, 0);
  }

  float sampleAngleOffset(float angle)
  {
    if(_ignoreAngleOffset)return 0;
    return angOffsetTable->sampleAngleOffset(angle);
  }
};


#endif