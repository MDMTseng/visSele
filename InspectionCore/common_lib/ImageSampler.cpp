#include <acvImage_BasicTool.hpp>
#include <ImageSampler.h>
#include <vector>
#include <algorithm>
#include "logctrl.h"

//acvCalibMap
acvCalibMap::acvCalibMap(double *MX_data, double *MY_data,
                         int downSizedMapW, int downSizedMapH, int fullFrameW, int fullFrameH)
{
  acvCalibMap();
  RESET();
  SET(MX_data, MY_data, downSizedMapW, downSizedMapH, fullFrameW, fullFrameH);
}
//acvCalibMap
acvCalibMap::acvCalibMap()
{

  fwdMap = NULL;
  //the xy idx is in corrected mapping
  //fwdMap(x,y) is the xy coord from original image
  downSizedMapW = downSizedMapH = 0;
  invMap = NULL;
  iw = ih = 0;

  downScale = 1;
  //fmapScale = 1;
  calibPpB=1;
}

void acvCalibMap::RESET()
{
  CLEAR();
}
//acvCalibMap

//downSizedMapWH is the Map WH, it's usually smaller than fullFrameW(full original dim)
void acvCalibMap::SET(double *MX_data, double *MY_data,
                      int downSizedMapW, int downSizedMapH, int fullFrameW, int fullFrameH)
{
  CLEAR();
  this->downSizedMapW = downSizedMapW;
  this->downSizedMapH = downSizedMapH;
  this->fullFrameW = fullFrameW;
  this->fullFrameH = fullFrameH;
  origin_offset.X = 0;
  origin_offset.Y = 0;
  invMap = NULL;
  int pixCount = downSizedMapW * downSizedMapH;
  fwdMap = new float[downSizedMapW * downSizedMapH * 2];

  acvImage img(downSizedMapW, downSizedMapH, 3);
  for (int i = 0; i < pixCount; i++)
  {
    fwdMap[i * 2] = (float)MX_data[i];
    fwdMap[i * 2 + 1] = (float)MY_data[i];
    int x = i % downSizedMapW, y = i / downSizedMapW;
    // if( (x==0&&y==0)||(x==downSizedMapW-1&&y==downSizedMapH-1)  )
    // {
    //
    // }

    // if(fwdMap[i*2]>0 && fwdMap[i*2+1]>0 &&
    //   fwdMap[i*2]<fullFrameW &&
    //   fwdMap[i*2+1]<fullFrameH
    //   )
    // {
    //   img.CVector[y][x*3]=(int)fwdMap[i*2];
    //   img.CVector[y][x*3+1]=(int)fwdMap[i*2+1];
    //   img.CVector[y][x*3+2]=255;
    // }
    // else
    //   img.CVector[y][x*3]=img.CVector[y][x*3+1]=img.CVector[y][x*3+2]=0;
    // if((x%10)==0&&(y%10)==0)
    // {
    //   if(x==0)printf("\nY:%d\n",y);
    //   if(x%100==0)printf("\nX:%d \n",x);
    //   if(fwdMap[i*2]>0 && fwdMap[i*2+1]>0)
    //   printf("%4.1f,%4.1f_  ",fwdMap[i*2],fwdMap[i*2+1]);
    // }
  }
  reMap(1);
  
  // acvSaveBitmapFile("data/ddddd.bmp", &img);
  // exit(0);
  // printf("\n");

  //Find the relative scale factor(fmapScale) between down sized map to real image space
  //pa is for sparse sampling factor, just to accelerate compute process,
  //the absolute accuracy isn't crucial.
  // int pa = 4;
  // int detC = 0;
  // float detSum = 0;
  // for (int i = downSizedMapH / pa; i < downSizedMapH * (pa - 1) / pa; i++)
  // {
  //   for (int j = downSizedMapW / pa; j < downSizedMapW * (pa - 1) / pa; j++)
  //   {
  //     float xyVec[4];
  //     float det;
  //     int ret = acvCalibMapUtil::map_vec(fwdMap, downSizedMapW, downSizedMapH, j, i, xyVec, &det);
  //     if (ret == 0)
  //     {
  //       detSum += det;
  //       detC++;
  //     }
  //   }
  // }
  // float aveDet = detSum / detC;
  // fmapScale = sqrt(aveDet);
  //fmapScale=1;
  //fmapScale is to find a mapping searching jump factor,
  //the search will start from image center then compute real jumping distance = fmapScale*distance
}

