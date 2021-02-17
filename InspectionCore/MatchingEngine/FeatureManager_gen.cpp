#include "FeatureManager_gen.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>
// #include <acvImage_.hpp>
#include <rotCaliper.h>

#include <algorithm>


void LineState(acvImage *img,acv_XY pt1,acv_XY pt2,int steps,int ch,float statMoment[2]);
/*
  FeatureManager_platingCheck Section
*/

uint8_t* pixFetching(acvImage *img,int x,int y,int shrink=0)
{
  if(x<shrink || y<shrink)return NULL;
  if(x>=img->GetWidth()-shrink || y>=img->GetHeight()-shrink)return NULL;


  return &(img->CVector[y][3*x]);
}
uint8_t* pixFetching(acvImage *img,acv_XY pt,int shrink=0)
{
  return pixFetching(img,round(pt.X),round(pt.Y),shrink);
}
FeatureManager_gen::FeatureManager_gen(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
  reload(json_str);
  backGroundTemplate.ReSize(1,1);
}

FeatureManager_gen::~FeatureManager_gen()
{
  ClearReport();
}
void FeatureManager_gen::ClearReport()
{
  if(report.data.cjson_report.cjson!=NULL)
  {
    
    cJSON_Delete(report.data.cjson_report.cjson);

    report.data.cjson_report.cjson=NULL;
  }

  report.type=FeatureReport::cjson;



}

int FeatureManager_gen::parse_jobj()
{
  LOGI(">>>parse_jobj>>>");
  SetParam(root);

  LOGI(">>>inspectionStage:%d>>>",inspectionStage);
  return 0;
}


int FeatureManager_gen::reload(const char *json_str)
{
  LOGI(">>>reload>>>");
  report.data.cjson_report.cjson=NULL;


  root = cJSON_Parse(json_str);
  // if (root == NULL)
  // {
  //   LOGE("cJSON parse failed");
  //   return -1;
  // }
  // int ret_err = parse_jobj();
  // if (ret_err != 0)
  // {
  //   featureCircleList.resize(0);
  //   featureLineList.resize(0);
  //   feature_signature.RESET(0);
  //   reload("");
  //   return -2;
  // }
  parse_jobj();

  // cJSON_Delete(root);

  return 0;
}

typedef struct colorInfo{

  float R,G,B;

}colorInfo;
void CableFilter(acvImage &image,acvImage &buff)
{

}

void RGBToHSV(acvImage &im)
{
    for(int i=0; i<im.GetHeight(); i++)
    {
        uint8_t *ImLine=im.CVector[i];
        for(int j=0; j<im.GetWidth(); j++)
        {
            acvImage::HSVFromRGB(ImLine,ImLine);
            ImLine+=3;
        }
    }
}




float Point3Angle(acv_XY p1,acv_XY pc,acv_XY p2)
{
  acv_XY v1={.X=p1.X-pc.X,.Y=p1.Y-pc.Y};
  acv_XY v2={.X=pc.X-p2.X,.Y=pc.Y-p2.Y};
  return acvVectorAngle(v1,v2);
}


void ComputeConvexHull(const acv_XY *polygon,const int L,std::vector<int> &ret_chIdxs)//The polygon cannot have internal overlap/loop
{
	// The polygon needs to have at least three points
	if (L < 3)
	{
		return ;
	}

	ret_chIdxs.clear();
	ret_chIdxs.push_back(0);
	ret_chIdxs.push_back(1);

  double angleSum=0;//The angleSum is to make sure there is no inner loop;
  float angleSum_sigma=1;
  //since the angleSum is incremental with +++---++...., so the floating point noise might have some effect
  int order=1;
	/*
	We piecewise construct the convex hull and combine them at the end of the method. Note that this could be
	optimized by combing the while loops.
	*/
	for (size_t i = 2; i < L; i++)
	{
    float angle=0;
		while (ret_chIdxs.size() > 1)
		{
      angle = Point3Angle(polygon[ret_chIdxs[ret_chIdxs.size() - 2]], polygon[ret_chIdxs[ret_chIdxs.size() - 1]], polygon[i]);
      // if(ret_chIdxs.size()==2)
      // {
      //   if(angle<0)angle=0.01;
      // }
      // if(i>430)
      // {
      //    LOGI("idx:%d  %f,%f  %f,%f  %f,%f   X=>%f  A:%f",i,
      //     polygon[ret_chIdxs[ret_chIdxs.size() - 2]].X,polygon[ret_chIdxs[ret_chIdxs.size() - 2]].Y,
      //     polygon[ret_chIdxs[ret_chIdxs.size() - 1]].X,polygon[ret_chIdxs[ret_chIdxs.size() - 1]].Y,
      //     polygon[i ].X,polygon[i].Y,angle
      //     );
      // }
      if(angle>0&&(angleSum+angle)<2*M_PI+angleSum_sigma)
      {
        break;
      }

      //retract
      if(ret_chIdxs.size()>2)
        angleSum-=Point3Angle(polygon[ret_chIdxs[ret_chIdxs.size() - 3]], polygon[ret_chIdxs[ret_chIdxs.size() - 2]], polygon[ret_chIdxs[ret_chIdxs.size() - 1]]);
			ret_chIdxs.pop_back();

		}
    if(angle>0)
      angleSum+=angle;
		ret_chIdxs.push_back(i);
          
    // {
    //     LOGI("idx:%d points:%d angleSum:%f",i,ret_chIdxs.size(),angleSum*180/M_PI);
    // }
	}

  int firstSecL=ret_chIdxs.size();
  
  //shake elimination
  int fEl_idx=0;
  if(0)
	for (size_t i = 0; i < L; i++)
	{
    int XCount=0;
		while (ret_chIdxs.size() > 1)//forward elimination
		{
      float crossP=CrossProduct(polygon[ret_chIdxs[ret_chIdxs.size() - 1]], polygon[fEl_idx], polygon[fEl_idx+1])*order;
      // LOGI("FE: idx:%d    %f  %d %d %d",i,crossP,fEl_idx,XCount,chIdxs.size());
      if(crossP>=0)break;
      fEl_idx++;
      XCount++;
		}

		while (ret_chIdxs.size() > 1)//backward elimination
		{
      float crossP=CrossProduct(polygon[ret_chIdxs[ret_chIdxs.size() - 2]], polygon[ret_chIdxs[ret_chIdxs.size() - 1]], polygon[fEl_idx])*order;
      // LOGI("FB: idx:%d    %f  %d %d %d",i,crossP,fEl_idx,XCount,chIdxs.size());
      if(crossP>=0)break;
			ret_chIdxs.pop_back();
      XCount++;
		}

    if(XCount==0)
      break;
    //Check repeat

		// chIdxs.push_back(i);

	}



	// upperIdx.insert(upperIdx.end(), lowerIdx.begin(), lowerIdx.end());
  // for(int i=fEl_idx;i<chIdxs.size();i++)
  // {
  //   int idx=chIdxs[i];
    
  //   LOGI("idx:%d, %f %f",idx,polygon[idx].X,polygon[idx].Y);
  // }

  for(int i=fEl_idx;i<ret_chIdxs.size();i++)
  {
    ret_chIdxs[i-fEl_idx]=ret_chIdxs[i];
  }
  ret_chIdxs.resize(ret_chIdxs.size()-fEl_idx);

  // for (size_t i = 2; i < chIdxs.size(); i++)
	// {
  //   LOGI("idx:%d>%d>%d  %f,%f  %f,%f  %f,%f   X=>%f",chIdxs[i-2],chIdxs[i-1],chIdxs[i-0],
  //     polygon[chIdxs[i-2]].X,polygon[chIdxs[i-2]].Y,
  //     polygon[chIdxs[i-1]].X,polygon[chIdxs[i-1]].Y,
  //     polygon[chIdxs[i]  ].X,polygon[chIdxs[i]  ].Y,
  //     CrossProduct(polygon[chIdxs[i-2]  ], polygon[chIdxs[i-1]], polygon[chIdxs[i]])
  //     );
  //   // LOGI("idx:%d  %f,%f  %f,%f  %f,%f",i,);
  // }
	return;
}


