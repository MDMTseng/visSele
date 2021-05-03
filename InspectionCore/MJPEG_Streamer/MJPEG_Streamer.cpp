#include "MJPEG_Streamer.hpp"

#include <cstdlib>
#include <ctime>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <logctrl.h>

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKADDR struct sockaddr
#define SOCKADDR_IN struct sockaddr_in

int MJPEG_Streamer::CalcMaxfd()
{
  FD_ZERO(&tracking_fdset);
  int max = sock;
  FD_SET(sock, &tracking_fdset);
  for (int i = 0; i < clientTable.size(); i++)
  {

    FD_SET(clientTable[i].client_fd, &tracking_fdset);
    if (max < clientTable[i].client_fd)
      max = clientTable[i].client_fd;
  }

  max_fd = max;
  return max_fd;
}

void MJPEG_Streamer::setFdset(fd_set *dst)
{
  
  FD_SET(sock, dst);
  for (int i = 0; i < clientTable.size(); i++)
  {
    FD_SET(clientTable[i].client_fd, dst);
  }
}

MJPEG_Streamer::clientInfo MJPEG_Streamer::parse(uint8_t *data, size_t dataL)
{
  std::cmatch res_match;
  std::cmatch orig_match;
  clientInfo cinfo;
  cinfo.waitForHttpInfo = true;
  LOGI(">>>%s", (const char *)data);
  if (std::regex_search((const char *)data, (const char *)data + dataL, res_match, res_rgx))
  {
    // LOGI("RES:%s  host:%s",res_match[1],res_match[2]);
    // std::cout << "match: " << match[1] << '\n';
    cinfo.resource = std::string(res_match[1]);
    cinfo.host = std::string(res_match[2]);
    cinfo.waitForHttpInfo = false;
    // _write(client, header.c_str(), header.length());
    // clients.push_back(client);
  }

  return cinfo;
}

int MJPEG_Streamer::s_send(int sock, const uint8_t *s, int len)
{
  if (len < 1)
    return -1;
  {
    try
    {
      int retval = ::send(sock, s, len, 0x4000);
      return retval;
    }
    catch (int e)
    {
      // cout << "An exception occurred. Exception Nr. " << e << '\n';
    }
  }
  return -1;
}
int MJPEG_Streamer::s_read(int sock, uint8_t *buffer, int bufflen)
{
  int result;
  result = recv(sock, buffer, bufflen, 0);
  if (result < 0)
  {
    // cout << "An exception occurred. Exception Nr. " << result << '\n';
    return result;
  }
  // string s = buffer;
  // buffer = (char*) s.substr(0, (int) result).c_str();
  return result;
}

bool MJPEG_Streamer::CLIENT_REMOVE_by_Idx(int clientIdx)
{

  ::shutdown(clientTable[clientIdx].client_fd, 2);

  FD_CLR(clientTable[clientIdx].client_fd, &tracking_fdset);
  clientTable.erase(clientTable.begin() + clientIdx);

  CalcMaxfd();
  return false;
}

bool MJPEG_Streamer::CLIENT_REMOVE_by_fd(int clientFd)
{
  for (int i = 0; i < clientTable.size(); i++)
  {
    if (clientTable[i].client_fd == clientFd)
      ;
    return CLIENT_REMOVE_by_Idx(i);
  }
  return false;
}

void MJPEG_Streamer::EVT(struct MJPEG_Streamer_EVT_DATA ev_data)
{
}

MJPEG_Streamer::MJPEG_Streamer(int port)
    : sock(INVALID_SOCKET), port(port), res_rgx("GET\\s(.+)\\sHTTP.+\\r\\nHost:\\s(.+)\\r\\n"), orig_rgx("Origin\\s(.+)\n")
{

  signal(SIGPIPE, SIG_IGN);
  FD_ZERO(&tracking_fdset);

  header = "HTTP/1.0 200 OK\r\n";
  header += "Cache-Control: no-cache\r\n";
  header += "Pragma: no-cache\r\n";
  header += "Connection: close\r\n";
  header += "Content-Type: multipart/x-mixed-replace; boundary=mjpegstream\r\n\r\n";

  recv_buffer.resize(2048);
  open(port);
}

MJPEG_Streamer::~MJPEG_Streamer()
{
  release();
}
int MJPEG_Streamer::GetMaxfd()
{
  return max_fd;
}

fd_set MJPEG_Streamer::GetFdset()
{
  return tracking_fdset;
}

bool MJPEG_Streamer::release()
{
  if (sock != INVALID_SOCKET)
    shutdown(sock, 2);
  sock = (INVALID_SOCKET);
  return false;
}