float acvCalibMap::get_PpB_ideal()//Pixels per Block
{
  return map_loca_scale*calibPpB;
}

float acvCalibMap::get_mmpP_ideal()//mm per pix
{
  return calibmmpB/get_PpB_ideal();
}


void acvCalibMap::reMap(int type)
{
  switch(type)
  {
    case 1:
    {

      //Now we gonna find the size scale between calib map pixel  and original pixel 
      //it's to find how can we generate a complete corrected image under different sized calib map(usually downscaled)
      //
      /*  
      For example, 
      [O]original image dim: 500x500
      [C]calib image dim:100x100 
      the relationship between O and C isn't just 1/5
      because the calib map may include certain scale factor and out of image pixels(by unwarping)
      therefore we need a simple way to grasp the approx pixel size scale between O and C

      in here, we have two position OP_00 and OP_WH on calib map 
      and find CP_00=inv_calib_map(OP_00) ,CP_WH=inv_calib_map(OP_HW) 
      and the scale is 
      scale=|OP_HW-OP_00|/|CP_WH-CP_00|;

      the reason why I do this is to let calib user have a more or less similar position scale, 
      which is not nessasary logically but to make calibrated image easier
      */
      float shrinkScale=0.8;
      float mapLoc_00[2]={NAN};
      acv_XY start={fullFrameW*(1-shrinkScale)/2,fullFrameH*(1-shrinkScale)/2};
      acv_XY end={fullFrameW*(1-(1-shrinkScale)/2),fullFrameH*(1-(1-shrinkScale)/2)};
      acvCalibMapUtil::locateMapPosition(fwdMap, downSizedMapW, downSizedMapH, start.X,start.Y, mapLoc_00,0.0001,1, 200);

      float mapLoc_WH[2]={NAN};
      acvCalibMapUtil::locateMapPosition(fwdMap, downSizedMapW, downSizedMapH, end.X,end.Y, mapLoc_WH,0.0001,1, 200);
      //Just to get 00 and W0
      //Diagnal scale
      //(0,0)o
      //      \
      //       \
      //        \
      //         \
      //          o(W,H)
      map_loca_scale= hypot(fullFrameW,fullFrameH)*(shrinkScale)/hypot(mapLoc_WH[0] - mapLoc_00[0], mapLoc_WH[1] - mapLoc_00[1]);
    }
    break;
    default: 
      map_loca_scale=1;

  }
}
void acvCalibMap::generateInvMap(int iw_, int ih_)
{
  invMap = generateExtInvMap(iw_, ih_);
  iw = iw_;
  ih = ih_;
}

