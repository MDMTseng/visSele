#include "MLNN.hpp"

using namespace std;
MLNN::MLNN(int batchSize,int netDim[],int dimL)
{
  layers.resize(dimL-1);

  layers[0].init(&nu,batchSize,netDim[0],netDim[1]);

  for(int i=1;i<layers.size();i++)
  {
    layers[i].init(&nu,layers[i-1],netDim[i+1]);
  }
  //layers[layers.size()-1].nu=new NeuralUtil_Sigmoid();
}

vector<vector<float> >& MLNN::get_input_vec()
{
  return layers[0].InArr;
}

void MLNN::ForwardPass(const vector<vector<float> > &in)
{

  for(int i=0;i<in.size();i++)
    for(int j=0;j<in[i].size();j++)
  {
    layers[0].InArr[i][j]=in[i][j];
  }
  ForwardPass();
}
void MLNN::ForwardPass()
{
  layers[0].ForwardPass();

  for(int i=1;i<layers.size();i++)
  {
    layers[i].ForwardPass(layers[i-1].pred_Y);
  }
  p_pred_Y = &layers[layers.size()-1].pred_Y;
}

void MLNN::backProp(const vector<vector<float> > &error_gradient)
{
  if(layers.size()>1)
  {
    layers[layers.size()-1].backProp(layers[layers.size()-2].pred_Y,error_gradient);
    for(int i=layers.size()-2;i!=0;i--)
    {
        layers[i].backProp(layers[i-1].pred_Y,layers[i].pred_Y);
    }
    layers[0].backProp(layers[0].pred_Y);
  }
  else
    layers[0].backProp(error_gradient);
}

void MLNN::backProp(vector<vector<float> > &back_gradient,const vector<vector<float> > &error_gradient)
{
  if(layers.size()>1)
  {
    layers[layers.size()-1].backProp(layers[layers.size()-2].pred_Y,error_gradient);
    for(int i=layers.size()-2;i!=0;i--)
    {
        layers[i].backProp(layers[i-1].pred_Y,layers[i].pred_Y);
    }
    layers[0].backProp(back_gradient,layers[0].pred_Y);
  }
  else
    layers[0].backProp(back_gradient,error_gradient);
}

void MLNN::reset_deltaW()
{
  for(int i=0;i<layers.size();i++)
  {
    layers[i].reset_deltaW();

  }
}

void MLNN::updateW(float learningRate)
{
  for(int i=0;i<layers.size();i++)
  {
      layers[i].updateW(learningRate);
  }
}

void MLNN::WDecay(float rate)
{
  for(int i=0;i<layers.size();i++)
  {
      layers[i].WDecay(rate);
  }
}
