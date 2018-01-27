
#include "experiment.h"
#include <cstdlib>
#include <unistd.h>
#include <time.h>

using namespace std;
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

class contour_grid{
    vector< vector <acv_XY> > contourSections;
    vector< int > intersectTestNodes;
    int gridSize;
    int dataNumber=0;
    int sectionCol;
    int sectionRow;
    public:
    contour_grid(int grid_size,int img_width,int img_height)
    {
      RESET(grid_size,img_width,img_height);
    }

    void RESET(int grid_size,int img_width,int img_height)
    {
      dataNumber = 0;
      if(grid_size==-1)
        grid_size = gridSize;
      gridSize = grid_size;
      sectionCol=ceil((float)img_width/grid_size);
      sectionRow=ceil((float)img_height/grid_size);
      contourSections.resize(sectionCol*sectionRow);
      for(int i=0;i<contourSections.size();i++)
      {
        contourSections[i].resize(0);
      }
    }

    int getColumSize()
    {
      return sectionCol;
    }

    int getRowSize()
    {
      return sectionRow;
    }


    int getSecIdx(int X,int Y)
    {
      int gridX=X/gridSize;
      int gridY=Y/gridSize;
      return sectionCol*gridY+gridX;
    }

    vector<acv_XY> &fetchBelongingSection(acv_XY data)
    {
      int gridIdx=getSecIdx(data.X,data.Y);
      return contourSections[gridIdx];
    }

    void push(acv_XY data)
    {
      fetchBelongingSection(data).push_back(data);
      dataNumber++;
    }

    int dataSize()
    {
      return dataNumber;
    }

    const acv_XY* get(int idx)
    {
      int idx_count_down=idx;
      for(int i=0;i<contourSections.size();i++)
      {
        if(idx_count_down<contourSections[i].size())
        {
          return &(contourSections[i][idx_count_down]);
        }
        else
        {
          idx_count_down-=contourSections[i].size();
        }
      }
      return NULL;
    }

    enum intersectTestType
    {
      intersectTestType_outer,
      intersectTestType_middle,
      intersectTestType_inner
    };

    void GetSectionsWithInCircleContour(float X,float Y,float radius,float epsilon,vector<int> &intersectIdxs)
    {
      intersectIdxs.resize(0);


      int gridNodeW=sectionCol+1;
      int gridNodeH=sectionRow+1;
      intersectTestNodes.resize(gridNodeW*gridNodeH);

      //resize the world down gridSize times
      X/=gridSize;
      Y/=gridSize;
      radius/=gridSize;
      epsilon/=gridSize;

      float outerDist_sq=radius+epsilon;
      outerDist_sq*=outerDist_sq;

      float innerDist_sq=radius-epsilon;
      if(innerDist_sq<0)
        innerDist_sq=0;
      else
        innerDist_sq*=innerDist_sq;

      //one exception for grid node detection method is
      //when circle fit in the grid completely, every node is outer node
      int idCircleCrossGrid=0;

      for(int i=0;i<intersectTestNodes.size();i++)
      {
        int nodeX=i%gridNodeW;
        int nodeY=i/gridNodeW;
        float dX = X-nodeX;
        float dY = Y-nodeY;
        float dist_sq = dX*dX + dY*dY;

        if(dist_sq<innerDist_sq)
        {
          idCircleCrossGrid=1;
          intersectTestNodes[i]=intersectTestType_inner;
        }
        else if(dist_sq<outerDist_sq)
        {
          idCircleCrossGrid=1;
          intersectTestNodes[i]=intersectTestType_middle;
        }
        else
        {
          intersectTestNodes[i]=intersectTestType_outer;
        }
      }

      if(!idCircleCrossGrid)
      {
        //If we detect there is no circle cross the boundery. ie, circle in grid or totally out of grid group
        //We do single detection
        if(X>0 && Y>0)
        {
          int gX = (int)X;
          int gY = (int)Y;
          if(gX<sectionCol && gY<sectionRow)
          {
            printf(">>>>\n");
            int idx = gX+gY*sectionCol;
            intersectIdxs.push_back(idx);
            //intersectTestNodes[idx]=intersectTestType_outer_exclude;
          }
        }
      }
      else
      {
        for(int i=0;i<gridNodeH-1;i++)
        {
          for(int j=0;j<gridNodeW-1;j++)
          {
            int idx1 = i*gridNodeW + j;
            int idx2 = idx1+1;
            int idx3 = idx1 + gridNodeW;
            int idx4 = idx2 + gridNodeW;

            if(
              intersectTestNodes[idx1]!=intersectTestNodes[idx2] ||
              intersectTestNodes[idx3]!=intersectTestNodes[idx4] ||
              intersectTestNodes[idx1]!=intersectTestNodes[idx3]
            )
            {
              //If any nodes are different from each other there must be the contour in between
              intersectIdxs.push_back(i*sectionCol + j);
              continue;
            }
            if(intersectTestNodes[idx1]==intersectTestType_middle)
            {
              //All in the middle
              intersectIdxs.push_back(i*sectionCol + j);
              continue;
            }
            //All Test nodes are in same type in/out, pass
          }
        }
      }

    }



