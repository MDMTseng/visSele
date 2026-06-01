#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <main.h>
#include "tmpCodes.hpp"
#include "polyfit.h"

#include "cJSON.h"

#include "Data_Layer_Protocol.hpp"
#include "Data_Layer_PHY.hpp"


#include <logctrl.h>
#include <log_crash.h>
LOG_MODULE("core.boot");

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




int main(int argc, char **argv)
{
  /* Phase F.1: opt-in shm-ring + drainer at boot.
   * Enable with INSP_LOG_DAEMON=1.  The drainer writes the rolling
   * /var/log/insp/insp.log (or INSP_LOG_DIR/INSP_LOG_FILE) and is the
   * Phase G crash-dump source.  Default off so dev/test runs stay
   * stderr-only. */
  if (const char *e = std::getenv("INSP_LOG_DAEMON"); e && *e == '1') {
    /* Pick exe path relative to argv[0]'s dir so installed layouts work. */
    std::string exe = "inspd_log";
    if (argc > 0) {
      std::string a0 = argv[0];
      auto slash = a0.find_last_of('/');
      if (slash != std::string::npos) exe = a0.substr(0, slash + 1) + "inspd_log";
    }
    log_open_shm_ring(nullptr, 0);  /* defaults: 16 MB, name "insp_log_ring" */
    log_install_crash_handlers();
    log_spawn_drainer(exe.c_str());
  }

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

