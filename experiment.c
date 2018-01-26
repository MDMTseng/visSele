
#include "experiment.h"
#include <cstdlib>
#include <unistd.h>
#include <time.h>

enum searchType_C
{
  searchType_C_B2W,
  searchType_C_W2B
};
inline int valueWarping(int v,int ringSize)
{
  v%=ringSize;
  return (v<0)?v+ringSize:v;
}
int acvDrawContourX(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char searchType,acvImage *buff,std::vector<acv_XY> &meancc_list)
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



    const int Dist=30;
    const int SampleDist=120;
    const int SamplingNum=SampleDist;
    for(int i=0;i<L;i+=Dist)
    {
      acv_XY meancc={0};
      int meancc_count=0;
      for(int j=0;j<SamplingNum;j++)
      {
        int v1 = rand() % SampleDist-SampleDist/2+i;
        int v2 = rand() % SampleDist-SampleDist/2+i;
        int v3 = rand() % SampleDist-SampleDist/2+i;
        acv_XY p1 = contour[valueWarping(v1,L)];
        acv_XY p2 = contour[valueWarping(v2,L)];
        acv_XY p3 = contour[valueWarping(v3,L)];
        acv_XY cc = acvCircumcenter(p1,p2,p3);

        v1 = rand() % SampleDist-SampleDist/2+i;
        v2 = rand() % SampleDist-SampleDist/2+i;
        v3 = rand() % SampleDist-SampleDist/2+i;
        p1 = contour[valueWarping(v1,L)];
        p2 = contour[valueWarping(v2,L)];
        p3 = contour[valueWarping(v3,L)];

        acv_XY cc2 = acvCircumcenter(p1,p2,p3);

        cc2.X-=cc.X;
        cc2.Y-=cc.Y;
        if(!isnormal(cc2.X) || !isnormal(cc2.Y) )continue;

        if(cc2.X*cc2.X+cc2.Y*cc2.Y>1)
        {
          continue;
        }


        cc.X+=cc2.X/2;
        cc.Y+=cc2.Y/2;
        float radious = acvDistance(cc,p1);
        radious += acvDistance(cc,p2);
        radious += acvDistance(cc,p3);
        radious/=3;
        acvDrawCircle(buff, cc.X, cc.Y, radious,20,255, 0, 0);
        meancc_list.push_back(cc);
        meancc.X+=cc.X;
        meancc.Y+=cc.Y;
        meancc_count++;

      }
      if(0&&meancc_count!=0)
      {
        meancc.X/=meancc_count;
        meancc.Y/=meancc_count;
        int X = round(meancc.X);
        int Y = round(meancc.Y);
        printf("%f  %f<<%d\n",meancc.X,meancc.Y,meancc_count);

        if(X>=0 && X < buff->GetWidth()  &&
        Y>=0 && Y<buff->GetHeight())
        {
              buff->CVector[Y][X*3]=255;
              buff->CVector[Y][X*3+1]=0;
              buff->CVector[Y][X*3+2]=0;
        }

      }

    }
    printf("SIZE::%d\n", contour.size());
    return 0;
}
void CircleDetect(acvImage *img,acvImage *buff)
{
    BYTE *OutLine, *OriLine;

    static std::vector<acv_XY> meancc_list;
    meancc_list.resize(0);


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
            acvDrawContourX(img, j, i, 1, 128, 1, searchType_C_W2B,buff,meancc_list);
          }
          else if(pre_pix==0 && OriLine[0] == 255)//black to white
          {
            acvDrawContourX(img, j-1, i, 1, 128, 1, searchType_C_B2W,buff,meancc_list);
          }
          pre_pix= OriLine[0];
        }
    }



    for(int i=0;i<meancc_list.size();i++)
    {
      int X = round(meancc_list[i].X);
      int Y = round(meancc_list[i].Y);
      if(X>=0 && X < buff->GetWidth()  &&
      Y>=0 && Y<buff->GetHeight())
      {
            buff->CVector[Y][X*3]=255;
            if(buff->CVector[Y][X*3+1])
              buff->CVector[Y][X*3+1]--;
            buff->CVector[Y][X*3+2]=0;
      }
    }

}
