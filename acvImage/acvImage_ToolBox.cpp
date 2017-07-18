
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

double GetRMSE(acvImage *Pic1,acvImage *Pic2)
{
    double MSE=0;
    int    i,j,Tmp;
    for(i=0;i<Pic1->GetHeight();i++)for(j=0;j<Pic2->GetWidth();j++)
    {
          Tmp=Pic1->CVector[i][3*j]-Pic2->CVector[i][3*j];
          MSE+=Tmp*Tmp;
    }
    return sqrt(MSE/Pic1->GetHeight()/Pic2->GetWidth());
}



//const char SearchTable[SearchWay]={0,7,1,6,2,5,3};


/*0 1 2
  7 X 3
  6 5 4
*/
const int SearchTable[][2]={{0,0},{7,1},{1,7},{6,2},{2,6},{5,3},{3,5}};
const int WalkV[8][2]={{-1,-1},{-1,0},{-1,1},{0,1},{1,1},{1,0},{1,-1},{0,-1}};
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


        for(j=0;j<Size;j++)
        {
                Sum+=(InRingArray[j+1]+InRingArray[RingSize-j-1]);
        }
        OutRingArray[0]=Sum/(Size*2+1);
        for(j=1;j<=Size;j++)
        {
                Sum-= InRingArray[RingSize+j-Size-1];
                Sum+= InRingArray[j+Size];

                OutRingArray[j]=Sum/(Size*2+1);
        }


        for(j=Size+1;j<RingSize-Size;j++)
        {

                Sum-= InRingArray[j-Size-1];
                Sum+= InRingArray[j+Size];

                OutRingArray[j]=Sum/(Size*2+1);
                //OutRingArray[j]=0;
        }
        for(j=RingSize-Size;j<RingSize;j++)
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

        for(j=1;j<RingSize;j++)
        {
                OutRingArray[j]=InRingArray[j]-InRingArray[j-1];
        }

}

void acvRingDataDeZero(int * OutRingArray,int * InRingArray,int RingSize)
{
        int ZeroStart=0,i,j;

        int DataLPos=RingSize,DataRPos=0;
        int ZeroL=0,NewL=0;
        if(!InRingArray[0]||!InRingArray[RingSize-1])
        {
                for(i=0;i<RingSize;i++)if(InRingArray[i])
                {
                        DataRPos=i;
                        break;
                }
                if(i==RingSize-1)return;
                for(i=RingSize-1;i>=0;i--)if(InRingArray[i])
                {
                        DataLPos=i;
                        break;
                }
                ZeroL=RingSize-DataLPos+DataRPos+1;
                for(j=0,i=DataLPos-1;j<ZeroL;i++,j++)
                {
                        if(i==RingSize)i=0;
                        OutRingArray[i]=
                        InRingArray[DataLPos]+
                        j*(InRingArray[DataRPos]-InRingArray[DataLPos])/ZeroL;
                }

        }
        NewL=DataLPos;
        for(i=DataRPos;i<NewL;i++)
        {
                if(ZeroStart)
                {
                        ZeroL++;
                        if(InRingArray[i])
                        {
                             ZeroStart=0;
                             for(j=1;j<ZeroL;j++)
                             {
                                   OutRingArray[DataLPos+j]=

                                   InRingArray[DataLPos]+
                                j*(InRingArray[i]-InRingArray[DataLPos])/ZeroL;

                             }

                        }
                }
                else
                {
                        if(!InRingArray[i])
                        {
                             ZeroL=1;
                             ZeroStart=1;
                             DataLPos=i-1;
                        }
                        else
                             OutRingArray[i]=InRingArray[i];
                }
                //OutArrayData[i]=InArrayData[i];
        }
}
#define MaxContinueZero 15
void acvRingDataDeZero2(int * OutRingArray,int * InRingArray,int RingSize)
{
        int ZeroStart=-1,i,j;


        for(i=0;i<RingSize;i++)
        {
                OutRingArray[i]=InRingArray[i];
                if(InRingArray[i])
                {
                        if(ZeroStart!=-1)for(j=ZeroStart;j<i;j++)
                        {
                                OutRingArray[j]=InRingArray[i];
                                ZeroStart=-1;
                        }
                        continue;
                }
                else if(ZeroStart==-1)
                        ZeroStart=i;
                else if(i-ZeroStart>MaxContinueZero)
                        ZeroStart=i-MaxContinueZero;


        }
        if(ZeroStart!=-1&&ZeroStart!=0)for(i=ZeroStart;i<RingSize;i++)
        {
                OutRingArray[i]=InRingArray[0];
        }

}

int RingDataTotalRMSE(int *DataArray,int * StadardDataArray,int RingSize)
{
        int i;
        double MSE=0;


        for(i=0;i<RingSize;i++)
        {
                MSE+=(DataArray[i]-StadardDataArray[i])
                        *(DataArray[i]-StadardDataArray[i]);
        }
        return 1000*sqrt(MSE/RingSize);
}