float *acvCalibMap::generateExtInvMap(int iw_, int ih_)
{

  float *_invMap = new float[iw_ * ih_ * 2];

  int missCount = 0;
  float errSum = 0;

  float mapSeed_row[2] = {downSizedMapW / 2.0f, downSizedMapH / 2.0f};
  for (int i = 0; i < ih_; i++)
  {
    float mapSeed_ret[2] = {mapSeed_row[0], mapSeed_row[1]};
    for (int j = 0; j < iw_; j++)
    {
      int idx = i * iw_ + j;
      if (mapSeed_ret[0] != mapSeed_ret[0]) //NAN
      {
        mapSeed_ret[0] = downSizedMapW / 2.0f;
        mapSeed_ret[1] = downSizedMapH / 2.0f;
      }
      float tarLoc[2] = {(float)j, (float)i};
      float error = acvCalibMapUtil::locateMapPosition(fwdMap, downSizedMapW, downSizedMapH, tarLoc[0], tarLoc[1], mapSeed_ret);

      if (j == 0)
      {
        mapSeed_row[0] = mapSeed_ret[0];
        mapSeed_row[1] = mapSeed_ret[1];
      }
      if (error > 0.02 || error != error)
      {
        _invMap[idx * 2 + 1] =
            _invMap[idx * 2] = NAN;
        missCount++;
      }
      else
      {
        errSum += error;
        _invMap[idx * 2] = mapSeed_ret[0];
        _invMap[idx * 2 + 1] = mapSeed_ret[1];
      }
    }
  }
  return _invMap;
}
int acvCalibMap::fwdMapDownScale(int dscale_idx)
{
  if (dscale_idx < 0)
    return -1;
  if (dscale_idx == 0)
    return 0;

  int dscale = 1 << dscale_idx;
  int dfw = downSizedMapW / dscale;
  int dfh = downSizedMapH / dscale;
  float *dfwdMap = new float[dfw * dfh * 2];
  for (int di = 0; di < dfh; di++)
  {
    int i = di * dscale;
    for (int dj = 0; dj < dfw; dj++)
    {
      int j = dj * dscale;
      int didx = di * dfw + dj;
      int idx = i * downSizedMapW + j;
      dfwdMap[didx * 2] = fwdMap[idx * 2];
      dfwdMap[didx * 2 + 1] = fwdMap[idx * 2 + 1];
    }
  }

  delete fwdMap;
  fwdMap = dfwdMap;
  downSizedMapW = dfw;
  downSizedMapH = dfh;
  downScale *= dscale;

  deleteInvMap();
  return 0;
}

void acvCalibMap::deleteInvMap()
{
  if (invMap)
  {
    delete invMap;
    invMap = NULL;
  }
}
acvCalibMap::~acvCalibMap()
{
  CLEAR();
}
void acvCalibMap::CLEAR()
{
  deleteInvMap();
  if (fwdMap)
  {
    delete fwdMap;
    fwdMap = NULL;
  }
}
int acvCalibMap::i2c(float coord[2], bool useInvMap) //real image coord to calibrated coord
{

  if (fwdMap==NULL && invMap==NULL)//In unset state
  {
    return 0;//No mapping would be performed
  }
  // if (downSizedMapW + downSizedMapH == 0)
  // {
  //   return 0;
  // }
  int ret;

  coord[0] += origin_offset.X;
  coord[1] += origin_offset.Y;
  if (invMap && useInvMap)
  {
    ret = acvCalibMapUtil::sample_vec(invMap, iw, ih, coord[0], coord[1], coord);
  }
  else
  {
    ret = 0;
    float x = coord[0], y = coord[1];
    // coord[0] = coord[0]*downSizedMapW/fullFrameW;
    // coord[1] = coord[0]*downSizedMapH/fullFrameH;
    coord[0]=coord[1]=NAN;
    //printf("----: %f %f\n",x,y);
    float error = acvCalibMapUtil::locateMapPosition(fwdMap, downSizedMapW, downSizedMapH, x, y, coord);
    //printf("----: %f %f\n",coord[0],coord[1]);
    if (error > 0.01 || error != error)
      ret = -1;
  }

  if (ret == 0)
  {
    coord[0] *= downScale*map_loca_scale;
    coord[1] *= downScale*map_loca_scale;
  }
  else
  {
    coord[0] =
        coord[1] = NAN;
  }
  return ret;
}

