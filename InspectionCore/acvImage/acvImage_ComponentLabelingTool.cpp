
#include "acvImage_BasicTool.hpp"
#include "acvImage_ComponentLabelingTool.hpp"

const int ContourWalkV[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}};
void acvLabeledColorDispersion(acvImage *ColorDispersionPic, acvImage *LabeledPic, int ColorNum) //0
{
    int i, j,
        Xoffset = LabeledPic->GetROIOffsetX(),
        Yoffset = LabeledPic->GetROIOffsetY(),
        Width = LabeledPic->GetWidth() - 1 + Xoffset,
        Height = LabeledPic->GetHeight() - 1 + Yoffset

                 ;
    _24BitUnion *NumTranse;
    BYTE *LabeledLine, *CDPLine;
    BYTE Color[3];

    for (i = Yoffset; i < Height; i++)
    {
        LabeledLine = LabeledPic->CVector[i] + Xoffset * 3;
        ;
        CDPLine = ColorDispersionPic->CVector[i] + Xoffset * 3;
        ;

        for (j = Xoffset; j < Width; j++)
        {
            if (LabeledLine[2] == 255)
            {
                LabeledLine += 3;
                *CDPLine++ = 255;
                *CDPLine++ = 255;
                *CDPLine++ = 255;
                continue;
            }
            NumTranse = (_24BitUnion *)LabeledLine;
            LabeledLine += 3;

            Color[2] = ((NumTranse->_3Byte.Num) * HSV_HMax / ColorNum);
            Color[1] =
                Color[0] = HSV_VMax;
            LabeledPic->RGBFromHSV(Color, Color);
            *CDPLine++ = Color[0];
            *CDPLine++ = Color[1];
            *CDPLine++ = Color[2];
        }
    }
}

BYTE *acvContourWalk(acvImage *Pic, int *X_io, int *Y_io, int *dir_io, int dirinc)//Find next non-0xFF
{
    //if(*dir_io>8||*dir_io<0)return NULL;
    BYTE **CVector = Pic->CVector;
    int dir = (*dir_io) & 0x7;
    int x, y;
    for (int i = 0; i < 8; i++)
    {
        y = *Y_io + ContourWalkV[dir][0];
        x = *X_io + ContourWalkV[dir][1];
        if (CVector[y][x * 3 + 2] != 255)
        {
            *Y_io = y;
            *X_io = x;
            *dir_io = dir;
            return &CVector[y][x * 3];
        }

        dir = (dir + dirinc) & 0x7;
    }
    return NULL;
}
//#include <unistd.h>
int acvDrawContour(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char InitDir)
{
    int NowPos[2] = {FromX, FromY};

    int NowWalkDir; //=5;//CounterClockWise
    BYTE **CVector = Pic->CVector;

    CVector[FromY][FromX * 3] = B; //StartSymbol
    CVector[FromY][FromX * 3 + 1] = G;
    CVector[FromY][FromX * 3 + 2] = 254;

    NowWalkDir = 7;
    //012
    //7 3
    //654

    int searchDir = 1; //clockwise
    BYTE *next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, searchDir);

    if (next == NULL)
    {
        CVector[FromY][FromX * 3 + 2] = R;
        return 0;
    }

    int hitExistedLabel = 0;
    //NowWalkDir=InitDir;
    while (1)
    {
        if (next[2] == 254)
        {
            break;
        }
        if (hitExistedLabel || (next[0] == 0 && next[1] == 0 && next[2] == 0))
        {
            next[0] = B;
            next[1] = G;
            next[2] = R;
        }
        else if (next[0] != B || next[1] != G || next[2] != R)
        {   //There is an existed label, set the old label as
            hitExistedLabel = 1;
            CVector[FromY][FromX * 3 + 2] = R;
            B = next[0];
            G = next[1];
            R = next[2];
            next[2] = 254;
            searchDir = -1;
            NowWalkDir = NowWalkDir - 2 + 4;
        }

        NowWalkDir = (NowWalkDir - 2 * searchDir) & 0x7; //%8

        //printf(">>:%d %d X:%d wDir:%d\n",NowPos[0],NowPos[1],next[2],NowWalkDir);
        //sleep(1);
        next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, searchDir);
    }
    next[2] = R;

    return hitExistedLabel ? -1 : 0;
}



