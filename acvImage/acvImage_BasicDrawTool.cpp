
#include <math.h>
#include <vector>
#include "acvImage_BasicTool.hpp"
#include "acvImage_BasicDrawTool.hpp"



int acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB)
{
       int XVi,YVi,DotNum,i;
       BYTE* Dot;
       //Clip
       if((X1<=0&&X2<=0)||(Y1<=0&&Y2<=0)
       ||(X1>=Pic->GetWidth()&&X2>=Pic->GetWidth())
       ||(Y1>=Pic->GetHeight()&&Y2>=Pic->GetHeight()))
       return 0;

       if(X1<0)
       {
                Y1=Y1-(Y2-Y1)*X1/(X2-X1);
                X1=0;
       }
       if(X2<0)
       {
                Y2=Y2-(Y1-Y2)*X2/(X1-X2);
                X2=0;
       }
       if(Y1<0)
       {
                X1=X1-(X2-X1)*Y1/(Y2-Y1);
                Y1=0;
       }
       if(Y2<0)
       {
                X2=X2-(X1-X2)*Y2/(Y1-Y2);
                Y2=0;
       }


       if(X1>=Pic->GetWidth())
       {

                Y1=Y1+(Y2-Y1)*(X1-Pic->GetWidth())/(X1-X2);
                X1=Pic->GetWidth()-1;
       }
       if(X2>=Pic->GetWidth())
       {
                Y2=Y2+(Y1-Y2)*(X2-Pic->GetWidth())/(X2-X1);
                X2=Pic->GetWidth()-1;
       }
       if(Y1>=Pic->GetHeight())
       {
                X1=X1+(X2-X1)*(Y1-Pic->GetHeight())/(Y1-Y2);
                Y1=Pic->GetHeight()-1;
       }
       if(Y2>=Pic->GetHeight())
       {
                X2=X2+(X1-X2)*(Y2-Pic->GetHeight())/(Y2-Y1);
                Y2=Pic->GetHeight()-1;
       }

       XVi=(X2-X1);
       YVi=(Y2-Y1);
       if(XVi<0)XVi=-XVi;
       if(YVi<0)YVi=-YVi;
       DotNum=(XVi>YVi)? XVi:YVi;


       if(!DotNum)return 0;

       for(i=0;i<=DotNum;i++)
       {
                XVi=(X2-X1)*i/DotNum+X1;
                YVi=(Y2-Y1)*i/DotNum+Y1;

                Dot=(Pic->CVector[YVi]+3*XVi);
                Dot[0]=LineB;
                Dot[1]=LineG;
                Dot[2]=LineR;

       }

       return 1;
}
void acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB,int LineWidth)
{



       acvDrawLine(Pic,X1,Y1,
                        X2,Y2,LineR,LineG,LineB);
       if(LineWidth<=1)return;

       int XVi,YVi,DotNum,i,PriXVi=0,PriYVi=0;
       XVi=(X2-X1);
       YVi=(Y2-Y1);
       if(XVi<0)XVi=-XVi;
       if(YVi<0)YVi=-YVi;
       DotNum=(XVi>YVi)? XVi:YVi;
       if(DotNum==0)return;
       LineWidth>>=1;

       for(i=1;i<LineWidth;i++)
       {

                XVi=(Y2-Y1)*i/DotNum;
                YVi=-(X2-X1)*i/DotNum;
                if(YVi!=PriYVi&&XVi!=PriXVi)
                {

                        acvDrawLine(Pic,X1+PriXVi,Y1+YVi,
                        X2+PriXVi,Y2+YVi,LineR,LineG,LineB);
                        acvDrawLine(Pic,X1-PriXVi,Y1-YVi,
                        X2-PriXVi,Y2-YVi,LineR,LineG,LineB);
                }
                PriXVi=XVi;
                PriYVi=YVi;
                acvDrawLine(Pic,X1+XVi,Y1+YVi,
                X2+XVi,Y2+YVi,LineR,LineG,LineB);

                acvDrawLine(Pic,X1-XVi,Y1-YVi,
                X2-XVi,Y2-YVi,LineR,LineG,LineB);
       }
}
void acvDrawLine_P(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE LineR,BYTE LineG,BYTE LineB,int LineWidth)
{



       acvDrawLine(Pic,X1,Y1,
                        X2,Y2,LineR,LineG,LineB);
       if(LineWidth<=1)return;

       int XVi,YVi,DotNum,i,PriXVi=0,PriYVi=0;
       XVi=(X2-X1);
       YVi=(Y2-Y1);
       if(XVi<0)XVi=-XVi;
       if(YVi<0)YVi=-YVi;
       DotNum=(XVi>YVi)? XVi:YVi;

       LineWidth=LineWidth*(XVi+YVi)/sqrt(XVi*XVi+YVi*YVi)/2;

       for(i=1;i<LineWidth;i++)
       {

                XVi=(Y2-Y1)*i/DotNum;
                YVi=-(X2-X1)*i/DotNum;
                if(YVi!=PriYVi&&XVi!=PriXVi)
                {

                        acvDrawLine(Pic,X1+PriXVi,Y1+YVi,
                        X2+PriXVi,Y2+YVi,LineR,LineG,LineB);
                        acvDrawLine(Pic,X1-PriXVi,Y1-YVi,
                        X2-PriXVi,Y2-YVi,LineR,LineG,LineB);
                }
                PriXVi=XVi;
                PriYVi=YVi;
                acvDrawLine(Pic,X1+XVi,Y1+YVi,
                X2+XVi,Y2+YVi,LineR,LineG,LineB);

                acvDrawLine(Pic,X1-XVi,Y1-YVi,
                X2-XVi,Y2-YVi,LineR,LineG,LineB);
       }
}
void acvDrawDots(acvImage *Pic,double* Dots,int XMin,int XMax,double YMin,double YMax,BYTE LineR,BYTE LineG,BYTE LineB)
{
        int     i,PicWidth,PicHeight,SampleDots;
        int     PriX,NowX;
        double  PriY,NowY;
        double  XRatio,YRatio;
        if(XMax<=XMin||YMax<=YMin)goto Exit;
        Dots+=XMin;
        XMax-=XMin;



        SampleDots=XMax+1;
        PicWidth=Pic->GetWidth();
        PicHeight=Pic->GetHeight();
        YRatio=(double)PicHeight/(YMax-YMin);
        if(SampleDots<PicWidth)
        {
                PriX=0; PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<SampleDots;i++)
                {

                        NowX=i*PicWidth/(SampleDots-1);
                        NowY=PicHeight-YRatio*(Dots[i]-YMin);


                        if(PriY<0)
                        {
                                if(NowY!=PriY&&NowY>-1)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PriY)/(PriY-NowY),0,NowX,NowY,LineR,LineG,LineB);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY!=PriY&&NowY<PicHeight)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PicHeight-PriY)/(NowY-PriY),PicHeight-1,NowX,NowY,LineR,LineG,LineB);
                        }
                        else    acvDrawLine(Pic,PriX,PriY,NowX,NowY,LineR,LineG,LineB);
                        PriX=NowX;
                        PriY=NowY;
                }
        }
        else
        {
                PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<PicWidth;i++)
                {
                        NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                        if(PriY<0)
                        {
                                if(NowY>-1)
                                if(-PriY>NowY)acvDrawLine(Pic,i,0,i,NowY,LineR,LineG,LineB);
                                else          acvDrawLine(Pic,i-1,0,i,NowY,LineR,LineG,LineB);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY<PicHeight)
                                if(PriY-PicHeight>PicHeight-NowY)
                                              acvDrawLine(Pic,i-1,PicHeight-1,i-1,NowY,LineR,LineG,LineB);
                                else          acvDrawLine(Pic,i-1,PicHeight-1,i,NowY,LineR,LineG,LineB);
                        }
                        else    acvDrawLine(Pic,i-1,PriY,i,NowY,LineR,LineG,LineB);
                        PriY=NowY;
                }
                NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                if(PriY<0)PriY=0;
                else if(PriY>=PicHeight)PriY=PicHeight-1;
                acvDrawLine(Pic,i-1,PriY,i,NowY,LineR,LineG,LineB);
        }
        Exit:;
}