int acvCalibMap::i2c(acv_XY &coord, bool useInvMap)
{
  float _coord[2] = {coord.X, coord.Y};
  int ret = i2c(_coord, useInvMap);
  coord.X = _coord[0];
  coord.Y = _coord[1];
  return ret;
}
// int XX=0;
int acvCalibMap::c2i(float coord[2]) //calibrated coord to real image coord
{
  // if( (XX%1000)==0)
  // {
  //   printf("1:%d");
  // }
  // XX++;


  if (fwdMap==NULL && invMap==NULL)//In unset state
  {
    return 0;//No mapping would be performed
  }

  if (downSizedMapW + downSizedMapH == 0)
  {
    return 0;
  }
  //printf("\ndownScale:%d fmapScale:%f\n",downScale,fmapScale);
  coord[0] /= downScale*map_loca_scale;
  coord[1] /= downScale*map_loca_scale;
  int ret = acvCalibMapUtil::sample_vec(fwdMap, downSizedMapW, downSizedMapH, coord[0], coord[1], coord);
  coord[0] -= origin_offset.X;
  coord[1] -= origin_offset.Y;
  return ret;
}
int acvCalibMap::c2i(acv_XY &coord)
{
  float _coord[2] = {coord.X, coord.Y};
  int ret = c2i(_coord);
  coord.X = _coord[0];
  coord.Y = _coord[1];
  return ret;
}

acvCalibMap *acvCalibMap::CLONE()
{
  acvCalibMap *obj = new acvCalibMap();
  int ret = CLONE(obj);
  if (ret)
  {
    delete obj;
    return NULL;
  }
  return obj;
}
int acvCalibMap::CLONE(acvCalibMap *obj)
{
  return CLONE(obj, this);
}
int acvCalibMap::CLONE(acvCalibMap *dst, acvCalibMap *src)
{
  if (dst == NULL || src == NULL)
    return -1;
  dst->RESET();
  {
    dst->fwdMap = new float[src->downSizedMapW * src->downSizedMapH * 2];
    memcpy(dst->fwdMap, src->fwdMap,
           sizeof(dst->fwdMap[0]) * src->downSizedMapW * src->downSizedMapH * 2);
    dst->downSizedMapW = src->downSizedMapW;
    dst->downSizedMapH = src->downSizedMapH;
  }

  {
    dst->invMap = new float[src->iw * src->ih * 2];
    memcpy(dst->invMap, src->invMap,
           sizeof(dst->invMap[0]) * src->iw * src->ih * 2);
    dst->iw = src->iw;
    dst->ih = src->ih;
  }
  dst->calibPpB = src->calibPpB;
  // dst->downScale = src->downScale;
  // dst->fmapScale = src->fmapScale;

  dst->fullFrameH = src->fullFrameH;
  dst->fullFrameW = src->fullFrameW;
  dst->origin_offset = src->origin_offset;
}

float acvCalibMapUtil::NumRatio(float a, float b, float ratio)
{
  return a + ratio * (b - a);
}

int acvCalibMapUtil::sample_vec(float *map, int width, int height, float mapfX, float mapfY, float sampleXY[2])
{

  sampleXY[0] = sampleXY[1] = NAN;
  if (mapfX > width - 2 || mapfY > height - 2 || mapfX < 0 || mapfY < 0 || mapfX!=mapfX || mapfY!=mapfY)
  {

    return -1;
  }

  //printf("_______%d %d %f %f %f %f\n",width,height,mapfX,mapfY,sampleXY[0],sampleXY[1]);
  int mapX = mapfX;
  int mapY = mapfY;

  float ratioX = mapfX - mapX;
  float ratioY = mapfY - mapY;

  int idxLT = (mapY)*width + mapX;
  int idxRT = idxLT + 1;
  int idxLB = idxLT+ width;
  int idxRB = idxLB + 1;
  /*
    LT  RT
    
    LB  RB
    */

  float v1[2] = {NumRatio(map[idxLT * 2], map[idxRT * 2], ratioX),
                 NumRatio(map[idxLT * 2 + 1], map[idxRT * 2 + 1], ratioX)};
  float v2[2] = {NumRatio(map[idxLB * 2], map[idxRB * 2], ratioX),
                 NumRatio(map[idxLB * 2 + 1], map[idxRB * 2 + 1], ratioX)};

  sampleXY[0] = NumRatio(v1[0], v2[0], ratioY);
  sampleXY[1] = NumRatio(v1[1], v2[1], ratioY);

  return 0;
}