int RingDataRMSE(int *DataArray,int * StadardDataArray,int RingSize,int CompareChannel)
{
        int i;
        double MSE=0;

        DataArray+=CompareChannel;
        StadardDataArray+=CompareChannel;
        RingSize/=3;
        for(i=0;i<RingSize;i++)
        {
                MSE+=(*DataArray-*StadardDataArray)
                        *(*DataArray-*StadardDataArray);
                DataArray+=3;
                StadardDataArray+=3;

        }
        return 1000*sqrt(MSE/RingSize);
}

#define HistoGenInfoNum (256*3/HistoGenDataCompress)
#define HistoGenNormalizeNum 10000   //the sum of Histo is NormalizeNum

#define HistoGenDataCompress  15
double RingDataTotalBhattacharyya(int *DataArray,int * StadardDataArray,int RingSize)
{
        int i,Tmp;
        double Coef=0;

        for(i=0;i<RingSize;i++)
        {
                Tmp=(*DataArray++)*(*StadardDataArray++);
                if(Tmp>0)Coef+=sqrt((double)Tmp);

        }
        return Coef/3/HistoGenNormalizeNum;
}

double RingDataBhattacharyya(int *DataArray,int * StadardDataArray,int RingSize,int CompareChannel)
{
        int i,Tmp;
        double Coef=0;

        DataArray+=CompareChannel;
        StadardDataArray+=CompareChannel;
        RingSize/=3;
        for(i=0;i<RingSize;i++)
        {
                Tmp=(*DataArray)*(*StadardDataArray);
                if(Tmp>0)Coef+=sqrt((double)Tmp);
                DataArray+=3;
                StadardDataArray+=3;

        }

        return Coef/HistoGenNormalizeNum;
}





void acvGetHisto(acvImage *Pic,int *Histo,int ROIX1,int ROIY1,int ROIX2,int ROIY2)
{
        int i,j;
        BYTE  *BMPLine;
        if(ROIX1>ROIX2)
        {
                i=ROIX2;
                ROIX2=ROIX1;
                ROIX1=i;
        }
        if(ROIY1>ROIY2)
        {
                i=ROIY2;
                ROIY2=ROIY1;
                ROIY1=i;
        }


        if(ROIX1>=Pic->GetWidth())
                goto Exit;
        else
        {
                if(ROIX2>=Pic->GetWidth())
                        ROIX2=Pic->GetWidth();
                else if(ROIX2<0)   goto Exit;

                if(ROIX1<0) ROIX1=0;
        }
        if(ROIY1>=Pic->GetHeight())
                goto Exit;
        else
        {
                if(ROIY2>=Pic->GetHeight())
                        ROIY2=Pic->GetHeight();
                else if(ROIY2<0)   goto Exit;

                if(ROIY1<0) ROIY1=0;
        }












        for(i=0;i<HistoGenInfoNum;i++)Histo[i]=0;

        for(i=ROIY1;i<ROIY2;i++)
        {
                BMPLine=Pic->CVector[i];
                for(j=ROIX1;j<ROIX2;j++)
                {
                        Histo[(BMPLine[j*3+0]/HistoGenDataCompress)*3+0]++;
                        Histo[(BMPLine[j*3+1]/HistoGenDataCompress)*3+1]++;
                        Histo[(BMPLine[j*3+2]/HistoGenDataCompress)*3+2]++;



                }
        }
        ROIX1=(ROIY2-ROIY1)*(ROIX2-ROIX1);

        if(ROIX1)for(i=0;i<HistoGenInfoNum;i++)
                Histo[i]=(int)((double)Histo[i]*HistoGenNormalizeNum/ROIX1);
        Exit:;
}





int RingDataGetMean(int * RingArray,int RingSize)
{
        int j,Sum=0;

        for(j=0;j<RingSize;j++)Sum+=RingArray[j];

        return Sum/RingSize;

}
#define SFPMeanFilterSize 8
#define MaxDiffClusterMeanToPalm 4/7
#define MaxDiffClusterMeanToPalmOpen 45
#define MaxDiffClusterMeanToPalmClse 70


double GetBarCodeRMSE(acvImage *BarCodeRule,acvImage *CheckPic)
{
        double MSE=0;
        int    i,j,Tmp,RealPixel=0;
        BYTE  *BarCodeRuleLine,*CheckPicLine;
        for(i=0;i<BarCodeRule->GetWidth();i++)
        {
                BarCodeRuleLine=BarCodeRule->CVector[i];
                //if(*BarCodeRuleLine==125)continue;
                CheckPicLine=CheckPic->CVector[i];

                for(j=0;j<BarCodeRule->GetHeight();j++)
                {
                        if(*BarCodeRuleLine==125)continue;
                        Tmp=*CheckPicLine-*BarCodeRuleLine;
                        CheckPicLine+=3;
                        BarCodeRuleLine+=3;
                        RealPixel++;
                        MSE+=Tmp*Tmp;
                }
        }
        return sqrt(MSE/RealPixel);
}





#define FindParallelogramInfoColumn             SignInfoAngleNum

#define FindPalmFingerColumn                    3


