
#include "acvImage_BasicTool.hpp"
#include "logctrl.h"
#include <float.h>
#include "common_lib.h"

#include "BPG_Protocol.hpp"

class m_BPG_Protocol_Interface : public BPG_Protocol_Interface
{
public:
  int toUpperLayer(BPG_protocol_data bpgdat)
  {
    bpgdat.dat_raw[bpgdat.size] = '\0';
    LOGI(">>> data %s", bpgdat.dat_raw);

    BPG_protocol_data ret_dat;
    ret_dat.tl[0] = 'A';
    ret_dat.tl[1] = 'A';

    uint8_t *data = requestSendingBuffer(500);
    ret_dat.prop = 'x';
    ret_dat.dat_raw = data;
    ret_dat.size = sprintf((char *)data, "GOGOGO");
    ret_dat.pgID = bpgdat.pgID;
    ret_dat.callback = NULL;
    ret_dat.callbackInfo = NULL;
    fromUpperLayer(ret_dat);
  }
};

// DatCH_CallBack_WSBPG callbk_obj;
BPG_Link_Interface_WebSocket ifwebsocket(7714);
m_BPG_Protocol_Interface bpg_pi;
int tmpMain()
{
  ifwebsocket.setUpperLayer(&bpg_pi);
  bpg_pi.setLink(&ifwebsocket);
  while (1)
  {
    LOGI("WAIT..");
    fd_set fd_s = ifwebsocket.get_fd_set();
    int maxfd = ifwebsocket.findMaxFd();
    if (select(maxfd + 1, &fd_s, NULL, NULL, NULL) == -1)
    {
      perror("select");
      exit(4);
    }

    ifwebsocket.runLoop(&fd_s, NULL);
  }
  return 0;
}