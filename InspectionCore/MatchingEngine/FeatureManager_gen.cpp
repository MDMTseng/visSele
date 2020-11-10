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
FeatureManager_gen::FeatureManager_gen(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
  reload(json_str);
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

#define InDataR  InData[2]
#define InDataG  InData[1]
#define InDataB  InData[0]

void HSVFromRGB(BYTE* OutData,BYTE* InData)
{
    //0 V ~255
    //1 S ~255
    //2 H ~251
    BYTE Mod,Max,Min,D1,D2;
    Max=Min=InDataR;
    D1=InDataG;
    D2=InDataB;
    Mod=6;
    if(InDataG>Max)
    {
        Max=InDataG;
        Mod=2;       //
        D1=InDataB;
        D2=InDataR;
    }
    else
    {
        Min=InDataG;

    }

    if(InDataB>Max)
    {
        Max=InDataB;
        Mod=4;
        D1=InDataR;
        D2=InDataG;
    }
    else if(InDataB<Min)
    {
        Min=InDataB;

    }

    OutData[0]=Max;
    if(Max==0)
    {
        OutData[1]=
            OutData[2]=0;
        goto Exit;
    }
    else
        OutData[1]=255-Min*255/Max;
    Max-=Min;
    if(Max)
    {
        OutData[2]=(Mod*(Max)+D1-D2)*42/(Max);
        if(OutData[2]<42||OutData[2]>=252)OutData[2]+=4;
    }
    else
        OutData[2]=0;
Exit:
    ;
}


void RGBToHSV(acvImage &im)
{
    for(int i=0; i<im.GetHeight(); i++)
    {
        uint8_t *ImLine=im.CVector[i];
        for(int j=0; j<im.GetWidth(); j++)
        {
            HSVFromRGB(ImLine,ImLine);
            ImLine+=3;
        }
    }
}



float CrossProduct(acv_XY p1,acv_XY p2,acv_XY p3)
{
  acv_XY v1=acvVecNormalize({.X=p2.X-p1.X,.Y=p2.Y-p1.Y});
  acv_XY v2=acvVecNormalize({.X=p3.X-p2.X,.Y=p3.Y-p2.Y});

  return acv2DCrossProduct(v1,v2);
}


void ComputeConvexHull2(const acv_XY *polygon,const int L)
{
	// The polygon needs to have at least three points
	if (L < 3)
	{
		return ;
	}

	std::vector<int> upperIdx;
	std::vector<int> lowerIdx;
	upperIdx.push_back(0);
	upperIdx.push_back(1);

	/*
	We piecewise construct the convex hull and combine them at the end of the method. Note that this could be
	optimized by combing the while loops.
	*/
	for (size_t i = 2; i < L; i++)
	{
		while (upperIdx.size() > 1 && 
    !CrossProduct(polygon[upperIdx[upperIdx.size() - 2]], polygon[upperIdx[upperIdx.size() - 1]], polygon[i])>0)
		{
			upperIdx.pop_back();
		}
		upperIdx.push_back(i);

		while (lowerIdx.size() > 1 && 
    !CrossProduct(polygon[lowerIdx[lowerIdx.size() - 2]], polygon[lowerIdx[lowerIdx.size() - 1]], polygon[L - i - 1])>0)
		{
			lowerIdx.pop_back();
		}
		lowerIdx.push_back(L - i - 1);
	}
	// upperIdx.insert(upperIdx.end(), lowerIdx.begin(), lowerIdx.end());
  for(int idx:upperIdx)
  {
    LOGI("idx:%d",idx);
  }
	return;
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
    float val = acvUnsignedMap1Sampling(img, cur_pt,ch);
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

void LineParseCable_(acvImage *img,acv_XY pt1,acv_XY pt2,float cableRatio=0.0748,float startRatio=0.0512)
{
  float steps=acvDistance(pt1,pt2);
  acv_XY cur_pt=pt1;

  LOGI("%f,%f--->%f,%f",pt1.X,pt1.Y,pt2.X,pt2.Y);
  acv_XY advVec={(pt2.X-pt1.X)/(steps-1),(pt2.Y-pt1.Y)/(steps-1)};
  bool inGroup=false;
  for(int i=0;i<steps;i++,cur_pt=acvVecAdd(cur_pt,advVec))
  {
    int X = round(cur_pt.X);
    int Y = round(cur_pt.Y);
    
    uint8_t *pix = &(img->CVector[Y][3*X]);
    uint8_t pix_HSV[3];
    acvImage::HSVFromRGB(pix_HSV,pix);
    int objScore=pix_HSV[0];
    if(objScore>50)if(pix_HSV[1]>objScore)objScore=pix_HSV[1];

    inGroup=objScore>70;
    if(!inGroup)
    {
      printf("\n");
      continue;
    }

    printf("pt:%03d,%03d  ...",X,Y);
    printf("%03d,%03d,%03d>",pix[0],pix[1],pix[2]);
    printf("%03d,%03d,%03d  %c\n",
    pix_HSV[0],pix_HSV[1],pix_HSV[2],objScore>70?'0':' ');
  }
  printf("\n");
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

uint8_t* pixFetching(acvImage *img,int x,int y,int shrink=0)
{
  if(x<shrink || y<shrink)return NULL;
  if(x>=img->GetWidth()-shrink || y>=img->GetHeight()-shrink)return NULL;


  return &(img->CVector[y][3*x]);
}

void LineParseCable(acvImage *img,acvImage *buffer,acv_XY pt1,acv_XY pt2,float cableRatio,float startRatio,int cableCount,float stepWidth,colorInfo *info)
{
  float steps=acvDistance(pt1,pt2);

  acv_XY advVec={(pt2.X-pt1.X)/(steps),(pt2.Y-pt1.Y)/(steps)};
  acv_XY nor_advVec=acvVecNormal(advVec);
  nor_advVec=acvVecNormalize(nor_advVec);
  pt1=acvVecAdd(pt1 ,acvVecMult(nor_advVec,-stepWidth/2));
  pt2=acvVecAdd(pt2 ,acvVecMult(nor_advVec,-stepWidth/2));



  float spacingRatio=0.015*12/cableCount;
  float cableW=steps*(cableRatio-2*spacingRatio);

  int k=0;
  for(float ratio=startRatio;ratio<1-startRatio;k++,ratio+=cableRatio)
  {
    acv_XY cur_pt=acvVecInterp(pt1,pt2,ratio+spacingRatio);
    float center=0;
    float V=0;
    float S=0;
    float H=0;
    int sR=0,sG=0,sB=0;

    for(int k=0;k<stepWidth;k++)
    {
      acv_XY region_pt =acvVecAdd(cur_pt ,acvVecMult(nor_advVec,k));

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



        uint8_t pix_HSV[3];
        acvImage::HSVFromRGB(pix_HSV,pix);
        // printf("pt:%03d,%03d  ...",X,Y);
        // printf("%03d,%03d,%03d>",pix[0],pix[1],pix[2]);
        // printf("%03d,%03d,%03d \n",
        // pix_HSV[0],pix_HSV[1],pix_HSV[2]);
        int vv=pix_HSV[0]*pix_HSV[0];
        V+=vv;
        center+=i*vv;

        sB+=vv*pix[0];
        sG+=vv*pix[1];
        sR+=vv*pix[2];
        region_pt=acvVecAdd(region_pt,advVec);
      }
    }
    // majorColorTracing(buffer,cableW,stepWidth);

    center=center/V;
    sB/=V;
    sG/=V;
    sR/=V;
    if(sB>255)sB=255;
    if(sG>255)sG=255;
    if(sR>255)sR=255;
    uint8_t pix_ave[3]={(uint8_t)(sB),(uint8_t)(sG),(uint8_t)(sR)};



    {
      
      for(int k=0;k<stepWidth;k++)
      {
        acv_XY region_pt =acvVecAdd(cur_pt ,acvVecMult(nor_advVec,k));
        for(int i=0;i<cableW;i++)
        {
          int X = round(region_pt.X);
          int Y = round(region_pt.Y);

          uint8_t *pix = pixFetching(img,X,Y);
          if(pix==NULL)continue;
          pix[0]=pix_ave[0];
          pix[1]=pix_ave[1];
          pix[2]=pix_ave[2];
          region_pt=acvVecAdd(region_pt,advVec);
        }
      }


      if(info!=NULL && k<=cableCount )
      {
        info[k].B=pix_ave[2];
        info[k].G=pix_ave[1];
        info[k].R=pix_ave[0];
      }
    }
    //acvImage::HSVFromRGB(pix_ave,pix_ave);
    // printf("center:%f V:%d  S:%d H:%d\n\n",center,pix_ave[0],pix_ave[1],pix_ave[2]);
  }



  // bool inGroup=false;
  // for(int i=0;i<steps;i++,cur_pt=acvVecAdd(cur_pt,advVec))
  // {
  //   int X = round(cur_pt.X);
  //   int Y = round(cur_pt.Y);
    
  //   uint8_t *pix = &(img->CVector[Y][3*X]);
  //   uint8_t pix_HSV[3];
  //   acvImage::HSVFromRGB(pix_HSV,pix);
  //   int objScore=pix_HSV[0];
  //   if(objScore>50)if(pix_HSV[1]>objScore)objScore=pix_HSV[1];

  //   inGroup=objScore>70;
  //   if(!inGroup)
  //   {
  //     printf("\n");
  //     continue;
  //   }

  //   printf("pt:%03d,%03d  ...",X,Y);
  //   printf("%03d,%03d,%03d>",pix[0],pix[1],pix[2]);
  //   printf("%03d,%03d,%03d  %c\n",
  //   pix_HSV[0],pix_HSV[1],pix_HSV[2],objScore>70?'0':' ');
  // }
  // printf("\n");
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

#define SETPARAM_INT_NUMBER(json,pName) {double *tmpN; if((tmpN=JFetch_NUMBER(json,#pName)))pName=(int)*tmpN;}
#define SETPARAM_DOU_NUMBER(json,pName) {double *tmpN; if((tmpN=JFetch_NUMBER(json,#pName)))pName=*tmpN;}


#define RETPARAM_NUMBER(json,pName) {cJSON_AddNumberToObject(json, #pName, pName);}
cJSON * FeatureManager_gen::SetParam(cJSON *jsonParam)
{
  char* jsonStr = cJSON_Print(jsonParam);
  LOGI("%s..",jsonStr);
  delete jsonStr;
  // exit(-1);
  // double *tmpN;
  // if((tmpN=JFetch_NUMBER(jsonParam,"HFrom")))
  //   HFrom=(int)*tmpN;


  
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

  // const int cableTableCount=2;
  return ret_jobj;
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
  
  colorInfo tar_ci_bk[]={
    
      {.R=17.000000,.G=18.000000, .B=27.000000}, 
      {.R=84.000000,.G=28.000000, .B=39.000000}, 
      {.R=32.000000,.G=33.000000, .B=53.000000}, 
      {.R=114.000000,.G=82.000000, .B=34.000000}, 
      {.R=100.000000,.G=40.000000, .B=51.000000}, 
      {.R=19.000000,.G=39.000000, .B=55.000000}, 
      {.R=18.000000,.G=27.000000, .B=67.000000}, 
      {.R=30.000000,.G=28.000000, .B=54.000000}, 
      {.R=64.000000,.G=77.000000, .B=113.000000}, 
      {.R=52.000000,.G=31.000000, .B=38.000000}, 
      {.R=16.000000,.G=17.000000, .B=25.000000}, 
      {.R=82.000000,.G=29.000000, .B=38.000000}, 
    
      {.R=17.000000,.G=18.000000, .B=27.000000}, 
      {.R=84.000000,.G=28.000000, .B=39.000000}, 
      {.R=32.000000,.G=33.000000, .B=53.000000}, 
      {.R=114.000000,.G=82.000000, .B=34.000000}, 
      {.R=100.000000,.G=40.000000, .B=51.000000}, 
      {.R=19.000000,.G=39.000000, .B=55.000000}, 
      {.R=18.000000,.G=27.000000, .B=67.000000}, 
      {.R=30.000000,.G=28.000000, .B=54.000000}, 
      {.R=64.000000,.G=77.000000, .B=113.000000}, 
      {.R=52.000000,.G=31.000000, .B=38.000000}, 
      {.R=16.000000,.G=17.000000, .B=25.000000}, 
      {.R=82.000000,.G=29.000000, .B=38.000000}, 
    
  };


  colorInfo tar_ci_fr[]={
    
      {.R=17.000000,.G=17.000000, .B=28.000000}, 
      {.R=72.000000,.G=21.000000, .B=29.000000}, 
      {.R=37.000000,.G=40.000000, .B=65.000000}, 
      {.R=88.000000,.G=68.000000, .B=29.000000}, 
      {.R=97.000000,.G=33.000000, .B=40.000000}, 
      {.R=9.000000,.G=31.000000, .B=42.000000}, 
      {.R=12.000000,.G=18.000000, .B=53.000000}, 
      {.R=21.000000,.G=18.000000, .B=40.000000}, 
      {.R=58.000000,.G=71.000000, .B=103.000000}, 
      {.R=38.000000,.G=21.000000, .B=28.000000}, 
      {.R=47.000000,.G=15.000000, .B=22.000000}, 
      {.R=50.000000,.G=26.000000, .B=42.000000},

      {.R=17.000000,.G=18.000000, .B=26.000000}, 
      {.R=82.000000,.G=25.000000, .B=31.000000}, 
      {.R=30.000000,.G=33.000000, .B=50.000000}, 
      {.R=98.000000,.G=79.000000, .B=29.000000}, 
      {.R=106.000000,.G=41.000000, .B=45.000000}, 
      {.R=14.000000,.G=39.000000, .B=52.000000}, 
      {.R=15.000000,.G=24.000000, .B=61.000000}, 
      {.R=28.000000,.G=27.000000, .B=53.000000}, 
      {.R=62.000000,.G=77.000000, .B=107.000000}, 
      {.R=47.000000,.G=28.000000, .B=34.000000}, 
      {.R=18.000000,.G=18.000000, .B=24.000000}, 
      {.R=81.000000,.G=31.000000, .B=38.000000},

  };  
  float cableRatio=0.074;
  float maxDiffMargin=24;


  // inspectionStage=1;

  ClearReport();
  cJSON *jsonRep=cJSON_CreateObject();
  
  cJSON_AddStringToObject(jsonRep, "type", GetFeatureTypeName());
  report.data.cjson_report.cjson=jsonRep;
  // LOGI("GOGOGOGOGOGGO....inspectionStage:%d",inspectionStage);

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


  acvBoxFilter(&ImgOutput,&buf2,boxFilter1_Size);
  acvThreshold(&buf2,boxFilter1_thres);
  acvBoxFilter(&ImgOutput,&buf2,boxFilter2_Size);
  acvThreshold(&buf2,boxFilter2_thres);

  if(inspectionStage==2)
  {
    acvCloneImage(&buf2,img,-1);
    return -1;
  }
  acvBoxFilter(&ImgOutput,&buf2,boxFilter1_Size);
  acvThreshold(&buf2,boxFilter1_thres);
  // acvSaveBitmapFile("step0.bmp", &buf2);
  acvTurn(&buf2);

  acvDrawBlock(&buf2, 0, 0, buf2.GetWidth() - 1, buf2.GetHeight() - 1,255,255,255,20);
  // acvDrawBlock(&buf2, 1, 1, buf2.GetWidth() - 2, buf2.GetHeight() - 2);
  // acvHSVThreshold(&buf2,0,256,255,20,200,0); //0V ~255  1S ~255  2H ~252
  acvComponentLabeling(&buf2);
  
  vector<acv_LabeledData> ldData;
  acvLabeledRegionInfo(&buf2, &ldData);



  if(inspectionStage==3)
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
  for(int i=1;i<ldData.size();i++)
  {
    if(ldData[i].area<100)continue;
    bool isOK =  acvOuterContourExtraction(&buf2, ldData[i], i,contour);

    ComputeConvexHull(&(contour[0]), contour.size(),convexHullIdx);
    
    for(int k=0;k<convexHullIdx.size();k++)
    {
      contour[k]=contour[convexHullIdx[k]];
    }
    
    contour.resize(convexHullIdx.size());
    float out[6]={0}; 
    rotatingCalipers( &(contour[0]), contour.size(),CALIPERS_MINAREARECT, out );

    // LOGI("out:%f %f %f %f %f %f",out[0],out[1],out[2],out[3],out[4],out[5]);


    float len_btm=hypot(out[2],out[3]);
    float len_side=hypot(out[4],out[5]);

    LOGI("GOGOGOGOGOGGO..[%d]..%f %f",i,len_btm,len_side);
    
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
      if(m1_sideMax<100 && m2_sideMax<100)
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
      if(m2_side1<m2_side2)
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

    cJSON *singleRep=cJSON_CreateObject();
    
    cJSON_AddItemToArray(reports_jarr,singleRep);




    float axis_crossP=-1*acv2DCrossProduct(vec_btm,vec_side);


    if(!isFrontFace)
    {
      axis_crossP*=-1;
    }
      /* axis direction in image corrdinate (ie y  downward )
      target: Face side
      |
      |_________________
         | | | | | | |
      

      target: back side
                       |
      _________________|
         | | | | | | |

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


    cJSON_AddBoolToObject(singleRep, "front_facing", isFrontFace);

    {


      colorInfo *tar_ci=isFrontFace?tar_ci_fr:tar_ci_bk;



      acv_XY vec_sideHalf=acvVecMult(vec_side,-cableSeachingRatio);
      acv_XY vec_p1=acvVecAdd(vec_sideHalf,pt_anchor);
      acv_XY vec_p2=acvVecAdd(vec_p1,vec_btm);

      colorInfo ci[100];
      LineParseCable(img,&buf1,vec_p1,vec_p2,cableRatio,(1-cableRatio*cableCount)/2,cableCount,5,ci);

      printf("====================\n");
      for(int j=0;j<cableCount;j++)
      {
        printf("{.R=%f,.G=%f, .B=%f},\n",ci[j].R,ci[j].G,ci[j].B);
      }
      printf("====================\n");

      float min_maxDiff=99999;
      int min_maxDiffIdx=-1;

      for(int j=0;j<cableTableCount;j++)
      {
        float _maxDiff;
        int _maxDiffIdx=-1;
        colorCompare(tar_ci+j*cableCount, ci, cableCount,&_maxDiff,&_maxDiffIdx);

        if(min_maxDiff>_maxDiff)
        {
          min_maxDiff=_maxDiff;
          min_maxDiffIdx=_maxDiffIdx;
        }

      }
      LOGI("min_maxDiff:%f min_maxDiffIdx:%d",min_maxDiff,min_maxDiffIdx);

      acv_XY rect_center={pt_anchor.X+vec_side.X/2+vec_btm.X/2,pt_anchor.Y+vec_side.Y/2+vec_btm.Y/2};
      //if(minScore<0.90 || minBriRatio<0.5)
      if(min_maxDiff>maxDiffMargin)
      {
        acvDrawCrossX(img,rect_center.X,rect_center.Y, 50, 0, 0,255, 10);
      }
      else
      {
        acvDrawCircle(img,rect_center.X,rect_center.Y, 50,10, 0, 255,0, 10);
      }
      cJSON_AddNumberToObject(singleRep, "maxDiff", min_maxDiff);

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
    

    acvDrawLine(img,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_btm.X+pt_anchor.X,(int)vec_btm.Y+pt_anchor.Y,0,0,255,6);
    acvDrawLine(img,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_side.X+pt_anchor.X,(int)vec_side.Y+pt_anchor.Y,0,255,0,6);
    acvDrawLine(&buf2,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_btm.X+pt_anchor.X,(int)vec_btm.Y+pt_anchor.Y,0,0,255,6);
    acvDrawLine(&buf2,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_side.X+pt_anchor.X,(int)vec_side.Y+pt_anchor.Y,0,255,0,6);


    // printf("m2:%f  m2_side1:%f  m2_side2:%f\n",m2,m2_side1,m2_side2);
    // printf("p1:%f %f  p2:%f %f   m:%f,%f rm:%f\n",
    //   vec_p1.X,vec_p1.Y,
    //   vec_p2.X,vec_p2.Y,
    //   statMoment[0],statMoment[1],statMoment[1]/statMoment[0]);

      //:5.313280
    for(int k=1;k<contour.size();k++)
    {

      // LOGI("j:%d, %f %f",j,contour[j].X,contour[j].Y);
      acvDrawLine(&buf2,
      (int)contour[k-1].X,(int)contour[k-1].Y,
      (int)contour[k].X,(int)contour[k].Y,0,255,255,3);
    }
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
