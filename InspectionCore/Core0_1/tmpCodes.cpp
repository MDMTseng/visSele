
#include "acvImage_BasicTool.hpp"
#include <acvImage_SpDomainTool.hpp>
#include <acvImage_BasicDrawTool.hpp>
#include <acvImage_ComponentLabelingTool.hpp>
#include "logctrl.h"
#include <float.h>
#include "common_lib.h"

const int TABLE_SIN_512_LENGTH=512;
#define IDXWarp_SIN512(idx) (idx&(TABLE_SIN_512_LENGTH-1))

#define IDXWarp_COS512(idx) (IDXWarp_SIN512(idx+TABLE_SIN_512_LENGTH/4))
int TABLE_SIN_512[]=
{
   0,   6,  12,  18,  25,  31,  37,  43,  50,  56,  62,  68,  75,  81,  87,  93,
  99, 106, 112, 118, 124, 130, 136, 142, 148, 154, 160, 166, 172, 178, 184, 190,
 195, 201, 207, 213, 218, 224, 230, 235, 241, 246, 252, 257, 263, 268, 273, 279,
 284, 289, 294, 299, 304, 310, 314, 319, 324, 329, 334, 339, 343, 348, 353, 357,
 362, 366, 370, 375, 379, 383, 387, 391, 395, 399, 403, 407, 411, 414, 418, 422,
 425, 429, 432, 435, 439, 442, 445, 448, 451, 454, 457, 460, 462, 465, 468, 470,
 473, 475, 477, 479, 482, 484, 486, 488, 489, 491, 493, 495, 496, 498, 499, 500,
 502, 503, 504, 505, 506, 507, 508, 508, 509, 510, 510, 511, 511, 511, 511, 511,
 512, 511, 511, 511, 511, 511, 510, 510, 509, 508, 508, 507, 506, 505, 504, 503,
 502, 500, 499, 498, 496, 495, 493, 491, 489, 488, 486, 484, 482, 479, 477, 475,
 473, 470, 468, 465, 462, 460, 457, 454, 451, 448, 445, 442, 439, 435, 432, 429,
 425, 422, 418, 414, 411, 407, 403, 399, 395, 391, 387, 383, 379, 375, 370, 366,
 362, 357, 353, 348, 343, 339, 334, 329, 324, 319, 314, 310, 304, 299, 294, 289,
 284, 279, 273, 268, 263, 257, 252, 246, 241, 235, 230, 224, 218, 213, 207, 201,
 195, 190, 184, 178, 172, 166, 160, 154, 148, 142, 136, 130, 124, 118, 112, 106,
  99,  93,  87,  81,  75,  68,  62,  56,  50,  43,  37,  31,  25,  18,  12,   6,
   0,  -6, -12, -18, -25, -31, -37, -43, -50, -56, -62, -68, -75, -81, -87, -93,
 -99,-106,-112,-118,-124,-130,-136,-142,-148,-154,-160,-166,-172,-178,-184,-190,
-195,-201,-207,-213,-218,-224,-230,-235,-241,-246,-252,-257,-263,-268,-273,-279,
-284,-289,-294,-299,-304,-310,-314,-319,-324,-329,-334,-339,-343,-348,-353,-357,
-362,-366,-370,-375,-379,-383,-387,-391,-395,-399,-403,-407,-411,-414,-418,-422,
-425,-429,-432,-435,-439,-442,-445,-448,-451,-454,-457,-460,-462,-465,-468,-470,
-473,-475,-477,-479,-482,-484,-486,-488,-489,-491,-493,-495,-496,-498,-499,-500,
-502,-503,-504,-505,-506,-507,-508,-508,-509,-510,-510,-511,-511,-511,-511,-511,
-512,-511,-511,-511,-511,-511,-510,-510,-509,-508,-508,-507,-506,-505,-504,-503,
-502,-500,-499,-498,-496,-495,-493,-491,-489,-488,-486,-484,-482,-479,-477,-475,
-473,-470,-468,-465,-462,-460,-457,-454,-451,-448,-445,-442,-439,-435,-432,-429,
-425,-422,-418,-414,-411,-407,-403,-399,-395,-391,-387,-383,-379,-375,-370,-366,
-362,-357,-353,-348,-343,-339,-334,-329,-324,-319,-314,-310,-304,-299,-294,-289,
-284,-279,-273,-268,-263,-257,-252,-246,-241,-235,-230,-224,-218,-213,-207,-201,
-195,-190,-184,-178,-172,-166,-160,-154,-148,-142,-136,-130,-124,-118,-112,-106,
 -99, -93, -87, -81, -75, -68, -62, -56, -50, -43, -37, -31, -25, -18, -12,  -6,
};

void NShift(int v,int* max, int* sec, int* trd)
{
  if(*trd<v)
  {
    *trd=v;
  }

  if(*sec<v)
  {
    *trd=*sec;
    *sec=v;
  }

  if(*max<v)
  {
    *sec=*max;
    *max=v;
  }
}

typedef struct seqInfo{
  int type;
  int count;
}seqInfo;

