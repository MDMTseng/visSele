#include "logctrl.h"
#include <stdexcept>
#include <MatchingCore.h>
#include <common_lib.h>
#include <FM_camera_calibration.h>
#include "acvImage_SpDomainTool.hpp"


static acvRadialDistortionParam calcCameraCalibration(acvImage &img);



static int markPointExtraction(acvImage &labelImg, vector<acv_LabeledData> &ldData,vector<struct idx_dist>& ret_idxList);

static acv_XY LableCenterRefine(acvImage &grayLevelImage,acv_LabeledData ldat,int range);

static double dNAN_=std::numeric_limits<double>::quiet_NaN();
static float fNAN_=(float)dNAN_;

FM_camera_calibration::FM_camera_calibration(const char *json_str): FeatureManager_binary_processing(json_str)
{
  root= NULL;
  int ret = reload(json_str);
  if(ret)
    throw std::invalid_argument( "Error:FM_camera_calibration failed... " );
}


int FM_camera_calibration::parse_jobj()
{
  cJSON *subObj = cJSON_GetObjectItem(root,"type");
  const char *type_str = subObj?subObj->valuestring:NULL;
  subObj = cJSON_GetObjectItem(root,"ver");
  const char *ver_str = subObj?subObj->valuestring:NULL;
  const double *pattern_dist = JFetch_NUMBER(root,"pattern_dist");
  if(type_str==NULL||ver_str==NULL||pattern_dist==NULL)
  {
    LOGE("ptr: type:<%p>  ver:<%p>  pattern_dist(number):<%p>",type_str,ver_str,pattern_dist);
    return -1;
  }
  LOGI("type:<%s>  ver:<%s>  pattern_dist(number):<%g>",type_str,ver_str,*pattern_dist);



  return 0;
}


int FM_camera_calibration::reload(const char *json_str)
{
  if(root)
  {
    cJSON_Delete(root);
  }

  root = cJSON_Parse(json_str);
  if(root==NULL)
  {
    LOGE("cJSON parse failed");
    return -1;
  }
  int ret_err = parse_jobj();
  if(ret_err!=0)
  {
    reload("");
    return -2;
  }
  return 0;
}

int FM_camera_calibration::FeatureMatching(acvImage *img)
{
  acvRadialDistortionParam param = calcCameraCalibration(*(this->originalImage));;
  this->param = param;

  LOGV("K: %g %g %g RNormalFactor:%g",param.K0,param.K1,param.K2,param.RNormalFactor);
  LOGV("Center: %g,%g",param.calibrationCenter.X,param.calibrationCenter.Y);
  return 0;
}

const FeatureReport* FM_camera_calibration::GetReport()
{
  report.type = FeatureReport::camera_calibration;
  report.data.camera_calibration.param=this->param;
  if(this->param.K0==this->param.K0)
  {
    report.data.camera_calibration.error=FeatureReport_camera_calibration::NONE;
  }
  else
  {
    report.data.camera_calibration.error=FeatureReport_camera_calibration::FAIL;
  }
  return &report;
}


struct idx_dist{
    int idx1;
    int idx2;
    float dist;
};

static void idixMinPush(struct idx_dist* idxArray, int arrL, idx_dist newIdx)
{
    for(int i=0;i<arrL;i++)
    {
        if(newIdx.dist<idxArray[i].dist)
        {
            struct idx_dist tmp=idxArray[i];
            tmp = idxArray[i];
            idxArray[i] = newIdx;
            newIdx = tmp;
        }
    }
}


static struct idx_dist getResetIdxDist()
{
    return {
        idx1:-1,
        idx2:-1,
        dist:100000
    };
}



static float findMedianSize(vector<acv_LabeledData> &ldData)
{
    

    vector<float> size;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area<=0)continue;
        size.push_back(ldat.area);
    }

    sort(size.begin(), size.end()); 
  
    return size[size.size()/2];


}