//#include <unistd.h>
int acvDrawContour_2(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char InitDir, int SearchDir)
{
    int NowWalkDir; //=5;//CounterClockWise
    NowWalkDir = InitDir;
    //012
    //7 3
    //654

    int lastStepDir=NowWalkDir;//Get the last contour step direction
    {
        int searchDir = -SearchDir;//Find previous step
        int XPos[2] = {FromX, FromY};
        BYTE *next = acvContourWalk(Pic, &XPos[0], &XPos[1], &lastStepDir, searchDir);

        if (next == NULL)
        {
            return 0;
        }

        lastStepDir-=4;
        //Because we find it backward, so the real direction is opposite to the current direction.
        if(lastStepDir<0)lastStepDir+=8;
        //warp around
    }

    int NowPos[2] = {FromX, FromY};

    int searchDir = SearchDir;
    BYTE *next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, searchDir);

    while (1)
    {
        if (NowPos[0] == FromX && NowPos[1] == FromY && lastStepDir == NowWalkDir)
        {
            break;
        }
        next[0] = B;
        next[1] = G;
        next[2] = R;

        NowWalkDir = (NowWalkDir - 2 * searchDir) & 0x7; //%8

        //printf(">>:%d %d X:%d wDir:%d\n",NowPos[0],NowPos[1],next[2],NowWalkDir);
        //sleep(1);
        next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, searchDir);
    }

    next[0] = B;
    next[1] = G;
    next[2] = R;
    return 0;
}


void acvComponentLabeling(acvImage *Pic)
{
    acvComponentLabeling(Pic,1);
}
void acvComponentLabeling(acvImage *Pic,int lineSkip) //,DyArray<int> * Information)
{
    if(lineSkip<1)return;
    int Pic_H = Pic->GetROIOffsetY() + Pic->GetHeight() - 1,
        Pic_W = Pic->GetROIOffsetX() + Pic->GetWidth() - 1;
    char State = 0;
    int Tmp = 0;
    _24BitUnion NowLable;
    acvDeleteFrame(Pic);

    int ccstop = 0;
    NowLable._3Byte.Num = 0;
    for (int i = Pic->GetROIOffsetY() + 1; i < Pic_H; i+=lineSkip, State = 0)
        for (int j = Pic->GetROIOffsetX() + 1; j < Pic_W; j++)
        {
            if (Pic->CVector[i][3 * j + 2] == 255)
            {
                State = 0;
                continue;
            }
            if (State == 0)
            {
                State = 1;
                if ((Pic->CVector[i][3 * j] == 0 &&
                        Pic->CVector[i][3 * j + 1] == 0 &&
                        Pic->CVector[i][3 * j + 2] == 0))
                {
                    NowLable._3Byte.Num++;
                    int isOldContour = acvDrawContour(Pic, j, i,
                                                      NowLable.Byte3.Num2,
                                                      NowLable.Byte3.Num1,
                                                      NowLable.Byte3.Num0, 5);
                    if (isOldContour)
                    {
                        NowLable._3Byte.Num--;
                    }
                }
            }
            else
            {
                Pic->CVector[i][3 * j] = Pic->CVector[i][3 * (j - 1)];
                Pic->CVector[i][3 * j + 1] = Pic->CVector[i][3 * (j - 1) + 1];
                Pic->CVector[i][3 * j + 2] = Pic->CVector[i][3 * (j - 1) + 2];
            }
        }
}

