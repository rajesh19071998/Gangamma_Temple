#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include "wifi_config.h"
#include "lights.h"
#include "ota_update.h"
#include "settings.h"
#include "settings_page.h"
#include "uart_link.h"
#include "web_page.h"
#include "temple_mqtt.h"

static ESP8266WebServer server(80);

static String lightsJson()
{
  String json = "{\"lights\":[";
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += Lights_Get(i) ? "true" : "false";
  }
  json += "]}";
  return json;
}

static void handleRoot()
{
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleGetLights()
{
  server.send(200, "application/json", lightsJson());
}

static bool statusFlag(const String &reply, const char *key)
{
  int i = reply.indexOf(key);
  return i >= 0 && reply.substring(i + strlen(key), i + strlen(key) + 2) == "ON";
}

constexpr uint32_t STATUS_POLL_INTERVAL_MS = 120000UL;
constexpr uint32_t PAGE_ACTIVE_MS = 10000UL;  // the page asks for /api/status every 3 s while open

struct StatusCache {
  bool valid;
  bool online;
  bool lights[4];
  bool switches[4];
};

static StatusCache statusCache = {false, false, {false, false, false, false}, {false, false, false, false}};
static uint32_t lastStatusPoll = 0;
static uint32_t lastPageSeen = 0;
static bool pageSeen = false;
static bool statusPollPending = false;
static uint32_t statusPollAt = 0;

static bool pageActive()
{
  return pageSeen && millis() - lastPageSeen < PAGE_ACTIVE_MS;
}

// Store a "STATUS:L1=ON ... S4=OFF" line in the cache.
static void applyStatusReply(const String &reply)
{
  const char *lightKeys[4] = {"L1=", "L2=", "L3=", "L4="};
  const char *switchKeys[4] = {"S1=", "S2=", "S3=", "S4="};
  for (uint8_t i = 0; i < 4; i++) {
    statusCache.lights[i] = statusFlag(reply, lightKeys[i]);
    statusCache.switches[i] = statusFlag(reply, switchKeys[i]);
  }
  statusCache.valid = true;
  statusCache.online = true;
}

static void pollControllerStatus()
{
  String reply;
  lastStatusPoll = millis();
  if (!UartLink_Request("ALLSTATUS", "STATUS:", reply)) {
    statusCache.online = false;
    return;
  }
  applyStatusReply(reply);
}

// Pick up STATUS lines the CH32 sends on its own, e.g. after a physical switch change.
static void handleUnsolicitedUart()
{
  String line;
  while (UartLink_ReadLine(line)) {
    if (line.startsWith("STATUS:")) {
      applyStatusReply(line);
      lastStatusPoll = millis();
      statusPollPending = false;
    }
  }
}

// Served from the cache; the CH32 is queried on first page open, then every 2 minutes.
static void handleGetStatus()
{
  if (!pageActive()) {
    pollControllerStatus();
  }
  pageSeen = true;
  lastPageSeen = millis();

  if (!statusCache.online) {
    String json = lightsJson();
    json.remove(json.length() - 1);
    json += ",\"online\":false,\"switches\":null}";
    server.send(200, "application/json", json);
    return;
  }

  char buf[128];
  snprintf(buf, sizeof(buf),
           "{\"online\":true,\"lights\":[%s,%s,%s,%s],\"switches\":[%s,%s,%s,%s]}",
           statusCache.lights[0] ? "true" : "false", statusCache.lights[1] ? "true" : "false",
           statusCache.lights[2] ? "true" : "false", statusCache.lights[3] ? "true" : "false",
           statusCache.switches[0] ? "true" : "false", statusCache.switches[1] ? "true" : "false",
           statusCache.switches[2] ? "true" : "false", statusCache.switches[3] ? "true" : "false");
  server.send(200, "application/json", buf);
}

static void sendError(int code, const char *message)
{
  server.send(code, "application/json", String("{\"error\":\"") + message + "\"}");
}

// Convert "ALARM:ENABLED=ON ON=18:00 OFF=06:00" into JSON.
static bool alarmReplyToJson(const String &reply, String &json)
{
  char enabled[4];
  int onH, onM, offH, offM;
  if (sscanf(reply.c_str(), "ALARM:ENABLED=%3s ON=%d:%d OFF=%d:%d", enabled, &onH, &onM, &offH, &offM) != 5) {
    return false;
  }

  char buf[80];
  snprintf(buf, sizeof(buf), "{\"enabled\":%s,\"on\":\"%02d:%02d\",\"off\":\"%02d:%02d\"}",
           strcmp(enabled, "ON") == 0 ? "true" : "false", onH, onM, offH, offM);
  json = buf;
  return true;
}

static void handleGetAlarm()
{
  String reply, json;
  if (!UartLink_Request("GETALARM", "ALARM:", reply) || !alarmReplyToJson(reply, json)) {
    sendError(504, "no response from controller");
    return;
  }
  server.send(200, "application/json", json);
}

static void handleSetAlarm()
{
  int onH, onM, offH, offM;
  if (!server.hasArg("on") || !server.hasArg("off") || !server.hasArg("enabled") ||
      sscanf(server.arg("on").c_str(), "%d:%d", &onH, &onM) != 2 ||
      sscanf(server.arg("off").c_str(), "%d:%d", &offH, &offM) != 2 ||
      onH < 0 || onH > 23 || offH < 0 || offH > 23 ||
      onM < 0 || onM > 59 || offM < 0 || offM > 59) {
    sendError(400, "invalid alarm times");
    return;
  }

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "SETALARM=%02d:%02d,%02d:%02d", onH, onM, offH, offM);

  String reply, json;
  if (!UartLink_Request(cmd, "ALARM:", reply) ||
      !UartLink_Request(server.arg("enabled").toInt() ? "ALARMON" : "ALARMOFF", "ALARM:", reply) ||
      !alarmReplyToJson(reply, json)) {
    sendError(504, "no response from controller");
    return;
  }
  server.send(200, "application/json", json);
}

