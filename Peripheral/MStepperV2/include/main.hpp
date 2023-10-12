#pragma once
#include <Arduino.h>

// #include <WiFi.h>
// #include <AsyncTCP.h>
// #include <ESPAsyncWebServer.h>

#include "UTIL.h"
#include "RingBuf.hpp"
#include "SimpPacketParse.hpp"
#include <ArduinoJson.h>



#define PRTF_B2b_PAT "%c%c%c%c%c%c%c%c"
#define PRTF_B2b(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 



void setup_comm();
void loop_comm();


// void G_LOG(char* str);


// void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
//              void *arg, uint8_t *data, size_t len);