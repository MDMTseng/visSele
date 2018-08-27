
#include "include_priv/MatchingCore.h"
#include <time.h>

#include "logctrl.h"
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

float acvPoint3Angle(acv_XY p1,acv_XY pc,acv_XY p2)
{
  acv_XY v1={.X=p1.X-pc.X,.Y=p1.Y-pc.Y};
  acv_XY v2={.X=pc.X-p2.X,.Y=pc.Y-p2.Y};
  return acvVectorAngle(v1,v2);
}

void ContourFilter(vector<acv_XY> &contour,vector<acv_XY> &innerCornorContour,vector<acv_XY> &lineContour)
{
    const int L = contour.size();
    if(L==0)return;
    float crossP_LF_sum=0;

    const int Dist=10;
    const int LP_hWindow=Dist*2;

    float epsilon=0.05;

    float crossPHist[LP_hWindow*2];
    int crossPHist_head=0;

    for(int i=-LP_hWindow ;i<LP_hWindow;i++)
    {

      float angle=acvPoint3Angle(
        contour[valueWarping(i-Dist,L)],
        contour[valueWarping(i     ,L)],
        contour[valueWarping(i+Dist,L)]);

      crossPHist[i+LP_hWindow] =angle;
      crossP_LF_sum+=angle;
    }
    crossPHist_head=0;

    for(int i=0;i<L;i++)
    {

      //Filter out Non-inward contour
      //Cross product

      float angle=acvPoint3Angle(
        contour[valueWarping(i+LP_hWindow-Dist,L)],
        contour[valueWarping(i+LP_hWindow,L)],
        contour[valueWarping(i+LP_hWindow+Dist,L)]);
      crossP_LF_sum=crossP_LF_sum+angle;

      float crossP_LF=crossP_LF_sum/(LP_hWindow*2+1);
      crossP_LF_sum-=crossPHist[crossPHist_head];
      crossPHist[crossPHist_head] = angle;
      crossPHist_head = valueWarping(crossPHist_head+1,LP_hWindow*2);

      //If the cross product is more than -epsilon(the epsilon is margin to filter out straight line)
      //if the low filtered cross product is more than 0 (history shows it's most likely an outward contour)

      if(crossP_LF<-7*epsilon){
        continue;
      }//Inner curve
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

bool CircleFitTest(ContourGrid &contourGrid,
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

int CircleFitTest(ContourGrid &contourGrid,
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

float SecRegionCircleFit(ContourGrid &contourGrid, int secX,int secY,int secW,int secH,
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


float ContourDataCircleFit(ContourGrid &contourGrid, acv_XY *innerCornorContour, int conL,
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

acv_XY* findEndPoint(acv_Line line, int signedness, vector<acv_XY> &points)
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
    float dist = signedness*acvDistance_Signed(line, points[i]);
    if(max_dist<dist)
    {
      max_dist=dist;
      maxi=i;
    }
  }
  return (maxi>=0)?&points[maxi]:NULL;
}

bool LineFitTest(ContourGrid &contourGrid,
    acv_Line line,acv_LineFit *ret_lf,float epsilon1,float epsilon2,
    float minInBoundPoints)
{
  static vector<int> s_intersectIdxs;
  static vector<acv_XY> s_points;

  contourGrid.getContourPointsWithInLineContour(line,1000000,epsilon1,s_intersectIdxs,s_points);
  if(s_points.size()>minInBoundPoints)
  {
    acvFitLine(&s_points[0], s_points.size(),&line,NULL);

    contourGrid.getContourPointsWithInLineContour(line,1000000,epsilon2,s_intersectIdxs,s_points);
    if(s_points.size()>minInBoundPoints)
    {
      float sigma;
      acvFitLine(&s_points[0], s_points.size(),&line,&sigma);

      acv_XY *end_pos=findEndPoint(line, 1, s_points);
      acv_XY *end_neg=findEndPoint(line, -1, s_points);

      acv_LineFit lf;
      lf.line=line;
      lf.matching_pts=s_points.size();
      lf.s=sigma;
      if(end_pos)lf.end_pos=*end_pos;
      if(end_neg)lf.end_neg=*end_neg;
      *ret_lf = lf;
      return true;
    }
  }
  return false;
}

float SecRegionLineFit(ContourGrid &contourGrid, int secX,int secY,int secW,int secH,
  int dataSizeMinThre,float simThres, float sampleRate, float matching_margin, vector<acv_LineFit> &detectedLines)
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
    bool is_a_line;
    is_a_line=acvFitLine(pts  , 2,&line1,NULL);
    if(!is_a_line)continue;
    is_a_line=acvFitLine(pts+2, 2,&line2,NULL);
    if(!is_a_line)continue;
    float epsilon =3;
    if(LineSimilarity(line1,line2,epsilon) < simThres)continue;

    is_a_line=acvFitLine(pts, 4,&line1,NULL);
    if(!is_a_line)continue;
    float similarity=0;
    int sim_idx=findTheMostSimilarLineIdx(line1,detectedLines,epsilon,&similarity);
    if(similarity>simThres)
    {
      continue;
    }
    //printf("%f\n",similarity);
    acv_LineFit lf;
    if(LineFitTest(contourGrid,line1,&lf,matching_margin*2,matching_margin,40) == true)
    {
      findTheMostSimilarLineIdx(lf.line,detectedLines,epsilon, &similarity);
      if(similarity<0.90)
        detectedLines.push_back(lf);
    }

  }
  return maxMatchingScore;
}

void extractContourDataToContourGrid(acvImage *labeledImg,int grid_size,ContourGrid &inward_curve_grid, ContourGrid &straight_line_grid, int scanline_skip)
{

  inward_curve_grid.RESET(grid_size,labeledImg->GetWidth(),labeledImg->GetHeight());
  straight_line_grid.RESET(grid_size,labeledImg->GetWidth(),labeledImg->GetHeight());

  static vector<acv_XY> extractedContour;
  static vector<acv_XY> innerCornorContour;
  static vector<acv_XY> lineContour;


  extractedContour.resize(0);
  innerCornorContour.resize(0);
  lineContour.resize(0);

  if(scanline_skip<0)return;

  BYTE *OutLine, *OriLine;
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

          extractedContour.resize(0);
          acvContourExtraction(labeledImg, j, i, 1, 128, 1, searchType_C_W2B,extractedContour);
          ContourFilter(extractedContour,innerCornorContour,lineContour);
        }
        else if(pre_pix==0 && cur_pix == 255)//black to white
        {
          extractedContour.resize(0);
          acvContourExtraction(labeledImg, j-1, i, 1, 128, 1, searchType_C_B2W,extractedContour);
          ContourFilter(extractedContour,innerCornorContour,lineContour);
        }

        pre_pix= cur_pix;
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


}

void MatchingCore_CircleLineExtraction(acvImage *img,acvImage *buff,std::vector<acv_LabeledData> &ldData,
  std::vector<acv_CircleFit> &detectedCircles,std::vector<acv_LineFit> &detectedLines)
{

    clock_t t = clock();
    int grid_size = 50;
    static ContourGrid inward_curve_grid(grid_size,img->GetWidth(),img->GetHeight());
    static ContourGrid straight_line_grid(grid_size,img->GetWidth(),img->GetHeight());

    acvCloneImage( img,buff, -1);

    int scanline_skip=1;
    extractContourDataToContourGrid(buff,grid_size,inward_curve_grid, straight_line_grid,scanline_skip);
//inward_curve_grid  straight_line_grid
    int gridG_W = 3;
    int gridG_H = 3;

    detectedCircles.resize(0);
    detectedLines.resize(0);

    for(int i=0;i<inward_curve_grid.getRowSize()-gridG_H;i++)
    {
      for(int j=0;j<inward_curve_grid.getColumSize()-gridG_W;j++)
      {
        //inward_curve_grid.setSecROI(j,i,gridG_W,gridG_H);
        //straight_line_grid.setSecROI(j,i,gridG_W,gridG_H);
        float matching_margin=2;
        SecRegionCircleFit(inward_curve_grid, j,i,gridG_W,gridG_H,40,0.2,0.01,detectedCircles);
        SecRegionLineFit(straight_line_grid, j,i,gridG_W,gridG_H,40,0.9,0.05,matching_margin,detectedLines);
      }
    }


    refineMatchedCircle(detectedCircles,0.8,1.2);

    refineMatchedLine(detectedLines,0.9,0.8);
    t = clock() - t;
    logv("%fms \n", ((double)t) / CLOCKS_PER_SEC * 1000);

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

}
