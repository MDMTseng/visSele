#ifndef  acvImage_HeaderIncludeFlag
        #include "acvImage_BasicTool.cpp"
#endif

void acvHistoEqColor(acvImage *OutPic,acvImage *OriPic)
{
    int i,j;
    unsigned long int Histo[256];      //B
    for(i=0;i<256;i++)Histo[i]=0;

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         Histo[(OriPic->CVector[i][3*j]+OriPic->CVector[i][3*j+1]+OriPic->CVector[i][3*j+2])/3]++;

    for(i=1;i<256;i++)Histo[i]+=Histo[i-1];

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
    {
         OutPic->CVector[i][3*j  ]=255*Histo[OutPic->CVector[i][3*j  ]]/Histo[255];
         OutPic->CVector[i][3*j+1]=255*Histo[OutPic->CVector[i][3*j+1]]/Histo[255];
         OutPic->CVector[i][3*j+2]=255*Histo[OutPic->CVector[i][3*j+2]]/Histo[255];

    }
      /*


    for(i=0;i<256;i++)Histo[i]=0;            //G

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         Histo[OriPic->CVector[i][3*j+1]]++;

    for(i=1;i<256;i++)Histo[i]+=Histo[i-1];

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         OutPic->CVector[i][3*j+1]=255*Histo[OutPic->CVector[i][3*j+1]]/Histo[255];




    for(i=0;i<256;i++)Histo[i]=0;         //R

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         Histo[OriPic->CVector[i][3*j+2]]++;

    for(i=1;i<256;i++)Histo[i]+=Histo[i-1];

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         OutPic->CVector[i][3*j+2]=255*Histo[OutPic->CVector[i][3*j+2]]/Histo[255];  */


}

void acvHistoEq(acvImage *OriPic,acvImage *OutPic)
{
    int i,j;
    unsigned long int Histo[256];
    for(i=0;i<256;i++)Histo[i]=0;

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         Histo[OriPic->CVector[i][3*j]]++;

    for(i=1;i<256;i++)Histo[i]+=Histo[i-1];

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
         OutPic->CVector[i][3*j]=255*Histo[OutPic->CVector[i][3*j]]/Histo[255];


}

void acvHistoEq_Ref(acvImage *RefPic,acvImage *OriPic,acvImage *OutPic)
{
    int i,j;
    unsigned long int Histo[256];
    for(i=0;i<256;i++)Histo[i]=0;

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
    if(!RefPic->CVector[i][3*j])
         Histo[OriPic->CVector[i][3*j]]++;

    for(i=1;i<256;i++)Histo[i]+=Histo[i-1];

    for(i=0;i<OriPic->GetHeight();i++)for(j=0;j<OriPic->GetWidth();j++)
    if(!RefPic->CVector[i][3*j])
         OutPic->CVector[i][3*j]=255*Histo[OutPic->CVector[i][3*j]]/Histo[255];
    else
         OutPic->CVector[i][3*j]=255;


}