float min (float a, float b) { return a<b?a:b;}
void FAST512_chessBoard(acvImage *src,acvImage *fast_map,int steps,int circleR, int circleR_segs,int smargin=20)
{
  if(circleR_segs>512)circleR_segs=512;

  int segs_div_4=circleR_segs/4;
  int ssss=0;
  int pixbuf[512];
  int space=circleR+2;
  for (int i = space; i < src->GetHeight()-space; i+=steps)
  {
    for (int j = space; j < src->GetWidth()-space; j+=steps)
    {
      uint8_t *_centerPix = &(src->CVector[i][3*j]);
      uint8_t *fastMapPix =  &(fast_map->CVector[i/steps][(j/steps)*3]);
      int centerPix=_centerPix[0];
      int avgPix=0;

      for(int k=0;k<circleR_segs;k++)
      {
        int idx = k*TABLE_SIN_512_LENGTH/circleR_segs;
        int sin512=TABLE_SIN_512[IDXWarp_SIN512(idx)]*circleR/512;
        int cos512=TABLE_SIN_512[IDXWarp_COS512(idx)]*circleR/512;

        uint8_t *circPix = &(src->CVector[sin512+i][3*(cos512+j)]);
        pixbuf[k]=circPix[0];
        avgPix+=circPix[0];
      }
      avgPix/=circleR_segs;

      centerPix = avgPix;
      int unMod=0;
      int count=0;

      int seqILen=0;
      seqInfo seqI[10]={0};
      for(int k=0;k<circleR_segs;k++)
      {

        int curMod=0;
        if(pixbuf[k]>centerPix+smargin)
        {
          curMod=1;
        }
        else if(pixbuf[k]<centerPix-smargin)
        {
          curMod=2;
        }

        if(unMod!=curMod)
        {
          if(count!=0)
          {
            seqI[seqILen].type=unMod;
            seqI[seqILen].count=count;
            seqILen++;
          }
            // NShift(count,maxCount+unMod, secCount+unMod,trdCount+unMod);
          unMod=curMod;
          count=1;
        }
        else
        {
          count++;
        }

        // printf("(%d,%d,%d),",pixbuf[k],unMod,count);
      }

      if(seqILen>0 && seqI[0].type==unMod)
      {
        seqI[0].count+=count;
      }
      else
      {
        seqI[seqILen].type=unMod;
        seqI[seqILen].count=count;
        seqILen++;
      }

      // printf("\n=========[%d,%d]\n",j,i);
      // for(int k=0;k<seqILen;k++)
      // {
      //   printf("=(%d,%d),",seqI[k].type,seqI[k].count);
      // }
      // printf("\n");

      int mergeThres=2;
      bool removeMode0=true;
      if(mergeThres>0||removeMode0)
      {
        int headIdx=0;
        for(int k=0;k<seqILen;k++)
        {
          if(seqI[k].count<=mergeThres ||seqI[k].type==0)
            continue;
          if(headIdx>0 &&seqI[headIdx-1].type==seqI[k].type)
          {
            seqI[headIdx-1].count+=seqI[k].count;
          }
          else
          {
            seqI[headIdx]=seqI[k];
            headIdx++;
          }
        }

        if(headIdx>0 &&seqI[headIdx-1].type==seqI[0].type)
        {
          seqI[0].count+=seqI[headIdx-1].count;
          headIdx--;
        }
        seqILen=headIdx;
      }
      float score=0;
      if(seqILen==4)
      {

        float ratio_1=(float)seqI[0].count/seqI[2].count;
        float ratio_2=(float)seqI[1].count/seqI[3].count;
        float minS=min(min(seqI[0].count,seqI[2].count),min(seqI[1].count,seqI[3].count));
        // if(minS/segs_div_4>0.5)score=0;
        if(ratio_2>1)ratio_2=1/ratio_2;
        if(ratio_1>1)ratio_1=1/ratio_1;
        float ratio_12=ratio_1*ratio_2;

        // if((int)(ratio_12*20)>1)
        //   acvDrawBlock(fast_map,j-2,i-2,j+2,i+2,0,(int)(ratio_12*255),0,(int)(ratio_12*20));
        ratio_12*=3;
        if(ratio_12>1)ratio_12=1;

        fastMapPix[0]=0;
        fastMapPix[2]=0;//maxCount[1]*255/circleR_segs;
        fastMapPix[1]=(int)(ratio_12*255);//maxCount[2]*255/circleR_segs;
        // printf("\n=========[%d,%d]\n",j,i);

        // for(int k=0;k<seqILen;k++)
        // {
        //   printf("=(%d,%d),",seqI[k].type,seqI[k].count);
        // }

        // printf("\n");
      }
    }
    // exit(-1);
  }
}

void FAST512(acvImage *src,acvImage *fast_map,int steps,int circleR, int circleR_segs,int smargin=20)
{
  if(circleR_segs>512)circleR_segs=512;

  int segs_div_4=circleR_segs/4;
  int ssss=0;
  int pixbuf[512];
  int space=circleR+2;
  for (int i = space; i < src->GetHeight()-space; i+=steps)
  {
    for (int j = space; j < src->GetWidth()-space; j+=steps)
    {
      uint8_t *_centerPix = &(src->CVector[i][3*j]);
      uint8_t *fastMapPix =  &(fast_map->CVector[i/steps][(j/steps)*3]);
      int centerPix=_centerPix[0];

      for(int k=0;k<circleR_segs;k++)
      {
        int idx = k*TABLE_SIN_512_LENGTH/circleR_segs;
        int sin512=TABLE_SIN_512[IDXWarp_SIN512(idx)]*circleR/512;
        int cos512=TABLE_SIN_512[IDXWarp_COS512(idx)]*circleR/512;

        uint8_t *circPix = &(src->CVector[sin512+i][3*(cos512+j)]);
        pixbuf[k]=circPix[0];
      }

      int unMod=0;
      int count=0;

      int maxCount[3]={0};
      int secCount[3]={0};
      int trdCount[3]={0};
      int initMode=-1;
      int initCount=0;
      for(int k=0;k<circleR_segs;k++)
      {

        int curMod=0;
        if(pixbuf[k]>centerPix+smargin)
        {
          curMod=1;
        }
        else if(pixbuf[k]<centerPix-smargin)
        {
          curMod=2;
        }

        if(unMod!=curMod)
        {
          if(initMode==-1 && maxCount[unMod]<count)
          {
            initMode=unMod;
            initCount=count;
          }
          else
            NShift(count,maxCount+unMod, secCount+unMod,trdCount+unMod);
          unMod=curMod;
          count=1;
        }
        else
        {
          count++;
        }

        // printf("(%d,%d,%d),",pixbuf[k],unMod,count);
      }
      // printf("\n");

      if(unMod==initMode)
      {
        count+=initCount;
      }

      NShift(count,maxCount+unMod, secCount+unMod,trdCount+unMod);

      fastMapPix[0]=(maxCount[2]>maxCount[1]?maxCount[2]:maxCount[1]);
      fastMapPix[2]=0;//maxCount[1]*255/circleR_segs;
      fastMapPix[1]=0;//maxCount[2]*255/circleR_segs;

    }
  }
}

