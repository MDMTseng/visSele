#ifndef ACV_IMG_BASIC_TOOL_H
#define ACV_IMG_BASIC_TOOL_H
#include "acvImage.hpp"

#ifdef _MSC_VER
#pragma pack(push,1)
typedef struct acv_tagBITMAPFILEHEADER
{
	int16_t bfType;  //specifies the file type
	int32_t bfSize;  //specifies the size in bytes of the bitmap file
	int32_t bfReserved;  //reserved; must be 0
	int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
} acv_BITMAPFILEHEADER;
typedef struct acv_tagBITMAPINFOHEADER
{
	int32_t biSize;  //specifies the number of bytes required by the struct
	int32_t biWidth;  //specifies width in pixels
	int32_t biHeight;  //species height in pixels
	int16_t biPlanes; //specifies the number of color planes, must be 1
	int16_t biBitCount; //specifies the number of bit per pixel
	int32_t biCompression;//spcifies the type of compression
	int32_t biSizeImage;  //size of image in bytes
	int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
	int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
	int32_t biClrUsed;  //number of colors used by th ebitmap
	int32_t biClrImportant;  //number of colors that are important
} acv_BITMAPINFOHEADER;
#pragma pack(pop)
#else

#pragma pack(push,1)
typedef struct __attribute__((__packed__)) acv_tagBITMAPFILEHEADER
{
	int16_t bfType;  //specifies the file type
	int32_t bfSize;  //specifies the size in bytes of the bitmap file
	int32_t bfReserved;  //reserved; must be 0
	int32_t bOffBits;  //species the offset in bytes from the bitmapfileheader to the bitmap bits
} acv_BITMAPFILEHEADER;
typedef struct __attribute__((__packed__)) acv_tagBITMAPINFOHEADER
{
	int32_t biSize;  //specifies the number of bytes required by the struct
	int32_t biWidth;  //specifies width in pixels
	int32_t biHeight;  //species height in pixels
	int16_t biPlanes; //specifies the number of color planes, must be 1
	int16_t biBitCount; //specifies the number of bit per pixel
	int32_t biCompression;//spcifies the type of compression
	int32_t biSizeImage;  //size of image in bytes
	int32_t biXPelsPerMeter;  //number of pixels per meter in x axis
	int32_t biYPelsPerMeter;  //number of pixels per meter in y axis
	int32_t biClrUsed;  //number of colors used by th ebitmap
	int32_t biClrImportant;  //number of colors that are important
} acv_BITMAPINFOHEADER;
#pragma pack(pop)
#endif


typedef struct acv_XY
{
    float X,Y;
} acv_XY;


typedef struct acv_Circle
{
    acv_XY circumcenter;
	float radius;
} acv_Circle;

typedef struct acv_Line {
  acv_XY line_vec;
  acv_XY line_anchor;
} acv_Line;


typedef struct acv_CircleFit
{
  acv_Circle circle;
  int matching_pts;
  float s;//sigma
}acv_CircleFit;

typedef struct acv_LineFit
{
  acv_Line line;
  int matching_pts;

  acv_XY end_pos;
  acv_XY end_neg;
  float s;//sigma
}acv_LineFit;





class acvCalibMapUtil
{
    public:

    static float NumRatio(float a,float b,float ratio)
    {
        return a+ratio*(b-a);
    }

    static int sample_vec(float* map,int width,int height,float mapfX,float mapfY,float sampleXY[2])
    {
        if(mapfX>width-2)return -1;
        if(mapfY>height-2)return -1;
        if(mapfX<0)return -1;
        if(mapfY<0)return -1;
        sampleXY[0]=sampleXY[1]=0;
        
        int mapX=mapfX;
        int mapY=mapfY;

        float ratioX=mapfX-mapX;
        float ratioY=mapfY-mapY;

        int idxLT = (mapY)*width+mapX;
        int idxRT = (mapY)*width+mapX+1;
        int idxLB = (mapY+1)*width+mapX;
        int idxRB = (mapY+1)*width+mapX+1;
        /*
        LT  RT
        
        LB  RB
        */
        
        float v1[2]={NumRatio(map[idxLT*2  ],map[idxRT*2  ],ratioX),
                    NumRatio(map[idxLT*2+1],map[idxRT*2+1],ratioX)};
        float v2[2]={NumRatio(map[idxLB*2  ],map[idxRB*2  ],ratioX),
                    NumRatio(map[idxLB*2+1],map[idxRB*2+1],ratioX)};

        sampleXY[0]=NumRatio(v1[0],v2[0],ratioY);
        sampleXY[1]=NumRatio(v1[1],v2[1],ratioY);

        return 0;
    }