#define FindPalmFingerLength(X)                 ((X)*FindPalmFingerColumn+0)
#define FindPalmFingerAngleS(X)                 ((X)*FindPalmFingerColumn+1)
#define FindPalmFingerAngleE(X)                 ((X)*FindPalmFingerColumn+2)



#define FindPalmFingerLength1                   0
#define FindPalmFingerAngleS1                   1
#define FindPalmFingerAngleE1                   2

#define FindPalmFingerLength2                   3
#define FindPalmFingerAngleS2                   4
#define FindPalmFingerAngleE2                   5

#define FindPalmFingerLength3                   6
#define FindPalmFingerAngleS3                   7
#define FindPalmFingerAngleE3                   8

#define FindPalmFingerLength4                   9
#define FindPalmFingerAngleS4                   10
#define FindPalmFingerAngleE4                   11

#define FindPalmFingerLength5                   12
#define FindPalmFingerAngleS5                   13
#define FindPalmFingerAngleE5                   14




#define SFPHFMeanFilterSize                  12
#define SFPHFAngleWidth                     110

void acvSignatureFindPalmFinger(int* MeanSign,int* Signature,int AngleCentral,int* FingerNum)
{
        int i,VertexNum=0,State,*MeanData,*SignData,AngleS,AngleE,&Mean=AngleCentral;
        int Record[FindPalmFingerColumn*5];

        Record[FindPalmFingerLength1]=
        Record[FindPalmFingerLength2]=
        Record[FindPalmFingerLength3]=
        Record[FindPalmFingerLength4]=
        Record[FindPalmFingerLength5]=0;


        //if(AngleCentral<0)return -1;
        if(AngleCentral<0)
        {
                *FingerNum=-1;
                goto Exit;
        }
        SignData=Signature;
        MeanData=MeanSign;
        /*AngleE=State+(SignInfoAngleNum>>2);
        AngleS=State-(SignInfoAngleNum>>2);    */
        if(AngleCentral==-1)
        {
                AngleE=AngleCentral+SignInfoAngleNum/2;
                AngleS=AngleCentral-SignInfoAngleNum/2;
        }
        else
        {
                AngleE=AngleCentral+SignInfoAngleNum*SFPHFAngleWidth/360;
                AngleS=AngleCentral-SignInfoAngleNum*SFPHFAngleWidth/360;
        }
        if(AngleE>=SignInfoAngleNum)AngleE-=SignInfoAngleNum;
        else if(AngleS<0)AngleS+=SignInfoAngleNum;
        Mean=0;

        if(AngleE>AngleS)
        {
                for(i=AngleS;i<AngleE;i++)Mean+=SignData[i];
                Mean/=SignInfoAngleNum*SFPHFAngleWidth/180;        //2/360==1/180
        }
        else
        {
                for(i=AngleS;i<SignInfoAngleNum;i++)Mean+=SignData[i];
                for(i=0;i<AngleE;i++)Mean+=SignData[i];
                Mean/=SignInfoAngleNum*SFPHFAngleWidth/180;

        }
        Mean=Mean*60/50;
        if(AngleE<AngleS)
                for(i=AngleE;i<AngleS;i++) SignData[i]=Mean;
        else
        {
                for(i=AngleE;i<SignInfoAngleNum;i++)SignData[i]=Mean;
                for(i=0;i<AngleS;i++)SignData[i]=Mean;
        }
        Mean=0;
        State=false;

        acvRingDataMeanFilter(MeanData,SignData,SignInfoAngleNum,30);



        if(AngleE>AngleS)
        {
                for(i=AngleS;i<AngleE;i++)
                {
                        if(SignData[i]>MeanData[i]*60/50)
                        {
                                if(!State)
                                {
                                        VertexNum++;
                                        if(VertexNum>5)
                                        {
                                                *FingerNum=-1;
                                                goto Exit;
                                        }

                                        Record[FindPalmFingerAngleE(VertexNum-1)]=0;
                                        State=true;
                                }
                                else if(Record[FindPalmFingerLength(VertexNum-1)]<SignData[i])
                                {
                                        Record[FindPalmFingerLength(VertexNum-1)]=SignData[i];
                                        Record[FindPalmFingerAngleS(VertexNum-1)]=i;
                                        Record[FindPalmFingerAngleE(VertexNum-1)]++;

                                }
                        }
                        else if(SignData[i]<MeanData[i]*43/50)
                        {
                                if(State)
                                        State=false;
                        }

                }
        }
        else
        {
                for(i=AngleS;i<SignInfoAngleNum;i++)
                {
                        if(SignData[i]>MeanData[i]*60/50)
                        {
                                if(!State)
                                {
                                        VertexNum++;
                                        if(VertexNum>5)
                                        {
                                                *FingerNum=-1;
                                                goto Exit;
                                        }

                                        Record[FindPalmFingerAngleE(VertexNum-1)]=0;
                                        State=true;
                                }
                                else if(Record[FindPalmFingerLength(VertexNum-1)]<SignData[i])
                                {
                                        Record[FindPalmFingerLength(VertexNum-1)]=SignData[i];
                                        Record[FindPalmFingerAngleS(VertexNum-1)]=i;
                                        Record[FindPalmFingerAngleE(VertexNum-1)]++;

                                }
                        }
                        else if(SignData[i]<MeanData[i]*43/50)
                        {
                                if(State)State=false;
                        }

                }
                for(i=0;i<AngleE;i++)
                {
                        if(SignData[i]>MeanData[i]*60/50)
                        {
                                if(!State)
                                {
                                        VertexNum++;
                                        if(VertexNum>5)
                                        {
                                                *FingerNum=-1;
                                                goto Exit;
                                        }
                                        Record[FindPalmFingerAngleE(VertexNum-1)]=0;
                                        State=true;
                                }
                                else if(Record[FindPalmFingerLength(VertexNum-1)]<SignData[i])
                                {
                                        Record[FindPalmFingerLength(VertexNum-1)]=SignData[i];
                                        Record[FindPalmFingerAngleS(VertexNum-1)]=i;
                                        Record[FindPalmFingerAngleE(VertexNum-1)]++;

                                }
                        }
                        else if(SignData[i]<MeanData[i]*43/50)
                        {
                                if(State)
                                        State=false;
                        }

                }


        }
        SignData[AngleS]=SignData[AngleE]=0;
        for(i=0;i<5;i++)
        {
                MeanData[i*3]=Record[i*3];
                if(!Record[i*3])break;
                MeanData[1+i*3]=Record[1+i*3];
                MeanData[2+i*3]=Record[2+i*3];



        }


        *FingerNum=VertexNum;
        Exit:;
        //return VertexNum;

}