void LineState(acvImage *img,acv_XY pt1,acv_XY pt2,int steps,int ch,float statMoment[2])
{
  acv_XY advVec={(pt2.X-pt1.X)/(steps-1),(pt2.Y-pt1.Y)/(steps-1)};
  acv_XY cur_pt=pt1;

  float sum=0,sq_sum=0;
  for(int i=0;i<steps;i++,cur_pt=acvVecAdd(cur_pt,advVec))
  {
    // float val = acvUnsignedMap1Sampling(img, cur_pt,ch);


    uint8_t *pix = pixFetching(img,(int)cur_pt.X,(int)cur_pt.Y);
    if(pix==NULL)continue;
    float val=(float)(pix[0]+pix[1]*2+pix[2])/4;


    // printf("%.1f,",val);
    sum+=val;
    sq_sum+=val*val;
  }

  // printf("\n");
  sum/=steps;
  sq_sum/=steps;
  statMoment[0]=sum;
  statMoment[1]=sq_sum-sum*sum;
}
void majorColorTracing(acvImage *colorBuffer,int w,int h)
{
  float bri_thres=0.5;
  uint8_t maxBri=0;
  float bri=0;
  float sigma=0;


  int count=0;
  for(int i=0;i<h;i++)
  {
    for(int j=w/3;j<2*w/3;j++)
    {
      int R=colorBuffer->CVector[i][3*j+2];
      int G=colorBuffer->CVector[i][3*j+1];
      int B=colorBuffer->CVector[i][3*j+0];
      int cbri=(R+G+B)/3;
      if(maxBri<cbri)
      {
        maxBri = cbri;
      }
      bri+=cbri;
      sigma+=cbri*cbri;
      count++;
    }
  }

  bri/=count;
  sigma=sqrt(sigma/count-bri*bri);
  bri+=sigma;//set to high Thres

  for(int i=0;i<h;i++)
  {
    for(int j=w/3;j<2*w/3;j++)
    {
      // int R=colorBuffer[i][3*j+2];
      // int G=colorBuffer[i][3*j+1];
      // int B=colorBuffer[i][3*j+0];
      // int cbri=(R+G+B)/3;  

    }
  }
}


int abs_int(int val)
{
  return val<0?-val:val;
}




void LineScanMatchCalc(acvImage *img,int X_start,int X_L,int Y,regionInfo_single *rinfo)
{
  int i=Y;
  for(int j=X_start;j<X_start+X_L;j++)
  {
    uint8_t *pix = &img->CVector[i][j*3];

    rinfo->inspRes.RGBA[0]+=pix[2];
    rinfo->inspRes.RGBA[1]+=pix[1];
    rinfo->inspRes.RGBA[2]+=pix[0];
    
    float diffs[]={rinfo->RGBA[0]-pix[2],rinfo->RGBA[1]-pix[1],rinfo->RGBA[2]-pix[0]};
    rinfo->inspRes.diff+=sqrt((diffs[0]*diffs[0]+diffs[1]*diffs[1]+diffs[2]*diffs[2])/3);
    // rinfo->sgRGBA[0]+=pix[2]*pix[2];
    // rinfo->sgRGBA[1]+=pix[1]*pix[1];
    // rinfo->sgRGBA[2]+=pix[0]*pix[0];
    
  }

  rinfo->inspRes.count+=X_L;
}

void LineScanRGB_Sum(acvImage *img,int X_start,int X_L,int Y,int *R,int *G,int *B)
{
  int i=Y;
  for(int j=X_start;j<X_start+X_L;j++)
  {
    uint8_t *pix = &img->CVector[i][j*3];
    *R+=pix[2];
    *G+=pix[1];
    *B+=pix[0];
    
  }
}


float RGB_Error(uint8_t *rgb1,uint8_t *rgb2,int RGBMargin)
{

  //sim: within RGBMargin => 1   2XRGBMargin => 0

  // if(0)
  // {

  //   float sim0=2-(float)abs_int(rgb1[0]-rgb2[0])/RGBMargin;
  //   if(sim0>1)sim0=1;
  //   if(sim0<0)sim0=0;

  //   float sim1=2-(float)abs_int(rgb1[1]-rgb2[1])/RGBMargin;
  //   if(sim1>1)sim1=1;
  //   if(sim1<0)sim1=0;

  //   float sim2=2-(float)abs_int(rgb1[2]-rgb2[2])/RGBMargin;
  //   if(sim2>1)sim2=1;
  //   if(sim2<0)sim2=0;


  //   //return sim0*sim1*sim2;
  //   float minSim=sim0;
  //   if(minSim>sim1)minSim>sim1;
  //   if(minSim>sim2)minSim>sim2;
  //   return minSim;
  // }

  float diff0=rgb1[0]-rgb2[0];
  float diff1=rgb1[1]-rgb2[1];
  float diff2=rgb1[2]-rgb2[2];


  float diff_avg=(diff0+diff1+diff2)/3;
  // if(abs(diff_avg)>RGBMargin)
  // {

  // }
  diff_avg*=0.8;
  diff0-=diff_avg;
  diff1-=diff_avg;
  diff2-=diff_avg;

  return (diff0*diff0+diff1*diff1+diff2*diff2)/(RGBMargin*RGBMargin*3);
}



/*
EXP: rangeL=5

0 0 0 0 2 3 5 8 8 8 8 8 9 9 9 10 12

step1 remove 0
2 3 5 8 8 8 8 8 9 9 9 10 12
     
step2 find min diffRatio
2 3 5 8 8 8 8 8 9 9 9 10 12
  | - - - - | sliding
  a         b diffRatio=a/b



*/
float sortedAccendArrayMajorParse(float *arr,int arrL,int rangeL,float *ret_diffRatio=NULL)
{
  while(*arr==0)//remove zero from the head
  {
    arr++;
    arrL--;
  }
  if(arrL<rangeL)
  {
    // LOGI("arrL<rangeL");
    return NAN;
  }
  const float centerBiasFactor=0.05;

  float minRatio=9999999;
  int minRatioIdx=-1;

  int searchL=arrL-rangeL;
  for(int i=0;i<searchL;i++)
  {
    float epsilon=centerBiasFactor*abs((float)(i-searchL/2)/(searchL/2));//bias toward center
    float ratio = arr[i+rangeL]/arr[i]+epsilon;



    if(minRatio>ratio)
    {
      minRatio=ratio;
      minRatioIdx=i;
    }
  }
  if(minRatioIdx==-1)
  {
    // LOGI("minRatioIdx==NULL");
    return NAN;
  }
  float tmp=0;
  for(int i=0;i<rangeL;i++)
  {
    tmp += arr[i+minRatioIdx];
  }
  tmp/=rangeL;

  if(ret_diffRatio)*ret_diffRatio=minRatio;
  return tmp;
}

