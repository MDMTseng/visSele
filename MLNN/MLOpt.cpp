#include "MLNN.hpp"


void MLOpt::init(MLNL &layer)
{
  this->layer=&layer;

  nu.Init2DVec(speed,layer.dW.size(),layer.dW[0].size());

}
MLOpt::MLOpt(MLNL &layer)
{
  init(layer);
}

MLOpt::MLOpt()
{
}

#include<stdio.h>
void MLOpt::update_dW()
{
  for(int i=0;i<speed.size();i++)
  {
    for(int j=0;j<speed[0].size();j++)
    {
      float tmp=layer->dW[i][j];
      if((speed[i][j]>0)!=(tmp>0))speed[i][j]*=0.2;
      speed[i][j]+=tmp;
      //printf("%f,",speed[i][j]);
      layer->dW[i][j]=(speed[i][j]);
      speed[i][j]*=0.8;
    }
  }
  //printf("\n");
}