const int ContourWalkV[8][2]={{-1,-1},{-1,0},{-1,1},{0,1},{1,1},{1,0},{1,-1},{0,-1}};

/*0 1 2
  7 X 3
  6 5 4
*/
void acvContourSignature(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature,int InitDir)
{
        int NowPos[2]={StartPos[0],StartPos[1]};

        int NowWalkDir=InitDir,Predir=6;//CounterClockWise
        int *WalkDirV;
        int Angle;
        int Dx,Dy;
        BYTE PointSymbol;
        BYTE **CVector=LabeledPic->CVector;
        int Distance;
        int OriginVar;

        /*
        NowWalkDir=5;
        while(CVector[NowPos[1]][(NowPos[0]+1)*3]-
              CVector[NowPos[1]][(NowPos[0]  )*3]<=0) //Black(+0)->White (+1)
        {
                NowPos[0]++;
                //CVector[ NowPos[1]][(NowPos[0])*3+1]=255;
        }
        Signature[0]=SignInfoDistanceMuti*(NowPos[0]-Centroid[0]);  */

        OriginVar=CVector[NowPos[1]][NowPos[0]*3];
        CVector[NowPos[1]][NowPos[0]*3]=254;//StartSymbol
        do
        {
                NowWalkDir+=2;
                if(NowWalkDir>7)NowWalkDir-=8;

                PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                //Predir=NowWalkDir;
                while(PointSymbol==255)
                {
                        Predir=NowWalkDir;
                        if(!NowWalkDir)NowWalkDir=7;
                        else           NowWalkDir--;
                        PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                }
                //NowWalkDir=SearchDir;
                /*
                CVector[ NowPos[1]+ContourWalkV[Predir][0]
                ][(NowPos[0]+ContourWalkV[Predir][1])*3+1]=255;
                CVector[ NowPos[1]+ContourWalkV[Predir][0]
                ][(NowPos[0]+ContourWalkV[Predir][1])*3+2]=0;
                */
                Dx=((NowPos[0]-Centroid[0])<<1)+ContourWalkV[Predir][1]+ContourWalkV[NowWalkDir][1];
                Dy=((Centroid[1]-NowPos[1])<<1)-ContourWalkV[Predir][0]-ContourWalkV[NowWalkDir][0];
                NowPos[1]+=ContourWalkV[NowWalkDir][0];
                NowPos[0]+=ContourWalkV[NowWalkDir][1];
                //Angle=(int)(atan2(Dy,Dx)*SignInfoAngleNum/MathPI);
                if(!Dy||!Dx)
                {
                        if(Dy==0)
                        {
                                if(Dx>=0)Angle=0;
                                else     Angle=SignInfoAngleNum>>1;
                        }
                        else
                        if(Dy>0)         Angle=SignInfoAngleNum>>2;
                        else             Angle=(SignInfoAngleNum*3)>>2;

                }
                else
                        Angle=DoubleRoundInt(Radian2Degree*atan2(Dy,Dx));
                if(Angle<0)Angle+=SignInfoAngleNum;
                Distance=(int)(SignInfoDistanceMuti*hypot(Dx,Dy)/2);
                if(Signature[Angle]<Distance)Signature[Angle]=Distance;

        }while(PointSymbol!=254);

        CVector[NowPos[1]][NowPos[0]*3]=OriginVar;
}
void acvContourSignatureWPA(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature,int *PresciseAngle,int InitDir)
{                                            //
        int NowPos[2]={StartPos[0],StartPos[1]};

        int NowWalkDir=InitDir,Predir=6;//CounterClockWise
        int *WalkDirV;
        int Angle,HPAngle;
        int Dx,Dy;
        BYTE PointSymbol;
        BYTE **CVector=LabeledPic->CVector;
        int Distance;
        int OriginVar;

        /*
        NowWalkDir=5;
        while(CVector[NowPos[1]][(NowPos[0]+1)*3]-
              CVector[NowPos[1]][(NowPos[0]  )*3]<=0) //Black(+0)->White (+1)
        {
                NowPos[0]++;
                //CVector[ NowPos[1]][(NowPos[0])*3+1]=255;
        }
        Signature[0]=SignInfoDistanceMuti*(NowPos[0]-Centroid[0]);  */

        OriginVar=CVector[NowPos[1]][NowPos[0]*3];
        CVector[NowPos[1]][NowPos[0]*3]=254;//StartSymbol
        do
        {
                NowWalkDir+=2;
                if(NowWalkDir>7)NowWalkDir-=8;

                PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                //Predir=NowWalkDir;
                while(PointSymbol==255)
                {
                        Predir=NowWalkDir;
                        if(!NowWalkDir)NowWalkDir=7;
                        else           NowWalkDir--;
                        PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                }
                //NowWalkDir=SearchDir;
                /*
                CVector[ NowPos[1]+ContourWalkV[Predir][0]
                ][(NowPos[0]+ContourWalkV[Predir][1])*3+1]=255;
                CVector[ NowPos[1]+ContourWalkV[Predir][0]
                ][(NowPos[0]+ContourWalkV[Predir][1])*3+2]=0;
                */
                Dx=((NowPos[0]-Centroid[0])<<1)+ContourWalkV[Predir][1]+ContourWalkV[NowWalkDir][1];
                Dy=((Centroid[1]-NowPos[1])<<1)-ContourWalkV[Predir][0]-ContourWalkV[NowWalkDir][0];
                NowPos[1]+=ContourWalkV[NowWalkDir][0];
                NowPos[0]+=ContourWalkV[NowWalkDir][1];
                //Angle=(int)(atan2(Dy,Dx)*SignInfoAngleNum/MathPI);
                if(!Dy||!Dx)
                {
                        if(Dy==0)
                        {
                                if(Dx>=0)Angle=0;
                                else     Angle=SignInfoAngleNum>>1;
                        }
                        else
                        if(Dy>0)         Angle=SignInfoAngleNum>>2;
                        else             Angle=(SignInfoAngleNum*3)>>2;

                }
                else
                {
                        HPAngle=Radian2Degree*atan2(Dy,Dx)*SignInfoDistanceMuti;
                        Angle=HPAngle/SignInfoDistanceMuti;
                }
                if(Angle<0)Angle+=SignInfoAngleNum;
                Distance=(int)(SignInfoDistanceMuti*hypot(Dx,Dy)/2);
                if(Signature[Angle]<Distance)
                {
                        Signature[Angle]=Distance;

                        PresciseAngle[Angle]=HPAngle;
                }

        }while(PointSymbol!=254);

        CVector[NowPos[1]][NowPos[0]*3]=OriginVar;
}