int LineScanF(acvImage *img,int X_start,int X_L,int Y,uint8_t tarRGB[3],int RGBMargin,int *lineW_from,int *lineW_to)
{

  int i=Y;
  int maxContinuousCount=0;
  int continuousCount=0;
  int connectSkip=0;
  int line_start=X_start;
  const int skipDist =3;
  for(int j=X_start;j<X_start+X_L;j++)
  {
    uint8_t *pix = &img->CVector[i][j*3];

    bool withinMargin=RGB_Error(pix,tarRGB,RGBMargin)<1;
    
    

    if(withinMargin)
    {
      continuousCount++;
      connectSkip=skipDist;
    }
    else
    {
      if(connectSkip==0)
      {
        if(maxContinuousCount<continuousCount)
        {
          maxContinuousCount=continuousCount;
          *lineW_from=line_start;
          *lineW_to=j-skipDist;
        }
        continuousCount=0;
        line_start=j;
      }
      else
      {
        continuousCount++;
        connectSkip--;
      }
    }

  }

  if(maxContinuousCount<continuousCount)
  {
    maxContinuousCount=continuousCount;
    *lineW_from=line_start;
    *lineW_to=line_start+maxContinuousCount;
  }

  return maxContinuousCount;
}
void parseCable(acvImage *img,int W,int H,uint8_t tarRGB[3],regionInfo_single *rinfo)
{

  int lineW_min=5;
  int lineW_max=220;
  float *sgRGB=rinfo->sgRGBA;
  float RGBMargin=sqrt( (sgRGB[0]*sgRGB[0]+sgRGB[1]*sgRGB[1]+sgRGB[2]*sgRGB[2])/3 )*2;
  float regionLocalMargin=RGBMargin/3;
  vector<float> distArray;
  
  float RGBMargin_sq=RGBMargin*RGBMargin;
  int maxW_from=-1;
  int maxW_to=-1;
  int maxW=-1;
  int maxW_Y=-1;

  int pixCount=0;
  //find scanLine that fullfill lineW_min and lineW_max
  for(int i=H/3;i<H*2/3;i++)
  {
 
    int W_from=-1;
    int W_to=-1;

    int cur_maxL = LineScanF(img,0,W,i,tarRGB,RGBMargin,&W_from,&W_to);
    if(cur_maxL>lineW_min && cur_maxL<lineW_max && maxW<cur_maxL)
    {
      maxW=cur_maxL;
      maxW_from=W_from;
      maxW_to=W_to;
      maxW_Y=i;
    }
  }

  LOGI("maxW:%d,%d,%d ",maxW,lineW_min,lineW_max);
  if(maxW<lineW_min || maxW>lineW_max)
  {
    return;
  }



  int R_ave=0;
  int G_ave=0;
  int B_ave=0;
  LineScanMatchCalc(img, maxW_from, maxW_to-maxW_from, maxW_Y,rinfo);

  // LOGI("count:%f , RGB:%f,%f,%f diff:%f",rinfo->inspRes.count,rinfo->inspRes.RGBA[0],rinfo->inspRes.RGBA[1],rinfo->inspRes.RGBA[2],rinfo->inspRes.diff);
  // rinfo->inspRes.RGBA[0]/=rinfo->inspRes.count;
  // rinfo->inspRes.RGBA[1]/=rinfo->inspRes.count;
  // rinfo->inspRes.RGBA[2]/=rinfo->inspRes.count;
  // rinfo->inspRes.diff/=rinfo->inspRes.count;


  int latest_maxW_from=maxW_from;
  int latest_maxW_to=maxW_to;
  for(int i=maxW_Y-1;i>0;i--)//trace back
  {
    int W_from=latest_maxW_from;
    int W_to=latest_maxW_to;

    int cur_maxL = LineScanF(img,0,W,i,tarRGB,RGBMargin,&W_from,&W_to);
    distArray.push_back(cur_maxL);
    if(cur_maxL>lineW_min && cur_maxL<lineW_max && maxW<cur_maxL)
    {
      maxW=cur_maxL;
      maxW_from=W_from;
      maxW_to=W_to;
      maxW_Y=i;
    }


    LineScanMatchCalc(img, W_from, W_to-W_from, i,rinfo);

    // for(int j=W_from;j<W_to;j++)
    // {
    //   uint8_t *c= &img->CVector[i][3*j];
    //   c[0]=~c[0];
    //   c[1]=~c[1];
    //   c[2]=~c[2];
    // }

  }

  // LOGI("count:%f , RGB:%f,%f,%f diff:%f",rinfo->inspRes.count,rinfo->inspRes.RGBA[0],rinfo->inspRes.RGBA[1],rinfo->inspRes.RGBA[2],rinfo->inspRes.diff);
 
  latest_maxW_from=maxW_from;
  latest_maxW_to=maxW_to;
  for(int i=maxW_Y+1;i<H;i++)//trace back
  {
    int W_from=-1;
    int W_to=-1;

    int cur_maxL = LineScanF(img,0,W,i,tarRGB,RGBMargin,&W_from,&W_to);
    distArray.push_back(cur_maxL);
    if(cur_maxL>lineW_min && cur_maxL<lineW_max && maxW<cur_maxL)
    {
      maxW=cur_maxL;
      maxW_from=W_from;
      maxW_to=W_to;
      maxW_Y=i;
    }
    // for(int j=W_from;j<W_to;j++)
    // {
    //   uint8_t *c= &img->CVector[i][3*j];
    //   c[0]=~c[0];
    //   c[1]=~c[1];
    //   c[2]=~c[2];
    // }
    
    LineScanMatchCalc(img, W_from, W_to-W_from, i,rinfo);
  }



  std::sort(distArray.begin(), distArray.end());

  int rangeL=distArray.size()/3;
  const int min_RangeL=5;
  if(rangeL<min_RangeL)rangeL=min_RangeL<distArray.size()?min_RangeL:distArray.size();
  float w2w_ratio = sortedAccendArrayMajorParse(&(distArray[0]),distArray.size(),rangeL)/W;

  // for(int i=0;i<arrL;i++)
  // {
  //   printf("%0.3f,",arr[i]);
  // }
  // printf("\n");
  // LOGI("w2w_ratio:%f W:%d",w2w_ratio,W);
  rinfo->inspRes.RGBA[0]/=rinfo->inspRes.count;
  rinfo->inspRes.RGBA[1]/=rinfo->inspRes.count;
  rinfo->inspRes.RGBA[2]/=rinfo->inspRes.count;
  rinfo->inspRes.diff/=rinfo->inspRes.count;
  rinfo->inspRes.max_window2wire_width_ratio=w2w_ratio;
  /*
  
  rinfo->inspRes.count=accCount;

  rinfo->inspRes.diff+=sqrt(diff/accCount);
  rinfo->inspRes.RGBA[0]=accRGB[0]/accCount;
  rinfo->inspRes.RGBA[1]=accRGB[1]/accCount;
  rinfo->inspRes.RGBA[2]=accRGB[2]/accCount;*/
  // eeee
}

void LineParseCable2(acvImage *img,acvImage *background,acvImage *buffer,acv_XY pt1,acv_XY pt2,float region_width,regionInfo_single *rinfo,float start_ratio=0)
{
  float f_steps=acvDistance(pt1,pt2);

  acv_XY advVec={(pt2.X-pt1.X),(pt2.Y-pt1.Y)};
  advVec=acvVecNormalize(advVec);
  acv_XY nor_advVec=acvVecNormal(advVec);

  acv_XY cur_pt=acvVecInterp(pt1,pt2,start_ratio);
  cur_pt=acvVecAdd(cur_pt,acvVecMult(nor_advVec,region_width/2));

  int steps=(int)(f_steps*(1-start_ratio*2));
  int cableW=(int)region_width;



  float *sgRGB=rinfo->sgRGBA;
  float *tarRGB=rinfo->RGBA;
  float thres=(sgRGB[0]*sgRGB[0]+sgRGB[1]*sgRGB[1]+sgRGB[2]*sgRGB[2])/3*3;


  float accRGB[3]={0};
  float accCount=0;
  float diff=0;
  for(int k=0;k<steps;k++)
  {
    acv_XY region_pt =acvVecAdd(cur_pt ,acvVecMult(advVec,k));

    // LOGI("k:[%d]  region_pt:%f,%f",k,region_pt.X,region_pt.Y);
    for(int i=0;i<cableW;i++)
    {
      int X = round(region_pt.X);
      int Y = round(region_pt.Y);

      uint8_t *pix = pixFetching(img,X,Y);
      if(pix==NULL)continue;
      buffer->CVector[k][i*3]=pix[0];
      buffer->CVector[k][i*3+1]=pix[1];
      buffer->CVector[k][i*3+2]=pix[2];

      // float dR=tarRGB[0]-pix[2];//the color order im acvImage is inversed
      // float dG=tarRGB[1]-pix[1];
      // float dB=tarRGB[2]-pix[0];
      // // printf("<%f,%d,%f>",tarRGB[0],pix[0],dB);
      // float diff3=(dR*dR+dG*dG+dB*dB)/3;
      // if(diff3<thres)
      // {
      //   diff+=diff3;
      //   accRGB[0]+=pix[2];
      //   accRGB[1]+=pix[1];
      //   accRGB[2]+=pix[0];
      //   accCount++;
      // }
      // pix[0]=0;

      // printf("<%d,%d>",X,Y);
      region_pt=acvVecSub(region_pt,nor_advVec);
    }
    // printf("\n");
  }

  // rinfo->inspRes.count=accCount;

  // rinfo->inspRes.diff+=sqrt(diff/accCount);
  // rinfo->inspRes.RGBA[0]=accRGB[0]/accCount;
  // rinfo->inspRes.RGBA[1]=accRGB[1]/accCount;
  // rinfo->inspRes.RGBA[2]=accRGB[2]/accCount;
  // parseCable(buffer,cableW,steps,(acv_XY){0,0},rinfo);
  uint8_t rgbArr[]={(uint8_t)tarRGB[2],(uint8_t)tarRGB[1],(uint8_t)tarRGB[0]};

  rinfo->inspRes.count=0;
  rinfo->inspRes.diff=0;
  rinfo->inspRes.max_window2wire_width_ratio=0;
  rinfo->inspRes.RGBA[0]=0;
  rinfo->inspRes.RGBA[1]=0;
  rinfo->inspRes.RGBA[2]=0;
  rinfo->inspRes.RGBA[3]=0;
  parseCable(buffer,cableW,steps,rgbArr,rinfo);

}


