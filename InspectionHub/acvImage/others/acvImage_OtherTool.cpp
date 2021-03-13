#ifndef  acvImage_HeaderIncludeFlag
        #include "acvImage_BasicTool.cpp"
#endif



typedef struct  ExpandParameter
{

        int Dot[4][2];
        int YVector[2][2];
        int XC1,XC2;  //constant    C1=(AlphaN-AlphaD);   C2=(Width/Height)*AlphaN;
        int YC1,YC2;  //  AlphaN=(L1>L2)? L2:L1;      AlphaN=L1-L2;
        int XN,XD,YN,YD;
        //  LW1 ____
        //LH1  /   /  LH2
        //    /___/
        //    LW2
}ExpandParameter;



void acvExpandPreProcess(acvImage *Mapping2Pic,ExpandParameter *EP)
{
       int L1,L2;     //H

       EP->YVector[0][0]=EP->Dot[3][0]-EP->Dot[0][0];
       EP->YVector[0][1]=EP->Dot[3][1]-EP->Dot[0][1];

       EP->YVector[1][0]=EP->Dot[2][0]-EP->Dot[1][0];
       EP->YVector[1][1]=EP->Dot[2][1]-EP->Dot[1][1];

       
       L1=hypot(EP->YVector[0][0],EP->YVector[0][1]);
       L2=hypot(EP->YVector[1][0],EP->YVector[1][1]);


       /*
       EP->XN=((L1>L2)?  L2:L1);

       EP->XD=(L2-L1);  */

       EP->XN=(L2+L1)/1.5;

       EP->XD=(L2-L1);



       ///if(EP->YD<0)EP->XN+=-EP->XD;

       if(EP->XD==0)
       {
                EP->XN=10000;
                EP->XD=1;
       }/*
       if(EP->XD*EP->XD>EP->XN*EP->XN)
       {
                EP->XN=100;
                EP->XD=99;
       }  */
       EP->XC1=EP->XN-EP->XD;
       EP->XC2=Mapping2Pic->GetWidth()*EP->XN;

       L1=hypot(EP->Dot[3][0]-EP->Dot[2][0],EP->Dot[3][1]-EP->Dot[2][1]);
       L2=hypot(EP->Dot[0][0]-EP->Dot[1][0],EP->Dot[0][1]-EP->Dot[1][1]);
       /*
       EP->YN=((L1>L2)?  L2:L1);

       EP->YD=(L1-L2);  */
       EP->YN=(L2+L1)/1.5;

       EP->YD=(L1-L2);

       if(EP->YD==0)
       {
                EP->YN=1000;
                EP->YD=1;
       }
       /*
       if(EP->YD*EP->YD>EP->YN*EP->YN)
       {
                EP->YN=100;
                EP->YD=99;
       } */


       EP->YC1=EP->YN-EP->YD;
       EP->YC2=Mapping2Pic->GetHeight()*EP->YN;


}


void acvExpand(acvImage *OutPic,acvImage *OriPic,ExpandParameter *EP)
{
        double HCoordinate1[2],HCoordinate2[2];
        int XN,XD,YN,YD;
        int PicHeight=OutPic->GetHeight(),PicWidth=OutPic->GetWidth();
        int MappingCoordinate[2];

        for(int i=0;i<PicHeight;i++)
        {

                YN=EP->YC1*i;
                YD=EP->YC2-EP->YD*i;


                HCoordinate1[0]=EP->YVector[0][0]*YN/YD+EP->Dot[0][0];
                HCoordinate1[1]=EP->YVector[0][1]*YN/YD+EP->Dot[0][1];

                HCoordinate2[0]=EP->YVector[1][0]*YN/YD+EP->Dot[1][0];
                HCoordinate2[1]=EP->YVector[1][1]*YN/YD+EP->Dot[1][1];



                for(int j=0;j<PicWidth;j++)
                {
                        XN=EP->XC1*j;
                        XD=EP->XC2-EP->XD*j;
                        MappingCoordinate[0]=(int)(( HCoordinate2[1]-HCoordinate1[1])*XN/XD+HCoordinate1[1]);
                        MappingCoordinate[1]=(int)(((HCoordinate2[0]-HCoordinate1[0])*XN/XD+HCoordinate1[0]))*3;
                        if(MappingCoordinate[0]<OriPic->GetHeight() && MappingCoordinate[1]<OriPic->GetWidth()*3 &&
                           MappingCoordinate[0]>=0  &&  MappingCoordinate[1]>=0   )
                        {
                                OutPic->CVector[i][3*j+2]=OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]+2];
                                OutPic->CVector[i][3*j+1]=OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]+1];
                                OutPic->CVector[i][3*j]  =OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]];
                        }
                        else
                        {
                                OutPic->CVector[i][3*j+2]=OutPic->CVector[i][3*j+1]=OutPic->CVector[i][3*j+0]=0;
                        }

                }
        }
}



