
#include "include_priv/MatchingCore.h"
#include <time.h>

#include "logctrl.h"
#include "circleFitting.h"
#include <acvImage_SpDomainTool.hpp>

#include "polyfit.h"

using namespace std;
enum searchType_C
{
  searchType_C_B2W,
  searchType_C_W2B
};


//return 0 => two real roots
//return 1 => single complex root => r0: real part r1: positive imaginary part
//aX^2+bx+c=0
int quadratic_roots(float a,float b,float c,float *r0,float*r1)
{
  double discriminant, root1, root2, realPart, imagPart;
  // printf("Enter coefficients a, b and c: ");
  // scanf("%lf %lf %lf", &a, &b, &c);

  discriminant = b * b - 4 * a * c;

  // condition for real and different roots
  if (discriminant > 0) {
      root1 = (-b + sqrt(discriminant)) / (2 * a);
      root2 = (-b - sqrt(discriminant)) / (2 * a);
      // printf("root1 = %.2lf and root2 = %.2lf", root1, root2);
      *r0=root1;
      *r1=root2;
      return 0;
  }

  // // condition for real and equal roots
  // else if (discriminant == 0) {
  //     root1 = root2 = -b / (2 * a);
  //     // printf("root1 = root2 = %.2lf;", root1);
  //     return 0;
  // }

  // if roots are not real
  else {
      realPart = -b / (2 * a);
      imagPart = sqrt(-discriminant) / (2 * a);
      if(-imagPart)imagPart=-imagPart;//keep it positive
      
      *r0=realPart;
      *r1=imagPart;
      
      return 1;
      // printf("root1 = %.2lf+%.2lfi and root2 = %.2f-%.2fi", realPart, imagPart, realPart, imagPart);
  }
}



inline int valueWarping(int v,int ringSize)
{
  v%=ringSize;
  return (v<0)?v+ringSize:v;
}

inline int valueSaturation(int v,int ringSize)
{
  if(v<0)return 0;
  if(v>ringSize-1)return ringSize-1;

  return v;
}

//return 0 => two real roots
//return 1 => single complex root => r0: real part r1: positive imaginary part
int quadratic_roots(float a,float b,float c,float *r0,float*r1);
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
          sobel:{.X=NAN,.Y=NAN},
          contourDir:{.X=NAN,.Y=NAN},
          pt_img:{.X=NAN,.Y=NAN},
          curvature:NAN,
          edgeRsp:0,
          tmp:NAN,
          // sobel:{.X=0,.Y=0}

          };

        pt.pt_img=pt.pt;
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

void ContourFilter(acvImage *grayLevel,vector<ContourFetch::ptInfo> &contour,float epsilon=0.05,int Dist=10)
{
    const int L = contour.size();
    if(L==0)return;

  
    // for(int i=0;i<L;i++)
    // {
    //   contour[i] = refineEdgeInfo(grayLevel,contour[i],3);
    // }


    float crossP_LF_sum=0;

    const int LP_hWindow=10;
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

int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int jump,acv_XY *ret_point_opt,float *ret_edge_response);
int EdgePointOpt2(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,int range,float thres,acv_XY *ret_point_opt,float *ret_edge_response);


