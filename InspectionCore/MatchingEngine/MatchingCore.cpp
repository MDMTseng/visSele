
#include "include_priv/MatchingCore.h"
#include <time.h>

#include "logctrl.h"
#include "circleFitting.h"
#include <acvImage_SpDomainTool.hpp>

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

int acvContourExtraction(acvImage *Pic, int FromX, int FromY, BYTE B, BYTE G, BYTE R, char searchType, vector<ContourFetch::ptInfo> &contour)
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
        ContourFetch::ptInfo pt={
          pt:{.X=(float)NowPos[0],.Y=(float)NowPos[1]},
          
          sobel:{.X=0,.Y=0}
          };
        pt.curvature=0;
        contour.push_back(pt);
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
float acvPoint3Angle(acv_XY p1,acv_XY pc,acv_XY p2)
{
  acv_XY v1={.X=p1.X-pc.X,.Y=p1.Y-pc.Y};
  acv_XY v2={.X=pc.X-p2.X,.Y=pc.Y-p2.Y};
  return acvVectorAngle(v1,v2);
}


ContourGrid::ptInfo refineEdgeInfo(acvImage *grayLevel,ContourGrid::ptInfo ptinfo,int searching_limit)
{


  //TODO:
  return ptinfo;
}

void ContourFilter(acvImage *grayLevel,vector<ContourFetch::ptInfo> &contour)
{
    const int L = contour.size();
    if(L==0)return;

  
    // for(int i=0;i<L;i++)
    // {
    //   contour[i] = refineEdgeInfo(grayLevel,contour[i],3);
    // }


    float crossP_LF_sum=0;

    const int Dist=3;
    const int LP_hWindow=1;

    float epsilon=0.05;

    float crossPHist[LP_hWindow*2];
    int crossPHist_head=0;

    for(int i=-LP_hWindow ;i<LP_hWindow;i++)
    {

      float angle=acvPoint3Angle(
        contour[valueWarping(i-Dist,L)].pt,
        contour[valueWarping(i     ,L)].pt,
        contour[valueWarping(i+Dist,L)].pt);

      crossPHist[i+LP_hWindow] =angle;
      crossP_LF_sum+=angle;
    }
    crossPHist_head=0;

    for(int i=0;i<L;i++)
    {

      //Filter out Non-inward contour
      //Cross product
      acv_XY headPT = contour[valueWarping(i+LP_hWindow+Dist,L)].pt;
      acv_XY centPT = contour[valueWarping(i+LP_hWindow,L)].pt;
      acv_XY tailPT = contour[valueWarping(i+LP_hWindow-Dist,L)].pt;

      float angle=acvPoint3Angle(tailPT,centPT,headPT);


      crossP_LF_sum=crossP_LF_sum+angle;

      float crossP_LF=crossP_LF_sum/(LP_hWindow*2+1);
      crossP_LF_sum-=crossPHist[crossPHist_head];
      crossPHist[crossPHist_head] = angle;
      crossPHist_head = valueWarping(crossPHist_head+1,LP_hWindow*2);


      //refineEdgeInfo(Pic,ContourGrid::ptInfo *inout_ptinfo,int searching_limit)
      //If the cross product is more than -epsilon(the epsilon is margin to filter out straight line)
      //if the low filtered cross product is more than 0 (history shows it's most likely an outward contour)
      ContourFetch::ptInfo ptinfo=contour[i];
      ptinfo.curvature = crossP_LF;

      {
        acv_XY dir={
          X:headPT.X - tailPT.X,
          Y:headPT.Y - tailPT.Y
        };
        
        dir = acvVecNormalize(dir);
        ptinfo.contourDir = dir;
        contour[i]=ptinfo;
      }

      
      /*if(crossP_LF<-7*epsilon){
        continue;
      }//Inner curve
      if(crossP_LF>epsilon){
        innerCornorContour.push_back(contour[i]);
      }//Inner curve
      if(crossP_LF<epsilon && crossP_LF>-epsilon ){
        lineContour.push_back(contour[i]);
      }//Line*/

    }
}

