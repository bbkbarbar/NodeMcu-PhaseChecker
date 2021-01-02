/* 
 * IoT thermostat 1ch with new ThingSpeak API
 * Version 0.1 SNAPSHOT

 * Version history:
 *    v0.1 add basic update for ThingSpeak
 *         add working local website
 */

//#define MOCK_TEMPERATURE_SENSORS

#include "pin_definitions.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>

#include <ThingSpeak.h>
#include "secrets.h"

#define VERSION "v0.1"

#define SOFTWARE_NAME "Phase checker"


//==================================
// Pinout configuration
//==================================
// Pin configuration defined in "secrets.h"

//==================================
// CONSTANTS FOR CALCULATIONS
//==================================

#define SERVER_PORT 81

#define ON 1
#define OFF 0


//==================================
// WIFI
//==================================
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;


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




void setup() {

  // Output
  pinMode(HEATER1_PIN1, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(D7, OUTPUT);
  pinMode(BUILT_IN_LED, OUTPUT);
  pinMode(analogPin, INPUT);
  /*pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);/**/
  #ifdef HEATER1_PIN2
    pinMode(HEATER1_PIN2, OUTPUT);
  #endif
  #ifdef FEEDBACK_1
    pinMode(FEEDBACK_1, OUTPUT);
  #endif
  
  // Setup Serial port speed
  Serial.begin(115200);

  Serial.println("\n");
  Serial.print(String(SOFTWARE_NAME));
  Serial.println(VERSION);

  digitalWrite(5, HIGH);

  // Setup WIFI
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for WIFI connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(A0, LOW);

  server.on("/", HandleRoot);
  server.onNotFound( HandleNotFound );
  server.begin();
  Serial.println("HTTP server started at ip " + WiFi.localIP().toString() );
  
  // ThingSpeak
  ThingSpeak.begin(client);  // Initialize ThingSpeak
  Serial.println("TS initialized.");
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
    }
    /*
    digitalWrite(3, HIGH);
    digitalWrite(4, HIGH);
    digitalWrite(5, HIGH);
    digitalWrite(6, HIGH);
    digitalWrite(7, HIGH);
    digitalWrite(8, HIGH);  /**/
    i++;
  }else{
    if(lastValue != 0){
      digitalWrite(BUILT_IN_LED, HIGH);
      lastValue = 0;
      currentState = 0;
      Serial.println("time: " + String(t) + " i: " + String(i) + "\tVal: " + String(val));
      needToUpdate = 1;
    }
    /*digitalWrite(3, LOW);
    digitalWrite(4, LOW);
    digitalWrite(5, LOW);
    digitalWrite(6, LOW);
    digitalWrite(7, LOW);
    digitalWrite(8, LOW); /**/
    i++;
  }

  if(needToUpdate && canUpdateTS()){
    updateTS(currentState);
    needToUpdate = 0;
  }
  
}
