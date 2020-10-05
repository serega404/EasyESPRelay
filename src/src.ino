 /*
|-------------------------------------------|
            -|By serega404|-
               SmartRelay
|-------------------------------------------|
*/

//------Libraries------
#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <Ticker.h>
//------Libraries------

//------Const------
#define RELAY           3 // relay connected to GPIO3, dont use GPIO2 in ESP-01

#define WIFI_SSID       "SSID"
#define WIFI_PASSWORD   "PASSWORD"

#define OTAUSER         "admin"
#define OTAPASSWORD     "admin"
#define OTAPATH         "/firmware"
#define SERVERPORT      80

#define MQTT_HOST       IPAddress(192, 168, 0, 2)
#define MQTT_PORT       1883
#define MQTT_LOGIN      "user"
#define MQTT_PASSWORD   "password"

// 0 - off | 1 - on
const bool defaultState = 1; // if wifi or mqtt is not available then the relay will be automatically turned on or off
const int reconnectTime = 10;

// MQTT Topics
const char* ipTopic = "dvor/light1/ip"; // this topic will return the ip address after entering the boot mode
const char* bootTopic = "dvor/light1/boot"; // this topic is required for http updater
const char* setStateTopic = "dvor/light1/state/set"; // this topic is necessary in order to get information from mqqt broker
const char* stateTopic = "dvor/light1/state"; // this topic is necessary to find out whether the relay is enabled or disabled
//------Const------

//------Variables------
bool bootMode = false;
//------Variables------

//------Initialization------
AsyncMqttClient mqttClient;
Ticker reconnectTimer;

ESP8266WebServer HttpServer(SERVERPORT);
ESP8266HTTPUpdateServer httpUpdater;
//------Initialization------

void setup() 
{
  pinMode(RELAY,OUTPUT);
  
  // If you use the RX TX pins for relay, you need to disable UART
  if (RELAY != 3 && RELAY != 1)
    Serial.begin(9600);

  Serial.println();
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_LOGIN, MQTT_PASSWORD);

  httpUpdater.setup(&HttpServer, OTAPATH, OTAUSER, OTAPASSWORD);
  HttpServer.onNotFound(handleNotFound);
  HttpServer.begin();
  
  wifiConnect();
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Couldn't connect to Wi Fi!");
    defaultState ? digitalWrite(RELAY, HIGH) : digitalWrite(RELAY, LOW);
  } else if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
  }
}

void loop() 
{
  if (bootMode) {
    HttpServer.handleClient();
  }
}

void wifiConnect() {
    unsigned long ConnectStart = millis();
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  
    Serial.print("Connecting to Wi-Fi");
    
    while (millis() - ConnectStart < 10000 && WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    } 
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
}

void reconnectFunc() {
  Serial.println("Reconnect...");
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
  } else if (mqttClient.connected()) {
    reconnectTimer.detach();
  }
}

// MQTT Callback start

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT. ");
  
  mqttClient.subscribe(setStateTopic, 1);
  Serial.println("Subscribing at setStateTopic");

  mqttClient.subscribe(bootTopic, 2);
  Serial.println("Subscribing at bootTopic");
  
  mqttClient.publish(stateTopic, 1, true, "1");
  Serial.println("Publishing default state");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  defaultState ? digitalWrite(RELAY, HIGH) : digitalWrite(RELAY, LOW);
  if (!mqttClient.connected()) {
    reconnectTimer.attach(reconnectTime, reconnectFunc);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println("New message: \"" + String(payload) + "\" in topic: " + String(topic));
  if (String(topic).substring(0, 21) == setStateTopic) {
    if (String(payload).substring(0, 1) == "1") {
      Serial.println("RELAY: ON");
      digitalWrite(RELAY, HIGH);
      mqttClient.publish(stateTopic, 1, true, "1");
    } else if (String(payload).substring(0, 1) == "0") {
      Serial.println("RELAY: OFF");
      digitalWrite(RELAY, LOW);
      mqttClient.publish(stateTopic, 1, true, "0");
    }
  } else if(String(topic).substring(0, 16) == bootTopic) {
    if (String(payload).substring(0, 1) == "1") {
      Serial.println("BOOT: ON");
      mqttClient.publish(ipTopic, 1, true, WiFi.localIP().toString().c_str());
      bootMode = true;
    } else if (String(payload).substring(0, 1) == "0") {
      Serial.println("BOOT: OFF");
      bootMode = false;
    }
  }
}

// MQTT Callback end

void handleNotFound() {
  HttpServer.send(404, "text/plain", "404: Not found");
}