void circleRefine(vector<ContourFetch::ptInfo> &pointsInRange,int Len,acv_CircleFit *circleF)
{
  static Data CircleFitData(2000);
  int skip=1;
  //CircleFitData.resize_force(0);
  CircleFitData.resize(pointsInRange.size()/skip);
  
  float sum_edgeRsp=0;
  for(int i=0;i<CircleFitData.size();i++)
  {
    sum_edgeRsp+=pointsInRange[i*skip].edgeRsp;
  }
  for(int i=0;i<CircleFitData.size();i++)
  {
    CircleFitData.X[i]=pointsInRange[i*skip].pt.X;
    CircleFitData.Y[i]=pointsInRange[i*skip].pt.Y;
    //printf(">>>>>>>%d>>>>>>> %f\n",i,pointsInRange[i*skip].edgeRsp);
    if(sum_edgeRsp==0)
    {
      CircleFitData.W[i]=1;
    }
    else
    {
      CircleFitData.W[i]=pointsInRange[i*skip].edgeRsp;
    }
  }

  Circle circle;
  circle = CircleFitByHyper (CircleFitData);
  circleF->circle.circumcenter.X = circle.a;
  circleF->circle.circumcenter.Y = circle.b;
  circleF->circle.radius = circle.r;
  circleF->matching_pts = pointsInRange.size();
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

  float sim = line1.line_vec.X*line2.line_vec.X+line1.line_vec.Y*line2.line_vec.Y;
  if(sim<0)sim-=sim;
  float dist = acvDistance(line1, line2.line_anchor);
  if(!isnormal(dist) && dist!=0)
  {
    return 0.0002;
  }
  if(dist>epsilon)
  {
    return 0.0003;
  }
  sim = sim*(epsilon-dist)/epsilon;
  return sim;
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

int refineMatchedCircle(vector<acv_CircleFit> &circleList,float simThres,float sigmaThres)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<((int)circleList.size());i++)
  {
    if(circleList[i].matching_pts==0)continue;

    if(circleList[i].s>sigmaThres)
    {
      //circleList[i].matching_pts=0;
      //continue;
    }
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
    if(circleList[i].matching_pts==0)
    {
      //printf(">>>>%d\n",w_h);
      continue;
    }
    if(w_h!=i)
      circleList[w_h]=circleList[i];
    w_h++;
  }
  circleList.resize(w_h);
}
int refineMatchedLine(vector<acv_LineFit> &LineList,float simThres,float sigmaThres)
{
  int idx=-1;
  float max_sim=0;

  for(int i=0;i<(LineList.size());i++)
  {
    if(LineList[i].matching_pts==0)continue;
    float matchingScore1 = LineList[i].matching_pts;
    if(LineList[i].s>sigmaThres)
    {
      LineList[i].matching_pts=0;
      continue;
    }
    for(int j=i+1;j<LineList.size();j++)
    {
      if(LineList[j].matching_pts==0)continue;
      float sim= LineSimilarity(LineList[i].line,LineList[j].line,5);
      if(sim<simThres)continue;

      float matchingScore2 = LineList[j].matching_pts;
      if(matchingScore2>matchingScore1)
      {
        LineList[i].matching_pts = 0;
        break;
      }
      else
      {
        LineList[j].matching_pts = 0;
      }
    }
  }

  int w_h=0;
  for(int i=0;i<LineList.size();i++)
  {
    if(LineList[i].matching_pts==0)continue;
    if(w_h!=i)
      LineList[w_h]=LineList[i];
    w_h++;
  }
  LineList.resize(w_h);
}

// bool CircleFitTest(ContourGrid &contourGrid,
//     acv_Circle c,acv_CircleFit *ret_cf,float epsilon1,float epsilon2,
//     float circle_data_ratio)
// {

//     static vector<int> s_intersectIdxs;
//     static vector<ContourGrid::ptInfo> s_points;
//     contourGrid.getContourPointsWithInCircleContour(c.circumcenter.X,c.circumcenter.Y,c.radius,
//       0,2*M_PI,0,
//       epsilon1,
//       s_intersectIdxs,s_points);

//     float matchingScore =(float)s_points.size() / c.radius/((float)(2*M_PI));//Around 2PI

//     if(matchingScore>circle_data_ratio)
//     {
//       acv_CircleFit cf ;

//       circleRefine(s_points,s_points.size(),&cf);
//       contourGrid.getContourPointsWithInCircleContour(
//         cf.circle.circumcenter.X,
//         cf.circle.circumcenter.Y,
//         cf.circle.radius,0,
//         0,
//         2*M_PI,
//         epsilon2,
//         s_intersectIdxs,s_points);
//       matchingScore =(float)s_points.size() / cf.circle.radius/((float)(2*M_PI));//Around 2PI
//       circleRefine(s_points,s_points.size(),&cf);
//       cf.matching_pts=s_points.size();
//       *ret_cf = cf;
//       return true;
//     }
//     return false;
// }

// int CircleFitTest(ContourGrid &contourGrid,
//     const acv_XY* dataArr6,
//     float circle_data_ratio, vector<acv_CircleFit> &detectedCircles)
// {
//     acv_XY cc = acvCircumcenter(dataArr6[0],dataArr6[1],dataArr6[2]);
//     acv_XY cc2 = acvCircumcenter(dataArr6[3],dataArr6[4],dataArr6[5]);
//     acv_XY cc_diff={.X=cc2.X-cc.X,.Y=cc2.Y-cc.Y};
//     if(!isnormal(cc_diff.X) || !isnormal(cc_diff.Y) )return -1;

//     if(cc_diff.X*cc_diff.X+cc_diff.Y*cc_diff.Y>0.5)
//     {
//       return -1;
//     }

