// Signature matching, lifted out of acvImage/acvImage_ToolBox.cpp when acvImage
// was retired. Every function here works on cv::Point2f and std::vector and has
// nothing to do with the old image container -- of the 147 symbols acvImage
// exported, the five still referenced were all in this family, and they were
// only in that library because that is where they happened to be written.
//
// The functions that DID take acvImage* (acvSpatialMatchingError,
// acvSpatialMatchingGradient, acvContourExtract, acvOuterContourExtraction,
// acvLabeledSignatureByContour and the acvImage* overload of
// acvContourCircleSignature) were dropped rather than ported: nothing outside
// acvImage referenced any of them.

#define _USE_MATH_DEFINES
#include <math.h>
#include "SignatureTools.h"
#include "vis_geom.h"

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
// (a commented-out acvLabeledSignatureByContour used to follow here; it took
//  acvImage* and went with the rest of the container.)

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
    float sf=signature[start].x;
    float se_diff=(signature[end].x-sf)/distance;

    for(i=1; i<distance; i++,head++)
    {
        if(head>=signature.size())head-=signature.size();
        float X=sf+i*(se_diff);
        if(signature[head].x<X)
        {
            signature[head].x=X;
            signature[head].y=signature[start].y;
        }

    }
    return distance;
}

bool acvContourCircleSignature(std::vector<acv_XY> &contour,std::vector<acv_XY> &signature)
{
    memset((void*)&(signature[0]),0,signature.size()*sizeof(signature[0]));

    int preIdx=-1;
    int _1stIdx=-1;
    for(acv_XY pt :contour)
    {
        
        float theta=acvFAtan2(pt.y,pt.x);//-pi ~pi
        //if(theta<0)theta+=2*M_PI;
        int idx=round(signature.size()*theta/(2*M_PI));
        if(idx<0)idx+=signature.size();
        //if(idx>=signature.size())idx-=signature.size();
        if(preIdx==-1)
        {
            _1stIdx=idx;
            preIdx=idx;
        }
        float R=hypot(pt.y,pt.x);
        if(signature[idx].x<R)
        {
            signature[idx].x=R;
            signature[idx].y=theta;
            interpolateSignData(signature,preIdx,idx);
        }
        preIdx=idx;
    }
    interpolateSignData(signature,_1stIdx,preIdx);
    return true;

}



bool acvContourCircleSignature
(acv_XY  center,std::vector<acv_XY> &contour,std::vector<acv_XY> &o_signature)
{
  if(contour.size()==0)return false;
  
  int preIdx=-1;
  int _1stIdx=-1;
  for( acv_XY copos: contour)
  {
    float diffY=copos.y-center.y;
    float diffX=copos.x-center.x;
    if(diffX!=diffX||diffY!=diffY)
    {
        continue;
    }
    float theta=acvFAtan2(diffY,diffX);//-pi ~pi
    //if(theta<0)theta+=2*M_PI;
    int idx=round(o_signature.size()*theta/(2*M_PI));
    if(idx<0)idx+=o_signature.size();
    //if(idx>=signature.size())idx-=signature.size();
    if(preIdx==-1)
    {
        _1stIdx=idx;
        preIdx=idx;
    }
    float R=hypot(diffX,diffY);
    if(o_signature[idx].x<R)
    {
        o_signature[idx].x=R;
        o_signature[idx].y=theta;
        interpolateSignData(o_signature,preIdx,idx);
    }
    preIdx=idx;
  }


  return true;
}

float SignatureMatchingErrorX(const acv_XY *signature, int offset,
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
        float error = signature[(signIdx)].x - tar_signature[i].x;
        errorSum += error * error;
    }
    signIdx -= arrsize;
    size = arrsize;
    for (; i < size; i += stride, signIdx += stride)
    {
        float error = signature[(signIdx)].x - tar_signature[i].x;
        errorSum += error * error;
    }
    return errorSum;
}

inline int valueWarping(int v,int ringSize)
{
  v%=ringSize;
  return (v<0)?v+ringSize:v;
}
inline float valueWarping_f(float fv,int ringSize)
{
  fv+=10*ringSize;
  return fmod(fv,ringSize);
}



int doPrint=0;
inline float signatureSampling(const acv_XY *signature,int signSize,float fidx)
{
  int idx1 = (int)fidx;
  
  float sub_idx=fidx-idx1;
  idx1=valueWarping(idx1,signSize);
  int idx2=valueWarping(idx1+1,signSize);
  float X1 = signature[idx1].x ;
  float X2 = signature[idx2].x ;
  return X1+(X2-X1)*sub_idx;
}