static void findMeanVec(vector<acv_LabeledData> &ldData, vector<struct idx_dist> &idxList,float cosSim,vector<acv_XY> &ret_vec)
{
    ret_vec.resize(0);
    for(int i=0;i<idxList.size();i++)
    {
        struct idx_dist idd = idxList[i];
        acv_XY pt1=ldData[idd.idx1].Center;
        acv_XY pt2=ldData[idd.idx2].Center;
        acv_XY vec = {X:pt2.X-pt1.X,Y:pt2.Y-pt1.Y};
        vec =  acvVecNormalize(vec);
        bool isInSet=false;
        for(int j=0;j<ret_vec.size()/2;j++)
        {
            acv_XY refVec = ret_vec[2*j];
            float csim = acv2DDotProduct(refVec,vec);
            csim/=ret_vec[2*j+1].X;
            if(csim<0)
            {
                csim=-csim;
                vec.X=-vec.X;
                vec.Y=-vec.Y;
            }
            
            //LOGV("%d:%d, csim:%f",idd.idx1,idd.idx2,csim);
            if(csim>cosSim)
            {
                isInSet=true;
                ret_vec[2*j].X+=vec.X;
                ret_vec[2*j].Y+=vec.Y;
                ret_vec[2*j+1].X++;
                break;
            }
        }

        if(!isInSet)
        {
            ret_vec.push_back(vec);
            vec={X:1,Y:0};
            ret_vec.push_back(vec);
        }
    }

    for(int j=0;j<ret_vec.size()/2;j++)
    {
        ret_vec[j].X = ret_vec[2*j].X/ret_vec[2*j+1].X;
        ret_vec[j].Y = ret_vec[2*j].Y/ret_vec[2*j+1].X;
        ret_vec[j]=acvVecNormalize(ret_vec[j]);
    }
    ret_vec.resize(ret_vec.size()/2);
}


static acv_XY PatternCoord2imgXY(
    acv_XY patternXY, 
    vector<vector<int>> &boardCoordIdx, vector<acv_LabeledData> &ldData)
    {
        acv_XY tl = patternXY;
        tl.X=floor(tl.X);
        tl.Y=floor(tl.Y);
        
        acv_XY ratio = patternXY;
        ratio.X-=tl.X;
        ratio.Y-=tl.Y;

        if(tl.Y<0 || tl.Y+1>=boardCoordIdx.size() || tl.X<0 || tl.X+1>=boardCoordIdx[0].size() )
        {
            return {X:fNAN_,Y:fNAN_};
        }
        int idxTL = boardCoordIdx[tl.Y  ][tl.X  ];
        int idxTR = boardCoordIdx[tl.Y  ][tl.X+1];
        int idxBL = boardCoordIdx[tl.Y+1][tl.X  ];
        int idxBR = boardCoordIdx[tl.Y+1][tl.X+1];

        if(idxTL<0 || idxTR<0 || idxBL<0 || idxBR<0)
        {
            return {X:fNAN_,Y:fNAN_};
        }

        acv_XY imgXY_Top = acvVecInterp(ldData[idxTL].Center,ldData[idxTR].Center,ratio.X);
        acv_XY imgXY_Btn = acvVecInterp(ldData[idxBL].Center,ldData[idxBR].Center,ratio.X);

        return acvVecInterp(imgXY_Top,imgXY_Btn,ratio.Y);
    }

static acv_XY imgXY2PatternCoord(
    acv_XY imgXY, 
    vector<vector<int>> &boardCoordIdx, vector<acv_LabeledData> &ldData, 
    vector<acv_XY> &mainVec, float meanDist)
{
    int idxS=-1;
    for(int i=0;i<boardCoordIdx.size();i++)
    {
        for(int j=0;i<boardCoordIdx[i].size();j++)
        {
            if(boardCoordIdx[i][j]!=-1)
            {
                idxS = boardCoordIdx[i][j];
                break;
            }
        }
        if(idxS!=-1)break;
    }

    /*
    [v11 v12][cX]=[iX= v11*cX + v12*cY]
    [v21 v22][cY] [iY= v21*cX + v22*cY]
    */
    acv_XY v0 = mainVec[0];
    acv_XY v1 = mainVec[1];
    /*
    [v0.X v1.X]
    [v0.Y v1.Y]
    */

    float det = ( v0.X*v1.Y - v0.Y*v1.X );
    /*
    [ v1.Y -v1.X]
    [-v0.Y  v0.X]
    */
    acv_XY iv0 = acvVecMult({X: v1.Y,Y:-v0.Y},1/det/meanDist);
    acv_XY iv1 = acvVecMult({X:-v1.X,Y: v0.X},1/det/meanDist);
    
    acv_XY pX;
    for(int i=0;i<3;i++)
    {
        acv_XY imgVec = acvVecSub(imgXY,ldData[idxS].Center); 
        //LOGV("%f %f  ",imgVec.X,imgVec.Y);

        pX=acvVecAdd(acvVecMult(iv0,imgVec.X),acvVecMult(iv1,imgVec.Y));
        pX=acvVecAdd(pX,ldData[idxS].LTBound);
        //LOGV("%f %f   %f %f   %f %f",iv0.X,iv0.Y, iv1.X,iv1.Y, pX.X,pX.Y);
        //LOGV("%f,%f",pX.X,pX.Y);
        int tmpY = round(pX.Y);
        int tmpX = round(pX.X);
        if(tmpY<0 || tmpY>=boardCoordIdx.size() || 
        tmpX<0 || tmpX>=boardCoordIdx[0].size() )
        {
            pX.X=-1;
            pX.Y=-1;
            break;
        }
        int new_idxS = boardCoordIdx[tmpY][tmpX];
        //LOGV("%d,%d",idxS,new_idxS);
        if(new_idxS==-1){
            pX.X=-1;
            pX.Y=-1;
            break;
        }
        
        if(new_idxS==idxS)break;
        idxS = new_idxS;
    }

    if(pX.X==-1)return pX;
    //boardCoordIdx[floor(pX.Y)][floor(pX.X)];

    //refine

    {
        acv_XY ref_pX = pX;
        float alpha=0.8/meanDist;
        for(int i=0;i<20;i++)
        {
            acv_XY imgXY_est=PatternCoord2imgXY(ref_pX, boardCoordIdx, ldData);  
            if(imgXY_est.X!=imgXY_est.X)break; 
            ref_pX.X+=(imgXY.X-imgXY_est.X)*alpha;
            ref_pX.Y+=(imgXY.Y-imgXY_est.Y)*alpha;
            pX=ref_pX;
        }
    }
    

    return pX;
}


