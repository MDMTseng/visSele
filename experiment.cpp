
#include "experiment.h"
#include <cstdlib>
#include <unistd.h>
#include <time.h>

#include "circleFitting.h"

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
      intersectTestType_outer=0,
      intersectTestType_middle,
      intersectTestType_inner,
    };

    void GetSectionsWithinCircleContour(float X,float Y,float radius,float epsilon,vector<int> &intersectIdxs)
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

      int ROI_X1 = (int)(X-radius-epsilon);
      int ROI_Y1 = (int)(Y-radius-epsilon);
      int ROI_X2 = ceil(X+radius+epsilon);
      int ROI_Y2 = ceil(Y+radius+epsilon);

      if(ROI_X2<0 || ROI_Y2<0 || ROI_X1>=gridNodeW || ROI_Y1>=gridNodeH)
      {
        return;
      }
      if(ROI_X1<0)ROI_X1=0;
      if(ROI_Y1<0)ROI_Y1=0;
      if(ROI_X2>=gridNodeW)ROI_X2=gridNodeW-1;
      if(ROI_Y2>=gridNodeH)ROI_Y2=gridNodeH-1;
      outerDist_sq*=outerDist_sq;

      float innerDist_sq=radius-epsilon;
      if(innerDist_sq<0)
        innerDist_sq=0;
      else
        innerDist_sq*=innerDist_sq;

      //one exception for grid node detection method is
      //when circle fit in the grid completely, every node is outer node
      int idCircleCrossGrid=0;

      //The outer most rect is always outer side
      for(int i=ROI_Y1;i<=ROI_Y2;i++)
      {
        int idx = i*gridNodeW+ROI_X1;
        intersectTestNodes[idx]=intersectTestType_outer;
        idx = i*gridNodeW+ROI_X2;
        intersectTestNodes[idx]=intersectTestType_outer;
      }
      for(int j=ROI_X1;j<=ROI_X2;j++)
      {
        int idx = ROI_Y1*gridNodeW+j;
        intersectTestNodes[idx]=intersectTestType_outer;
        idx = ROI_Y2*gridNodeW+j;
        intersectTestNodes[idx]=intersectTestType_outer;
      }

      /*
      P indicates ROI that is possible to contain circle
      X indicates RONI that is impossible to contain circle

      PPPPPPPPPPP
      PPPPPPPPPPP
      PPPXXXXXPPP
      PPPXXXXXPPP
      PPPXXXXXPPP
      PPPPPPPPPPP
      PPPPPPPPPPP
      */

      float innerBoundOffset=(radius-epsilon)/1.414;//sqrt(2)
      int RONI_X1 = ceil(X-innerBoundOffset);
      int RONI_Y1 = ceil(Y-innerBoundOffset);
      int RONI_X2 = (int)(X+innerBoundOffset);
      int RONI_Y2 = (int)(Y+innerBoundOffset);
      //printf("%d %d %d %d\n",RONI_X1,RONI_Y1,RONI_X2,RONI_Y2);
      //Skip the outer most rect
      for(int i=ROI_Y1+1;i<=ROI_Y2-1;i++)
      {
        for(int j=ROI_X1+1;j<=ROI_X2-1;j++)
        {
          int idx = i*gridNodeW+j;
          /*if(i>=RONI_Y1&&i<RONI_Y2 && j>=RONI_X1 && j<RONI_X2)
          {
              intersectTestNodes[idx]=intersectTestType_inner;
              continue;
          }*/


          float dX = j-X;
          float dY = i-Y;
          float dist_sq = dX*dX + dY*dY;

          if(dist_sq<innerDist_sq)
          {
            idCircleCrossGrid=1;
            intersectTestNodes[idx]=intersectTestType_inner;
          }
          else if(dist_sq<outerDist_sq)
          {
            idCircleCrossGrid=1;
            intersectTestNodes[idx]=intersectTestType_middle;
          }
          else
          {
            intersectTestNodes[idx]=intersectTestType_outer;
          }
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
            int idx = gX+gY*sectionCol;
            intersectIdxs.push_back(idx);
            //intersectTestNodes[idx]=intersectTestType_outer_exclude;
          }
        }
      }
      else
      {
        for(int i=ROI_Y1;i<=ROI_Y2-1;i++)
        {
          for(int j=ROI_X1;j<=ROI_X2-1;j++)
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
      GetSectionsWithinCircleContour(X,Y,radius,epsilon,intersectIdxs);

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

    int getGetSectionRegionDataSize(int secX,int secY,int secW,int secH)
    {

      int count=0;


      if(secX>=sectionCol || secY>=sectionRow || secW<0 || secH<0)return 0;

      if(secX<0)
      {
        secW+=secX;
        secX = 0;
      }

      if(secY<0)
      {
        secH+=secY;
        secY = 0;
      }

      int secX2=secX+secW;
      int secY2=secY+secH;

      if(secX2>sectionCol)secX2 = sectionCol;
      if(secY2>sectionRow)secY2 = sectionRow;

      for(int i=secY;i<secY2;i++)
      {
        for(int j=secX;j<secX2;j++)
        {
          int idx = i*sectionCol+j;
          count+=contourSections[idx].size();
        }
      }
      return count;
    }
    const acv_XY* getGetSectionRegionData(int secX,int secY,int secW,int secH,int dataIdx)
    {
      int count=0;


      if(secX>=sectionCol || secY>=sectionRow || secW<0 || secH<0)return 0;

      if(secX<0)
      {
        secW+=secX;
        secX = 0;
      }

      if(secY<0)
      {
        secH+=secY;
        secY = 0;
      }

      int secX2=secX+secW;
      int secY2=secY+secH;

      if(secX2>sectionCol)secX2 = sectionCol;
      if(secY2>sectionRow)secY2 = sectionRow;

      for(int i=secY;i<secY2;i++)
      {
        for(int j=secX;j<secX2;j++)
        {
          int idx = i*sectionCol+j;
          int curSecSize = contourSections[idx].size();

          if(curSecSize+count > dataIdx)
          {
            return &(contourSections[idx][dataIdx-count]);
          }
          count+=curSecSize;
        }
      }
      printf("SSSS\n");
      return NULL;
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

    float crossP_LF_sum=0;
    const int Dist=5;
    float crossPHist[Dist*2];
    int crossPHist_head=0;

    for(int i=-Dist ;i<Dist;i++)
    {
      float crossP=acvVectorOrder(
        contour[valueWarping(i-Dist,L)],
        contour[valueWarping(i     ,L)],
        contour[                i+Dist]);
      crossPHist[i+Dist] =crossP;
      crossP_LF_sum+=crossP;
    }
    crossPHist_head=0;

    float epsilon=1.5;
    for(int i=0;i<L;i++)
    {

      //Filter out Non-inward contour
      //Cross product
      float crossP=acvVectorOrder(
        contour[                       i],
        contour[valueWarping(i+  Dist,L)],
        contour[valueWarping(i+2*Dist,L)]);

      crossP_LF_sum=crossP_LF_sum+crossP-crossPHist[crossPHist_head];

      float crossP_LF=crossP_LF_sum/(Dist*2+1);
      crossPHist[crossPHist_head] = crossP;
      crossPHist_head = valueWarping(crossPHist_head+1,Dist*2);

      //If the cross product is more than -epsilon(the epsilon is margin to filter out straight line)
      //if the low filtered cross product is more than 0 (history shows it's most likely an outward contour)

      if(crossP_LF>-epsilon)continue;//Inner curve
      //if(crossP_LF>epsilon || crossP_LF<-epsilon )continue;//straight

      contourGrid.push(contour[i]);
      //buff->CVector[(int)contour[i].Y][(int)contour[i].X*3+2]=255;

    }
    return 0;
}

void circleRefine(vector<acv_XY> &pointsInRange,acv_XY *circumcenter, float *radius)
{
  static Data CircleFitData(2000);
  int skip=1;
  //CircleFitData.resize_force(0);
  CircleFitData.resize(pointsInRange.size()/skip);
  for(int i=0;i<CircleFitData.size();i++)
  {
    CircleFitData.X[i]=pointsInRange[i*skip].X;
    CircleFitData.Y[i]=pointsInRange[i*skip].Y;
  }

  Circle circle;
  circle = CircleFitByHyper (CircleFitData);
  circumcenter->X = circle.a;
  circumcenter->Y = circle.b;
  *radius = circle.r;
}

int findTheMostSimilarCircleIdx(acv_Circle circle,const vector<acv_Circle> &circleList, float *similarity)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<circleList.size();i++)
  {
    float MaxR;
    float minR;
    if(circle.radius>circleList[i].radius)
    {
      MaxR = circle.radius;
      minR = circleList[i].radius;
    }
    else
    {
      minR = circle.radius;
      MaxR = circleList[i].radius;
    }
    float sim =  minR/MaxR;
    if(sim<0.8)continue;

    float MaxR_sq = (minR+MaxR)/2;
    MaxR_sq*=MaxR_sq;

    float dx = circle.circumcenter.X - circleList[i].circumcenter.X;
    float dy = circle.circumcenter.Y - circleList[i].circumcenter.Y;
    float dist_sq = dx*dx + dy*dy;

    float cc_sim = (MaxR_sq - dist_sq)/MaxR_sq;
    sim*=cc_sim;

    if(max_sim<sim)
    {
      max_sim = sim;
      idx = i;
    }


  }

  if(similarity)
    *similarity = max_sim;
  return idx;
}



