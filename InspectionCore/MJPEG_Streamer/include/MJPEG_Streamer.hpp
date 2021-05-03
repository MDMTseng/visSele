#ifndef MJPEG_STREAMER_HPP
#define MJPEG_STREAMER_HPP

#include <vector>
#include <string>
#include <regex>

#include <netinet/in.h>

#define SOCKET int
using namespace std;

class MJPEG_Streamer
{
protected:
  struct clientInfo
  {
    int client_fd;
    int state;
    string host;
    string origin;
    string resource;
    struct sockaddr_in addr;
    int waitForHttpInfo;
  };

  struct MJPEG_Streamer_EVT_DATA
  {
    const struct clientInfo *c;

    enum
    {
      CONNECTED=500,
      HTTP_INFO,
      DATA,
      DISCONNECTED_BY_REMOTE,
      DISCONNECTED_BY_LOCAL
    } ev_type;

    uint8_t *raw_data;
    size_t raw_data_L;
  };
  std::regex res_rgx, orig_rgx;
  std::string header;
  SOCKET sock;
  fd_set tracking_fdset;
  int max_fd;
  int port;

  std::vector<uint8_t> recv_buffer;
  int timeout;
  std::vector<struct clientInfo> clientTable;

  int CalcMaxfd();
  clientInfo parse(uint8_t *data, size_t dataL);

  int s_send(int sock, const uint8_t *s, int len);
  int s_read(int sock, uint8_t *buffer, int bufflen);

  bool CLIENT_REMOVE_by_Idx(int clientIdx);

  bool CLIENT_REMOVE_by_fd(int clientFd);

  virtual void EVT(struct MJPEG_Streamer_EVT_DATA ev_data);


  bool release();
  bool open(int port);
public:
  MJPEG_Streamer(int port);
  ~MJPEG_Streamer();
  int GetMaxfd();

  fd_set GetFdset();
  int SendFrame(string channel, uint8_t *jpeg_raw, size_t rawL);
  int fdEventFetch(fd_set &fd_set_flag);
};

#endif