void acvDrawDots(acvImage *Pic,int* Dots,int XMin,int XMax,double YMin,double YMax,BYTE LineR,BYTE LineG,BYTE LineB)
{
        int     i,PicWidth,PicHeight,SampleDots;
        int     PriX,NowX;
        double  PriY,NowY;
        double  XRatio,YRatio;
        if(XMax<=XMin||YMax<=YMin)goto Exit;
        Dots+=XMin;
        XMax-=XMin;



        SampleDots=XMax+1;
        PicWidth=Pic->GetWidth();
        PicHeight=Pic->GetHeight();
        YRatio=(double)PicHeight/(YMax-YMin);
        if(SampleDots<PicWidth)
        {
                PriX=0; PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<SampleDots;i++)
                {

                        NowX=i*PicWidth/(SampleDots-1);
                        NowY=PicHeight-YRatio*(Dots[i]-YMin);


                        if(PriY<0)
                        {
                                if(NowY!=PriY&&NowY>-1)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PriY)/(PriY-NowY),0,NowX,NowY,LineR,LineG,LineB);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY!=PriY&&NowY<PicHeight)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PicHeight-PriY)/(NowY-PriY),PicHeight-1,NowX,NowY,LineR,LineG,LineB);
                        }
                        else    acvDrawLine(Pic,PriX,PriY,NowX,NowY,LineR,LineG,LineB);
                        PriX=NowX;
                        PriY=NowY;
                }
        }
        else
        {
                PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<PicWidth;i++)
                {
                        NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                        if(PriY<0)
                        {
                                if(NowY>-1)
                                if(-PriY>NowY)acvDrawLine(Pic,i,0,i,NowY,LineR,LineG,LineB);
                                else          acvDrawLine(Pic,i-1,0,i,NowY,LineR,LineG,LineB);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY<PicHeight)
                                if(PriY-PicHeight>PicHeight-NowY)
                                              acvDrawLine(Pic,i-1,PicHeight-1,i-1,NowY,LineR,LineG,LineB);
                                else          acvDrawLine(Pic,i-1,PicHeight-1,i,NowY,LineR,LineG,LineB);
                        }
                        else    acvDrawLine(Pic,i-1,PriY,i,NowY,LineR,LineG,LineB);
                        PriY=NowY;
                }
                NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                if(PriY<0)PriY=0;
                else if(PriY>=PicHeight)PriY=PicHeight-1;
                acvDrawLine(Pic,i-1,PriY,i,NowY,LineR,LineG,LineB);
        }
        Exit:;
}




int acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2)
{
       int XVi,YVi,DotNum,i;
       BYTE* Dot;
       //Clip
       if((X1<=0&&X2<=0)||(Y1<=0&&Y2<=0)
       ||(X1>=Pic->GetWidth()&&X2>=Pic->GetWidth())
       ||(Y1>=Pic->GetHeight()&&Y2>=Pic->GetHeight()))
       return 0;

       if(X1<0)
       {
                Y1=Y1-(Y2-Y1)*X1/(X2-X1);
                X1=0;
       }
       if(X2<0)
       {
                Y2=Y2-(Y1-Y2)*X2/(X1-X2);
                X2=0;
       }
       if(Y1<0)
       {
                X1=X1-(X2-X1)*Y1/(Y2-Y1);
                Y1=0;
       }
       if(Y2<0)
       {
                X2=X2-(X1-X2)*Y2/(Y1-Y2);
                Y2=0;
       }


       if(X1>=Pic->GetWidth())
       {

                Y1=Y1+(Y2-Y1)*(X1-Pic->GetWidth())/(X1-X2);
                X1=Pic->GetWidth()-1;
       }
       if(X2>=Pic->GetWidth())
       {
                Y2=Y2+(Y1-Y2)*(X2-Pic->GetWidth())/(X2-X1);
                X2=Pic->GetWidth()-1;
       }
       if(Y1>=Pic->GetHeight())
       {
                X1=X1+(X2-X1)*(Y1-Pic->GetHeight())/(Y1-Y2);
                Y1=Pic->GetHeight()-1;
       }
       if(Y2>=Pic->GetHeight())
       {
                X2=X2+(X1-X2)*(Y2-Pic->GetHeight())/(Y2-Y1);
                Y2=Pic->GetHeight()-1;
       }
       XVi=(X2-X1);
       YVi=(Y2-Y1);
       if(XVi<0)XVi=-XVi;
       if(YVi<0)YVi=-YVi;
       DotNum=(XVi>YVi)? XVi:YVi;


       if(!DotNum)return 0;

       for(i=0;i<=DotNum;i++)
       {
                XVi=(X2-X1)*i/DotNum+X1;
                YVi=(Y2-Y1)*i/DotNum+Y1;

                Dot=(Pic->CVector[YVi]+3*XVi);
                Dot[0]=~Dot[0];
                Dot[1]=~Dot[1];
                Dot[2]=~Dot[2];

       }

       return 1;
}