int findFirstSignificantValue(vector<float> &val,int stablizeCount,bool ForB=true,float sigmaThres=3)
{
  int idxPadding=ForB?0:val.size()-1;
  int idxMult=ForB?1:-1;

  sigmaThres=sigmaThres*sigmaThres;

  float mean=0;
  float sigma=0;
  for(int i=0;i<stablizeCount;i++)
  {
    float v= val[idxMult*i+idxPadding];
    mean+=v;
    sigma+=v*v;
  }
  for(int i=stablizeCount;i<val.size();i++)
  {
    
    float v= val[idxMult*i+idxPadding];
    float curMean=mean/i;
    float curSigma_sq=sigma/i-curMean*curMean;

    float diff= (v-(mean/i));
    diff=diff*diff;
    if(curSigma_sq<1)curSigma_sq=1;

    
    // LOGI("[%d]:d:%f,si:%f,sixt:%f",i,diff,curSigma_sq,curSigma_sq*sigmaThres);
    if(diff>curSigma_sq*sigmaThres)
    {
      return idxMult*i+idxPadding;
    }

  }
  return -1;
}


static bool ptInfo_tmp_comp(const ContourFetch::ptInfo &a, const ContourFetch::ptInfo &b)
{
  return a.tmp < b.tmp;
}

acv_Line pointsFitLine(vector<ContourFetch::ptInfo> &s_points,float *ret_sigma)
{
  
  acv_Line line_cand;
  float sigma;
  
  int sptL = s_points.size();

  float minS_pts = 0;
  float minSigma = 99999;
  
  int sampleL = s_points.size() / 10 + 3;
  if (sampleL > s_points.size())
  {
    sampleL = s_points.size() / 10;
  }
  if (sampleL > 20)
  {
    sampleL = 20;
  }
  for (int m = 0; m < 17; m++) //RANSAC find a good starting line
  {
    for (int k = 0; k < sampleL; k++) //Shuffle in
    {
      int idx2Swap = (rand() % (s_points.size() - k)) + k;

      ContourFetch::ptInfo tmp_pt = s_points[k];
      s_points[k] = s_points[idx2Swap];
      s_points[idx2Swap] = tmp_pt;
      //s_points[j].edgeRsp=1;
    }
    float sigma;
    acv_Line tmp_line;
    acvFitLine(
        &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
        &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo), sampleL, &tmp_line, &sigma);

    int sigma_count = 0;
    float sigma_sum = 0;
    for (int i = 0; i < s_points.size(); i++)
    {
      float diff = acvDistance_Signed(tmp_line, s_points[i].pt);
      float abs_diff = (diff < 0) ? -diff : diff;
      // if (abs_diff > 5)
      // {
      //   //s_points[j].edgeRsp=0;
      //   continue;
      // }
      sigma_count++;
      sigma_sum += diff * diff;
    }
    sigma_sum = sqrt(sigma_sum / sigma_count);
    if (minSigma > sigma_sum)
    {
      minS_pts = sigma_count;
      minSigma = sigma_sum;
      line_cand = tmp_line;
      //LOGI("minSigma:%f",minSigma);
    }
  }

  int usable_L = 0;
  for (int k = 0; k < 2; k++)
  {
    usable_L = 0;
    if (s_points.size() == 0)
    {
      line_cand.line_vec.X=
      line_cand.line_vec.Y=
      line_cand.line_anchor.Y=
      line_cand.line_anchor.X=NAN;
      if(ret_sigma)*ret_sigma=NAN;
      return line_cand;
    }

    for (int n = 0; n < s_points.size(); n++)
    {
      float dist = acvDistance_Signed(line_cand, s_points[n].pt);
      if (dist < 0)
        dist = -dist;
      s_points[n].tmp = dist;
      //s_points[i].edgeRsp=1;
    }

    std::sort(s_points.begin(), s_points.end(), ptInfo_tmp_comp); //
    // LOGI("distThres:%f",s_points[s_points.size()*2 / 3].tmp);
    float distThres = s_points[s_points.size() *2/ 3].tmp + 3;
    
    // float distThres = s_points[s_points.size() / 3].tmp*1.1;
    LOGV("sort finish size:%d, distThres:%f", s_points.size(), distThres);

    usable_L = s_points.size();

    //usable_L=usable_L*10/11;//back off
    LOGV("usable_L:%d/%d  minSigma:%f=>%f",
          usable_L, s_points.size(),
          s_points[usable_L - 1].tmp, distThres);
    acvFitLine(
        &(s_points[0].pt), sizeof(ContourFetch::ptInfo),
        &(s_points[0].edgeRsp), sizeof(ContourFetch::ptInfo),
        usable_L, &line_cand, &sigma);
  
  }
    
  if(ret_sigma)*ret_sigma=sigma;
  return line_cand;
}


float LineXYSpace(acvImage *img,acv_XY p1,acv_XY p2,float step_count,vector<acv_XY> &pixXY)
{
  acv_XY vec=acvVecSub(p2,p1);
  float adv_steps=(step_count==step_count)?step_count:max(abs(vec.X),abs(vec.Y));

  acv_XY adv_vec=acvVecMult(vec,1/adv_steps);
  acv_XY pt_cur=p1;
  
  pt_cur=acvVecAdd(pt_cur,adv_vec);
  for(int i=0;i<adv_steps;i++)
  {
    pixXY.push_back(pt_cur);
    pt_cur=acvVecAdd(pt_cur,adv_vec);
  }
  

  return adv_steps;
}

float LinePixDiffSampling(acvImage *img,acv_XY p1,acv_XY p2,float step_count,vector<float> &PixDiff)
{
  

  vector<acv_XY> pixXY;
  step_count = LineXYSpace(img,p1,p2,step_count,pixXY);
  
  uint8_t *prePixel=pixFetching(img,pixXY[0]);
  
  for(int i=1;i<pixXY.size();i++)
  {

    uint8_t *curPixel=pixFetching(img,pixXY[i]);

    float diff =0;
    if(prePixel!=NULL && curPixel!=NULL)
    {
      int diff0 = prePixel[0]-curPixel[0];
      int diff1 = prePixel[1]-curPixel[1];
      int diff2 = prePixel[2]-curPixel[2];
      diff = sqrt((diff0*diff0+diff1*diff1+diff2*diff2)/3);
      
    }
    // LOGI("[%d]:%0.1f,%0.1f diff:%0.1f",i,pixXY[i].X,pixXY[i].Y,diff);

    PixDiff.push_back(diff);
    prePixel = curPixel;

  }
  return step_count;
}

float LineFindChangeDist(acvImage *img,acv_XY p1,acv_XY p2,float clearDist,float sigmaThres = 3.0F)
{
  
  vector<float> PixDiff;
  // LOGI(">>>");
  float step_count =LinePixDiffSampling(img,p1,p2,NAN,PixDiff);
  // for(int i=0;i<PixDiff.size();i++)
  // {
    
  //   LOGI("[%d]:%f",i,PixDiff[i]);
  // }
  
  float fullDist = acvDistance(p2,p1);

  int stablizeCount=ceil(clearDist*step_count/fullDist/2);
  
  int idxF = findFirstSignificantValue(PixDiff,stablizeCount,true,sigmaThres);

  if(idxF<0)return NAN;


  // LOGI(">>>idxF:%d::%f",idxF,(idxF+0.5)*fullDist/step_count);

  return (idxF+0.5)*fullDist/step_count;
}




int findHeadPinchPoints(acvImage *img,acv_XY pt_anchor,acv_XY vec_btm,float expand_ratio,float offset_ratio,acv_XY *ret_ppts)
{
  if(ret_ppts==NULL)return -1;
  acv_XY offset_vec=acvVecMult({vec_btm.Y,-vec_btm.X},offset_ratio);


  acv_XY pt_anchor_expandBack = acvVecAdd(pt_anchor,acvVecMult(vec_btm,-(expand_ratio-1)/2));
  acv_XY pt1=acvVecAdd(pt_anchor_expandBack,offset_vec);
  acv_XY pt2=acvVecAdd(pt1,acvVecMult(vec_btm,expand_ratio));
  acv_XY ptm=acvVecMult(acvVecAdd(pt1,pt2),0.5);

  float fullDist=acvDistance(pt2,pt1);


  float clearDist = fullDist*(expand_ratio-1)/2/2;
  
  // LOGI("anc_btm:%f,%f,  %f,%f  pt1m: %f,%f,  %f,%f  \n",pt_anchor.X,pt_anchor.Y,vec_btm.X,vec_btm.Y,pt1.X,pt1.Y,ptm.X,ptm.Y);
  float cdist  = LineFindChangeDist(img,pt1,ptm,clearDist,5);
  ret_ppts[0]=acvVecSub(acvVecAdd(pt1,acvVecMult(acvVecSub(ptm,pt1),cdist/(fullDist/2))),offset_vec);

  cdist        = LineFindChangeDist(img,pt2,ptm,clearDist,5);
  ret_ppts[1]=acvVecSub(acvVecAdd(pt2,acvVecMult(acvVecSub(ptm,pt2),cdist/(fullDist/2))),offset_vec);

  return 0;
}