    static int map_vec(float* map,int width,int height,float mapfX,float mapfY,float xyVec[4])
    {
        if(mapfX>width-2)mapfX=width-2;
        if(mapfY>height-2)mapfY=height-2;
        if(mapfX<0)mapfX=0;
        if(mapfY<0)mapfY=0;
        
        int mapX=mapfX;
        int mapY=mapfY;

        float ratioX=mapfX-mapX;
        float ratioY=mapfY-mapY;

        int idxLT = (mapY)*width+mapX;
        int idxRT = (mapY)*width+mapX+1;
        int idxLB = (mapY+1)*width+mapX;
        int idxRB = (mapY+1)*width+mapX+1;
        /*
        
        LT  RT
        
        LB  RB
        */
        
        float v1[2]={NumRatio(
                        map[idxRT*2]-map[idxLT*2],
                        map[idxRB*2]-map[idxLB*2],
                        ratioX),
                    NumRatio(
                        map[idxRT*2+1]-map[idxLT*2+1],
                        map[idxRB*2+1]-map[idxLB*2+1],
                        ratioX)};
        float v2[2]={NumRatio(
                        map[idxLB*2]-map[idxLT*2],
                        map[idxRB*2]-map[idxRT*2],
                        ratioY),
                    NumRatio(
                        map[idxLB*2+1]-map[idxLT*2+1],
                        map[idxRB*2+1]-map[idxRT*2+1],
                        ratioY)};
                        

        // LOGI("%f  %f",v1[0],v1[1]);
        // LOGI("%f  %f",v2[0],v2[1]);
        float det = v1[0]*v2[1]-v1[1]*v2[0];
        float invMat[4]={
            v2[1]/det,-v1[1]/det,
            -v2[0]/det, v1[0]/det
        };
        memcpy(xyVec,invMat,sizeof(invMat));
        return 0;
    }

    static float locateMapPosition(float* map,int width,int height,float tar_x,float tar_y,float mapSeed_ret[2],float maxError=0.01,int iterC=5)
    {

        float sampleXY[2];
        float x=tar_x, y=tar_y;
        float error = hypot(sampleXY[0]-x,sampleXY[1]-y);
        for(int i=0;i<iterC;i++)
        {


            sample_vec(map,width,height,mapSeed_ret[0],mapSeed_ret[1],sampleXY);
            error = hypot(sampleXY[0]-x,sampleXY[1]-y);

            if(error<maxError)return  error;
            if(error!=error)
            {
                mapSeed_ret[0]=
                mapSeed_ret[1]=NAN;
                return NAN;
            }
            // LOGI("mapSeed_ret:%f  %f",mapSeed_ret[0],mapSeed_ret[1]);
            // LOGI("xy:%f  %f",x,y);
            // LOGI("sample_vec:%f  %f",sampleXY[0],sampleXY[1]);
            float diffXY[2]={
                x-sampleXY[0],
                y-sampleXY[1]
            };

            float xyVec[4];
            
            int ret= map_vec(map, width, height,mapSeed_ret[0],mapSeed_ret[1], xyVec);
            // LOGI("%f  %f",xyVec[0],xyVec[1]);
            // LOGI("%f  %f",xyVec[2],xyVec[3]);

            
            mapSeed_ret[0]+=diffXY[0]*xyVec[0]+diffXY[1]*xyVec[2];
            mapSeed_ret[1]+=diffXY[0]*xyVec[1]+diffXY[1]*xyVec[3];
        }

        sample_vec(map,width,height,mapSeed_ret[0],mapSeed_ret[1],sampleXY);

        error = hypot(sampleXY[0]-x,sampleXY[1]-y);
        return error;
    }

};
class acvCalibMap
{
    float* fwdMap;
    //the xy idx is in corrected mapping
    //fwdMap(x,y) is the xy coord from original image
    int fw,fh;
    float* invMap;
    int iw,ih;

    int downScale=1;
    public:
    