float SecRegionCircleFit(contour_grid &contourGrid, int secX,int secY,int secW,int secH,
  int dataSizeMinThre, float circle_data_ratio,
  float sampleRate, vector<acv_Circle> &detectedCircles)
{
  static vector<int> s_intersectIdxs;
  static vector<acv_XY> s_points;

  int SecRSize = contourGrid.getGetSectionRegionDataSize(secX,secY,secW,secH);

  if(SecRSize<dataSizeMinThre)return 0;
  int SampleNumber = sampleRate*SecRSize*SecRSize;
  float maxMatchingScore=0;
  for(int i=0;i<SampleNumber;i++)
  {
    const acv_XY* p1 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));

    const acv_XY* p2 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    const acv_XY* p3 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));

    const acv_XY* p4 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    const acv_XY* p5 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    const acv_XY* p6 = contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));


    acv_XY cc = acvCircumcenter(*p1,*p2,*p3);

    acv_XY cc2 = acvCircumcenter(*p6,*p4,*p5);

    acv_XY cc_diff={.X=cc2.X-cc.X,.Y=cc2.Y-cc.Y};
    if(!isnormal(cc_diff.X) || !isnormal(cc_diff.Y) )continue;

    if(cc_diff.X*cc_diff.X+cc_diff.Y*cc_diff.Y>0.5)
    {
      continue;
    }

    float radius = acvDistance(cc,*p1);
    radius += acvDistance(cc2,*p5);
    radius/=2;


    cc.X=(cc.X+cc2.X)/2;
    cc.Y=(cc.Y+cc2.Y)/2;

    acv_Circle c = {.circumcenter=cc, .radius=radius};
    float similarity = 0;
    int sim_idx=findTheMostSimilarCircleIdx(c,detectedCircles,&similarity);
    if(similarity>0.8)
    {
      continue;
    }

    contourGrid.getContourPointsWithInCircleContour(cc.X,cc.Y,radius,4,s_intersectIdxs,s_points);

    float matchingScore =(float)s_points.size() / radius/((float)(2*M_PI));//Around 2PI

    if(maxMatchingScore<matchingScore)maxMatchingScore=matchingScore;
    if(matchingScore>circle_data_ratio)
    {
      circleRefine(s_points,&cc, &radius);
      contourGrid.getContourPointsWithInCircleContour(cc.X,cc.Y,radius,1.5,s_intersectIdxs,s_points);
      circleRefine(s_points,&cc, &radius);
      acv_Circle c = {.circumcenter=cc, .radius=radius};
      detectedCircles.push_back(c);
      //exit(0);
    }
  }
  return maxMatchingScore;
}


