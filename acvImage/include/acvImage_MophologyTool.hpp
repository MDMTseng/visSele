#ifndef  acvImage_HeaderIncludeFlag
        #include "acvImage_BasicTool.cpp"
#endif

#define MophologySize    1//(Size*2+1)*(Size*2+1)
#define MophologyDim    (MophologySize*2+1)*(MophologySize*2+1)
void acvDilation(acvImage *OutPic,acvImage *OriPic)       //Max
{
    int i,j,k,l,gg;
    int Max;
    for(i=MophologySize;i<OriPic->GetHeight()-MophologySize;i++)
        for(j=MophologySize;j<OriPic->GetWidth()-MophologySize;j++)
    {

                Max=0;
                for(k=-MophologySize;k<MophologySize+1;k++)for(l=-MophologySize;l<MophologySize+1;l++)
                if(Max<OriPic->CVector[i+k][3*(j+l)])Max=OriPic->CVector[i+k][3*(j+l)];


                OutPic->CVector[i][3*j]=Max;
    }

}
void acvErosion(acvImage *OutPic,acvImage *OriPic)       //Min
{
    int i,j,k,l,gg;
    int Min;
    for(i=MophologySize;i<OriPic->GetHeight()-MophologySize;i++)
        for(j=MophologySize;j<OriPic->GetWidth()-MophologySize;j++)
    {

                Min=255;
                for(k=-MophologySize;k<MophologySize+1;k++)for(l=-MophologySize;l<MophologySize+1;l++)
                if(Min>OriPic->CVector[i+k][3*(j+l)])Min=OriPic->CVector[i+k][3*(j+l)];

                OutPic->CVector[i][3*j]=Min;
    }

}
void acvOpening(acvImage *BuffPic,acvImage *OriPic)
{
     acvErosion(BuffPic,OriPic);
     acvDilation(OriPic,BuffPic);

}

void acvClosing(acvImage *BuffPic,acvImage *OriPic)
{
     acvDilation(BuffPic,OriPic);
     acvErosion(OriPic,BuffPic);

}



#define bMophologySize    5//(Size*2+1)*(Size*2+1)
void acvbDilation(acvImage *BuffPic,acvImage *Pic)       //Max
{
    int i,j,k,TmpSum;


    for(j=0;j<Pic->GetWidth();j++)
    {
        TmpSum=0;
        for(k=1;k<bMophologySize;k++)if(Pic->CVector[k][3*j])
           TmpSum++;

        for(i=0;i<Pic->GetHeight();i++)
        {


                if(i>bMophologySize+1)if(Pic->CVector[i-bMophologySize-1][3*j])
                    TmpSum--;
                if(i+bMophologySize<Pic->GetHeight())if(Pic->CVector[i+bMophologySize][3*j])
                    TmpSum++;


                if(TmpSum)
                        BuffPic->CVector[i][3*j]=255;
                else
                        BuffPic->CVector[i][3*j]=0;
        }
    }

    for(i=0;i<Pic->GetHeight();i++)
    {
        TmpSum=0;
        for(k=1;k<bMophologySize;k++)if(BuffPic->CVector[i][3*(k)])
           TmpSum++;
        for(j=0;j<Pic->GetWidth();j++)
        {


                if(j>bMophologySize+1)if(BuffPic->CVector[i][3*(j-bMophologySize-1)])
                    TmpSum--;
                if(j+bMophologySize<Pic->GetWidth())if(BuffPic->CVector[i][3*(j+bMophologySize)])
                    TmpSum++;


                if(TmpSum)
                        Pic->CVector[i][3*j]=255;
                else
                        Pic->CVector[i][3*j]=0;
        }
    }
}
void acvbDilation(acvImage *BuffPic,acvImage *Pic,int Size)       //Max
{
    int i,j,k,TmpSum;


    for(j=0;j<Pic->GetWidth();j++)
    {
        TmpSum=0;
        for(k=1;k<Size;k++)if(Pic->CVector[k][3*j])
           TmpSum++;

        for(i=0;i<Pic->GetHeight();i++)
        {


                if(i>Size+1)if(Pic->CVector[i-Size-1][3*j])
                    TmpSum--;
                if(i+Size<Pic->GetHeight())if(Pic->CVector[i+Size][3*j])
                    TmpSum++;


                if(TmpSum)
                        BuffPic->CVector[i][3*j]=255;
                else
                        BuffPic->CVector[i][3*j]=0;
        }
    }

    for(i=0;i<Pic->GetHeight();i++)
    {
        TmpSum=0;
        for(k=1;k<Size;k++)if(BuffPic->CVector[i][3*(k)])
           TmpSum++;
        for(j=0;j<Pic->GetWidth();j++)
        {


                if(j>Size+1)if(BuffPic->CVector[i][3*(j-Size-1)])
                    TmpSum--;
                if(j+Size<Pic->GetWidth())if(BuffPic->CVector[i][3*(j+Size)])
                    TmpSum++;


                if(TmpSum)
                        Pic->CVector[i][3*j]=255;
                else
                        Pic->CVector[i][3*j]=0;
        }
    }
}