typedef struct gridsSort_ptNote{
  int idx;
  float dist;
}gridsSort_ptNote;

typedef struct gridsSort_ptInfo{
  int idx;
  acv_XY cenPt;

  int d00;
  int d01;
  int d10;
  int d11;
  int mstage;
  //mstage 
  //0:not set yet
  //1:get  initial vector
  //2:complete expansion

  acv_XY v0;
  acv_XY v1;

  int coord[2];
  int linkCount;

}gridsSort_ptInfo;


int gridsSort_ptNoteFindIdx(gridsSort_ptNote newPt,gridsSort_ptNote* pts,int ptsL)
{
  int newpt_idx=-1;
  for(int i=0;i<ptsL;i++)
  {
    if(newPt.dist<pts[i].dist)
    {
      newpt_idx=i;
      break;
    }
  }
  return newpt_idx;
}

int gridsSort_ptNoteInsert(gridsSort_ptNote newPt,int insertIdx,gridsSort_ptNote* pts,int ptsL)
{
  if(insertIdx==-1)return -1;

  for(int i=ptsL-1;i>insertIdx;i--)
  {
    pts[i]=pts[i-1];
  }
  pts[insertIdx]=newPt;
  return insertIdx;
}

int gridsSort_ptNoteFindNClosest(gridsSort_ptNote* pts,int ptsL,int crossN=3,float *ret_ratio=NULL)
{
  float maxRatio=0;
  int maxRatioIdx=-1;

  for(int i=0;i<ptsL-crossN;i++)
  {
    float ratio = pts[i].dist/pts[i+crossN].dist;
    if(maxRatio<ratio)
    {
      maxRatio=ratio;
      maxRatioIdx=i;
    }
  }
  if(ret_ratio)*ret_ratio=maxRatio;
  return maxRatioIdx;
}
int intStackTryPush(std::vector<int> &gstack,int pushData)
{
  for(int i=0;i<gstack.size();i++)
  {
    if(gstack[i]==pushData)return -1;
  }

  gstack.push_back(pushData);
  return gstack.size()-1;
}
gridsSort_ptInfo dsfsdf(std::vector<acv_LabeledData> &ldData,int tarIdx,float min_dotP=0.95,float min_distMixScore=0.95)
{
  gridsSort_ptInfo ptInfo;
  ptInfo.idx=tarIdx;

  const int topN=5;
  gridsSort_ptNote closeNPt[topN]={0};

  for(int j=0;j<topN;j++)
  {
    closeNPt[j].dist=FLT_MAX;
  }
  acv_XY c1=ldData[tarIdx].Center;

  ptInfo.cenPt=c1;

  
  bool LineDoInsert=false;
  for(int j=1;j<ldData.size();j++)
  {
    if(tarIdx==j)continue;
    acv_XY c2=ldData[j].Center;


    // ldData[j].misc=
    float dist = acvDistance(c1,c2);
    gridsSort_ptNote newPt={.idx=j,.dist=dist};

    int iidx=gridsSort_ptNoteFindIdx(newPt,closeNPt,topN);

    gridsSort_ptNoteInsert(newPt,iidx,closeNPt,topN);
    // if(iidx!=-1)LineDoInsert=true;

  }

  float N4Ratio=NAN;
  int N4Idx=gridsSort_ptNoteFindNClosest(closeNPt,topN,3,&N4Ratio);
  float N5Ratio=NAN;
  int N5Idx=gridsSort_ptNoteFindNClosest(closeNPt,topN,4,&N5Ratio);
  N5Ratio*=1.414;
  if(N5Ratio>1)N5Ratio=1/N5Ratio;


  float mixScore=N4Ratio*N5Ratio;//mix N4 qnd N5
  
  if(mixScore>min_distMixScore)
  {

    acv_LabeledData ld0 = ldData[closeNPt[N4Idx+0].idx];
    acv_LabeledData ld1 = ldData[closeNPt[N4Idx+1].idx];
    acv_LabeledData ld2 = ldData[closeNPt[N4Idx+2].idx];
    acv_LabeledData ld3 = ldData[closeNPt[N4Idx+3].idx];
    

    acv_XY v0=acvVecNormalize(acvVecSub(ld0.Center,c1));
    acv_XY v1=acvVecNormalize(acvVecSub(ld1.Center,c1));
    acv_XY v2=acvVecNormalize(acvVecSub(ld2.Center,c1));
    acv_XY v3=acvVecNormalize(acvVecSub(ld3.Center,c1));



    /*
    one example, the order might be different
         00
          ^
          |
    03<---o---->01
          |
          V
         02

    */


    if     (acv2DDotProduct(v0,v1)<-min_dotP  && acv2DDotProduct(v2,v3)<-min_dotP)
    {//01 -> 23
    // ldData[0].misc
      ptInfo.d00=closeNPt[N4Idx+0].idx;
      ptInfo.d01=closeNPt[N4Idx+1].idx;
      ptInfo.d10=closeNPt[N4Idx+2].idx;
      ptInfo.d11=closeNPt[N4Idx+3].idx;
      ptInfo.v0=acvVecMult(acvVecSub(ld1.Center,ld0.Center),0.5);
      ptInfo.v1=acvVecMult(acvVecSub(ld3.Center,ld2.Center),0.5);
    }
    else if(acv2DDotProduct(v0,v2)<-min_dotP  && acv2DDotProduct(v1,v3)<-min_dotP)
    {//02 -> 13

      ptInfo.d00=closeNPt[N4Idx+0].idx;
      ptInfo.d01=closeNPt[N4Idx+2].idx;
      ptInfo.d10=closeNPt[N4Idx+1].idx;
      ptInfo.d11=closeNPt[N4Idx+3].idx;
      ptInfo.v0=acvVecMult(acvVecSub(ld2.Center,ld0.Center),0.5);
      ptInfo.v1=acvVecMult(acvVecSub(ld3.Center,ld1.Center),0.5);
    }
    else if(acv2DDotProduct(v0,v3)<-min_dotP  && acv2DDotProduct(v1,v2)<-min_dotP)
    {//03 -> 12

      ptInfo.d00=closeNPt[N4Idx+0].idx;
      ptInfo.d01=closeNPt[N4Idx+3].idx;
      ptInfo.d10=closeNPt[N4Idx+1].idx;
      ptInfo.d11=closeNPt[N4Idx+2].idx;
      ptInfo.v0=acvVecMult(acvVecSub(ld3.Center,ld0.Center),0.5);
      ptInfo.v1=acvVecMult(acvVecSub(ld2.Center,ld1.Center),0.5);
    }
    else
    {//ERROR the cross pattern is not aligned 

      ptInfo.idx=-1;
      ptInfo.d00=-1;
      ptInfo.d01=-1;
      ptInfo.d10=-1;
      ptInfo.d11=-1;
      ptInfo.v0.X=
      ptInfo.v0.Y=
      ptInfo.v1.X=
      ptInfo.v1.Y=NAN;
    }


  }
  else
  {
    //ERROR the cross pattern is not aligned 
      ptInfo.idx=-1;
      ptInfo.d00=-1;
      ptInfo.d01=-1;
      ptInfo.d10=-1;
      ptInfo.d11=-1;
      ptInfo.v0.X=
      ptInfo.v0.Y=
      ptInfo.v1.X=
      ptInfo.v1.Y=NAN;
  }
  return ptInfo;
}
float vectorSim(acv_XY vec1,acv_XY vec2)
{
  float distSq=vec1.X*vec1.X+vec1.Y*vec1.Y;
  float _distSq=vec2.X*vec2.X+vec2.Y*vec2.Y;
  if(distSq<_distSq)distSq=_distSq;
  float dotP=acv2DDotProduct(vec1,vec2);
  dotP/=distSq;
  return dotP;
}