void acvContourSignature2(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature)
{
        int NowPos[2]={StartPos[0],StartPos[1]};

        int NowWalkDir=5;//CounterClockWise
        int *WalkDirV;
        int AngleRange=0;
        int Angle;
        int Dx,Dy;
        BYTE PointSymbol;
        BYTE **CVector=LabeledPic->CVector;
        int Distance;
        int OriginVar;
        /*

        NowWalkDir=5;
        while(CVector[NowPos[1]][(NowPos[0]+1)*3]-
              CVector[NowPos[1]][(NowPos[0]  )*3]<=0) //Black(+0)->White (+1)
        {
                NowPos[0]++;
                //CVector[ NowPos[1]][(NowPos[0])*3+1]=255;
        }
        Signature[0]=SignInfoDistanceMuti*(NowPos[0]-Centroid[0]);  */
        OriginVar=CVector[NowPos[1]][NowPos[0]*3];
        CVector[NowPos[1]][NowPos[0]*3]=254;//StartSymbol
        do
        {
                NowWalkDir+=2;
                if(NowWalkDir>7)NowWalkDir-=8;

                PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                while(PointSymbol==255)
                {

                        if(NowWalkDir==0)NowWalkDir=7;
                        else            NowWalkDir--;
                        PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                }
                NowPos[1]+=ContourWalkV[NowWalkDir][0];
                NowPos[0]+=ContourWalkV[NowWalkDir][1];
                Dx=NowPos[0]-Centroid[0];
                Dy=Centroid[1]-NowPos[1];
                //Angle=(int)(atan2(Dy,Dx)*SignInfoAngleNum/MathPI);
                if(!Dy||!Dx)
                {
                        if(Dy==0)
                        {
                                if(Dx>=0)Angle=0;
                                else     Angle=SignInfoAngleNum>>1;
                        }
                        else
                        if(Dy>0)         Angle=SignInfoAngleNum>>2;
                        else             Angle=(SignInfoAngleNum*3)>>2;

                }
                else
                        Angle=DoubleRoundInt(Radian2Degree*atan2(Dy,Dx));
                if(Angle==0)
                        Angle=0;
                if(Angle<0)Angle+=SignInfoAngleNum;
                Distance=(int)(SignInfoDistanceMuti*hypot(Dx,Dy));
                if(Signature[Angle]<Distance)Signature[Angle]=Distance;
                //CVector[ NowPos[1]][(NowPos[0])*3+1]=255;
        }while(PointSymbol!=254);

        CVector[NowPos[1]][NowPos[0]*3]=OriginVar;
}

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