acv_Line refineBtmVec(acvImage *img,acv_XY pt_anchor,acv_XY vec_btm,int searchMargin)
{

  acv_XY _pt_anchor=pt_anchor;
  acv_XY _vec_btm=vec_btm;

  acv_XY searchVec=acvVecNormalize({_vec_btm.Y,-_vec_btm.X});

  acv_XY pt_start=acvVecAdd(_pt_anchor,acvVecMult(searchVec,-searchMargin));
  float adv_steps = max(abs(vec_btm.X),abs(vec_btm.Y));
  acv_XY adv_vec=acvVecMult(_vec_btm,1/adv_steps);
  
  
  float clearDist=searchMargin/4;
  vector<ContourFetch::ptInfo> pixXY;
  acv_XY pt_cur=pt_start;
  for(int i=0;i<adv_steps;i++)
  {

    acv_XY pt_to=acvVecAdd(pt_start ,acvVecMult(searchVec,2*searchMargin));


    // LOGI("[%d]:%f,%f  ,%f,%f",i,pt_start.X,pt_start.Y,  pt_to.X,pt_to.Y);
    float cdist  = LineFindChangeDist(img,pt_start,pt_to,clearDist,2);

    if(cdist==cdist)
    {
      // cdist=16;
      pt_to=acvVecAdd(pt_start,acvVecMult(searchVec,cdist));
      ContourFetch::ptInfo pt;
      pt.pt=pt_to;
      pt.edgeRsp=1;
      pixXY.push_back(pt);
    }
    pt_start=acvVecAdd(pt_start,adv_vec);

    // LOGI("[%d]:%f,%f,%f",i,pt_to.X,pt_to.Y,cdist);
  }

  if(pixXY.size()<adv_steps*7/10)//less than 70% avaliable 
  {
    acv_Line line={NAN};
    return line;
  }



  float sigma;
  acv_Line line = pointsFitLine(pixXY,&sigma);
  
  // LOGI("LINE:%f,%f  %f,%f  sigma:%f",line.line_anchor.X,line.line_anchor.Y,line.line_vec.X,line.line_vec.Y,sigma);
  line.line_anchor= acvClosestPointOnLine(pt_anchor,line);
  if(acv2DDotProduct(line.line_vec,vec_btm)<0)
  {
    line.line_vec=acvVecMult(line.line_vec,-1);
  }
  
  line.line_vec=acvVecMult(line.line_vec,hypot(vec_btm.X,vec_btm.Y));
  // LOGI("line:%f,%f  %f,%f",pt_anchor.X,pt_anchor.Y,vec_btm.X,vec_btm.Y);
  // LOGI("LINE:%f,%f  %f,%f  sigma:%f",line.line_anchor.X,line.line_anchor.Y,line.line_vec.X,line.line_vec.Y,sigma);
  return line;
}



void colorCompare(colorInfo *arr1, colorInfo *arr2, int arrL,float *ret_maxDiff,int *ret_maxDiffIdx)
{
  float minScore=99;
  float minBriRatio=1;
  float maxDiffSq=0;
  int maxDiffIdx=-1;
  for(int i=0;i<arrL;i++)
  {
    float diffR=arr1[i].R-arr2[i].R;
    float diffG=arr1[i].G-arr2[i].G;
    float diffB=arr1[i].B-arr2[i].B;

    float mean = (diffR+diffG+diffB)/3;
    diffR-=mean;
    diffG-=mean;
    diffB-=mean;

    float diffSq=diffR*diffR+diffG*diffG+diffB*diffB;
    if(maxDiffSq<diffSq)
    {
      maxDiffSq=diffSq;
      maxDiffIdx=i;
    }


    // float tarMag = tar_ci[i].R*tar_ci[i].R+tar_ci[i].G*tar_ci[i].G+tar_ci[i].B*tar_ci[i].B;
    // float curMag = ci[i].R*ci[i].R+ci[i].G*ci[i].G+ci[i].B*ci[i].B;

    // float briRatio=curMag/tarMag;
    // if(briRatio>1)briRatio=1/briRatio;
    // float dotP   =(tar_ci[i].R*ci[i].R+tar_ci[i].G*ci[i].G+tar_ci[i].B*ci[i].B)/sqrt(tarMag*curMag);

    // if(minScore>dotP)
    // {
    //   minScore=dotP;
    // }
    // if(minBriRatio>briRatio )
    // {
    //   minBriRatio=briRatio;
    // }
    
    // printf("{.R=%f,.G=%f, .B=%f}, maxDiffSq:%f\n",ci[i].R,ci[i].G,ci[i].B, sqrt(diffSq));
  }
  minBriRatio=sqrt(minBriRatio);
  maxDiffSq=sqrt(maxDiffSq);
  printf("\n");
  LOGI("minScore=%f, minBriRatio=%f maxDiffSq=%f",minScore, minBriRatio,maxDiffSq);
  if(ret_maxDiff)*ret_maxDiff=maxDiffSq;
  if(ret_maxDiffIdx)*ret_maxDiffIdx=maxDiffIdx;
}

#define SETSPARAM_NUMBER(json,structVarAssign,pName) {double *tmpN; if((tmpN=JFetch_NUMBER(json,pName))) structVarAssign*tmpN;}

#define RETSPARAM_NUMBER(json,structVar,pName) {cJSON_AddNumberToObject(json, pName, structVar);}

acv_XY readXY(cJSON *jsonParam)
{
  
  acv_XY xy={NAN,NAN};
  double* num=JFetch_NUMBER(jsonParam,"x");
  if(num!=NULL)xy.X=*num;
  else return xy;


  num=JFetch_NUMBER(jsonParam,"y");
  if(num!=NULL)xy.Y=*num;
  else{
    xy.X=xy.Y=NAN;
    return xy;
  }  
  return xy;
}

double JFetch_NUMBER_V(cJSON *json, char* path)
{
  double* p_n=JFetch_NUMBER(json,path);
  if(p_n==NULL)return NAN;
  return *p_n;
}