//     float radius = acvDistance(cc,dataArr6[1]);
//     radius += acvDistance(cc2,dataArr6[4]);
//     radius/=2;


//     cc.X=(cc.X+cc2.X)/2;
//     cc.Y=(cc.Y+cc2.Y)/2;

//     acv_Circle c = {.circumcenter=cc, .radius=radius};

//     acv_CircleFit cf;

//     if(CircleFitTest(contourGrid,c,&cf,3,1.5,circle_data_ratio) == true)
//     {
//       detectedCircles.push_back(cf);
//     }
//     return 0;
// }

// float SecRegionCircleFit(ContourGrid &contourGrid, int secX,int secY,int secW,int secH,
//   int dataSizeMinThre, float circle_data_ratio,
//   float sampleRate, vector<acv_CircleFit> &detectedCircles)
// {

//   int SecRSize = contourGrid.getGetSectionRegionDataSize(secX,secY,secW,secH);

//   if(SecRSize<dataSizeMinThre)return 0;
//   int SampleNumber = sampleRate*SecRSize*SecRSize;

//   acv_XY sampleC[6];
//   for(int i=0;i<SampleNumber;i++)
//   {
//     for(int j=0;j<6;j++)
//     {
//       sampleC[j]=*contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
//     }

//     CircleFitTest(contourGrid,sampleC,circle_data_ratio,detectedCircles);
//   }
//   return 0;
// }


// float ContourDataCircleFit(ContourGrid &contourGrid, acv_XY *innerCornorContour, int conL,
//   int SampleNumber, float circle_data_ratio, vector<acv_CircleFit> &detectedCircles)
// {
//   acv_XY sampleC[6];
//   for(int i=0;i<SampleNumber;i++)
//   {

//     for(int j=0;j<6;j++)
//     {
//       sampleC[j]=innerCornorContour[valueWarping(rand(),conL)];
//     }

//     CircleFitTest(contourGrid,sampleC,circle_data_ratio,detectedCircles);
//   }
//   return 0;
// }

ContourFetch::ptInfo* findEndPoint(acv_Line line, int signedness, vector<ContourFetch::ptInfo> &points)
{
  signedness=(signedness>0)?1:-1;
  int maxi=-1;
  float max_dist=0;

  //Find normal vector
  {
    float tmp=line.line_vec.X;
    line.line_vec.X=line.line_vec.Y;
    line.line_vec.Y=-tmp;
  }

  for(int i=0;i<points.size();i++)
  {
    float dist = signedness*acvDistance_Signed(line, points[i].pt);
    if(max_dist<dist)
    {
      max_dist=dist;
      maxi=i;
    }
  }
  return (maxi>=0)?&points[maxi]:NULL;
}

// bool LineFitTest(ContourGrid &contourGrid,
//     acv_Line line,acv_LineFit *ret_lf,float epsilon1,float epsilon2,
//     float minInBoundPoints)
// {
//   static vector<int> s_intersectIdxs;
//   static vector<ContourGrid::ptInfo> s_points;

//   contourGrid.getContourPointsWithInLineContour(line,1000000,epsilon1,1,s_intersectIdxs,s_points);
//   if(s_points.size()>minInBoundPoints)
//   {
//     //acvFitLine(&s_points[0], s_points.size(),&line,NULL);
    
//     acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, s_points.size(),&line,NULL);

//     contourGrid.getContourPointsWithInLineContour(line,1000000,epsilon2,1,s_intersectIdxs,s_points);
//     if(s_points.size()>minInBoundPoints)
//     {
//       float sigma;
//       acvFitLine(&(s_points[0].pt),sizeof(ContourGrid::ptInfo), NULL,0, s_points.size(),&line,NULL);

//       ContourGrid::ptInfo *end_pos=findEndPoint(line, 1, s_points);
//       ContourGrid::ptInfo *end_neg=findEndPoint(line, -1, s_points);

//       acv_LineFit lf;
//       lf.line=line;
//       lf.matching_pts=s_points.size();
//       lf.s=sigma;
//       if(end_pos)lf.end_pos=end_pos->pt;
//       if(end_neg)lf.end_neg=end_neg->pt;
//       *ret_lf = lf;
//       return true;
//     }
//   }
//   return false;
// }

// float SecRegionLineFit(ContourGrid &contourGrid, int secX,int secY,int secW,int secH,
//   int dataSizeMinThre,float simThres, float sampleRate, float matching_margin, vector<acv_LineFit> &detectedLines)
// {
//   int SecRSize = contourGrid.getGetSectionRegionDataSize(secX,secY,secW,secH);