//Displacement, Scale, Aangle
float acvSpatialMatchingGradient(acvImage  *Pic,acv_XY *PicPtList,
  acvImage *targetMap,acvImage *targetGradient,acv_XY *TarPtList,
  acv_XY *ErrorGradientList,int ListL)
{
  float errorSum=0;

  //  []
  //[][][]
  //  []
  for(int i=0;i<ListL;i++)
  {
    acv_XY tarpt=TarPtList[i];
    acv_XY picpt=PicPtList[i];
    float error=(
      acvUnsignedMap1Sampling_Nearest(Pic,picpt)-
      acvUnsignedMap1Sampling(targetMap,tarpt));

    acv_XY gradient=acvSignedMap2Sampling_Nearest(targetGradient,tarpt);
    //error=(error<0)?-128:128;

    //printf(">%f %d\n",gradient.X,(char)targetGradient->CVector[(int)tarpt.Y][(int)tarpt.X*3]);
    //Sobel [0] ^  [1] <
    ErrorGradientList[i].X=error*gradient.X;
    ErrorGradientList[i].Y=error*gradient.Y;


    error*=error;
    errorSum+=error;
  }
  return errorSum;
}


bool acvContourCircleSignature
(acvImage  *LabeledPic,acv_LabeledData ldata,int labelIdx,std::vector<acv_XY> &signature)
{
  char State=0;
  int ret=-1;
  memset((void*)&(signature[0]),0,signature.size()*sizeof(signature[0]));

  int X,Y;
  int startX,startY;
  X=(int)ldata.LTBound.X;
  Y=(int)ldata.LTBound.Y;
  for(int j=X;j<(int)ldata.RBBound.X;j++)
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


  int CC=0;
  //0|1|2
  //7|X|3
  //6|5|4
  int dir =3;//>
  do {
      BYTE* pix=acvContourWalk(LabeledPic,&X,&Y,&dir,1);
      dir-=2;
      float diffX=Y-ldata.Center.Y;
      float diffY=X-ldata.Center.X;
      float theta=acvFAtan2(diffY,diffX);//-pi ~pi
      if(theta<0)theta+=2*M_PI;
      int idx=(int)(signature.size()*theta/(2*M_PI));
      float R=hypot(diffX,diffY);
      if(signature[idx].X<R)
      {
        signature[idx].X=R;
        signature[idx].Y=theta;
      }

    /* code */
  } while(X!=startX||Y!=startY);


}


/*0 1 2
  7 X 3
  6 5 4
*/
#define FailCounterCMax             10//SignInfoAngleNum
bool acvContourCircleSignature
(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature)
{
        int NowPos[2]={StartPos[0],StartPos[1]};

        int NowWalkDir=5,Predir=6;//CounterClockWise
        int *WalkDirV;
        int AngleRange=0;
        int ErrorAngle=0;
        int Angle;
        int Dx,Dy;
        BYTE PointSymbol;
        BYTE **CVector=LabeledPic->CVector;
        int Distance;
        int OriginVar;
        int AngleFront;
        int FailCounter=0;
        OriginVar=CVector[NowPos[1]][NowPos[0]*3];
        CVector[NowPos[1]][NowPos[0]*3]=254;//StartSymbol

        AngleFront=DoubleRoundInt(Radian2Degree*atan2(
        ((Centroid[1]-NowPos[1])<<1),
        ((NowPos[0]-Centroid[0])<<1)));
        if(AngleFront<0)AngleFront+=SignInfoAngleNum;
        do
        {
                NowWalkDir+=2;
                if(NowWalkDir>7)NowWalkDir-=8;

                PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                //Predir=NowWalkDir;
                while(PointSymbol==255)
                {
                        Predir=NowWalkDir;
                        if(!NowWalkDir)NowWalkDir=7;
                        else           NowWalkDir--;
                        PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                }
                Dx=((NowPos[0]-Centroid[0])<<1)+ContourWalkV[Predir][1]+ContourWalkV[NowWalkDir][1];
                Dy=((Centroid[1]-NowPos[1])<<1)-ContourWalkV[Predir][0]-ContourWalkV[NowWalkDir][0];
                NowPos[1]+=ContourWalkV[NowWalkDir][0];
                NowPos[0]+=ContourWalkV[NowWalkDir][1];
                if(!Dy||!Dx)
                {
                        if(Dy==0)
                        {
                                if(Dx>=0)Angle=0;
                                else     Angle=SignInfoAngleNum>>1;
                        }
                        else
                        if(Dy>0)         Angle=SignInfoAngleNum>>2;
                        else             Angle=(SignInfoAngleNum*3)>>2;

                }
                else
                        Angle=DoubleRoundInt(Radian2Degree*atan2(Dy,Dx));
                if(Angle<0)Angle+=SignInfoAngleNum;
                Distance=(int)(SignInfoDistanceMuti*hypot(Dx,Dy)/2);

                AngleFront=Angle-AngleFront;
                if(AngleFront<0&&AngleFront>=-(SignInfoAngleNum>>1))
                {
                        ErrorAngle+=-AngleFront;
                        if(++FailCounter>FailCounterCMax)
                        {
                                CVector[StartPos[1]][StartPos[0]*3]=OriginVar;
                                return false;
                        }
                }

                AngleFront=Angle;
                if(Signature[Angle]<Distance)Signature[Angle]=Distance;

        }while(PointSymbol!=254);

        CVector[NowPos[1]][NowPos[0]*3]=OriginVar;

        return true;
}
//#define MaxnumErrorAngle 8000000
//#define MaxnumErrorAngle 50000
#define MaxnumErrorAngle 50000

