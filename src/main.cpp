#include <Preferences.h>
#include <esp_dmx.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <WS2812FX.h>
#include <ArduinoJson.h>

// Init Internal LED
WS2812FX ws2812fx = WS2812FX(1, 18, NEO_GRB + NEO_KHZ800);

// Init persistant storage
Preferences preferences;

// DMX Config
int transmitPin = 17;
int recievePin = 16;
int enablePin = 21;
dmx_port_t dmxPort = 1; // USE UART1

byte dmxData[DMX_PACKET_SIZE];


// WIFI CONFIG (TEMP)
const char* ssid = "FRITZ!Box Netter";
const char* password = "W-LAN_Netter(44)";

// WEBSOCKET CONFIG
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

long lastWifiLoopRun = 0;
long lastWSLoopRun = 0;
int lastWifiStatus = -1;
void driveWifi() {
  if (millis() - lastWifiLoopRun >= 200) {
    int status = WiFi.status();

    if (status != lastWifiStatus) {

      String message = "";
      switch (WiFi.status()) {
        case 0:
          message = "WL_IDLE_STATUS";
          break;
        case 1:
          message = "WL_NO_SSID_AVAIL";
          break;
        case 2:
          message = "WL_SCAN_COMPLETED";
          break;
        case 3:
          message = "WL_CONNECTED";
          ws2812fx.setMode(FX_MODE_STATIC);
          ws2812fx.setColor(0,0,0);

          Serial.println(WiFi.localIP());
          break;
        case 4:
          message = "WL_CONNECT_FAILED";
           ws2812fx.setMode(FX_MODE_BLINK);
            ws2812fx.setColor(255,0,0);
            ws2812fx.setSpeed(100);
          break;
        case 5:
          message = "WL_CONNECTION_LOST";
          ws2812fx.setMode(FX_MODE_BLINK);
          ws2812fx.setColor(255,0,0);
          ws2812fx.setSpeed(100);
          break;
        case 6:
          message = "WL_DISCONNECTED";
          ws2812fx.setMode(FX_MODE_BLINK);
          ws2812fx.setColor(255,255,0);
          ws2812fx.setSpeed(500);
        default:
          break;
      }

      Serial.println(message);
    }

    lastWifiStatus = status;
    lastWifiLoopRun = millis();
  }

  // WS Alive Ticker
  if (millis() - lastWSLoopRun >= 500) {
    String output = "STATUS|CH[";

    for (size_t i = 1; i < sizeof(dmxData); i++)
    {
      output += String(dmxData[i]) + ",";
    }

    output = output.substring(0, output.length()-1);
    output += "]";
    
    ws.textAll(output);

    lastWSLoopRun = millis();
  }
}

bool lol = false;
long lastDmxLoopRun = 0;
void driveDMX() {
  if (millis() - lastDmxLoopRun >= 100) {
    dmxData[4] = 255;
    //dmxData[5]++;

    /*
    if (lol) {
      dmxData[8] = 255;
    } else {
      dmxData[8] = 0;
    }
    lol = !lol;
    */

    //Serial.println(String(dmxData[5]) + "|" + String(dmxData[6]) + "|" + String(dmxData[7]));

    dmx_write(dmxPort, dmxData, DMX_PACKET_SIZE);
  }

  dmx_send(dmxPort, DMX_PACKET_SIZE);
  dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
}


void handleWSMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;

    Serial.println((char*)data);
    String msg = String((char*)data);

    if (msg == "r") {
      dmxData[5] = 255;
      dmxData[6] = 0;
      dmxData[7] = 0;
    } else if (msg == "g") {
      Serial.println("gggg");
      dmxData[5] = 0;
      dmxData[6] = 255;
      dmxData[7] = 0;
    } else if (msg == "b") {
      dmxData[5] = 0;
      dmxData[6] = 0;
      dmxData[7] = 255;
    }
  }
}

void onWSEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWSMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void setup() {

  // Setup Serial
  Serial.begin(115200);

  // Setup Internal LED
  ws2812fx.init();
  ws2812fx.setBrightness(25);
  ws2812fx.start();

  // Setup persistant storage
  preferences.begin("espdmx", false);

  /*
  preferences.putUInt("test",preferences.getUInt("test") + 1);

  Serial.println(String(preferences.getUInt("test")));
  */

  // Setup DMX
  dmx_set_pin(dmxPort, transmitPin, recievePin, enablePin);
  dmx_driver_install(dmxPort, DMX_DEFAULT_INTR_FLAGS);
  
  // Setup WIFI
  WiFi.begin(ssid, password);

  // Setup WebSocket
  ws.onEvent(onWSEvent);
  server.addHandler(&ws);

  server.begin();

  // Setup Service Discovery
  if(!MDNS.begin("espdmx-device")) {
     Serial.println("Error starting mDNS");
     return;
  }
  	
  MDNS.addService("espdmx", "tcp", 80);
}

void loop() {

  // WIFI DRIVER
  driveWifi();

  // DMX DRIVER
  driveDMX();

  // LED DRIVER
  ws2812fx.service();
}