#include "MLNN.hpp"

using namespace std;
template<typename DataType>
DataType** New2D_(int d1,int d2)
{
  DataType* data=new DataType[d1*d2];
  DataType** arr=new DataType*[d1];
  for(int i=0;i<d1;i++)
  {
    arr[i]=data[i*d2];
  }
  return arr;
}

template<typename DataType>
void Init2DVec(vector<vector<DataType> > &vec,int d1,int d2)
{
  vec.clear();
  vec.resize(d1);
  for(int i=0;i<d1;i++)
  {
    vec[i].clear();
    vec[i].resize(d2);
  }

}

MLNN::MLNN(NeuralUtil* nu,int batchSize,int inDim,int ouDim)
{
  Init2DVec(InArr,batchSize,inDim+1);
  Init2DVec(pred_preY,batchSize,ouDim);
  Init2DVec(pred_Y,batchSize,ouDim);
  Init2DVec(error_gradient,batchSize,ouDim);
  Init2DVec(W,inDim,ouDim);
  Init2DVec(dW,inDim,ouDim);
  nu->randWMat(W);
  nu->randWMat(dW);
  for(int i=0;i<InArr.size();i++)
  {
    InArr[i][InArr[i].size()-1]=1;
  }
  this->nu=nu;
}

MLNN::~MLNN()
{
}

void MLNN::ForwardPass(const vector<vector<float> > &in)
{
  for(int i=0;i<in.size();i++)
    for(int j=0;j<in[i].size();j++)
  {
    InArr[i][j]=in[i][j];
  }
  nu->matMul(pred_preY,InArr,W);

  nu->actvationF(pred_Y,pred_preY);
}

void MLNN::reset_deltaW()
{
  nu->matZero(dW);
}
void MLNN::updateW(float learningRate)
{
  nu->matAdd(W,W,dW,learningRate/pred_Y.size());
}
void MLNN::backProp(vector<vector<float> > error_gradient)
{
  backProp(NULL,error_gradient);
}
void MLNN::backProp(
  vector<vector<float> > *back_gradient,
  vector<vector<float> > error_gradient)
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
  if(back_gradient!=NULL)
    nu->backgradient(*back_gradient,W,this->error_gradient);
}


void MLNN::WDecay(float rate)
{
  nu->matMul(W,W,rate);
}