bool acvContourCircleSignatureWPA
(acvImage  *LabeledPic,int *Centroid,int *StartPos,int *Signature,int *PresciseAngle)
{
        int NowPos[2]={StartPos[0],StartPos[1]};

        int NowWalkDir=5,Predir=6;//CounterClockWise
        int *WalkDirV;
        int Angle,ErrorAngle=0;
        int PreError=0;
        int HPAngle;

        int Dx,Dy;
        BYTE PointSymbol;
        BYTE **CVector=LabeledPic->CVector;
        int Distance;
        int OriginVar;
        int AngleFront;
        OriginVar=CVector[NowPos[1]][NowPos[0]*3];
        CVector[NowPos[1]][NowPos[0]*3]=254;//StartSymbol


        AngleFront=SignInfoDistanceMuti*Radian2Degree*atan2(
        ((Centroid[1]-NowPos[1])<<1),
        ((NowPos[0]-Centroid[0])<<1));

        if(AngleFront<0)AngleFront+=SignInfoDistanceMuti*SignInfoAngleNum;

        do
        {
                NowWalkDir+=2;
                if(NowWalkDir>7)NowWalkDir-=8;

                PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                //Predir=NowWalkDir;
                while(PointSymbol==255)
                {
                        Predir=NowWalkDir;
                        if(!NowWalkDir)NowWalkDir=7;
                        else           NowWalkDir--;
                        PointSymbol=CVector[ NowPos[1]+ContourWalkV[NowWalkDir][0]   ]
                             [(NowPos[0]+ContourWalkV[NowWalkDir][1])*3];
                }
                Dx=((NowPos[0]-Centroid[0])<<1)+ContourWalkV[Predir][1]+ContourWalkV[NowWalkDir][1];
                Dy=((Centroid[1]-NowPos[1])<<1)-ContourWalkV[Predir][0]-ContourWalkV[NowWalkDir][0];
                NowPos[1]+=ContourWalkV[NowWalkDir][0];
                NowPos[0]+=ContourWalkV[NowWalkDir][1];


                if(!Dy||!Dx)
                {
                        if(Dy==0)
                        {
                                if(Dx>=0)Angle=0;
                                else     Angle=SignInfoAngleNum>>1;
                        }
                        else
                        if(Dy>0)         Angle=SignInfoAngleNum>>2;
                        else             Angle=(SignInfoAngleNum*3)>>2;

                        HPAngle=Angle*SignInfoDistanceMuti;

                }
                else
                        HPAngle=SignInfoDistanceMuti*Radian2Degree*atan2(Dy,Dx);

                Distance=(int)(SignInfoDistanceMuti*hypot(Dx,Dy)/2);
                if(HPAngle<0)
                        HPAngle+=SignInfoDistanceMuti*SignInfoAngleNum;
                Dx=HPAngle/SignInfoDistanceMuti*SignInfoDistanceMuti;
                if(HPAngle-Dx>(SignInfoDistanceMuti>>1))
                     Angle=Dx/SignInfoDistanceMuti+1;
                else
                     Angle=Dx/SignInfoDistanceMuti;
                //PresciseAngle

                AngleFront=HPAngle-AngleFront;
                if(AngleFront<0&&AngleFront>=-(SignInfoDistanceMuti*SignInfoAngleNum>>1))
                {
                        if(PreError)
                                ErrorAngle-=AngleFront+PreError;
                        PreError=AngleFront;
                        if(ErrorAngle>MaxnumErrorAngle)
                        {
                                CVector[StartPos[1]][StartPos[0]*3]=OriginVar;
                                return false;
                        }


                }
                else
                        PreError=0;

                AngleFront=HPAngle;
                if(Signature[Angle]<Distance)
                {
                        Signature[Angle]=Distance;
                        PresciseAngle[Angle]=HPAngle;
                }

        }while(PointSymbol!=254);

        CVector[NowPos[1]][NowPos[0]*3]=OriginVar;

        return true;
}