static acvRadialDistortionParam calcCameraCalibration(acvImage &img)
{
    
    acvImage &calibImage=img;

    acvRadialDistortionParam retParam;
    
    {
        acvRadialDistortionParam errParam={
            calibrationCenter:{fNAN_,fNAN_},
            RNormalFactor:dNAN_,
            K0:dNAN_,
            K1:dNAN_,
            K2:dNAN_,
            //r = r_image/RNormalFactor
            //C1 = K1/K0
            //C2 = K2/K0
            //r"=r'/K0
            //Forward: r' = r*(K0+K1*r^2+K2*r^4)
            //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
            //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
            //r/r'=r*K0/r"
            ppb2b:dNAN_
        };
        retParam = errParam;
    }

    acvImage labelImg;
    acvImage tmp;
    acvImage tmp2;

    labelImg.ReSize(&calibImage);
    tmp.ReSize(&calibImage);
    tmp2.ReSize(&calibImage);
    
    acvCloneImage(&calibImage,&labelImg, -1);


    acvBoxFilter(&tmp,&labelImg,6);
    acvBoxFilter(&tmp,&labelImg,6);
    acvBoxFilter(&tmp,&labelImg,6);
    acvBoxFilter(&tmp,&labelImg,6);
    acvCloneImage(&labelImg,&tmp, -1);
    acvThreshold(&labelImg, 120);


    //Draw a labeling black cage for labling algo, which is needed for acvComponentLabeling
    acvDrawBlock(&labelImg, 1, 1, labelImg.GetWidth() - 2, labelImg.GetHeight() - 2);


    //The labeling starts from (1 1) => (W-2,H-2), ie. it will not touch the outmost pixel to simplify the boundary condition
    //You need to draw a black/white cage to work(not crash).
    //The advantage of black cage is you can know which area touches the boundary then we can exclude it
    
    vector<acv_LabeledData> ldData;
    acvComponentLabeling(&labelImg);
    acvLabeledRegionInfo(&labelImg, &ldData);
    
    //acvSaveBitmapFile("data/labelImg.bmp",&labelImg);
    vector<struct idx_dist> idxList;
    markPointExtraction(labelImg, ldData,idxList);   

    
    float distMean=0;
    for(int i=0;i<idxList.size();i++)
    {
        //LOGV("%d+%d>>%f",idxList[i].idx1,idxList[i].idx2,idxList[i].dist);
        distMean+=idxList[i].dist;
        
        /*acvDrawLine(&labelImg, 
        ldData[idxList[i].idx1].Center.X, ldData[idxList[i].idx1].Center.Y, 
        ldData[idxList[i].idx2].Center.X, ldData[idxList[i].idx2].Center.Y, 
        255, 0,0,1);*/

        
    }
    distMean/=idxList.size();

    LOGV("distMean:%f",distMean);

    //Following calculation will use LTBound as the grid coordinate(the Nature number grid coordinate, (-1,-1) if fail to locate the point)
    //and use RBBound as calibrated cordinate(the XY after radial calibration)
    for(int i=2;i<ldData.size();i++) //refine the center
    {
        
        acv_XY cr = LableCenterRefine(calibImage,ldData[i],distMean);
        ldData[i].Center = cr;
        int X = round(cr.X);
        int Y = round(cr.Y);
        //acvDrawCrossX(&calibImage,X,Y, 4);
        //calibImage.CVector[Y][3*X+1]=0;
        //calibImage.CVector[Y][3*X+2]=255;
    }
    

    for(int i=0;i<idxList.size();i++)//update distance
    {
        idxList[i].dist = hypot(
            ldData[idxList[i].idx1].Center.X-ldData[idxList[i].idx2].Center.X,
            ldData[idxList[i].idx1].Center.Y-ldData[idxList[i].idx2].Center.Y
            );
    }
    
    vector<acv_XY> ret_vec_mean;
    findMeanVec(ldData, idxList,0.6,ret_vec_mean);
    vector<acv_XY> ret_vec_sigma;

    for(int i=0;i<ret_vec_mean.size();i++) 
    {
        if(ret_vec_mean[i].X+ret_vec_mean[i].Y<0)
        {
            ret_vec_mean[i].X*=-1;
            ret_vec_mean[i].Y*=-1;
        }
        LOGV(">>>%f,%f",ret_vec_mean[i].X,ret_vec_mean[i].Y);
        //acvDrawCrossX(&labelImg, cr.X, cr.Y, 4);
    }

    if(ret_vec_mean.size()!=2)
    {
        LOGV("Main vector has to be size in 2 but we found %d",ret_vec_mean.size());
        return retParam;
    }


    //Barrel distortion 
    //x and y starts from r center
    //x=x-cx;
    //x=y-cy;
    //r=hypot(x,y) 
    //x_ = x(1+k1*r^2+k2*r^4)
    //y_ = y(1+k1*r^2+k2*r^4)
    LOGV("..................%d",ret_vec_mean.size());



    float minXY=100000000;
    float minXY_idx=-1;
    for(int i=2;i<ldData.size();i++)//Find left top
    {
        if(minXY>ldData[i].Center.X+ldData[i].Center.Y)
        {
            minXY=ldData[i].Center.X+ldData[i].Center.Y;
            minXY_idx = i;
        }
        ldData[i].LTBound.X=dNAN_;//Use LTBound as new coord tmp data
        ldData[i].LTBound.Y=dNAN_;
        
    }
    ldData[idxList[0].idx1].LTBound.X=0;//Set a start seed
    ldData[idxList[0].idx1].LTBound.Y=0;
    

    int deadClock=100;
    bool do_update=true;
    while(do_update)
    {
        do_update=false;
        //find all that can be updated( one idx has recorded XY but another idx has NAN XY)
        for(int i=0;i<idxList.size();i++)
        {
            float X1 = ldData[idxList[i].idx1].LTBound.X ;
            float X2 = ldData[idxList[i].idx2].LTBound.X ;
            
            if( X1==X1 ^ X2==X2 )//NAN!=NAN
            {//If there is an only one updated coord in idx1/idx2 so we can update another
                acv_XY vec=acvVecSub(
                    ldData[idxList[i].idx2].Center,
                    ldData[idxList[i].idx1].Center); 
                acv_XY nvec=acvVecNormalize(vec);
                float dotPX = acv2DDotProduct(nvec,ret_vec_mean[0]);
                float dotPY = acv2DDotProduct(nvec,ret_vec_mean[1]);


                int srcIdx,dstIdx;
                if(X1==X1)//Means we gonna set idx2 coord
                {
                    srcIdx = idxList[i].idx1;
                    dstIdx = idxList[i].idx2;
                }
                else//Means we gonna set idx1 coord
                {
                    srcIdx = idxList[i].idx2;
                    dstIdx = idxList[i].idx1;
                    dotPX=-dotPX;
                    dotPY=-dotPY;
                }
                do_update=true;

                
                int coordX = ldData[srcIdx].LTBound.X ;
                int coordY = ldData[srcIdx].LTBound.Y ;
                //LOGV("coord:%f,%f",coordX,coordY);
                if(abs(dotPX)>abs(dotPY))//x axis as main direction
                {
                    coordX+=(dotPX>0)?1:-1;
                }else
                {
                    coordY+=(dotPY>0)?1:-1;
                }
                

                if(do_update)
                {
                    ldData[dstIdx].LTBound.X=coordX;
                    ldData[dstIdx].LTBound.Y=coordY;
                }
            }
        }
    }
    int minX=10000,maxX=-10000;
    int minY=10000,maxY=-10000;

    for(int i=2;i<ldData.size();i++)//Find left top
    {   
        if(minX>ldData[i].LTBound.X)minX=ldData[i].LTBound.X;
        if(maxX<ldData[i].LTBound.X)maxX=ldData[i].LTBound.X;
        if(minY>ldData[i].LTBound.Y)minY=ldData[i].LTBound.Y;
        if(maxY<ldData[i].LTBound.Y)maxY=ldData[i].LTBound.Y;
    }
    
    for(int i=2;i<ldData.size();i++)//Find left top
    {   
        ldData[i].LTBound.X-=minX;
        ldData[i].LTBound.Y-=minY;
    }
    maxX-=minX;
    minX=0;
    maxY-=minY;
    minY=0;
    LOGV("X:%d~%d Y:%d~%d" ,minX,maxX,minY,maxY);


    vector<vector<int>> boardCoord;
    boardCoord.resize(maxY+1);
    for(int i=0;i<boardCoord.size();i++)
    {
        boardCoord[i].resize(maxX+1);
        for(int j=0;j<boardCoord[i].size();j++)
        {
            boardCoord[i][j]=-1;
        }
    }

    for(int i=2;i<ldData.size();i++)//Find left top
    {   
        float X = ldData[i].LTBound.X;
        float Y = ldData[i].LTBound.Y;
        if(X!=X)continue;
        boardCoord[(int)Y][(int)X]=i;
    }


    /*if(0){

        for(int i=0;i<boardCoord.size();i++)
        {
            int preIdx=-1;
            for(int j=0;j<boardCoord[i].size();j++)
            {
                int idx = boardCoord[i][j];
                if(j>0 && (preIdx!=-1 && idx!=-1))
                {

                    acvDrawLine(&labelImg, 
                    ldData[idx].Center.X, ldData[idx].Center.Y, 
                    ldData[preIdx].Center.X, ldData[preIdx].Center.Y, 
                    255, 0,0,1);
                }
                preIdx= idx;

                printf("%4d ",boardCoord[i][j]);
            }
            printf("\n");
        }

        acvSaveBitmapFile("data/tmp.bmp",&labelImg);


    }*/
    acv_XY dCenter={X:(float)((labelImg.GetWidth()-1)/2),Y:(float)((labelImg.GetHeight()-1)/2)}; 
    //dCenter.X+=5;
    //dCenter.Y-=4;
    acv_XY coordCenter=imgXY2PatternCoord(dCenter, boardCoord, ldData, ret_vec_mean, distMean);

    double K0=1,K1=0,K2=0;
    float alpha=0.9;
    
    double dK0=0,dK1=0,dK2=0,aveErr=0,maxErr=0;
    float RNorm = labelImg.GetWidth()/2.0;

    acv_XY vec_offset;
    float Y_scale = 1/1.000;
    int iterNum=10000;
    for(int i=0;i<iterNum;i++)
    {
        aveErr=maxErr = dK0=dK1=dK2=0;
        vec_offset=acvVecMult(vec_offset,0);
        int count =0;
        for(int j=2;j<ldData.size();j++)
        {
            if(ldData[j].area<0)continue;
            int idx1 = j;


            acv_XY v1 = acvVecSub(ldData[idx1].Center,dCenter);
            v1.Y*=Y_scale;
            float R1 = hypot(v1.Y,v1.X)/RNorm;

                
            float R1_sq=R1*R1;
            float mult = K0+K1*R1_sq+K2*R1_sq*R1_sq;
            
            float R_coord = acvDistance(ldData[idx1].LTBound,coordCenter)*distMean/RNorm;
            if(R_coord==0 || R_coord!=R_coord)continue;

            float error = R_coord-R1*mult;
            vec_offset=acvVecAdd(vec_offset, acvVecMult(v1,error));
            
            float error_sq = error*error;
            /*if(i>800 && sqrt(error_sq)*RNorm>0.8)
            {
                ldData[i].area=-1;
                continue;
            }*/


            if(maxErr<error_sq)maxErr = error_sq;
            aveErr+=error_sq;
            dK0+=(error);
            dK1+=(error)*R1_sq;
            dK2+=(error)*R1_sq*R1_sq;

            count++;
        
        }
        dK0/=count;
        dK1/=count;
        dK2/=count;
        aveErr/=count;
        vec_offset = acvVecMult(vec_offset,RNorm/count);


        //K2-=alpha*midR_sq*midR_sq;

        {
            
            //LOGV("%f,  %f, %f  %f ",R_coord,R,R1,tar_K1);
            if(i%1000==0 || i==iterNum-1)
            {
                LOGV("K: %g %g %g aveE:%f,%f",K0,K1,K2,sqrt(aveErr)*RNorm,sqrt(maxErr)*RNorm);
                LOGV("Center: %g,%g RNorm:%f",vec_offset.X,vec_offset.Y,RNorm);
            }
            K0+=dK0*alpha;
            K1+=dK1*alpha;
            K2+=dK2*alpha/20;
            //alpha+=0.1*(0.5-alpha);
            //LOGV("K:%f %g %g",K0,K1,K2);
            //K2+=dK2;
        }
    }

    retParam.calibrationCenter = dCenter;
    retParam.RNormalFactor = RNorm;
    retParam.K0=K0;
    retParam.K1=K1;
    retParam.K2=K2;

    
    LOGD("K: %g %g %g RNormalFactor:%g",K0,K1,K2,retParam.RNormalFactor);
    LOGD("Center: %g,%g",retParam.calibrationCenter.X,retParam.calibrationCenter.Y);

    for(int i=2;i<ldData.size();i++)//Find left top
    {   
        
        acv_XY calibP = acvVecRadialDistortionRemove(ldData[i].Center,retParam);
        ldData[i].RBBound = calibP;
        acv_XY calibP_rec = acvVecRadialDistortionApply(ldData[i].RBBound,retParam);

        /*LOGV("%f",acvDistance(calibP_rec,ldData[i].Center));

        acvDrawLine(&labelImg, 
                    ldData[i].Center.X, ldData[i].Center.Y, 
                    calibP_rec.X, calibP_rec.Y, 
                    0, 255,0,1);*/

    }



    distMean = 0;
    for(int i=0;i<idxList.size();i++)
    {
        idxList[i].dist = acvDistance(ldData[idxList[i].idx1].RBBound,ldData[idxList[i].idx2].RBBound);
        distMean+=idxList[i].dist;
    }
    distMean/=idxList.size();

    retParam.ppb2b=distMean;
    float sigma=0;

    for(int i=0;i<idxList.size();i++)
    {
        float dist = idxList[i].dist;
        float tmp = distMean-dist;
        sigma+=tmp*tmp;
    }
    sigma=sqrt(sigma/idxList.size());

    LOGD("distMean:%f sigma:%f",distMean,sigma);

    // 0.989113 0.0106168 0.000557766
    /*if(0){//Draw debug grid to check if the corrected dot is in the straight line

        for(int i=0;i<boardCoord.size();i++)
        {
            int preIdx=-1;
            int idx1 = boardCoord[i][0];
            int idx2 = boardCoord[i][boardCoord[i].size()-1];
            
            acvDrawLine(&labelImg, 
            ldData[idx1].RBBound.X, ldData[idx1].RBBound.Y, 
            ldData[idx2].RBBound.X, ldData[idx2].RBBound.Y, 
            0, 0,255,1);
        }
        
        for(int j=0;j<boardCoord[0].size();j++)
        {
            int preIdx=-1;
            int idx1 = boardCoord[0][j];
            int idx2 = boardCoord[boardCoord.size()-1][j];
            
            acvDrawLine(&labelImg, 
            ldData[idx1].RBBound.X, ldData[idx1].RBBound.Y, 
            ldData[idx2].RBBound.X, ldData[idx2].RBBound.Y, 
            0, 0,255,1);
        }
        for(int i=0;i<boardCoord.size();i++)
        {
            int preIdx=-1;
            for(int j=0;j<boardCoord[i].size();j++)
            {
                int idx = boardCoord[i][j];
                if(j>0 && (preIdx!=-1 && idx!=-1))
                {

                    // acvDrawLine(&labelImg, 
                    // ldData[idx].Center.X, ldData[idx].Center.Y, 
                    // ldData[preIdx].Center.X, ldData[preIdx].Center.Y, 
                    // 255, 0,0,1);

                    
                    // acvDrawLine(&labelImg, 
                    // ldData[idx].RBBound.X, ldData[idx].RBBound.Y, 
                    // ldData[preIdx].RBBound.X, ldData[preIdx].RBBound.Y, 
                    // 0, 255,0,1);
                    int X = ldData[idx].RBBound.X;
                    int Y = ldData[idx].RBBound.Y;
                    labelImg.CVector[Y][3*X+0]=0;
                    labelImg.CVector[Y][3*X+1]=255;
                    labelImg.CVector[Y][3*X+2]=255;

                    X+=1;
                    labelImg.CVector[Y][3*X+0]=0;
                    labelImg.CVector[Y][3*X+1]=255;
                    labelImg.CVector[Y][3*X+2]=255;

                    
                    X-=1;
                    Y+=1;
                    labelImg.CVector[Y][3*X+0]=0;
                    labelImg.CVector[Y][3*X+1]=255;
                    labelImg.CVector[Y][3*X+2]=255;

                    
                    X+=1;
                    labelImg.CVector[Y][3*X+0]=0;
                    labelImg.CVector[Y][3*X+1]=255;
                    labelImg.CVector[Y][3*X+2]=255;
                }
                preIdx= idx;

            }
        }
        acvSaveBitmapFile("data/tmp.bmp",&labelImg);

    }



    if(0){//Check sigma value
        

        float distMean = 0;

        for(int i=0;i<idxList.size();i++)
        {
            distMean+=acvDistance(ldData[idxList[i].idx1].RBBound,ldData[idxList[i].idx2].RBBound);
        }
        distMean/=idxList.size();


        float sigma=0;
        float maxError=0;
        int sampleCount=100000;
        for(int i=0;i<sampleCount;i++)
        {
            int idx1 = (rand()%(ldData.size()-2))+2;
            int idx2 = (rand()%(ldData.size()-2))+2;

            if(idx1==idx2 || ldData[idx1].LTBound.X!=ldData[idx1].LTBound.X || ldData[idx2].LTBound.X!=ldData[idx2].LTBound.X)
            {
                i--;
                continue;
            }
            
            if(ldData[idx1].area<0|| ldData[idx2].area<0)
            {
                i--;
                continue;
            }

            float dist = acvDistance(ldData[idx1].RBBound,ldData[idx2].RBBound);
            float dist_ = acvDistance(ldData[idx1].LTBound,ldData[idx2].LTBound);
            float tmp = distMean-dist/dist_;
            if(maxError<abs(tmp))
            {
                maxError=abs(tmp);
                    
            }
            sigma+=tmp*tmp;
        }
        sigma=sqrt(sigma/sampleCount);

        LOGV("distMean:%f sigma:%g maxError:%f",distMean,sigma,maxError);

        for(int i=0;i<sampleCount;i++)
        {
            int idx1 = (rand()%(ldData.size()-2))+2;
            int idx2 = (rand()%(ldData.size()-2))+2;

            if(idx1==idx2 || ldData[idx1].LTBound.X!=ldData[idx1].LTBound.X || ldData[idx2].LTBound.X!=ldData[idx2].LTBound.X)
            {
                i--;
                continue;
            }
            
            if(ldData[idx1].area<0|| ldData[idx2].area<0)
            {
                i--;
                continue;
            }

            float dist = acvDistance(ldData[idx1].RBBound,ldData[idx2].RBBound);
            float dist_ = acvDistance(ldData[idx1].LTBound,ldData[idx2].LTBound);
            float tmp = distMean-dist/dist_;
            if(0.7<abs(tmp))
            {
                acvDrawLine(&calibImage, 
                ldData[idx1].RBBound.X, ldData[idx1].RBBound.Y, 
                ldData[idx2].RBBound.X, ldData[idx2].RBBound.Y, 
                255, 0,0,4);
                
                LOGV("dist:%f",dist);
            }
        }

    }



    if(0){//debug image

        for(int i=2;i<ldData.size();i++)//Find left top
        {   
            
            acv_XY v1 = acvVecSub(ldData[i].Center,retParam.calibrationCenter);
            v1.Y*=Y_scale;
            float R1 = hypot(v1.Y,v1.X)/retParam.RNormalFactor;
            float R1_sq=R1*R1;
            
            float R_coord = acvDistance(ldData[i].LTBound,coordCenter)*distMean/retParam.RNormalFactor;

            float mult = retParam.K0+retParam.K1*R1_sq+retParam.K2*R1_sq*R1_sq;
            acv_XY new_Center = acvVecMult(v1,mult);
            new_Center=acvVecAdd(new_Center,dCenter);


        
            float error = R_coord-R1*mult;
            float error_sq = error*error;

            acvDrawLine(&calibImage, 
            ldData[i].Center.X, ldData[i].Center.Y, 
            new_Center.X, new_Center.Y, 
            255, 0,0,4);

            float width = sqrt(error_sq)*RNorm;
            if(width>1.0)
                acvDrawCrossX(&calibImage,new_Center.X,new_Center.Y,14,255,0 , 255,10*(width-1)+2);
            //calibImage.CVector[Y][3*X+1]=0;
            //calibImage.CVector[Y][3*X+2]=255;
        }
        acvSaveBitmapFile("data/calibImage.bmp",&calibImage);
    }*/

    return retParam;
}


