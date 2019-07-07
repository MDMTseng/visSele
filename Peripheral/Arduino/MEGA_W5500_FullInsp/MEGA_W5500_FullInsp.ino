/*
  Websocket Server Protocol

  This example demostrate a simple echo server.
  It demostrate how the library <WebSocketProtocol.h> works
  and how to handle the state changes.

  dependent library:WIZNET <Ethernet.h>,ETH_Extra.h

  created  14 Feb 2015
  by MDM Tseng
*/

#include "include/Websocket.hpp"
#include "websocket_FI.hpp"


char buff[600];
IPAddress _ip(192,168,2,2);
IPAddress _gateway(169, 254, 170, 254);
IPAddress _subnet(255, 255, 255, 0);
Websocket_FI WS_Server(buff,sizeof(buff),_ip,5213,_gateway,_subnet);
void setup() {
  Serial.begin(57600);
  setRetryTimeout(3, 100);
  setup_Stepper();
}

void loop() {
  WS_Server.loop_WS();
  loop_Stepper();
}
