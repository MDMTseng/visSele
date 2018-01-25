
#include "experiment.h"

enum searchType_C
{
  searchType_C_B2W,
  searchType_C_W2B
};

int acvDrawContourX(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char searchType,acvImage *buff)
{

    static std::vector<acv_XY> contour;
    //static std::vector<acv_XY> contour;
    contour.resize(0);
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
    if(searchType == searchType_C_B2W)
    {
      NowWalkDir = 3;
    }
    else
    {
      NowWalkDir = 7;
    }

    BYTE *next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, 1);

    if (next == NULL)
    {
        CVector[FromY][FromX * 3 + 2] = R;
        return 0;
    }
    acv_XY p_hist;

    while (1)
    {
        acv_XY XY={.X=NowPos[0],.Y=NowPos[1]};
        contour.push_back(XY);
        if (next[2] == 254)
        {
            break;
        }
        next[0] = B;
        next[1] = G;
        next[2] = R;

        NowWalkDir = (NowWalkDir - 2) & 0x7; //%8

        next = acvContourWalk(Pic, &NowPos[0], &NowPos[1], &NowWalkDir, 1);
    }
    next[2] = R;


    const int L = contour.size();
    int DisconnectC=99999;
    const int DiscCThres=8;

    const int Dist=20;
    for(int i=0;i<L;i++)
    {
      int tmp = (i-Dist*3);
      if(tmp<0)tmp+=L;
      acv_XY p1 = contour[tmp];
      tmp=(tmp+Dist*2)%L;
      acv_XY p2 = contour[tmp];
      tmp=(tmp+Dist*2)%L;
      acv_XY p3 = contour[tmp];


      if(acvVectorOrder(p1,p2,p3)>0)continue;


      tmp=(tmp+Dist*2)%L;
      acv_XY p4 = contour[tmp];


      acv_XY cc = acvCircumcenter(p1,p2,p4);
      acv_XY cc2 = acvCircumcenter(p1,p3,p4);

      if(!isnormal(cc.X) || !isnormal(cc.Y) || !isnormal(cc2.X) || !isnormal(cc2.Y))
      {
        continue;
      }

      cc2.X-=cc.X;
      cc2.Y-=cc.Y;
      if(cc2.X*cc2.X+cc2.Y*cc2.Y>30)
      {
        continue;
      }
      cc.X+=cc2.X/2;
      cc.Y+=cc2.Y/2;

      int X = round(cc.X);
      int Y = round(cc.Y);
      if(X>=0 && X < buff->GetWidth()  &&
      Y>=0 && Y<buff->GetHeight())
      {
        //buff->CVector[Y][X*3]=128;
        if(buff->CVector[Y][X*3+1])buff->CVector[Y][X*3+1]--;
        buff->CVector[Y][X*3+2]=128;
        buff->CVector[(int)contour[i].Y][(int)contour[i].X*3+2]=255;
      }

    }

    printf("SIZE::%d\n", contour.size());
    return 0;
}
void CircleDetect(acvImage *img,acvImage *buff)
{
    BYTE *OutLine, *OriLine;

    acvCloneImage(img, buff, -1);
    for (int i = 0; i < img->GetHeight(); i++)
    {
        OutLine = buff->CVector[i];
        OriLine = img->CVector[i];
        uint8_t pre_pix = 255;
        for (int j = 0; j < buff->GetWidth(); j++,OutLine+=3,OriLine+=3)
        {
          if(pre_pix==255 && OriLine[0] == 0)//White to black
          {
            acvDrawContourX(img, j, i, 1, 128, 1, searchType_C_W2B,buff);
          }
          else if(pre_pix==0 && OriLine[0] == 255)//black to white
          {
            acvDrawContourX(img, j-1, i, 1, 128, 1, searchType_C_B2W,buff);
          }
          pre_pix= OriLine[0];
        }
    }
}