static acv_XY LableCenterRefine(acvImage &grayLevelImage,acv_LabeledData ldat,int range)
{
    acv_XY SPoint = ldat.Center;
    float rangeCenter=((float)range-1)/2;
    SPoint.X-=rangeCenter;
    SPoint.Y-=rangeCenter;

    int max,min=1000,mean=0;
    for(int i=0;i<range;i++)
    {
        int Y = round(SPoint.Y)+i;
        if(Y<0)continue;
        if(Y>=grayLevelImage.GetHeight())break;
        for(int j=0;j<range;j++)
        {
            int X = round(SPoint.X)+j;
            if(X<0)continue;
            if(X>=grayLevelImage.GetWidth())break;
            int intensity = grayLevelImage.CVector[Y][X*3];
            if(max<intensity)max=intensity;
            if(min>intensity)min=intensity;
            mean+=intensity;
        }
    }
    mean/=range*range;
    min=mean;
    max=mean+20;


    acv_XY center={0};
    float  ptCount=0;
    for(int i=0;i<range;i++)
    {
        float weightY=1;//rangeCenter-i;
        int Y = round(SPoint.Y)+i;
        if(Y<0)continue;
        if(Y>=grayLevelImage.GetHeight())break;
        for(int j=0;j<range;j++)
        {
            
            int X = round(SPoint.X)+j;
            if(X<0)continue;
            if(X>=grayLevelImage.GetWidth())break;

            float weightX=1;//rangeCenter-j;
            float weight = weightX*weightY;
            weight*=weight;

            float intensity = grayLevelImage.CVector[Y][X*3];

            float convInternsity = 1 - (intensity-min)/(max-min);
            if(convInternsity<0)convInternsity=0;
            float w = weight*convInternsity;
            
            center.X+=w*X;
            center.Y+=w*Y;
            ptCount+=w;
            //ptCount.X+=weight*X;
        }
    }
    center.X/=ptCount;
    center.Y/=ptCount;
    
    //LOGV("%f,%f,%f",center.X,center.Y,ptCount);
    return center;
}