static float sampling(float *f,int fL,float fIndex)
{
  int idx=(int)fIndex;
  fIndex-=idx;
  
  if(idx==-1 && fIndex==1)return f[0];
  if(idx<0)return NAN;
  
  if(idx==fL-1 && fIndex==0)return f[idx];
  if(idx>=fL-1)return NAN;
  float V1=f[idx];
  float V2=f[idx+1];
  return V1+(V2-V1)*fIndex;
}
static  int calc_pdf_mean_sigma(float *f,float fStart,int fL,float *ret_mean,float *ret_sigma)
{
    float sum=0;
    float EX=0, var=0;
    float EX2=0;
    //Find max

    float maxV=-1000;
    float maxIdx=0;

    for (float i = fStart; i < fL+fStart; i++) {
      float saV=sampling(f,fL+fStart+1,i);
      if(maxV<saV)
      {
        maxV=saV;
        maxIdx=i;
      }
      sum += saV;
      EX += i*saV;
      EX2+= i*i*saV;
    }
    EX/=sum;
    EX2/=sum;

    // printf("EX:%f  EX2:%f\n",EX,sqrt(EX2-EX*EX));

    if(ret_mean)*ret_mean=maxIdx;
    if(ret_sigma)*ret_sigma=sqrt(EX2-EX*EX);

    return 0;
    
}