int spline_max40(float *f,float *x,int fL,float *s,float *h)
{
    int n,i,j,k;
    
    const int L_MAX = 40;
    
    if(fL>L_MAX)
    {
      return -1;
    }
    float a,b,c,d,sum,F[L_MAX],p,m[L_MAX][L_MAX]={0},temp;
    n = fL;


    for(i=n-1;i>0;i--)
    {
      float x_diff=(x==NULL)?1:x[i]-x[i-1];
      F[i]=(f[i]-f[i-1])/x_diff;
      h[i-1]=x_diff;
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

float sampling(float *f,int fL,float fIndex)
{
  int idx=(int)fIndex;
  fIndex-=idx;
  
  if(idx==-1 && fIndex==1)return f[0];
  if(idx<0)return NAN;
  
  if(idx==fL-1 && fIndex==0)return f[idx];
  if(idx>=fL-1)return NAN;
  float V1=f[idx];
  float V2=f[idx+1];
  return V1+(V2-V1)*fIndex;
}

int calc_pdf_mean_sigma(float *f,float fStart,int fL,float *ret_mean,float *ret_sigma)
{
    float sum=0;
    float EX=0, var=0;
    float EX2=0;
    for (float i = fStart; i < fL+fStart; i++) {
      float saV=sampling(f,fL+fStart+1,i);
      sum += saV;
      EX += i*saV;
      EX2+= i*i*saV;
    }
    EX/=sum;
    EX2/=sum;



    if(ret_mean)*ret_mean=EX;
    if(ret_sigma)*ret_sigma=sqrt(EX2-EX*EX);

    return 0;
    
}

void simple_edge(float *f,int fL,float *edgeX,float *ret_edge_response)
{
  if(ret_edge_response)*ret_edge_response=10;
  float center=0;
  if(edgeX)*edgeX=center;

  float maxEdge=0;
  float maxEdgeIdx=-1;
  
  float edgeBoostS=1;
  float edgeBoostE=1;
  for(int i=1;i<fL-1;i++)
  {
    float boost=(f[i]/255.0);
    //boost=pow(boost,2);
    boost= edgeBoostS+boost*(edgeBoostE-edgeBoostS);
    float edge = (f[i+1]-f[i-1])*boost;//Positive edge only
    if(maxEdge<edge)
    {
      maxEdge=edge;
      maxEdgeIdx=i;
    }
  }

  if(edgeX)*edgeX=maxEdgeIdx;
  if(ret_edge_response)*ret_edge_response=maxEdge;
}

void smooth_edge(float *f,int fL,float *edgeX,float *ret_edge_response)
{
  if(ret_edge_response)*ret_edge_response=10;
  float center=0;
  if(edgeX)*edgeX=center;

  float maxEdge=0;
  float maxEdgeIdx=-1;
  
  float edgeBoostS=1;
  float edgeBoostE=1;
  float weightSum=0;
  float weightedEdgeSum=0;
  for(int i=1;i<fL-1;i++)
  {
    float boost=(f[i]/255.0);
    //boost=pow(boost,2);
    boost= edgeBoostS+boost*(edgeBoostE-edgeBoostS);
    float edge = (f[i+1]-f[i-1])*boost;//Positive edge only

    weightedEdgeSum=edge*i;
    weightSum+=edge;


    if(maxEdge<edge)
    {
      maxEdge=edge;
      maxEdgeIdx=i;
    }
  }
  maxEdgeIdx=weightedEdgeSum/=weightSum;
  maxEdge = weightedEdgeSum/(fL-2);
  if(edgeX)*edgeX=maxEdgeIdx;
  if(ret_edge_response)*ret_edge_response=maxEdge;
}

int EdgePointOpt_(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,float jump,acv_XY *ret_point_opt,float *ret_edge_response,FeatureManager_BacPac *bacpac=NULL)
{
  if(ret_point_opt==NULL)return -1;
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=(2*4)+1;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);
  gradVec.X*=jump;
  gradVec.Y*=jump;
  
  const int nM=3;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  const float sideJump=1.5;
  nvec.X*=sideJump;
  nvec.Y*=sideJump;
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
    float lightComp=1;
    if(bacpac && bacpac->sampler)
    {
      
      acv_XY CenterCurPt=curpoint;
      CenterCurPt.X+=nvec.X*nvec.X*((nM-1)/2);
      CenterCurPt.Y+=nvec.Y*nvec.Y*((nM-1)/2);//set to center position
      lightComp=bacpac->sampler->sampleBackLightFactor_ImgCoord(CenterCurPt);
    }
    // bacpac->sampler->



    gradTable[i] = lightComp*ptn/nM;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  float edgeX;
  simple_edge(gradTable,GradTableL,&edgeX,ret_edge_response);
  if(edgeX!=edgeX)//NAN
  {
    return -1;
  }

  gradVec = acvVecMult(gradVec,edgeX);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);


  return 0;
}