cJSON * FeatureManager_gen::SetParam0(cJSON *jsonParam)
{
#define SETSPARAM_INT_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=(int),pName)
#define SETSPARAM_DOU_NUMBER(json,structVar,pName) SETSPARAM_NUMBER(json,structVar=,pName)
#define SETPARAM_INT_NUMBER(json,pName) SETSPARAM_INT_NUMBER(json,this->pName,#pName)
#define SETPARAM_DOU_NUMBER(json,pName) SETSPARAM_DOU_NUMBER(json,this->pName,#pName)
#define RETPARAM_NUMBER(json,pName) RETSPARAM_NUMBER(json,this->pName,#pName)
  
  SETPARAM_INT_NUMBER(jsonParam,inspectionStage);
  
  SETPARAM_INT_NUMBER(jsonParam,HFrom);
  SETPARAM_INT_NUMBER(jsonParam,HTo);
  SETPARAM_INT_NUMBER(jsonParam,SMax);
  SETPARAM_INT_NUMBER(jsonParam,SMin);
  SETPARAM_INT_NUMBER(jsonParam,VMax);
  SETPARAM_INT_NUMBER(jsonParam,VMin);



  SETPARAM_INT_NUMBER(jsonParam,boxFilter1_Size);
  SETPARAM_INT_NUMBER(jsonParam,boxFilter1_thres);
  SETPARAM_INT_NUMBER(jsonParam,boxFilter2_Size);
  SETPARAM_INT_NUMBER(jsonParam,boxFilter2_thres);



  SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatio);
  SETPARAM_DOU_NUMBER(jsonParam,minHeadArea);
  SETPARAM_DOU_NUMBER(jsonParam,targetHeadWHRatioMargin);
  SETPARAM_DOU_NUMBER(jsonParam,FacingThreshold);
  

  SETPARAM_DOU_NUMBER(jsonParam,cableSeachingRatio);
  SETPARAM_INT_NUMBER(jsonParam,cableCount);
  SETPARAM_INT_NUMBER(jsonParam,cableTableCount);

  int vType = getDataFromJson(jsonParam, "backgroundFlag", NULL);
  if(vType==cJSON_True)
  {
    backgroundFlag=true;
  }
  else if(vType==cJSON_False)
  {
    backgroundFlag=false;
  }

  acv_XY vec_btm =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_btm")); 
  acv_XY vec_side =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.vec_side")); 
  acv_XY pt_cornor =readXY(JFetch_OBJECT(jsonParam,"regionInfo.anchorInfo.pt_cornor")); 


  regionInfo.resize(0);
  if(!isnan(vec_btm.X) && !isnan(vec_side.X) && !isnan(pt_cornor.X))
  {
    LOGI("GO................");
    
    float L_btm=hypot(vec_btm.Y,vec_btm.X);
    for(int i=0;;i++)
    {
      char tmpPath[50];
      sprintf(tmpPath,"regionInfo.regions[%d]",i);
      cJSON *region = JFetch_OBJECT(jsonParam,tmpPath);
      if(region==NULL)break;
      acv_XY pt1 =readXY(JFetch_OBJECT(region,"pt1")); 
      acv_XY pt2 =readXY(JFetch_OBJECT(region,"pt2")); 
      double* p_margin = JFetch_NUMBER(region,"margin");
      double* p_id = JFetch_NUMBER(region,"id");
      double margin=p_margin==NULL?NAN:*p_margin;
      int id=p_id==NULL?-1:(int)*p_id;
      regionInfo_single rIs;
      rIs.normalized_pt1=rIs.pt1=pt1;
      rIs.normalized_pt1.X/=L_btm;
      rIs.normalized_pt1.Y/=L_btm;

      rIs.normalized_pt2=rIs.pt2=pt2;
      rIs.normalized_pt2.X/=L_btm;
      rIs.normalized_pt2.Y/=L_btm;
      rIs.margin=margin;
      rIs.id=id;

      rIs.RGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[0]");
      rIs.RGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[1]");
      rIs.RGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.RGBA[2]");
      rIs.sgRGBA[0]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[0]");
      rIs.sgRGBA[1]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[1]");
      rIs.sgRGBA[2]=(float)JFetch_NUMBER_V(region,"colorInfo.sigma[2]");


      regionInfo.push_back(rIs);
      // LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,pt1.X,pt1.Y,pt2.X,pt2.Y);
      LOGI("[%d]   ID:%d, margin:%f   %f,%f> %f,%f",i,id,margin,rIs.normalized_pt1.X,rIs.normalized_pt1.Y,rIs.normalized_pt2.X,rIs.normalized_pt2.Y);
    }


  }


  cJSON* ret_jobj = NULL;
  if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  {
    ret_jobj = cJSON_CreateObject();
    RETPARAM_NUMBER(ret_jobj,HFrom);
    RETPARAM_NUMBER(ret_jobj,HTo);
    RETPARAM_NUMBER(ret_jobj,SMax);
    RETPARAM_NUMBER(ret_jobj,SMin);
    RETPARAM_NUMBER(ret_jobj,VMax);
    RETPARAM_NUMBER(ret_jobj,VMin);

    RETPARAM_NUMBER(ret_jobj,boxFilter1_Size);
    RETPARAM_NUMBER(ret_jobj,boxFilter1_thres);
    RETPARAM_NUMBER(ret_jobj,boxFilter2_Size);
    RETPARAM_NUMBER(ret_jobj,boxFilter2_thres);

    RETPARAM_NUMBER(ret_jobj,targetHeadWHRatio);
    RETPARAM_NUMBER(ret_jobj,minHeadArea);
    RETPARAM_NUMBER(ret_jobj,targetHeadWHRatioMargin);
    RETPARAM_NUMBER(ret_jobj,FacingThreshold);

    RETPARAM_NUMBER(ret_jobj,cableSeachingRatio);
    RETPARAM_NUMBER(ret_jobj,cableCount);
    RETPARAM_NUMBER(ret_jobj,cableTableCount);
  }
  // float cableSeachingRatio=0.2;


  // const int cableCount=12;
  return ret_jobj;
  // const int cableTableCount=2;
}

cJSON * FeatureManager_gen::SetParam1(cJSON *jsonParam)
{
  
  // SETSPARAM_INT_NUMBER(jsonParam,insp02.inspectionType,"inspectionType");
  SETSPARAM_NUMBER(jsonParam,insp02.pos.X=(int),"pos.X");
  SETSPARAM_NUMBER(jsonParam,insp02.pos.Y=(int),"pos.Y");


  cJSON* ret_jobj = NULL;
  if (getDataFromJson(jsonParam, "get_param", NULL) == cJSON_True)
  {
    ret_jobj = cJSON_CreateObject();
  }
  return ret_jobj;
}

cJSON * FeatureManager_gen::SetParam(cJSON *jsonParam)
{
  char* jsonStr = cJSON_Print(jsonParam);
  LOGI("%s..",jsonStr);
  delete jsonStr;
  // exit(-1);
  // double *tmpN;
  // if((tmpN=JFetch_NUMBER(jsonParam,"HFrom")))
  //   HFrom=(int)*tmpN;

  SETSPARAM_INT_NUMBER(jsonParam,this->inspectionType,"inspectionType");
  switch(inspectionType)
  {
    case 0:
    return SetParam0(jsonParam);

    case 1:
    return SetParam1(jsonParam);


  }
  return NULL;
}


float VEC_SAMP_REACHING_CABLE_MOMENT(acvImage *img,acv_XY v1,acv_XY v2,acv_XY pt_anchor,float len_btm,float reachingSpaceRatio,float *ret_statMoment2=NULL)
{

    

  acv_XY v2Half=acvVecMult(v2,reachingSpaceRatio);
  float scanSize=0.8;
  acv_XY vec_p1=acvVecAdd(v2Half,pt_anchor);


  
  acv_XY vec_p2=acvVecAdd(vec_p1,v1);
  acv_XY vecMiddle=acvVecMult( acvVecAdd(vec_p1,vec_p2),0.5);
  acv_XY reachVec=acvVecMult(acvVecSub(vec_p2,vecMiddle),(1-scanSize)/2);

  vec_p1=acvVecSub(vecMiddle,reachVec);
  vec_p2=acvVecAdd(vecMiddle,reachVec);

  float statMoment[2];
  LineState(img,vec_p1,vec_p2,len_btm,0,statMoment);
  float m2_side1=statMoment[1]/statMoment[0];
  if(ret_statMoment2)
  {
    ret_statMoment2[0]=statMoment[0];
    ret_statMoment2[1]=statMoment[1];
  }
  return statMoment[1];
}

int FeatureManager_gen::FeatureMatching(acvImage *img)
{

  ClearReport();
  cJSON *jsonRep=cJSON_CreateObject();
  
  cJSON_AddStringToObject(jsonRep, "type", GetFeatureTypeName());
  report.data.cjson_report.cjson=jsonRep;
  // LOGI("GOGOGOGOGOGGO....inspectionStage:%d",inspectionStage);


  switch(inspectionType)
  {
    case 0:
    return FeatureMatching0(img);

    case 1:
    return FeatureMatching1(img);


  }
  return -1;
}

int FeatureManager_gen::FeatureMatching1(acvImage *img)
{

  cJSON *jsonRep=report.data.cjson_report.cjson;

  return -1;
}

