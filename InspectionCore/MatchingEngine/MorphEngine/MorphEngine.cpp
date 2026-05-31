#include "MorphEngine.h"


void MorphEngine::RESET(int grid_size,int img_width,int img_height)
{
  if(grid_size==-1)
    grid_size = gridSize;
  gridSize = grid_size;
  sectionCol=(img_width/grid_size)+1;//Always one more colum&Row to make interpolation boundery case easier
  sectionRow=(img_height/grid_size)+1;
  morphNodes.resize(sectionCol*sectionRow);
  morphNodes_optimizer.resize(sectionCol*sectionRow);
  for(int i=0;i<sectionRow;i++)
  {
    for(int j=0;j<sectionCol;j++)
    {
      acv_XY c((float)(grid_size*j), (float)(grid_size*i));
      morphNodes[sectionCol*i+j]=c;
    }
  }
  reset_optimization();
}

int MorphEngine::getSecIdx(acv_XY from)
{
  if(from.x<0||from.y<0)return -2;
  int gridX=floor(from.x/gridSize);
  int gridY=floor(from.y/gridSize);
  if(gridX>=sectionCol||gridY>=sectionRow)return -1;
  return sectionCol*gridY+gridX;
}

int MorphEngine::grid_adjust(int X,int Y, acv_XY vec)
{
  if(X<0 || X>=sectionCol)return -1;
  if(Y<0 || Y>=sectionRow)return -1;
  acv_XY *c = &(morphNodes[sectionCol*Y+X]);
  c->x+=vec.x;
  c->y+=vec.y;
  return 0;
}

int MorphEngine::Mapping_adjust_Global(acv_XY offset)
{
  for(int i=0;i<sectionRow;i++)
  {
    for(int j=0;j<sectionCol;j++)
    {
      grid_adjust(j,i,offset);
    }
  }
}

int MorphEngine::Mapping_adjust(acv_XY pt, acv_XY vec)
{
  acv_XY scal_pt=pt;
  scal_pt.x/=gridSize;
  scal_pt.y/=gridSize;

  int gridX=(int)scal_pt.x;
  int gridY=(int)scal_pt.y;


  acv_XY ratio_pt=scal_pt;
  ratio_pt.x-=gridX;
  ratio_pt.y-=gridY;

  acv_XY adj_vec=vec;
  adj_vec.x*=(1-ratio_pt.x);
  adj_vec.y*=(1-ratio_pt.y);
  grid_adjust(gridX  , gridY  , adj_vec);
  adj_vec=vec;
  adj_vec.x*=(  ratio_pt.x);
  adj_vec.y*=(1-ratio_pt.y);
  grid_adjust(gridX+1, gridY  , adj_vec);
  adj_vec=vec;
  adj_vec.x*=(1-ratio_pt.x);
  adj_vec.y*=(  ratio_pt.y);
  grid_adjust(gridX  , gridY+1, adj_vec);
  adj_vec=vec;
  adj_vec.x*=(  ratio_pt.x);
  adj_vec.y*=(  ratio_pt.y);
  grid_adjust(gridX+1, gridY+1, adj_vec);
}

int MorphEngine::Mapping_adjust(acv_XY pt, acv_XY vec, float *distGainTbl,const int TblL)
{
  acv_XY scal_pt=pt;
  scal_pt.x/=gridSize;
  scal_pt.y/=gridSize;



  int gridX1=(int)ceil(scal_pt.x-TblL);
  int gridY1=(int)ceil(scal_pt.y-TblL);

  int gridX2=(int)floor(scal_pt.x+TblL);
  int gridY2=(int)floor(scal_pt.y+TblL);

  for(int i=gridY1;i<=gridY2;i++)
  {
    for(int j=gridX1;j<=gridX2;j++)
    {
      float dist=hypot(j-scal_pt.x,i-scal_pt.y);
      int idx = round(dist);
      if(idx>=TblL)continue;
      float weight=distGainTbl[idx];
      acv_XY wvec = {vec.x*weight, vec.y*weight};
      grid_adjust(j, i, wvec);
    }
  }
}