static void handleSetTime()
{
  int year, month, date, hour, minute, second;
  if (!server.hasArg("value") ||
      sscanf(server.arg("value").c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &date, &hour, &minute, &second) != 6 ||
      year < 2000 || year > 2099 || month < 1 || month > 12 || date < 1 || date > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    sendError(400, "invalid date/time");
    return;
  }

  char cmd[40];
  snprintf(cmd, sizeof(cmd), "SETTIME=%04d-%02d-%02d %02d:%02d:%02d", year, month, date, hour, minute, second);

  String reply;
  if (!UartLink_Request(cmd, "RTC:", reply)) {
    sendError(504, "no response from controller");
    return;
  }
  server.send(200, "application/json", String("{\"rtc\":\"") + reply.substring(4) + "\"}");
}

static void handleSetLight()
{
  if (!server.hasArg("id") || !server.hasArg("state")) {
    server.send(400, "application/json", "{\"error\":\"id and state required\"}");
    return;
  }

  int id = server.arg("id").toInt();
  int state = server.arg("state").toInt();
  if (id < 1 || id > LIGHT_COUNT || (state != 0 && state != 1)) {
    server.send(400, "application/json", "{\"error\":\"invalid id or state\"}");
    return;
  }

  Lights_Set((uint8_t)(id - 1), state == 1);
  statusCache.lights[id - 1] = state == 1;  // keep the cached view consistent until the confirming poll
  statusPollPending = true;
  statusPollAt = millis() + 1500;
  Serial.printf("L%d%s\n", id, state == 1 ? "ON" : "OFF");
  server.send(200, "application/json", lightsJson());
}

static uint32_t rebootAt = 0;
static uint32_t lastMqttStatusAt = 0;

static bool requireAuth()
{
  if (server.authenticate(settings.otaUser, settings.otaPass)) {
    return true;
  }
  server.requestAuthentication();
  return false;
}

static String jsonEscape(const char *text)
{
  String out;
  for (; *text; text++) {
    if (*text == '"' || *text == '\\') {
      out += '\\';
    }
    out += *text;
  }
  return out;
}

static void handleSettingsPage()
{
  if (requireAuth()) {
    server.send_P(200, "text/html", SETTINGS_HTML);
  }
}

// Passwords are never sent back to the browser.
static void handleGetSettings()
{
  if (!requireAuth()) {
    return;
  }
  String json = String("{\"wifiSsid\":\"") + jsonEscape(settings.wifiSsid) +
                "\",\"apSsid\":\"" + jsonEscape(settings.apSsid) +
                "\",\"otaUser\":\"" + jsonEscape(settings.otaUser) + "\"}";
  server.send(200, "application/json", json);
}

static bool lengthBetween(const String &value, size_t minLen, size_t maxLen)
{
  return value.length() >= minLen && value.length() <= maxLen;
}

