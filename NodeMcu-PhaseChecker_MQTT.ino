/* 
 * IoT thermostat 1ch with new ThingSpeak API
 * Version 1.2 RC

 * Version history:
 *    v0.1 add basic update for ThingSpeak
 *         add working local website
 *         
 *    v1.2 add this device as MQTT client
 *         
 */

/*
 * Used board setting:
 * NodeMCU 1.0 (ESP-12E Module)
 */ 

//#define MOCK_TEMPERATURE_SENSORS

#include "pin_definitions.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <Ticker.h>
#include <AsyncMqttClient.h>

#include <ThingSpeak.h>
#include "secrets.h"

#define VERSION "v1.2 SNAPSHOT"
#define IOT_DEVICE_ID  1

#define SOFTWARE_NAME "Phase checker MQTT"

#define TITLE SOFTWARE_NAME

// Temperature MQTT Topic(s)
#define MQTT_PUB_PHASE_STATE "boorfeszek/energy-cheap/state"


//==================================
// Pinout configuration
//==================================
// Pin configuration defined in "secrets.h"

//==================================
//
//==================================

#define SERVER_PORT 81

#define ON 1
#define OFF 0


//==================================
// WIFI
//==================================
const char* ssid = S_SSID;
const char* password = S_PASS;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

//==================================
// MQTT broker
//==================================
#define MQTT_HOST "192.168.1.190"
#define MQTT_PORT 1883


//==================================
// HTTP
//==================================
ESP8266WebServer server(SERVER_PORT);


//==================================
// ThingSpeak
//==================================
WiFiClient client;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

#define UPDATE_DELAY 60000


//------------------------------------------

int analogPin = A0;
int val = 0;
int currentState = 0;

int i = 0;

int lastValue = 0;
int needToUpdate = 1;
long lastUpdateTime = -9999999;

//------------------------------------------




void updateTS(int status){

  ThingSpeak.setField(1, status);

  // write to the ThingSpeak channel
  //int resp = ThingSpeak.writeFields(SECRET_CH_ID, myWriteAPIKey);
  int resp = ThingSpeak.writeField(SECRET_CH_ID, 1, status, myWriteAPIKey);
  if(resp == 200){
    lastUpdateTime = millis();
    Serial.println("Channel update successful. "+ String(millis()));
  }
  else{
    Serial.println("Problem updating channel. HTTP error code " + String(resp) + " " + String(millis()));
  }
  
}

int canUpdateTS(){
  long now = millis();
  if((now - lastUpdateTime) < UPDATE_DELAY){
    Serial.println("Can not update TS now (" + String(millis()) + ")");
    return 0;
  }else{
    return 1;
  }
}


String generateHtmlHeader(){
  String h = "<!DOCTYPE html>";
  h += "<html lang=\"en\">";
  h += "\n\t<head>";
  h += "\n\t\t<meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  h += "\n\t</head>";
  return h;
}


String generateHtmlBody(){
  String m = "\t<body>";

  m += "\t\t<h4>";
  m += String(SOFTWARE_NAME) + " " + String(VERSION);
  m += "</h4><br>";

  m += "\t\t<h1>";
  m += TITLE;
  m += "</h1><br>";
  if(currentState){
    m += "<b>State: ON</b>";
  }else{
    m += "<b>State: OFF</b>";
  }

  return m;
}


void HandleRoot(){
  //String message = "<!DOCTYPE html><html lang=\"en\">";
  //message += "<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>";
  String message = generateHtmlHeader();
  message += generateHtmlBody();

  server.send(200, "text/html", message );
}

void HandleData(){
  String message = "" + String(currentState);
  server.send(200, "text/html", message );
}


void HandleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}


void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  Serial.println("MQTT server set to: " + String(MQTT_HOST));
  WiFi.begin(ssid, password);
}



void setup() {

  // Output
  pinMode(13, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(BUILT_IN_LED, OUTPUT);
  pinMode(analogPin, INPUT);
  
  // Setup Serial port speed
  Serial.begin(115200);

  Serial.println("\n");
  Serial.print(String(SOFTWARE_NAME));
  Serial.println(VERSION);

  digitalWrite(5, HIGH);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  Serial.println("MQTT server set to: " + String(MQTT_HOST));
  // If your broker requires authentication (username and password), set them below
  mqttClient.setCredentials("homeassistant", "Iephi1puhiexeiWiecie4aifooreramo5pooNgeem2ahvie7Thaibae1booxaech");

  // Setup WIFI
  connectToWifi();
  Serial.println("");

  // Wait for WIFI connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);

  // for reconnecting feature
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.hostname(String(SOFTWARE_NAME) + " [" + String(IOT_DEVICE_ID) + "] " + String(VERSION));
  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(A0, LOW);

  server.on("/", HandleRoot);
  server.on("/data", HandleData);
  server.onNotFound( HandleNotFound );
  server.begin();
  Serial.println("HTTP server started at ip " + WiFi.localIP().toString() );
  
  // ThingSpeak
  //ThingSpeak.begin(client);  // Initialize ThingSpeak
  //Serial.println("TS initialized.");
  digitalWrite(BUILT_IN_LED, HIGH);

}


void loop() {
  long t = millis();
  server.handleClient();
  delay(1000);
  
  val = analogRead(analogPin);  // read the input pin
  
  if(val > 512){
    if(lastValue != 1){
      digitalWrite(BUILT_IN_LED, LOW);
      lastValue = 1;
      currentState = 1;
      Serial.println("time: " + String(t) + " i: " + String(i) + "\tVal: " + String(val));
      needToUpdate = 1;
      
      // Publish MQTT msg
      uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_PHASE_STATE, 1, true, String(currentState).c_str());                            
      Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_PHASE_STATE, packetIdPub1);
      Serial.printf("Message: %i \n", currentState);
    }
    
    i++;
  }else{
    if(lastValue != 0){
      digitalWrite(BUILT_IN_LED, HIGH);
      lastValue = 0;
      currentState = 0;
      Serial.println("time: " + String(t) + " i: " + String(i) + "\tVal: " + String(val));
      needToUpdate = 1;
      // Publish MQTT msg
      uint16_t packetIdPub1 = mqttClient.publish(MQTT_PUB_PHASE_STATE, 1, true, String(currentState).c_str());                            
      Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", MQTT_PUB_PHASE_STATE, packetIdPub1);
      Serial.printf("Message: %i \n", currentState);
    }
    i++;
  }

  /*
  if(needToUpdate && canUpdateTS()){
    updateTS(currentState);
    needToUpdate = 0;
  }
  /**/
  
}