    void getContourPointsWithInCircleContour(float X,float Y,float radius,float epsilon,vector<int> &intersectIdxs,vector<acv_XY> &points)
    {
      points.resize(0);
      GetSectionsWithInCircleContour(X,Y,radius,epsilon,intersectIdxs);

      float outerDist_sq=radius+epsilon;
      outerDist_sq*=outerDist_sq;

      float innerDist_sq=radius-epsilon;
      if(innerDist_sq<0)
        innerDist_sq=0;
      else
        innerDist_sq*=innerDist_sq;

      int count=0;
      for(int i=0;i<intersectIdxs.size();i++)
      {
        int idx = intersectIdxs[i];
        for(int j=0;j<contourSections[idx].size();j++)
        {
          float dX = X-contourSections[idx][j].X;
          float dY = Y-contourSections[idx][j].Y;
          float dist_sq = dX*dX + dY*dY;

          if(dist_sq>innerDist_sq && dist_sq<outerDist_sq)//The point is in the epsilon region
          {
            points.push_back(contourSections[idx][j]);
          }
        }
      }
    }
};



int acvDrawContourX(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char searchType,acvImage *buff,contour_grid &contourGrid)
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

    const int Dist=20;

    float crossP_LF=0;
    float epsilon=0.2;
    for(int i=0;i<L;i++)
    {
      //Filter out Non-inward contour
      //Cross product
      float crossP=acvVectorOrder(
        contour[valueWarping(i-Dist,L)],
        contour[                     i],
        contour[valueWarping(i+Dist,L)]);

      if(i==0)crossP_LF=crossP;
      crossP_LF+=0.1*(crossP-crossP_LF);
      //If the cross product is more than -epsilon(the epsilon is margin to filter out straight line)
      //if the low filtered cross product is more than 0 (history shows it's most likely an outward contour)
      if(crossP_LF>0||crossP>-epsilon)continue;
      contourGrid.push(contour[i]);
      //buff->CVector[(int)contour[i].Y][(int)contour[i].X*3+2]=255;

    }
    printf("SIZE::%d\n", contour.size());
    return 0;
}
void CircleDetect(acvImage *img,acvImage *buff)
{
    BYTE *OutLine, *OriLine;


    int grid_size = 120;
    static contour_grid contourGrid(grid_size,img->GetWidth(),img->GetHeight());

    contourGrid.RESET(-1,img->GetWidth(),img->GetHeight());
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
            acvDrawContourX(img, j, i, 1, 128, 1, searchType_C_W2B,buff,contourGrid);
          }
          else if(pre_pix==0 && OriLine[0] == 255)//black to white
          {
            acvDrawContourX(img, j-1, i, 1, 128, 1, searchType_C_B2W,buff,contourGrid);
          }
          pre_pix= OriLine[0];
        }
    }

    /*for(int i=0;i<contourGrid.dataSize();i++)
    {
      const acv_XY *data = contourGrid.get(i);

      int X = round(data->X);
      int Y = round(data->Y);

      {
            buff->CVector[Y][X*3]=255;
            if(buff->CVector[Y][X*3+1])
              buff->CVector[Y][X*3+1]--;
            buff->CVector[Y][X*3+2]=255;
      }


    }*/

    vector<int> intersectIdxs;
    vector<acv_XY> points;

    int cX=100;
    int cY=150;
    int r=10;
    int e=7;

    contourGrid.getContourPointsWithInCircleContour(cX,cY,r,e,intersectIdxs,points);
    acvDrawCircle(buff, cX, cY, r-e,20,255, 0, 0);
    acvDrawCircle(buff, cX, cY, r+e,20,255, 0, 0);

    for(int i=0;i<points.size();i++)
    {
      int X = round(points[i].X);
      int Y = round(points[i].Y);
      {
            buff->CVector[Y][X*3]=0;
            buff->CVector[Y][X*3+2]=255;
      }
    }


}