//   if(SecRSize<dataSizeMinThre)return 0;
//   int SampleNumber = sampleRate*SecRSize*SecRSize;
//   float maxMatchingScore=0;
//   for(int i=0;i<SampleNumber;i++)
//   {
//     acv_XY pts[4];
//     pts[0] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
//     pts[1] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
//     pts[2] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
//     pts[3] = *contourGrid.getGetSectionRegionData(secX,secY,secW,secH,valueWarping(rand(),SecRSize));
//     acv_Line line1,line2;
//     bool is_a_line;
//     is_a_line=acvFitLine(pts  , 2,&line1,NULL);
//     if(!is_a_line)continue;
//     is_a_line=acvFitLine(pts+2, 2,&line2,NULL);
//     if(!is_a_line)continue;
//     float epsilon =3;
//     if(LineSimilarity(line1,line2,epsilon) < simThres)continue;

//     is_a_line=acvFitLine(pts, 4,&line1,NULL);
//     if(!is_a_line)continue;
//     float similarity=0;
//     int sim_idx=findTheMostSimilarLineIdx(line1,detectedLines,epsilon,&similarity);
//     if(similarity>simThres)
//     {
//       continue;
//     }
//     //printf("%f\n",similarity);
//     acv_LineFit lf;
//     if(LineFitTest(contourGrid,line1,&lf,matching_margin*2,matching_margin,40) == true)
//     {
//       findTheMostSimilarLineIdx(lf.line,detectedLines,epsilon, &similarity);
//       if(similarity<0.90)
//         detectedLines.push_back(lf);
//     }

//   }
//   return maxMatchingScore;
// }

void extractContourDataToContourGrid(acvImage *grayLevelImg,acvImage *labeledImg,int grid_size,ContourFetch &edge_grid, int scanline_skip)
{

  edge_grid.RESET();


  if(scanline_skip<0)return;

  BYTE *OutLine, *OriLine;
  int contourIdx=0;
  //ldData[i].
  for (int i = 3; i < labeledImg->GetHeight()-3; i+=scanline_skip)
  {
      OriLine = labeledImg->CVector[i]+3*3;

      uint8_t pre_pix = 255;
      uint8_t cur_pix;
      for (int j = 3; j < labeledImg->GetWidth()-3; j++,OriLine+=3)
      {
        cur_pix = OriLine[2];
        if(pre_pix==255 && cur_pix == 0)//White to black
        {
          edge_grid.tmpXYSeq.resize(0);
          acvContourExtraction(labeledImg, j, i, 1, 128, 1, searchType_C_W2B,edge_grid.tmpXYSeq);
          ContourFilter(grayLevelImg,edge_grid.tmpXYSeq);
        }
        else if(pre_pix==0 && cur_pix == 255)//black to white
        {
          edge_grid.tmpXYSeq.resize(0);
          acvContourExtraction(labeledImg, j-1, i, 1, 128, 1, searchType_C_B2W,edge_grid.tmpXYSeq);
          ContourFilter(grayLevelImg,edge_grid.tmpXYSeq);
        }
        if(edge_grid.tmpXYSeq.size()>0)
        {
          for(int k=0;k<edge_grid.tmpXYSeq.size();k++)
          {
            edge_grid.push(contourIdx,edge_grid.tmpXYSeq[k]);
          }
          contourIdx++;
          edge_grid.tmpXYSeq.resize(0);
        }

        pre_pix= cur_pix;
      }
  }


}

int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,acv_XY *ret_point_opt,float *ret_edge_response);
int EdgePointOpt2(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int range,float thres,acv_XY *ret_point_opt,float *ret_edge_response);

void spline9X(float *f,float *x,int fL,float *s,float *h)
{
    int n,i,j,k;
    const int L = 9;
    float a,b,c,d,sum,F[L],p,m[L][L]={0},temp;
    n = fL;


    for(i=n-1;i>0;i--)
    {
        F[i]=(f[i]-f[i-1])/(x[i]-x[i-1]);
        h[i-1]=x[i]-x[i-1];
    }
    
    //*********** formation of h, s , f matrix **************//
    for(i=1;i<n-1;i++)
    {
        m[i][i]=2*(h[i-1]+h[i]);
        if(i!=1)
        {
            m[i][i-1]=h[i-1];
            m[i-1][i]=h[i-1];
        }
        m[i][n-1]=6*(F[i+1]-F[i]);
    }

    /*
    for(i=0;i<n;i++)
    {
        printf(" %f",f[i]);
    }
    printf("....%d\n",n);
    
    for(i=0;i<n;i++)
    {
      for(j=0;j<n;j++)
      {
        printf(" %f",m[i][j]);
      }
      printf("\n");
    }*/
    

    //***********  forward elimination **************//

    for(i=1;i<n-2;i++)
    {
        temp=(m[i+1][i]/m[i][i]);
        for(j=1;j<=n-1;j++)
        {
            m[i+1][j]-=temp*m[i][j];
        }
    }

    //*********** back ward substitution *********//
    for(i=n-2;i>0;i--)
    {
        sum=0;
        for(j=i;j<=n-2;j++)
            sum+=m[i][j]*s[j];
        s[i]=(m[i][n-1]-sum)/m[i][i];
    }
}