int acvCalibMapUtil::map_vec(float *map, int width, int height, float mapfX, float mapfY, float xyVec[4], float *opt_det)
{
  if (mapfX > width - 2)
    mapfX = width - 2;
  if (mapfY > height - 2)
    mapfY = height - 2;
  if (mapfX < 0)
    mapfX = 0;
  if (mapfY < 0)
    mapfY = 0;

  int mapX = mapfX;
  int mapY = mapfY;

  float ratioX = mapfX - mapX;
  float ratioY = mapfY - mapY;

  int idxLT = (mapY)*width + mapX;
  int idxRT = (mapY)*width + mapX + 1;
  int idxLB = (mapY + 1) * width + mapX;
  int idxRB = (mapY + 1) * width + mapX + 1;
  /*
    
    LT  RT
    
    LB  RB
    */

  float v1[2] = {NumRatio(
                     map[idxRT * 2] - map[idxLT * 2],
                     map[idxRB * 2] - map[idxLB * 2],
                     ratioX),
                 NumRatio(
                     map[idxRT * 2 + 1] - map[idxLT * 2 + 1],
                     map[idxRB * 2 + 1] - map[idxLB * 2 + 1],
                     ratioX)};
  float v2[2] = {NumRatio(
                     map[idxLB * 2] - map[idxLT * 2],
                     map[idxRB * 2] - map[idxRT * 2],
                     ratioY),
                 NumRatio(
                     map[idxLB * 2 + 1] - map[idxLT * 2 + 1],
                     map[idxRB * 2 + 1] - map[idxRT * 2 + 1],
                     ratioY)};

  // LOGI("%f  %f",v1[0],v1[1]);
  // LOGI("%f  %f",v2[0],v2[1]);
  float det = v1[0] * v2[1] - v1[1] * v2[0];
  if (opt_det)
    *opt_det = det;
  float invMat[4] = {
      v2[1] / det, -v1[1] / det,
      -v2[0] / det, v1[0] / det};
  memcpy(xyVec, invMat, sizeof(invMat));
  return 0;
}

float acvCalibMapUtil::locateMapPosition(float *map, int width, int height, float tar_x, float tar_y, float mapSeed_ret[2], float maxError,float stepSize, int iterC)
{

  float sampleXY[2]={0};
  float x = tar_x, y = tar_y;
  float error;// = hypot(sampleXY[0] - x, sampleXY[1] - y);
  if(mapSeed_ret[0]!=mapSeed_ret[0])
    mapSeed_ret[0]=width/2;
  if(mapSeed_ret[1]!=mapSeed_ret[1])
    mapSeed_ret[1]=height/2;
    
  sample_vec(map, width, height, mapSeed_ret[0], mapSeed_ret[1], sampleXY);
  for (int i = 0; i < iterC; i++)
  {

    float xyVec[4];
    int ret = map_vec(map, width, height, mapSeed_ret[0], mapSeed_ret[1], xyVec);

    float diffXY[2] = {
        x - sampleXY[0],
        y - sampleXY[1]};
    float stepVec[2]={
      diffXY[0] * xyVec[0] + diffXY[1] * xyVec[2],
      diffXY[0] * xyVec[1] + diffXY[1] * xyVec[3]};

    int err;
    for(int j=0;j<iterC;j++)//Try current stepSize, if the step surpass the boundary, make step smaller and try again
    {
      float xx=mapSeed_ret[0]+stepVec[0]*stepSize;
      float yy=mapSeed_ret[1]+stepVec[1]*stepSize;
      err=sample_vec(map, width, height, xx,yy, sampleXY);

      //LOGI("xx:%f yy:%f err:%d",xx,yy,err);
      if(err==0)break;
      stepSize*=0.8;
    }

    mapSeed_ret[0] += stepVec[0]*stepSize;
    mapSeed_ret[1] += stepVec[1]*stepSize;

    error = hypot(sampleXY[0] - x, sampleXY[1] - y);
    if (error < maxError)
      return error;
    if (error != error)
    {
      mapSeed_ret[0] =
      mapSeed_ret[1] = NAN;
      return NAN;
    }
  }

  return error;
}

