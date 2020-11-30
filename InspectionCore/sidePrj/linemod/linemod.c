#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "common_lib.h"
#include <math.h>


float floatArrSampling(float *arr,int arrL,float sampleX)
{
  if(sampleX<=0)return arr[0];
  if(sampleX>arrL-1)return arr[arrL-1];

  int sampX_int = sampleX;
  float ratio = sampleX-sampX_int;

  return arr[sampX_int]+ratio*(arr[sampX_int+1]-arr[sampX_int]);
}

float floatArrSampling_central(float *arr,int arrL,float sampleX)
{
  sampleX+=(float)(arrL-1)/2;
  return floatArrSampling( arr, arrL, sampleX);
}
struct singleLogistic
{//W4+W3*logistic(W1*X+W2)
  float X;
  float W1,W1X;
  float W2,W1XpW2;
  float Logi;
  float W3,W3xLogi;
  float W4,W4pW3xLogi;

  float sdW1,sdW2,sdW3,sdW4;
  float dW1,dW2,dW3,dW4;
  
  float powSum_dW1,powSum_dW2,powSum_dW3,powSum_dW4;
};


float singleLogisticForward(struct singleLogistic &sL,float X)
{
  sL.X = X;
  sL.W1X = sL.W1*X;
  sL.W1XpW2 = sL.W1X+sL.W2;
    //printf("W1XpW2:========  %f \n",sL.W1XpW2);
  sL.Logi = 1/(1+exp(-sL.W1XpW2));
  sL.W3xLogi=sL.W3*sL.Logi;
  sL.W4pW3xLogi=sL.W4+sL.W3xLogi;
  return sL.W4pW3xLogi;
}

void singleLogisticBackward(struct singleLogistic &sL,float err_grad)
{
   // printf("err_grad:-------  %f \n",err_grad);
  sL.sdW4=err_grad;
  sL.sdW3=err_grad*sL.Logi;
  float logi_output_grad = err_grad*sL.W3;

  float W3_x = sL.W3;
  float logi_grad = (sL.Logi*(1-sL.Logi))*(err_grad*W3_x);
  sL.sdW2=logi_grad;
  sL.sdW1=logi_grad*sL.X;




  sL.dW1+=sL.sdW1;
  sL.dW2+=sL.sdW2;
  sL.dW3+=sL.sdW3;
  sL.dW4+=sL.sdW4;
}


void singleLogisticParamUpdate(struct singleLogistic &sL,float alpha,int batch_size)
{
  sL.dW1/=batch_size;
  sL.dW2/=batch_size;
  sL.dW3/=batch_size;
  sL.dW4/=batch_size;
  sL.powSum_dW1+=sL.dW1*sL.dW1;
  sL.powSum_dW2+=sL.dW2*sL.dW2;
  sL.powSum_dW3+=sL.dW3*sL.dW3;
  sL.powSum_dW4+=sL.dW4*sL.dW4;
  float epsilon=1;
  sL.W1-=alpha*sL.dW1/(epsilon+sqrt(sL.powSum_dW1));
  sL.W2-=alpha*sL.dW2/(epsilon+sqrt(sL.powSum_dW2));
  sL.W3-=alpha*sL.dW3/(epsilon+sqrt(sL.powSum_dW3));
  sL.W4-=alpha*sL.dW4/(epsilon+sqrt(sL.powSum_dW4));

  sL.powSum_dW1*=0.9;
  sL.powSum_dW2*=0.9;
  sL.powSum_dW3*=0.9;
  sL.powSum_dW4*=0.9;
  
  sL.dW1=
  sL.dW2=
  sL.dW3=
  sL.dW4=0;
}