class acv_Line {
protected:
  acv_XY line_vec;
  acv_XY line_anchor;
public:

  float Distance(acv_XY point)
  {
    // P1=(x1,y1)=line_anchor
    // P2=(x2,y2)=P1+line_vec then the distance of
    // point = (x0,y0)
    float denominator = hypot(line_vec.X,line_vec.Y);
    float XX = +line_vec.Y*point.X
               -line_vec.X*point.Y
               +(line_vec.X+line_anchor.X)*line_anchor.Y
               -(line_vec.Y+line_anchor.Y)*line_anchor.X ;

    if (XX < 0)XX=-XX;
    return XX/denominator;
  }
  float Angle(acv_Line &line2)
  {
    return acos(line_vec.X*line2.line_vec.X+line_vec.Y*line2.line_vec.Y);
  }
  // Construct line from points

  bool fitPoints(const std::vector<acv_XY> &pts) {
    return fitPoints(&pts[0], pts.size());
  }
  bool fitPoints(const acv_XY *pts, int ptsL) {
    int nPoints = ptsL;
    if( nPoints < 2 ) {
      // Fail: infinitely many lines passing through this single point
      return false;
    }
    float sumX=0, sumY=0, sumXY=0, sumX2=0;
    for(int i=0; i<nPoints; i++) {
      sumX += pts[i].X;
      sumY += pts[i].Y;
      sumXY += pts[i].X * pts[i].Y;
      sumX2 += pts[i].X * pts[i].X;
    }
    float xMean = sumX / nPoints;
    float yMean = sumY / nPoints;
    float denominator = sumX2 - sumX * xMean;

    line_vec.Y =  (sumXY - sumX * yMean);
    line_vec.X =  denominator;
    denominator = hypot(line_vec.X,line_vec.Y);
    line_vec.X /=denominator;
    line_vec.Y /=denominator;

    line_anchor.X = xMean;
    line_anchor.Y = yMean;

    /*
    a/b = (sumXY - sumX * yMean) / denominator;
    a*X/b+C = Y
    => (0,c)+n*(1,a/b)
    => (0,c)+n*(b,a)
    line_vec = (b,a)
    line_anchor = (0,c)+n*(b,a)
    */
    return true;
  }
};


