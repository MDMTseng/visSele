
#include <math.h>
#include "acvImage.hpp"
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "acvImage_ComponentLabelingTool.hpp"

#define MathPI           3.141592654

#define SignInfoAngleNum 180
#define Radian2Degree    SignInfoAngleNum/MathPI/2
#define Degree2Radian    MathPI*2/SignInfoAngleNum

#define SignInfoDistanceMuti 1000


//const char SearchTable[SearchWay]={0,7,1,6,2,5,3};


/*0 1 2
  7 X 3
  6 5 4
*/
const int SearchTable[][2]= {{0,0},{7,1},{1,7},{6,2},{2,6},{5,3},{3,5}};
const int WalkV[8][2]= {{-1,-1},{-1,0},{-1,1},{0,1},{1,1},{1,0},{1,-1},{0,-1}};
int RoundTypeChoose(float Num,char *SearchType)
{
    if(Num-(int)Num<0.5)
    {
        *SearchType=0;
        return Num;
    }
    *SearchType=1;
    return Num+1;


}

#define LimitedError 3
#define Math_PI 3.14159265
/*


    /A               ___A  -|-
   /   A       ------    A  |
  /     A -----          A  | LimitedError
 /___--_A________________A _|_




*/



#define RingCounter(X,size)   (X<0)? (X+size):((X>=size)? X-size:X)

void acvRingDataMeanFilter(int * OutRingArray,int * InRingArray
                           ,int RingSize,int Size)
{
    int Sum,TmpNum,j,k;

    Sum=InRingArray[0];


    for(j=0; j<Size; j++)
    {
        Sum+=(InRingArray[j+1]+InRingArray[RingSize-j-1]);
    }
    OutRingArray[0]=Sum/(Size*2+1);
    for(j=1; j<=Size; j++)
    {
        Sum-= InRingArray[RingSize+j-Size-1];
        Sum+= InRingArray[j+Size];

        OutRingArray[j]=Sum/(Size*2+1);
    }


    for(j=Size+1; j<RingSize-Size; j++)
    {

        Sum-= InRingArray[j-Size-1];
        Sum+= InRingArray[j+Size];

        OutRingArray[j]=Sum/(Size*2+1);
        //OutRingArray[j]=0;
    }
    for(j=RingSize-Size; j<RingSize; j++)
    {

        Sum-= InRingArray[j-Size-1];
        Sum+= InRingArray[j+Size-RingSize];

        OutRingArray[j]=Sum/(Size*2+1);
    }


}


void acvRingDataDiffFilter(int * OutRingArray,int * InRingArray,int RingSize)
{
    int j;

    OutRingArray[0]=InRingArray[0]-InRingArray[RingSize-1];

    for(j=1; j<RingSize; j++)
    {
        OutRingArray[j]=InRingArray[j]-InRingArray[j-1];
    }

}

const int ContourWalkV[8][2]= {{-1,-1},{-1,0},{-1,1},{0,1},{1,1},{1,0},{1,-1},{0,-1}};

/*
void acvLabeledSignatureByContour(acvImage  *LabeledPic,
                        DyArray<int> * LabelInfo,DyArray<int> * SignInfo)
{
        int i,j,StartPos[2];
        SignInfo->ReLength(SignInfoAngleNum*LabelInfo->Length()/LabelingInfoColumn);
        for(i=0;i<SignInfo->Length();i++)
                *SignInfo->GetPointer(i)=0;
        for(j=i=0;i<LabelInfo->Length();i+=LabelingInfoColumn,j+=SignInfoAngleNum)
        {
                if(*LabelInfo->GetPointer(i+LabelingArea))
                {
                        StartPos[0]=*LabelInfo->GetPointer(i+LabelingStartPosX);
                        StartPos[1]=*LabelInfo->GetPointer(i+LabelingStartPosY);
                        acvContourSignature(LabeledPic,
                        LabelInfo->GetPointer(i+LabelingCentroidX),
                        LabelInfo->GetPointer(i+LabelingStartPosX),
                        SignInfo->GetPointer(j),5);
                        acvRingDataDeZero(SignInfo->GetPointer(j),SignInfo->GetPointer(j),
                        SignInfoAngleNum);

                }
        }


}
*/

