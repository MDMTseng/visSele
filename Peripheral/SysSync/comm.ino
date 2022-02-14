

#include "main.hpp"

// #include "esp_wifi.h"

// #include "esp_event_loop.h"

AsyncWebServer server(80);
const char* ssid = "AAAPPP";
const char* password = "88888888";



buffered_print BP(1024);

SimpPacketParse SPP(500);
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);

AsyncWebSocket ws("/ws");






void connectToWiFi(const char * ssid, const char * pwd)
{

  byte mac[6] = {0,0,0,0,0,0};
  WiFi.macAddress(mac);
  Serial.printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    

  Serial.println("scan start");

  WiFi.mode(WIFI_STA);


  while(true)
  {
    
    WiFi.disconnect();
    delay(1000);
    // WiFi.scanNetworks will return the number of networks found
    if(1){
      int n = WiFi.scanNetworks();
      Serial.println("scan done");
      if (n == 0) {
          Serial.println("no networks found");
      } else {
          Serial.print(n);
          Serial.println(" networks found");
          for (int i = 0; i < n; ++i) {
              // Print SSID and RSSI for each network found
              Serial.print(i + 1);
              Serial.print(": ");
              Serial.print(WiFi.SSID(i));
              Serial.print(" (");
              Serial.print(WiFi.RSSI(i));
              Serial.print(")");
              Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
              delay(10);
          }
      }
      Serial.println("");


    }


    Serial.println("Connecting to WiFi network: " + String(ssid));
    WiFi.begin(ssid, pwd);
    delay(2000);
    int waitingCountDown=5;
    
    while (WiFi.status() != WL_CONNECTED) 
    {
      Serial.printf("%d,",WiFi.status());
      if(--waitingCountDown==0)
      {
        break;
      }
      delay(2000);
    }
    Serial.printf("%d",WiFi.status());
    Serial.println();
    if(WiFi.status() == WL_CONNECTED)
    {
      break;
    }

    Serial.println("retry connection");

  }


  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){

  server.end();
  connectToWiFi(ssid,password);
}

extern const char index_html[] asm("_binary_resource_webapp_index_html_start");
extern const char index_html_end[] asm("_binary_resource_webapp_index_html_end");

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){

  // Start server
  server.begin();
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
}

void setup_comm() {

  int retry = 0;
  Serial.printf("%s:%d:",__func__,__LINE__);

  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);

  connectToWiFi(ssid,password);
  Serial.println("Connected!!\n");

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

}

void loop_comm()
{
  // if(WiFi.status() != WL_CONNECTED)
  // {
  //   connectToWiFi(ssid,password);
  // }
  ws.cleanupClients();

  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if(SPP.feed(inChar))
    {
      BP.resize(0);

      BP.write(SPP.sChar());
      CMD_parse(SPP,&BP);

      BP.write(SPP.eChar());
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

            if(client->canSend())
            {
              BP.resize(0);
              BP.write(SPP.sChar());
              CMD_parse(SPP,&BP);
              BP.write(SPP.eChar());
              client->text(BP.buffer());
              BP.resize(0);
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