void acvExpandSimple(acvImage *OutPic,acvImage *OriPic,ExpandParameter *EP)
{
        int HCoordinate1[2],HCoordinate2[2];
        int XN,XD,YN,YD;
        int PicHeight=OutPic->GetHeight(),PicWidth=OutPic->GetWidth();
        int MappingCoordinate[2];





        for(int i=0;i<PicHeight;i++)
        {


                
                HCoordinate1[0]=DoubleRoundInt((double)EP->YVector[0][0]*i/(PicHeight-1)+EP->Dot[0][0]);
                HCoordinate1[1]=DoubleRoundInt((double)EP->YVector[0][1]*i/(PicHeight-1)+EP->Dot[0][1]);

                HCoordinate2[0]=DoubleRoundInt((double)EP->YVector[1][0]*i/(PicHeight-1)+EP->Dot[1][0]);
                HCoordinate2[1]=DoubleRoundInt((double)EP->YVector[1][1]*i/(PicHeight-1)+EP->Dot[1][1]);


                for(int j=0;j<PicWidth;j++)
                {
                        MappingCoordinate[0]=DoubleRoundInt((double)( HCoordinate2[1]-HCoordinate1[1])*j/(PicWidth-1)+HCoordinate1[1]);
                        MappingCoordinate[1]=DoubleRoundInt((double)((HCoordinate2[0]-HCoordinate1[0])*j/(PicWidth-1)+HCoordinate1[0])*3);

                        if(MappingCoordinate[0]<OriPic->GetHeight() && MappingCoordinate[1]<OriPic->GetWidth()*3 &&
                           MappingCoordinate[0]>=0  &&  MappingCoordinate[1]>=0   )
                        {
                                OutPic->CVector[i][3*j+2]=OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]+2];
                                OutPic->CVector[i][3*j+1]=OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]+1];
                                OutPic->CVector[i][3*j]  =OriPic->CVector[MappingCoordinate[0]][MappingCoordinate[1]];
                        }
                        else
                        {
                                OutPic->CVector[i][3*j+2]=OutPic->CVector[i][3*j+1]=OutPic->CVector[i][3*j+0]=0;
                        }


                }
        }
}

void acvExpandRect(acvImage *OutPic,acvImage *OriPic,int Top,int Left,int Width,int Height)
{
        int OutPicHeight=OutPic->GetHeight(),OutPicWidth=OutPic->GetWidth()
            ,i,j,Tmp;

        BYTE *OutLine,*OriLine;




        for(i=0;i<OutPicHeight;i++)
        {
               OutLine=OutPic->CVector[i];
               OriLine =OriPic->CVector[i*Height/OutPicHeight+Top]+Left;


                for(j=0;j<OutPicWidth;j++)
                {

                        Tmp=j*Width/OutPicWidth;
                        Tmp*=3;
                        *OutLine++=OriLine[Tmp];
                        *OutLine++=OriLine[Tmp+1];
                        *OutLine++=OriLine[Tmp+2];





                }
        }
}