int FeatureManager_gen::FeatureMatching0(acvImage *img)
{
  
  float cableRatio=0.074;
  float maxDiffMargin=24;


  // inspectionStage=1;



  cJSON *jsonRep=report.data.cjson_report.cjson;



  if(backgroundFlag)
  {
    backGroundTemplate.ReSize(img);
    acvCloneImage(img,&backGroundTemplate,-1);
    backgroundFlag=false;
    // return -1;
  }

  // for(int i=0; i<img->GetHeight(); i++)
  // {
  //   uint8_t *ImLine=img->CVector[i];
  //   for(int j=0; j<img->GetWidth(); j++)
  //   {
  //     ImLine[2]=((j)%img->GetWidth())*250/img->GetWidth();
  //     ImLine[1]=255;//i*256/img->GetHeight();
  //     ImLine[0]=255;
  //     img->RGBFromHSV(ImLine,ImLine);
  //     // img->HSVFromRGB(ImLine,ImLine);

      


  //     // img->RGBFromHSV(ImLine,ImLine);
  //     // HSVFromRGB(ImLine,ImLine);
  //     ImLine+=3;
  //   }
  // }


  if(inspectionStage==0)
  {
    return -1;
  }

  buf1.ReSize(img);
  buf2.ReSize(img);
  ImgOutput.ReSize(img);
  acvCloneImage(img,&buf1,-1);
  buf1.RGBToHSV();
  acvCloneImage(&buf1,&buf2,-1);


  if(inspectionStage==1)
  {
    acvCloneImage(&buf2,img,-1);
    return -1;
  }
  // acvSaveBitmapFile("step0H.bmp", &buf2);
  // acvHSVThreshold(&buf2,0,256,256,128,255,50);
  acvHSVThreshold(&buf2,HFrom,HTo,SMax,SMin,VMax,VMin); //0V ~255  1S ~255  2H ~252


  if(inspectionStage!=2)
  {
  acvBoxFilter(&ImgOutput,&buf2,boxFilter1_Size);
  acvThreshold(&buf2,boxFilter1_thres);
  acvBoxFilter(&ImgOutput,&buf2,boxFilter2_Size);
  acvThreshold(&buf2,boxFilter2_thres);
  }
  // acvSaveBitmapFile("step0.bmp", &buf2);
  acvTurn(&buf2);

  LOGI("Start...");
  // acvDrawBlock(&buf2, 2, 2, buf2.GetWidth() - 3, buf2.GetHeight() - 3,0,0,0,10);
  // acvHSVThreshold(&buf2,0,256,255,20,200,0); //0V ~255  1S ~255  2H ~252
  acvComponentLabeling(&buf2);
  
  vector<acv_LabeledData> ldData;
  acvLabeledRegionInfo(&buf2, &ldData);

  if(inspectionStage==3 || inspectionStage==2)
  {
    acvLabeledColorDispersion(img,&buf2,10);
    return -1;
  }
  // acvLabeledColorDispersion(&buf2,&buf2,5);
  // acvCloneImage(&buf2,img,-1);
  // acvLabeledColorDispersion(&ImgOutput,&buf2,105);
  // acvThreshold(&buf1,128);
  // acvSaveBitmapFile("step1.bmp", &buf1);
  // acvSaveBitmapFile("step2.bmp", &buf2);

  // acvSaveBitmapFile("step2.bmp", &buf2);
  vector<acv_XY> contour;
  vector<int> convexHullIdx;

  cJSON* reports_jarr = cJSON_CreateArray();
  cJSON_AddItemToObject(jsonRep,"reports",reports_jarr);

  float radiousMargin_sq=img->GetWidth()/2*0.8;
  acv_XY imgCen={(float)img->GetWidth()/2,(float)img->GetHeight()/2};
  for(int i=1;i<ldData.size();i++)
  {
    if(ldData[i].area<100)continue;
    acv_XY cen_dist=acvVecSub(ldData[i].Center,imgCen);

    if(hypot(cen_dist.X,cen_dist.Y)>radiousMargin_sq)continue;

    LOGI("acvOuterContourExtraction..");
    bool isOK =  acvOuterContourExtraction(&buf2, ldData[i], i,contour);


    ComputeConvexHull(&(contour[0]), contour.size(),convexHullIdx);
    
    for(int k=0;k<convexHullIdx.size();k++)
    {
      contour[k]=contour[convexHullIdx[k]];
    }
    
    LOGI("rotatingCalipers..");
    contour.resize(convexHullIdx.size());
    float out[6]={0}; 
    rotatingCalipers( &(contour[0]), contour.size(),CALIPERS_MINAREARECT, out );

    // LOGI("out:%f %f %f %f %f %f",out[0],out[1],out[2],out[3],out[4],out[5]);


    float len_btm=hypot(out[2],out[3]);
    float len_side=hypot(out[4],out[5]);

    LOGI("GOGOGOGOGOGGO..[%d]..%f %f",i,ldData[i].Center.X,ldData[i].Center.Y);
    
    if(len_btm*len_side<minHeadArea)continue;

    acv_XY pt_anchor={out[0],out[1]};
    acv_XY vec_btm={out[2],out[3]};
    acv_XY vec_side={out[4],out[5]};
    if(len_side>len_btm)
    {
      acv_XY tmp=vec_btm;
      vec_btm=vec_side;
      vec_side=tmp;
      float tmpLen=len_btm;
      len_btm=len_side;
      len_side=tmpLen;

    }

    {
      float ratio = len_btm/len_side;
      ratio/=targetHeadWHRatio;//close to 4.0
      if(ratio<1)ratio=1/ratio;
      // printf("ratio:%f   len1:%f len_side:%f\n",ratio,len_btm,len_side);

      LOGI("GOGOGOGOGOGGO.ratio:%f targetHeadWHRatioMargin:%f...",ratio,targetHeadWHRatioMargin);
      // if(ratio>targetHeadWHRatioMargin)
      // {
      //   continue;
      // }
      
      float m2=NAN;
      {
        acv_XY vec_sideHalf=acvVecMult(vec_side,0.5);
        acv_XY vec_p1=acvVecAdd(vec_sideHalf,pt_anchor);
        acv_XY vec_p2=acvVecAdd(vec_p1,vec_btm);

        vec_p1=acvVecAdd(vec_p1,acvVecMult(vec_btm,0.2));
        vec_p2=acvVecSub(vec_p2,acvVecMult(vec_btm,0.2));
        float statMoment[2];


        LineState(img,vec_p1,vec_p2,len_btm*2,0,statMoment);
        m2=statMoment[1]/statMoment[0];
      }


      // float VEC_SAMP_REACHING_CABLE_MOMENT(acvImage *img,acv_XY v1,acv_XY v2,acv_XY pt_anchor,float len_btm,float reachingSpaceRatio,float *ret_statMoment2)
      float m1_side1=VEC_SAMP_REACHING_CABLE_MOMENT(img,vec_side,vec_btm,pt_anchor,len_side,-cableSeachingRatio);
      if(m1_side1!=m1_side1)m1_side1=0;
      float m1_side2=VEC_SAMP_REACHING_CABLE_MOMENT(img,vec_side,vec_btm,pt_anchor,len_side,1+cableSeachingRatio);
      if(m1_side2!=m1_side2)m1_side2=0;

      float m1_sideMax=m1_side1>m1_side2?m1_side1:m1_side2;

      float m2_side1=VEC_SAMP_REACHING_CABLE_MOMENT(img,vec_btm,vec_side,pt_anchor,len_btm,-cableSeachingRatio);
      if(m2_side1!=m2_side1)m2_side1=0;

      float m2_side2=VEC_SAMP_REACHING_CABLE_MOMENT(img,vec_btm,vec_side,pt_anchor,len_btm,1+cableSeachingRatio);
      if(m2_side2!=m2_side2)m2_side2=0;

      float m2_sideMax=m2_side1>m2_side2?m2_side1:m2_side2;

      LOGI("m11:%f m12:%f  m21:%f  m22:%f\n",m1_side1,m1_side2,m2_side1,m2_side2);

      // if(m1_side1==0 || m1_side2==0 ||m2_side1==0 || m2_side2==0 )
      // {
      //   continue;
      // }
      // if(m1_sideMax<m2_sideMax)//gives the main direction
      // {
      //   if(len_side>len_btm)
      //   {
      //     acv_XY tmp=vec_btm;
      //     vec_btm=vec_side;
      //     vec_side=tmp;
      //     float tmpLen=len_btm;
      //     len_btm=len_side;
      //     len_side=tmpLen;

      //   }
      //   m2_side1=m1_side1;
      //   m2_side2=m1_side2;
      // }
      if(m1_sideMax<7 && m2_sideMax<7)
      {
        continue;
      }
      if(m1_sideMax>m2_sideMax)
      {

        acv_XY tmp=vec_btm;
        vec_btm=vec_side;
        vec_side=tmp;
        float tmpLen=len_btm;
        len_btm=len_side;
        len_side=tmpLen;
        m2_side1=m1_side1;
        m2_side2=m1_side2;
      }
      if(m2_side1>m2_side2)
      { //          ______     _____
        //could be |        or      |
        
        pt_anchor=acvVecAdd(pt_anchor,vec_side);
        vec_side=acvVecMult(vec_side,-1);
        float tmp=m2_side1;
        m2_side1=m2_side2;
        m2_side2=tmp;//swap side info
      }


    }
    bool isFrontFace=true;


    float axis_crossP=-1*acv2DCrossProduct(vec_btm,vec_side);


    if(!isFrontFace)
    {
      axis_crossP*=-1;
    }
      /* axis direction in image corrdinate (ie y  downward )
      target: Face side
      
         | | | | | | |
      |
      |__________________
      
      */
    // if(m2_side1>m2_side2)//cable is on side 1
    {
      //could be |____    or _____|
      if(axis_crossP<0)
      {
        pt_anchor=acvVecAdd(pt_anchor,vec_btm);
        vec_btm=acvVecMult(vec_btm,-1);
      }
      else
      {
        //perfect
      }
    }



    LOGI("<<pt_anchor %f,%f   vec_btm  %f,%f",pt_anchor.X,pt_anchor.Y,vec_btm.X,vec_btm.Y);

    if(1)
    {

      acv_Line btmLine=refineBtmVec(img,pt_anchor,vec_btm,30);


      pt_anchor=btmLine.line_anchor;
      vec_btm=btmLine.line_vec;
      LOGI(">>pt_anchor %f,%f   vec_btm  %f,%f",pt_anchor.X,pt_anchor.Y,vec_btm.X,vec_btm.Y);


      if(isnan(acv2DDotProduct(pt_anchor,vec_btm)))
      {
        
        LOGI("refineBtmVec failed");
        continue;
      }
    }


    if(1)

    {

      acv_XY ppts[2]={NAN};
      int ret = findHeadPinchPoints(img,pt_anchor,vec_btm,1.5,0.2,ppts);
      pt_anchor=ppts[0];
      vec_btm=acvVecSub(ppts[1],ppts[0]);
      LOGI(">>pt_anchor %f,%f   vec_btm  %f,%f",pt_anchor.X,pt_anchor.Y,vec_btm.X,vec_btm.Y);
      LOGI("findHeadPinchPoints:0.1  %f,%f  %f,%f",ppts[0].X,ppts[0].Y,ppts[1].X,ppts[1].Y);


      if(isnan(acv2DDotProduct(pt_anchor,vec_btm)))
      {
        
        LOGI("findHeadPinchPoints failed");
        continue;
      }

    }


    vec_side={vec_btm.Y,-vec_btm.X};

    {


      cJSON *singleRep=cJSON_CreateObject();
      
      cJSON_AddItemToArray(reports_jarr,singleRep);



      
      cJSON_AddBoolToObject(singleRep, "front_facing", isFrontFace);
      cJSON* inspArray = cJSON_AddArrayToObject(singleRep, "inspection_report");


      acv_XY btm_center={pt_anchor.X+vec_btm.X/2,pt_anchor.Y+vec_btm.Y/2};
      float L_btm=hypot(vec_btm.X,vec_btm.Y);
      float angle=atan2(vec_btm.Y,vec_btm.X);
      float flipX=1;
      for(int i=0;i<regionInfo.size();i++)
      {
        acv_XY pt1=regionInfo[i].normalized_pt1;
        pt1.Y*=L_btm;
        pt1.X*=L_btm*flipX;
        pt1 =acvVecAdd(acvRotation(angle,pt1),btm_center);
        acv_XY pt2=regionInfo[i].normalized_pt2;
        pt2.Y*=L_btm;
        pt2.X*=L_btm*flipX;
        pt2 = acvVecAdd(acvRotation(angle,pt2),btm_center);
        
        LineParseCable2(img,&backGroundTemplate,img,pt1,pt2,regionInfo[i].margin,&(regionInfo[i]));
        
        // acvDrawLine(img,
        // (int)pt1.X,(int)pt1.Y,
        // (int)pt2.X,(int)pt2.Y,0,255,0,6);


        {
          cJSON* singleInsp = cJSON_CreateObject();
          cJSON* matchingRGBA = cJSON_AddArrayToObject(singleInsp, "matchingRGBA");

          cJSON_AddItemToArray(matchingRGBA,cJSON_CreateNumber(regionInfo[i].inspRes.RGBA[0]));
          cJSON_AddItemToArray(matchingRGBA,cJSON_CreateNumber(regionInfo[i].inspRes.RGBA[1]));
          cJSON_AddItemToArray(matchingRGBA,cJSON_CreateNumber(regionInfo[i].inspRes.RGBA[2]));
          cJSON_AddItemToArray(matchingRGBA,0);

          cJSON_AddNumberToObject(singleInsp, "diff",regionInfo[i].inspRes.diff);
          cJSON_AddNumberToObject(singleInsp, "id",regionInfo[i].id);


          cJSON_AddNumberToObject(singleInsp, "count",regionInfo[i].inspRes.count);
          cJSON_AddNumberToObject(singleInsp, "max_window2wire_width_ratio",regionInfo[i].inspRes.max_window2wire_width_ratio);

          cJSON_AddItemToArray(inspArray, singleInsp);
        }

      }
      
      
      {
        cJSON* _vec_btm = cJSON_CreateObject();
        cJSON_AddNumberToObject(_vec_btm, "x", vec_btm.X);
        cJSON_AddNumberToObject(_vec_btm, "y", vec_btm.Y);
        cJSON_AddItemToObjectCS(singleRep, "vec_btm", _vec_btm);
      }
      {
        cJSON* _vec_side = cJSON_CreateObject();
        cJSON_AddNumberToObject(_vec_side, "x", vec_side.X);
        cJSON_AddNumberToObject(_vec_side, "y", vec_side.Y);
        cJSON_AddItemToObjectCS(singleRep, "vec_side", _vec_side);
      }
      
      {
        cJSON* pt_cornor = cJSON_CreateObject();
        cJSON_AddNumberToObject(pt_cornor, "x", pt_anchor.X);
        cJSON_AddNumberToObject(pt_cornor, "y", pt_anchor.Y);
        cJSON_AddItemToObjectCS(singleRep, "pt_cornor", pt_cornor);
      }
    

    }


    // acvDrawLine(img,
    // (int)pt_anchor.X,(int)pt_anchor.Y,
    // (int)vec_btm.X+pt_anchor.X,(int)vec_btm.Y+pt_anchor.Y,0,0,255,6);
    // acvDrawLine(img,
    // (int)pt_anchor.X,(int)pt_anchor.Y,
    // (int)vec_side.X+pt_anchor.X,(int)vec_side.Y+pt_anchor.Y,0,255,0,6);
    // acvDrawLine(&buf2,
    // (int)pt_anchor.X,(int)pt_anchor.Y,
    // (int)vec_btm.X+pt_anchor.X,(int)vec_btm.Y+pt_anchor.Y,0,0,255,6);
    // acvDrawLine(&buf2,
    // (int)pt_anchor.X,(int)pt_anchor.Y,
    // (int)vec_side.X+pt_anchor.X,(int)vec_side.Y+pt_anchor.Y,0,255,0,6);


    // printf("m2:%f  m2_side1:%f  m2_side2:%f\n",m2,m2_side1,m2_side2);
    // printf("p1:%f %f  p2:%f %f   m:%f,%f rm:%f\n",
    //   vec_p1.X,vec_p1.Y,
    //   vec_p2.X,vec_p2.Y,
    //   statMoment[0],statMoment[1],statMoment[1]/statMoment[0]);

      //:5.313280
    // for(int k=1;k<contour.size();k++)
    // {

    //   // LOGI("j:%d, %f %f",j,contour[j].X,contour[j].Y);
    //   acvDrawLine(&buf2,
    //   (int)contour[k-1].X,(int)contour[k-1].Y,
    //   (int)contour[k].X,(int)contour[k].Y,0,255,255,3);
    // }
  }
  // acvCloneImage(&buf2,img,-1);
  // acvSaveBitmapFile("srcImg.bmp", img);
  
  // acvSaveBitmapFile("step3.bmp", &ImgOutput);
  // printf("\n");

  // 
  // acv_XY points_T[]={{10,1},{1,1},     {0,0},{100,0},{100,100},{0,100},   {1,99},{10,99}};
  // rotatingCalipers( points_T, sizeof(points_T)/sizeof(*points_T),CALIPERS_MINAREARECT, out );
  // ComputeConvexHull(points_T, sizeof(points_T)/sizeof(*points_T));
  // LOGI("out:%f %f %f %f %f",out[0],out[1],out[2],out[3],out[4]);
  return 0;
}