void acvDrawLine(acvImage *Pic,int X1,int Y1,int X2,int Y2,int LineWidth)
{
       acvDrawLine(Pic,X1,Y1,
                        X2,Y2);
       if(LineWidth<=1)return;

       int XVi,YVi,DotNum,i,PriXVi=0,PriYVi=0;
       XVi=(X2-X1);
       YVi=(Y2-Y1);
       if(XVi<0)XVi=-XVi;
       if(YVi<0)YVi=-YVi;
       DotNum=(XVi>YVi)? XVi:YVi;

       LineWidth>>=1;
       for(i=1;i<LineWidth;i++)
       {

                XVi=(Y2-Y1)*i/DotNum;
                YVi=-(X2-X1)*i/DotNum;
                if(YVi!=PriYVi&&XVi!=PriXVi)
                {

                        acvDrawLine(Pic,X1+PriXVi,Y1+YVi,
                        X2+PriXVi,Y2+YVi);
                        acvDrawLine(Pic,X1-PriXVi,Y1-YVi,
                        X2-PriXVi,Y2-YVi);
                }
                PriXVi=XVi;
                PriYVi=YVi;
                acvDrawLine(Pic,X1+XVi,Y1+YVi,
                X2+XVi,Y2+YVi);

                acvDrawLine(Pic,X1-XVi,Y1-YVi,
                X2-XVi,Y2-YVi);
       }


}


void acvDrawDots(acvImage *Pic,const double* Dots,int XMin,int XMax,double YMin,double YMax)
{
        int     i,PicWidth,PicHeight,SampleDots;
        int     PriX,NowX;
        double  PriY,NowY;
        double  XRatio,YRatio;
        if(XMax<=XMin||YMax<=YMin)goto Exit;
        Dots+=XMin;
        XMax-=XMin;



        SampleDots=XMax+1;
        PicWidth=Pic->GetWidth();
        PicHeight=Pic->GetHeight();
        YRatio=(double)PicHeight/(YMax-YMin);
        if(SampleDots<PicWidth)
        {
                PriX=0; PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<SampleDots;i++)
                {

                        NowX=i*PicWidth/(SampleDots);
                        NowY=PicHeight-YRatio*(Dots[i]-YMin);


                        if(PriY<0)
                        {
                                if(NowY!=PriY&&NowY>-1)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PriY)/(PriY-NowY),0,NowX,NowY);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY!=PriY&&NowY<PicHeight)
                                        acvDrawLine(Pic,PriX+(NowX-PriX)*(PicHeight-PriY)/(NowY-PriY),PicHeight-1,NowX,NowY);
                        }
                        else    acvDrawLine(Pic,PriX,PriY,NowX,NowY);
                        PriX=NowX;
                        PriY=NowY;
                }
        }
        else
        {
                PriY=PicHeight-YRatio*(Dots[0]-YMin);
                for(i=1;i<PicWidth;i++)
                {
                        NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                        if(PriY<0)
                        {
                                if(NowY>-1)
                                if(-PriY>NowY)acvDrawLine(Pic,i,0,i,NowY);
                                else          acvDrawLine(Pic,i-1,0,i,NowY);
                        }
                        else if(PriY>=PicHeight)
                        {
                                if(NowY<PicHeight)
                                if(PriY-PicHeight>PicHeight-NowY)
                                              acvDrawLine(Pic,i-1,PicHeight-1,i-1,NowY);
                                else          acvDrawLine(Pic,i-1,PicHeight-1,i,NowY);
                        }
                        else    acvDrawLine(Pic,i-1,PriY,i,NowY);
                        PriY=NowY;
                }
                NowY=PicHeight-YRatio*(Dots[i*SampleDots/PicWidth]-YMin);
                if(PriY<0)PriY=0;
                else if(PriY>=PicHeight)PriY=PicHeight-1;
                acvDrawLine(Pic,i-1,PriY,i,NowY);
        }
        Exit:;
}