int EdgePointOpt(acvImage *graylevelImg,acv_XY gradVec,acv_XY point,float jump,acv_XY *ret_point_opt,float *ret_edge_response,FeatureManager_BacPac *bacpac=NULL)
{
  if(ret_point_opt==NULL)return -1;
  *ret_point_opt = point;
  *ret_edge_response = 1;
  const int GradTableL=(2*4)+1;
  float gradTable[GradTableL]={0};

  //curpoint = point -(GradTableL-1)*gVec/2
  gradVec = acvVecNormalize(gradVec);
  gradVec.X*=jump;
  gradVec.Y*=jump;
  
  const int nM=3;
  acv_XY nvec = {X:gradVec.Y,Y:-gradVec.X};
  const float sideJump=1.5;
  nvec.X*=sideJump;
  nvec.Y*=sideJump;
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
    float lightComp=1;
    if(bacpac && bacpac->sampler)
    {
      
      acv_XY CenterCurPt=curpoint;
      CenterCurPt.X+=nvec.X*nvec.X*((nM-1)/2);
      CenterCurPt.Y+=nvec.Y*nvec.Y*((nM-1)/2);//set to center position
      lightComp=bacpac->sampler->sampleBackLightFactor_ImgCoord(CenterCurPt);
    }
    // bacpac->sampler->



    gradTable[i] = lightComp*ptn/nM;

    curpoint = acvVecAdd(curpoint,gradVec);
  }

  float edgeX;
  simple_edge(gradTable,GradTableL,&edgeX,ret_edge_response);
  if(edgeX!=edgeX)//NAN
  {
    return -1;
  }

  gradVec = acvVecMult(gradVec,edgeX);
  *ret_point_opt = acvVecAdd(bkpoint,gradVec);


  return 0;
}




