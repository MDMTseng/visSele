#ifndef MLNN_HPP
#define MLNN_HPP
using namespace std;
#include<vector>

#include <cstdlib>
#include <ctime>


class MLNNUtil{
public :
  void randWMat(vector<vector<float> > &WMat);
  void matMul(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B);
  void matMul(vector<vector<float> > &C,const vector<vector<float> > &A, float B);
  void matAdd(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B,float B_coeff) ;
  void deltaW_accumulate(vector<vector<float> > &deltaW,vector<vector<float> > &in, vector<vector<float> > &error_gradient);
  void backgradient(vector<vector<float> > &backg,vector<vector<float> > &W, vector<vector<float> > &error_gradient) ;
  void actvationF(vector<vector<float> > &out,const vector<vector<float> > &in);
  void gradient_actvationF(vector<vector<float> > &out,const vector<vector<float> > &in);
};

class MLNL
{
private:
        vector<vector<float> > InArr;
        vector<vector<float> > pred_preY;
        vector<vector<float> > pred_Y;
        vector<vector<float> > error_gradient;
        vector<vector<float> > dW,W;
        MLNNUtil *nu;
public :
        MLNL();
        MLNL(MLNNUtil* nu,int batchSize,int inDim,int ouDim);
        MLNL(MLNNUtil* nu,MLNL &preLayer,int layerDim);
        ~MLNL();
        void init(MLNNUtil* nu,int batchSize,int inDim,int ouDim);
        void init(MLNNUtil* nu,MLNL &preLayer,int layerDim);
        void ForwardPass(const vector<vector<float> > &in);
        void reset_deltaW();
        void updateW(float learningRate);
        void backProp(const vector<vector<float> > &error_gradient);
        void backProp(
          vector<vector<float> > &back_gradient,
          const vector<vector<float> > &error_gradient);
        void WDecay(float rate);
        vector<vector<float> >& get_Pred_Y(){return pred_Y;};
        vector<vector<float> >& get_Error_gradient(){return error_gradient;};
};




class MLNN{
  vector<MLNL> layers;
  MLNNUtil nu;
  //NeuralUtil nu=new NeuralUtil_Tanh();
  vector<vector<float> > pred_Y;
public:
  MLNN(int batchSize,int netDim[],int dimL);

  void ForwardPass(const vector<vector<float> > &in);

  void backProp(vector<vector<float> > &back_gradient,const vector<vector<float> > &error_gradient);

  void reset_deltaW();

  void updateW(float learningRate);

  void WDecay(float rate);

};










#endif