bool MJPEG_Streamer::open(int port)
{
  sock = socket(AF_INET, SOCK_STREAM, 0);

  bool _TRUE = true;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &_TRUE, sizeof(int)) == -1)
  {
    perror("Setsockopt");
    // exit(1);
  }

  SOCKADDR_IN address;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (::bind(sock, (SOCKADDR *)&address, sizeof(SOCKADDR_IN)) == SOCKET_ERROR)
  {
    // cerr << "error : couldn't bind sock " << sock << " to port " << port << "!" << endl;
    return release();
  }
  if (listen(sock, 20) == SOCKET_ERROR)
  {
    LOGE("LISTEN FAILED!!!!!");
    // cerr << "error : couldn't listen on sock " << sock << " on port " << port << " !" << endl;
    // return release();
  }

  // LOGI("OPEN!!!!:fd=%d",sock);
  FD_ZERO(&tracking_fdset);
  FD_SET(sock, &tracking_fdset);
  max_fd = sock;
  return true;
}

int MJPEG_Streamer::SendFrame(string channel, uint8_t *jpeg_raw, size_t rawL)
{
  string string_head = "--mjpegstream\r\nContent-Type: image/jpeg\r\nContent-Length: " + std::to_string(rawL) + "\r\n\r\n";
  int clientCount = 0;
  for (int i = 0; i < clientTable.size(); i++)
  {
    if (channel.compare(clientTable[i].resource) != 0)
      continue;

    s_send(clientTable[i].client_fd, (uint8_t *)string_head.c_str(), string_head.size());
    int n = s_send(clientTable[i].client_fd, (uint8_t *)(jpeg_raw), rawL);
    if (n < rawL)
    {
      CLIENT_REMOVE_by_Idx(i);
      i = -1;
    }
  }
  return 0;
}

int MJPEG_Streamer::fdEventFetch(fd_set &fd_set_flag)
{
  if (FD_ISSET(sock, &fd_set_flag))
  {
    struct sockaddr_in remote;
    socklen_t sockaddrLen = sizeof(remote);
    int NewSock = accept(sock, (struct sockaddr *)&remote, &sockaddrLen);
    if (NewSock == -1)
    {

      LOGV("accept failed");
      printf("accept failed");
      //sleep(1000);
      return -2;
    }

    struct clientInfo cinfo;
    cinfo.client_fd = NewSock;
    cinfo.addr = remote;
    cinfo.waitForHttpInfo = true;
    // LOGI("connected %s:%d sock:%d",
    //        inet_ntoa(cinfo.addr), ntohs(cinfo.addr.sin_port),cinfo.client_fd);

    LOGI("NEW CONN sock:%d", cinfo.client_fd);
    FD_SET(NewSock, &tracking_fdset);
    if (NewSock > max_fd)
    {
      max_fd = NewSock;
    }
    clientTable.push_back(cinfo);
    // printf("List size %d\n", ws_conn_pool.size());

    struct MJPEG_Streamer_EVT_DATA ev_data = {
        .client = &(cinfo),
        .ev_type = MJPEG_Streamer_EVT_DATA::CONNECTED};
    EVT(ev_data);
  }
  else
  {
    int evt_counter = 0;
    for (int i = 0; i < clientTable.size(); i++)
    {
      if (FD_ISSET(clientTable[i].client_fd, &fd_set_flag))
      {
        evt_counter++;
        int fd = clientTable[i].client_fd;
        FD_CLR(fd, &fd_set_flag);
        size_t recv_len = recv(fd, &(recv_buffer[0]), recv_buffer.size(), 0);
        if (recv_len >= 0)
        {
          if (clientTable[i].waitForHttpInfo == true)
          {
            clientTable[i].waitForHttpInfo = false;
            clientInfo cinfo = parse(&(recv_buffer[0]), recv_len);
            if (cinfo.waitForHttpInfo == false)
            {
              clientTable[i].resource = cinfo.resource;
              clientTable[i].host = cinfo.host;

              struct MJPEG_Streamer_EVT_DATA ev_data = {
                  .client = &(clientTable[i]),
                  .ev_type = MJPEG_Streamer_EVT_DATA::HTTP_INFO};
              EVT(ev_data);
              s_send(clientTable[i].client_fd, (const uint8_t *)header.c_str(), header.length());
            }
            else
            {

              struct MJPEG_Streamer_EVT_DATA ev_data = {
                  .client = &(clientTable[i]),
                  .ev_type = MJPEG_Streamer_EVT_DATA::DISCONNECTED_BY_LOCAL};
              EVT(ev_data);
              CLIENT_REMOVE_by_Idx(i);
              i--;
              //has to have "resource" info
            }
          }
          else
          {

            struct MJPEG_Streamer_EVT_DATA ev_data = {
              .client = &(clientTable[i]),
              .ev_type = MJPEG_Streamer_EVT_DATA::DATA,
              raw_data : &(recv_buffer[0]),
              raw_data_L : recv_len
            };
            EVT(ev_data);
          }
        }
        else
        {
          struct MJPEG_Streamer_EVT_DATA ev_data = {
              .client = &(clientTable[i]),
              .ev_type = MJPEG_Streamer_EVT_DATA::DISCONNECTED_BY_REMOTE};
          EVT(ev_data);
          CLIENT_REMOVE_by_Idx(i);
          i--;
        }
      }
    }
    // LOGV("listenSocket else end for");

    if (evt_counter == 0)
    {
      // LOGV("No matching event");
      return -2;
    }
  }

  return 0;
}
