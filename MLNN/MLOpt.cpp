#include "MLNN.hpp"


MLOpt::MLOpt(MLNL &layer)
{
  this->layer=&layer;

  nu.Init2DVec(speed,layer.dW.size(),layer.dW[0].size());

}
#include<stdio.h>
void MLOpt::update_dW()
{
  for(int i=0;i<speed.size();i++)
  {
    for(int j=0;j<speed[0].size();j++)
    {
      float tmp=layer->dW[i][j];
      if(speed[i][j]*tmp<0)speed[i][j]*=0.0;
      speed[i][j]+=tmp;
      //printf("%f,",speed[i][j]);
      layer->dW[i][j]=(speed[i][j]);
      speed[i][j]*=0.9;
    }
  }
  //printf("\n");
}