void acvbErosion(acvImage *BuffPic,acvImage *Pic)       //Min
{
    int i,j,k,TmpSum;


    for(j=0;j<Pic->GetWidth();j++)
    {
        TmpSum=0;
        for(k=1;k<bMophologySize;k++)if(!Pic->CVector[k][3*j])
           TmpSum++;

        for(i=0;i<Pic->GetHeight();i++)
        {


                if(i>bMophologySize+1)if(!Pic->CVector[i-bMophologySize-1][3*j])
                    TmpSum--;
                if(i+bMophologySize<Pic->GetHeight())if(!Pic->CVector[i+bMophologySize][3*j])
                    TmpSum++;


                if(TmpSum)
                        BuffPic->CVector[i][3*j]=0;
                else
                        BuffPic->CVector[i][3*j]=255;
        }
    }

    for(i=0;i<Pic->GetHeight();i++)
    {
        TmpSum=0;
        for(k=1;k<bMophologySize;k++)if(!BuffPic->CVector[i][3*(k)])
           TmpSum++;
        for(j=0;j<Pic->GetWidth();j++)
        {


                if(j>bMophologySize+1)if(!BuffPic->CVector[i][3*(j-bMophologySize-1)])
                    TmpSum--;
                if(j+bMophologySize<Pic->GetWidth())if(!BuffPic->CVector[i][3*(j+bMophologySize)])
                    TmpSum++;


                if(TmpSum)
                        Pic->CVector[i][3*j]=0;
                else
                        Pic->CVector[i][3*j]=255;
        }
    }
}

void acvbErosion(acvImage *BuffPic,acvImage *Pic,int Size)       //Min
{
    int i,j,k,TmpSum;


    for(j=Pic->GetROIOffsetX();j<Pic->GetWidth();j++)
    {
        TmpSum=0;
        for(k=1;k<Size;k++)if(!Pic->CVector[k][3*j])
           TmpSum++;

        for(i=Pic->GetROIOffsetY();i<Pic->GetHeight();i++)
        {


                if(i>Size+1)if(!Pic->CVector[i-Size-1][3*j])
                    TmpSum--;
                if(i+Size<Pic->GetHeight())if(!Pic->CVector[i+Size][3*j])
                    TmpSum++;


                if(TmpSum)
                        BuffPic->CVector[i][3*j]=0;
                else
                        BuffPic->CVector[i][3*j]=255;
        }
    }

    for(i=Pic->GetROIOffsetY();i<Pic->GetHeight();i++)
    {
        TmpSum=0;
        for(k=1;k<Size;k++)if(!BuffPic->CVector[i][3*(k)])
           TmpSum++;
        for(j=Pic->GetROIOffsetX();j<Pic->GetWidth();j++)
        {


                if(j>Size+1)if(!BuffPic->CVector[i][3*(j-Size-1)])
                    TmpSum--;
                if(j+Size<Pic->GetWidth())if(!BuffPic->CVector[i][3*(j+Size)])
                    TmpSum++;


                if(TmpSum)
                        Pic->CVector[i][3*j]=0;
                else
                        Pic->CVector[i][3*j]=255;
        }
    }
}


void acvbOpening(acvImage *BuffPic,acvImage *OriPic)
{
     acvbErosion(BuffPic,OriPic);
     acvbDilation(BuffPic,OriPic);

}

void acvbClosing(acvImage *BuffPic,acvImage *OriPic)
{
     acvbDilation(BuffPic,OriPic);
     acvbErosion(BuffPic,OriPic);

}

void acvImAnd(acvImage *Pic1,acvImage *Pic2)
{
    for(int i=0;i<Pic1->GetHeight();i++)for(int j=0;j<Pic1->GetWidth();j++)
    {
        //if(!Pic1->CVector[i][3*j])Pic2->CVector[i][3*j]=0;
        Pic2->CVector[i][3*j]&=Pic1->CVector[i][3*j];

    }
}

void acvImAndColor(acvImage *Pic1,acvImage *Pic2)
{
    for(int i=0;i<Pic1->GetHeight();i++)for(int j=0;j<Pic1->GetWidth();j++)
    {
        //if(!Pic1->CVector[i][3*j])Pic2->CVector[i][3*j]=0;
        Pic2->CVector[i][3*j]&=Pic1->CVector[i][3*j];
        Pic2->CVector[i][3*j+1]&=Pic1->CVector[i][3*j+1];
        Pic2->CVector[i][3*j+2]&=Pic1->CVector[i][3*j+2];

    }
}