static acv_XY pointRegionRefine(acvImage *src,acv_XY p1, acv_XY aloneNNVec,int width,float *ret_sigma=NULL)
{
  float buffer[200];
  float offset = -width/2.0;
  acv_XY initPt= acvVecAdd(p1,acvVecMult(aloneNNVec,offset));
  p1=initPt;
  for(int i=0;i<width;i++)
  {
    
    buffer[i]= acvUnsignedMap1Sampling(src, p1, 0);
    // LOGI("%f,%f:%f",p1.X,p1.Y,buffer[i]);
    // src
    p1=acvVecAdd(p1,aloneNNVec);
  }

  float LBri=0,HBri=0;
  for(int i=0;i<width/3;i++)
  {
    LBri+=buffer[i];
    HBri+=buffer[width-1-i];
  }
  float BriDiff=(HBri-LBri)/(width/3);
  if(BriDiff<0)BriDiff=-BriDiff;
  BriDiff/=40;
  BriDiff=-10000;
  int gradRev = LBri<HBri?1:-1;//make sure the brighness is increasing => gradient is positive

  // LOGI("LH:%f,%f",LBri,HBri); 
  int edgeFwd=2;
  for(int i=0;i<width-edgeFwd;i++)
  {
    buffer[i]=gradRev*(buffer[i+edgeFwd]-buffer[i]);
    if(buffer[i]<BriDiff)buffer[i]=0;
  }
  for(int i=width-edgeFwd;i<width;i++)
  {
    buffer[i]=buffer[i-1];
  }
  

  // for(int i=0;i<width;i++)
  // {
  //   printf("%.02f,",buffer[i]);
  // }
  // printf("\n");

  float mean=NAN,sigma=NAN;
  calc_pdf_mean_sigma(buffer,0,width-edgeFwd,&mean,&sigma);

  printf("=====INIT:Mean: %f sigma:%f\n",mean,sigma);
  //if(sigma<width/4)
  {
    
    float _mean_bk=mean;
    for(int j=0;j<10;j++)
    {
      
      if(mean<width/4||mean>=width*3/4)
      {
        mean=_mean_bk;
        sigma=NAN;
        break;
      }

      float _mean_pre=mean;
      calc_pdf_mean_sigma(buffer,mean-width/4,width/2,&mean,&sigma);

      printf("[%d]:Mean: %f sigma:%f\n",j,mean,sigma);
      LOGI("mean[%d]: %f",j,mean);
      if(abs(_mean_pre-mean)<0.01)break;
    }


  }



  acv_XY retPt = acvVecAdd(initPt,acvVecMult(aloneNNVec,edgeFwd/2+mean));

  // for(int i=0;i<width;i++)
  // {
  //   printf("%.02f,",buffer[i]);
  // }
  // printf("\n");
  // printf("retPt:%f,%f\n",retPt.X,retPt.Y);

  // exit(-1);


  if(ret_sigma)*ret_sigma=sigma;
  
  
  return retPt;
}

acv_Line lineRegionRefine(acvImage *src,acv_XY p1,acv_XY p2,int line_width)
{
  gridsSort_ptInfo edgeX[500];
  acv_XY vec = acvVecSub(p2,p1);
  float dist=hypot(vec.X,vec.Y);
  float initSkip=dist*0.1;
  float shrinkDist=dist*0.8;

  vec=acvVecNormalize(vec);
  acv_XY vecN= acvVecNormal(vec);
  // p1=acvVecSub(p1,acvVecMult(vecN,-(float)line_width/2));

  // LOGI("p1:%f,%f,  p2:%f,%f  line_width:%d  vecN:%f,%f",p1.X,p1.Y,p2.X,p2.Y,line_width,vecN.X,vecN.Y); 
  acv_XY cur_pt=acvVecAdd(p1,acvVecMult(vec,(initSkip)));
  p1=cur_pt;
  int nanCount=0;
  for(int i=0;i<shrinkDist;i++)
  {
    float sigma=NAN;
    acv_XY pt=pointRegionRefine(src,cur_pt,vecN,line_width,&sigma);
    float confi=1.0/(sigma+0.5);
    if(confi!=confi || pt.X!=pt.X||pt.Y!=pt.Y)
    {

      // printf("edgeX[%d] :%f   sigma:%f  cur_pt:%f,%f\n",i,pt.X,sigma,cur_pt.X,cur_pt.Y);
      confi=0;
      pt.X=0;
      pt.Y=0;
      nanCount++;
      
    }

    edgeX[i].cenPt=pt;
    edgeX[i].v0.X=confi;
    cur_pt=acvVecAdd(cur_pt,vec);
    // printf("(%.02f,%.02f:%f)\n",edgeX[i].cenPt.X,edgeX[i].cenPt.Y,sigma);
  }
  p2=cur_pt;
  // if(doNan)
  // for(int i=0;i<shrinkDist;i++)
  // {
  //   printf("edgeX[%d] :%f,%f   confi:%f\n",i,edgeX[i].cenPt.X,edgeX[i].cenPt.Y,edgeX[i].v0.X);
  // }
  acv_Line tmp_line;

  tmp_line.line_vec.X=
  tmp_line.line_vec.Y=
  tmp_line.line_anchor.X=
  tmp_line.line_anchor.Y=NAN;

  float sigma=NAN;
  if(nanCount<shrinkDist/5)
    acvFitLine(
        &(edgeX[0].cenPt), sizeof(edgeX[0]),
        &(edgeX[0].v0.X), sizeof(edgeX[0]), (int)shrinkDist, &tmp_line, &sigma);

  float dotP=acv2DDotProduct(tmp_line.line_vec,vec);
  if(abs(dotP)>0.95 && sigma<1){
    acv_XY targP={805,450};
    acv_XY np1=acvClosestPointOnLine(p1, tmp_line);
    acv_XY np2=acvClosestPointOnLine(p2, tmp_line);

    if(acvDistance(targP,np1)<10||acvDistance(targP,np2)<10)
    {
      printf("p:%f,%f    %f,%f\n",np1.X,np1.Y,np2.X,np2.Y);
      acvDrawLine(src, np1.X, np1.Y, np2.X, np2.Y,255, 0, 0, 1);
    }
    tmp_line.line_anchor=np1;
  }
  else
  {
    tmp_line.line_vec.X=
    tmp_line.line_vec.Y=
    tmp_line.line_anchor.X=
    tmp_line.line_anchor.Y=NAN;
    printf("DOTP:%f   vec:%f,%f\n",dotP,tmp_line.line_vec.X,tmp_line.line_vec.Y);
  }
  
  return tmp_line;

  // printf("====(%f,%f=>%.02f,%.02f)\n",
  //   np1.X, np1.Y, np2.X, np2.Y);
  // printf("(%f,%f=>%.02f,%.02f)\n",
  //   tmp_line.line_vec.X,tmp_line.line_vec.Y,
  //   tmp_line.line_anchor.X,tmp_line.line_anchor.Y);
  
  // SaveIMGFile("data/srcsrcsrc.png",src);

    // for(int i=0;i<width;i++)
  // {
  //   printf("%.02f,",buffer[i]);
  // }
  // printf("\n");
  // printf("retPt:%f,%f\n",retPt.X,retPt.Y);

  // exit(-1);

  // exit(-1);
  // acv_XY pointRegionRefine(acvImage *src,acv_XY p1, acv_XY aloneNNVec,line_width);


}