int MorphEngine::regularization(float alpha)
{
  for(int i=1;i<sectionRow-1;i++)
  {
    for(int j=1;j<sectionCol-1;j++)
    {
      acv_XY *R = &(morphNodes[sectionCol*i+(j-1)]);
      acv_XY *L = &(morphNodes[sectionCol*i+(j+1)]);
      acv_XY *T = &(morphNodes[sectionCol*(i-1)+j]);
      acv_XY *B = &(morphNodes[sectionCol*(i+1)+j]);
      acv_XY *C = &(morphNodes[sectionCol*i+j]);
      acv_XY *bC = &(morphNodes_optimizer[sectionCol*i+j].buf_node);
      bC->x=(R->x+L->x+T->x+B->x)/4*(1-alpha)+C->x*alpha;
      bC->y=(R->y+L->y+T->y+B->y)/4*(1-alpha)+C->y*alpha;
    }
  }

  for(int i=1;i<sectionRow-1;i++)
  {
    for(int j=1;j<sectionCol-1;j++)
    {
      morphNodes[sectionCol*i+j] = morphNodes_optimizer[sectionCol*i+j].buf_node;
    }
  }
}

int MorphEngine::reset_optimization()
{
  acv_XY zero_vec={0};
  for(int i=1;i<sectionRow-1;i++)
  {
    for(int j=1;j<sectionCol-1;j++)
    {
      morphNodes_optimizer[sectionCol*i+j].buf_node=morphNodes[sectionCol*i+j];
      morphNodes_optimizer[sectionCol*i+j].v=zero_vec;
    }
  }
  return 0;
}
int MorphEngine::optimization(float alpha)
{
  for(int i=1;i<sectionRow-1;i++)
  {
    for(int j=1;j<sectionCol-1;j++)
    {
      morphOptimizer *nodeOpt = &morphNodes_optimizer[sectionCol*i+j];
      acv_XY *node = &morphNodes[sectionCol*i+j];
      nodeOpt->v.x+=(node->x-nodeOpt->buf_node.x)*alpha;
      nodeOpt->v.y+=(node->y-nodeOpt->buf_node.y)*alpha;
      nodeOpt->v.x*=0.8;
      nodeOpt->v.y*=0.8;
      node->x=nodeOpt->buf_node.x+nodeOpt->v.x;
      node->x=nodeOpt->buf_node.x+nodeOpt->v.x;
      nodeOpt->buf_node=*node;
    }
  }
  return 0;
}

int MorphEngine::Mapping(acv_XY from,acv_XY *ret_to)
{
  if(ret_to==NULL)return -1;

  int gridX=floor(from.x/gridSize);
  int gridY=floor(from.y/gridSize);

  float ratioX=(from.x-gridX*gridSize)/gridSize;
  float ratioY=(from.y-gridY*gridSize)/gridSize;

  //     |ratioX(0~1)
  //m11 -- m12
  // |     |    -ratioY(0~1)
  //m21 -- m22
  acv_XY m11 = morphNodes[gridY*sectionCol+gridX];
  acv_XY m12 = morphNodes[gridY*sectionCol+gridX+1];
  acv_XY m21 = morphNodes[(gridY+1)*sectionCol+gridX];
  acv_XY m22 = morphNodes[(gridY+1)*sectionCol+gridX+1];

  //          |ratioX
  //m11 ----m1_1----------- m12
  acv_XY m1_1;
  m1_1.x= (m12.x-m11.x)*ratioX+m11.x;
  m1_1.y= (m12.y-m11.y)*ratioX+m11.y;

  //          |ratioX
  //m21 ----m2_1----------- m22
  acv_XY m2_1;
  m2_1.x= (m22.x-m21.x)*ratioX+m21.x;
  m2_1.y= (m22.y-m21.y)*ratioX+m21.y;

  //m1_1
  //  |
  // mp -ratioY(0~1)
  //  |
  //  |
  //  |
  //m2_1
  acv_XY mp;
  mp.x= (m2_1.x-m1_1.x)*ratioY+m1_1.x;
  mp.y= (m2_1.y-m1_1.y)*ratioY+m1_1.y;
  *ret_to=mp;

  return 0;
}