int ERF_Test()
{
  const int tableL=21;
  const float sigma=2.4;
  float erf[tableL];
  float gaussian[tableL];
  const float tableL_mid = (float)(tableL-1)/2;

  float sum=0;
  for(int i=0;i<tableL;i++)
  {
    float x = (i-tableL_mid)/sigma;
    float pt = exp(-(x*x));
    sum+=pt;
    gaussian[i] = pt;
  }

  erf[0]=gaussian[0]/sum;
  for(int i=1;i<tableL;i++)
  {
    gaussian[i] /= sum;
    erf[i]=erf[i-1]+gaussian[i];
  }

  for(int i=tableL-1;i!=0;i--)
  {
    erf[i]=(erf[i]+erf[i-1])/2;
  }
  erf[0]=1-erf[tableL-1];
  for(int i=0;i<tableL;i++)
  {
    printf("[%02d]:%0.4f:%0.4g\n",i,erf[i],gaussian[i]);
  }

  float offset = -8;

  float updateMomentum=0;
  for(int j=0;j<5;j++)
  {

    float diffSum=0;
    for(int i=0;i<tableL;i++)
    {
      float diff = 
        floatArrSampling_central(erf,tableL,(i-tableL/2)*1 ) - 
        floatArrSampling_central(erf,tableL,(i+0.8+offset)*0.2 );
      diffSum+=diff;
    }
    printf("s[%02d]:diffSum:%0.4f  offset:%0.4f\n",j,diffSum,offset);
    if(updateMomentum*diffSum<0)updateMomentum=0;
    updateMomentum+=diffSum*0.1;
    offset+=updateMomentum;
  }

  printf("===========Newton=============");


  struct singleLogistic sL={0};
  sL.W1=3;
  sL.W2=0;
  sL.W3=1;
  sL.W4=0;


  
  float targetFunc[tableL];
  
  {

    struct singleLogistic sL_target;
    sL_target.W1=   40;
    sL_target.W2=  -sL_target.W1*0.0;
    sL_target.W3=  1;
    sL_target.W4=   0;
    for(int i=0;i<tableL;i++)
    {

      float x = (((float)i/(tableL-1))-0.5)*2;
      float v = singleLogisticForward(sL_target,x);
      float randNoise = 0;//(((rand()%2000)-1000)/1000.0)*10/255;
      targetFunc[i]=v+randNoise;
      
      printf("noise[i]:%f\n",randNoise);

    }
  }

  float alpha=2.0;
  for(int j=0;j<10;j++)
  {
    
    float WSum=0;
    float diffSum=0;

    float diffMean=0;
    for(int i=0;i<tableL;i++)
    {
      float v = targetFunc[i];
      float x = (((float)i/(tableL-1))-0.5)*2;
      float V = singleLogisticForward(sL,x);
      float diff = V-v;

      singleLogisticBackward(sL,diff);
      //printf("v:%0.4f  V:%0.4f \n",v,V);
      diffSum+=diff*diff;
    }
    diffSum/=tableL;

    printf("s[%02d]:diffSum:%0.4f   %0.4f,%0.4f,%0.4f,%0.4f\n",j,diffSum,sL.W1,sL.W2,sL.W3,sL.W4);

    singleLogisticParamUpdate(sL,alpha,tableL);
    //alpha+=0.01*(2-alpha);
  }

  for(int i=0;i<tableL;i++)
  {
    float v = targetFunc[i];
    float V = singleLogisticForward(sL,(i-tableL/2.0)/(tableL/2.0));
    float diff = v-V;


    printf("diff:%0.4f t:%0.4f V:%0.4f\n",diff,v,V);

  }


  {
    printf("===========moment method=============\n");

    float M0=0;
    float M1=0;
    float M2=0;
    float x_nor_factor = (tableL)/2.0;
    for(int i=0;i<tableL;i++)
    {
      float x = (((float)i/(tableL-1))-0.5)*2;
      float v = targetFunc[i];
      
      M0+=    v;
      M1+=  x*v;
      M2+=x*x*v;
      
      printf("x:%0.4f v:%0.4f\n",x,v);
    }
    printf("M0:%f M1:%f M2:%f\n",M0,M1,M2);
    M0/=x_nor_factor;
    M1/=x_nor_factor;
    M2/=x_nor_factor;
    float l=(3*M2-M0)/(2*M1);//edge location
    float k=(2*M1)/(1-l*l);//edge contrast
    float h=(M0-k*(1-l))/2;//Background value
    printf("l:%f, k:%f h:%f\n",l,k,h);

  }

  return 0;
}

int main(int argc, char** argv)
{
  printf("sdsd");
  return 0;
}