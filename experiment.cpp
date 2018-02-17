
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
    int secROI_X, secROI_Y, secROI_W, secROI_H;
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
      secROI_X=0;
      secROI_Y=0;
      secROI_W=sectionCol;
      secROI_H=sectionRow;
      contourSections.resize(sectionCol*sectionRow);
      for(int i=0;i<contourSections.size();i++)
      {
        contourSections[i].resize(0);
      }
    }


    void setSecROI(int secROI_X,int secROI_Y,int secROI_W,int secROI_H)
    {
      this->secROI_X=secROI_X;
      this->secROI_Y=secROI_Y;
      this->secROI_W=secROI_W;
      this->secROI_H=secROI_H;
    }
    void resetSecROI()
    {
      setSecROI(0,0,sectionCol,sectionRow);
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

    void GetSectionsWithinCircleContour(float X,float Y,float radius,float epsilon,
      vector<int> &intersectIdxs)
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

      int secROI_X1 = (int)(X-radius-epsilon);
      int secROI_Y1 = (int)(Y-radius-epsilon);
      int secROI_X2 = ceil(X+radius+epsilon);
      int secROI_Y2 = ceil(Y+radius+epsilon);


      if(secROI_X2<0 || secROI_Y2<0 || secROI_X1>=gridNodeW || secROI_Y1>=gridNodeH)
      {
        return;
      }
      if(secROI_X1<0)secROI_X1=0;
      if(secROI_Y1<0)secROI_Y1=0;
      if(secROI_X2>=gridNodeW)secROI_X2=gridNodeW-1;
      if(secROI_Y2>=gridNodeH)secROI_Y2=gridNodeH-1;

      if(secROI_X1<secROI_X)secROI_X1=secROI_X;
      if(secROI_Y1<secROI_Y)secROI_Y1=secROI_Y;
      if(secROI_X2>secROI_X+secROI_W)secROI_X2 = secROI_X+secROI_W;
      if(secROI_Y2>secROI_Y+secROI_H)secROI_Y2 = secROI_Y+secROI_H;

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
      for(int i=secROI_Y1;i<=secROI_Y2;i++)
      {
        int idx = i*gridNodeW+secROI_X1;
        intersectTestNodes[idx]=intersectTestType_outer;
        idx = i*gridNodeW+secROI_X2;
        intersectTestNodes[idx]=intersectTestType_outer;
      }
      for(int j=secROI_X1;j<=secROI_X2;j++)
      {
        int idx = secROI_Y1*gridNodeW+j;
        intersectTestNodes[idx]=intersectTestType_outer;
        idx = secROI_Y2*gridNodeW+j;
        intersectTestNodes[idx]=intersectTestType_outer;
      }

      /*
      P indicates secROI that is possible to contain circle
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
      for(int i=secROI_Y1+1;i<=secROI_Y2-1;i++)
      {
        for(int j=secROI_X1+1;j<=secROI_X2-1;j++)
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
        for(int i=secROI_Y1;i<=secROI_Y2-1;i++)
        {
          for(int j=secROI_X1;j<=secROI_X2-1;j++)
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

    void getContourPointsWithInCircleContour(float X,float Y,float radius,float epsilon,
      vector<int> &intersectIdxs,vector<acv_XY> &points)
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



    void GetSectionsWithinLineContour(acv_Line line,float epsilon,vector<int> &intersectIdxs)
    {
      intersectIdxs.resize(0);

      int secROI_X1 = secROI_X;
      int secROI_Y1 = secROI_Y;
      int secROI_X2 = secROI_X+secROI_W;
      int secROI_Y2 = secROI_Y+secROI_H;

      int gridNodeW=sectionCol+1;
      int gridNodeH=sectionRow+1;
      intersectTestNodes.resize(gridNodeW*gridNodeH);

      //resize the world down gridSize times
      line.line_anchor.X/=gridSize;
      line.line_anchor.Y/=gridSize;
      epsilon/=gridSize;

      //printf("%d %d %d %d\n",RONI_X1,RONI_Y1,RONI_X2,RONI_Y2);
      //Skip the outer most rect
      for(int i=secROI_Y1;i<=secROI_Y2;i++)
      {
        for(int j=secROI_X1;j<=secROI_X2;j++)
        {
          int idx = i*gridNodeW+j;
          acv_XY pt={.X=j,.Y=i};

          int dist_signed = acvDistance_Signed(line, pt);
          int dist =(dist_signed>0)?dist_signed:-dist_signed;

          if(dist < epsilon)
          {
            intersectTestNodes[idx]=intersectTestType_middle;
          }
          else if(dist_signed<0)
          {
            intersectTestNodes[idx]=intersectTestType_inner;
          }
          else
          {
            intersectTestNodes[idx]=intersectTestType_outer;
          }
        }
      }

      for(int i=secROI_Y1;i<=secROI_Y2-1;i++)
      {
        for(int j=secROI_X1;j<=secROI_X2-1;j++)
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

    void getContourPointsWithInLineContour(acv_Line line,float epsilon,vector<int> &intersectIdxs,vector<acv_XY> &points)
    {
      points.resize(0);
      GetSectionsWithinLineContour(line,epsilon,intersectIdxs);
      //exit(0);
      int count=0;
      for(int i=0;i<intersectIdxs.size();i++)
      {
        int idx = intersectIdxs[i];
        for(int j=0;j<contourSections[idx].size();j++)
        {

          int dist = acvDistance(line, contourSections[idx][j]);
          if(dist<epsilon)//The point is in the epsilon region
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
      return NULL;
    }
};



int acvContourExtraction(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char searchType, vector<acv_XY> &contour)
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

    return 0;
}

void ContourFilter(vector<acv_XY> &contour,vector<acv_XY> &innerCornorContour,vector<acv_XY> &lineContour)
{
    const int L = contour.size();

    float crossP_LF_sum=0;
    const int Dist=10;
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

    float epsilon=3;
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

      if(crossP_LF<-epsilon){
        innerCornorContour.push_back(contour[i]);
      }//Inner curve


      if(crossP_LF<epsilon && crossP_LF>-epsilon ){
        lineContour.push_back(contour[i]);
      }//Line

    }
}

void circleRefine(vector<acv_XY> &pointsInRange,acv_CircleFit *circleF)
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
  circleF->circle.circumcenter.X = circle.a;
  circleF->circle.circumcenter.Y = circle.b;
  circleF->circle.radius = circle.r;
  circleF->s = circle.s;
}

float CircleSimilarity(acv_Circle c1,acv_Circle c2)
{
  float sim = c1.radius/c2.radius;
  if(sim>1)sim=1/sim;

  float MaxR_sq = (c1.radius+c2.radius)/2;
  MaxR_sq*=MaxR_sq;

  float dx = c1.circumcenter.X - c2.circumcenter.X;
  float dy = c1.circumcenter.Y - c2.circumcenter.Y;
  float dist_sq = dx*dx + dy*dy;

  float cc_sim = (MaxR_sq - dist_sq)/MaxR_sq;

  return sim*cc_sim;
}

float LineSimilarity(acv_Line line1,acv_Line line2,float epsilon)
{
  float ang= acvLineAngle(line1,line2);
  if(ang>M_PI_2)ang=M_PI-ang;
  if(ang>0.2)return 0;//around 0.4deg


  if( !isnormal(ang) )return 0;
  float sim = (M_PI_2-ang)/M_PI_2;
  float dist = acvDistance(line1, line2.line_anchor);
  return sim*(epsilon-dist)/epsilon;
}
int findTheMostSimilarLineIdx(acv_Line line,const vector<acv_LineFit> &lineList,float epsilon, float *similarity)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<lineList.size();i++)
  {
    float sim = LineSimilarity(line,lineList[i].line,epsilon);

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

int findTheMostSimilarCircleIdx(acv_Circle circle,const vector<acv_CircleFit> &circleList, float *similarity)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<circleList.size();i++)
  {
    float sim = CircleSimilarity(circle,circleList[i].circle);

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

int refineMatchedCircle(vector<acv_CircleFit> &circleList,int simThres)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<((int)circleList.size()-1);i++)
  {
    if(circleList[i].matching_pts==0)continue;
    float matchingScore1 = circleList[i].matching_pts/(circleList[i].s+0.3);
    for(int j=i+1;j<circleList.size();j++)
    {
      if(circleList[j].matching_pts==0)continue;
      float sim= CircleSimilarity(circleList[i].circle,circleList[j].circle);
      if(sim<simThres)continue;

      float matchingScore2 = circleList[j].matching_pts/(circleList[i].s+0.3);
      if(matchingScore2>matchingScore1)
      {
        circleList[i].matching_pts = 0;
        break;
      }
      else
      {
        circleList[j].matching_pts = 0;
      }
    }
  }

  int w_h=0;
  for(int i=0;i<circleList.size();i++)
  {
    if(circleList[i].matching_pts==0)continue;
    if(w_h!=i)
      circleList[w_h]=circleList[i];
    w_h++;
  }
  circleList.resize(w_h);
}

bool CircleFitTest(contour_grid &contourGrid,
    acv_Circle c,acv_CircleFit *ret_cf,float epsilon1,float epsilon2,
    float circle_data_ratio)
{

    static vector<int> s_intersectIdxs;
    static vector<acv_XY> s_points;
    contourGrid.getContourPointsWithInCircleContour(c.circumcenter.X,c.circumcenter.Y,c.radius,epsilon1,
      s_intersectIdxs,s_points);

    float matchingScore =(float)s_points.size() / c.radius/((float)(2*M_PI));//Around 2PI

    if(matchingScore>circle_data_ratio)
    {
      acv_CircleFit cf ;

      circleRefine(s_points,&cf);
      contourGrid.getContourPointsWithInCircleContour(cf.circle.circumcenter.X,cf.circle.circumcenter.Y,cf.circle.radius,epsilon2,
        s_intersectIdxs,s_points);
      matchingScore =(float)s_points.size() / cf.circle.radius/((float)(2*M_PI));//Around 2PI
      circleRefine(s_points,&cf);
      cf.matching_pts=s_points.size();
      *ret_cf = cf;
      return true;
    }
    return false;
}

int CircleFitTest(contour_grid &contourGrid,
    const acv_XY* dataArr6,
    float circle_data_ratio, vector<acv_CircleFit> &detectedCircles)
{
    acv_XY cc = acvCircumcenter(dataArr6[0],dataArr6[1],dataArr6[2]);
    acv_XY cc2 = acvCircumcenter(dataArr6[3],dataArr6[4],dataArr6[5]);
    acv_XY cc_diff={.X=cc2.X-cc.X,.Y=cc2.Y-cc.Y};
    if(!isnormal(cc_diff.X) || !isnormal(cc_diff.Y) )return -1;

    if(cc_diff.X*cc_diff.X+cc_diff.Y*cc_diff.Y>0.5)
    {
      return -1;
    }

    float radius = acvDistance(cc,dataArr6[1]);
    radius += acvDistance(cc2,dataArr6[4]);
    radius/=2;


    cc.X=(cc.X+cc2.X)/2;
    cc.Y=(cc.Y+cc2.Y)/2;

    acv_Circle c = {.circumcenter=cc, .radius=radius};

    acv_CircleFit cf;

    if(CircleFitTest(contourGrid,c,&cf,3,1.5,circle_data_ratio) == true)
    {
      detectedCircles.push_back(cf);
    }
    return 0;
}

float SecRegionCircleFit(contour_grid &contourGrid, int secX,int secY,int secW,int secH,
  int dataSizeMinThre, float circle_data_ratio,
  float sampleRate, vector<acv_CircleFit> &detectedCircles)
{

  int SecRSize = contourGrid.getGetSectionRegionDataSize(secX,secY,secW,secH);

  if(SecRSize<dataSizeMinThre)return 0;
  int SampleNumber = sampleRate*SecRSize*SecRSize;

  acv_XY sampleC[6];
  for(int i=0;i<SampleNumber;i++)
  {
    for(int j=0;j<6;j++)
    {
      sampleC[j]=*contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    }

    CircleFitTest(contourGrid,sampleC,circle_data_ratio,detectedCircles);
  }
  return 0;
}


float ContourDataCircleFit(contour_grid &contourGrid, acv_XY *innerCornorContour, int conL,
  int SampleNumber, float circle_data_ratio, vector<acv_CircleFit> &detectedCircles)
{
  acv_XY sampleC[6];
  for(int i=0;i<SampleNumber;i++)
  {

    for(int j=0;j<6;j++)
    {
      sampleC[j]=innerCornorContour[valueWarping(rand(),conL)];
    }

    CircleFitTest(contourGrid,sampleC,circle_data_ratio,detectedCircles);
  }
  return 0;
}

bool LineFitTest(contour_grid &contourGrid,
    acv_Line line,acv_LineFit *ret_lf,float epsilon1,float epsilon2,
    float minInBoundPoints)
{
  static vector<int> s_intersectIdxs;
  static vector<acv_XY> s_points;

  contourGrid.getContourPointsWithInLineContour(line,epsilon1,s_intersectIdxs,s_points);
  if(s_points.size()>minInBoundPoints)
  {
    acvFitLine(&s_points[0], s_points.size(),&line);

    contourGrid.getContourPointsWithInLineContour(line,epsilon2,s_intersectIdxs,s_points);
    if(s_points.size()>minInBoundPoints)
    {
      acvFitLine(&s_points[0], s_points.size(),&line);
      acv_LineFit lf ={.line = line,.matching_pts = s_points.size()};
      *ret_lf = lf;
      return true;
    }
  }
  return false;
}

float SecRegionLineFit(contour_grid &contourGrid, int secX,int secY,int secW,int secH,
  int dataSizeMinThre,float simThres, float sampleRate, vector<acv_LineFit> &detectedLines)
{
  int SecRSize = contourGrid.getGetSectionRegionDataSize(secX,secY,secW,secH);

  if(SecRSize<dataSizeMinThre)return 0;
  int SampleNumber = sampleRate*SecRSize*SecRSize;
  float maxMatchingScore=0;
  for(int i=0;i<SampleNumber;i++)
  {
    acv_XY pts[4];
    pts[0] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    pts[1] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    pts[2] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    pts[3] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
    acv_Line line1,line2;
    acvFitLine(pts  , 2,&line1);
    acvFitLine(pts+2, 2,&line2);
    float epsilon =3;
    if(LineSimilarity(line1,line2,epsilon) < simThres)continue;

    acvFitLine(pts, 4,&line1);
    float similarity=0;
    int sim_idx=findTheMostSimilarLineIdx(line1,detectedLines,epsilon,&similarity);
    if(similarity>simThres/5)
    {
      continue;
    }
    //printf("%f\n",similarity);
    acv_LineFit lf;
    if(LineFitTest(contourGrid,line1,&lf,0.5,0.25,80) == true)
    {
      detectedLines.push_back(lf);
    }

  }
  return maxMatchingScore;
}


void CircleDetect(acvImage *img,acvImage *buff)
{

    static vector<acv_XY> extractedContour;
    static vector<acv_XY> innerCornorContour;
    static vector<acv_XY> lineContour;

    static vector<acv_CircleFit> detectedCircles;
    static vector<acv_LineFit> detectedLines;
    int grid_size = 50;
    static contour_grid inward_curve_grid(grid_size,img->GetWidth(),img->GetHeight());
    static contour_grid straight_line_grid(grid_size,img->GetWidth(),img->GetHeight());

    extractedContour.resize(0);
    innerCornorContour.resize(0);
    lineContour.resize(0);
    inward_curve_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());
    straight_line_grid.RESET(grid_size,img->GetWidth(),img->GetHeight());


    acvCloneImage(img, buff, -1);



    std::vector<acv_LabeledData> ldData;


    clock_t t = clock();



    acvCloneImage( buff,img, -1);


    acvDrawBlock(img, 1, 1, img->GetWidth() - 2, img->GetHeight() - 2);

    acvComponentLabeling(img);
    acvLabeledRegionInfo(img, &ldData);
    ldData[1].area = 0;

    //Delete the object that has less than certain amount of area on ldData
    acvRemoveRegionLessThan(img, &ldData, 120);


    acvCloneImage( buff,img, -1);

    BYTE *OutLine, *OriLine;
    for (int i = 0; i < img->GetHeight(); i++)
    {
        OriLine = img->CVector[i];
        uint8_t pre_pix = 255;
        for (int j = 0; j < buff->GetWidth(); j++,OriLine+=3)
        {
          if(pre_pix==255 && OriLine[0] == 0)//White to black
          {
            extractedContour.resize(0);
            acvContourExtraction(img, j, i, 1, 128, 1, searchType_C_W2B,extractedContour);
            ContourFilter(extractedContour,innerCornorContour,lineContour);
          }
          else if(pre_pix==0 && OriLine[0] == 255)//black to white
          {
            extractedContour.resize(0);
            acvContourExtraction(img, j-1, i, 1, 128, 1, searchType_C_B2W,extractedContour);
            ContourFilter(extractedContour,innerCornorContour,lineContour);
          }

          pre_pix= OriLine[0];
        }
    }

    for (int i = 0; i < innerCornorContour.size(); i++)
    {
      inward_curve_grid.push(innerCornorContour[i]);
    }
    for (int i = 0; i < lineContour.size(); i++)
    {
      straight_line_grid.push(lineContour[i]);
    }



//inward_curve_grid  straight_line_grid
    int gridG_W = 2;
    int gridG_H = 2;

    detectedCircles.resize(0);
    detectedLines.resize(0);

    {
      {
        acv_Circle init_guess;
        init_guess.circumcenter.X=125;
        init_guess.circumcenter.Y=141;
        init_guess.radius=72;

        acv_CircleFit result;
        if(CircleFitTest(inward_curve_grid,init_guess,&result,5,1.5,0.3) == true)
        {
          detectedCircles.push_back(result);
        }
      }

      {
        straight_line_grid.setSecROI(2,1,5,1);
        acv_XY p1={.X=157,.Y=55};
        acv_XY p2={.X=330,.Y=59};
        acv_XY ps[]={p1,p2};
        acv_Line init_guess;
        acvFitLine(ps, 2, &init_guess);
        acv_LineFit result;
        if(LineFitTest(straight_line_grid,init_guess,&result,4,1,50) == true)
        {
          detectedLines.push_back(result);
        }
      }
    }



    /*for(int i=0;i<sizeof(circleRegion)/sizeof(circleRegion[0]);i++)
    {
      SecRegionCircleFit(inward_curve_grid,
        circleRegion[i].X,circleRegion[i].Y,
        circleRegion[i].W,circleRegion[i].H,
        40,0.2,0.01,detectedCircles);

    }*/




    /*
    struct XXXX lineRegion[]={
      {3,0,5,2},
    };

    for(int i=0;i<sizeof(lineRegion)/sizeof(lineRegion[0]);i++)
    {

      SecRegionLineFit(straight_line_grid,
        lineRegion[i].X,lineRegion[i].Y,
        lineRegion[i].W,lineRegion[i].H,
        40,0.8,0.005,detectedLines);
    }*/





    /*for(int i=0;i<inward_curve_grid.getRowSize()-gridG_H;i++)
    {
      for(int j=0;j<inward_curve_grid.getColumSize()-gridG_W;j++)
      {
        //inward_curve_grid.setSecROI(j,i,gridG_W,gridG_H);
        //straight_line_grid.setSecROI(j,i,gridG_W,gridG_H);
        SecRegionCircleFit(inward_curve_grid, j,i,gridG_W,gridG_H,40,0.2,0.01,detectedCircles);
        SecRegionLineFit(straight_line_grid, j,i,gridG_W,gridG_H,40,0.8,0.005,detectedLines);
      }
    }*/


    refineMatchedCircle(detectedCircles,0.8);


    t = clock() - t;
    printf("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);

    for(int i=0;i<15;i++)for(int j=0;j<15;j++)
    {
      acvDrawBlock(buff, j*grid_size,i*grid_size,(j+1)*grid_size,(i+1)*grid_size);
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


    for(int i=0;i<straight_line_grid.dataSize();i++)
    {
        const acv_XY* p2 = straight_line_grid.get(i);
        int X = round(p2->X);
        int Y = round(p2->Y);
        {
              buff->CVector[Y][X*3]=0;
              buff->CVector[Y][X*3+2]=255;
              buff->CVector[Y][X*3+1]=255;
        }
    }
    for(int i=0;i<detectedCircles.size();i++)
    {
        if(detectedCircles[i].s>0.9)
        {
          printf(">>SKIP...\n");
          continue;
        }
        acvDrawCircle(buff,
          detectedCircles[i].circle.circumcenter.X, detectedCircles[i].circle.circumcenter.Y,
          detectedCircles[i].circle.radius,
          20,255, 0, 0);

    }

    for(int i=0;i<detectedLines.size();i++)
    {
      acv_Line line = detectedLines[i].line;
      /*printf(">>%f %f %f %f\n",
        line.line_anchor.X,line.line_anchor.Y,
        line.line_vec.X,line.line_vec.Y
      );*/
      float mult=100;
        acvDrawLine(buff,
          line.line_anchor.X-mult*line.line_vec.X,
          line.line_anchor.Y-mult*line.line_vec.Y,
          line.line_anchor.X+mult*line.line_vec.X,
          line.line_anchor.Y+mult*line.line_vec.Y,
          255,0,128);

    }

}