acv_XY chessBoardPointRefine(acvImage *src,std::vector<gridsSort_ptInfo> &infoData,gridsSort_ptInfo &initCross)
{
  acv_XY p00=(initCross.d00==-1)?acvVecSub(initCross.cenPt,initCross.v0):infoData[initCross.d00].cenPt;
  acv_XY p01=(initCross.d01==-1)?acvVecAdd(initCross.cenPt,initCross.v0):infoData[initCross.d01].cenPt;
  acv_XY p10=(initCross.d10==-1)?acvVecSub(initCross.cenPt,initCross.v1):infoData[initCross.d10].cenPt;
  acv_XY p11=(initCross.d11==-1)?acvVecAdd(initCross.cenPt,initCross.v1):infoData[initCross.d11].cenPt;
  


  acv_Line l00,l01,l10,l11;
  if(0)
  {
    acv_Line _l00={.line_vec=initCross.cenPt,.line_anchor=acvVecSub(p00,initCross.cenPt)};
    acv_Line _l01={.line_vec=initCross.cenPt,.line_anchor=acvVecSub(p01,initCross.cenPt)};
    acv_Line _l10={.line_vec=initCross.cenPt,.line_anchor=acvVecSub(p10,initCross.cenPt)};
    acv_Line _l11={.line_vec=initCross.cenPt,.line_anchor=acvVecSub(p11,initCross.cenPt)};
    l00=_l00;
    l01=_l01;
    l10=_l10;
    l11=_l11;
  }
  else
  {
    
    float dist =acvDistance(p10,p11);
    l00= lineRegionRefine(src,initCross.cenPt,p00,dist/2);
    l01= lineRegionRefine(src,initCross.cenPt,p01,dist/2);
    l10= lineRegionRefine(src,initCross.cenPt,p10,dist/2);
    l11= lineRegionRefine(src,initCross.cenPt,p11,dist/2);

  }


  if(l00.line_vec.X!=l00.line_vec.X ||
  l01.line_vec.X!=l01.line_vec.X ||
  l10.line_vec.X!=l10.line_vec.X ||
  l11.line_vec.X!=l11.line_vec.X
  )
  {
    printf("XXXXXXXXX\n");
  }



  l00.line_vec=acvVecMult(acvVecAdd(l00.line_vec,l01.line_vec),0.5);//Vec mix
  l10.line_vec=acvVecMult(acvVecAdd(l10.line_vec,l11.line_vec),0.5);

  l00.line_anchor=acvVecMult(acvVecAdd(l00.line_anchor,l01.line_anchor),0.5);//anchor mix
  l10.line_anchor=acvVecMult(acvVecAdd(l10.line_anchor,l11.line_anchor),0.5);


  return acvLineIntersect(l00,l10);


}