    acvCalibMap(double *MX_data, double *MY_data, int fw_,int fh_)
    {
        
        fw=fw_;
        fh=fh_;
        invMap=NULL;
        int pixCount = fw*fh;
        fwdMap=new float[fw*fh*2];
        for(int i=0;i<pixCount;i++)
        {
            fwdMap[i*2]=(float)MX_data[i];
            fwdMap[i*2+1]=(float)MY_data[i];
        }

        //SET(fwdMap, fw_, fh_, iw_,ih_);
    }
    void generateInvMap(int iw_,int ih_)
    {
        
        iw=iw_;
        ih=ih_;
        invMap=new float[iw*ih*2];

        int missCount=0;
        float errSum=0;
    
        float mapSeed_row[2]={fw/2.0f,fh/2.0f};
        for(int i=0;i<ih;i++)
        {
            float mapSeed_ret[2]={mapSeed_row[0],mapSeed_row[1]};   
            for(int j=0;j<iw;j++)
            {
                int idx = i*iw+j;
                if(mapSeed_ret[0]!=mapSeed_ret[0])//NAN
                {
                    mapSeed_ret[0]=fw/2.0f;
                    mapSeed_ret[1]=fh/2.0f;
                }
                float tarLoc[2]={(float)j,(float)i};
                float error = acvCalibMapUtil::locateMapPosition(fwdMap,fw,fh,tarLoc[0],tarLoc[1],mapSeed_ret);
                
                if(j==0)
                {
                    mapSeed_row[0]=mapSeed_ret[0];
                    mapSeed_row[1]=mapSeed_ret[1];
                }
                if(error>0.02 || error!=error){
                    invMap[idx*2+1]=
                    invMap[idx*2]=NAN;
                    missCount++;
                }else{
                    errSum+=error;
                    invMap[idx*2]=mapSeed_ret[0];
                    invMap[idx*2+1]=mapSeed_ret[1];
                }
            }
        }
        // LOGE("missCount:%d rate:%f%%",missCount,100*(float)missCount/(iw*ih));
        // LOGE("errorSum:%f",errSum/(iw*ih-missCount));

    }

    int fwdMapDownScale(int dscale_idx)
    {
        if(dscale_idx<0)return -1;
        if(dscale_idx==0)return 0;

        int dscale=1<<dscale_idx;
        int dfw=fw/dscale;
        int dfh=fh/dscale;
        float *dfwdMap=new float[dfw*dfh*2];
        for(int di=0;di<dfh;di++)
        {
            int i = di*dscale;
            for(int dj=0;dj<dfw;dj++)
            {
                int j = dj*dscale;
                int didx=di*dfw+dj;
                int idx=i*fw+j;
                dfwdMap[didx*2]=fwdMap[idx*2];
                dfwdMap[didx*2+1]=fwdMap[idx*2+1];
            }
        }
        
        delete fwdMap;
        fwdMap=dfwdMap;
        fw=dfw;
        fh=dfh;
        downScale*=dscale;

        deleteInvMap();
        return 0;
    }
    
    void deleteInvMap()
    {
        if(invMap)
        {
            delete invMap;
            invMap=NULL;
        }
    }
    ~acvCalibMap()
    {
        deleteInvMap();
        delete fwdMap;
    }

    int i2c(float coord[2],bool useInvMap=true)
    {
        int ret;
        if(invMap && useInvMap)
        {
             ret = acvCalibMapUtil::sample_vec(invMap,iw,ih,coord[0],coord[1],coord);
        }
        else
        {
            ret=0;
            float error = acvCalibMapUtil::locateMapPosition(fwdMap,fw,fh,coord[0],coord[1],coord);
            if(error>0.01 || error!=error)
                ret=-1;
        }
        if(ret == 0)
        {
            coord[0]*=downScale;
            coord[1]*=downScale;
        }
        else
        {
            coord[0]=
            coord[1]=NAN;
        }
        return ret;
    }
    int c2i(float coord[2])
    {
        
        coord[0]/=downScale;
        coord[1]/=downScale;
        return acvCalibMapUtil::sample_vec(fwdMap,fw,fh,coord[0],coord[1],coord);
    }

};


