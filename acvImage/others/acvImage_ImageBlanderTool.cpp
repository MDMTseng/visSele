#ifndef  acvImage_HeaderIncludeFlag
        #include "acvImage_BasicTool.cpp"
#endif




void acvPastModeDeWhite(acvImage *PasteToPic,acvImage *CopyPic,int X,int Y,int* Bound)
{
        for(int i=Bound[1];i<Bound[3];i++)for(int j=Bound[0];j<Bound[2];j++)
                if(CopyPic->CVector[i][3*j]!=255||
                   CopyPic->CVector[i][3*j+1]!=255||
                   CopyPic->CVector[i][3*j+2]!=255)
                {
                     PasteToPic->CVector[i+Y][3*(j+X)]=CopyPic->CVector[i][3*j];
                     PasteToPic->CVector[i+Y][3*(j+X)+1]=CopyPic->CVector[i][3*j+1];
                     PasteToPic->CVector[i+Y][3*(j+X)+2]=CopyPic->CVector[i][3*j+2];
                }
                else
                     continue;
}
void acvPastModeDWAlphaBlender(acvImage *PasteToPic,acvImage *CopyPic,int X,int Y,int* Bound,int Alpha)
{
        int Beta;
        if(Alpha>256)
        {
             acvPastModeDeWhite(PasteToPic,CopyPic,X,Y,Bound);
             goto Exit;
        }
        else Beta=256-Alpha;
        for(int i=Bound[1];i<Bound[3];i++)for(int j=Bound[0];j<Bound[2];j++)
                if(CopyPic->CVector[i][3*j]!=255||
                   CopyPic->CVector[i][3*j+1]!=255||
                   CopyPic->CVector[i][3*j+2]!=255)
                {
                     PasteToPic->CVector[i+Y][3*(j+X)]=(CopyPic->CVector[i][3*j]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)]*Beta)>>8;
                     PasteToPic->CVector[i+Y][3*(j+X)+1]=(CopyPic->CVector[i][3*j+1]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)+1]*Beta)>>8;
                     PasteToPic->CVector[i+Y][3*(j+X)+2]=(CopyPic->CVector[i][3*j+2]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)+2]*Beta)>>8;
                }
                else
                     continue;
        Exit:;
}





void acvPastModeAlphaBlender(acvImage *PasteToPic,acvImage *CopyPic,int X,int Y,int* Bound,int Alpha)
{
        int Beta;
        if(Alpha>256)
        {
             goto Exit;
        }
        else Beta=256-Alpha;
        for(int i=Bound[1];i<Bound[3];i++)for(int j=Bound[0];j<Bound[2];j++)
                {
                     PasteToPic->CVector[i+Y][3*(j+X)]=(CopyPic->CVector[i][3*j]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)]*Beta)>>8;
                     PasteToPic->CVector[i+Y][3*(j+X)+1]=(CopyPic->CVector[i][3*j+1]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)+1]*Beta)>>8;
                     PasteToPic->CVector[i+Y][3*(j+X)+2]=(CopyPic->CVector[i][3*j+2]*Alpha+PasteToPic->CVector[i+Y][3*(j+X)+2]*Beta)>>8;
                }
        Exit:;
}


void acvPicPaste(acvImage *PasteToPic,acvImage *CopyPic,int X,int Y,int Mode)
{
        int Bound[4];//XStart YStart XEnd YEnd

        if(CopyPic->GetWidth()<-X||CopyPic->GetHeight()<-Y||Mode<0)
                goto Exit;
        if(X<0)Bound[0]=CopyPic->GetWidth()+X;
        else   Bound[0]=0;
        if(Y<0)Bound[1]=CopyPic->GetHeight()+Y;
        else   Bound[1]=0;
        Bound[2]=PasteToPic->GetWidth()-X;
        Bound[3]=PasteToPic->GetHeight()-Y;
        if(Bound[2]>CopyPic->GetWidth())Bound[2]=CopyPic->GetWidth();
        if(Bound[3]>CopyPic->GetHeight())Bound[3]=CopyPic->GetHeight();


        if(Mode<1024)acvPastModeDWAlphaBlender(PasteToPic,CopyPic,X,Y,Bound,Mode);
        else         acvPastModeAlphaBlender(PasteToPic,CopyPic,X,Y,Bound,Mode-1024);





        Exit:;

}
void acvPicPaste(acvImage *PasteToPic,acvImage *CopyPic,int Mode)
{
        int Bound[4];
        Bound[0]=Bound[1]=0;
        Bound[2]=(PasteToPic->GetWidth()>CopyPic->GetWidth())?
                  CopyPic->GetWidth():PasteToPic->GetWidth();
        Bound[3]=(PasteToPic->GetHeight()>CopyPic->GetHeight())?
                  CopyPic->GetHeight():PasteToPic->GetHeight();


        if(Mode<1024)acvPastModeDWAlphaBlender(PasteToPic,CopyPic,0,0,Bound,Mode);
        else         acvPastModeAlphaBlender(PasteToPic,CopyPic,0,0,Bound,Mode-1024);

}
void acvBackGroundRestore(acvImage *BackGround,acvImage *NowPic,acvImage *ReferencePic)
{
        int Alpha,Beta;
        for(int i=0;i<BackGround->GetHeight();i++)for(int j=0;j<BackGround->GetWidth();j++)
        {
                     Beta=ReferencePic->CVector[i][3*j];
                     Alpha=256-Beta;
                     BackGround->CVector[i][3*j+2]=(NowPic->CVector[i][3*j+2]*Alpha+BackGround->CVector[i][3*j+2]*Beta)>>8;
                     BackGround->CVector[i][3*j+1]=(NowPic->CVector[i][3*j+1]*Alpha+BackGround->CVector[i][3*j+1]*Beta)>>8;
                     BackGround->CVector[i][3*j]=(NowPic->CVector[i][3*j]*Alpha+BackGround->CVector[i][3*j]*Beta)>>8;
        }
        Exit:;

}
void acvBackGroundRestore(acvImage *BackGround,acvImage *NowPic,acvImage *ReferenceBinaryPic,int Alpha)
{
        int Beta;
        BYTE *BGLine,*NPLine,*RBLine;
        if(Alpha>256)
        {
             goto Exit;
        }
        else Beta=256-Alpha;
        for(int i=0;i<BackGround->GetHeight();i++)
        {
                RBLine=ReferenceBinaryPic->CVector[i];
                BGLine=BackGround->CVector[i];
                NPLine=NowPic->CVector[i];
                for(int j=0;j<BackGround->GetWidth();j++)
                {
                        RBLine+=2;
                        if(*RBLine++)
                        {
                                *BGLine++=((*NPLine++)*Alpha+(*BGLine)*Beta)>>8;
                                *BGLine++=((*NPLine++)*Alpha+(*BGLine)*Beta)>>8;
                                *BGLine++=((*NPLine++)*Alpha+(*BGLine)*Beta)>>8;
                        }
                        else
                        {
                                BGLine+=3;
                                NPLine+=3;
                        }

                }
        }
        Exit:;

}

