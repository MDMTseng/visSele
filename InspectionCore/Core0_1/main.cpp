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




#ifdef _WIN32
/* Opt out of Windows power throttling. Windows 11/10 throttles "background"
 * processes (lowers clock, parks them on E-cores). When visSele is spawned by
 * Electron it's not the foreground window, so it gets throttled and runs much
 * slower than in a console. Declare ourselves performance-critical so Windows
 * runs us at full speed regardless of foreground/background. Disable this
 * opt-out (let Windows throttle) with INSP_ALLOW_THROTTLE=1. */
static void win_optout_power_throttling()
{
  if (const char *e = std::getenv("INSP_ALLOW_THROTTLE"); e && *e == '1') return;

  /* Resolve SetProcessInformation dynamically (Windows 8+/10 API; the static
   * declaration is _WIN32_WINNT-gated and windows.h is already included with a
   * lower target). Struct + constants are defined locally so there's no header
   * version dependency. No-op on older Windows. */
  typedef struct _PPT_STATE { ULONG Version; ULONG ControlMask; ULONG StateMask; } PPT_STATE;
  const ULONG PPT_VERSION_1        = 1;
  const ULONG PPT_EXECUTION_SPEED  = 0x1;
  const int   PROC_POWER_THROTTLING = 4;   /* ProcessPowerThrottling class */

  typedef BOOL (WINAPI *PSetProcessInformation)(HANDLE, int, LPVOID, DWORD);
  HMODULE k32 = GetModuleHandleA("kernel32.dll");
  if (!k32) return;
  PSetProcessInformation pSPI =
      (PSetProcessInformation)(void *)GetProcAddress(k32, "SetProcessInformation");
  if (!pSPI) return;

  PPT_STATE st;
  ZeroMemory(&st, sizeof(st));
  st.Version     = PPT_VERSION_1;
  st.ControlMask = PPT_EXECUTION_SPEED;
  st.StateMask   = 0;   /* 0 => execution-speed throttling DISABLED (full speed) */
  pSPI(GetCurrentProcess(), PROC_POWER_THROTTLING, &st, sizeof(st));
}
#endif

int main(int argc, char **argv)
{
#ifdef _WIN32
  win_optout_power_throttling();
#endif
  /* Phase F.1: opt-in shm-ring + drainer at boot.
   * Enable with INSP_LOG_DAEMON=1.  The drainer writes the rolling
   * /var/log/insp/insp.log (or INSP_LOG_DIR/INSP_LOG_FILE) and is the
   * Phase G crash-dump source.  Default off so dev/test runs stay
   * stderr-only. */
  /* Log daemon (shm ring + drainer process) is ON by default: the drainer owns
   * the rolling ./insp.log (override dir/name via INSP_LOG_DIR / INSP_LOG_FILE)
   * and is the crash-dump source, while the producer hot path becomes just
   * format + memcpy into the ring (no synchronous stderr write -- slow on
   * Windows). Opt OUT with INSP_LOG_DAEMON=0. */
  {
    const char *e = std::getenv("INSP_LOG_DAEMON");
    bool daemon_on = !(e && *e == '0');   /* default on; only "0" disables */
    if (daemon_on) {
      /* Pick the drainer exe path relative to argv[0]'s dir so installed
       * layouts work (handle both '/' and '\\' separators on Windows). */
      std::string exe = "inspd_log";
      if (argc > 0) {
        std::string a0 = argv[0];
        auto slash = a0.find_last_of("/\\");
        if (slash != std::string::npos) exe = a0.substr(0, slash + 1) + "inspd_log";
      }
      log_open_shm_ring(nullptr, 0);  /* defaults: 16 MB, name "insp_log_ring" */
      log_install_crash_handlers();
      int drainer_pid = log_spawn_drainer(exe.c_str());
      /* Only silence the (slow, synchronous) stderr sink once the drainer is
       * actually up -- otherwise there'd be NO log output at all. Keep stderr
       * too with INSP_LOG_KEEP_STDERR=1 (e.g. for a live `tail`). */
      if (drainer_pid > 0) {
        if (const char *k = std::getenv("INSP_LOG_KEEP_STDERR"); !(k && *k == '1'))
          log_set_stderr_enabled(0);
      }
    }
  }
  /* Standalone kill-switch for the synchronous stderr sink, independent of the
   * daemon (e.g. ring-only or quiet high-throughput runs). */
  if (const char *e = std::getenv("INSP_LOG_NO_STDERR"); e && *e == '1')
    log_set_stderr_enabled(0);

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