static int markPointExtraction(acvImage &labelImg, vector<acv_LabeledData> &ldData,vector<struct idx_dist>& ret_idxList)
{
    
    const int topListN=4;
    struct idx_dist idist[topListN];
    
    float targetArea =  findMedianSize(ldData);

    LOGV("targetArea:%f",targetArea);
    float range_Area=4;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area< targetArea/range_Area|| ldat.area>targetArea*range_Area )
        {
            LOGV("targetArea[%d]:%d",i,ldat.area);
            ldData[i].area = -1;
        }
    }
    float range=1.2;
    vector<float> distances;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area<=0)continue;
        //LOGV(">%d>>%f, %f",ldat.area,ldat.Center.X,ldat.Center.Y);
        for(int j=0;j<topListN;j++)
        {
            idist[j]=getResetIdxDist();
        }
        
        for(int j=i+1;j<ldData.size();j++) 
        {
            auto ldat2 = ldData[j];
            if(ldat2.area<=0)continue;
            struct idx_dist newIdx={ 
                idx1:i,
                idx2:j,
                dist:acvDistance(ldat.Center,ldat2.Center)
            };
            idixMinPush(idist, topListN, newIdx);
            //if(i==j)continue;

            
        }

        
        for(int j=0;j<topListN;j++) 
        {
            if(idist[j].idx2<0)continue;
            
            auto ldat2 = ldData[idist[j].idx2];
            distances.push_back(idist[j].dist);
        }

    }

    if(distances.size()==0)return -1;
    
    sort(distances.begin(), distances.end()); 
    float mainDist = distances[distances.size()*1/3];

    auto count=0;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area<=0)continue;
        
        for(int j=i+1;j<ldData.size();j++) 
        {
            auto ldat2 = ldData[j];
            if(ldat2.area<=0)continue;

            float dist = acvDistance(ldat.Center,ldat2.Center);
            
            if(dist< mainDist/range|| dist>mainDist*range )continue;

            struct idx_dist idxD={
                idx1:i,
                idx2:j,
                dist:dist
            };
            ret_idxList.push_back(idxD);
            count++;
        }

    }

    return 0;
}
