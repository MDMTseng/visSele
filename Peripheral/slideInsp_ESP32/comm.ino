

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
const char* ssid = "AAAPPP";
const char* password = "88888888";
AsyncWebSocket ws("/ws");


SimpPacketParse SPP(500);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);
void setup_comm() {

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());


  // Start server
  server.begin();

  ws.onEvent(onEvent);
  server.addHandler(&ws);
}



void loop_comm()
{

  ws.cleanupClients();

  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if(SPP.feed(inChar))
    {
      BP.resize(0);

      ret_doc.clear();
      CMD_parse(SPP,&BP,ret_doc);
      Serial.print(BP.buffer());
      SPP.clean();
    }
  }
}



void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
    {
      SPP.clean();
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        for(int i=0;i<info->len;i++)
        {
          char inChar =data[i];
          if(SPP.feed(inChar))
          {
            BP.resize(0);

            ret_doc.clear();
            CMD_parse(SPP,&BP,ret_doc);
            if(client->canSend())
            {
              client->printf(BP.buffer());
            }
            SPP.clean();
          }
        }
        data[len] = '\0';//zero it
        //TODO
        
      }


      break;
    }
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
