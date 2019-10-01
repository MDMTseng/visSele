
#include "ContourGrid.h"
#include "logctrl.h"

ContourGrid::ContourGrid()
{

}
ContourGrid::ContourGrid(int grid_size,int img_width,int img_height)
{
  RESET(grid_size,img_width,img_height);
}

void ContourGrid::RESET(int grid_size,int img_width,int img_height)
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


void ContourGrid::setSecROI(int secROI_X,int secROI_Y,int secROI_W,int secROI_H)
{
  this->secROI_X=secROI_X;
  this->secROI_Y=secROI_Y;
  this->secROI_W=secROI_W;
  this->secROI_H=secROI_H;
}
void ContourGrid::resetSecROI()
{
  setSecROI(0,0,sectionCol,sectionRow);
}

int ContourGrid::getColumSize()
{
  return sectionCol;
}

int ContourGrid::getRowSize()
{
  return sectionRow;
}


int ContourGrid::getSecIdx(int X,int Y)
{
  int gridX=X/gridSize;
  int gridY=Y/gridSize;
  return sectionCol*gridY+gridX;
}

std::vector<ContourGrid::ptInfo> &ContourGrid::fetchBelongingSection(acv_XY pt)
{
  int gridIdx=getSecIdx(pt.X,pt.Y);
  return contourSections[gridIdx];
}

void ContourGrid::push(ptInfo data)
{
  fetchBelongingSection(data.pt).push_back(data);
  dataNumber++;
}

int ContourGrid::dataSize()
{
  return dataNumber;
}

const ContourGrid::ptInfo* ContourGrid::get(int idx)
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

void ContourGrid::GetSectionsWithinCircleContour(float X,float Y,float radius,float epsilon,
  std::vector<int> &intersectIdxs)
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

bool isAngleBetween(float angle,float sAngle,float eAngle)
{


  //LOGV("%f < %f < %f",sAngle*180/3.14159,angle*180/3.14159,eAngle*180/3.14159);
  angle-=sAngle;

  while(angle<0)
  {
    angle+=2*M_PI;
  }
  while(angle>2*M_PI)
  {
    angle-=2*M_PI;
  }


  eAngle-=sAngle;
  
  while(eAngle<0)
  {
    eAngle+=2*M_PI;
  }
  while(eAngle>2*M_PI)
  {
    eAngle-=2*M_PI;
  }


  //LOGV("angle: %f eAngle: %f",angle*180/3.14159,eAngle*180/3.14159);
  //LOGV(">>%f %f",angle,eAngle);
  return angle<eAngle;
}

//outter_inner => bigger than 0 is for outer circle, smaller than 0 is fo inner, 0 is for both
void ContourGrid::getContourPointsWithInCircleContour(float X,float Y,float radius,float sAngle,float eAngle,float outter_inner,
  float epsilon,
  std::vector<int> &intersectIdxs,std::vector<ptInfo> &points)
{
  points.resize(0);
  GetSectionsWithinCircleContour(X,Y,radius,epsilon,intersectIdxs);
  float outerDist_sq=radius+epsilon;
  outerDist_sq*=outerDist_sq;
  const float arcCurvatureMin = 0.02;
  const float arcCurvatureMax = 10  ;

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
      ptInfo pti = contourSections[idx][j];
      float dX = pti.pt.X-X;
      float dY = pti.pt.Y-Y;
      float dist_sq = dX*dX + dY*dY;
      pti.edgeRsp=1;
      if(dist_sq>innerDist_sq && dist_sq<outerDist_sq)//The point is in the epsilon region
      {
        float absCurv = abs(pti.curvature);
        //LOGV("P1");
        if(absCurv>arcCurvatureMin &&
          absCurv<arcCurvatureMax &&
         (pti.curvature*outter_inner)>=0)
        {
          float angle = atan2(dY,dX);
          // LOGV(">>%f,%f-> %f %f",pti.pt.X,pti.pt.Y,X,Y);
          // LOGV(">>%f,%f:  %f %f %f",dX,dY,angle, sAngle, eAngle);
          if(isAngleBetween( angle, sAngle, eAngle))
          {
            points.push_back(pti);
          }
        }
      }
    }
  }
}



