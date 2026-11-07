#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "lights.h"

namespace TempleMqtt {

constexpr const char *TOPIC_LIGHTS = "temple/lights";
static WiFiClient wifiClient;
static PubSubClient client(wifiClient);

static const char *brokerHost = "0.tcp.in.ngrok.io";
static uint16_t brokerPort = 26350;
static const char *brokerUser = "";
static const char *brokerPassword = "";
static String clientId = "temple-esp12e";
static String lastPublishedState = "";

static bool connected() {
  return client.connected();
}

static void publishStatusSnapshot(const bool *lights, const bool *switches, bool force = false) {
  if (!client.connected()) {
    return;
  }

  String payload = "{\"lights\":[";
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    if (i > 0) {
      payload += ',';
    }
    payload += lights[i] ? "true" : "false";
  }
  payload += "],\"switches\":[";
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    if (i > 0) {
      payload += ',';
    }
    payload += switches[i] ? "true" : "false";
  }
  payload += "]}";

  if (!force && payload == lastPublishedState) {
    return;
  }

  lastPublishedState = payload;
  client.publish(TOPIC_LIGHTS, payload.c_str());
  Serial.printf("MQTT publish [%s]: %s\n", TOPIC_LIGHTS, payload.c_str());
}

static void publishLightsState() {
  bool lights[LIGHT_COUNT];
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    lights[i] = Lights_Get(i);
  }
  bool switches[LIGHT_COUNT] = {false, false, false, false};
  publishStatusSnapshot(lights, switches);
}

static void applyPayloadToLights(const String &payload) {
  String msg = payload;
  msg.trim();

  if (msg.length() == 0) {
    return;
  }

  DynamicJsonDocument doc(128);
  DeserializationError error = deserializeJson(doc, msg);

  if (!error && doc.is<JsonObject>()) {
    if (doc.containsKey("lights") && doc["lights"].is<JsonArray>()) {
      JsonArray lights = doc["lights"];
      for (uint8_t i = 0; i < LIGHT_COUNT && i < lights.size(); i++) {
        Lights_Set(i, lights[i].as<bool>());
      }
      publishLightsState();
      return;
    }

    if (doc.containsKey("light") && doc["light"].is<int>()) {
      const int index = doc["light"].as<int>();
      const bool state = doc.containsKey("state") ? doc["state"].as<bool>() : false;
      if (index >= 0 && index < LIGHT_COUNT) {
        Lights_Set(index, state);
      }
      publishLightsState();
      return;
    }

    if (doc.containsKey("state") && doc["state"].is<bool>()) {
      for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
        Lights_Set(i, doc["state"].as<bool>());
      }
      publishLightsState();
      return;
    }
  }

  if (msg.equalsIgnoreCase("ON") || msg.equalsIgnoreCase("1") || msg.equalsIgnoreCase("true")) {
    for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
      Lights_Set(i, true);
    }
    publishLightsState();
    return;
  }

  if (msg.equalsIgnoreCase("OFF") || msg.equalsIgnoreCase("0") || msg.equalsIgnoreCase("false")) {
    for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
      Lights_Set(i, false);
    }
    publishLightsState();
  }
}

static void mqttCallback(char *topic, byte *payload, unsigned int length) {
  if (String(topic) != TOPIC_LIGHTS) {
    return;
  }

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (message == lastPublishedState) {
    return;
  }

  Serial.printf("MQTT [%s]: %s\n", topic, message.c_str());
  applyPayloadToLights(message);
}

static void begin(const char *host, uint16_t port, const char *user = "", const char *password = "") {
  brokerHost = host;
  brokerPort = port;
  brokerUser = user;
  brokerPassword = password;
  client.setServer(brokerHost, brokerPort);
  client.setCallback(mqttCallback);
}

static bool reconnect() {
  if (client.connected()) {
    return true;
  }

  Serial.printf("Connecting to MQTT broker %s:%d...\n", brokerHost, brokerPort);

  bool connected = false;
  if (strlen(brokerUser) > 0 || strlen(brokerPassword) > 0) {
    connected = client.connect(clientId.c_str(), brokerUser, brokerPassword);
  } else {
    connected = client.connect(clientId.c_str());
  }

  if (!connected) {
    Serial.printf("MQTT connection failed, rc=%d\n", client.state());
    return false;
  }

  Serial.println("MQTT broker connected successfully.");
  client.subscribe(TOPIC_LIGHTS);
  publishLightsState();
  return true;
}

static void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!client.connected()) {
    reconnect();
  }

  if (client.connected()) {
    client.loop();
  }
}

}  // namespace TempleMqtt
