#include <playground.h>
#include "MorphEngine.h"

static int markPointExtraction(acvImage &labelImg, vector<acv_LabeledData> &ldData,vector<struct idx_dist>& ret_idxList);

static acv_XY LableCenterRefine(acvImage &grayLevelImage,acv_LabeledData ldat,int range);


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


void findMeanVec(vector<acv_LabeledData> &ldData, vector<struct idx_dist> &idxList,float cosSim,vector<acv_XY> &ret_vec)
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


acv_XY PatternCoord2imgXY(
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
            float NAN_=0/0;
            return {X:NAN_,Y:NAN_};
        }
        int idxTL = boardCoordIdx[tl.Y  ][tl.X  ];
        int idxTR = boardCoordIdx[tl.Y  ][tl.X+1];
        int idxBL = boardCoordIdx[tl.Y+1][tl.X  ];
        int idxBR = boardCoordIdx[tl.Y+1][tl.X+1];

        if(idxTL<0 || idxTR<0 || idxBL<0 || idxBR<0)
        {
            float NAN_=0/0;
            return {X:NAN_,Y:NAN_};
        }

        acv_XY imgXY_Top = acvVecInterp(ldData[idxTL].Center,ldData[idxTR].Center,ratio.X);
        acv_XY imgXY_Btn = acvVecInterp(ldData[idxBL].Center,ldData[idxBR].Center,ratio.X);

        return acvVecInterp(imgXY_Top,imgXY_Btn,ratio.Y);
    }