int acvRemoveRegionLessThan(acvImage *LabeledPic, std::vector<acv_LabeledData> *list, int threshold)
{
    int Pic_H = LabeledPic->GetROIOffsetY() + LabeledPic->GetHeight() - 1,
        Pic_W = LabeledPic->GetROIOffsetX() + LabeledPic->GetWidth() - 1;

    BYTE **LPCVec = LabeledPic->CVector;

    int topLabel = 1; //Label starts from 1
    if (threshold > 0)
    {
        //Label starts from 1
        for (int i = topLabel; i < list->size(); i++)
        {
            acv_LabeledData *ld = &((*list)[i]);
            if (ld->area < threshold)
            {
                ld->area = 0;
                ld->misc = 0;
            }
            else
            {
                ld->misc = topLabel;
                topLabel++;
            }
        }
    }
    topLabel -= 1; //rm last ++

    if (topLabel + 1 == list->size())
        return topLabel;
    _24BitUnion *lableConv;
    for (int i = LabeledPic->GetROIOffsetY() + 1; i < Pic_H; i++)
        for (int j = LabeledPic->GetROIOffsetX() + 1; j < Pic_W; j++)
        {
            if (LPCVec[i][j * 3 + 2] == 255)
                continue;
            lableConv = (_24BitUnion *)&(LPCVec[i][j * 3]);
            int label = lableConv->_3Byte.Num;
            acv_LabeledData *ld = &((*list)[label]);
            if (ld->area == 0)
            {
                LPCVec[i][j * 3] =
                    LPCVec[i][j * 3 + 1] =
                        LPCVec[i][j * 3 + 2] = 255;
            }
            else
            {
                lableConv->_3Byte.Num = ld->misc; //Assign to new label
            }
        }
    for (int i = 1; i < list->size(); i++) //collapse the list
    {
        acv_LabeledData *ld = &((*list)[i]);
        if (ld->misc != 0)
        {
            acv_LabeledData *nld = &((*list)[ld->misc]);
            *nld = *ld;
        }
    }
    list->resize(topLabel + 1);
}

int acvLabeledRegionInfo(acvImage *LabeledPic, std::vector<acv_LabeledData> *ret_list)
{
    ret_list->clear();
    std::vector<acv_LabeledData> *list = ret_list;
    int Pic_H = LabeledPic->GetROIOffsetY() + LabeledPic->GetHeight() - 1,
        Pic_W = LabeledPic->GetROIOffsetX() + LabeledPic->GetWidth() - 1;

    BYTE **LPCVec = LabeledPic->CVector;

    _24BitUnion *lableConv;
    for (int i = LabeledPic->GetROIOffsetY() + 1; i < Pic_H; i++)
        for (int j = LabeledPic->GetROIOffsetX() + 1; j < Pic_W; j++)
        {
            if (LPCVec[i][j * 3 + 2] == 255)
                continue;
            lableConv = (_24BitUnion *)&(LPCVec[i][j * 3]);
            int label = lableConv->_3Byte.Num;
            if (list->size() <= label)
                list->resize(label + 1);
            acv_LabeledData *ld = &((*list)[label]);
            if (ld->area == 0)
            {
                ld->LTBound.X = j;
                ld->LTBound.Y = i; //This is fixed
                ld->RBBound.X = j;
                ld->RBBound.Y = i;
            }
            else
            {
                if (ld->LTBound.X > j)
                    ld->LTBound.X = j;
                else if (ld->RBBound.X < j)
                    ld->RBBound.X = j;
                if (ld->RBBound.Y < i)
                    ld->RBBound.Y = i;
            }
            ld->area++;
            ld->Center.X += j;
            ld->Center.Y += i;
        }

    for (int i = 0; i < list->size(); i++)
    {
        acv_LabeledData *ld = &((*list)[i]);
        ld->Center.X /= ld->area;
        ld->Center.Y /= ld->area;
    }
	return 0;
}