int angledOffsetTable::size()
{
  return table.size();
}

int angledOffsetTable::findRange(float angle_rad)
{
  sort();
  int curLen = size();
  for (int k = 0; k < curLen; k++)
  {
    if (table[k].angle_rad > angle_rad)
      return k - 1;
  }
  return curLen - 1;
}

int angledOffsetTable::RESET()
{
  table.clear();
  sorted = false;
  preOffset = 0;
  return 0;
}

int angledOffsetTable::findRange(acv_XY Vec)
{
  return -1; //TODO
  sort();
  int curLen = size();
  if (curLen == 0)
    return -1;
  acv_XY Vec_Nor = acvVecNormal(Vec);
  float pre_dotP_N = 0;
  float pre_dotP = 0;
  for (int k = 0; k < curLen; k++)
  {
    float dotP = acv2DDotProduct(Vec, table[k].angle_vec);
    if (dotP == 1)
    { //rare but possible
      return 0;
    }

    //if(dotP<0)continue;
    float dotP_N = acv2DDotProduct(Vec_Nor, table[k].angle_vec);
  }
  return curLen - 1;
}
float angledOffsetTable::sampleAngleOffset(acv_XY Vec)
{ //TODO
}

float angledOffsetTable::sampleAngleOffset(float angle_rad) //in rad
{
  if (size() == 0)
    return preOffset;
  sort();
  int subIdx = findRange(angle_rad);

  int topIdx = subIdx + 1;
  if (subIdx < 0 || topIdx == size())
  {
    subIdx = size() - 1;
    topIdx = 0;
  }

  angledOffsetG subG = table[subIdx];
  angledOffsetG topG = table[topIdx];

  if (subG.angle_rad > topG.angle_rad)
  { //It's in warp section
    subG.angle_rad -= M_PI * 2;
  }

  //Offset angle to sub based angle
  //example in deg 60,94,180 => 0, 34, 120
  topG.angle_rad -= subG.angle_rad;
  angle_rad -= subG.angle_rad;
  subG.angle_rad -= subG.angle_rad;
  if (angle_rad > M_PI * 2)
    angle_rad -= M_PI * 2;
  return subG.offset + (topG.offset - subG.offset) * (angle_rad / topG.angle_rad) + preOffset;
}

int angledOffsetTable::find(float angle)
{
  sort();
  int curLen = size();
  for (int k = 0; k < curLen; k++)
  {

    angledOffsetG newPair = table[k];
    if (newPair.angle_rad == angle)
      return k;
  }
  return -1;
}

void angledOffsetTable::push_back(angledOffsetG aog)
{
  sorted = false;
  aog.angle_vec.X = (float)cos(aog.angle_rad);
  aog.angle_vec.Y = (float)sin(aog.angle_rad);
  return table.push_back(aog);
}

void angledOffsetTable::applyPreOffset(float gOffset)
{
  preOffset = gOffset;
}

void angledOffsetTable::makeSymmetic()
{
  sorted = false;
  int curLen = size();
  for (int k = 0; k < curLen; k++)
  {

    angledOffsetG newPair = table[k];
    if (newPair.angle_rad >= M_PI)
      newPair.angle_rad -= M_PI;
    else
      newPair.angle_rad += M_PI;
    push_back(newPair);
  }
  sort();
}

void angledOffsetTable::sort()
{
  if (sorted)
    return;
  std::sort(table.begin(), table.end(),
            [](const angledOffsetG &a, const angledOffsetG &b) -> bool {
              return a.angle_rad < b.angle_rad;
            });
  sorted = true;
}