typedef struct acvRadialDistortionParam{
    acv_XY calibrationCenter;
    double RNormalFactor;
    double K0,K1,K2;
	//r = r_image/RNormalFactor
    //C1 = K1/K0
    //C2 = K2/K0
	//r"=r'/K0
    //Forward: r' = r*(K0+K1*r^2+K2*r^4)
    //         r"=r'/K0=r*(1+C1*r^2 + C2*r^4)
    //Backward:r  =r"(1-C1*r"^2 + (3*C1^2-C2)*r"^4)
    //r/r'=r*K0/r"
    double ppb2b;//pixels per block 2 block	
    double mmpb2b;//the distance between block and block
    acvCalibMap* map;
}acvRadialDistortionParam;


acv_XY acvVecRadialDistortionRemove(acv_XY distortedVec,acvRadialDistortionParam param);//Forward
acv_XY acvVecRadialDistortionApply(acv_XY Vec,acvRadialDistortionParam param);//Backward

void acvThreshold(acvImage *Pic,BYTE Var);
void acvThreshold(acvImage *Pic,BYTE Var,int channel);
void acvThreshold_single(acvImage *Pic,BYTE Var,int channel);
void acvContrast(acvImage *dst, acvImage *src, int offset, int shift,int channel);
void acvDeleteFrame(acvImage *Pic,int width);
void acvDeleteFrame(acvImage *Pic);
void acvClear(acvImage *Pic,BYTE Var);
void acvClear(acvImage *Pic,int channel,BYTE Var);
void acvTurn(acvImage *Pic);
double acvFAtan2( double y, double x );
double acvFAtan(double x);
void acvFullB2W(acvImage *OriPic,acvImage *OutPic);
void acvClone_B2Gray(acvImage *OriPic,acvImage *OutPic);
void acvCloneImage(acvImage *OriPic,acvImage *OutPic,int Mode);
void acvCloneImage_single(acvImage *OriPic, int layer_ori, acvImage *OutPic, int layer_out);

char *PrintHexArr_buff(char *strBuff,int strBuffL,char *data, int dataL);
char *PrintHexArr(char *data, int dataL);
unsigned char *acvLoadBitmapFile(const char *filename, acv_BITMAPINFOHEADER *bitmapInfoHeader);
int acvLoadBitmapFile(acvImage *img,const char *filename);
int acvSaveBitmapFile(const char *filename,unsigned char* pixData,int width,int height);
int acvSaveBitmapFile(const char *filename,acvImage *img);
#define div_round(dividend, divisor) (((int)(dividend) + ((int)(divisor) >>1)) / (int)(divisor))
void acvImageAdd(acvImage *src,int num);
#define DoubleRoundInt(Num) ((int)round(Num))
void acvInnerFramePixCopy(acvImage *Pic,int FrameX);
acv_XY acvIntersectPoint(acv_XY p1,acv_XY p2,acv_XY p3,acv_XY p4);
acv_XY acvCircumcenter(acv_XY p1,acv_XY p2,acv_XY p3);
float acv2DCrossProduct(acv_XY v1,acv_XY v2);
float acv2DDotProduct(acv_XY v1,acv_XY v2);
float acvVectorOrder(acv_XY p1,acv_XY p2,acv_XY p3);
float acvDistance(acv_XY p1,acv_XY p2);
acv_XY acvVecNormal(acv_XY vec);
acv_XY acvVecNormalize(acv_XY vec);
acv_XY acvVecInterp(acv_XY vec1,acv_XY vec2,float alpha);
acv_XY acvVecAdd(acv_XY vec1,acv_XY vec2);
acv_XY acvVecSub(acv_XY vec1,acv_XY vec2);
acv_XY acvVecMult(acv_XY vec1,float mult);
acv_XY acvRotation(float sine,float cosine,float flip_f,acv_XY input);
acv_XY acvRotation(float sine,float cosine,acv_XY input);
acv_XY acvRotation(float angle,acv_XY input);


acv_XY acvClosestPointOnLine(acv_XY point, acv_Line line);
float acvDistance_Signed(acv_Line line, acv_XY point);
float acvDistance(acv_Line line, acv_XY point);
float acvLineAngle(acv_Line line1,acv_Line line2);
float acvVectorAngle(acv_XY v1,acv_XY v2);
bool acvFitLine(const acv_XY *pts, int ptsL,acv_Line *line, float *ret_sigma);
bool acvFitLine(const acv_XY *pts, const float *ptsw, int ptsL,acv_Line *line, float *ret_sigma);
bool acvFitLine(const void *pts_struct,int pts_step, const void *ptsw_struct,int ptsw_step, int ptsL,acv_Line *line, float *ret_sigma);
#endif
