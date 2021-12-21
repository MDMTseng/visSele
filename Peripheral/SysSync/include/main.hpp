#pragma once
#include <Arduino.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "UTIL.h"
#include "RingBuf.hpp"
#include "SimpPacketParse.hpp"
#include <ArduinoJson.h>



void setup_comm();
void loop_comm();


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);