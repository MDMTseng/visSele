#ifndef B_IMAGE_TEMPLATE_FITTING_HPP
#define B_IMAGE_TEMPLATE_FITTING_HPP

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "acvImage_ToolBox.hpp"
#include "acvImage_BasicDrawTool.hpp"
#include "acvImage_BasicTool.hpp"
#include <float.h>

#include "acvImage_MophologyTool.hpp"
#include "acvImage_SpDomainTool.hpp"
#include "MLNN.hpp"

class BinaryImageTemplateFitting
{
  MLNN NN;
  MLOpt MO;
  vector<vector<float> > error_gradient;
  vector<acv_XY> regionSampleXY;

  vector<acv_XY> mappedXY;
  vector<acv_XY> errorXY;

  acv_LabeledData tar_ldData;

  acvImage *tarImg;
  acvImage *srcImg;
  acvImage *tarDistGradient;


public:
  BinaryImageTemplateFitting(
                     acv_LabeledData &tar_ldData,
                     acvImage *tarImg,
                     acvImage *srcImg,
                     acvImage *target_DistGradient)
  {   //*******************************************
      int dim_in = 2;
      int dim_out = 2;
      int batchSize = 700;
      int NNDim[] = {dim_in, dim_out};
      MLNNUtil nu;

      nu.Init2DVec(error_gradient, batchSize, dim_out);

      NN.init(batchSize, NNDim, sizeof(NNDim) / sizeof(*NNDim));

      MO.init(NN.layers[0]);
      this->tar_ldData = tar_ldData;
      this->tarImg = tarImg;
      this->srcImg = srcImg;
      regionSampleXY.resize(batchSize);
      tarDistGradient = target_DistGradient;
      //****************************************
  }

  float find_subpixel_params(vector<acv_XY> &tracking_region,acv_LabeledData &src_ldData,
                            float AngleDiff, int iterCount)
  {
      static int idx_c = 0;
      float scale = 1;
      //init W params
      NN.layers[0].W[0][0] = cos(AngleDiff) * scale; //Rough angle from signature
      NN.layers[0].W[1][0] = sin(AngleDiff) * scale;
      NN.layers[0].W[1][1] = NN.layers[0].W[0][0];
      NN.layers[0].W[0][1] = -NN.layers[0].W[1][0];

      NN.layers[0].W[2][0] = (tar_ldData.Center.X - src_ldData.Center.X); //rough offset from lebeling
      NN.layers[0].W[2][1] = (tar_ldData.Center.Y - src_ldData.Center.Y);

      errorXY.resize(regionSampleXY.size());
      mappedXY.resize(regionSampleXY.size());
      float alpha = 12;
      float alphaDown = (alpha - 0.5) / iterCount;
      float minErr=FLT_MAX;
      float error;
      for (int j = 0; j < iterCount; j++) //Iteration
      {

          sampleXYFromRegion(regionSampleXY, tracking_region, regionSampleXY.size());

          DotsTransform(regionSampleXY, mappedXY, NN, src_ldData.Center, 1);

          //return;
          error = acvSpatialMatchingGradient(srcImg, &(regionSampleXY[0]),
                        tarImg, tarDistGradient, &(mappedXY[0]),
                        &(errorXY[0]), regionSampleXY.size());

          for (int k = 0; k < errorXY.size(); k += 1)
          {
              error_gradient[k][0] = -errorXY[k].X / (errorXY.size() * 256 * 128);
              error_gradient[k][1] = -errorXY[k].Y / (errorXY.size() * 256 * 128);
          }
          NN.backProp(error_gradient);
          MO.update_dW();
          NN.layers[0].dW[2][0] *= 2500; //Special treat
          NN.layers[0].dW[2][1] *= 2500;
          NN.updateW(alpha);
          alpha -= alphaDown;
          //nu.printMat(NN.layers[0].dW);printf("\n");
          NN.reset_deltaW();

          //Limit transform to be only rotate and translate
          float a00 = (NN.layers[0].W[0][0] + NN.layers[0].W[1][1]) / 2;
          float a10 = (NN.layers[0].W[1][0] - NN.layers[0].W[0][1]) / 2;
          float LL = hypot(a00, a10);
          a00 /= LL;
          a10 /= LL;
          NN.layers[0].W[0][0] = a00;
          NN.layers[0].W[0][1] = -a10;
          NN.layers[0].W[1][0] = a10;
          NN.layers[0].W[1][1] = a00;

          //if(minErr>error)minErr=error;
          if(minErr==FLT_MAX)
            minErr=error;
          else
            minErr+=0.5*(error-minErr);
          /*if(j%1!=0)continue;
          continue;*/

          /*printf(">er:%f, mer:%f, %f %f %f\n",error,minErr,
                 NN.layers[0].W[2][0], NN.layers[0].W[2][1],
                 180 / M_PI * atan2(NN.layers[0].W[1][0] - NN.layers[0].W[0][1], NN.layers[0].W[0][0] + NN.layers[0].W[1][1]));*/

          /*if(minErr>75)
          {
            printf("BAD!!!!!\n");
          }*/
          //if(true)continue;
          //sleep(1);

      }


      /*acvImage buff;
      buff.ReSize(tarImg->GetWidth(), tarImg->GetHeight());

      acvCloneImage(tarImg, &buff, 2);
      acvClear(&buff,0);*/
      error=0;
      for (int k = 0; k < tracking_region.size() / regionSampleXY.size(); k++)
      {
          sampleXYFromRegion_Seq(regionSampleXY, tracking_region,
                                 k * regionSampleXY.size(), regionSampleXY.size());
          DotsTransform(regionSampleXY, mappedXY, NN, src_ldData.Center, 1);
          for (int m = 0; m < mappedXY.size(); m++)
          {
                int t=(int)acvUnsignedMap1Sampling(tarImg, mappedXY[m], 0);
                int s=(int)acvUnsignedMap1Sampling(srcImg, regionSampleXY[m], 0);
                t-=s;
                /*if(t<0)
                {
                  t=-t;
                  buff.CVector[(int)round(mappedXY[m].Y)][(int)round(mappedXY[m].X) * 3 + 1] =t;
                }
                else
                {
                  buff.CVector[(int)round(mappedXY[m].Y)][(int)round(mappedXY[m].X) * 3 + 2] =t;
                }*/
                error+=t*t;
          }
      }

      //acvClear(&buff,128,1);
      /*
      acvContrast(&buff,&buff,0,1,1);
      acvContrast(&buff,&buff,0,1,2);
      char name[100];
      sprintf(name, "data/target_test_cover_%03d.bmp", idx_c++);
      acvSaveBitmapFile(name, &buff);*/
      return error;
  }