angledOffsetTable *angledOffsetTable::CLONE()
{
  angledOffsetTable *obj = new angledOffsetTable();
  int ret = CLONE(obj);
  if (ret)
  {
    delete obj;
    return NULL;
  }
  return obj;
}
int angledOffsetTable::CLONE(angledOffsetTable *clone_to)
{
  return CLONE(clone_to, this);
}

int angledOffsetTable::CLONE(angledOffsetTable *dst, angledOffsetTable *src)
{
  if (src == NULL || dst == NULL)
    return -1;
  dst->RESET();
  dst->table.assign(src->table.begin(), src->table.end());
  dst->sorted = src->sorted;
  dst->preOffset = src->preOffset;
  return 0;
}

int nodeInfoIdxCorrection(std::vector<BGLightNodeInfo> &infoArr, int targetIdx, int idxW)
{
}
void stageLightParam::nodesIdxWHSetup()
{

  printf("BG_nodes.size():%d\n", BG_nodes.size());
  //assume that node is always in board order
  idxW = -1;
  idxH = -1;
  for (BGLightNodeInfo &node : BG_nodes)
  {
    if (idxW < node.index.X)
      idxW = node.index.X;
    if (idxH < node.index.Y)
      idxH = node.index.Y;
  }

  idxW++;
  idxH++;
  BG_exnodes.clear();
  BG_exnodes.resize((idxW + 2) * (idxH + 2)); //expend

  for (int i = 0; i < idxH + 2; i++)
    for (int j = 0; j < idxW + 2; j++)
    {
      int eqIdx = j + i * (idxW + 2);
      BG_exnodes[eqIdx].mean = NAN; //Clear
    }

  for (BGLightNodeInfo &node : BG_nodes) //Fill data into correct location
  {
    if (node.mean != node.mean)
      continue;
    int exIdx = (node.index.X + 1) + (node.index.Y + 1) * (idxW + 2);
    BG_exnodes[exIdx] = node;
  }
  { //Fill out frame
    for (int i = 0; i < idxW; i++)
    {
      {
        BGLightNodeInfo *topBar = fetchIdx(i, -1);
        BGLightNodeInfo *t1 = fetchIdx(i, 0);
        BGLightNodeInfo *t2 = fetchIdx(i, 1);
        float alpha = -1;
        topBar->mean = t1->mean * (1 - alpha) + t2->mean * (alpha);
      }

      {
        BGLightNodeInfo *btnBar = fetchIdx(i, idxH);
        BGLightNodeInfo *t1 = fetchIdx(i, idxH - 1);
        BGLightNodeInfo *t2 = fetchIdx(i, idxH - 2);
        float alpha = -1;
        btnBar->mean = t1->mean * (1 - alpha) + t2->mean * (alpha);
      }
    }

    // for(int i=0;i<idxH+2;i++)for(int j=0;j<idxW+2;j++)
    // {
    //   if(j==0) printf("\n");
    //   int eqIdx= j+i*(idxW+2);
    //   printf("%3.1f  ",j-1,i-1,BG_exnodes[eqIdx].mean);
    // }

    // printf("\n");

    for (int i = -1; i < idxH + 1; i++)
    {
      {
        BGLightNodeInfo *leftBar = fetchIdx(-1, i);
        BGLightNodeInfo *t1 = fetchIdx(0, i);
        BGLightNodeInfo *t2 = fetchIdx(1, i);
        float alpha = -1;
        leftBar->mean = t1->mean * (1 - alpha) + t2->mean * (alpha);
      }

      {
        BGLightNodeInfo *rightBar = fetchIdx(idxW, i);
        BGLightNodeInfo *t1 = fetchIdx(idxW - 1, i);
        BGLightNodeInfo *t2 = fetchIdx(idxW - 2, i);
        float alpha = -1;
        rightBar->mean = t1->mean * (1 - alpha) + t2->mean * (alpha);
      }
    }

    // for(int i=0;i<idxH+2;i++)for(int j=0;j<idxW+2;j++)
    // {
    //   if(j==0) printf("\n");
    //   int eqIdx= j+i*(idxW+2);
    //   printf("%3.1f  ",j-1,i-1,BG_exnodes[eqIdx].mean);
    // }

    // printf("\n");
    if (0)
    {

      {
        BGLightNodeInfo *t0 = fetchIdx(-1, -1); //LT
        BGLightNodeInfo *t1 = fetchIdx(0, 0);
        BGLightNodeInfo *t2 = fetchIdx(1, 1);
        float alpha = -1;
        t0->mean = t1->mean * (1 - alpha) + t1->mean * (alpha);
      }

      {
        BGLightNodeInfo *t0 = fetchIdx(idxW, -1); //RT
        BGLightNodeInfo *t1 = fetchIdx(idxW - 1, 0);
        BGLightNodeInfo *t2 = fetchIdx(idxW - 2, 1);
        float alpha = -1;
        t0->mean = t1->mean * (1 - alpha) + t1->mean * (alpha);
      }

      {
        BGLightNodeInfo *t0 = fetchIdx(idxW, idxH); //RB
        BGLightNodeInfo *t1 = fetchIdx(idxW - 1, idxH - 1);
        BGLightNodeInfo *t2 = fetchIdx(idxW - 2, idxH - 2);
        float alpha = -1;
        t0->mean = t1->mean * (1 - alpha) + t1->mean * (alpha);
      }

      {
        BGLightNodeInfo *t0 = fetchIdx(-1, idxH); //RB
        BGLightNodeInfo *t1 = fetchIdx(0, idxH - 1);
        BGLightNodeInfo *t2 = fetchIdx(1, idxH - 2);
        float alpha = -1;
        t0->mean = t1->mean * (1 - alpha) + t1->mean * (alpha);
      }
    }
  }

  // }
}

