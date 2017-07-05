#ifndef ACV_IMG_H
#define ACV_IMG_H

#define S_RGB 0
#define S_HSV 1




#define HSV_V  0
#define HSV_S  1
#define HSV_H  2
#define HSV_HMax 251
#define HSV_SMax 255
#define HSV_VMax 255


#define RGB_B  0
#define RGB_G  1
#define RGB_R  2

#define NCC_g  1
#define NCC_r  2
#define BYTE  unsigned char




class acvImage
{

        private:
                 unsigned char ColorType;
                 BYTE*   ImLine;
                int ROIWidth,ROIHeight,RealWidth,RealHeight;
                int ROIOffsetX,ROIOffsetY;
        public :
                int Channel;
                BYTE    *ImageData;
                BYTE    **CVector;
        acvImage()
        {
                VarInit();
        }
        acvImage(int SetWidth,int SetHeight,int SetChannel)
        {
                VarInit();
                Channel=SetChannel;
                RESIZE(SetWidth,SetHeight);
        }
        void VarInit(void)
        {
                CVector=NULL;
                ImageData=0;
                ColorType=S_RGB;
                ROIWidth=ROIHeight
                =RealWidth=RealHeight=
                ROIOffsetX=ROIOffsetY=0;
                Channel=3;
        }

        void ReSize(int SetWidth,int SetHeight)
        {
                RESIZE(SetWidth,SetHeight);
        }
        void SetROI(int SetOffsetX,int SetOffsetY,int SetWidth,int SetHeight)
        {
                //if(!Bitmap)Bitmap->FreeImage();
                //RESIZE(SetWidth,SetHeight);
                if(SetOffsetX+SetWidth>RealWidth||SetOffsetY+SetHeight>RealHeight)
                {
                     //ReSize(ROIOffsetX+SetWidth,ROIOffsetY+SetHeight);
                }

                ROIWidth=SetWidth;
                ROIHeight=SetHeight;
                ROIOffsetX=SetOffsetX;
                ROIOffsetY=SetOffsetY;


        }
        void ReSetROI()
        {
                ROIWidth=RealWidth;
                ROIHeight=RealHeight;
                ROIOffsetX=0;
                ROIOffsetY=0;


        }

        void FreeImage()
        {
                if(ImageData)
                {
                        delete(ImageData);
                        delete(CVector);

                        ImageData=NULL;
                        CVector=NULL;
                }
        }
        private:

        void RESIZE(int SetWidth,int SetHeight)
        {
                RealWidth=ROIWidth=SetWidth;
                RealHeight=ROIHeight=SetHeight;
                FreeImage();
                ImageData=new BYTE[SetWidth*SetHeight*Channel];
                CVector=new BYTE* [SetHeight];
                CVector[0]=ImageData;
                for(int i=1;i<SetHeight;i++)
                {
                        CVector[i]=CVector[i-1]+SetWidth*Channel;
                }

        }