static void handleSetSettings()
{
  if (!requireAuth()) {
    return;
  }

  String wifiSsid = server.arg("wifiSsid");
  String wifiPass = server.arg("wifiPass");
  String apSsid = server.arg("apSsid");
  String apPass = server.arg("apPass");
  String otaUser = server.arg("otaUser");
  String otaPass = server.arg("otaPass");

  // Blank password fields keep the stored value.
  if (!lengthBetween(wifiSsid, 1, 32) || !lengthBetween(apSsid, 1, 32) || !lengthBetween(otaUser, 1, 31)) {
    sendError(400, "SSIDs and username are required (SSID max 32, username max 31)");
    return;
  }
  if (wifiPass.length() > 0 && !lengthBetween(wifiPass, 8, 63)) {
    sendError(400, "Wi-Fi password must be 8-63 characters");
    return;
  }
  if (apPass.length() > 0 && !lengthBetween(apPass, 8, 63)) {
    sendError(400, "Hotspot password must be 8-63 characters");
    return;
  }
  if (otaPass.length() > 0 && !lengthBetween(otaPass, 8, 31)) {
    sendError(400, "Admin password must be 8-31 characters");
    return;
  }

  Settings_Copy(settings.wifiSsid, sizeof(settings.wifiSsid), wifiSsid.c_str());
  Settings_Copy(settings.apSsid, sizeof(settings.apSsid), apSsid.c_str());
  Settings_Copy(settings.otaUser, sizeof(settings.otaUser), otaUser.c_str());
  if (wifiPass.length() > 0) {
    Settings_Copy(settings.wifiPass, sizeof(settings.wifiPass), wifiPass.c_str());
  }
  if (apPass.length() > 0) {
    Settings_Copy(settings.apPass, sizeof(settings.apPass), apPass.c_str());
  }
  if (otaPass.length() > 0) {
    Settings_Copy(settings.otaPass, sizeof(settings.otaPass), otaPass.c_str());
  }

  if (!Settings_Save()) {
    sendError(500, "could not write settings");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
  rebootAt = millis() + 1500;
}

static bool hotspotActive = false;
static uint32_t lastWifiCheck = 0;

// Keep the hotspot up while retrying so connected phones are not dropped.
static bool connectStation()
{
  WiFi.mode(hotspotActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(settings.wifiSsid, settings.wifiPass);
  Serial.print("Connecting to WiFi");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  return WiFi.status() == WL_CONNECTED;
}

static void startHotspot()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(settings.apSsid, settings.apPass);
  hotspotActive = true;
  Serial.print("\nWiFi failed. Hotspot \"");
  Serial.print(settings.apSsid);
  Serial.print("\" started, open http://");
  Serial.println(WiFi.softAPIP());
}

static void connectOrHotspot()
{
  if (connectStation()) {
    if (hotspotActive) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      hotspotActive = false;
    }
    Serial.print("\nOpen http://");
    Serial.println(WiFi.localIP());
  } else if (!hotspotActive) {
    startHotspot();
  } else {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
  }
}

void setup()
{
  Serial.begin(115200);
  Settings_Load();
  Lights_Init();

  connectOrHotspot();
  TempleMqtt::begin("0.tcp.in.ngrok.io", 26350);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/lights", HTTP_GET, handleGetLights);
  server.on("/api/status", HTTP_GET, handleGetStatus);
  server.on("/api/light", HTTP_POST, handleSetLight);
  server.on("/api/alarm", HTTP_GET, handleGetAlarm);
  server.on("/api/alarm", HTTP_POST, handleSetAlarm);
  server.on("/api/time", HTTP_POST, handleSetTime);
  Ota_Init(server, settings.otaUser, settings.otaPass);
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  server.begin();
}

void loop()
{
  TempleMqtt::loop();
  if (TempleMqtt::connected() && (millis() - lastMqttStatusAt >= 5000UL)) {
    lastMqttStatusAt = millis();
    TempleMqtt::publishStatusSnapshot(statusCache.lights, statusCache.switches, true);
  }

  server.handleClient();
  handleUnsolicitedUart();

  if (pageActive()) {
    bool confirmDue = statusPollPending && (int32_t)(millis() - statusPollAt) >= 0;
    if (confirmDue || millis() - lastStatusPoll >= STATUS_POLL_INTERVAL_MS) {
      statusPollPending = false;
      pollControllerStatus();
    }
  }

  if (rebootAt != 0 && (int32_t)(millis() - rebootAt) >= 0) {
    ESP.restart();
  }

  if (millis() - lastWifiCheck >= WIFI_CHECK_INTERVAL_MS) {
    lastWifiCheck = millis();
    if (hotspotActive || WiFi.status() != WL_CONNECTED) {
      connectOrHotspot();
    }
  }
}