void CircleDetect(acvImage *img,acvImage *buff)
{

    clock_t t = clock();
    BYTE *OutLine, *OriLine;

    static vector<acv_Circle> detectedCircles;
    detectedCircles.resize(0);
    int grid_size = 50;
    static contour_grid inward_curve_grid(grid_size,img->GetWidth(),img->GetHeight());
    static contour_grid straight_contour_grid(grid_size,img->GetWidth(),img->GetHeight());

    inward_curve_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());
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
            acvDrawContourX(img, j, i, 1, 128, 1, searchType_C_W2B,buff,inward_curve_grid);
          }
          else if(pre_pix==0 && OriLine[0] == 255)//black to white
          {
            acvDrawContourX(img, j-1, i, 1, 128, 1, searchType_C_B2W,buff,inward_curve_grid);
          }
          pre_pix= OriLine[0];
        }
    }


/*
    vector<int> intersectIdxs;
    vector<acv_XY> points;

    int cX=278;
    int cY=61;
    int r=90;
    int e=90;

    contourGrid.getContourPointsWithInCircleContour(cX,cY,r,e,intersectIdxs,points);

    for(int i=0;i<15;i++)for(int j=0;j<15;j++)
    {
      acvDrawBlock(buff, j*grid_size,i*grid_size,(j+1)*grid_size,(i+1)*grid_size);
    }

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
    }*/

    int gridG_W = 2;
    int gridG_H = 2;

    /*for(int i=-gridG_H;i<contourGrid.getRowSize();i++)
    {
      for(int j=-gridG_H;j<contourGrid.getColumSize();j++)
      {*/
    for(int i=0;i<inward_curve_grid.getRowSize()-gridG_H;i++)
    {
      for(int j=0;j<inward_curve_grid.getColumSize()-gridG_W;j++)
      {
        SecRegionCircleFit(inward_curve_grid, j,i,gridG_W,gridG_H,40,0.6,0.01,detectedCircles);
      }
    }


    t = clock() - t;
    printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);

    for(int i=0;i<15;i++)for(int j=0;j<15;j++)
    {
      acvDrawBlock(buff, j*grid_size,i*grid_size,(j+1)*grid_size,(i+1)*grid_size);
    }

    for(int i=0;i<detectedCircles.size();i++)
    {
        acvDrawCircle(buff,
          detectedCircles[i].circumcenter.X, detectedCircles[i].circumcenter.Y,
          detectedCircles[i].radius,
          20,255, 0, 0);
    }

    for(int i=0;i<inward_curve_grid.dataSize();i++)
    {

      const acv_XY* p = inward_curve_grid.get(i);
      int X = round(p->X);
      int Y = round(p->Y);
      {
            buff->CVector[Y][X*3]=255;
            buff->CVector[Y][X*3+1]=255;
      }
    }
}
