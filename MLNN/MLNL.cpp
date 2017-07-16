#include "MLNN.hpp"

using namespace std;

MLNL::MLNL()
{

}
MLNL::MLNL(MLNNUtil* nu,int batchSize,int inDim,int ouDim)
{
  init(nu,batchSize,inDim,ouDim);
}
MLNL::MLNL(MLNNUtil* nu,MLNL &preLayer,int layerDim)
{
  init(nu,preLayer,layerDim);
}
MLNL::~MLNL()
{
}
void MLNL::init(MLNNUtil* nu,MLNL &preLayer,int layerDim)
{
  init(nu,preLayer.InArr.size(),preLayer.pred_Y[0].size(),layerDim);
}
void MLNL::init(MLNNUtil* nu,int batchSize,int inDim,int ouDim)
{
  nu->Init2DVec(InArr,batchSize,inDim+1);
  nu->Init2DVec(pred_preY,batchSize,ouDim);
  nu->Init2DVec(pred_Y,batchSize,ouDim);
  nu->Init2DVec(error_gradient,batchSize,ouDim);
  nu->Init2DVec(W,inDim+1,ouDim);
  nu->Init2DVec(dW,inDim+1,ouDim);
  nu->initWMat(W);
  for(int i=0;i<InArr.size();i++)
  {
    InArr[i][InArr[i].size()-1]=50;
  }
  this->nu=nu;
}

void MLNL::ForwardPass(const vector<vector<float> > &in)
{
  for(int i=0;i<in.size();i++)
    for(int j=0;j<in[i].size();j++)
  {
    InArr[i][j]=in[i][j];
  }
  ForwardPass();
}
void MLNL::ForwardPass()
{
  nu->matMul(pred_preY,InArr,W);

  nu->actvationF(pred_Y,pred_preY);
}

void MLNL::reset_deltaW()
{
  nu->matMul(dW,dW,0);
}
void MLNL::updateW(float learningRate)
{
  nu->matAdd(W,W,dW,learningRate/pred_Y.size());
}
void MLNL::backProp(const vector<vector<float> > &error_gradient)
{
  //println("==="+error_gradient[0].size());
  //println(">>" +this->error_gradient[0].size());
  nu->gradient_actvationF(this->error_gradient,pred_preY);//get sigmoid gradient

  for(int i=0;i<error_gradient.size();i++)//Multiply error gradient with sigmoid gradient
    for(int j=0;j<error_gradient[0].size();j++)
  {
    this->error_gradient[i][j]*=error_gradient[i][j];
  }

  nu->deltaW_accumulate(dW,InArr,this->error_gradient);
}
void MLNL::backProp(
  vector<vector<float> > &back_gradient,
  const vector<vector<float> > &error_gradient)
{
    backProp(error_gradient);
    nu->backgradient(back_gradient,W,this->error_gradient);
}


void MLNL::WDecay(float rate)
{
  nu->matMul(W,W,rate);
}
void MLNL::printW()
{
  nu->printMat(W);
}