float acvSpatialMatchingError(acvImage  *Pic,acv_XY *PicPtList,
                                 acvImage *targetMap,acv_XY *TarPtList,int ListL)
{
    float errorSum=0;

    //  []
    //[][][]
    //  []
    for(int i=0; i<ListL; i++)
    {
        acv_XY tarpt=TarPtList[i];
        acv_XY picpt=PicPtList[i];
        float error=(   acvUnsignedMap1Sampling_Nearest(Pic,picpt,0)-
                        acvUnsignedMap1Sampling(targetMap,tarpt,0));

        error*=error;
        errorSum+=error*acvUnsignedMap1Sampling_Nearest(targetMap,tarpt,1);
    }
    return errorSum;
}

//Displacement, Scale, Aangle
float acvSpatialMatchingGradient(acvImage  *Pic,acv_XY *PicPtList,
                                 acvImage *targetMap,acvImage *targetGradient,acv_XY *TarPtList,
                                 acv_XY *ErrorGradientList,int ListL)
{
    float errorSum=0;

    //  []
    //[][][]
    //  []
    float normalFactor=0;
    for(int i=0; i<ListL; i++)
    {
        acv_XY tarpt=TarPtList[i];
        acv_XY picpt=PicPtList[i];
        float weight=acvUnsignedMap1Sampling_Nearest(targetMap,tarpt,1)/255;
        normalFactor+=weight;
        float error=(   acvUnsignedMap1Sampling_Nearest(Pic,picpt,0)-
                        acvUnsignedMap1Sampling(targetMap,tarpt,0))
                        *weight;

        acv_XY gradient=acvSignedMap2Sampling_Nearest(targetGradient,tarpt);
        //error=(error<0)?-128:128;

        //printf(">%f %d\n",gradient.X,(char)targetGradient->CVector[(int)tarpt.Y][(int)tarpt.X*3]);
        //Sobel [0] ^  [1] <
        ErrorGradientList[i].X=error*gradient.X;
        ErrorGradientList[i].Y=error*gradient.Y;


        error*=error;
        errorSum+=error;
    }
    return errorSum/normalFactor;
}

int interpolateSignData(std::vector<acv_XY> &signature,int start,int end)
{   //Always from start to end increase

    int tmp;
    //0~2 => dist 2 abs(2-0)
    //199~2 (signature.size()==200) => dist 3 (signature.size()+(2-199))

    //2~0
    //2~199

    int distance=(end-start);
    if(distance<0)
    {
        distance=signature.size()+distance;
    }

    if(distance>signature.size()/2)
    {
        distance=signature.size()-distance;
        int tmp=end;
        end=start;
        start=tmp;
    }
    if(distance<2)return distance;

    int head=start+1;
    int i;
    float sf=signature[start].X;
    float se_diff=(signature[end].X-sf)/distance;

    for(i=1; i<distance; i++,head++)
    {
        if(head>=signature.size())head-=signature.size();
        float X=sf+i*(se_diff);
        if(signature[head].X<X)
        {
            signature[head].X=X;
            signature[head].Y=signature[start].Y;
        }

    }
    return distance;
}