void spline9_edge(float *f,int fL,float *edgeX,float *ret_edge_response)
{
    int n,i,j,k;
    const int L = 9;
    float h[L],a,b,c,d,s[L]={0},x[L];
    n = fL;

    for(i=0;i<n;i++)
    {
        x[i]=i;
        //printf("%f\n",f[i]);
    }

    spline9X(f,x,fL,s,h);



    float maxEdge_response = 0;
    float maxEdge_offset=NAN;
    
    float edgePowerInt = 0;
    float edgeCentralInt = 0;
    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        b=s[i]/2;
        c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
        d=f[i];
        
        //f' = 3ax^2+2bx + c
        //pow(f',2) = 9aax^4 + 12abx^3 + (6ac+4bb)x^2 + 4cbx +cc
        float edgePower = 
          (9*a*a)/5 + 
          (12*a*b)/4 + 
          (6*a*c+4*b*b)/3 +
          (4*c*b)/2+
          (c*c)/1;//Integeral(pow(f',2),0,1)

    
        float edgeCentral = 
          (9*a*a)/6 + 
          (12*a*b)/5 + 
          (6*a*c+4*b*b)/4 +
          (4*c*b)/3+
          (c*c)/2;//Integeral(pow(f'(x),2)x,0,1)
        edgeCentral/=edgePower;
        edgeCentralInt+=edgePower*(i+edgeCentral);

        edgePowerInt+=edgePower;
        bool zeroCross = (s[i+1]*s[i])<0;

        if(zeroCross || (s[i]==0 && i!=0))
        {
            float offset = s[i]/(s[i]-s[i+1]);
            float xi =offset;
            float firDir = 3*a*xi*xi+2*b*xi+c;
            float secDir = 6*a*xi+2*b;
            float abs_firDir = abs(firDir);
            if(maxEdge_response<abs_firDir)
            {
                maxEdge_response=abs_firDir;
                maxEdge_offset = i+offset;
            }
            //printf("cross: offset:%f\n",i+offset);
        }
    }



    float edgeSide1 = 0;
    float edgeSide2 = 0;
    for(i=0;i<n;i++)
    {
      if(i<maxEdge_offset)
      {
        edgeSide1+=f[i];
      }
      else
      {
        edgeSide2+=f[i];
      }
    }
    edgeSide1/=(1+(int)maxEdge_offset);
    edgeSide2/=(n-1-(int)maxEdge_offset);
    float edgeSideDiff = edgeSide1-edgeSide2;

    if(edgeSideDiff<0)edgeSideDiff=-edgeSideDiff;
    //edgeSideDiff=edgeSideDiff*edgeSideDiff;
    edgeSideDiff-=20;
    if(edgeSideDiff<0)edgeSideDiff=0;

    edgeCentralInt/=edgePowerInt;
    float edgeRange=1;
    float BGEdgePower = (edgePowerInt-maxEdge_response*maxEdge_response*edgeRange)/(n-1-edgeRange);
    if(BGEdgePower<0)BGEdgePower=0;
    BGEdgePower=sqrt(BGEdgePower);
    float SNR = maxEdge_response/(BGEdgePower+0.1);
    //printf("MAX rsp>>> %f %f %f\n",maxEdge_response,BGEdgePower, maxEdge_response/(BGEdgePower+0.1));
    
    //LOGV("%f %f %f %f %f %f %f --- %f:",f[0],f[1],f[2],f[3],f[4],f[5],f[6],edgeSideDiff);
    //LOGV("edgeCentralInt::%f %f",edgeCentralInt,maxEdge_offset);
    if(0)
    {
      *edgeX = edgeCentralInt;
      *ret_edge_response = edgePowerInt;
    }
    else
    {
      *edgeX = edgeCentralInt;
      *ret_edge_response = edgeSideDiff;
    }

}
void spline9_max(float *f,int fL,int div,float *ret_maxf,float *ret_maxf_x)
{
    int n,i,j,k;
    const int L = 9;
    float h[L],a,b,c,d,s[L]={0},x[L];
    n = fL;

    for(i=0;i<n;i++)
    {
        x[i]=i;
        //printf("%f\n",f[i]);
    }

    spline9X(f,x,fL,s,h);


    
    float maxf=f[0];
    float maxf_x=0;
    for(i=0;i<n-1;i++)
    {
        a=(s[i+1]-s[i])/(6*h[i]);
        b=s[i]/2;
        c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
        d=f[i];
        //f = ax^3 + bx^2 + cx + d
        //f' = 3ax^2+2bx + c
        //Find max f = 
        if(maxf<f[i])
        {
          maxf=f[i];
          maxf_x=i;
        }
        for(j=1;j<div;j++)
        {
          float x_=(float)j/div;

          float val = a*x_*x_*x_ + b*x_*x_ + c*x_ +d;

          if(maxf<val)
          {
            maxf=val;
            maxf_x=i+x_;
          }
        }

    }
    *ret_maxf_x = maxf_x;
    *ret_maxf = maxf;

}
int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,acv_XY *ret_point_opt,float *ret_edge_response)
{
  if(ret_point_opt==NULL)return -1;
  
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=7;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);

  
  const int nM=1;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  acv_XY nvecBM = acvVecMult(nvec,-(float)(nM-1)/2);
  
  //gradVec= acvVecMult(gradVec,1);
  
  acv_XY  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  acv_XY bkpoint = curpoint;
  curpoint = acvVecAdd(curpoint,nvecBM);
  for(int i=0;i<GradTableL;i++)
  {
    float ptn = 0;
    acv_XY tmpCurPt=curpoint;
    for(int j=0;j<nM;j++)
    {
      ptn+= acvUnsignedMap1Sampling(graylevelImg, tmpCurPt, 0);
      tmpCurPt = acvVecAdd(tmpCurPt,nvec);
    }
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn/nM;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  float edgeX;
  spline9_edge(gradTable,GradTableL,&edgeX,ret_edge_response);


  
  /*for(int i=0;i<GradTableL;i++)
  {
    printf("%5.2f ",gradTable[i]);
    //gradTable[i]=0;
  }
  printf("...edgeX:%f ret_edge_rsp:%f\n",edgeX,*ret_edge_response);
  */
  //LOGV("<<%f",edgeX);
  if(edgeX!=edgeX)//NAN
  {
    return -1;
  }
  /*
  LOGV("%f %f %f %f %f %f %f <= %f",
  gradTable[0],
  gradTable[1],
  gradTable[2],
  gradTable[3],
  gradTable[4],
  gradTable[5],
  gradTable[6],
  edgeX
  );*/
  gradVec = acvVecMult(gradVec,edgeX);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);


  return 0;
}


