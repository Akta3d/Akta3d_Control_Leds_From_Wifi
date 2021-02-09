#include <Adafruit_NeoPixel.h>    // https://github.com/adafruit/Adafruit_NeoPixel
#include <ESP8266WiFi.h>
#include <WebSocketClient.h>      // https://github.com/morrissinger/ESP8266-Websocket
#include <ArduinoJson.h>

#define PIXEL_COUNT       30
#define WIFI_SSID         "tittrucnet"
#define WIFI_PASSWORD     "gr0sTrucP@sNet"
#define WEBSOCKET_HOST    "192.168.1.26"
#define WEBSOCKET_PORT    8080
#define WEBSOCKET_PATH    "/ws"

struct RGB {
  byte r;
  byte g;
  byte b;
};

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, D4, NEO_GRB + NEO_KHZ800);

String colorMode = "colorModeRandom"; // "colorModeSame": all sames colors, "colorModeSlide": slide, "colorModeRandom": tourne en rond en choisissant n'importe quoi
int delaySlide;

WebSocketClient wsClient;
WiFiClient client;

RGB color;
RGB lastColor;
int scrolling;

DynamicJsonDocument doc(1024);
DeserializationError jsonError;
JsonObject jsonObj;

bool connected;

void setup() {
  Serial.begin(9600); // Used for debugging messages

  // Initialize the colors
  lastColor = {255, 0, 0};
  color = {0, 0, 255};
  scrolling = 0;
  delaySlide = 20;
  
  strip.begin();
  strip.show();

  // start scrolling
  scrolling = strip.numPixels();
      
  setup_wifi();
  setup_websocket();
}

void loop() {
  delaySlide = 20;
  if(colorMode == "colorModeRandom") {
    delaySlide = 100;
  }
          
  if (scrolling > 0) { // If scrolling is greater than 0, render LEDs
    renderLED(strip.numPixels() - scrolling);
    delay(delaySlide);
    scrolling--;
    return;
  } else if(colorMode == "colorModeRandom") {
    color = {random(255), random(255), random(255)};
    scrolling = strip.numPixels();
  }
  
  // If connection fails / breaks, the first LED blinks red
  if (!client.connected() || !connected) {
    renderLEDColor(0, {255, 0, 0});
    delay(500);
    renderLEDColor(0, {0, 0, 0});
    delay(500);
  }   
  
  if (client.connected() && connected){
    // When receiving data, process it and start scrolling animation
    // Expected format is: RRRGGGBBB (as in 255000000, 035127078, ...)
    // There is no error handling
    String data;
    wsClient.getData(data);
    if (data.length() > 0) {
      Serial.print("Received: ");
      Serial.println(data);

      // Deserialize the JSON document
      jsonError = deserializeJson(doc, data);
      if (jsonError) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(jsonError.f_str());
        return;
      }

      jsonObj = doc.as<JsonObject>();
      
      String action = jsonObj[String("action")];
      String value = jsonObj[String("value")];
      Serial.println("Action: " + action);
      Serial.println("Value: " + value);
      
      if(action == "color") {
        int red   = value.substring(0, 3).toInt() / 255.0 * 100;
        int green = value.substring(3, 6).toInt() / 255.0 * 100;
        int blue  = value.substring(6, 9).toInt() / 255.0 * 100;
        Serial.print("Red: ");
        Serial.println(red);
        Serial.print("Green: ");
        Serial.println(green);
        Serial.print("Blue: ");
        Serial.println(blue);
  
        // Update colors
        lastColor = color;
        color = {red, green, blue};
  
        // Start scrolling animation
        scrolling = strip.numPixels();
      } else if(action == "colorMode") {
        colorMode = value;
        
        // Start scrolling animation
        scrolling = strip.numPixels();
      }
    }
  }
}


void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WiFi is connected on ");
  Serial.println(WIFI_SSID);
  Serial.print("=> IP address : ");
  Serial.println(WiFi.localIP());
}

void setup_websocket() {
  wsClient.host = WEBSOCKET_HOST;
  wsClient.path = WEBSOCKET_PATH;
  Serial.println("Initiating Websocket connection...");
  if(client.connect(WEBSOCKET_HOST, WEBSOCKET_PORT)) {
    Serial.println("Connected.");
    Serial.println("Initializing websocket...");
    if(wsClient.handshake(client)) {
      Serial.println("Handshake successful.");
      
      connected = true;
    } else {
      Serial.println("Handshake failed.");
      connected = false;
    }
  } else {
    Serial.println("Connection failed.");
    connected = false;
  }
}

double diffAbs(byte last, byte current) {
  return (double) (last > current ? last - current : current - last);
}

byte computeValueAt(byte last, byte current, int i) {
  double ratio = (double) i / (double) strip.numPixels();
  return (current + (last > current ? 1 : -1) * (byte) (diffAbs(last, current) * ratio));
}

void renderLED(int i) {
  byte r = (byte) color.r;
  byte g = (byte) color.g;
  byte b = (byte) color.b;
  if(colorMode == "colorModeSlide") {
    r = computeValueAt(lastColor.r, color.r, i);
    g = computeValueAt(lastColor.g, color.g, i);
    b = computeValueAt(lastColor.b, color.b, i);
  }
    
  strip.setPixelColor(i, r, g, b);
  strip.show();
}

void renderLEDColor(int i, RGB color) {
  strip.setPixelColor(i, color.r, color.g, color.b);
  strip.show();
}