        public:
        void YUY2ToYUV()
        {
                int i,j;
                int hWidth=ROIWidth>>1;

                for(i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(j=ROIOffsetX;j<hWidth;j++)
                        {
                             ImLine[5]=ImLine[1];
                             ImLine[2]=ImLine[4];
                             ImLine+=6;

                        }
                }

        }
        void YUY2ToRGB()
        {
                int i,j,RUV,GUV,BUV,TmpR,TmpG,TmpB;
                BYTE Y1,Y2;
                int U,V;
                int hWidth=ROIHeight>>1;

                for(i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(j=ROIOffsetX;j<hWidth;j++)
                        {
                             Y1=ImLine[0];

                             U =(int)ImLine[1]-128;

                             Y2=ImLine[3];

                             V =(int)ImLine[4]-128;

                             /*
                             RUV=V*13/10;
                             GUV=-(U*3+V*7)/10;
                             BUV=U*17/10; */
                             RUV=V*1.370705;
                             GUV=-(U*0.337633+V*0.698);
                             BUV=U*1.732446;


                             TmpR=Y1+RUV;
                             TmpG=Y1+GUV;
                             TmpB=Y1+BUV;

                             if(TmpR&0x800)TmpR=0;
                             else if(TmpR&0x300)TmpR=0xff;

                             if(TmpG&0x800)TmpG=0;
                             else if(TmpG&0x300)TmpG=0xff;

                             if(TmpB&0x800)TmpB=0;
                             else if(TmpB&0x300)TmpB=0xff;



                             *(ImLine++)=TmpB;
                             *(ImLine++)=TmpG;
                             *(ImLine++)=TmpR;

                             TmpR=Y2+RUV;
                             TmpG=Y2+GUV;
                             TmpB=Y2+BUV;

                             if(TmpR&0x800)TmpR=0;
                             else if(TmpR&0x300)TmpR=0xff;

                             if(TmpG&0x800)TmpG=0;
                             else if(TmpG&0x300)TmpG=0xff;

                             if(TmpB&0x800)TmpB=0;
                             else if(TmpB&0x300)TmpB=0xff;


                             *(ImLine++)=TmpB;
                             *(ImLine++)=TmpG;
                             *(ImLine++)=TmpR;
                        }
                }
        }
        void YUY2ToGray()
        {
                for(int i=ROIOffsetY;i<ROIHeight;i++)
                {ImLine=CVector[i];
                        for(int j=ROIOffsetX;j<ROIWidth;j++)
                        {
                               ImLine[1]=ImLine[2]=ImLine[0];
                               ImLine+=3;
                        }
                }
        }



        void RGBToHSV()
        {
                ColorType=S_HSV;
                for(int i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(int j=ROIOffsetX;j<ROIWidth;j++)
                        {
                                HSVFromRGB(ImLine,ImLine);
                                ImLine+=3;
                        }
                }
        }

        void HSVToRGB()
        {
                ColorType=S_RGB;
                for(int i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(int j=ROIOffsetX;j<ROIWidth;j++)
                        {
                                RGBFromHSV(ImLine,ImLine);
                                ImLine+=3;
                        }
                }
        }

        void RGBToGray()
        {
                for(int i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(int j=ROIOffsetX;j<ROIWidth;j++)
                        {

                               ImLine[0]=ImLine[1]=ImLine[2]=
                                        (ImLine[0]+6*ImLine[1]+3*ImLine[2])/10;
                               ImLine+=3;
                        }
                }
        }

        void RGBToEvenGray()
        {
                for(int i=ROIOffsetY;i<ROIHeight;i++)
                {
                        ImLine=CVector[i];
                        for(int j=ROIOffsetX;j<ROIWidth;j++)
                        {
                               ImLine[0]=ImLine[1]=ImLine[2]=
                                        (ImLine[0]+ImLine[1]+ImLine[2])/3;
                               ImLine+=3;
                        }
                }
        }


        #define InDataR  InData[2]
        #define InDataG  InData[1]
        #define InDataB  InData[0]

        static void HSVFromRGB(BYTE* OutData,BYTE* InData)
        {
                //0 V ~255
                //1 S ~255
                //2 H ~251
                BYTE Mod,Max,Min,D1,D2;
                Max=Min=InDataR;
                D1=InDataG;D2=InDataB;
                Mod=6;
                if(InDataG>Max)
                {
                        Max=InDataG;Mod=2;       //
                        D1=InDataB;D2=InDataR;
                }
                else
                {
                        Min=InDataG;

                }

                if(InDataB>Max)
                {
                        Max=InDataB;Mod=4;
                        D1=InDataR;D2=InDataG;
                }
                else if(InDataB<Min)
                {
                        Min=InDataB;

                }

                OutData[0]=Max;
                if(Max==0)
                {
                        OutData[1]=
                        OutData[2]=0;
                        goto Exit;
                }
                else
                        OutData[1]=255-Min*255/Max;
                Max-=Min;
                if(Max)
                {
                        OutData[2]=(Mod*(Max)+D1-D2)*42/(Max);
                        if(OutData[2]<42||OutData[2]>=252)OutData[2]+=4;
                }
                else
                        OutData[2]=0;
                Exit:;
        }