acv_XY pointSobel(acvImage *graylevelImg,acv_XY point,int range)
{
  int X=point.X;
  int Y=point.Y;
  int offset=range;
  if(X<offset || X+offset>= graylevelImg->GetWidth() ||
  Y<offset || Y+offset>= graylevelImg->GetHeight())
  {
    return (acv_XY){0,0};
  }
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

float findMaxIdx_spline(float *f,int fL,float *ret_max)
{

    int n,i,j,k;
    const int MaxL = 40;
    if(fL>MaxL)return NAN;
    float h[MaxL],s[MaxL]={0};
    n = fL;

    spline_max40(f,NULL,fL,s,h);

    float max=__FLT_MIN__;
    float max_idx=NAN;
    for(int i=0;i<fL-1;i++)
    {
      float a=(s[i+1]-s[i])/(6*h[i]);
      float b=s[i]/2;
      float c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
      float d=f[i];
      

      // printf("\n[%d]\n",i);
      int len=40;
      for(int j=0;j<len;j++)
      {
        float x=(float)j/(len-1);
        float fex=x*(x*(x*a+b)+c)+d;
        // printf("%.3f\n",fex);
      }

      //f  = ax^3+2bx^2 + cx+d
      //f  = x*(x*(x*a+b)+x)
      //f' = 3ax^2+2bx + c

      //find f'=0

      float jud=4*b*b-14*a*c;
      //check 4bb-14ac
      if(jud<0) //no zero crossing in real domain
      {
        continue;
      }

      float sqjud=sqrt(jud);
      float s2 = (-2*b-sqjud)/(6*a);
      float s1 = (-2*b+sqjud)/(6*a);
      float epsilon=0.2;
      {

        if(s1>=-epsilon && s1<1+epsilon)
        {
          float x=s1;
          float fex=x*(x*(x*a+b)+c)+d;
          if(max<fex)
          {
            max=fex;
            max_idx=x+i;
          }
        }
      }


      
      {
        if(s2>=-epsilon && s2<1+epsilon)
        {
          float x=s2;
          float fex=x*(x*(x*a+b)+c)+d;
          if(max<fex)
          {
            max=fex;
            max_idx=x+i;
          }
        }
      }
      
      // printf("\nmax:%f,idx:%f, sx:[%f,%f]\n",max,max_idx,s1,s2);
    }
    if(ret_max)*ret_max=max;
    return max_idx;
    //ax^3+bx^2+cx+d=y;

}

float findGradMaxIdx_spline(float *f,int fL,float *ret_max)
{

    int n,i,j,k;
    const int MaxL = 40;
    if(fL>MaxL)return NAN;
    float h[MaxL],s[MaxL]={0};
    n = fL;

    spline_max40(f,NULL,fL,s,h);

    float max=__FLT_MIN__;
    float max_idx=NAN;
    for(int i=0;i<fL-1;i++)
    {
      float a=(s[i+1]-s[i])/(6*h[i]);
      float b=s[i]/2;
      float c=(f[i+1]-f[i])/h[i]-(2*h[i]*s[i]+s[i+1]*h[i])/6;
      float d=f[i];
      

      // printf("\n[%d]\n",i);
      int len=40;
      for(int j=0;j<len;j++)
      {
        float x=(float)j/(len-1);
        float fex=x*(x*(x*a+b)+c)+d;
        float dfex=x*(x*3*a+2*b)+c;
        // printf("%.3f ,d:%.3f\n",fex,dfex);
      }

      //f  = ax^3+2bx^2 + cx+d
      //f  = x*(x*(x*a+b)+x)
      //f' = 3ax^2+2bx + c
      //f'' = 6ax+2b
      float zeroPoint=-b/(3*a);
      
      float epsilon=0.01;
      {

        if(zeroPoint>=-epsilon && zeroPoint<1+epsilon)
        {
          float x=zeroPoint;
          float fex=(x*(x*3*a+2*b)+c);
          if(max<fex)
          {
            max=fex;
            max_idx=x+i;
          }
        }
      }
      
      // printf("\nmax:%f,idx: %f, zeroPoint:%f\n",max,max_idx,zeroPoint);
    }
    if(ret_max)*ret_max=max;
    return max_idx;
    //ax^3+bx^2+cx+d=y;

}


edgeTracking::edgeTracking (acvImage *graylevelImg,acv_XY imgOffset,FeatureManager_BacPac *bacpac)
{
  this->graylevelImg=graylevelImg;
  this->bacpac=bacpac;
  this->imgOffset=imgOffset;
}


void edgeTracking::initTracking (ContourFetch::contourMatchSec &section,int new_regionSideWidth)
{ 
  return;
  if(section.section.size()==0)return;
  // for(int i=0;i<section.section.size();i++ )//offset Test
  // {
  //   acv_XY sobel = acvVecNormalize(section.section[i].sobel);
  //   sobel=acvVecMult(sobel,0);
  //   section.section[i].pt_img=acvVecAdd(sobel,section.section[i].pt_img);
  //   // section.section[i].pt_img.X-=2;
  // }


  if(new_regionSideWidth>regionSideWidth)
  {
    new_regionSideWidth=regionSideWidth;
  }
  
  // LOGI("sizeof(pixRegion):%d",sizeof(pixRegion));
  memset((void*)pixRegion[0],0,sizeof(pixRegion));
  fbIndex=0;
  gradIndex=0;
  for(int i=-new_regionSideWidth;i<new_regionSideWidth;i++)//pre setup
  {//idx:1 2
  
    // LOGI("i:%d",i);
    int secidx = valueSaturation(i,section.section.size());
    int widx = valueWarping(i,regionWidth);
    contourPixExtraction(graylevelImg, section.section[secidx].pt_img,
      section.section[secidx].sobel,gradIndex,stepDist,pixWidth,pixRegion[widx],bacpac);

  }

  
  // LOGI("graylevelImg:%p",graylevelImg);
  //[0' 0' 0'     0  1  2  3..... N-2  N-1      N-1' N-1' N-1'] 

  pixSumReset=true;
  PixSumReCalc(-new_regionSideWidth,new_regionSideWidth-1);
  fbIndex=-1;
  runTracking (section,new_regionSideWidth);
}


void edgeTracking::runTracking (ContourFetch::contourMatchSec &section,int new_regionSideWidth)
{

  
  // for(int i=0;i<section.section.size();i++ )//offset Test
  // {
  //   acv_XY sobel = acvVecNormalize(section.section[i].sobel);
  //   sobel=acvVecMult(sobel,0);
  //   section.section[i].pt_img=acvVecAdd(sobel,section.section[i].pt_img);
  //   // section.section[i].pt_img.X-=2;
  // }
  // acvImage FFKFKJDK;
  // FFKFKJDK.ReSize(graylevelImg);
  // acvCloneImage(graylevelImg,&FFKFKJDK,-1);
  do
  {
    // LOGI("RUN  fbIndex:%d",fbIndex);
    
    goAdv (section,true,new_regionSideWidth);
    // LOGI("RUN  fbIndex:%d",fbIndex);
    float mean_offset;
    float sigma;
    calc_info(&mean_offset,&sigma);
    
    if(sigma==sigma && sigma!=0)
      section.section[fbIndex].edgeRsp=1/sigma;
    else
      section.section[fbIndex].edgeRsp=0;




    float aoffset = bacpac->sampler->sampleAngleOffset(0);//should calc angle.... but ...
    acv_XY sobelV = acvVecNormalize(section.section[fbIndex].sobel);
    // LOGI("RUN:idx:%d sobel:%f,%f  pt:%f,%f",fbIndex,
      // section.section[fbIndex].sobel.X,section.section[fbIndex].sobel.Y,
      // section.section[fbIndex].pt_img.X,section.section[fbIndex].pt_img.Y);
    // LOGI("RUN:fbIndex:%d/%d  mean_offset:%f  aoffset:%f sobel:%f,%f sigma:%f",fbIndex,section.section.size(),mean_offset,aoffset,sobelV.X,sobelV.Y,sigma);
    acv_XY dirX=acvVecMult(sobelV,mean_offset+aoffset);
    
    acv_XY prePt = section.section[fbIndex].pt_img;
    // DRAW(&FFKFKJDK,prePt,0,255,0);
    // acv_XY tmp= prePt;
    // tmp.Y=(tmp.Y-691)*5+1000;
    // DRAW(&FFKFKJDK,tmp,0,255,255);
    section.section[fbIndex].pt=acvVecAdd(dirX,section.section[fbIndex].pt_img);
    
    // section.section[fbIndex].pt_img=section.section[fbIndex].pt;
    acv_XY curPt = section.section[fbIndex].pt_img;
    
    // DRAW(&FFKFKJDK,section.section[fbIndex].pt_img,255,0,0);
    // tmp= curPt;
    // tmp.Y=(tmp.Y-691)*5+1000;
    // DRAW(&FFKFKJDK,tmp,255,0,255);
    // LOGI("XY:%f,%f >> %f,%f ",prePt.X,prePt.Y,curPt.X,curPt.Y);
    bacpac->sampler->img2ideal(&section.section[fbIndex].pt);//pt in ideal coord
    
  }while(fbIndex<section.section.size()-1);

  
  // acvSaveBitmapFile("FFKFKJDK.BMP", &FFKFKJDK);
  //exit(-1);
}


void edgeTracking::PixSumReCalc(int start,int end)
{
  if(!pixSumReset)return;
  
  for(int j=0;j<pixWidth;j++)//pre setup
  {//idx:1 2
    pixSum[j]=0;
  }
  // printf("\nPixSumReCalc:");
  int widx=valueWarping(start,regionWidth);
  int wend=valueWarping(end,regionWidth);
  while(true){
    
    for(int j=0;j<pixWidth;j++)//pre setup
    {//idx:1 2
      pixSum[j]+=pixRegion[widx][j];
      
      // printf("%.4f ",pixRegion[widx][j]);
    }
    // printf("\n");
    if(widx==wend)break;
    widx=valueWarping(widx+1,regionWidth);
  }

  
  // printf("\n");
  // for(int j=0;j<pixWidth;j++)
  // {
  //   printf("%.4f ",pixSum[j]);
  // }
  // printf("\n");
  pixSumReset=false;
}


int edgeTracking::contourPixExtraction(acvImage *graylevelImg, acv_XY center_point,acv_XY sobel,int stepJump,float stepDist,int steps,float *pixels,FeatureManager_BacPac *bacpac)
{
  sobel = acvVecNormalize(sobel);
  
  acv_XY  stepDir= acvVecMult(sobel,stepDist);

  float stepsBack=-((float)(steps-1)/2);
  // printf("stepDist:%f steps:%d stepsBack:%f stepJump:%d  \n",stepDist,steps,stepsBack,stepJump);
  acv_XY  curpoint= acvVecMult(stepDir,stepsBack+stepJump);
  curpoint = acvVecAdd(center_point,curpoint);
  // printf("center_point:%f %f\n",center_point.X,center_point.Y);
  
  // LOGI("=====center_point:%f %f\n",center_point.X,center_point.Y);
  for(int i=0;i<steps;i++)
  {
    float lightComp=1;
    if(bacpac && bacpac->sampler)
    {
      lightComp=bacpac->sampler->sampleBackLightFactor_ImgCoord(curpoint);
    }
    float ptn= acvUnsignedMap1Sampling(graylevelImg, curpoint, 0)*lightComp;
    
    // bacpac->sampler->
    
    pixels[valueWarping(i+stepJump,steps)] = ptn;

    // if(i==(steps-1)/2)
    //   printf("||");
    // printf("%.1f %.1f:%.1f>",curpoint.X,curpoint.Y,ptn);

    curpoint = acvVecAdd(curpoint,stepDir);
  }
  // printf("\n");
  return 0;
} 



float edgeTracking::pixFetch(acvImage *graylevelImg, acv_XY pt,FeatureManager_BacPac *bacpac)
{
  acv_XY  curpoint= pt;
  
  float lightComp=1;
  if(bacpac && bacpac->sampler)
  {
    lightComp=bacpac->sampler->sampleBackLightFactor_ImgCoord(curpoint);
  }
  
  float ptn= acvUnsignedMap1Sampling(graylevelImg, pt, 0);
  // bacpac->sampler->
  return lightComp*ptn;

} 



void edgeTracking::calc_info(float *mean_offset, float *sigma)
{


  for(int j=1;j<pixWidth-1;j++)
  {
    int pSHIdx=valueWarping(j+gradIndex+1,pixWidth);
    int pSTIdx=valueWarping(j+gradIndex-1,pixWidth);
    grad[j]=pixSum[pSHIdx]-pixSum[pSTIdx];
    if(grad[j]<0)grad[j]=0;
  }

  grad[pixWidth-1]=grad[pixWidth-2];
  grad[0]=grad[1];

  // int xxdd=(pixWidth-1)/2;
  // for(int j=0;j<pixWidth;j++)
  // {
  //   if(j==xxdd)
  //   {
  //     printf("||");
  //   }
  //   printf("%.2f, ",grad[j]/3);

  // }
  // printf("\n");

  float _mean,_sigma;
  float idealMeanCenter=pixSideWidth;//(pixWidth-1)/2;
  calc_pdf_mean_sigma(grad,0,pixWidth,&_mean,&_sigma);
  float _mean_bk=_mean;
  for(int j=0;j<2;j++)
  {
    
    if(_mean<pixWidth/4||_mean>=pixWidth*3/4)
    {
      _mean=_mean_bk;
      _sigma=999;
      break;
    }

    float _mean_pre=_mean;
    calc_pdf_mean_sigma(grad,_mean-pixWidth/4,pixWidth/2,&_mean,&_sigma);
    // LOGI("mean[%d]: %f",j,_mean);
    if(abs(_mean_pre-_mean)<0.1)break;
  }


  // {
  //   float coefficients[3];
  //   int polyRet = polyfit(NULL,errorArr,NULL,polyFitLen,2,coefficients);
  //   float ret_r0,ret_r1;
  //   int rootRes= quadratic_roots(coefficients[2],coefficients[1],coefficients[0],&ret_r0,&ret_r1);
  // }

  _mean-=idealMeanCenter+gradIndex;
  // LOGI("idealMeanCenter:%f _mean:%f sigma:%f, pixSideWidth:%d",idealMeanCenter,_mean,_sigma,pixSideWidth);
  if(mean_offset)*mean_offset=_mean;
  if(sigma)*sigma=_sigma;

}


void edgeTracking::goSideShift (ContourFetch::contourMatchSec &section,bool goGradDir)
{

  if(goGradDir)
    gradIndex++;
  else
    gradIndex--;

  int gradFetchOffset=gradIndex+goGradDir?pixSideWidth:-pixSideWidth;
  int newRowIdx=valueWarping(gradFetchOffset,pixWidth);

  int regSum=0;

  for(int i=-regionSideWidth;i<=regionSideWidth;i++)//pre setup
  {//idx:1 2

  
    int secidx = valueSaturation(i+fbIndex,section.section.size());
    int widx = valueWarping(i+fbIndex,regionWidth);

    acv_XY pt = acvVecMult(section.section[secidx].sobel,gradFetchOffset);
    pt=acvVecAdd(pt,section.section[secidx].pt_img);
    float tmp = pixFetch(graylevelImg, pt,bacpac);
    pixRegion[widx][newRowIdx]=tmp;
    regSum+=tmp;
  }

  pixSum[newRowIdx]=regSum;
}

void edgeTracking::goAdv (ContourFetch::contourMatchSec &section,bool goForward,int new_regionSideWidth)
{
  if(goForward)
    fbIndex++;
  else
    fbIndex--;

  
  // LOGI("graylevelImg:%p",graylevelImg);
  int sideWidth_w_Dir=goForward?new_regionSideWidth:-new_regionSideWidth;
  
  int head_idx = valueWarping(fbIndex+sideWidth_w_Dir,regionWidth);
  int tail_idx = valueWarping(fbIndex-sideWidth_w_Dir-1,regionWidth);
  int sec_tail_idx = valueSaturation(fbIndex+sideWidth_w_Dir,section.section.size());

  // LOGI("RUN,  sec_tail_idx:%d  head_idx:%d tail_idx:%d",sec_tail_idx,head_idx,tail_idx);
  for(int j=0;j<pixWidth;j++)
  {
    pixSum[j]-=pixRegion[tail_idx][j];
  }
  // LOGI("sobel[%d]:%f,%f  << %f,%f",
  // sec_tail_idx,
  // section.section[sec_tail_idx].sobel.X,section.section[sec_tail_idx].sobel.Y,
  // section.section[sec_tail_idx].pt_img.X,section.section[sec_tail_idx].pt_img.Y);
  contourPixExtraction(graylevelImg, section.section[sec_tail_idx].pt_img,
    section.section[sec_tail_idx].sobel,gradIndex,stepDist,pixWidth,pixRegion[head_idx],bacpac);

  // for(int i=0;i<regionWidth;i++)
  // {
  //   for(int j=0;j<pixWidth;j++)
  //   {
  //     printf("%.1f, ",pixRegion[i][j]);
  //   }
    
  //   if(i==head_idx)
  //     printf("  [H:%d]\n",head_idx);
  //   else if(i==tail_idx)
  //     printf("  [X:%d]\n",tail_idx);
  //   else
  //     printf("\n");
  // }
  
  
  // LOGI("graylevelImg:%p",graylevelImg);
  // LOGI("RUN");
  for(int j=0;j<pixWidth;j++)
  {
    pixSum[j]+=pixRegion[head_idx][j];
  }

  // printf("pixSum:");
  // for(int j=0;j<pixWidth;j++)
  // {
  //   printf("%.1f, ",pixSum[j]);
  // }
  // printf("\n");
  // LOGI("graylevelImg:%p",graylevelImg);
}

void contourGridGrayLevelRefine(acvImage *grayLevelImg,ContourFetch &edge_grid,FeatureManager_BacPac *bacpac)
{
  for(int i=0;i<edge_grid.contourSections.size();i++)
  {
    vector<ContourFetch::ptInfo> &pts= edge_grid.contourSections[i];
    for(int j=0;j<pts.size();j++)
    {
      pts[j].sobel = pointSobel(grayLevelImg,pts[j].pt_img,2);

     // LOGI("[%d]>>s:%f,%f i:%f,%f ",j,pts[j].sobel.X,pts[j].sobel.Y,pts[j].contourDir.X,pts[j].contourDir.Y);
      // pts[j].pt_img=pts[j].pt;//pt in image coord
      if(bacpac)bacpac->sampler->img2ideal(&pts[j].pt);//pt in ideal coord
      pts[j].edgeRsp = 1;
    }
    ContourFilter(grayLevelImg,pts);
    
    for(int j=0;j<pts.size();j++)
    {
      if(pts[j].sobel.X==0 || pts[j].sobel.Y==0)//if somehow sobel is zero use contourDir as sobel
        pts[j].sobel = (acv_XY){pts[j].contourDir.Y,-pts[j].contourDir.X};
      // LOGI("[%d]>>s:%f,%f i:%f,%f ",j,pts[j].sobel.X,pts[j].sobel.Y,pts[j].contourDir.X,pts[j].contourDir.Y);
    }
  }
}


void extractLabeledContourDataToContourGrid(acvImage *labeledImg,int label,acv_LabeledData ldat,ContourFetch &edge_grid,int scanline_skip)
{

  edge_grid.RESET();

  if(scanline_skip<0)return;

  int sX = (int)ldat.LTBound.X;
  int sY = (int)ldat.LTBound.Y;
  int eX = (int)ldat.RBBound.X;
  int eY = (int)ldat.RBBound.Y;
  LOGI("%d %d %d %d",sX,sY,eX,eY);
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







float CrossProduct_SS(acv_XY p1,acv_XY p2,acv_XY p3)
{
  acv_XY v1=acvVecNormalize({.X=p2.X-p1.X,.Y=p2.Y-p1.Y});
  acv_XY v2=acvVecNormalize({.X=p3.X-p2.X,.Y=p3.Y-p2.Y});

  return acv2DCrossProduct(v1,v2);
}


void ComputeConvexHull2(const acv_XY *polygon,const int L)
{
	// The polygon needs to have at least three points
	if (L < 3)
	{
		return ;
	}

	std::vector<int> upperIdx;
	std::vector<int> lowerIdx;
	upperIdx.push_back(0);
	upperIdx.push_back(1);

	/*
	We piecewise construct the convex hull and combine them at the end of the method. Note that this could be
	optimized by combing the while loops.
	*/
	for (size_t i = 2; i < L; i++)
	{
		while (upperIdx.size() > 1 && 
    !CrossProduct_SS(polygon[upperIdx[upperIdx.size() - 2]], polygon[upperIdx[upperIdx.size() - 1]], polygon[i])>0)
		{
			upperIdx.pop_back();
		}
		upperIdx.push_back(i);

		while (lowerIdx.size() > 1 && 
    !CrossProduct_SS(polygon[lowerIdx[lowerIdx.size() - 2]], polygon[lowerIdx[lowerIdx.size() - 1]], polygon[L - i - 1])>0)
		{
			lowerIdx.pop_back();
		}
		lowerIdx.push_back(L - i - 1);
	}
	// upperIdx.insert(upperIdx.end(), lowerIdx.begin(), lowerIdx.end());
  for(int idx:upperIdx)
  {
    LOGI("idx:%d",idx);
  }
	return;
}