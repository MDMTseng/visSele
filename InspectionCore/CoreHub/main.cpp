#include <stdio.h>
#include <unistd.h>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"

#include "cJSON.h"

#include "Data_Layer_Protocol.hpp"
#include "Data_Layer_PHY.hpp"
#include "CameraLayerManager.hpp"

float randomGen(float from=0,float to=1)
{
  float r01=(rand()%1000000)/1000000.0;
  return r01*(to-from)+from;
}

int_fast32_t testPolyFit()
{
  srand(time(NULL));
  // These inputs should result in the following approximate coefficients:
  //         0.5           2.5           1.0        3.0
  //    y = (0.5 * x^3) + (2.5 * x^2) + (1.0 * x) + 3.0
  const int dataL=100;
  struct DATA_XY
  {
    float x;
    float y;
  }Data[dataL];


  float coeff[]={1.0001,2.5,-3,4,5};
  const int order = sizeof(coeff)/sizeof(coeff[0])-1;
  for(int i=0;i<dataL;i++)
  {
    Data[i].x=(i-dataL/2)*0.1;
    Data[i].y=polycalc(Data[i].x, coeff,order+1);//+randomGen(-1,1);
  }

  float resCoeff[order+1]={0}; // resulting array of coefs

  // Perform the polyfit
  int result = polyfit(&(Data[0].x),
                       &(Data[0].y),
                       NULL,
                       dataL,
                       order,
                       resCoeff,
                       sizeof(Data[0]),
                       sizeof(Data[0])
                       );




  printf("Original coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",coeff[order-i]);
  printf("\nNew coeff\n");
  for(int i=0;i<order+1;i++)printf(",%.5f",resCoeff[order-i]);
  printf("\n===========\n");

  
  for(int i=0;i<dataL;i++)
  {
    float pred_yData=polycalc(Data[i].x, resCoeff,order+1);

    LOGI("[%d]: x:%.5f   y:%.5f   _y:%.5f  diff:%.5f",i,Data[i].x,Data[i].y,pred_yData,pred_yData-Data[i].y);
  }

  return 0;
}

CameraLayer::status CameraLayer_Callback_CAM1(CameraLayer &cl_obj, int type, void *context)
{
  
  LOGI("type:%d\n",type);
  return CameraLayer::status::ACK;
}


CameraLayer::status CameraLayer_Callback_CAM2(CameraLayer &cl_obj, int type, void *context)
{
  
  LOGI("type:%d\n",type);
  return CameraLayer::status::ACK;
}


void camLyerTest()
{
  CameraLayerManager clm;
  clm.discover();
  printf("cam:\n%s\n",clm.genJsonStringList().c_str());
  CameraLayer *camLayer=clm.connectCamera(0,"",CameraLayer_Callback_CAM1,NULL);
  CameraLayer *camLayer2=clm.connectCamera(1,"data/BMP_carousel_test1",CameraLayer_Callback_CAM2,NULL);
  // CameraLayer *camLayer=clm.connectCamera(1,0,"data/BMP_carousel_test",CameraLayer_Callback_XXX,NULL);

  if(camLayer!=NULL)
  {
    printf("connected:\n%s\n",camLayer->getCameraJsonInfo().c_str());
  }
  else
  {
    printf("connect failed\n");
    return ;
  }
  // camLayer->TriggerMode(1);
  // printf("WAIT for image trigger~\n");
  // for(int i=0;i<2;i++)
  // {
  //   camLayer->Trigger();
  //   sleep(1);
  // }


  camLayer->TriggerMode(0);
  camLayer->SetFrameRate(NAN);
  

  camLayer2->TriggerMode(0);
  camLayer2->SetFrameRate(NAN);
  printf("WAIT for image streaming~\n");
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }
  
  camLayer->SetROI(0,0,100,100,100,100);
  printf("WAIT for image streaming~ setFPS:err:%d\n",camLayer->SetFrameRate(INFINITY));
  
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }


  camLayer->SetROI(0,0,100,100,100,100);
  printf("WAIT for image streaming~ setFPS:err:%d\n",camLayer->SetFrameRate(5));
  
  for(int i=0;i<3;i++)
  {
    sleep(1);
  }

  delete camLayer;
}

int main(int argc, char **argv)
{

  // while(1)
  // {
  //   sleep(1000);
  // }
  

  // char *sendMsg="{\"type\":\"PING\",\"id\":445}";
  // JRL.send_data(0,(uint8_t*)sendMsg,strlen(sendMsg),0);


  // std::thread _inspSnapSaveThread(testUART_thread);
  // _inspSnapSaveThread.join();
  // Data_UART_Layer UARTCH("/dev/cu.SLAB_USBtoUART",921600, "8N1");
  // MData_String_NL_Layer MUART_NL;
  // MData_String_Dev StrDev_NL;
  
  // MUART_NL.setDLayer(&UARTCH);
  // StrDev_NL.setDLayer(&MUART_NL);
  // char *tmpMsg="1";
  // StrDev_NL.send_data(0,(uint8_t*)tmpMsg,strlen(tmpMsg)+1,0);
  // char *tmpMsg2="1";
  // StrDev_NL.send_data(0,(uint8_t*)tmpMsg2,strlen(tmpMsg2)+1,0);

  // return demomain(argc,argv);
  // return 0;
  // return testPolyFit();
  // tmpMain();
  // printf(">>>");
  return cp_main(argc, argv);
}