void acvImOr(acvImage *Pic1,acvImage *Pic2)
{
    for(int i=Pic1->GetROIOffsetY();i<Pic1->GetHeight();i++)for(int j=Pic1->GetROIOffsetX();j<Pic1->GetWidth();j++)
    {
        //if(Pic2->CVector[i][3*j])Pic2->CVector[i][3*j]=255;
        Pic2->CVector[i][3*j]|=Pic1->CVector[i][3*j];
    }
}

void acvImNot(acvImage *Pic)
{
    for(int i=Pic->GetROIOffsetY();i<Pic->GetHeight();i++)for(int j=Pic->GetROIOffsetX();j<Pic->GetWidth();j++)
    {
        Pic->CVector[i][3*j]=~Pic->CVector[i][3*j];
    }
}



void acvImXor(acvImage *Pic1,acvImage *Pic2)
{
    for(int i=Pic1->GetROIOffsetY();i<Pic1->GetHeight();i++)for(int j=Pic1->GetROIOffsetX();j<Pic1->GetWidth();j++)
    {
        Pic2->CVector[i][3*j]^=Pic1->CVector[i][3*j];
    }
}
void acvCountXB(int* X,int*B,acvImage *Pic,int i,int j,BYTE Valve)
{
         *X=0;
         *B=8;
         if(Pic->CVector[i-1][3*(j-1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i-1][3*(j)]<Valve)
                *X=*X+1;
         }
         if(Pic->CVector[i-1][3*(j)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i-1][3*(j+1)]<Valve)
                 *X=*X+1;
         }
         if(Pic->CVector[i-1][3*(j+1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i][3*(j+1)]<Valve)
                 *X=*X+1;
         }
         if(Pic->CVector[i][3*(j+1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i+1][3*(j+1)]<Valve)
                 *X=*X+1;
         }
         if(Pic->CVector[i+1][3*(j+1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i+1][3*(j)]<Valve)
                 *X=*X+1;
         }
         if(Pic->CVector[i+1][3*(j)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i+1][3*(j-1)]<Valve)
                 *X=*X+1;
         }
         if(Pic->CVector[i+1][3*(j-1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i][3*(j-1)]<Valve)
                 *X=*X+1;
         }

         if(Pic->CVector[i][3*(j-1)]>=Valve)
         {
                *B=*B-1;
                if(Pic->CVector[i-1][3*(j-1)]<Valve)
                 *X=*X+1;
         }


}


void acvReFresh(acvImage *Pic)
{
    for(int i=0;i<Pic->GetHeight();i++)for(int j=0;j<Pic->GetWidth();j++)
    {
           if(Pic->CVector[i][3*j+1])continue;
           if(Pic->CVector[i][3*j]>0)

                Pic->CVector[i][3*j+2]=255;
           else
                Pic->CVector[i][3*j+2]=0;
           Pic->CVector[i][3*j]=0;
    }
}

void acvZ_S_Skelet(acvImage *Pic)
{
    int i,j,B,X;
    BYTE LevelNum;
    bool   IfChange=1;
    LevelNum=255;
    while(IfChange)
    {IfChange=0;
        if(--LevelNum==2)
        {
                //Threshold(Pic,0);
                acvReFresh(Pic);
                LevelNum=254;
        }
        for(i=1;i<Pic->GetHeight()-1;i++)for(j=1;j<Pic->GetWidth()-1;j++)
        {

                if(Pic->CVector[i][3*j])continue;

                if(Pic->CVector[i][3*(j+1)]>LevelNum||Pic->CVector[i+1][3*j]>LevelNum||
                (Pic->CVector[i-1][3*j]>LevelNum&&Pic->CVector[i][3*(j-1)]>LevelNum))
                        acvCountXB(&X,&B,Pic,i,j,LevelNum+1);
                else   continue;
                if(X==1&&B>1&&B<7)
                {IfChange=1;
                        Pic->CVector[i][3*j]=LevelNum;
                }
        }
        LevelNum--;
        for(i=1;i<Pic->GetHeight()-1;i++)for(j=1;j<Pic->GetWidth()-1;j++)
        {
                if(Pic->CVector[i][3*j])continue;

                if(Pic->CVector[i][3*(j-1)]>LevelNum||Pic->CVector[i-1][3*j]>LevelNum||
                (Pic->CVector[i+1][3*j]>LevelNum&&Pic->CVector[i][3*(j+1)]>LevelNum))
                        acvCountXB(&X,&B,Pic,i,j,LevelNum+1);
                else   continue;
                if(X==1&&B>1&&B<7)
                {IfChange=1;
                        Pic->CVector[i][3*j]=LevelNum;
                }
        }
    }
    acvThreshold(Pic,0);

}

