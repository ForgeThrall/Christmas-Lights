#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <SPIFFS.h>

// networking
#define NETWORK_SSID "SSID"
#define NETWORK_PASS "PASS"
#define WEB_PORT    80
#define SOCKET_PORT 81
#define DOMAIN_NAME "lights"

// lights
#define DATA_PIN 13
#define NUM_LEDS 150
#define MAX_BRIGHTNESS 20 // out of ?
#define FPS 60

// networking
WebServer webServer(WEB_PORT);
websockets::WebsocketsServer socketServer;
websockets::WebsocketsClient socketClient;

enum API{
  COLOR = 100,
};

// lights
CRGB leds[NUM_LEDS];
byte currentPattern = 0;
byte sequenceLength = 1;
CRGB sequencePallet[] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x390F14};

//networking
void initWifi() {
  WiFi.begin(NETWORK_SSID, NETWORK_PASS);

  Serial.print("Connecting to WiFi");
  while(WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
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
        for(int i = 1; i < message.size(); i++){
          Serial.printf("%02x ", message[i]);
        }
        Serial.println();
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

// lights
void sinelon() {
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS - 1 );
  leds[pos] += CHSV(128, 255, 192);
}

// Run
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
		sinelon();
		FastLED.show();
	}
  pollWebServices();
}