void acvExpandRect(acvImage *OutPic,acvImage *OriPic)
{
        int OutPicHeight=OutPic->GetHeight(),OutPicWidth=OutPic->GetWidth(),
            InPicHeight=OriPic->GetHeight(),InPicWidth=OriPic->GetWidth()
            ,i,j,Tmp;

        BYTE *OutLine,*OriLine;




        for(i=0;i<OutPicHeight;i++)
        {
               OutLine=OutPic->CVector[i];
               OriLine=OriPic ->CVector[i*InPicHeight/OutPicHeight];


                for(j=0;j<OutPicWidth;j++)
                {
                        Tmp=j*InPicWidth/OutPicWidth;
                        Tmp*=3;
                        *OutLine++=OriLine[Tmp];
                        *OutLine++=OriLine[Tmp+1];
                        *OutLine++=OriLine[Tmp+2];






                }
        }
}






#define f1Func(r) -0.13767*r*r+1.0743*r+0.1452
#define f2Func(r)   -0.776*r*r+0.5601*r+0.1766
#define  WFunc(r,g)   (r-0.33)*(r-0.33)+(r-0.33)*(r-0.33)
void acvSkinRangeFormRGB(acvImage *Pic)
{

    BYTE* BMPLine;
    int   RGBSum;
    double f1,f2,W,r,g;

    for(int i=0;i<Pic->GetHeight();i++)
    {
        BMPLine=Pic->CVector[i];
        for(int j=0;j<Pic->GetWidth();j++)
        {
               RGBSum=BMPLine[0]+BMPLine[1]+BMPLine[2];
               if(!RGBSum)
               {
                        *BMPLine++=0;
                        *BMPLine++=0;
                        *BMPLine++=0;
                        continue;
               }

               r=(double)BMPLine[2]/RGBSum;

               if(r<0.2||r>0.6)
               {
                        *BMPLine++=0;
                        *BMPLine++=0;
                        *BMPLine++=0;
                        continue;
               }

               g=(double)BMPLine[1]/RGBSum;

               f1=f1Func(r);
               f2=f2Func(r);
               W=WFunc(r,g);

               if(r>0.2&&r<0.6&&g<f1&&g>f2&&W>0.0004)
               {
                        *BMPLine++=255;
                        *BMPLine++=255;
                        *BMPLine++=255;
               }
               else
               {
                        *BMPLine++=0;
                        *BMPLine++=0;
                        *BMPLine++=0;
               }


        }

    }
}
void acvSkinRangeFormNCC(acvImage *Pic)
{

    BYTE* BMPLine;
    double f1,f2,W,r,g;

    for(int i=0;i<Pic->GetHeight();i++)
    {
        BMPLine=Pic->CVector[i];
        for(int j=0;j<Pic->GetWidth();j++)
        {
               r=(double)BMPLine[2]/255.;
               if(r<0.2||r>0.6)
               {
                        *BMPLine++=0;
                        *BMPLine++=0;
                        *BMPLine++=0;
                        continue;
               }

               g=(double)BMPLine[1]/255.;

               f1=f1Func(r);
               f2=f2Func(r);
               W=WFunc(r,g);

               if(r>0.2&&r<0.6&&g<f1&&g>f2&&W>0.0004)
               {
                        *BMPLine++=255;
                        *BMPLine++=255;
                        *BMPLine++=255;
               }
               else
               {
                        *BMPLine++=0;
                        *BMPLine++=0;
                        *BMPLine++=0;
               }


        }

    }
}




void acvHSVImSub(acvImage *Pic1,acvImage *Pic2)           //0V ~255  1S ~255  2H ~252
{
        int TmpPixel;

        for(int i=0;i<Pic1->GetHeight();i++)for(int j=0;j<Pic1->GetWidth();j++)
        {

                TmpPixel=Pic1->CVector[i][3*j]-Pic2->CVector[i][3*j];    //V
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                Pic2->CVector[i][3*j]=TmpPixel;

                TmpPixel=Pic1->CVector[i][3*j+2]-Pic2->CVector[i][3*j+2]; //S
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                Pic2->CVector[i][3*j+2]=TmpPixel;

                TmpPixel=Pic1->CVector[i][3*j+1]-Pic2->CVector[i][3*j+1]; //H
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                if(TmpPixel>126)TmpPixel=-126;
                Pic2->CVector[i][3*j+1]=TmpPixel;

        }

}