void gridsSort(acvImage *img,std::vector<acv_LabeledData> &ldData)
{
  float min_sim=0.98;
  float min_areaRatio=0.8;

  const int topN=5;
  gridsSort_ptNote closeNPt[topN];

  int sampleNPoints=10;
  gridsSort_ptInfo ptInfo;
  ptInfo.idx=-1;
  for(int i=0;i<sampleNPoints;i++)
  {
    int sampleIdx=rand()%(ldData.size()-1)+1;
    ptInfo = dsfsdf(ldData,sampleIdx);
    
    if(ptInfo.idx>=0)
    {
      break;
    }
  }

  
  // LOGI("IDX:%d [%f,%f] =>  [%f,%f] [%f,%f] ",
  //   ptInfo.idx,ptInfo.cenPt.X,ptInfo.cenPt.X,
  //   ptInfo.v1.X,
  //   ptInfo.v1.Y,
  //   ptInfo.v2.X,
  //   ptInfo.v2.Y
  //   );


  std::vector<int> gstack;

  std::vector<gridsSort_ptInfo> infoData;
  infoData.resize(ldData.size());
  for(int i=1;i<infoData.size();i++)
  {
    infoData[i].idx=i;
    infoData[i].cenPt=ldData[i].Center;
    infoData[i].d00=
    infoData[i].d01=
    infoData[i].d10=
    infoData[i].d11=-1;
    infoData[i].v0.X=
    infoData[i].v0.Y=
    infoData[i].v1.X=
    infoData[i].v1.Y=NAN;
    infoData[i].mstage=0;
    infoData[i].linkCount=0;
  }
  ptInfo.mstage=1;//althogh the dxx is already here but for code fluency, we will redo the point searching again
  infoData[ptInfo.idx]=ptInfo;
  infoData[ptInfo.idx].coord[0]=0;
  infoData[ptInfo.idx].coord[1]=0;
  //startSeed
  // gstack.push_back(ptInfo.idx);
  intStackTryPush(gstack,ptInfo.idx);

  int XCount=0;
  while(gstack.size()>0)
  {
    int tailIdx = gstack[gstack.size()-1];
    gstack.pop_back();
    gridsSort_ptInfo &curPt = infoData[tailIdx];
    if(curPt.mstage==2)
    {
      LOGI("REPEAT filtered...");
      continue;
    }
    curPt.mstage=2;
    float max00=0;
    float max01=0;
    float max10=0;
    float max11=0;
    int d00,d01,d10,d11;
    for(int i=1;i<infoData.size();i++)
    {
      gridsSort_ptInfo &tPt=infoData[i];
      // if(tPt.mstage==2)
      // {//the stage 2 should/would never be touched again.
      //   continue;
      // }
      acv_XY vec= acvVecSub(tPt.cenPt,curPt.cenPt);
      {
        float sim0 =vectorSim(curPt.v0,vec);
        if(sim0>max01)
        {//d01
          max01=sim0;
          d01=i;
        }
        if(-sim0>max00)
        {//d00
          max00=-sim0;
          d00=i;
        }

        float sim1 =vectorSim(curPt.v1,vec);
        if(sim1>max11)
        {//d11
          max11=sim1;
          d11=i;
        }
        if(-sim1>max10)
        {//d10
          max10=-sim1;
          d10=i;
        }

        // printf("[%0.2f,%0.2f],",curPt.v0.X,curPt.v0.Y);
      }


    }


    // printf("\n");
    {

      if(max00>min_sim)
      {
        if(infoData[d00].mstage<2)
        {
          infoData[d00].mstage=1;
          infoData[d00].v0=acvVecSub(curPt.cenPt,infoData[d00].cenPt);
          infoData[d00].v1=curPt.v1;
          // gstack.push_back(d00);
          infoData[d00].coord[0]=curPt.coord[0]-1;
          infoData[d00].coord[1]=curPt.coord[1]+0;
          intStackTryPush(gstack,d00);
        }
        infoData[d00].linkCount++;
        curPt.d00=d00;
        // printf(">>>>>>>>00>>>>>>>>[%0.2f,%0.2f]\n",infoData[d00].v0.X,infoData[d00].v0.Y);
      }
      if(max01>min_sim)
      {
        if(infoData[d01].mstage<2)
        {
          infoData[d01].mstage=1;
          infoData[d01].v0=acvVecSub(infoData[d01].cenPt,curPt.cenPt);
          infoData[d01].v1=curPt.v1;
          infoData[d01].coord[0]=curPt.coord[0]+1;
          infoData[d01].coord[1]=curPt.coord[1]+0;
          // gstack.push_back(d01);
          intStackTryPush(gstack,d01);
        }
        infoData[d01].linkCount++;
        curPt.d01=d01;
        // printf(">>>>>>>>01>>>>>>>>[%0.2f,%0.2f]\n",infoData[d01].v0.X,infoData[d01].v0.Y);
      }
      
      if(max10>min_sim)
      {
        if(infoData[d10].mstage<2)
        {
          infoData[d10].mstage=1;
          infoData[d10].v1=acvVecSub(curPt.cenPt,infoData[d10].cenPt);
          infoData[d10].v0=curPt.v0;
          infoData[d10].coord[0]=curPt.coord[0]+0;
          infoData[d10].coord[1]=curPt.coord[1]-1;
          // gstack.push_back(d10);
          intStackTryPush(gstack,d10);
        }
        infoData[d10].linkCount++;
        curPt.d10=d10;
        // printf(">>>>>>>>10>>>>>>>>[%0.2f,%0.2f]\n",infoData[d10].v0.X,infoData[d10].v0.Y);
      }
      if(max11>min_sim)
      {

        // printf(">>>>>>>>11>>>>>>>>mstage_%d\n",infoData[d11].mstage);
        if(infoData[d11].mstage<2)
        {
          infoData[d11].mstage=1;
          infoData[d11].v1=acvVecSub(infoData[d11].cenPt,curPt.cenPt);
          infoData[d11].v0=curPt.v0;
          infoData[d11].coord[0]=curPt.coord[0]+0;
          infoData[d11].coord[1]=curPt.coord[1]+1;
          // gstack.push_back(d11);
          intStackTryPush(gstack,d11);
        }
        infoData[d11].linkCount++;
        curPt.d11=d11;
        // printf(">>>>>>>>11>>>>>>>>[%0.2f,%0.2f] [%0.2f,%0.2f]\n",infoData[d11].v0.X,infoData[d11].v0.Y,curPt.v0.X,curPt.v0.Y);
      }
    }
    // LOGI("%f:%d %f:%d %f:%d %f:%d",max00,curPt.d00,max01,curPt.d01,max10,curPt.d10,max11,curPt.d11);
    // exit(-1);
    curPt.mstage=2;
    acv_XY ptTmp = 
      chessBoardPointRefine(img,infoData,curPt);

    // acvDrawCrossX(img,ptTmp.X,ptTmp.Y,10,0,255,0,4);
    // if(XCount++>10)exit(-1);
  }

  SaveIMGFile("data/imgimgimg.png",img);
  exit(-1);
  int coordXMin=0;
  int coordYMin=0;
  {
  // coordXMin=99999;
  // coordYMin=99999;
  // for(int i=1;i<ldData.size();i++)
  // {
  //   if(infoData[i].mstage!=2)
  //     continue;

  //   if(coordXMin>infoData[i].coord[0])
  //     coordXMin=infoData[i].coord[0];
  //   if(coordYMin>infoData[i].coord[1])
  //     coordYMin=infoData[i].coord[1];

  // }
  }


  for(int i=1;i<ldData.size();i++)
  {
    if(infoData[i].mstage!=2)
    {
      ldData[i].LTBound.X=NAN;
      ldData[i].LTBound.Y=NAN;
    }
    else
    {
      ldData[i].LTBound.X=infoData[i].coord[0]-coordXMin;
      ldData[i].LTBound.Y=infoData[i].coord[1]-coordYMin;
      ldData[i].misc=infoData[i].linkCount;
    }

    // LOGI("idx:%d  coord:%.1f,%.1f",i,ldData[i].LTBound.X,ldData[i].LTBound.Y);
  }

}