int stageLightParam::RESET()
{
  BG_nodes.clear();
  BG_exnodes.clear();
  exposure_us = NAN;
  tarImgW = 0;
  tarImgH = 0;
  idxW = 0, idxH = 0;
  back_light_target=200;
  origin_offset.X=0;
  origin_offset.Y=0;
  return 0;
}

BGLightNodeInfo *stageLightParam::fetchIdx(int X, int Y)
{
  if (X < -1 || Y < -1 || X >= idxW + 1 || Y >= idxH + 1)
    return NULL;
  int idx = (X + 1) + (Y + 1) * (idxW + 2);
  if (idx >= BG_exnodes.size())
    return NULL;
  return &(BG_exnodes[idx]);
}
float stageLightParam::factorSampling(acv_XY pos)
{
  
  pos.X+=origin_offset.X;
  pos.Y+=origin_offset.Y;
  float alphaX = (pos.X * idxW / tarImgW) - 0.5;
  float alphaY = (pos.Y * idxH / tarImgH) - 0.5;
  int leadX = floor(alphaX);
  int leadY = floor(alphaY);

  alphaX -= leadX;
  alphaY -= leadY;
  /*
      
      aX  v1 1-aX
    t00----x----t01
           | aY
           x 
           |
           | 1-aY
    t10----x----t11
           v2
  */
  BGLightNodeInfo *t00 = fetchIdx(leadX, leadY);
  BGLightNodeInfo *t10 = fetchIdx(leadX, leadY + 1);
  BGLightNodeInfo *t01 = fetchIdx(leadX + 1, leadY);
  BGLightNodeInfo *t11 = fetchIdx(leadX + 1, leadY + 1);
  if (t00 && t10 && t01 && t11)
  {
    float v1 = (1 - alphaX) * t00->mean + alphaX * t01->mean;
    float v2 = (1 - alphaX) * t10->mean + alphaX * t11->mean;
    return v1 * (1 - alphaY) + v2 * alphaY;
  }
  return NAN;
}