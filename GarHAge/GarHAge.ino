/*   
 * GarHAge
 * a Home-Automation-friendly ESP8266-based MQTT Garage Door Controller
 * Licensed under the MIT License, Copyright (c) 2017 marthoc
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include "config.h"

const char* garhageVersion = "2.0.0";

// Mapping NodeMCU Ports to Arduino GPIO Pins
// Allows use of NodeMCU Port nomenclature in config.h
#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12 
#define D7 13
#define D8 15

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const boolean static_ip = STATIC_IP;
IPAddress ip(IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);

const char* mqtt_broker = MQTT_BROKER;
const char* mqtt_clientId = MQTT_CLIENTID;
const char* mqtt_username = MQTT_USERNAME;
const char* mqtt_password = MQTT_PASSWORD;

const boolean activeHighRelay = ACTIVE_HIGH_RELAY;

const char* door1_alias = DOOR1_ALIAS;
const char* mqtt_door1_action_topic = MQTT_DOOR1_ACTION_TOPIC;
const char* mqtt_door1_status_topic = MQTT_DOOR1_STATUS_TOPIC;
const int door1_openPin = DOOR1_OPEN_PIN;
const int door1_closePin = DOOR1_CLOSE_PIN;
const int door1_statusPin = DOOR1_STATUS_PIN;
const char* door1_statusSwitchLogic = DOOR1_STATUS_SWITCH_LOGIC;

const boolean door2_enabled = DOOR2_ENABLED;
const char* door2_alias = DOOR2_ALIAS;
const char* mqtt_door2_action_topic = MQTT_DOOR2_ACTION_TOPIC;
const char* mqtt_door2_status_topic = MQTT_DOOR2_STATUS_TOPIC;
const int door2_openPin = DOOR2_OPEN_PIN;
const int door2_closePin = DOOR2_CLOSE_PIN;
const int door2_statusPin = DOOR2_STATUS_PIN;
const char* door2_statusSwitchLogic = DOOR2_STATUS_SWITCH_LOGIC;

const boolean aux_door1_enabled = AUX_DOOR1_ENABLED;
const char* aux_door1_alias = AUX_DOOR1_ALIAS;
const char* mqtt_aux_door1_action_topic = MQTT_AUX_DOOR1_ACTION_TOPIC;
const char* mqtt_aux_door1_status_topic = MQTT_AUX_DOOR1_STATUS_TOPIC;
const int aux_door1_statusPin = AUX_DOOR1_STATUS_PIN;
const char* aux_door1_statusSwitchLogic = AUX_DOOR1_STATUS_SWITCH_LOGIC;

const boolean aux_door2_enabled = AUX_DOOR2_ENABLED;
const char* aux_door2_alias = AUX_DOOR2_ALIAS;
const char* mqtt_aux_door2_action_topic = MQTT_AUX_DOOR2_ACTION_TOPIC;
const char* mqtt_aux_door2_status_topic = MQTT_AUX_DOOR2_STATUS_TOPIC;
const int aux_door2_statusPin = AUX_DOOR2_STATUS_PIN;
const char* aux_door2_statusSwitchLogic = AUX_DOOR2_STATUS_SWITCH_LOGIC;

const boolean dht_enabled = DHT_ENABLED;
const char* mqtt_dht_action_topic = MQTT_DHT_ACTION_TOPIC;
String dhtBase = MQTT_DHT_STATUS_TOPIC;
String dhtTempSuffix = "/temperature";
String dhtTempTopicStr = dhtBase + dhtTempSuffix;
const char* dhtTempTopic = dhtTempTopicStr.c_str();
String dhtHumSuffix = "/humidity";
String dhtHumTopicStr = dhtBase + dhtHumSuffix;
const char* dhtHumTopic = dhtHumTopicStr.c_str();

const unsigned long dht_publish_interval_s = DHT_PUBLISH_INTERVAL;
unsigned long dht_lastReadTime = 0;

const int relayActiveTime = 500;
int door1_lastStatusValue = 2;
int door2_lastStatusValue = 2;
int aux_door1_lastStatusValue = 2;
int aux_door2_lastStatusValue = 2;
unsigned long door1_lastSwitchTime = 0;
unsigned long door2_lastSwitchTime = 0;
unsigned long aux_door1_lastSwitchTime = 0;
unsigned long aux_door2_lastSwitchTime = 0;
int debounceTime = 2000;
String door1_statusString = "";
String door2_statusString = "";
String aux_door1_statusString = "";
String aux_door2_statusString = "";

String availabilityBase = MQTT_CLIENTID;
String availabilitySuffix = "/availability";
String availabilityTopicStr = availabilityBase + availabilitySuffix;
const char* availabilityTopic = availabilityTopicStr.c_str();
const char* birthMessage = "online";
const char* lwtMessage = "offline";

// Home Assistant MQTT Discovery variables

const boolean discoveryEnabled = HA_MQTT_DISCOVERY;
String discoveryPrefix = HA_MQTT_DISCOVERY_PREFIX;
String discoveryConfigSuffix = "/discovery";
String discoveryConfigTopicStr = availabilityBase + discoveryConfigSuffix;
const char* discoveryConfigTopic = discoveryConfigTopicStr.c_str();

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// Wifi setup function

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  if (static_ip) {
    WiFi.config(ip, gateway, subnet);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print(" WiFi connected - IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_ota() {
  ArduinoOTA.setHostname(mqtt_clientId);

  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("End OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("OTA Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("OTA Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("OTA Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("OTA Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("OTA End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("Ready for OTA Upload...");
}

// Callback when MQTT message is received; calls triggerDoorAction(), passing topic and payload as parameters

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  
  Serial.println();

  String topicToProcess = topic;
  payload[length] = '\0';
  String payloadToProcess = (char*)payload;
  triggerDoorAction(topicToProcess, payloadToProcess);
}

// Functions that check door status and publish an update when called

void publish_door1_status() {
  if (digitalRead(door1_statusPin) == LOW) {
    if (door1_statusSwitchLogic == "NO") {
      Serial.print(door1_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_door1_status_topic, "closed", true);
      door1_statusString = "closed";
    }
    else if (door1_statusSwitchLogic == "NC") {
      Serial.print(door1_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_door1_status_topic, "open", true);
      door1_statusString = "open";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for DOOR1_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
  else {
    if (door1_statusSwitchLogic == "NO") {
      Serial.print(door1_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_door1_status_topic, "open", true);
      door1_statusString = "open";
    }
    else if (door1_statusSwitchLogic == "NC") {
      Serial.print(door1_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_door1_status_topic, "closed", true);
      door1_statusString = "closed";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for DOOR1_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
}

void publish_door2_status() {
  if (digitalRead(door2_statusPin) == LOW) {
    if (door2_statusSwitchLogic == "NO") {
      Serial.print(door2_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_door2_status_topic, "closed", true);
      door2_statusString = "closed";
    }
    else if (door2_statusSwitchLogic == "NC") {
      Serial.print(door2_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_door2_status_topic, "open", true);
      door2_statusString = "open";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for DOOR2_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
  else {
    if (door2_statusSwitchLogic == "NO") {
      Serial.print(door2_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_door2_status_topic, "open", true);
      door2_statusString = "open";
    }
    else if (door2_statusSwitchLogic == "NC") {
      Serial.print(door2_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_door2_status_topic, "closed", true);
      door2_statusString = "closed";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for DOOR2_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
}

void publish_aux_door1_status() {
  if (digitalRead(aux_door1_statusPin) == LOW) {
    if (aux_door1_statusSwitchLogic == "NO") {
      Serial.print(aux_door1_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_aux_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door1_status_topic, "closed", true);
      door2_statusString = "closed";
    }
    else if (aux_door1_statusSwitchLogic == "NC") {
      Serial.print(aux_door1_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_aux_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door1_status_topic, "open", true);
      door2_statusString = "open";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for AUX_DOOR1_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
  else {
    if (aux_door1_statusSwitchLogic == "NO") {
      Serial.print(aux_door1_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_aux_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door1_status_topic, "open", true);
      aux_door1_statusString = "open";
    }
    else if (aux_door1_statusSwitchLogic == "NC") {
      Serial.print(aux_door1_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_aux_door1_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door1_status_topic, "closed", true);
      aux_door1_statusString = "closed";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for AUX_DOOR1_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
}

void publish_aux_door2_status() {
  if (digitalRead(aux_door2_statusPin) == LOW) {
    if (aux_door2_statusSwitchLogic == "NO") {
      Serial.print(aux_door2_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_aux_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door2_status_topic, "closed", true);
      door2_statusString = "closed";
    }
    else if (aux_door2_statusSwitchLogic == "NC") {
      Serial.print(aux_door2_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_aux_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door2_status_topic, "open", true);
      door2_statusString = "open";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for AUX_DOOR2_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
  else {
    if (aux_door2_statusSwitchLogic == "NO") {
      Serial.print(aux_door2_alias);
      Serial.print(" open! Publishing to ");
      Serial.print(mqtt_aux_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door2_status_topic, "open", true);
      aux_door1_statusString = "open";
    }
    else if (aux_door2_statusSwitchLogic == "NC") {
      Serial.print(aux_door2_alias);
      Serial.print(" closed! Publishing to ");
      Serial.print(mqtt_aux_door2_status_topic);
      Serial.println("...");
      client.publish(mqtt_aux_door2_status_topic, "closed", true);
      aux_door2_statusString = "closed";      
    }
    else {
      Serial.println("Error! Specify only either NO or NC for AUX_DOOR2_STATUS_SWITCH_LOGIC in config.h! Not publishing...");
    }
  }
}

// Functions that run in loop() to check each loop if door status (open/closed) has changed and call publish_doorX_status() to publish any change if so

void check_door1_status() {
  int currentStatusValue = digitalRead(door1_statusPin);
  if (currentStatusValue != door1_lastStatusValue) {
    unsigned long currentTime = millis();
    if (currentTime - door1_lastSwitchTime >= debounceTime) {
      publish_door1_status();
      door1_lastStatusValue = currentStatusValue;
      door1_lastSwitchTime = currentTime;
    }
  }
}

void check_door2_status() {
  int currentStatusValue = digitalRead(door2_statusPin);
  if (currentStatusValue != door2_lastStatusValue) {
    unsigned long currentTime = millis();
    if (currentTime - door2_lastSwitchTime >= debounceTime) {
      publish_door2_status();
      door2_lastStatusValue = currentStatusValue;
      door2_lastSwitchTime = currentTime;
    }
  }
}

void check_aux_door1_status() {
  int currentStatusValue = digitalRead(aux_door1_statusPin);
  if (currentStatusValue != aux_door1_lastStatusValue) {
    unsigned long currentTime = millis();
    if (currentTime - aux_door1_lastSwitchTime >= debounceTime) {
      publish_aux_door1_status();
      aux_door1_lastStatusValue = currentStatusValue;
      aux_door1_lastSwitchTime = currentTime;
    }
  }
}

void check_aux_door2_status() {
  int currentStatusValue = digitalRead(aux_door2_statusPin);
  if (currentStatusValue != aux_door2_lastStatusValue) {
    unsigned long currentTime = millis();
    if (currentTime - aux_door2_lastSwitchTime >= debounceTime) {
      publish_aux_door2_status();
      aux_door2_lastStatusValue = currentStatusValue;
      aux_door2_lastSwitchTime = currentTime;
    }
  }
}

// Function that publishes birthMessage

void publish_birth_message() {
  // Publish the birthMessage
  Serial.print("Publishing birth message \"");
  Serial.print(birthMessage);
  Serial.print("\" to ");
  Serial.print(availabilityTopic);
  Serial.println("...");
  client.publish(availabilityTopic, birthMessage, true);
}

// Function that toggles the relevant relay-connected output pin

void toggleRelay(int pin) {
  if (activeHighRelay) {
    digitalWrite(pin, HIGH);
    delay(relayActiveTime);
    digitalWrite(pin, LOW);
  }
  else {
    digitalWrite(pin, LOW);
    delay(relayActiveTime);
    digitalWrite(pin, HIGH);
  }
}

// Functions to check the current door status and return true if status does not match the passed-in parameter 
// For use in triggerDoorAction()
// (i.e. the current state does not match the requested state, we are good to take action)

boolean door1_sanityCheck(String state) {
  if (state != door1_statusString) {
    return true;
  }
  else {
    return false;
  }
}

boolean door2_sanityCheck(String state) {
  if (state != door2_statusString) {
    return true;
  }
  else {
    return false;
  }
}

// Function called by callback() when a message is received 
// Passes the message topic as the "requestedDoor" parameter and the message payload as the "requestedAction" parameter
// Calls doorX_sanityCheck() to verify that the door is in a different state than requested before taking action, else trigger a status update

void triggerDoorAction(String requestedDoor, String requestedAction) {
  if (requestedDoor == mqtt_door1_action_topic && requestedAction == "OPEN") {
    if (door1_sanityCheck("open")) {
      Serial.print("Triggering ");
      Serial.print(door1_alias);
      Serial.println(" OPEN relay!");
      toggleRelay(door1_openPin);
    }
    else {
      Serial.print("OPEN requested but ");
      Serial.print(door1_alias);
      Serial.println(" is already open. Publishing status update instead!");
      publish_door1_status();
    }
  }

  else if (requestedDoor == mqtt_door1_action_topic && requestedAction == "CLOSE") {
    if (door1_sanityCheck("closed")) {
      Serial.print("Triggering ");
      Serial.print(door1_alias);
      Serial.println(" CLOSE relay!");
      toggleRelay(door1_closePin);
    }
    else {
      Serial.print("CLOSE requested but ");
      Serial.print(door1_alias);
      Serial.println(" is already closed. Publishing status update instead!");
      publish_door1_status();
    }
  }

  else if (requestedDoor == mqtt_door1_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand status update for ");
    Serial.print(door1_alias);
    Serial.println("!");
    publish_birth_message();
    publish_door1_status();
  }

  else if (requestedDoor == mqtt_door2_action_topic && requestedAction == "OPEN") {
    if (door2_sanityCheck("open")) {
      Serial.print("Triggering ");
      Serial.print(door2_alias);
      Serial.println(" OPEN relay!");
      toggleRelay(door2_openPin);  
    }
    else {
      Serial.print("OPEN requested but ");
      Serial.print(door2_alias);
      Serial.println(" is already open. Publishing status update instead!");
      publish_door2_status();
    }  
  }

  else if (requestedDoor == mqtt_door2_action_topic && requestedAction == "CLOSE") {
    if (door2_sanityCheck("closed")) {
      Serial.print("Triggering ");
      Serial.print(door2_alias);
      Serial.println(" CLOSE relay!");
      toggleRelay(door2_closePin);
    }
    else {
      Serial.print("CLOSE requested but ");
      Serial.print(door2_alias);
      Serial.println(" is already closed. Publishing status update instead!");
      publish_door2_status();
    }
  }

  else if (requestedDoor == mqtt_door2_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand status update for ");
    Serial.print(door2_alias);
    Serial.println("!");
    publish_birth_message();
    publish_door2_status();
  }  

  else if (requestedDoor == mqtt_aux_door1_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand status update for ");
    Serial.print(aux_door1_alias);
    Serial.println("!");
    publish_birth_message();
    publish_aux_door1_status();
  }

  else if (requestedDoor == mqtt_aux_door2_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand status update for ");
    Serial.print(aux_door2_alias);
    Serial.println("!");
    publish_birth_message();
    publish_aux_door2_status();
  }

  else if (requestedDoor == mqtt_dht_action_topic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand DHT Temperature and Humidity update!");
    dht_read_publish();
  }

  else if (requestedDoor == discoveryConfigTopic && requestedAction == "STATE") {
    Serial.print("Publishing on-demand MQTT Discovery payloads!")
    publish_ha_mqtt_discovery();
  }

  else { 
    Serial.println("Unrecognized action payload... taking no action!");
  }
}

// Function that runs in loop() to read and publish values from the DHT sensor

void dht_read_publish() {
  // Read values from sensor
  float hum = dht.readHumidity();
  float tempRaw = dht.readTemperature();

  // Check if there was an error reading values
  if (isnan(hum) || isnan(tempRaw)) {
    Serial.print("Failed to read from DHT sensor; will try again in ");
    Serial.print(dht_publish_interval_s);
    Serial.println(" seconds...");
    return;
  }

  boolean celsius = DHT_TEMPERATURE_CELSIUS;
  float temp;
  if (celsius) {
    temp = tempRaw;
  }
  else {
    temp = (tempRaw * 1.8 + 32);
  }

  char payload[4];

  // Publish the temperature payload via MQTT 
  dtostrf(temp, 4, 0, payload);
  Serial.print("Publishing DHT Temperature payload: ");
  Serial.print(payload);
  Serial.print(" to ");
  Serial.print(dhtTempTopic);
  Serial.println("...");
  client.publish(dhtTempTopic, payload, false);

  // Publish the humidity payload via MQTT
  dtostrf(hum, 4, 0, payload);
  Serial.print("Publishing DHT Humidity payload: ");
  Serial.print(payload);
  Serial.print(" to ");
  Serial.print(dhtHumTopic);
  Serial.println("...");
  client.publish(dhtHumTopic, payload, false);

}

void publish_ha_mqtt_discovery() {
  Serial.print("Publishing Home Assistant MQTT Discovery payloads...");
  publish_ha_mqtt_discovery_door1();
  if (door2_enabled) {
    publish_ha_mqtt_discovery_door2();
  }
  if (aux_door1_enabled) {
    publish_ha_mqtt_discovery_auxdoor1();
  }
  if (aux_door2_enabled) {
    publish_ha_mqtt_discovery_auxdoor2();
  }
  if (dht_enabled) {
    publish_ha_mqtt_discovery_temperature();
    publish_ha_mqtt_discovery_humidity();
  }
}

void publish_ha_mqtt_discovery_door1() {
  const size_t bufferSize = JSON_OBJECT_SIZE(13);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = door1_alias;
  root["command_topic"] = mqtt_door1_action_topic;
  root["payload_open"] = "OPEN";
  root["payload_close"] = "CLOSE";
  root["payload_stop"] = "STATE";
  root["state_topic"] = mqtt_door1_status_topic;
  root["state_open"] = "open";
  root["state_closed"] = "closed";
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;
  root["retain"] = false;

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/door1/config";
  String topicStr = discoveryPrefix + "/cover/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(door1_alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

void publish_ha_mqtt_discovery_door2() {
  const size_t bufferSize = JSON_OBJECT_SIZE(13);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = door2_alias;
  root["command_topic"] = mqtt_door2_action_topic;
  root["payload_open"] = "OPEN";
  root["payload_close"] = "CLOSE";
  root["payload_stop"] = "STATE";
  root["state_topic"] = mqtt_door2_status_topic;
  root["state_open"] = "open";
  root["state_closed"] = "closed";
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;
  root["retain"] = false;

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/door2/config";
  String topicStr = discoveryPrefix + "/cover/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(door2_alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

void publish_ha_mqtt_discovery_auxdoor1() {
  const size_t bufferSize = JSON_OBJECT_SIZE(9);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = aux_door1_alias;
  root["state_topic"] = mqtt_aux_door1_status_topic;
  root["payload_on"] = "open";
  root["payload_off"] = "closed";
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;
  root["device_class"] = "door";

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/auxdoor1/config";
  String topicStr = discoveryPrefix + "/binary_sensor/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(aux_door1_alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

void publish_ha_mqtt_discovery_auxdoor2() {
  const size_t bufferSize = JSON_OBJECT_SIZE(9);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = aux_door2_alias;
  root["state_topic"] = mqtt_aux_door2_status_topic;
  root["payload_on"] = "open";
  root["payload_off"] = "closed";
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;
  root["device_class"] = "door";

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/auxdoor2/config";
  String topicStr = discoveryPrefix + "/binary_sensor/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(aux_door2_alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

void publish_ha_mqtt_discovery_temperature() {
  const size_t bufferSize = JSON_OBJECT_SIZE(7);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  boolean celsius = DHT_TEMPERATURE_CELSIUS;
  String uom = "";
  if (celsius) {
    uom = "°C";
  }
  else {
    uom = "°F";
  }

  String alias = DHT_TEMPERATURE_ALIAS;

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = alias;
  root["state_topic"] = dhtTempTopic;
  root["unit_of_measurement"] = uom;
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/temperature/config";
  String topicStr = discoveryPrefix + "/sensor/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

void publish_ha_mqtt_discovery_humidity() {
  const size_t bufferSize = JSON_OBJECT_SIZE(7);
  DynamicJsonBuffer jsonBuffer(bufferSize);

  String alias = DHT_HUMIDITY_ALIAS;

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = alias;
  root["state_topic"] = dhtHumTopic;
  root["unit_of_measurement"] = "%";
  root["availability_topic"] = availabilityTopic;
  root["payload_available"] = "online";
  root["payload_unavailable"] = "offline";
  root["qos"] = 0;

  // Prepare payload for publishing
  String payloadStr = "";
  root.printTo(payloadStr);
  const char* payload = payloadStr.c_str();

  // Prepare topic for publishing
  String clientID = MQTT_CLIENTID;
  String suffix = "/humidity/config";
  String topicStr = discoveryPrefix + "/sensor/" + clientID + suffix;
  const char* topic = topicStr.c_str();

  // Publish payload
  Serial.print("Publishing MQTT Discovery payload for ");
  Serial.print(alias);
  Serial.println("...");
  client.publish(topic, payload, false);

}

// Function that runs in loop() to connect/reconnect to the MQTT broker, and publish the current door statuses on connect

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_username, mqtt_password, availabilityTopic, 0, true, lwtMessage)) {
      Serial.println("Connected!");

      // Subscribe to MQTT discovery config topic to listen for STATE payload
      // Publish disovery payloads before other messages so that entities are created first
      if (discoveryEnabled) {
      Serial.print("Subscribing to ");
      Serial.print(discoveryConfigTopic);
      Serial.println("...");
      client.subscribe(discoveryConfigTopic);
      publish_ha_mqtt_discovery();
      }

      // Publish the birth message on connect/reconnect
      publish_birth_message();

      // Subscribe to the action topics to listen for action messages
      Serial.print("Subscribing to ");
      Serial.print(mqtt_door1_action_topic);
      Serial.println("...");
      client.subscribe(mqtt_door1_action_topic);
      
      if (door2_enabled) {
        Serial.print("Subscribing to ");
        Serial.print(mqtt_door2_action_topic);
        Serial.println("...");
        client.subscribe(mqtt_door2_action_topic);
      }

      if (aux_door1_enabled) {
        Serial.print("Subscribing to ");
        Serial.print(mqtt_aux_door1_action_topic);
        Serial.println("...");
        client.subscribe(mqtt_aux_door1_action_topic);
      }

      if (aux_door2_enabled) {
        Serial.print("Subscribing to ");
        Serial.print(mqtt_aux_door2_action_topic);
        Serial.println("...");
        client.subscribe(mqtt_aux_door2_action_topic);
      }
      
      // Publish the current door status on connect/reconnect to ensure status is synced with whatever happened while disconnected
      publish_door1_status();
      if (door2_enabled) { publish_door2_status();
      }
      if (aux_door1_enabled) {
        publish_aux_door1_status();
      }
      if (aux_door2_enabled) {
        publish_aux_door2_status();
      }

      // Publish the current temperature and humidity readings 
      if (dht_enabled) {
        Serial.print("Subscribing to ");
        Serial.print(mqtt_dht_action_topic);
        Serial.println("...");
        client.subscribe(mqtt_dht_action_topic);
        
        dht_read_publish();
        dht_lastReadTime = millis();
      }

    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // Setup the output and input pins used in the sketch
  // Set the lastStatusValue variables to the state of the status pins at setup time

  // Setup Door 1 pins
  pinMode(door1_openPin, OUTPUT);
  pinMode(door1_closePin, OUTPUT);
  // Set output pins LOW with an active-high relay
  if (activeHighRelay) {
    digitalWrite(door1_openPin, LOW);
    digitalWrite(door1_closePin, LOW);
  }
  // Set output pins HIGH with an active-low relay
  else {
    digitalWrite(door1_openPin, HIGH);
    digitalWrite(door1_closePin, HIGH);
  }
  // Set input pin to use internal pullup resistor
  pinMode(door1_statusPin, INPUT_PULLUP);
  // Update variable with current door state
  door1_lastStatusValue = digitalRead(door1_statusPin);

  // Setup Door 2 pins
  if (door2_enabled) {
    pinMode(door2_openPin, OUTPUT);
    pinMode(door2_closePin, OUTPUT);
    // Set output pins LOW with an active-high relay
    if (activeHighRelay) {
      digitalWrite(door2_openPin, LOW);
      digitalWrite(door2_closePin, LOW);
    }
    // Set output pins HIGH with an active-low relay
    else {
      digitalWrite(door2_openPin, HIGH);
      digitalWrite(door2_closePin, HIGH);
    }
    // Set input pin to use internal pullup resistor
    pinMode(door2_statusPin, INPUT_PULLUP);
    // Update variable with current door state
    door2_lastStatusValue = digitalRead(door2_statusPin);
  }

  //Setup Aux Door 1 pin
  if (aux_door1_enabled) {
    pinMode(aux_door1_statusPin, INPUT_PULLUP);
    aux_door1_lastStatusValue = digitalRead(aux_door1_statusPin);
  }

  //Setup Aux Door 2 pin
  if (aux_door2_enabled) {
    pinMode(aux_door2_statusPin, INPUT_PULLUP);
    aux_door2_lastStatusValue = digitalRead(aux_door2_statusPin);
  }
  

  // Setup serial output, connect to wifi, connect to MQTT broker, set MQTT message callback
  Serial.begin(115200);

  Serial.print("GarHAge version ");
  Serial.print(garhageVersion);
  Serial.println(" starting...");

  if (activeHighRelay) {
    Serial.println("Relay mode: Active-High");
  }
  else {
    Serial.println("Relay mode: Active-Low");
  }

  if (dht_enabled) {
    dht.begin();
    Serial.println("DHT sensor: Enabled");
  }
  else {
    Serial.println("DHT sensor: Disabled");
  }
  
  setup_wifi();
  setup_ota();
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);
}

void loop() {
  // Connect/reconnect to the MQTT broker and listen for messages
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  ArduinoOTA.handle();
  
  // Check door open/closed status each loop and publish changes
  check_door1_status();
  if (door2_enabled) { check_door2_status(); 
  }
  if (aux_door1_enabled) {
    check_aux_door1_status();
  }
  if (aux_door2_enabled) {
    check_aux_door2_status();
  }

  // Run DHT function to read/publish if enabled and interval is OK
  if (dht_enabled) {
    unsigned long currentTime = millis();
    if (currentTime - dht_lastReadTime > (dht_publish_interval_s * 1000)) {
      dht_read_publish();
      dht_lastReadTime = millis();
    }
  }
  
}
