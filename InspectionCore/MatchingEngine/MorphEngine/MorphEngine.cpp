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
      acv_XY c={.X=(float)(grid_size*j),.Y=(float)(grid_size*i)};
      morphNodes[sectionCol*i+j]=c;
    }
  }
  reset_optimization();
}

int MorphEngine::getSecIdx(acv_XY from)
{
  if(from.X<0||from.Y<0)return -2;
  int gridX=floor(from.X/gridSize);
  int gridY=floor(from.Y/gridSize);
  if(gridX>=sectionCol||gridY>=sectionRow)return -1;
  return sectionCol*gridY+gridX;
}

int MorphEngine::grid_adjust(int X,int Y, acv_XY vec)
{
  if(X<0 || X>=sectionCol)return -1;
  if(Y<0 || Y>=sectionRow)return -1;
  acv_XY *c = &(morphNodes[sectionCol*Y+X]);
  c->X+=vec.X;
  c->Y+=vec.Y;
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
  scal_pt.X/=gridSize;
  scal_pt.Y/=gridSize;

  int gridX=(int)scal_pt.X;
  int gridY=(int)scal_pt.Y;


  acv_XY ratio_pt=scal_pt;
  ratio_pt.X-=gridX;
  ratio_pt.Y-=gridY;

  acv_XY adj_vec=vec;
  adj_vec.X*=(1-ratio_pt.X);
  adj_vec.Y*=(1-ratio_pt.Y);
  grid_adjust(gridX  , gridY  , adj_vec);
  adj_vec=vec;
  adj_vec.X*=(  ratio_pt.X);
  adj_vec.Y*=(1-ratio_pt.Y);
  grid_adjust(gridX+1, gridY  , adj_vec);
  adj_vec=vec;
  adj_vec.X*=(1-ratio_pt.X);
  adj_vec.Y*=(  ratio_pt.Y);
  grid_adjust(gridX  , gridY+1, adj_vec);
  adj_vec=vec;
  adj_vec.X*=(  ratio_pt.X);
  adj_vec.Y*=(  ratio_pt.Y);
  grid_adjust(gridX+1, gridY+1, adj_vec);
}

int MorphEngine::Mapping_adjust(acv_XY pt, acv_XY vec, float *distGainTbl,const int TblL)
{
  acv_XY scal_pt=pt;
  scal_pt.X/=gridSize;
  scal_pt.Y/=gridSize;



  int gridX1=(int)ceil(scal_pt.X-TblL);
  int gridY1=(int)ceil(scal_pt.Y-TblL);

  int gridX2=(int)floor(scal_pt.X+TblL);
  int gridY2=(int)floor(scal_pt.Y+TblL);

  for(int i=gridY1;i<=gridY2;i++)
  {
    for(int j=gridX1;j<=gridX2;j++)
    {
      float dist=hypot(j-scal_pt.X,i-scal_pt.Y);
      int idx = round(dist);
      if(idx>=TblL)continue;
      float weight=distGainTbl[idx];
      acv_XY wvec={.X=vec.X*weight,.Y=vec.Y*weight};
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
      bC->X=(R->X+L->X+T->X+B->X)/4*(1-alpha)+C->X*alpha;
      bC->Y=(R->Y+L->Y+T->Y+B->Y)/4*(1-alpha)+C->Y*alpha;
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
      nodeOpt->v.X+=(node->X-nodeOpt->buf_node.X)*alpha;
      nodeOpt->v.Y+=(node->Y-nodeOpt->buf_node.Y)*alpha;
      nodeOpt->v.X*=0.8;
      nodeOpt->v.Y*=0.8;
      node->X=nodeOpt->buf_node.X+nodeOpt->v.X;
      node->X=nodeOpt->buf_node.X+nodeOpt->v.X;
      nodeOpt->buf_node=*node;
    }
  }
  return 0;
}

int MorphEngine::Mapping(acv_XY from,acv_XY *ret_to)
{
  if(ret_to==NULL)return -1;

  int gridX=floor(from.X/gridSize);
  int gridY=floor(from.Y/gridSize);

  float ratioX=(from.X-gridX*gridSize)/gridSize;
  float ratioY=(from.Y-gridY*gridSize)/gridSize;

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
  m1_1.X= (m12.X-m11.X)*ratioX+m11.X;
  m1_1.Y= (m12.Y-m11.Y)*ratioX+m11.Y;

  //          |ratioX
  //m21 ----m2_1----------- m22
  acv_XY m2_1;
  m2_1.X= (m22.X-m21.X)*ratioX+m21.X;
  m2_1.Y= (m22.Y-m21.Y)*ratioX+m21.Y;

  //m1_1
  //  |
  // mp -ratioY(0~1)
  //  |
  //  |
  //  |
  //m2_1
  acv_XY mp;
  mp.X= (m2_1.X-m1_1.X)*ratioY+m1_1.X;
  mp.Y= (m2_1.Y-m1_1.Y)*ratioY+m1_1.Y;
  *ret_to=mp;

  return 0;
}