bool acvContourCircleSignature
(acvImage  *LabeledPic,acv_LabeledData ldata,int labelIdx,std::vector<acv_XY> &signature)
{
    int ret=-1;
    memset((void*)&(signature[0]),0,signature.size()*sizeof(signature[0]));

    int X,Y;
    int startX,startY;
    X=(int)ldata.LTBound.X;
    Y=(int)ldata.LTBound.Y;
    for(int j=X; j<(int)ldata.RBBound.X; j++)
    {
        _24BitUnion *pix=(_24BitUnion*)&(LabeledPic->CVector[Y][j*3]);
        if(pix->_3Byte.Num==labelIdx)
        {
            X=j;
            startX=j;
            startY=Y;
            ret=0;
            break;
        }
    }
    if(ret!=0)return false;


    int preIdx=-1;
    int _1stIdx=-1;
    //0|1|2
    //7|X|3
    //6|5|4
    int dir =3;//>
    do {

        float diffY=Y-ldata.Center.Y;
        float diffX=X-ldata.Center.X;
        float theta=acvFAtan2(diffY,diffX);//-pi ~pi
        //if(theta<0)theta+=2*M_PI;
        int idx=round(signature.size()*theta/(2*M_PI));
        if(idx<0)idx+=signature.size();
        //if(idx>=signature.size())idx-=signature.size();
        if(preIdx==-1)
        {
            _1stIdx=idx;
            preIdx=idx;
        }
        float R=hypot(diffX,diffY);
        if(signature[idx].X<R)
        {
            signature[idx].X=R;
            signature[idx].Y=theta;
            interpolateSignData(signature,preIdx,idx);
        }
        preIdx=idx;
        BYTE* pix=acvContourWalk(LabeledPic,&X,&Y,&dir,1);
        dir-=2;

        /* code */
    } while(X!=startX||Y!=startY);

    interpolateSignData(signature,_1stIdx,preIdx);

}

float SignatureMatchingError(const acv_XY *signature, int offset,
                             const acv_XY *tar_signature, int arrsize, int stride)
{
    if (stride == 0)
        return -1;
    float errorSum = 0;
    int signIdx = offset % arrsize;
    if (signIdx < 0)
        signIdx += arrsize;
    int size = (arrsize - signIdx);
    int i = 0;

    for (; i < size; i += stride, signIdx += stride)
    {
        float error = signature[(signIdx)].X - tar_signature[i].X;
        errorSum += error * error;
    }
    signIdx -= arrsize;
    size = arrsize;
    for (; i < size; i += stride, signIdx += stride)
    {
        float error = signature[(signIdx)].X - tar_signature[i].X;
        errorSum += error * error;
    }
    return errorSum;
}

float SignatureMatchingError(const std::vector<acv_XY> &signature, int offset,
                             const std::vector<acv_XY> &tar_signature, int stride)
{
    return SignatureMatchingError(&(signature[0]), offset, &(tar_signature[0]), signature.size(), stride);
}
#include <float.h>
int SignareIdxOffsetMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature, int roughSearchSampleRate, float *min_error)
{
    if (roughSearchSampleRate < 1)
        return -1;
    int fineSreachRadious = roughSearchSampleRate - 1;
    int minErrOffset = 0;
    float minErr = FLT_MAX; //rough search
    for (int j = 0; j < tar_signature.size(); j += roughSearchSampleRate)
    {
        float error = SignatureMatchingError(signature, j, tar_signature, roughSearchSampleRate);
        if (minErr > error)
        {
            minErr = error;
            minErrOffset = j;
        }
    }
    minErr = FLT_MAX;
    int searchHead = minErrOffset - fineSreachRadious;
    minErrOffset = -1;

    float error;
    //fine search
    for (int i = 0; i < 2 * fineSreachRadious + 1; i++, searchHead++)
    {
        error = SignatureMatchingError(signature, searchHead, tar_signature, 1);
        if (minErr > error)
        {
            minErr = error;
            minErrOffset = searchHead;
        }
    }
    if (minErrOffset < 0)
        minErrOffset += tar_signature.size();
    else if (minErrOffset >= tar_signature.size())
        minErrOffset -= tar_signature.size();
    if (min_error)
        *min_error = minErr;
    return minErrOffset;
}

float SignatureAngleMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature, float *min_error)
{
    int matchingIdx = SignareIdxOffsetMatching(signature, tar_signature, signature.size() / 160, min_error); //magic number
    float angle = matchingIdx * 2 * M_PI / signature.size();
    if (angle < -M_PI)
        angle += 2 * M_PI;
    else if (angle > M_PI)
        angle -= 2 * M_PI;
    return angle;
}