inline float signatureSampling2(const acv_XY *signature,int signSize,float fidx)
{
  float rad =fmod(2*M_PI* fidx/signSize,2*M_PI);
  
  int idx1 = (int)fidx;
  int idx2;
  float Y1 = signature[idx1].y;
  if(Y1<0)Y1+=2*M_PI;//0~2PI

  if(rad<Y1)
  {
    idx2=idx1;
    idx1--;
  }
  else
  {
    idx2=idx1+1;
  }
  

  idx1=valueWarping(idx1,signSize);
  idx2=valueWarping(idx2,signSize);
  Y1 = signature[idx1].y;
  float Y2=signature[idx2].y;
  if(Y2<Y1)
  {
    if(rad<M_PI)
    {
      Y1-=2*M_PI;
    }
    else
    {
      Y2+=2*M_PI;
    }
  }
  float sub_idx=(rad-Y1)/(Y2-Y1);

  float X1 = signature[idx1].x ;
  float X2 = signature[idx2].x ;
  return X1+(X2-X1)*sub_idx;
}

float SignatureMatchingError(const acv_XY *signature, float offset,
                             const acv_XY *tar_signature, int arrsize, int stride,bool reverse)//Single error result
{
  
    //doPrint = (offset>-0.2 && offset<0.2 );
      
    float errorSum = 0;
    if(offset<0)offset+=arrsize;
    float epsilon=0.01;
    for (float i=0; i < arrsize; i += stride)
    {
      float sigidx=(offset+i);
      if(reverse)sigidx=arrsize-sigidx;
      float x1=signatureSampling(signature,arrsize,sigidx) ;
      float x2=signatureSampling(tar_signature,arrsize,(float)i);

      float error1 = x1-x2 ;
      errorSum+=error1*error1;


      
        // int oid0 = valueWarping(offset+i-1,arrsize);
        // int oid1 = valueWarping(offset+i+0,arrsize);
        // int oid2 = valueWarping(offset+i+1,arrsize);
        // float error0 = signatureSampling(signature,arrsize,offset+i-1) - tar_signature[i].x;
        // float error2 = signatureSampling(signature,arrsize,offset+i+1) - tar_signature[i].x;
        // error0*=error0;
        // error2*=error2;
        // error0+=epsilon;
        // error2+=epsilon;
        // //Find the smallest error
        // if(error0<error1)
        // {
        //   errorSum += (error0<error2)?error0:error2;
        // }
        // else
        // {
        //   errorSum += (error1<error2)?error1:error2;
        // }
    }
    // printf("errorSum:%f arrsize:%d\n",errorSum,arrsize);
    return errorSum/arrsize;
}

float SignatureMatchingError(const std::vector<acv_XY> &signature, float offset,
                             const std::vector<acv_XY> &tar_signature, int stride)//Single error result
{
    return SignatureMatchingError(&(signature[0]), offset, &(tar_signature[0]), signature.size(), stride);
}




float SignatureMinMatching( std::vector<acv_XY> &signature,const std::vector<acv_XY> &tar_signature,
  float searchAngleOffset,float searchAngleRange,int facing,
  bool *ret_isInv, float *ret_angle)
{

    float sign_error=100000000;
    float AngleDiff = (facing>=0)?SignatureAngleMatching(signature, tar_signature,searchAngleOffset,searchAngleRange, &sign_error):0;
    float sign_error_rev=100000000;
    SignatureReverse(signature,signature);
    float AngleDiff_rev = (facing<=0)?SignatureAngleMatching(signature, tar_signature,searchAngleOffset+M_PI,searchAngleRange, &sign_error_rev):0;
    SignatureReverse(signature,signature);
    bool isInv=false;
    if(sign_error>sign_error_rev)
    {
        isInv=true;
        sign_error=sign_error_rev;
        AngleDiff=-AngleDiff_rev;
    }

    if(ret_isInv)*ret_isInv=isInv;
    if(ret_angle)*ret_angle=AngleDiff;

    return sign_error;
}





#include <float.h>
float SignareIdxOffsetMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature, int roughSearchSampleRate,
                             int startIdx,int stopIdx, float *min_error)
{
    if (roughSearchSampleRate < 1)
        return -1;
    int minErrOffset = 0;
    float minErr = FLT_MAX; 

    //Find the length between stopIdx and startIdx
    int idxL = stopIdx-startIdx+1;
    if(idxL<0)idxL+=tar_signature.size();
    //Find the Length that can be devided by roughSearchSampleRate
    int roughSearchCount = ( (idxL/roughSearchSampleRate) + ((idxL%roughSearchSampleRate!=0)?1:0) );

    for ( int i=0 ; i<roughSearchCount ; i++)
    {
        int idx=(startIdx+i*roughSearchSampleRate);
        float error = SignatureMatchingError(signature, (float)idx, tar_signature, roughSearchSampleRate);
        
        if (minErr > error)
        {
            minErr = error;
            minErrOffset = idx;
        }
    }

    minErr = FLT_MAX;
    int fineSreachRadious = roughSearchSampleRate;
    float searchCenter = minErrOffset;

    float error;

    float errSum=FLT_MAX;
    float f_minErrOffset = 0;
    for (float adv =-fineSreachRadious; adv < fineSreachRadious + 1; adv+=1)
    {
        error = SignatureMatchingError(signature, (float)searchCenter+adv, tar_signature, 1);

        if (errSum > error)
        {
            errSum = error;
            f_minErrOffset = searchCenter+adv;
        }
        
        // printf("offset:%f  error:%f\n",(searchCenter+adv),error);
    }


    // searchCenter=f_minErrOffset;
    // float offsetW=0;
    // f_minErrOffset=0;
    // for (float adv =-1.5; adv < 1.5+0.1; adv+=0.1)
    // {
    //     error = SignatureMatchingError(signature, (float)searchCenter+adv, tar_signature, 1);
    //     float W=1/(0.001+error);
    //     W=W*W*W*W;
    //     offsetW+=W;
    //     f_minErrOffset+=(searchCenter+adv)*W;

    //    // printf("offset:%f  error:%f  W:%f\n",(searchCenter+adv),error,W);
    // }

    // f_minErrOffset/=offsetW;




    if (f_minErrOffset < 0)
        f_minErrOffset += tar_signature.size();
    f_minErrOffset=fmod(f_minErrOffset,tar_signature.size());

    if (min_error)
        *min_error = errSum;
    return f_minErrOffset;
}

