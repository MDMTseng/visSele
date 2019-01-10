#include <playground.h>
#include "MorphEngine.h"

static int markPointExtraction(acvImage &labelImg, vector<acv_LabeledData> &ldData,vector<struct idx_dist>& ret_idxList);
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
void calcCameraCalibration()
{
    
    acvImage calibImage;
    acvLoadBitmapFile(&calibImage,"data/calibration_Img/rectS.bmp");

    
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

    for(int i=0;i<idxList.size();i++)
    {
        LOGV("%d+%d>>%f",idxList[i].idx1,idxList[i].idx2,idxList[i].dist);
    }

    MorphEngine moEng;
    int XFactor = 10;
    moEng.RESET(10*XFactor,labelImg.GetWidth(),labelImg.GetHeight());




    int drawMargin=80;
    int iterCount=100;
    int skipF=6;
    float lRate_start=1*skipF*skipF;
    float lRate_end=0.7*skipF*skipF;
    for(int iter=0;iter<iterCount;iter++)
    {
        if(iter%1==0)moEng.regularization(0.4);
        int offsetX=iter%skipF;
        int offsetY=(iter/skipF)%skipF;
        float traningRate=lRate_start+(lRate_end-lRate_start)*iter/iterCount;


        
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
    /*for(int i=0;i<labelImg.GetHeight();i++)
    {
        for(int j=0;j<labelImg.GetWidth();j++)
        {
            if(labelImg.CVector[i][3*j]==255)
            {
                tmp.CVector[i][3*j]=255;
            }
        }
    }*/




    //acvSaveBitmapFile("data/tmp.bmp",&tmp);


}
static int markPointExtraction(acvImage &labelImg, vector<acv_LabeledData> &ldData,vector<struct idx_dist>& ret_idxList)
{
    
    const int topListN=4;
    struct idx_dist idist[topListN];
    ret_idxList.resize(0);
    float targetArea =  findMedianSize(ldData);

    LOGV("targetArea:%f",targetArea);
    float range=1.2;
    for(int i=2;i<ldData.size();i++) 
    {
        auto ldat = ldData[i];
        if(ldat.area< targetArea/range|| ldat.area>targetArea*range )
        {
            ldData[i].area = -1;
        }
    }
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