/*
        ==>Normalize form 0~1
        f1 (r)=-0.13767*r^2+1.0743*r+0.1452

        f2 (r)=  -0.776*r^2+0.5601*r+0.1766

        W(r,g)=(r-0.33)^2+(r-0.33)^2

        SkinColor=(  g<f1  &&  g>f2  &&  W>0.0004  )

        ==>Normalize form 0~255
        e1 (r)=-0.13767*r^2    +1.0743*r*255+0.1452 *255*255         e1=f1*255*255

        e2 (r)=  -0.776*r^2    +0.5601*r*255+0.1766 *255*255         e2=f2*255*255

        X(r,g)=((r-0.33*255)^2+(r-0.33*255)^2)                       X= W*255*255

        SkinColor=(  255*g<f1  &&  255*g>f2  &&  W>0.0004*255*255  )
*/




void acvImError(acvImage *Pic1,acvImage *Pic2)           //0V ~255  1S ~255  2H ~252
{
        int TmpPixel,Tmp;
        BYTE *Pic1Line,*Pic2Line;
        for(int i=0;i<Pic1->GetHeight();i++)
        {
                Pic1Line=Pic1->CVector[i];
                Pic2Line=Pic2->CVector[i];
                for(int j=0;j<Pic1->GetWidth();j++)
                {
                        Tmp=Pic1Line[0] - Pic2Line[0];
                        TmpPixel=(Tmp<0)? -Tmp:Tmp;

                        Tmp=Pic1Line[1] - Pic2Line[1];
                        if(Tmp<0)Tmp=-Tmp;
                        if(TmpPixel<Tmp)TmpPixel=Tmp;

                        Tmp=Pic1Line[2] - Pic2Line[2];
                        if(Tmp<0)Tmp=-Tmp;
                        if(TmpPixel<Tmp)TmpPixel=Tmp;


                        *Pic2Line++=TmpPixel;
                        *Pic2Line++=TmpPixel;
                        *Pic2Line++=TmpPixel;
                        Pic1Line+=3;


                }
        }

}
/*
void acvImError(acvImage *Pic1,acvImage *Pic2)           //0V ~255  1S ~255  2H ~252
{
        int TmpPixel,Tmp;

        for(int i=0;i<Pic1->GetHeight();i++)for(int j=0;j<Pic1->GetWidth();j++)
        {
                Tmp=Pic1->CVector[i][3*j]-Pic2->CVector[i][3*j];
                TmpPixel=(Tmp<0)? -Tmp:Tmp;

                Tmp=Pic1->CVector[i][3*j+1]-Pic2->CVector[i][3*j+1];
                TmpPixel+=(Tmp<0)? -Tmp:Tmp;

                Tmp=Pic1->CVector[i][3*j+2]-Pic2->CVector[i][3*j+2];
                TmpPixel+=(Tmp<0)? -Tmp:Tmp;

                if(TmpPixel>255)
                        TmpPixel=255;

                Pic2->CVector[i][3*j]=
                Pic2->CVector[i][3*j+1]=
                Pic2->CVector[i][3*j+2]=TmpPixel;

        }

}   */




void acvImSub(acvImage *Pic1,acvImage *Pic2)
{
        int TmpPixel;

        for(int i=0;i<Pic1->GetHeight();i++)for(int j=0;j<Pic1->GetWidth();j++)
        {

                TmpPixel=Pic1->CVector[i][3*j]-Pic2->CVector[i][3*j];
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                //if(TmpPixel<0)TmpPixel=0;
                //if(TmpPixel<50)Pic->CVector[i][3*j]=0;
                Pic2->CVector[i][3*j]=TmpPixel;

                TmpPixel=Pic1->CVector[i][3*j+2]-Pic2->CVector[i][3*j+2];
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                //if(TmpPixel<50)Pic->CVector[i][3*j]=0;
                //if(TmpPixel<0)TmpPixel=0;
                Pic2->CVector[i][3*j+2]=TmpPixel;

                TmpPixel=Pic1->CVector[i][3*j+1]-Pic2->CVector[i][3*j+1];
                if(TmpPixel<0)TmpPixel=-TmpPixel;
                //if(TmpPixel<50)Pic->CVector[i][3*j]=0;
                //if(TmpPixel<0)TmpPixel=0;
                Pic2->CVector[i][3*j+1]=TmpPixel;

        }


}