acv_XY pointSobel(acvImage *graylevelImg,acv_XY point,int range)
{
  int X=point.X;
  int Y=point.Y;
  int offset=range;
  int I11 = graylevelImg->CVector[Y-offset][(X-offset)*3];
  int I12 = graylevelImg->CVector[Y-offset][(X)*3];
  int I13 = graylevelImg->CVector[Y-offset][(X+offset)*3];
  int I21 = graylevelImg->CVector[Y][(X-offset)*3];
  //int I22 = graylevelImg->CVector[Y][(X)*3];
  int I23 = graylevelImg->CVector[Y][(X+offset)*3];
  int I31 = graylevelImg->CVector[Y+offset][(X-offset)*3];
  int I32 = graylevelImg->CVector[Y+offset][(X)*3];
  int I33 = graylevelImg->CVector[Y+offset][(X+offset)*3];

  acv_XY sobel;
  //11 12 13
  //21  X 23
  //31 32 33
  sobel.X=(I13+I23*2+I33) - (I11+I21*2+I31);
  sobel.Y=(I31+I32*2+I33) - (I11+I12*2+I13);//sobel
  return sobel;
}


int EdgePointOpt2(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int range,float thres,acv_XY *ret_point_opt,float *ret_edge_response)
{
  if(ret_point_opt==NULL)return -1;
  
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=9;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);

  float subPixS=3;
  const int nM=range;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  acv_XY nvecBM = acvVecMult(nvec,-(float)(nM-1)/2);
  
  //gradVec= acvVecMult(gradVec,1);
  
  acv_XY  curpoint= acvVecMult(gradVec,-(float)(GradTableL-1)/2);
  curpoint = acvVecAdd(curpoint,point);
  acv_XY bkpoint = curpoint;
  curpoint = acvVecAdd(curpoint,nvecBM);

  nvec=acvVecMult(nvec,1.0f/subPixS);
  for(int i=0;i<GradTableL;i++)
  {
    float ptn = 0;
    acv_XY tmpCurPt=curpoint;
    for(int j=0;j<nM*subPixS;j++)
    {
      ptn+= acvUnsignedMap1Sampling(graylevelImg, tmpCurPt, 0);
      tmpCurPt = acvVecAdd(tmpCurPt,nvec);
    }
    //LOGV("%f<<%f,%f",ptn,curpoint.X,curpoint.Y);
    gradTable[i] = ptn/nM/subPixS;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  /*
  for(int i=0;i<GradTableL;i++)
  {
    gradTable[i]-=thres;
  }
  for(int i=0;i<GradTableL-1;i++)
  {
    if(gradTable[i]*gradTable[i+1]<0)
    {
      if(gradTable[i]<0)
      {
        gradTable[i]*=-1;
        gradTable[i+1]*=-1;
      }
      
      float edgeX = i+gradTable[i]/(gradTable[i]-gradTable[i+1]);
      *ret_edge_response=10;
      
      gradVec = acvVecMult(gradVec,edgeX);
      *ret_point_opt = acvVecAdd(bkpoint,gradVec);

      return 0;
    }
  }*/

  float edgeWSum=0;
  float edgePos=0;
  for(int i=0;i<GradTableL;i++)
  {
    
    float diff = gradTable[i]-thres;
    if(diff<0)diff=-diff;
    //if(diff>10)continue;
    float weight = 1/(diff*diff/100+1);
    //printf("%g ",weight);
    edgePos+=i*weight;
    edgeWSum+=weight;
  }
  //LOGV("edgeWSum:%f",edgeWSum);

  if(edgeWSum==0)return -1;
  edgePos/=edgeWSum;

  int edgeMargin=(int)edgePos+1;
  int edgeMarginS=0;
  if((edgeMargin)*2>GradTableL)
  {
    edgeMargin=GradTableL -(edgeMargin);
    edgeMarginS = GradTableL - (edgeMargin)*2;
  }


  edgeWSum=0;
  edgePos=0;
  
  for(int i=edgeMarginS;i<edgeMarginS+(edgeMargin)*2;i++)
  {
    
    float diff = gradTable[i]-thres;
    if(diff<0)diff=-diff;
    //if(diff>10)continue;
    float weight = 1/(diff*diff/100+1);
    //printf("%g ",weight);
    edgePos+=i*weight;
    edgeWSum+=weight;
  }
  edgePos/=edgeWSum;
  

  if(ret_edge_response)
  {

    int avgPart1=0;
    int avgPart2=0;

    {
      edgeMargin=(int)edgePos+1;
      edgeMarginS=0;
      if((edgeMargin)*2>GradTableL)
      {
        edgeMargin=GradTableL -(edgeMargin);
        edgeMarginS = GradTableL - (edgeMargin)*2;
      }

      if(edgeMargin>3)edgeMargin=3;//we just want to calculate the slop, so don't make it to wide
      for(int i=0;i<(edgeMargin);i++)
      {
        avgPart1+=gradTable[edgeMarginS+edgeMargin-i-1];
        avgPart2+=gradTable[edgeMarginS+edgeMargin+i];
      }
    }

    avgPart1-=avgPart2;
    if(avgPart1<0)avgPart1=-avgPart1;

    *ret_edge_response=((float)avgPart1)/(edgeMargin+1);
  }

  gradVec = acvVecMult(gradVec,edgePos);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);



  return 0;
}

