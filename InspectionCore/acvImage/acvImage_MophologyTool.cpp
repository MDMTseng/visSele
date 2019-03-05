
#include "acvImage.hpp"
#include "acvImage_BasicTool.hpp"
#include "acvImage_MophologyTool.hpp"

#define MophologySize 1 //(Size*2+1)*(Size*2+1)
#define MophologyDim (MophologySize * 2 + 1) * (MophologySize * 2 + 1)
void acvbDilation(acvImage *BuffPic, acvImage *Pic, int Size) //Max
{
    int i, j, k, TmpSum;

    for (j = 0; j < Pic->GetWidth(); j++)
    {
        TmpSum = 0;
        for (k = 1; k < Size; k++)
            if (Pic->CVector[k][3 * j])
                TmpSum++;

        for (i = 0; i < Pic->GetHeight(); i++)
        {

            if (i > Size + 1)
                if (Pic->CVector[i - Size - 1][3 * j])
                    TmpSum--;
            if (i + Size < Pic->GetHeight())
                if (Pic->CVector[i + Size][3 * j])
                    TmpSum++;

            if (TmpSum)
                BuffPic->CVector[i][3 * j] = 255;
            else
                BuffPic->CVector[i][3 * j] = 0;
        }
    }

    for (i = 0; i < Pic->GetHeight(); i++)
    {
        TmpSum = 0;
        for (k = 1; k < Size; k++)
            if (BuffPic->CVector[i][3 * (k)])
                TmpSum++;
        for (j = 0; j < Pic->GetWidth(); j++)
        {

            if (j > Size + 1)
                if (BuffPic->CVector[i][3 * (j - Size - 1)])
                    TmpSum--;
            if (j + Size < Pic->GetWidth())
                if (BuffPic->CVector[i][3 * (j + Size)])
                    TmpSum++;

            if (TmpSum)
                Pic->CVector[i][3 * j] = 255;
            else
                Pic->CVector[i][3 * j] = 0;
        }
    }
}

void acvWindowMax(acvImage *Pic, int Size) //Max
{
    int i, j, k;

    for (j = 0; j < Pic->GetWidth(); j++)
    {
        for (i = 0; i < Pic->GetHeight(); i++)
        {
            int max=0;
            for (k = -Size+1; k < Size; k++)
            {
                int y = k+i;
                if(y<0 || y>=Pic->GetHeight())continue;
                
                int tmp = Pic->CVector[y][3 * j];
                if(max<tmp)
                {
                    max=tmp;
                }
            }
            Pic->CVector[i][3 * j+1]=max;
        }
    }


    for (i = 0; i < Pic->GetHeight(); i++)
    {
        for (j = 0; j < Pic->GetWidth(); j++)
        {
            int max=0;
            for (k = -Size+1; k < Size; k++)
            {
                int x = k+j;
                if(x<0 || x>=Pic->GetWidth())continue;
                
                int tmp = Pic->CVector[i][3 * x+1];
                if(max<tmp)
                {
                    max=tmp;
                }
            }
            Pic->CVector[i][3 * j]=max;
        }
    }
}




void acvbErosion(acvImage *BuffPic, acvImage *Pic, int Size) //Min
{
    int i, j, k, TmpSum;

    for (j = Pic->GetROIOffsetX(); j < Pic->GetWidth(); j++)
    {
        TmpSum = 0;
        for (k = 1; k < Size; k++)
            if (!Pic->CVector[k][3 * j])
                TmpSum++;

        for (i = Pic->GetROIOffsetY(); i < Pic->GetHeight(); i++)
        {

            if (i > Size + 1)
                if (!Pic->CVector[i - Size - 1][3 * j])
                    TmpSum--;
            if (i + Size < Pic->GetHeight())
                if (!Pic->CVector[i + Size][3 * j])
                    TmpSum++;

            if (TmpSum)
                BuffPic->CVector[i][3 * j] = 0;
            else
                BuffPic->CVector[i][3 * j] = 255;
        }
    }

    for (i = Pic->GetROIOffsetY(); i < Pic->GetHeight(); i++)
    {
        TmpSum = 0;
        for (k = 1; k < Size; k++)
            if (!BuffPic->CVector[i][3 * (k)])
                TmpSum++;
        for (j = Pic->GetROIOffsetX(); j < Pic->GetWidth(); j++)
        {

            if (j > Size + 1)
                if (!BuffPic->CVector[i][3 * (j - Size - 1)])
                    TmpSum--;
            if (j + Size < Pic->GetWidth())
                if (!BuffPic->CVector[i][3 * (j + Size)])
                    TmpSum++;

            if (TmpSum)
                Pic->CVector[i][3 * j] = 0;
            else
                Pic->CVector[i][3 * j] = 255;
        }
    }
}

void acvbOpening(acvImage *BuffPic, acvImage *OriPic, int Size)
{
    acvbErosion(BuffPic, OriPic, Size);
    acvbDilation(BuffPic, OriPic, Size);
}

void acvbClosing(acvImage *BuffPic, acvImage *OriPic, int Size)
{
    acvbDilation(BuffPic, OriPic, Size);
    acvbErosion(BuffPic, OriPic, Size);
}

void acvImAnd(acvImage *Pic1, acvImage *Pic2)
{
    for (int i = 0; i < Pic1->GetHeight(); i++)
        for (int j = 0; j < Pic1->GetWidth(); j++)
        {
            //if(!Pic1->CVector[i][3*j])Pic2->CVector[i][3*j]=0;
            Pic2->CVector[i][3 * j] &= Pic1->CVector[i][3 * j];
        }
}