float SignatureAngleMatching(const std::vector<acv_XY> &signature,
                             const std::vector<acv_XY> &tar_signature,
                             float searchAngleOffset,float searchAngleMargin,
                             float *min_error)
{
    int roughSearchSampleRate=6;//magic number// signature.size() / 160;

    float error;

    //find offset in idx
    float cIdx=-searchAngleOffset*signature.size()/(2 * M_PI);
    cIdx=fmod(cIdx,signature.size());
    if(cIdx<0)cIdx+=signature.size();

    float idxRange=searchAngleMargin*signature.size()/(2 * M_PI);
    int startIdx=(int)floor(cIdx-idxRange);
    int stopIdx=(int)ceil(cIdx+idxRange);
    if(stopIdx-startIdx+1>signature.size())
    {
        startIdx=0;
        stopIdx=signature.size()-1;
    }

    while(startIdx<0)startIdx+=signature.size();
    startIdx%=signature.size();
    while(stopIdx<0)stopIdx+=signature.size();
    stopIdx%=signature.size();

    float matchingIdx = SignareIdxOffsetMatching(signature, tar_signature, roughSearchSampleRate,startIdx,stopIdx, &error);


    if(min_error)*min_error=error;
    //The offset apply to signature (array) means negative rotation.
    // f(x+5) means offset the graph negative (to left)
    float angle = -matchingIdx * 2 * M_PI / signature.size();
    angle += 2 * M_PI;
    if (angle < -M_PI)
        angle += 2 * M_PI;
    else if (angle > M_PI)
        angle -= 2 * M_PI;
    return angle;
}


void SignatureSoften(std::vector<acv_XY> &signature,int windowR)
{
  std::vector<acv_XY> buffer(signature.size());


  for(int i=0;i<signature.size();i++)
  {
    buffer[i]=signature[i];
  }

  SignatureSoften(buffer,signature,windowR);
}


void SignatureSharpen(std::vector<acv_XY> &signature,int windowR,float alpha)
{
  std::vector<acv_XY> soften_buffer(signature.size());


  SignatureSoften(signature,soften_buffer,windowR);
  
  for(int i=0;i<signature.size();i++)
  {
    signature[i].x-=alpha*soften_buffer[i].x;
  }
}

void SignatureSoften(std::vector<acv_XY> &signature,std::vector<acv_XY> &output,int windowR)
{
  output.resize(signature.size());

  
  for(int i=0;i<signature.size();i++)
  {
    float value=0;
    for(int j=-windowR;j<windowR+1;j++)
    {
      value+=signature[valueWarping(i+j,signature.size())].x;
    }
    value/=(2*windowR)+1;
    output[i].x=value/((2*windowR)+1);
    output[i].y=signature[i].y;
  }

}

void SignatureReverse_SWAP(std::vector<acv_XY> &sign)
{
    acv_XY* arr=&(sign[0]);
    int iterL=(sign.size()+1)/2; //3-4(iter 2),5-6 (iter 3)
    int inv_i;
    for(int i=1,inv_i=sign.size()-1;i<iterL;i++,inv_i--)
    {
      acv_XY tmp = arr[i];
      arr[i]=arr[inv_i];
      arr[inv_i]=tmp;
    }
}
void SignatureReverse_ASSIGN(std::vector<acv_XY> &dst,std::vector<acv_XY> &src)
{
    dst.resize(src.size());
    acv_XY* arr_dst=&(dst[0]);
    acv_XY* arr_src=&(src[0]);
    int inv_i;
    arr_dst[0]=arr_src[0];
    for(int i=1,inv_i=src.size()-1;i<src.size();i++,inv_i--)
    {
      arr_dst[i]=arr_src[inv_i];
    }
}
void SignatureReverse(std::vector<acv_XY> &dst, std::vector<acv_XY> &src)
{
  if(&dst == &src){
    SignatureReverse_SWAP(dst);
  }
  else{
    SignatureReverse_ASSIGN(dst,src);
  }
}
