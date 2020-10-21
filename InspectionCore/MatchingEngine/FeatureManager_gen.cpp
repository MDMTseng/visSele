#include "FeatureManager_gen.h"
#include "logctrl.h"
#include <stdexcept>
#include <common_lib.h>
#include <MatchingCore.h>
#include <acvImage_SpDomainTool.hpp>
#include <rotCaliper.h>

#include <algorithm>


/*
  FeatureManager_platingCheck Section
*/
FeatureManager_gen::FeatureManager_gen(const char *json_str): FeatureManager(json_str)
{

  report.data.cjson_report.cjson=NULL;
}


int FeatureManager_gen::parse_jobj()
{

  return 0;
}


int FeatureManager_gen::reload(const char *json_str)
{
 
  report.data.cjson_report.cjson=NULL;
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

        uint8_t *pix = &(img->CVector[Y][3*X]);
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

          uint8_t *pix = &(img->CVector[Y][3*X]);
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


int FeatureManager_gen::FeatureMatching(acvImage *img)
{

  int HFrom=0;
  int HTo=256;
  int SMax=90;
  int SMin=0;
  int VMax=255;
  int VMin=110;
  float minHeadArea=90*350*0.9;
  float targetHeadWHRatio=4.0;
  float targetHeadWHRatioMargin=1.2;
  float FacingThreshold=1;
  
  float cableSeachingRatio=0.2;


  const int cableCount=12;
  colorInfo tar_ci_bk[]={
{.R=25.000000,.G=27.000000, .B=47.000000}, 
{.R=159.000000,.G=45.000000, .B=69.000000}, 
{.R=154.000000,.G=99.000000, .B=46.000000}, 
{.R=200.000000,.G=135.000000, .B=71.000000}, 
{.R=176.000000,.G=61.000000, .B=89.000000}, 
{.R=25.000000,.G=67.000000, .B=95.000000}, 
{.R=28.000000,.G=42.000000, .B=115.000000}, 
{.R=48.000000,.G=43.000000, .B=91.000000}, 
{.R=96.000000,.G=113.000000, .B=181.000000}, 
{.R=78.000000,.G=43.000000, .B=65.000000}, 
{.R=19.000000,.G=20.000000, .B=34.000000}, 
{.R=124.000000,.G=39.000000, .B=62.000000}, 
  };


  colorInfo tar_ci_fr[]={
{.R=23.000000,.G=25.000000, .B=41.000000}, 
{.R=135.000000,.G=39.000000, .B=62.000000}, 
{.R=196.000000,.G=136.000000, .B=69.000000}, 
{.R=162.000000,.G=114.000000, .B=65.000000}, 
{.R=169.000000,.G=57.000000, .B=82.000000}, 
{.R=24.000000,.G=65.000000, .B=93.000000}, 
{.R=26.000000,.G=41.000000, .B=114.000000}, 
{.R=47.000000,.G=43.000000, .B=94.000000}, 
{.R=103.000000,.G=122.000000, .B=195.000000}, 
{.R=82.000000,.G=46.000000, .B=69.000000}, 
{.R=26.000000,.G=29.000000, .B=48.000000}, 
{.R=140.000000,.G=43.000000, .B=69.000000}, 


  };  
  float cableRatio=0.073;
  float maxDiffMargin=24;





  report.type=FeatureReport::cjson;
  if(report.data.cjson_report.cjson!=NULL)
  {
    
    cJSON_Delete(report.data.cjson_report.cjson);

    report.data.cjson_report.cjson=NULL;
  }
  cJSON *jsonRep=cJSON_CreateObject();
  
  cJSON_AddStringToObject(jsonRep, "type", GetFeatureTypeName());
  report.data.cjson_report.cjson=jsonRep;
  LOGI("GOGOGOGOGOGGO....");





  buf1.ReSize(img);
  buf2.ReSize(img);
  ImgOutput.ReSize(img);
  acvCloneImage(img,&buf1,-1);
  buf1.RGBToHSV();
  acvCloneImage(&buf1,&buf2,-1);


  // acvSaveBitmapFile("step0H.bmp", &buf2);
  // acvHSVThreshold(&buf2,0,256,256,128,255,50);
  acvHSVThreshold(&buf2,HFrom,HTo,SMax,SMin,VMax,VMin); //0V ~255  1S ~255  2H ~252
  acvBoxFilter(&ImgOutput,&buf2,3);
  acvThreshold(&buf2,30);
  // acvSaveBitmapFile("step0.bmp", &buf2);
  acvTurn(&buf2);

  acvDrawBlock(&buf2, 0, 0, buf2.GetWidth() - 1, buf2.GetHeight() - 1,255,255,255,20);
  // acvDrawBlock(&buf2, 1, 1, buf2.GetWidth() - 2, buf2.GetHeight() - 2);
  // acvHSVThreshold(&buf2,0,256,255,20,200,0); //0V ~255  1S ~255  2H ~252
  acvComponentLabeling(&buf2);
  
  vector<acv_LabeledData> ldData;
  acvLabeledRegionInfo(&buf2, &ldData);
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


    float len_long=hypot(out[2],out[3]);
    float len_short=hypot(out[4],out[5]);

    LOGI("GOGOGOGOGOGGO..[%d]..%f %f",i,len_long,len_short);
    
    if(len_long*len_short<minHeadArea)continue;

    acv_XY pt_anchor={out[0],out[1]};
    acv_XY vec_long={out[2],out[3]};
    acv_XY vec_short={out[4],out[5]};
    if(len_short>len_long)
    {
      acv_XY tmp=vec_long;
      vec_long=vec_short;
      vec_short=tmp;
      float tmpLen=len_long;
      len_long=len_short;
      len_short=tmpLen;

    }

    float ratio = len_long/len_short;
    ratio/=targetHeadWHRatio;//close to 4.0
    if(ratio<1)ratio=1/ratio;
    // printf("ratio:%f   len1:%f len_short:%f\n",ratio,len_long,len_short);

    LOGI("GOGOGOGOGOGGO.ratio:%f targetHeadWHRatioMargin:%f...",ratio,targetHeadWHRatioMargin);
    if(ratio>targetHeadWHRatioMargin)
    {
      continue;
    }
    
    cJSON *singleRep=cJSON_CreateObject();
    
    cJSON_AddItemToArray(reports_jarr,singleRep);
    float m2=NAN;
    {
      acv_XY vec_shortHalf=acvVecMult(vec_short,0.5);
      acv_XY vec_p1=acvVecAdd(vec_shortHalf,pt_anchor);
      acv_XY vec_p2=acvVecAdd(vec_p1,vec_long);

      vec_p1=acvVecAdd(vec_p1,acvVecMult(vec_long,0.2));
      vec_p2=acvVecSub(vec_p2,acvVecMult(vec_long,0.2));
      float statMoment[2];


      LineState(img,vec_p1,vec_p2,len_long*2,0,statMoment);
      m2=statMoment[1]/statMoment[0];
    }
    bool isFrontFace=m2>FacingThreshold;

    if(isFrontFace)
      printf("FRONT\n");
    else
      printf("BACK\n");

    float m2_side1=NAN;
    {

      acv_XY vec_shortHalf=acvVecMult(vec_short,-cableSeachingRatio);
      acv_XY vec_p1=acvVecAdd(vec_shortHalf,pt_anchor);
      acv_XY vec_p2=acvVecAdd(vec_p1,vec_long);
      float statMoment[2];
      LineState(img,vec_p1,vec_p2,len_long,0,statMoment);
      m2_side1=statMoment[1]/statMoment[0];
    }

    float m2_side2=NAN;
    {
      acv_XY vec_shortHalf=acvVecMult(vec_short,1+cableSeachingRatio);
      acv_XY vec_p1=acvVecAdd(vec_shortHalf,pt_anchor);
      acv_XY vec_p2=acvVecAdd(vec_p1,vec_long);
      float statMoment[2];
      LineState(img,vec_p1,vec_p2,len_long,0,statMoment);
      m2_side2=statMoment[1]/statMoment[0];
    }

    if(m2_side1<m2_side2)
    { //          ______     _____
      //could be |        or      |
      
      pt_anchor=acvVecAdd(pt_anchor,vec_short);
      vec_short=acvVecMult(vec_short,-1);
      float tmp=m2_side1;
      m2_side1=m2_side2;
      m2_side2=tmp;//swap side info
    }


    float axis_crossP=-1*acv2DCrossProduct(vec_long,vec_short);


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
        pt_anchor=acvVecAdd(pt_anchor,vec_long);
        vec_long=acvVecMult(vec_long,-1);
      }
      else
      {
        //perfect
      }
    }


    // cJSON_AddItemToObject(jsonRep,"rect_origin",reports_jarr);
    // cJSON_AddItemToObject(jsonRep,"rect_vlong",reports_jarr);
    // cJSON_AddItemToObject(jsonRep,"rect_vshort",reports_jarr);

    cJSON_AddBoolToObject(singleRep, "front_facing", isFrontFace);

    
    {


      colorInfo *tar_ci=isFrontFace?tar_ci_fr:tar_ci_bk;



      acv_XY vec_shortHalf=acvVecMult(vec_short,-cableSeachingRatio);
      acv_XY vec_p1=acvVecAdd(vec_shortHalf,pt_anchor);
      acv_XY vec_p2=acvVecAdd(vec_p1,vec_long);

      colorInfo ci[100];
      LineParseCable(img,&buf1,vec_p1,vec_p2,cableRatio,(1-cableRatio*cableCount)/2,cableCount,5,ci);
      float minScore=99;
      float minBriRatio=1;
      float maxDiffSq=0;
      for(int i=0;i<cableCount;i++)
      {
        float diffR=tar_ci[i].R-ci[i].R;
        float diffG=tar_ci[i].G-ci[i].G;
        float diffB=tar_ci[i].B-ci[i].B;

        float mean = (diffR+diffG+diffB)/3;
        diffR-=mean;
        diffG-=mean;
        diffB-=mean;

        float tarMag = tar_ci[i].R*tar_ci[i].R+tar_ci[i].G*tar_ci[i].G+tar_ci[i].B*tar_ci[i].B;
        float curMag = ci[i].R*ci[i].R+ci[i].G*ci[i].G+ci[i].B*ci[i].B;

        float briRatio=curMag/tarMag;
        if(briRatio>1)briRatio=1/briRatio;
        float dotP   =(tar_ci[i].R*ci[i].R+tar_ci[i].G*ci[i].G+tar_ci[i].B*ci[i].B)/sqrt(tarMag*curMag);
        float diffSq=diffR*diffR+diffG*diffG+diffB*diffB;
        if(maxDiffSq<diffSq)
        {
          maxDiffSq=diffSq;
        }
        if(minScore>dotP)
        {
         minScore=dotP;
        }
        if(minBriRatio>briRatio )
        {
         minBriRatio=briRatio;
        }
        
        printf("{.R=%f,.G=%f, .B=%f}, maxDiffSq:%f\n",ci[i].R,ci[i].G,ci[i].B, sqrt(diffSq));
      }
      minBriRatio=sqrt(minBriRatio);
      maxDiffSq=sqrt(maxDiffSq);
      printf("\n");
      LOGI("minScore=%f, minBriRatio=%f maxDiffSq=%f",minScore, minBriRatio,maxDiffSq);
      acv_XY rect_center={pt_anchor.X+vec_short.X/2+vec_long.X/2,pt_anchor.Y+vec_short.Y/2+vec_long.Y/2};
      //if(minScore<0.90 || minBriRatio<0.5)
      if(maxDiffSq>maxDiffMargin)
      {
        acvDrawCrossX(img,rect_center.X,rect_center.Y, 50, 0, 0,255, 10);
      }
      else
      {
        acvDrawCircle(img,rect_center.X,rect_center.Y, 50,10, 0, 255,0, 10);
      }
      cJSON_AddNumberToObject(singleRep, "score", minScore);

    }


    acvDrawLine(img,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_long.X+pt_anchor.X,(int)vec_long.Y+pt_anchor.Y,0,0,255,6);
    acvDrawLine(img,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_short.X+pt_anchor.X,(int)vec_short.Y+pt_anchor.Y,0,255,0,6);
    acvDrawLine(&buf2,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_long.X+pt_anchor.X,(int)vec_long.Y+pt_anchor.Y,0,0,255,6);
    acvDrawLine(&buf2,
    (int)pt_anchor.X,(int)pt_anchor.Y,
    (int)vec_short.X+pt_anchor.X,(int)vec_short.Y+pt_anchor.Y,0,255,0,6);


    printf("m2:%f  m2_side1:%f  m2_side2:%f\n",m2,m2_side1,m2_side2);
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
