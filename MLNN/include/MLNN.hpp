#ifndef MLNN_HPP
#define MLNN_HPP
using namespace std;
#include<vector>

#include <cstdlib>
#include <ctime>

class NeuralUtil{
  public :
  void randWMat(vector<vector<float> > &WMat){
    srand (static_cast <unsigned> (time(0)));
    for (int i = 0; i < WMat.size(); i++) { // aRow
        for (int j = 0; j < WMat[1].size(); j++) { // bColumn
          WMat[i][j]=-1+2*static_cast <float> (rand()) / static_cast <float> (RAND_MAX);//random(-1,1);
        }
      }
  }

  //AB  EF
  //CD  GH
  //AE+BG , AF+BH
  //CE+DG , CF+DH
  void matMul(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B) {
      int aRows = A.size();
      int aColumns = A[0].size();
      int bRows = B.size();
      int bColumns = B[0].size();

      for (int i = 0; i < C.size(); i++) {
          for (int j = 0; j < C[0].size(); j++) {
              C[i][j] = 0.0f;
          }
      }
      for (int i = 0; i < aRows; i++) { // aRow
          for (int j = 0; j < bColumns; j++) { // bColumn
              for (int k = 0; k < aColumns; k++) { // aColumn
                  C[i][j] += A[i][k] * B[k][j];
              }
          }
      }

  }

  void matMul(vector<vector<float> > &C,const vector<vector<float> > &A, float B) {

      for (int i = 0; i < C.size(); i++) {
          for (int j = 0; j < C[0].size(); j++) {
              C[i][j] = A[i][j]*B;
          }
      }
    }

  void matAdd(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B,float B_coeff) {
      for (int i = 0; i < A.size(); i++) { // aRow
          for (int j = 0; j < A[0].size(); j++) { // bColumn
            C[i][j]=A[i][j]+B_coeff*B[i][j];
          }
      }
  }

  void matZero(vector<vector<float> > &mat) {
      for (int i = 0; i < mat.size(); i++) { // aRow
          for (int j = 0; j < mat[0].size(); j++) { // bColumn
            mat[i][j]=0;
          }
      }
  }
  void deltaW_accumulate(vector<vector<float> > &deltaW,vector<vector<float> > &in, vector<vector<float> > &error_gradient) {

    for(int k=0;k<in.size();k++)//Iterate each data points
      for (int i = 0; i < deltaW.size(); i++) { // aRow
          for (int j = 0; j < deltaW[0].size(); j++) { // bColumn
              deltaW[i][j]+=error_gradient[k][j] * in[k][i];
          }
      }
  }
  void backgradient(vector<vector<float> > &backg,vector<vector<float> > &W, vector<vector<float> > &error_gradient) {

    for (int i = 0; i < backg.size(); i++) {
        for (int j = 0; j < backg[0].size(); j++) {
            backg[i][j]=0;
        }
    }
    for (int k = 0; k < backg.size(); k++) {
        for (int i = 0; i < backg[0].size(); i++) {
          for(int j=0;j<W[0].size();j++)
            backg[k][i]+=error_gradient[k][j] * W[i][j];

        }
    }
  }

  void actvationF(vector<vector<float> > &out,const vector<vector<float> > &in)
  {
      for (int i = 0; i < out.size(); i++) { // aRow
          for (int j = 0; j < out[0].size(); j++) { // bColumn
            out[i][j]=in[i][j];
          }
      }//out = in
  }

  void gradient_actvationF(vector<vector<float> > &out,const vector<vector<float> > &in){
      for (int i = 0; i < out.size(); i++) { // aRow
          for (int j = 0; j < out[0].size(); j++) { // bColumn
            out[i][j]=1;
          }
      }//gradient = 1
  }

  /*void printMat(float [][]C)
  {
      for (int i = 0; i < C.size(); i++) { // aRow
          for (int j = 0; j < C[0].size(); j++) { // bColumn
            print(C[i][j]+",");
          }
          println();
      }
  }*/
};

class MLNN
{

private:
        vector<vector<float> > InArr;
        vector<vector<float> > pred_preY;
        vector<vector<float> > pred_Y;
        vector<vector<float> > error_gradient;
        vector<vector<float> > dW,W;
        NeuralUtil *nu;
public :
        MLNN(NeuralUtil* nu,int batchSize,int inDim,int ouDim);
        ~MLNN();
        void ForwardPass(const vector<vector<float> > &in);
        void reset_deltaW();
        void updateW(float learningRate);
        void backProp(vector<vector<float> > error_gradient);
        void backProp(
          vector<vector<float> > *back_gradient,
          vector<vector<float> > error_gradient);
        void WDecay(float rate);
};


#endif
