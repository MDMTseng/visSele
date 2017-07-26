#ifndef MLNN_HPP
#define MLNN_HPP
using namespace std;
#include<vector>

#include <cstdlib>
#include <ctime>


class MLNNUtil {
public :
    void initWMat(vector<vector<float> > &WMat);
    void randWMat(vector<vector<float> > &WMat);
    void identityWMat(vector<vector<float> > &WMat,float noise);
    float random(float m,float M);
    void matMul(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B);
    void matMul(vector<vector<float> > &C,const vector<vector<float> > &A, float B);
    void matAdd(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B,float B_coeff) ;
    void deltaW_accumulate(vector<vector<float> > &deltaW,vector<vector<float> > &in, vector<vector<float> > &error_gradient);
    void backgradient(vector<vector<float> > &backg,vector<vector<float> > &W, vector<vector<float> > &error_gradient) ;
    void actvationF(vector<vector<float> > &out,const vector<vector<float> > &in);
    void gradient_actvationF(vector<vector<float> > &out,const vector<vector<float> > &in);
    void printMat(vector<vector<float> > &C);
    template<typename DataType>
    void Init2DVec(vector<vector<DataType> > &vec,int d1,int d2)
    {
        vec.clear();
        vec.resize(d1);
        for(int i=0; i<d1; i++)
        {
            vec[i].clear();
            vec[i].resize(d2);
        }

    }
};

class MLNL
{
private:
    vector<vector<float> > pred_preY;
    MLNNUtil *nu;
public :
    vector<vector<float> > dW,W;
    MLNL();
    MLNL(MLNNUtil* nu,int batchSize,int inDim,int ouDim);
    MLNL(MLNNUtil* nu,MLNL &preLayer,int layerDim);
    ~MLNL();
    void init(MLNNUtil* nu,int batchSize,int inDim,int ouDim);
    void init(MLNNUtil* nu,MLNL &preLayer,int layerDim);
    void ForwardPass(const vector<vector<float> > &in);
    void ForwardPass();
    void reset_deltaW();
    void updateW(float learningRate);
    void backProp(const vector<vector<float> > &error_gradient);
    void backProp(
        vector<vector<float> > &back_gradient,
        const vector<vector<float> > &error_gradient);
    void WDecay(float rate);
    vector<vector<float> > InArr;
    vector<vector<float> > pred_Y;
    vector<vector<float> > error_gradient;
    void printW();
};




class MLNN {
    MLNNUtil nu;
    //NeuralUtil nu=new NeuralUtil_Tanh();
public:
    vector<MLNL> layers;
    vector<vector<float> > *p_pred_Y;
    MLNN(int batchSize,int netDim[],int dimL);
    void init(int batchSize,int netDim[],int dimL);
    MLNN();
    vector<vector<float> > &get_input_vec();
    void ForwardPass(const vector<vector<float> > &in);
    void ForwardPass();
    void backProp(vector<vector<float> > &back_gradient,const vector<vector<float> > &error_gradient);
    void backProp(const vector<vector<float> > &error_gradient);
    void reset_deltaW();

    void updateW(float learningRate);

    void WDecay(float rate);

};

class MLOpt {
    MLNNUtil nu;
    MLNL *layer;
    vector<vector<float> > speed;
public:
    MLOpt();
    MLOpt(MLNL &layer);
    void init (MLNL &layer);
    void update_dW();
};




#endif
