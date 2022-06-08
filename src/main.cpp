#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <SPIFFS.h>

// networking
#define NETWORK_SSID "Lights"
#define NETWORK_PASS "PASSWORD"
#define WEB_PORT    80
#define SOCKET_PORT 81
#define DOMAIN_NAME "lights"
#define LOCAL_IP IPAddress(1,2,3,4)

// lights
#define DATA_PIN 13
#define NUM_LEDS 10
#define MAX_BRIGHTNESS 20 // out of ?
#define FPS 60

// networking
WebServer webServer(WEB_PORT);
websockets::WebsocketsServer socketServer;
websockets::WebsocketsClient socketClient;

enum API{
  COLOR = 100,
  PATTERN,
};

// lights
CRGB leds[NUM_LEDS];
byte currentPattern = 0;
byte sequenceLength = 1;
CRGB colorPallet[] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x390F14};

// TODO: split networking and lights into their own files. Getting messy
void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV(128, 255, 192);
}

void checkered() {
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i] = colorPallet[i%5];
  }
}

void (*patterns[])() = {checkered, sinelon};
const byte patternsCount = sizeof(patterns) / sizeof((patterns)[0]);

//networking
void initWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(NETWORK_SSID, NETWORK_PASS);
  WiFi.softAPConfig(LOCAL_IP, LOCAL_IP, IPAddress(255, 255, 255, 0));
}

void initMDNS() {
  if (!MDNS.begin(DOMAIN_NAME)) {
    Serial.println("MDNS responder failed to start");
    exit(0);
  }
  Serial.println("MDNS responder started");
  MDNS.addService("http", "tcp", WEB_PORT);
  MDNS.addService("ws", "tcp", SOCKET_PORT);
}

void initWebServices() {
  webServer.serveStatic("/", SPIFFS, "/");
  webServer.begin();
  Serial.println("HTTP server started");

  socketServer.listen(SOCKET_PORT);
  if(socketServer.available()){
    Serial.println("Websocket live");
  }
}

void messageCallback(websockets::WebsocketsMessage msg){
    Serial.print("Got Message: ");
    std::string message = msg.rawData();

    switch(message[0]){
      case COLOR:
        Serial.print("Color: ");
        for(int i = 1; i < message.size(); i++){
          Serial.printf("%02x ", message[i]);
        }
        Serial.println();
        break;
      case PATTERN:
        if(message.size() > 1 && message[1] < patternsCount){
          currentPattern = message[1];
          Serial.printf("Pattern: %d", message[1]);
        }
        break;
      default:
        Serial.println("Unknown");
    }
}

void pollWebServices() {
  if(socketServer.poll()){ // there is a waiting connection
    socketClient.close();
    Serial.println("Accepting connection");
    socketClient = socketServer.accept();
    socketClient.onMessage(messageCallback);
    // sendFrameToClient();
    Serial.println("Connection established");
  }
  socketClient.poll();
  webServer.handleClient();
}

void setup() {
  Serial.begin(115200);

  if(!SPIFFS.begin()){
    Serial.println("An error occurred while mounting SPIFFS");
    exit(0);
  }

  initWifi();
  initMDNS();
  initWebServices();
  webServer.begin();

  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, 0, 250).setCorrection(TypicalPixelString);
  FastLED.setBrightness(MAX_BRIGHTNESS);
}

void loop() {
  EVERY_N_MILLISECONDS(1000 / FPS){
  	patterns[currentPattern]();
  	FastLED.show();
  }
  pollWebServices();
}