        /*

         H 0~360 S 0~1 V 0~1   ==> RGB 0~1
         Hi=H/60       (float)

         i =floor(Hi)  (int)      // sector 0 to 5
         f =Hi-i        (float)    // factorial part of h

         p=V*(1-S)
         q=V*(1-S*(f))
         t=V*(1-S*(1-f))




         H 0~252 S 0~255 V 0~255 ==> RGB 0~255
         Hi=H/42        (float)

         i =floor(Hi)   (int)      // sector 0 to 5
         f =Hi-i        (float)    // factorial part of h

         p=V*(255-S)        /255
         q=V*(255-S*(f)  )  /255
         t=V*(255-S*(1-f))  /255



         H 0~252 S 0~255 V 0~255 ==> RGB 0~255

         i =H/42        (int)    // sector 0 to 5
         f =H-i*42      (int)    // factorial*42 to int

         p=V*(255-S)            /255
         q=V*(255*42-S*(f)  )   /255/42
         t=V*(255*42-S*(42-f))  /255/42

                RGB
         0      Vtp
         1      qVp
         2      PVt
         3      pqV
         4      tpV
         5      Vpq



        */



        #define InDataV  InData[0]
        #define InDataS  InData[1]
        #define InDataH  InData[2]
        /*H 0~251 S 0~255 V 0~255 ==> RGB 0~255

         i =H/42        (int)    // sector 0 to 5
         f =H-i*42      (int)    // factorial*42 to int

         p=V*(255-S)            /255
         q=V*(255*42-S*(f)  )   /255/42
         t=V*(255*42-S*(42-f))  /255/42*/
        static void RGBFromHSV(BYTE* OutData,BYTE* InData)
        {
                int i,f,R,G,B;
                i=InDataH /42;
                f=InDataH -i*42;

                /*
                        p=InDataV *(255-InDataS )/255
                        q=InDataV *(10710-InDataS *f)/10710
                        t=InDataV *(10710-InDataS *(42-f))/10710
                */
                switch( i )
                {
                        case 1:
                                B = InDataV *(255-InDataS )/255;
                                G = InDataV ;
                                R = InDataV *(10710-InDataS *f)/10710;
                        break;
                        case 2:
                                B = InDataV *(10710-InDataS *(42-f))/10710;
                                G = InDataV ;
                                R = InDataV *(255-InDataS )/255;
                        break;
                        case 3:
                                B = InDataV ;
                                G = InDataV *(10710-InDataS *f)/10710;
                                R = InDataV *(255-InDataS )/255;
                        break;
                        case 4:
                                B = InDataV ;
                                G = InDataV *(255-InDataS )/255;
                                R = InDataV *(6120-InDataS *(42-f))/6120;
                        break;
                        case 5:
                                B = InDataV *(6120-InDataS *f)/6120;
                                G = InDataV *(255-InDataS )/255;
                                R = InDataV ;
                        break;
                        default: // case 0||6:
                                B = InDataV *(255-InDataS )/255;
                                G = InDataV *(6120-InDataS *(42-f))/6120;
                                R = InDataV ;
                        break;
                }

                if(R&0x800)R=0;
                else if(R&0x300)R=0xff;

                if(G&0x800)G=0;
                else if(G&0x300)G=0xff;

                if(B&0x800)B=0;
                else if(B&0x300)B=0xff;

                *OutData++ = B;
                *OutData++ = G;
                *OutData++ = R;

        }


        int GetWidth(){return ROIWidth;}
        int GetHeight(){return ROIHeight;}

        int GetROIOffsetX(){return ROIOffsetX;}
        int GetROIOffsetY(){return ROIOffsetY;}
        int GetRealWidth(){return RealWidth;}
        int GetRealHeight(){return RealHeight;}


        unsigned char GetColorType(){return ColorType;}
};


#endif