void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize)
{
        acvDrawLine(Pic,X+CrossSize,Y+CrossSize,X-CrossSize,Y-CrossSize);
        acvDrawLine(Pic,X-CrossSize,Y+CrossSize,X+CrossSize,Y-CrossSize);
}
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,int LineWidth)
{
        acvDrawLine(Pic,X+CrossSize,Y+CrossSize,X-CrossSize,Y-CrossSize,LineWidth);
        acvDrawLine(Pic,X-CrossSize,Y+CrossSize,X+CrossSize,Y-CrossSize,LineWidth);
}
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B)
{
        acvDrawLine(Pic,X+CrossSize,Y+CrossSize,X-CrossSize,Y-CrossSize,R,G,B);
        acvDrawLine(Pic,X-CrossSize,Y+CrossSize,X+CrossSize,Y-CrossSize,R,G,B);
}
void acvDrawCrossX(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B,int LineWidth)
{
        acvDrawLine(Pic,X+CrossSize,Y+CrossSize,X-CrossSize,Y-CrossSize,R,G,B,LineWidth);
        acvDrawLine(Pic,X-CrossSize,Y+CrossSize,X+CrossSize,Y-CrossSize,R,G,B,LineWidth);
}


void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize)
{
        acvDrawLine(Pic,X,Y+CrossSize,X,Y-CrossSize);
        acvDrawLine(Pic,X-CrossSize,Y,X+CrossSize,Y);
}
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,int LineWidth)
{
        acvDrawLine(Pic,X,Y+CrossSize,X,Y-CrossSize,LineWidth);
        acvDrawLine(Pic,X-CrossSize,Y,X+CrossSize,Y,LineWidth);
}
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B)
{
        acvDrawLine(Pic,X,Y+CrossSize,X,Y-CrossSize,R,G,B);
        acvDrawLine(Pic,X-CrossSize,Y,X+CrossSize,Y,R,G,B);
}
void acvDrawCross(acvImage *Pic,int X,int Y,int CrossSize,BYTE R,BYTE G,BYTE B,int LineWidth)
{
        acvDrawLine(Pic,X,Y+CrossSize,X,Y-CrossSize,R,G,B,LineWidth);
        acvDrawLine(Pic,X-CrossSize,Y,X+CrossSize,Y,R,G,B,LineWidth);
}

void acvDrawBlock(acvImage *Pic,int X1,int Y1,int X2,int Y2,BYTE R,BYTE G,BYTE B,int LineWidth)
{
        acvDrawLine(Pic,X1,Y1,X1,Y2,R,G,B,LineWidth);
        acvDrawLine(Pic,X2,Y1,X2,Y2,R,G,B,LineWidth);
        acvDrawLine(Pic,X1,Y1,X2,Y1,R,G,B,LineWidth);
        acvDrawLine(Pic,X1,Y2,X2,Y2,R,G,B,LineWidth);
}
void acvDrawBlock(acvImage *Pic,int X1,int Y1,int X2,int Y2)
{
        acvDrawBlock(Pic, X1,Y1,X2,Y2,0,0,0,1);
}
void acvDrawReverseFillBlock(acvImage *Pic,int X1,int Y1,int X2,int Y2)
{
        if(Y1>Y2)for(int i=Y2;i<Y1;i++)
        {

                acvDrawLine(Pic,X1,i,X2,i);

        }
        else     for(int i=Y1;i<Y2;i++)
        {
                acvDrawLine(Pic,X1,i,X2,i);
        }
        /*DrawLine(Pic,X1,Y1,X1,Y2);
        DrawLine(Pic,X2,Y1,X2,Y2);
        DrawLine(Pic,X1,Y1,X2,Y1);
        DrawLine(Pic,X1,Y2,X2,Y2);*/
}