void extractLabeledContourDataToContourGrid(acvImage *grayLevelImg,acvImage *labeledImg,int label,acv_LabeledData ldat,int thres,int grid_size,ContourFetch &edge_grid,int scanline_skip,FeatureManager_BacPac *bacpac)
{

  edge_grid.RESET();


  if(scanline_skip<0)return;

  int sX = (int)ldat.LTBound.X;
  int sY = (int)ldat.LTBound.Y;
  int eX = (int)ldat.RBBound.X;
  int eY = (int)ldat.RBBound.Y;
  LOGV("%d %d %d %d",sX,sY,eX,eY);
  BYTE *OutLine, *OriLine;

  _24BitUnion *lableConv;
  int contourIdx=0;
  //ldData[i].
  for (int i = sY; i < eY; i+=scanline_skip)
  {
      OriLine = &(labeledImg->CVector[i][sX*3]);

      uint8_t pre_pix = 255;
      uint8_t cur_pix;
      for (int j = sX; j < eX; j++,OriLine+=3)
      {
        cur_pix = OriLine[2];
        
        lableConv=(_24BitUnion*)OriLine;
        if(edge_grid.tmpXYSeq.size()!=0)
        {
          edge_grid.tmpXYSeq.resize(0);
        }

        if(pre_pix==255 && cur_pix == 0)//White to black
        {
          if(lableConv->_3Byte.Num==label)
          {
            acvContourExtraction(labeledImg, j, i, 1, 128, 1, searchType_C_W2B,edge_grid.tmpXYSeq);
          }
        }
        else if(pre_pix==0 && cur_pix == 255)//black to white
        {
          
          if(lableConv->_3Byte.Num==label)
          {
            acvContourExtraction(labeledImg, j-1, i, 1, 128, 1, searchType_C_B2W,edge_grid.tmpXYSeq);
          }
        }

        if(edge_grid.tmpXYSeq.size()!=0)
        {
          // for(int i=0;i<edge_grid.tmpXYSeq.size();i++)
          // {
            
          //   edge_grid.tmpXYSeq[i].pt=
          //     acvVecRadialDistortionRemove(
          //       edge_grid.tmpXYSeq[i].pt,param);
          // }



          if(edge_grid.tmpXYSeq.size()>0)
          {
            for(int k=0;k<edge_grid.tmpXYSeq.size();k++)
            {

              float edgeResponse;
              acv_XY ret_point_opt;
              acv_XY sobel = pointSobel(grayLevelImg,edge_grid.tmpXYSeq[k].pt,2);
              edge_grid.tmpXYSeq[k].sobel=sobel;


              
              //Check sobel intensity
              if(hypot(edge_grid.tmpXYSeq[k].sobel.X,edge_grid.tmpXYSeq[k].sobel.Y)<100)continue;

              // int ret_val = EdgePointOpt2(grayLevelImg,edge_grid.tmpXYSeq[k].sobel,
              //   edge_grid.tmpXYSeq[k].pt,3,thres,&ret_point_opt,&edgeResponse);

              int ret_val = EdgePointOpt(grayLevelImg,edge_grid.tmpXYSeq[k].sobel,edge_grid.tmpXYSeq[k].pt,
                &ret_point_opt,&edgeResponse);



              edge_grid.tmpXYSeq[k].pt=ret_point_opt;
              edge_grid.tmpXYSeq[k].edgeRsp = (edgeResponse<0)?-edgeResponse:edgeResponse;

              //bacpac->sampler.
              acv_XY xy=edge_grid.tmpXYSeq[k].pt;
              
              bacpac->sampler->img2ideal(&xy);// =acvVecRadialDistortionRemove( edge_grid.tmpXYSeq[k].pt,param);
              
              sobel=acvVecNormalize(sobel);
              float angle = atan2(sobel.Y,sobel.X);
              if(angle<0)angle+=M_PI*2;

              float offset=bacpac->sampler->angOffsetTable->sampleAngleOffset(angle);
              // printf("ang:%f  XY:%f,%f offset:%f\n",
              //   angle*180/M_PI,
              //   edge_grid.tmpXYSeq[k].pt.X,
              //   edge_grid.tmpXYSeq[k].pt.Y,
              //   offset);
              xy=acvVecAdd(xy,acvVecMult(sobel,offset));
              edge_grid.tmpXYSeq[k].pt =xy;
            }
            
            ContourFilter(grayLevelImg,edge_grid.tmpXYSeq);
            for(int k=0;k<edge_grid.tmpXYSeq.size();k++)
            {

              // acv_XY p =edge_grid.tmpXYSeq[k].pt;
              // int X = round(p.X);
              // int Y = round(p.Y);
              // {
              //   grayLevelImg->CVector[Y][X*3+0]=0;
              //   grayLevelImg->CVector[Y][X*3+1]=0;
              //   grayLevelImg->CVector[Y][X*3+2]=255;
              // }

              edge_grid.push(contourIdx,edge_grid.tmpXYSeq[k]);
            }
            contourIdx++;
            edge_grid.tmpXYSeq.resize(0);
          }
        }

        pre_pix= cur_pix;
      }
  }


}

