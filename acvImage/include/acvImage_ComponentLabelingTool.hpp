
#ifndef ACV_IMG_COMP_LABEL_H
#define ACV_IMG_COMP_LABEL_H
#include "acvImage_BasicTool.hpp"

typedef struct _3BYTE
{
        unsigned Num:24;
        unsigned Empty:8;
}_3BYTE;
typedef struct BYTE3
{
        unsigned Num2:8;
        unsigned Num1:8;
        unsigned Num0:8;
        unsigned Empty:8;
}BYTE3;
typedef union _24BitUnion
{
        BYTE3 Byte3;
        _3BYTE _3Byte;
}_24BitUnion;
//Num  0000_0000       0000_0000       0000_0000
//Num0 0000_0000  Num1 0000_0000  Num2 0000_0000

/*
#ifndef LabelingBy4OrM_
#define LabelingBy4OrM_ 1          //1 ->  4-adjacency      0->  m-adjacency
#endif


#define LabelingInfoColumn              10
#define LabelingArea                     0
#define LabelingDotsXSum                1
#define LabelingDotsYSum                2
#define LabelingCentroidX                1
#define LabelingCentroidY                2


#define LabelingAmendCentroidX          3
#define LabelingAmendCentroidY          4


#define LabelingOtherInfo1          8
#define LabelingOtherInfo2          9
#define LabelingStartPosX           8
#define LabelingStartPosY           9

#define LabelingNoComponentExist       -1

void acvLabeledInfoAdjust(DyArray<int> * LabelInfo);//
//Lebeled Number Write in B G R .Exp Num=300 B=0  G=1 R=44
//        MaxNum=(256*256*256-1)>>ColorDispersion=16777215>>ColorDispersion=> B=254 G=255 B=255
void acvRecordLabel(BYTE *BGR,_24BitUnion NowLable,int X,int Y,int *TableData)
{
        TableData[0]=X;
        TableData[1]=Y;

        BGR[0]=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        BGR[1]=NowLable.Byte3.Num1;
        BGR[2]=NowLable.Byte3.Num2;

}


void acvCheck4Connect2(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * CoorTable)
{

      int NowInfoLength= CoorTable->Length();
      int *TableData;
      BYTE* BGR;
      X++;
      if(Pic->CVector[Y][3*(X)]==0)//Y   X+1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);

        NowInfoLength+=2;
      }
      X-=2;
      if(Pic->CVector[Y][3*(X)]==0)//Y   X-1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      X++;
      Y++;
      if(Pic->CVector[Y][3*(X)]==0)//Y+1   X
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      Y-=2;

      if(Pic->CVector[Y][3*(X)]==0)//Y-1   X
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      CoorTable->ReLength(NowInfoLength);
}
void acvCheck8Connect2(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * CoorTable)
{

      int NowInfoLength= CoorTable->Length();
      int *TableData;
      BYTE* BGR;
      if(Pic->CVector[Y][3*(++X)]==0)//Y   X+1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);

        NowInfoLength+=2;
      }
      if(Pic->CVector[++Y][3*(X)]==0)//Y+1   X+1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[Y][3*(--X)]==0)//Y+1   X
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[Y][3*(--X)]==0)//Y+1   X-1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[--Y][3*(X)]==0)//Y   X-1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[--Y][3*(X)]==0)//Y-1   X-1
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[Y][3*(++X)]==0)//Y-1   X
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      if(Pic->CVector[Y][3*(++X)]==0)//Y-1   X
      {
        TableData=(int*)CoorTable->GetPointer(NowInfoLength);
        BGR=&(Pic->CVector[Y][3*X]);
        acvRecordLabel(BGR,NowLable,X,Y,TableData);
        NowInfoLength+=2;
      }
      CoorTable->ReLength(NowInfoLength);
}

void acvComponentFilling2(acvImage *Pic,_24BitUnion NowLable
                        ,DyArray<int> * ProcTable,DyArray<int> * BackTable
                        ,DyArray<int> * Information)
{
      int NowTableLength=ProcTable->Length(),x,y,SumX=0,SumY=0,ToTalDotNum=0;

      BackTable->ReLength(0);
      while(NowTableLength)
      {
        for(int k=0;k<NowTableLength;k+=2)
        {
                x=DAData_Var(ProcTable,(k)  );
                y=DAData_Var(ProcTable,(k+1)  );
                SumX+=x;
                SumY+=y;
                ToTalDotNum++;
                #if LabelingBy4OrM_
                acvCheck4Connect2(Pic,NowLable,x,y,BackTable);
                #else
                acvCheck8Connect2(Pic,NowLable,x,y,BackTable);

                #endif
        }
        NowTableLength=(int)ProcTable;
        ProcTable=BackTable;
        BackTable=(DyArray<int>*)NowTableLength;
        BackTable->ReLength(0);
        NowTableLength=ProcTable->Length();
      }
      NowTableLength=NowLable._3Byte.Num*LabelingInfoColumn;
      Information->ReLength(NowTableLength+LabelingInfoColumn);
      DAData_Var(Information,NowTableLength+LabelingArea)=ToTalDotNum;
      DAData_Var(Information,NowTableLength+LabelingCentroidX)=SumX/ToTalDotNum;
      DAData_Var(Information,NowTableLength+LabelingCentroidY)=SumY/ToTalDotNum;


}
void acvComponentFilling2(acvImage *Pic,_24BitUnion NowLable
                        ,DyArray<int>* ProcTable,DyArray<int> * BackTable)
{
      int NowTableLength=ProcTable->Length();

      BackTable->ReLength(0);
      while(NowTableLength)
      {
        for(int k=0;k<NowTableLength;k+=2)
        {
                #if LabelingBy4OrM_
                acvCheck4Connect2(Pic,NowLable,DAData_Var(ProcTable,(k)  ),DAData_Var(ProcTable,(k)+1),BackTable);
                #else
                acvCheck8Connect2(Pic,NowLable,DAData_Var(ProcTable,(k)  ),DAData_Var(ProcTable,(k)+1),BackTable);

                #endif
        }
        NowTableLength=(int)ProcTable;
        ProcTable=BackTable;
        BackTable=(DyArray<int>*)NowTableLength;
        BackTable->ReLength(0);
        NowTableLength=ProcTable->Length();
      }
}

void acvComponentLabeling2(acvImage *Pic,DyArray<int>* Information)
{

       int      i,j,
                Width=Pic->GetWidth()-1,
                Height=Pic->GetHeight()-1;

       static DyArray<int>*ProcTable=new DyArray<int>(128);
       static DyArray<int>*BackTable=new DyArray<int>(128);
       Information->ReLength(0);
       _24BitUnion NowLable;

       acvDeletFrame(Pic);

       NowLable._3Byte.Num=0;

       for(i=1;i<Height;i++)for(j=1;j<Width;j++)
       {
             if(Pic->CVector[i][3*j])continue;

             DAData_Var(ProcTable,0)=j;    //X
             DAData_Var(ProcTable,1)=i;    //Y

             ProcTable->ReLength(2);
             acvComponentFilling2(Pic,NowLable,ProcTable,BackTable,Information);
             NowLable._3Byte.Num++;
       }
       for(i=1;i<Height;i++)for(j=1;j<Width;j++)
             if(Pic->CVector[i][3*j]!=255)Pic->CVector[i][3*j]-=1;
}
void acvCheck4Connect(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int> * ProcTable)
{

      int *Coordinate;
      BYTE* BGR;

      X++;
      if(Pic->CVector[Y][3*(X)]==0)//Y   X+1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      X-=2;
      if(Pic->CVector[Y][3*(X)]==0)//Y   X-1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      X++;
      Y++;
      if(Pic->CVector[Y][3*(X)]==0)//Y+1   X
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      Y-=2;

      if(Pic->CVector[Y][3*(X)]==0)//Y-1   X
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
}



void acvCheck8Connect(acvImage *Pic,_24BitUnion NowLable,int X,int Y,DyArray<int>* ProcTable)
{

      BYTE* BGR;
      int *Coordinate;


      if(Pic->CVector[Y][3*(++X)]==0)//Y   X+1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[++Y][3*(X)]==0)//Y+1   X+1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[Y][3*(--X)]==0)//Y+1   X
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[Y][3*(--X)]==0)//Y+1   X-1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[--Y][3*(X)]==0)//Y   X-1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[--Y][3*(X)]==0)//Y-1   X-1
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[Y][3*(++X)]==0)//Y-1   X
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
      if(Pic->CVector[Y][3*(++X)]==0)//Y-1   X
      {
        Coordinate=(int*)ProcTable->Push();
        Coordinate[0]=X;
        Coordinate[1]=Y;
        BGR=Pic->CVector[Y]+3*X;
        *BGR++=NowLable.Byte3.Num0+1;// 1 Be a Mark of labeled
        *BGR++=NowLable.Byte3.Num1;
        *BGR++=NowLable.Byte3.Num2;
      }
}

void acvComponentFilling(acvImage *Pic,_24BitUnion NowLable
                        ,DyArray<int> * ProcTable
                        ,DyArray<int> * Information)
{
      int SumX=0,SumY=0,ToTalDotNum=0,*Coordinate,*Info;
      int Tmp=0;
      Coordinate=(int*)ProcTable->Front();
      Tmp=NowLable._3Byte.Num*LabelingInfoColumn;
      Information->ReLength(Tmp+LabelingInfoColumn);

      Info=Information->GetPointer(Tmp);
      Info[LabelingStartPosX]=Coordinate[0];
      Info[LabelingStartPosY]=Coordinate[1];
      while(ProcTable->Length())
      {
        Coordinate=(int*)ProcTable->Front();
        Tmp++;
        ProcTable->QueuePop();
        SumX+=Coordinate[0];
        SumY+=Coordinate[1];
        ToTalDotNum++;

        #if LabelingBy4OrM_
        acvCheck4Connect(Pic,NowLable,Coordinate[0],Coordinate[1],ProcTable);
        #else
        acvCheck8Connect(Pic,NowLable,Coordinate[0],Coordinate[1],ProcTable);
        #endif
      }

      Info[LabelingArea]=ToTalDotNum;
      Info[LabelingCentroidX]=SumX/ToTalDotNum;
      Info[LabelingCentroidY]=SumY/ToTalDotNum;
      //Coordinate[LabelingArea]=ToTalDotNum;


}

void acvComponentLabeling(acvImage *Pic,DyArray<int> * Information) //0
{

       int      i,j,*QueueArray,
                Xoffset=Pic->GetROIOffsetX(),
                Yoffset=Pic->GetROIOffsetY(),
                Width=Pic->GetWidth()+Xoffset,
                Height=Pic->GetHeight()+Yoffset

                ;
       static DyArray<int> *ProcTable=new DyArray<int>(2);

       Information->ReLength(0);
       _24BitUnion NowLable;

       acvDeletFrame(Pic);

       NowLable._3Byte.Num=0;


       Xoffset++;
       Yoffset++;
       Height--;
       Width--;
       for(i=Yoffset;i<Height;i++)for(j=Xoffset;j<Width;j++)
       {
             if(Pic->CVector[i][3*j])
             {
                if(Pic->CVector[i][3*j]!=255)
                        Pic->CVector[i][3*j]--;
                continue;
             }

             ProcTable->ClearLinearContainer();
             QueueArray=(int*)ProcTable->Push();
             QueueArray[0]=j;    //X
             QueueArray[1]=i;    //Y

             acvComponentFilling(Pic,NowLable,ProcTable,Information);
             Pic->CVector[i][3*j]--;
             NowLable._3Byte.Num++;

       }
}

#define CentroidMutiNum  10
void acvComponentFilling_CentroidMuti(acvImage *Pic,_24BitUnion NowLable
                        ,DyArray<int> * ProcTable
                        ,DyArray<int> * Information)
{
      int SumX=0,SumY=0,ToTalDotNum=0,*Coordinate;
      int Tmp=0;
      while(ProcTable->Length())
      {
        Coordinate=(int*)ProcTable->Front();
        Tmp++;
        ProcTable->QueuePop();
        SumX+=Coordinate[0];
        SumY+=Coordinate[1];
        ToTalDotNum++;

        #if LabelingBy4OrM_
        acvCheck4Connect(Pic,NowLable,Coordinate[0],Coordinate[1],ProcTable);
        #else
        acvCheck8Connect(Pic,NowLable,Coordinate[0],Coordinate[1],ProcTable);
        #endif
      }

      Tmp=NowLable._3Byte.Num*LabelingInfoColumn;
      Information->ReLength(Tmp+LabelingInfoColumn);

      DAData_Var(Information,Tmp+LabelingArea)=ToTalDotNum;
      DAData_Var(Information,Tmp+LabelingCentroidX)=CentroidMutiNum*SumX/ToTalDotNum;
      DAData_Var(Information,Tmp+LabelingCentroidY)=CentroidMutiNum*SumY/ToTalDotNum;


}

void acvComponentLabeling_CentroidMuti(acvImage *Pic,DyArray<int> * Information) //0
{

       int      i,j,*QueueArray,
                Xoffset=Pic->GetROIOffsetX(),
                Yoffset=Pic->GetROIOffsetY(),
                Width=Pic->GetWidth()+Xoffset,
                Height=Pic->GetHeight()+Yoffset

                ;
       static DyArray<int> *ProcTable=new DyArray<int>(2);

       Information->ReLength(0);
       _24BitUnion NowLable;

       acvDeletFrame(Pic);

       NowLable._3Byte.Num=0;


       Xoffset++;
       Yoffset++;
       Height--;
       Width--;
       for(i=Yoffset;i<Height;i++)for(j=Xoffset;j<Width;j++)
       {
             if(Pic->CVector[i][3*j])
             {
                if(Pic->CVector[i][3*j]!=255)
                        Pic->CVector[i][3*j]--;
                continue;
             }

             ProcTable->ClearLinearContainer();
             QueueArray=(int*)ProcTable->Push();
             QueueArray[0]=j;    //X
             QueueArray[1]=i;    //Y
             acvComponentFilling_CentroidMuti(Pic,NowLable,ProcTable,Information);
             Pic->CVector[i][3*j]--;
             NowLable._3Byte.Num++;

       }
}









void acvLabeledColorDispersion(acvImage *ColorDispersionPic,acvImage *LabeledPic,int ColorNum)//0
{
        int     i,j,
                Xoffset=LabeledPic->GetROIOffsetX(),
                Yoffset=LabeledPic->GetROIOffsetY(),
                Width=LabeledPic->GetWidth()-1+Xoffset,
                Height=LabeledPic->GetHeight()-1+Yoffset

                ;
        _24BitUnion NumTranse;
        BYTE* LabeledLine,*CDPLine;
        BYTE    Color[3];

        if(ColorNum==-1)
        {
                for(i=Yoffset;i<Height;i++)
                {
                        LabeledLine=LabeledPic->CVector[i]+Xoffset*3;;

                        for(j=Xoffset;j<Width;j++)
                        {
                                if(*LabeledLine==255)
                                {
                                        LabeledLine+=3;
                                        continue;
                                }
                                NumTranse.Byte3.Num0=*LabeledLine++;
                                NumTranse.Byte3.Num1=*LabeledLine++;
                                NumTranse.Byte3.Num2=*LabeledLine++;

                                if(ColorNum<(int)NumTranse._3Byte.Num)
                                        ColorNum=NumTranse._3Byte.Num;
                        }
                }


        }
        else if(!ColorNum)ColorNum=1;
        for(i=Yoffset;i<Height;i++)
        {
                LabeledLine=LabeledPic->CVector[i]+Xoffset*3;;
                CDPLine=ColorDispersionPic->CVector[i]+Xoffset*3;;

                 for(j=Xoffset;j<Width;j++)
                {
                        if(*LabeledLine==255)
                        {
                               LabeledLine+=3;
                                *CDPLine++=255;
                                *CDPLine++=255;
                                *CDPLine++=255;
                                continue;
                        }
                        NumTranse.Byte3.Num0=*LabeledLine++;
                        NumTranse.Byte3.Num1=*LabeledLine++;
                        NumTranse.Byte3.Num2=*LabeledLine++;

                        Color[2]=((NumTranse._3Byte.Num)*HSV_HMax/ColorNum);
                        Color[1]=
                        Color[0]=HSV_VMax;
                        LabeledPic->RGBFromHSV(Color,Color);
                        *CDPLine++=Color[0];
                        *CDPLine++=Color[1];
                        *CDPLine++=Color[2];




                }
        }
}
#define MathPI           3.141592654

#define SignInfoAngleNum 30
#define Radian2Degree    (SignInfoAngleNum)/MathPI/2
#define Degree2Radian    MathPI*2/SignInfoAngleNum

#define SignInfoDistanceMuti 10000


void acvLabeledSignature(acvImage *LabeledPic,DyArray<int> * LabelInfo,DyArray<int> * SignInfo)
{
        BYTE *LabeledLine;
        _24BitUnion NumTranse;
        double  Degree;
        int *DataArray,XSub,YSub,*MaxDistance,Distance;
        SignInfo->ReLength(SignInfoAngleNum*LabelInfo->Length()/LabelingInfoColumn);
        for(int i=0;i<SignInfo->Length();i++)
                *(int*)SignInfo->GetPointer(i)=0;
        for(int i=0;i<LabeledPic->GetHeight();i++)
        {
                LabeledLine=LabeledPic->CVector[i];
                for(int j=0;j<LabeledPic->GetWidth();j++)
                {
                        if(*LabeledLine==255)
                        {
                                LabeledLine+=3;
                                continue;
                        }
                        NumTranse.Byte3.Num0=*LabeledLine++;
                        NumTranse.Byte3.Num1=*LabeledLine++;
                        NumTranse.Byte3.Num2=*LabeledLine++;

                        DataArray=(int*)LabelInfo->GetPointer(NumTranse._3Byte.Num*LabelingInfoColumn);
                        if(!DataArray[LabelingArea ])continue;

                        XSub=j-DataArray[LabelingAmendCentroidX];
                        YSub=DataArray[LabelingAmendCentroidY]-i;
                        if(!XSub&&!YSub)
                                Degree=0;
                        else
                        {
                                Degree=Radian2Degree*atan2(YSub,XSub);
                                Degree=DoubleRoundInt(Degree);
                                if(Degree<0)Degree+=SignInfoAngleNum;
                                //if(Degree>=SignInfoAngleNum)Degree-=SignInfoAngleNum;

                        }
                        MaxDistance=(int*)SignInfo->GetPointer(SignInfoAngleNum*NumTranse._3Byte.Num+(int)Degree);

                        Distance=SignInfoDistanceMuti*hypot(XSub,YSub);
                        if(*MaxDistance<Distance)*MaxDistance=Distance ;
                        //(*MaxDistance)=1000;
                }
        }

}

#define HistoGenInfoNum (256*3/HistoGenDataCompress)
#define HistoGenNormalizeNum 10000   //the sum of Histo is NormalizeNum

#define HistoGenDataCompress  30


void acvLabeledHistoGenerater(acvImage *LabeledPic,acvImage *RefPic,DyArray<int> * LabelInfo,DyArray<int> * HistoInfo)
{
        BYTE *LabeledLine,*RefLine;
        _24BitUnion NumTranse;
        int Area,*HistoData,i,j;

        HistoInfo->ReLength(HistoGenInfoNum*LabelInfo->Length()/LabelingInfoColumn);
        for(i=0;i<HistoInfo->Length();i++)
                DAData_Var(HistoInfo,i)=0;

        for(i=0;i<LabeledPic->GetHeight();i++)
        {
                LabeledLine=LabeledPic->CVector[i];
                RefLine =RefPic ->CVector[i];

                for(j=0;j<LabeledPic->GetWidth();j++)
                {
                        if(*LabeledLine==255)
                        {
                                LabeledLine+=3;
                                RefLine+=3;
                                continue;
                        }

                        NumTranse.Byte3.Num0=*LabeledLine++;
                        NumTranse.Byte3.Num1=*LabeledLine++;
                        NumTranse.Byte3.Num2=*LabeledLine++;
                        if(! LabelInfo->GetPointer(NumTranse._3Byte.Num+LabelingArea))continue;

                        HistoData=(int*)HistoInfo->GetPointer(HistoGenInfoNum*NumTranse._3Byte.Num);


                        HistoData[(RefLine[2]/HistoGenDataCompress)*3+2]++;
                        HistoData[(RefLine[1]/HistoGenDataCompress)*3+1]++;
                        HistoData[(RefLine[0]/HistoGenDataCompress)*3+0]++;

                        RefLine+=3;
                }
        }
        for(i=0;i<HistoInfo->Length();i+=HistoGenInfoNum)
        {
                Area=DAData_Var(LabelInfo,(i/HistoGenInfoNum)*LabelingInfoColumn+LabelingArea);
                if(!Area)continue;
                HistoData=(int*)HistoInfo->GetPointer(i);
                for(j=0;j<HistoGenInfoNum;j++)
                        HistoData[j]=HistoData[j]*HistoGenNormalizeNum/Area;

        }
}


void acvWeightedCentroid(acvImage *LabeledPic,DyArray<int> * Information,acvImage *WeightPic)
{
        int Weight,i,j;
        _24BitUnion NumTranse;
        BYTE* LabeledLine,*WeightLine;
        int *DataArray;

        for(i=0;i<Information->Length();i++)
                *(int*)Information->GetPointer(i)=0;

        for(i=0;i<LabeledPic->GetHeight();i++)
        {
                LabeledLine=LabeledPic->CVector[i];
                WeightLine=WeightPic->CVector[i];

                for(j=0;j<LabeledPic->GetWidth();j++)
                {
                        if(*LabeledLine==255)
                        {
                               LabeledLine+=3;
                                continue;
                        }
                        NumTranse.Byte3.Num0=*LabeledLine++;
                        NumTranse.Byte3.Num1=*LabeledLine++;
                        NumTranse.Byte3.Num2=*LabeledLine++;
                        DataArray=(int*)Information->GetPointer(NumTranse._3Byte.Num*LabelingInfoColumn);
                        //To Reach this Function the  UIA of Information Must be  Multiples of InformationColumn
                        //Or Will be Bugged
                        Weight=WeightLine[0];
                        WeightLine+=LabelingInfoColumn;

                        DataArray[LabelingArea]+=Weight;
                        DataArray[LabelingDotsXSum]+=j*Weight;
                        DataArray[LabelingDotsYSum]+=i*Weight;


                }
        }
       acvLabeledInfoAdjust(Information);

}

int acvThresholdArea2(acvImage *LabeledPic,DyArray<int> * Information,int Valve)
{
        int i,j,Tmp,Num=-1;
        _24BitUnion NumTranse;
        BYTE* BMPLine;

        if(*(int*)Information->GetPointer(0)==LabelingNoComponentExist){goto Exit;}

        if(Valve==-1)for(i=LabelingArea;i<Information->Length();i+=LabelingInfoColumn)
                if(DAData_Var(Information,i)>Valve)
                        Valve=DAData_Var(Information,i);


        for(i=LabelingArea;i<Information->Length();i+=LabelingInfoColumn)
                if(DAData_Var(Information,i)<Valve)
                        DAData_Var(Information,i)=0;


        for(i=0;i<LabeledPic->GetHeight();i++)
        {
                BMPLine=LabeledPic->CVector[i];
                for(j=0;j<LabeledPic->GetWidth();j++)
                {
                        if(*BMPLine==255)
                        {
                                BMPLine+=3;
                                continue;
                        }
                        NumTranse.Byte3.Num0=*BMPLine++;
                        NumTranse.Byte3.Num1=*BMPLine++;
                        NumTranse.Byte3.Num2=*BMPLine++;
                        Num=NumTranse._3Byte.Num;
                        Tmp=DAData_Var(Information,(Num)*LabelingInfoColumn);

                        if(!Tmp)
                        {
                                *(BMPLine-3)=255;
                        }
                }
        }
        Exit:;

        return Num;
}


int acvThresholdArea(acvImage *LabeledPic,DyArray<int> * Information,int Valve) //0
{
        int i,j,Tmp,Num=-1,*TmpPointer,NowDataLocate,NowLength,
                Xoffset=LabeledPic->GetROIOffsetX(),
                Yoffset=LabeledPic->GetROIOffsetY(),
                Width=LabeledPic->GetWidth()-1+Xoffset,
                Height=LabeledPic->GetHeight()-1+Yoffset

                ;
        _24BitUnion NumTranse;
        BYTE* BMPLine;
        static DyArray<int> *NewTable=new DyArray<int> (127);

        if(Information->Length()==0){goto Exit;}

        if(Valve==-1)for(Valve=1,i=LabelingArea;i<Information->Length();i+=LabelingInfoColumn)
                if(DAData_Var(Information,i)>Valve)
                        Valve=DAData_Var(Information,i);   //Find the max Area

        for(Tmp=j=0,i=LabelingArea;i<Information->Length();i+=LabelingInfoColumn,j++)//
        {
                if(DAData_Var(Information,i)>=Valve)
                {
                        DAData_Var(NewTable,j)=Tmp++;//Mark the New Labeling Number

                }
                else
                {
                        DAData_Var(Information,i)=0;//Area to Zero
                        DAData_Var(NewTable,j)=-1;//Set the Deleting Mark
                }
        }
        NowLength=Tmp*LabelingInfoColumn;


        //first ,Process the Image
        for(i=Yoffset;i<Height;i++)
        {
                BMPLine=LabeledPic->CVector[i]+3*Xoffset;
                for(j=Xoffset;j<Width;j++)
                {
                        if(*BMPLine==255)
                        {
                                BMPLine+=3;
                                continue;
                        }

                        ///Getting the old Labeling Number to Find the New Labeling Number
                        NumTranse.Byte3.Num0=BMPLine[0];
                        NumTranse.Byte3.Num1=BMPLine[1];
                        NumTranse.Byte3.Num2=BMPLine[2];
                        Num=NumTranse._3Byte.Num;
                        Tmp=DAData_Var(NewTable,Num);
                        ///


                        if(Tmp==-1)//-1 is the mark to Delete
                        {
                                BMPLine[0]=255;
                        }
                        else
                        {
                                NumTranse._3Byte.Num=Tmp;
                                BMPLine[0]=NumTranse.Byte3.Num0;
                                BMPLine[1]=NumTranse.Byte3.Num1;
                                BMPLine[2]=NumTranse.Byte3.Num2;
                        }
                        BMPLine+=3;
                        //Tmp=DIAData(Information,NumTranse._3Byte.Num*LabelingInfoColumn);

                }
        }

        NowDataLocate=0;
        //Second ,Process the Informateion
        Num=0;
        for(Num=Tmp=i=0;i<Information->Length();i+=LabelingInfoColumn,Tmp++)
        {
                if(DAData_Var(NewTable,Tmp)!=-1)
                {
                        TmpPointer=(int*)Information->GetPointer(Num);
                        for(j=0;j<LabelingInfoColumn;j++)
                        {
                                TmpPointer[j]
                                =DAData_Var(Information,i+j);

                        }
                        Num+=LabelingInfoColumn;
                }

        }
        Information->ReLength(NowLength);

        Exit:;

        return Num;
}
*/


#endif