int tmpMain()
{
  // int tableL=512;
  // for(int i=0;i<tableL;i++)
  // {
  //   printf("%4d,",(int)(512*sin(M_PI*2*i/tableL)));
  //   if(i%16==15)
  //     printf("\n");

  // }
  // printf("\n");
  // return 0;
  acvImage img;
  int fastMapDownScale=3;
  acvImage FAST_MAP;
  acvImage bufferDS;
  int ret = LoadIMGFile(&img, "data/calibTest1.BMP");
  LOGI("LoadIMGFile:%d",ret);
  FAST_MAP.ReSize(img.GetWidth()/fastMapDownScale,img.GetHeight()/fastMapDownScale);
  bufferDS.ReSize(&FAST_MAP);
  // acvThreshold(&img,128);
  // acvCloneImage(&img,&bufferDS,-1);
  acvClear(&FAST_MAP,255);

  if(1)
  {
    FAST512_chessBoard(&img,&FAST_MAP,fastMapDownScale,7,30,20);
    // acvBoxFilter(&bufferDS,&FAST_MAP,3);
    acvThreshold(&FAST_MAP,254);
    acvBoxFilter(&bufferDS,&FAST_MAP,3);
    acvThreshold(&FAST_MAP,240);
    acvBoxFilter(&bufferDS,&FAST_MAP,3);
    acvBoxFilter(&bufferDS,&FAST_MAP,3);
    acvThreshold(&FAST_MAP,160);
    
    acvComponentLabeling(&FAST_MAP);
    
  // SaveIMGFile("data/FAST_MAP.png",&FAST_MAP);
  // exit(-1);
    std::vector<acv_LabeledData> ldData;
    acvLabeledRegionInfo(&FAST_MAP, &ldData);
    // acvLabeledColorDispersion(&FAST_MAP,&FAST_MAP,ldData.size()/123);
    for(int i=1;i<ldData.size();i++)
    {
      ldData[i].area*=fastMapDownScale*fastMapDownScale;
      ldData[i].Center=acvVecMult(ldData[i].Center,fastMapDownScale);
      ldData[i].LTBound=acvVecMult(ldData[i].LTBound,fastMapDownScale);
      ldData[i].RBBound=acvVecMult(ldData[i].RBBound,fastMapDownScale);
    }
    gridsSort(&img,ldData);
    for(int i=1;i<ldData.size();i++)
    {
      if(ldData[i].LTBound.X!=ldData[i].LTBound.X)continue;
      int coordX=(int)ldData[i].LTBound.X/fastMapDownScale;
      int coordY=(int)ldData[i].LTBound.Y/fastMapDownScale;
      if((coordX%3)==0 && (coordY%3)==0)
        acvDrawCross(&FAST_MAP,ldData[i].Center.X,ldData[i].Center.Y,10,255,0,0,4);
      else
        acvDrawCrossX(&FAST_MAP,ldData[i].Center.X,ldData[i].Center.Y,1,0,255,0,1);

    }
  }


  LOGI("FAST512:DONE...");
  SaveIMGFile("data/FAST_buffer.png",&FAST_MAP);
  return 0;
  // acvThreshold(&img,128);
  // acvBoxFilter(&img,&buffer,5);
  // acvThreshold(&img,180);
  // acvBoxFilter(&img,&buffer,5);
  // acvThreshold(&img,10);
  // acvHarrisCornorResponse(&buffer, &img);
  
  // acvSobelFilter(&buffer, &img);

  // for (int i = 0; i < buffer.GetHeight(); i++)
  // {
  //     for (int j = 0; j < buffer.GetWidth(); j++)
  //     {
  //         int Ix = (char)buffer.CVector[i][j * 3 + 0];
  //         int Iy = (char)buffer.CVector[i][j * 3 + 1];
  //         float Ixy = Ix*Iy;
  //         if(Ixy<0)Ixy=-Ixy;
  //         img.CVector[i][j * 3 + 0]=
  //         img.CVector[i][j * 3 + 1]=
  //         img.CVector[i][j * 3 + 2]=(int)Ixy;
  //     }
  // }
  // acvBoxFilter(&buffer,&img,3);

  // acvThreshold(&img,100);

  // // ret = SaveIMGFile("data/calibImg1_exp.png",&buffer);
  // // acvCloneImage(&img,&img,0);
  // ret = SaveIMGFile("data/calibImg1_IMG.png",&img);
  // LOGI("W:%d H:%d",img.GetWidth(),img.GetHeight());
  return 0;
}