// void MatchingCore_CircleLineExtraction(acvImage *img,acvImage *buff,std::vector<acv_LabeledData> &ldData,
//   std::vector<acv_CircleFit> &detectedCircles,std::vector<acv_LineFit> &detectedLines)
// {

//     clock_t t = clock();
//     int grid_size = 50;
//     static ContourGrid edge_grid(grid_size,img->GetWidth(),img->GetHeight());
//     acvCloneImage( img,buff, -1);

//     int scanline_skip=1;
//     extractContourDataToContourGrid(buff,buff,grid_size,edge_grid,scanline_skip);
// //edge_grid  straight_line_grid
//     int gridG_W = 3;
//     int gridG_H = 3;

//     detectedCircles.resize(0);
//     detectedLines.resize(0);

//     for(int i=0;i<edge_grid.getRowSize()-gridG_H;i++)
//     {
//       for(int j=0;j<edge_grid.getColumSize()-gridG_W;j++)
//       {
//         //edge_grid.setSecROI(j,i,gridG_W,gridG_H);
//         //straight_line_grid.setSecROI(j,i,gridG_W,gridG_H);
//         float matching_margin=2;
//         SecRegionCircleFit(edge_grid, j,i,gridG_W,gridG_H,40,0.2,0.01,detectedCircles);
//         SecRegionLineFit(edge_grid, j,i,gridG_W,gridG_H,40,0.9,0.05,matching_margin,detectedLines);
//       }
//     }


//     refineMatchedCircle(detectedCircles,0.8,1.2);

//     refineMatchedLine(detectedLines,0.9,0.8);
//     t = clock() - t;
//     logv("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);

//     for(int i=0;i<15;i++)for(int j=0;j<15;j++)
//     {
//       acvDrawBlock(buff, j*grid_size,i*grid_size,(j+1)*grid_size,(i+1)*grid_size);
//     }




//     for(int i=0;i<edge_grid.dataSize();i++)
//     {

//       const acv_XY p = edge_grid.get(i)->pt;
//       int X = round(p.X);
//       int Y = round(p.Y);
//       {
//             buff->CVector[Y][X*3]=255;
//             buff->CVector[Y][X*3+1]=255;
//       }


//     }

// }