  void sampleXYFromRegion(vector<acv_XY> &sampleXY, const vector<acv_XY> &regionXY, int sampleCount)
  {
      sampleXY.clear();
      static int offset = rand() % (regionXY.size() / sampleCount);
      for (int i = 0; i < sampleCount; i++)
      {

          int randIdx = i * regionXY.size() / sampleCount + offset + rand() % 50;
          randIdx = randIdx % regionXY.size();
          sampleXY.push_back(regionXY[randIdx]);
      }
  }

  void sampleXYFromRegion_Seq(vector<acv_XY> &sampleXY, const vector<acv_XY> &regionXY, int from, int sampleCount)
  {
      sampleXY.clear();
      for (int i = 0; i < sampleCount; i++)
      {
          sampleXY.push_back(regionXY[from + i]);
      }
  }
  void DotsTransform(std::vector<acv_XY> &XY, std::vector<acv_XY> &tXY, MLNN &NN, acv_XY transOffset, float scale)
  {
      vector<vector<float> > &in_vec = NN.get_input_vec();
      tXY.resize(XY.size());
      for (int j = 0; j < in_vec.size(); j++)
      {
          in_vec[j][0] = (XY[j].X - transOffset.X) * scale;
          in_vec[j][1] = (XY[j].Y - transOffset.Y) * scale;
      }

      MLNNUtil nu;
      //  nu.printMat(NN.layers[0].W);
      NN.ForwardPass();
      for (int j = 0; j < NN.p_pred_Y->size(); j++)
      {
          tXY[j].X = (*NN.p_pred_Y)[j][0] / scale + transOffset.X;
          tXY[j].Y = (*NN.p_pred_Y)[j][1] / scale + transOffset.Y;
          //printf(">>%f %f %f\n",(*NN.p_pred_Y)[j][1],tXY[j].X,tXY[j].Y);
      }
  }

  //Displacement, Scale, Aangle
  void acvLabeledPixelExtraction(acvImage *LabelPic, acv_LabeledData *target_info, int target_idx, std::vector<acv_XY> *retData)
  {
      retData->clear();
      int i, j;
      BYTE *L;

      for (i = target_info->LTBound.Y; i < target_info->RBBound.Y + 1; i++)
      {
          L = &(LabelPic->CVector[i][(int)target_info->LTBound.X * 3]);
          for (j = target_info->LTBound.X; j < target_info->RBBound.X + 1; j++, L += 3)
          {
              _24BitUnion *lebel = (_24BitUnion *)L;

              if (lebel->_3Byte.Num == target_idx)
              {
                  acv_XY XY = {.X = j, .Y = i};
                  retData->push_back(XY);
              }
          }
      }
  }

};


#endif
