#include "MLNN.hpp"

void MLNNUtil::randWMat(vector<vector<float> > &WMat){
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
void MLNNUtil::matMul(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B) {
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

void MLNNUtil::matMul(vector<vector<float> > &C,const vector<vector<float> > &A, float B) {

    for (int i = 0; i < C.size(); i++) {
        for (int j = 0; j < C[0].size(); j++) {
            C[i][j] = A[i][j]*B;
        }
    }
  }

void MLNNUtil::matAdd(vector<vector<float> > &C,const vector<vector<float> > &A,const vector<vector<float> > &B,float B_coeff) {
    for (int i = 0; i < A.size(); i++) { // aRow
        for (int j = 0; j < A[0].size(); j++) { // bColumn
          C[i][j]=A[i][j]+B_coeff*B[i][j];
        }
    }
}

void MLNNUtil::deltaW_accumulate(vector<vector<float> > &deltaW,vector<vector<float> > &in, vector<vector<float> > &error_gradient) {

  for(int k=0;k<in.size();k++)//Iterate each data points
    for (int i = 0; i < deltaW.size(); i++) { // aRow
        for (int j = 0; j < deltaW[0].size(); j++) { // bColumn
            deltaW[i][j]+=error_gradient[k][j] * in[k][i];
        }
    }
}
void MLNNUtil::backgradient(vector<vector<float> > &backg,vector<vector<float> > &W, vector<vector<float> > &error_gradient) {

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

void MLNNUtil::actvationF(vector<vector<float> > &out,const vector<vector<float> > &in)
{
    for (int i = 0; i < out.size(); i++) { // aRow
        for (int j = 0; j < out[0].size(); j++) { // bColumn
          out[i][j]=in[i][j];
        }
    }//out = in
}

void MLNNUtil::gradient_actvationF(vector<vector<float> > &out,const vector<vector<float> > &in){
    for (int i = 0; i < out.size(); i++) { // aRow
        for (int j = 0; j < out[0].size(); j++) { // bColumn
          out[i][j]=1;
        }
    }//gradient = 1
}
/*
  void MLNNUtil::printMat(float [][]C)
  {
      for (int i = 0; i < C.size(); i++) { // aRow
          for (int j = 0; j < C[0].size(); j++) { // bColumn
            print(C[i][j]+",");
          }
          println();
      }
  }*/