acv_XY imgXY2PatternCoord(
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
        LOGV("%d,%d",idxS,new_idxS);
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


void calcCameraCalibration()
{
    
    acvImage calibImage;
    acvLoadBitmapFile(&calibImage,"data/calibration_Img/rectSmallS.bmp");

    
    acvImage labelImg;
    acvImage tmp;
    acvImage tmp2;

    labelImg.ReSize(&calibImage);
    tmp.ReSize(&calibImage);
    tmp2.ReSize(&calibImage);
    
    acvCloneImage(&calibImage,&labelImg, -1);

    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvBoxFilter(&tmp,&labelImg,3);
    acvCloneImage(&labelImg,&tmp, -1);
    acvThreshold(&labelImg, 90);


    //Draw a labeling black cage for labling algo, which is needed for acvComponentLabeling
    acvDrawBlock(&labelImg, 1, 1, labelImg.GetWidth() - 2, labelImg.GetHeight() - 2);


    //The labeling starts from (1 1) => (W-2,H-2), ie. it will not touch the outmost pixel to simplify the boundary condition
    //You need to draw a black/white cage to work(not crash).
    //The advantage of black cage is you can know which area touches the boundary then we can exclude it
    
    vector<acv_LabeledData> ldData;
    acvComponentLabeling(&labelImg);
    acvLabeledRegionInfo(&labelImg, &ldData);
    
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


    /*for(int i=2;i<ldData.size();i++) //refine the center
    {
        
        acv_XY cr = LableCenterRefine(calibImage,ldData[i],distMean);
        ldData[i].Center = cr;
        int X = round(cr.X);
        int Y = round(cr.Y);
        //acvDrawCrossX(&calibImage,X,Y, 4);
        calibImage.CVector[Y][3*X+1]=0;
        calibImage.CVector[Y][3*X+2]=255;
    }*/
    

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
        return;
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
    float _NAN_ = 0.0 / 0.0;
    for(int i=2;i<ldData.size();i++)//Find left top
    {
        if(minXY>ldData[i].Center.X+ldData[i].Center.Y)
        {
            minXY=ldData[i].Center.X+ldData[i].Center.Y;
            minXY_idx = i;
        }
        ldData[i].LTBound.X=_NAN_;//Use LTBound as new coord tmp data
        ldData[i].LTBound.Y=_NAN_;
        
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


    if(0){

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


    }
    acv_XY dCenter={X:(float)((labelImg.GetWidth()-1)/2),Y:(float)((labelImg.GetHeight()-1)/2)}; 
    acv_XY coordCenter=imgXY2PatternCoord(dCenter, boardCoord, ldData, ret_vec_mean, distMean);

    double K0=1,K1=0,K2=0;
    float alpha=1.5;
    float xSum=0;
    float xSumC=0;
    
    double dK0=0,dK1=0,dK2=0,aveErr=0;
    int updateBatch=100;
    int RNorm = labelImg.GetWidth();
    for(int i=0;i<100000;i++)
    {
        int idx1 = (rand()%(ldData.size()-2))+2;
        while(ldData[idx1].LTBound.X !=ldData[idx1].LTBound.X )
            idx1 = (rand()%(ldData.size()-2))+2;


        acv_XY v1 = acvVecSub(ldData[idx1].Center,dCenter);
        float R1 = acvDistance(dCenter,ldData[idx1].Center)/RNorm;

            
        float R1_sq=R1*R1;
        float mult = K0+K1*R1_sq+K2*R1_sq*R1_sq;
        float R_coord = acvDistance(ldData[idx1].LTBound,coordCenter)*distMean/RNorm;
        if(R_coord==0)continue;

        
        /*float tar_K1 = ((R_coord/R1-1)/(R1*R1));

        float R=R1*mult;
        xSum+=tar_K1;
        xSumC++;
        K1=tar_K1;*/
        float error = R_coord-R1*mult;
        float error_sq = error*error;
        if(aveErr<error_sq)aveErr = error_sq;
        dK0+=(error)/updateBatch;
        dK1+=(error)*R1_sq/updateBatch;
        dK2+=(error)*R1_sq*R1_sq/updateBatch;
        
        //K2-=alpha*midR_sq*midR_sq;

        if(i%updateBatch==0)
        {
            
            //LOGV("%f,  %f, %f  %f ",R_coord,R,R1,tar_K1);
            LOGV("dK %g %g %g aveE:%f",dK0,dK1,dK2,sqrt(aveErr)*RNorm);
            K0+=dK0*alpha;
            K1+=dK1*alpha*100;
            K2+=dK2*alpha/10;
            alpha+=0.1*(0.1-alpha);
            //LOGV("K:%f %g %g",K0,K1,K2);
            //K2+=dK2;
            aveErr = dK0=dK1=dK2=0;
        }
    }




    return;
}

/*
void XXX();
    MorphEngine moEng;
    int XFactor = 10;
    moEng.RESET(10*XFactor,labelImg.GetWidth(),labelImg.GetHeight());




    int drawMargin=80;
    int iterCount=10;
    int skipF=6;
    float lRate_start=1*skipF*skipF;
    float lRate_end=0.7*skipF*skipF;
    for(int iter=0;iter<iterCount;iter++)
    {
        if(iter%1==0)moEng.regularization(0.4);
        int offsetX=iter%skipF;
        int offsetY=(iter/skipF)%skipF;
        float traningRate=lRate_start+(lRate_end-lRate_start)*iter/iterCount;
        LOGV("=======================");
        float meanDist=0;
        for(int i=0;i<idxList.size();i++)
        {
            acv_LabeledData d1= ldData[idxList[i].idx1];
            acv_LabeledData d2= ldData[idxList[i].idx2];
            
            acv_XY mapPt1,mapPt2;
            moEng.Mapping(d1.Center,&mapPt1);
            moEng.Mapping(d2.Center,&mapPt2);
            idxList[i].dist = acvDistance(d1.Center,d2.Center);
            meanDist+= idxList[i].dist;
        }
        meanDist/=idxList.size();
        LOGV("%f",meanDist);


        
        for(int i=0;i<idxList.size();i++)
        {
            acv_LabeledData d1= ldData[idxList[i].idx1];
            acv_LabeledData d2= ldData[idxList[i].idx2];
            
            acv_XY mapPt1,mapPt2;
            moEng.Mapping(d1.Center,&mapPt1);
            moEng.Mapping(d2.Center,&mapPt2);

            LOGV("%d+%d>>%f",idxList[i].idx1,idxList[i].idx2,idxList[i].dist);
            
            //moEng.Mapping_adjust(adjposition,adjVec);
        }

        moEng.optimization(0.7);
    }
    
    //for(int i=0;i<labelImg.GetHeight();i++)
    {
        for(int j=0;j<labelImg.GetWidth();j++)
        {
            if(labelImg.CVector[i][3*j]==255)
            {
                tmp.CVector[i][3*j]=255;
            }
        }
    }




    //acvSaveBitmapFile("data/tmp.bmp",&tmp);


}*/


static acv_XY LableCenterRefine(acvImage &grayLevelImage,acv_LabeledData ldat,int range)
{
    acv_XY SPoint = ldat.Center;
    float rangeCenter=((float)range-1)/2;
    SPoint.X-=rangeCenter;
    SPoint.Y-=rangeCenter;
    acv_XY center={0};

    int max,min=1000,mean=0;
    for(int i=0;i<range;i++)
    {
        for(int j=0;j<range;j++)
        {
            int Y = round(SPoint.Y)+i;
            int X = round(SPoint.X)+i;
            int intensity = grayLevelImage.CVector[Y][X*3];
            if(max<intensity)max=intensity;
            if(min>intensity)min=intensity;
            mean+=intensity;
        }
    }
    mean/=range*range;



    float  ptCount=0;
    for(int i=0;i<range;i++)
    {
        float weightY=1;//rangeCenter-i;
        for(int j=0;j<range;j++)
        {
            
            int Y = round(SPoint.Y)+i;
            int X = round(SPoint.X)+j;

            float weightX=1;//rangeCenter-j;
            float weight = weightX*weightY;
            weight*=weight;

            float intensity = grayLevelImage.CVector[Y][X*3];

            float convInternsity = 1 - (intensity-min)/(max-min);
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
    ret_idxList.resize(0);
    float targetArea =  findMedianSize(ldData);

    //LOGV("targetArea:%f",targetArea);
    float range_Area=2;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area< targetArea/range_Area|| ldat.area>targetArea*range_Area )
        {
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