#define QR_VERINFO_COLUMEL    2//SignInfoAngleNum
#define QR_VERINFO_INNERW     0
#define QR_VERINFO_SFINDERNUM 1
#define GetQR_VerInfo(Ver,Info) QR_VerInfo[QR_VERINFO_COLUMEL*Ver+Info]
const unsigned char QR_VerInfo[]={
     7,0,
    11,1,
    15,1,
    19,1,
    23,1,
    27,1,
    31,6,
    35,6,
    39,6,
    43,6,
    47,6,
    51,6,
    55,6,
    59,13,
    63,13,
    67,13,
    71,13,
    75,13,
    79,13,
    83,13,
    87,22,
    91,22,
    95,22,
    99,22,
    103,22,
    107,22,
    111,22,
    115,33,
    119,33,
    123,33,
    127,33,
    131,33,
    135,33,
    139,33,
    143,46,
    147,46,
    151,46,
    155,46,
    159,46,
    163,46};


#define FindPolygonInfoColumn              SignInfoAngleNum

//#define FindPolygonCentroidX            0
//#define FindPolygonCentroidY         1
#define FindPolygonVertexS                   0



#define SFPoMeanFilterSize                  SignInfoAngleNum/12
int SignatureFindPolygonWPA(int* VertexPosition,int* Signature,int *PresciseAngle)
{
        int i,VertexNum,State,*SignData,*SFPoMeanData;

        int Position,j;
        int &Tmp=State;
        SignData=Signature;

        SFPoMeanData=VertexPosition;



        acvRingDataMeanFilter(SFPoMeanData,SignData,SignInfoAngleNum,SFPoMeanFilterSize);
        //FindParallelogramXSize
        int OOO=0;

        int TotalMean;//=SignData[0];
        if(SignData[0]>SFPoMeanData[0]*504/500)
        {

                VertexNum=2;
                //VertexData[(VertexNum-1)<<1]=SignData[0];
                //VertexData[(VertexNum-1)<<1+1]=0;
                SFPoMeanData[0]=SignData[0];
                SFPoMeanData[1]=0;
                State=true;

                OOO=1;
        }
        else
        {
                State=false;
                VertexNum=0;
        }
        for(i=1;i<SignInfoAngleNum;i++)
        {
                //TotalMean+=SignData[i];
                if(SignData[i]>SFPoMeanData[i]*504/500)
                {

                        if(!State)
                        {
                                VertexNum+=2;
                                SFPoMeanData[VertexNum-2]=SignData[i];
                                SFPoMeanData[VertexNum-1]=i;
                                State=true;
                        }
                        else if(SignData[i]>SFPoMeanData[VertexNum-2])
                        {
                                SFPoMeanData[VertexNum-2]=SignData[i];
                                SFPoMeanData[VertexNum-1]=i;
                        }
                        if(i==SignInfoAngleNum-1&&OOO)
                        {
                                i=360*(SignInfoAngleNum-SFPoMeanData[VertexNum-1]+SFPoMeanData[1])/SignInfoAngleNum;
                                /*if(i>25)
                                {

                                    break;
                                } */
                                if(SFPoMeanData[VertexNum-2]>SFPoMeanData[0])
                                {
                                        SFPoMeanData[0]=SFPoMeanData[VertexNum-2];
                                        SFPoMeanData[1]=SFPoMeanData[VertexNum-1];

                                }
                                VertexNum-=2;
                                break;
                        }
                }
                else
                {
                        if(State)
                        {
                                State=false;
                                j=i+1;
                                if(j>=SignInfoAngleNum)
                                       j=SignInfoAngleNum-1;
                                i++;
                                for(;i<j;i++)
                                if(SignData[i]>SFPoMeanData[i]*500/500
                                &&SignData[i]>SFPoMeanData[VertexNum-2])
                                {
                                        SFPoMeanData[VertexNum-2]=SignData[i];
                                        SFPoMeanData[VertexNum-1]=i;
                                        State=true;
                                        break;
                                }

                        }
                }
        }
        if(VertexNum<4)
                return -1;

        VertexNum>>=1;


        for(i=0;i<VertexNum*2;i+=2)
        {
                Position= PresciseAngle[SFPoMeanData[i+1]];
                Tmp=SFPoMeanData[i];
                //SignData[SFPaMeanData[i+1]]*=2;
                SFPoMeanData[FindPolygonVertexS+i]=
                DoubleRoundInt(Tmp*cos(Position*Degree2Radian/SignInfoDistanceMuti));
                SFPoMeanData[FindPolygonVertexS+i+1]=
                DoubleRoundInt(-Tmp*sin(Position*Degree2Radian/SignInfoDistanceMuti));
        }

        return VertexNum;

}
