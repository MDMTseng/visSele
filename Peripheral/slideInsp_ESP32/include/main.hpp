#pragma once
#include <Arduino.h>


class comm_interface{
  public:
  char name[30];
  // char version[30];
  virtual void onConnected()
  {

  }

  virtual void onError()
  {
    
  }

  virtual void onClosed()
  {
    
  }

  virtual void onMesssage(uint8_t *data, size_t len)
  {

  }

  virtual void sendMesssage(uint8_t *data, size_t len)
  {

  }

};

class Serial_interface:public comm_interface{

};
class IP_interface:public comm_interface{
  public:
  // char version[30];
  virtual void onConnected()
  {

  }

  virtual void onError()
  {
    
  }

  virtual void onClosed()
  {
    
  }

  virtual void onMesssage(uint8_t *data, size_t len)
  {

  }

  virtual void sendMesssage(uint8_t *data, size_t len)
  {

  }

};



void setup_comm();
void loop_comm();


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);