void acvImAndColor(acvImage *Pic1, acvImage *Pic2)
{
    for (int i = 0; i < Pic1->GetHeight(); i++)
        for (int j = 0; j < Pic1->GetWidth(); j++)
        {
            //if(!Pic1->CVector[i][3*j])Pic2->CVector[i][3*j]=0;
            Pic2->CVector[i][3 * j] &= Pic1->CVector[i][3 * j];
            Pic2->CVector[i][3 * j + 1] &= Pic1->CVector[i][3 * j + 1];
            Pic2->CVector[i][3 * j + 2] &= Pic1->CVector[i][3 * j + 2];
        }
}

void acvImOr(acvImage *Pic1, acvImage *Pic2)
{
    for (int i = Pic1->GetROIOffsetY(); i < Pic1->GetHeight(); i++)
        for (int j = Pic1->GetROIOffsetX(); j < Pic1->GetWidth(); j++)
        {
            //if(Pic2->CVector[i][3*j])Pic2->CVector[i][3*j]=255;
            Pic2->CVector[i][3 * j] |= Pic1->CVector[i][3 * j];
        }
}

void acvImNot(acvImage *Pic)
{
    for (int i = Pic->GetROIOffsetY(); i < Pic->GetHeight(); i++)
        for (int j = Pic->GetROIOffsetX(); j < Pic->GetWidth(); j++)
        {
            Pic->CVector[i][3 * j] = ~Pic->CVector[i][3 * j];
        }
}

void acvImXor(acvImage *Pic1, acvImage *Pic2)
{
    for (int i = Pic1->GetROIOffsetY(); i < Pic1->GetHeight(); i++)
        for (int j = Pic1->GetROIOffsetX(); j < Pic1->GetWidth(); j++)
        {
            Pic2->CVector[i][3 * j] ^= Pic1->CVector[i][3 * j];
        }
}
void acvCountXB(int *X, int *B, acvImage *Pic, int i, int j, BYTE Valve)
{
    *X = 0;
    *B = 8;
    if (Pic->CVector[i - 1][3 * (j - 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i - 1][3 * (j)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i - 1][3 * (j)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i - 1][3 * (j + 1)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i - 1][3 * (j + 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i][3 * (j + 1)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i][3 * (j + 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i + 1][3 * (j + 1)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i + 1][3 * (j + 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i + 1][3 * (j)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i + 1][3 * (j)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i + 1][3 * (j - 1)] < Valve)
            *X = *X + 1;
    }
    if (Pic->CVector[i + 1][3 * (j - 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i][3 * (j - 1)] < Valve)
            *X = *X + 1;
    }

    if (Pic->CVector[i][3 * (j - 1)] >= Valve)
    {
        *B = *B - 1;
        if (Pic->CVector[i - 1][3 * (j - 1)] < Valve)
            *X = *X + 1;
    }
}

void acvReFresh(acvImage *Pic)
{
    for (int i = 0; i < Pic->GetHeight(); i++)
        for (int j = 0; j < Pic->GetWidth(); j++)
        {
            if (Pic->CVector[i][3 * j + 1])
                continue;
            if (Pic->CVector[i][3 * j] > 0)

                Pic->CVector[i][3 * j + 2] = 255;
            else
                Pic->CVector[i][3 * j + 2] = 0;
            Pic->CVector[i][3 * j] = 0;
        }
}

void acvZ_S_Skelet(acvImage *Pic)
{
    int i, j, B, X;
    BYTE LevelNum;
    bool IfChange = 1;
    LevelNum = 255;
    while (IfChange)
    {
        IfChange = 0;
        if (--LevelNum == 2)
        {
            //Threshold(Pic,0);
            acvReFresh(Pic);
            LevelNum = 254;
        }
        for (i = 1; i < Pic->GetHeight() - 1; i++)
            for (j = 1; j < Pic->GetWidth() - 1; j++)
            {

                if (Pic->CVector[i][3 * j])
                    continue;

                if (Pic->CVector[i][3 * (j + 1)] > LevelNum || Pic->CVector[i + 1][3 * j] > LevelNum ||
                        (Pic->CVector[i - 1][3 * j] > LevelNum && Pic->CVector[i][3 * (j - 1)] > LevelNum))
                    acvCountXB(&X, &B, Pic, i, j, LevelNum + 1);
                else
                    continue;
                if (X == 1 && B > 1 && B < 7)
                {
                    IfChange = 1;
                    Pic->CVector[i][3 * j] = LevelNum;
                }
            }
        LevelNum--;
        for (i = 1; i < Pic->GetHeight() - 1; i++)
            for (j = 1; j < Pic->GetWidth() - 1; j++)
            {
                if (Pic->CVector[i][3 * j])
                    continue;

                if (Pic->CVector[i][3 * (j - 1)] > LevelNum || Pic->CVector[i - 1][3 * j] > LevelNum ||
                        (Pic->CVector[i + 1][3 * j] > LevelNum && Pic->CVector[i][3 * (j + 1)] > LevelNum))
                    acvCountXB(&X, &B, Pic, i, j, LevelNum + 1);
                else
                    continue;
                if (X == 1 && B > 1 && B < 7)
                {
                    IfChange = 1;
                    Pic->CVector[i][3 * j] = LevelNum;
                }
            }
    }
    acvThreshold(Pic, 0);
}
