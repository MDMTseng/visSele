#include <stdio.h>
#include <unistd.h>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"

#include "cJSON.h"

#include "Data_Layer_Protocol.hpp"
#include "Data_Layer_PHY.hpp"


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


class MData_uInsp:public Data_JsonRaw_Layer
{
  
  public:
  MData_uInsp():Data_JsonRaw_Layer()// throw(std::runtime_error)
  {
  }
  int recv_jsonRaw_data(uint8_t *raw,int rawL,uint8_t opcode){
    
    if(opcode==1 )
    {
      cJSON *json=cJSON_Parse((char*)raw);
      if(json)
      {
        char *jstr = cJSON_Print(json);
        printf("JSON:\n%s\n====len:%d\n",jstr,rawL);

        // for(int i=0;i<rawL;i++)
        // {
        //   printf("%X ",raw[i]);
        // }
        // printf("\n");

        delete jstr;
      }
      else
      {
        printf("STR:\n%s\n====len:%d\n",raw,rawL);
      }
    }
    printf(">>opcode:%d\n",opcode);
    return 0;


  }

  void connected(Data_Layer_IF* ch){
    
    printf(">>>%X connected\n",ch);
  }

  void disconnected(Data_Layer_IF* ch){
    printf(">>>%X disconnected\n",ch);
  }

  ~MData_uInsp()
  {
    close();
    printf("MData_uInsp DISTRUCT:%p\n",this);
  }

  // int send_data(int head_room,uint8_t *data,int len,int leg_room){
    
  //   // printf("==============\n");
  //   // for(int i=0;i<len;i++)
  //   // {
  //   //   printf("%d ",data[i]);
  //   // }
  //   // printf("\n");
  //   return recv_data(data,len, false);//LOOP back
  // }
};




int main(int argc, char **argv)
{
  {
    //  Data_TCP_Layer *PHYLayer=new Data_TCP_Layer("127.0.0.1",1234);
    Data_UART_Layer *PHYLayer=new Data_UART_Layer("/dev/cu.SLAB_USBtoUART",921600, "8N1");

    MData_uInsp *mift=new MData_uInsp();
    mift->setDLayer(PHYLayer);
    
    mift->askJsonRawSupport();

    sleep(1);
  
    if(0){
      char buffer[200];  
      int headerSize=30;
      char *str=buffer+headerSize;
      int str_len=sprintf(str,"{\"type\":\"protocol_JsonRaw\",\"id\":%d}",234);
      mift->send_string(headerSize,(uint8_t*)str,str_len,sizeof(buffer)-headerSize-str_len);
    }
    
    sleep(3);
  
    delete mift;
  }

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
  while(1)
  {
    sleep(1000);
  }
  

  // return demomain(argc,argv);
  // return 0;
  // return testPolyFit();
  // tmpMain();
  // printf(">>>");
  return 0;//cp_main(argc, argv);
}