void ContourGrid::GetSectionsWithinLineContour(acv_Line line,float epsilonX, float epsilonY, std::vector<int> &intersectIdxs)
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
  epsilonX/=gridSize;
  epsilonY/=gridSize;

  //printf("%d %d %d %d\n",RONI_X1,RONI_Y1,RONI_X2,RONI_Y2);
  //Skip the outer most rect
  for(int i=secROI_Y1;i<=secROI_Y2;i++)
  {
    for(int j=secROI_X1;j<=secROI_X2;j++)
    {
      int idx = i*gridNodeW+j;

      acv_XY pt={.X=j-line.line_anchor.X,.Y=i-line.line_anchor.Y};

      pt = acvRotation(-line.line_vec.Y,line.line_vec.X,1,pt);

      //By default, the current dot is in middle(0)
      intersectTestNodes[idx]=0;
      float abspt;

      abspt=pt.Y;
      if(abspt<0)abspt=-abspt;
      if(abspt > epsilonY)
      {//If Y distance is bigger than epsilonY
        //Set region type, in(1) or out(2)
        intersectTestNodes[idx]|=(pt.Y>0)?(0x01):(0x02);

        //negative (XX02)
        //--------
        //middle
        //--------
        //positive (XX01)
      }


      abspt=pt.X;
      if(abspt<0)abspt=-abspt;
      if(abspt > epsilonX)
      {//If X distance is bigger than epsilonX
        //Set region type, x_in(0x10) or x_out(0x20)
        intersectTestNodes[idx]|=(pt.X>0)?(0x10):(0x20);

        //negative (02XX) |middle| positive (01XX)
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

      int V1=intersectTestNodes[idx1];
      int V2=intersectTestNodes[idx2];
      int V3=intersectTestNodes[idx3];
      int V4=intersectTestNodes[idx4];

      //Check any changes or just in the region(hit the box)
      bool YDetection = (V1&0x0F)!=(V2&0x0F) ||(V3&0x0F)!=(V4&0x0F)||(V1&0x0F)!=(V3&0x0F)|| ((V1&0x0F)==0);
      bool XDetection = (V1&0xF0)!=(V2&0xF0) ||(V3&0xF0)!=(V4&0xF0)||(V1&0xF0)!=(V3&0xF0)|| ((V1&0xF0)==0);

      //Check X Y border type changes, if so, the current box is in the target region
      if( (YDetection && XDetection) )
      {
        //If any nodes are different from each other there must be the contour in between
        intersectIdxs.push_back(i*sectionCol + j);
        continue;
      }
    }
  }
}

void ContourGrid::getContourPointsWithInLineContour(acv_Line line, float epsilonX, float epsilonY,float flip_f, std::vector<int> &intersectIdxs,std::vector<ptInfo> &points)
{
  LOGV("test...");
  points.resize(0);
  line.line_vec=acvVecNormalize(line.line_vec);
  GetSectionsWithinLineContour(line,epsilonX,epsilonY,intersectIdxs);
  //exit(0);
  const float lineCurvatureMax = 0.15;
  int count=0;
  for(int i=0;i<intersectIdxs.size();i++)
  {
    int idx = intersectIdxs[i];
    for(int j=0;j<contourSections[idx].size();j++)
    {
  
      ptInfo pti = contourSections[idx][j];
      acv_XY pt=pti.pt;
      pt.X-=line.line_anchor.X;
      pt.Y-=line.line_anchor.Y;
      pti.edgeRsp=1;

      //reverse rotate the target point to check if the point is in the margin(rotated box)
      pt = acvRotation(-line.line_vec.Y,line.line_vec.X,1,pt);
      if(pt.X<0)pt.X=-pt.X;
      if(pt.Y<0)pt.Y=-pt.Y;
      if(pt.X < epsilonX && pt.Y < epsilonY)
      {
        float dotP = pti.contourDir.X * line.line_vec.X + pti.contourDir.Y * line.line_vec.Y;
        //LOGV("%f,%f   %f,%f  dotP:%f",pti.contourDir.X,pti.contourDir.Y,line.line_vec.X,line.line_vec.Y,dotP);
        if(dotP*flip_f>0.9 && abs(pti.curvature)<lineCurvatureMax)
        {
          //LOGV(">>>>>>>");
        
          points.push_back(pti);
        }
      }
    }

  }
}

int ContourGrid::getGetSectionRegionDataSize(int secX,int secY,int secW,int secH)
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
const acv_XY* ContourGrid::getGetSectionRegionData(int secX,int secY,int secW,int secH,int dataIdx)
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
        return &(contourSections[idx][dataIdx-count].pt);
      }
      count+=curSecSize;
    }
  }
  return NULL;
}
