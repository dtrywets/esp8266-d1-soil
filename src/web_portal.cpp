#include "web_portal.h"

#include "dashboard_html.h"
#include "event_log.h"
#include "improv_wifi.h"
#include "firmware_version.h"
#include "network_config.h"
#include "ota_update.h"
#include "platform_io.h"
#include "project_config.h"
#include "soil_sensor.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#if defined(ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "Bodenfeuchte 1"
#endif

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "D1Soil1"
#endif

static constexpr uint16_t kWebPort = 80;
static constexpr uint32_t kWifiConnectTimeoutMs = 15000;
static constexpr uint32_t kWifiRetryMs = 5000;

static PlatformWebServer server(kWebPort);
static DNSServer dnsServer;
static NetworkSettings networkSettings;
static NetworkSettings compileDefaults;
static String apSsid;
static bool apMode = false;
static bool webStarted = false;
static bool mqttConnectedFlag = false;
static bool mqttReconnectRequested = false;
static bool mqttRepublishRequested = false;
static uint32_t wifiAttemptStartedMs = 0;
static uint32_t lastWifiRetryMs = 0;
static uint32_t bootMs = 0;
static bool wifiConnectedLogged = false;
static bool mdnsStarted = false;

static void beginWifiStation() {
  platformWifiBeginStation();
  platformWifiNoSleep();
  platformSetHostname(WIFI_HOSTNAME);
  WiFi.begin(networkSettings.wifiSsid.c_str(),
             networkSettings.wifiPassword.c_str());
}

static void logWifiConnected() {
  if (wifiConnectedLogged) {
    return;
  }
  wifiConnectedLogged = true;

  Serial.printf("WiFi connected as %s\n", WIFI_HOSTNAME);
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Dashboard: http://%s/\n", WiFi.localIP().toString().c_str());
  logSysf("WLAN verbunden: %s -> %s", WIFI_HOSTNAME,
          WiFi.localIP().toString().c_str());
  logSysf("Dashboard: http://%s/  |  http://%s.local/",
          WiFi.localIP().toString().c_str(), WIFI_HOSTNAME);

  if (!mdnsStarted && MDNS.begin(WIFI_HOSTNAME)) {
    MDNS.addService("http", "tcp", kWebPort);
    mdnsStarted = true;
    logSysf("mDNS aktiv: http://%s.local/", WIFI_HOSTNAME);
  }

  otaUpdateOnWifiConnected();
}

static bool isCompileDefaultSsid(const String &ssid) {
  return ssid.isEmpty() || ssid == "your-wifi";
}

static String apNameFromMac() {
  const uint32_t chipId = platformChipIdSuffix();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", chipId & 0xFFFF);
  return String(AP_SSID_PREFIX) + suffix;
}

static void startApMode() {
  if (apMode) {
    return;
  }

  apSsid = apNameFromMac();
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid.c_str());
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("AP mode started: %s -> %s\n", apSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
  logSysf("AP-Modus: %s -> %s", apSsid.c_str(),
          WiFi.softAPIP().toString().c_str());
}

static void stopApMode() {
  if (!apMode) {
    return;
  }
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  apMode = false;
  logSys("AP-Modus beendet");
}

static void sendJson(int code, const JsonDocument &doc) {
  String payload;
  serializeJson(doc, payload);
  server.send(code, "application/json", payload);
}

static void sendError(int code, const char *message) {
  logWebf("Fehler %d: %s", code, message);
  JsonDocument doc;
  doc["error"] = message;
  sendJson(code, doc);
}

static void applyCalibrationAndNotify(const SoilCalibration &cal) {
  soilSensorSetCalibration(cal);
  mqttRepublishRequested = true;
}

static void handleRoot() {
  logWeb("Dashboard geöffnet (GET /)");
  server.send_P(200, "text/html; charset=utf-8", kDashboardHtml);
}

static void handleStatus() {
  const SoilReading reading = soilSensorLastReading();
  const SoilCalibration cal = soilSensorCalibration();
  const uint32_t lastMs = soilSensorLastMeasureMs();

  JsonDocument doc;
  doc["device_name"] = DEVICE_NAME;
  doc["firmware_version"] = FIRMWARE_VERSION_LABEL;
  doc["firmware_version_date"] = FIRMWARE_VERSION_DATE;
  doc["firmware_version_number"] = FIRMWARE_VERSION;
  doc["ap_mode"] = apMode;
  doc["ap_ssid"] = apSsid;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt_connected"] = mqttConnectedFlag;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString()
                                            : WiFi.softAPIP().toString();
  doc["hostname"] = WIFI_HOSTNAME;
  doc["mdns"] = String(WIFI_HOSTNAME) + ".local";
  doc["uptime_sec"] = (millis() - bootMs) / 1000UL;
  doc["rssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["arduino_ota_active"] = otaUpdateIsActive();
  doc["arduino_ota_port"] = otaUpdatePort();
  doc["chip_model"] = platformChipModel();
  doc["mac"] = WiFi.macAddress();
  doc["moisture_percent"] = reading.valid ? soilSensorSmoothedPercent() : 0.0f;
  doc["moisture_raw"] = reading.rawAdc;
  doc["cal_dry_adc"] = cal.dryAdc;
  doc["cal_wet_adc"] = cal.wetAdc;
  doc["sensor_label"] = cal.label;
  doc["last_measure_sec_ago"] =
      lastMs > 0 ? static_cast<uint32_t>((millis() - lastMs) / 1000UL) : 0;
  sendJson(200, doc);
}

static void handleSoilGet() {
  const SoilReading reading = soilSensorLastReading();
  const SoilCalibration cal = soilSensorCalibration();

  JsonDocument doc;
  doc["raw_adc"] = reading.rawAdc;
  doc["moisture_percent"] = reading.valid ? reading.moisturePercent : 0.0f;
  doc["moisture_smoothed"] = reading.valid ? soilSensorSmoothedPercent() : 0.0f;
  doc["valid"] = reading.valid;
  doc["dry_adc"] = cal.dryAdc;
  doc["wet_adc"] = cal.wetAdc;
  doc["label"] = cal.label;
  doc["last_measure_ms"] = soilSensorLastMeasureMs();
  sendJson(200, doc);
}

static void handleSoilMeasure() {
  logWeb("Sofortmessung angefordert");
  soilSensorRequestMeasure();
  JsonDocument doc;
  doc["status"] = "measuring";
  sendJson(200, doc);
}

static void handleSoilCalibration() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendError(400, "Invalid JSON");
    return;
  }

  SoilCalibration cal = soilSensorCalibration();
  if (doc["dry_adc"].is<int>()) {
    cal.dryAdc = static_cast<uint16_t>(doc["dry_adc"].as<int>());
  }
  if (doc["wet_adc"].is<int>()) {
    cal.wetAdc = static_cast<uint16_t>(doc["wet_adc"].as<int>());
  }
  if (doc["label"].is<const char *>()) {
    cal.label = doc["label"].as<String>();
  }

  if (cal.dryAdc <= cal.wetAdc) {
    sendError(400, "dry_adc muss größer als wet_adc sein");
    return;
  }

  logWebf("Kalibrierung gespeichert: trocken=%u, nass=%u, Label=%s", cal.dryAdc,
          cal.wetAdc, cal.label.c_str());
  applyCalibrationAndNotify(cal);
  sendJson(200, doc);
}

static void handleCalibrateDry() {
  const SoilReading reading = soilSensorLastReading();
  if (!reading.valid) {
    soilSensorRequestMeasure();
    sendError(409, "Keine Messung — bitte erneut versuchen");
    return;
  }

  SoilCalibration cal = soilSensorCalibration();
  cal.dryAdc = reading.rawAdc;
  if (cal.dryAdc <= cal.wetAdc) {
    sendError(400, "Trockenwert muss größer als Nasswert sein");
    return;
  }

  logWebf("Trocken kalibriert: ADC=%u", cal.dryAdc);
  applyCalibrationAndNotify(cal);
  JsonDocument doc;
  doc["dry_adc"] = cal.dryAdc;
  sendJson(200, doc);
}

static void handleCalibrateWet() {
  const SoilReading reading = soilSensorLastReading();
  if (!reading.valid) {
    soilSensorRequestMeasure();
    sendError(409, "Keine Messung — bitte erneut versuchen");
    return;
  }

  SoilCalibration cal = soilSensorCalibration();
  cal.wetAdc = reading.rawAdc;
  if (cal.dryAdc <= cal.wetAdc) {
    sendError(400, "Nasswert muss kleiner als Trockenwert sein");
    return;
  }

  logWebf("Nass kalibriert: ADC=%u", cal.wetAdc);
  applyCalibrationAndNotify(cal);
  JsonDocument doc;
  doc["wet_adc"] = cal.wetAdc;
  sendJson(200, doc);
}

static void handleGetConfig() {
  JsonDocument doc;
  doc["wifi_ssid"] = networkSettings.wifiSsid;
  doc["wifi_password_set"] = !networkSettings.wifiPassword.isEmpty();
  doc["mqtt_host"] = networkSettings.mqttHost;
  doc["mqtt_port"] = networkSettings.mqttPort;
  doc["mqtt_user"] = networkSettings.mqttUser;
  doc["mqtt_password_set"] = !networkSettings.mqttPassword.isEmpty();
  sendJson(200, doc);
}

static void handlePostConfig() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    sendError(400, "Invalid JSON");
    return;
  }

  NetworkSettings updated = networkSettings;
  if (doc["wifi_ssid"].is<const char *>()) {
    updated.wifiSsid = doc["wifi_ssid"].as<String>();
  }
  if (doc["wifi_password"].is<const char *>()) {
    const String password = doc["wifi_password"].as<String>();
    if (!password.isEmpty()) {
      updated.wifiPassword = password;
    }
  }
  if (doc["mqtt_host"].is<const char *>()) {
    updated.mqttHost = doc["mqtt_host"].as<String>();
  }
  if (doc["mqtt_port"].is<int>()) {
    updated.mqttPort = static_cast<uint16_t>(doc["mqtt_port"].as<int>());
  }
  if (doc["mqtt_user"].is<const char *>()) {
    updated.mqttUser = doc["mqtt_user"].as<String>();
  }
  if (doc["mqtt_password"].is<const char *>()) {
    const String password = doc["mqtt_password"].as<String>();
    if (!password.isEmpty()) {
      updated.mqttPassword = password;
    }
  }

  if (updated.wifiSsid.isEmpty()) {
    sendError(400, "SSID required");
    return;
  }

  networkSettings = updated;
  networkConfigSave(networkSettings);
  logWebf("Netzwerk gespeichert: WLAN=%s, MQTT=%s:%u", updated.wifiSsid.c_str(),
          updated.mqttHost.c_str(), updated.mqttPort);
  JsonDocument ok;
  ok["status"] = "saved";
  sendJson(200, ok);
  delay(250);
  ESP.restart();
}

static void handleWifiScan() {
  logWeb("WLAN-Scan gestartet");
  const int count = WiFi.scanNetworks();
  logWebf("WLAN-Scan: %d Netzwerke gefunden", count);
  JsonDocument doc;
  JsonArray networks = doc.to<JsonArray>();
  for (int i = 0; i < count; ++i) {
    JsonObject net = networks.add<JsonObject>();
    net["ssid"] = WiFi.SSID(i);
    net["rssi"] = WiFi.RSSI(i);
    net["secure"] = !platformWifiScanIsOpen(WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  sendJson(200, doc);
}

static void handleRestart() {
  logWeb("Neustart angefordert");
  server.send(200, "application/json", "{\"status\":\"restarting\"}");
  delay(250);
  ESP.restart();
}

static bool firmwareUploadFailed = false;

static void handleFirmwareUpload() {
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    firmwareUploadFailed = false;
    logWebf("Firmware-Upload: %s (%u Bytes erwartet)",
            upload.filename.c_str(), upload.totalSize);
    const uint32_t maxSketchSpace =
        (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      firmwareUploadFailed = true;
      Update.printError(Serial);
      logWeb("Firmware-Update: Update.begin fehlgeschlagen");
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (firmwareUploadFailed) {
      return;
    }
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      firmwareUploadFailed = true;
      Update.printError(Serial);
      logWeb("Firmware-Update: Schreibfehler");
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (firmwareUploadFailed || !Update.end(true)) {
      firmwareUploadFailed = true;
      Update.printError(Serial);
      logWeb("Firmware-Update fehlgeschlagen");
      return;
    }
    logWebf("Firmware-Update erfolgreich (%u Bytes)", upload.totalSize);
  }
}

static void handleFirmwareComplete() {
  if (firmwareUploadFailed || Update.hasError()) {
    sendError(500, "Firmware update failed");
    return;
  }

  JsonDocument doc;
  doc["ok"] = true;
  doc["restarting"] = true;
  sendJson(200, doc);
  delay(500);
  ESP.restart();
}

static void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

static void setupWebRoutes() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/soil", HTTP_GET, handleSoilGet);
  server.on("/api/soil/measure", HTTP_POST, handleSoilMeasure);
  server.on("/api/soil/calibration", HTTP_POST, handleSoilCalibration);
  server.on("/api/soil/calibrate/dry", HTTP_POST, handleCalibrateDry);
  server.on("/api/soil/calibrate/wet", HTTP_POST, handleCalibrateWet);
  server.on("/api/firmware", HTTP_POST, handleFirmwareComplete, handleFirmwareUpload);

  server.onNotFound([]() {
    if (apMode && server.method() == HTTP_GET) {
      handleCaptivePortal();
      return;
    }
    sendError(404, "Not found");
  });
}

static void startWebServer() {
  if (webStarted) {
    return;
  }
  setupWebRoutes();
  server.begin();
  webStarted = true;
  logSys("Web-Dashboard gestartet auf Port 80");
}

static void tryConnectWifi() {
  if (improvWifiShouldDeferNetwork()) {
    return;
  }

  if (networkSettings.wifiSsid.isEmpty()) {
    startApMode();
    return;
  }

  const uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (apMode) {
      stopApMode();
      WiFi.mode(WIFI_STA);
    }
    logWifiConnected();
    return;
  }

  wifiConnectedLogged = false;

  if (wifiAttemptStartedMs == 0) {
    wifiAttemptStartedMs = now;
    lastWifiRetryMs = 0;
    Serial.printf("Connecting WiFi to %s (hostname %s)\n",
                  networkSettings.wifiSsid.c_str(), WIFI_HOSTNAME);
    logSysf("WLAN-Verbindung zu %s (Hostname %s)",
            networkSettings.wifiSsid.c_str(), WIFI_HOSTNAME);
    beginWifiStation();
    return;
  }

  if (now - wifiAttemptStartedMs > kWifiConnectTimeoutMs) {
    Serial.println("WiFi connect timeout, starting AP mode");
    logSys("WLAN-Timeout, starte AP-Modus");
    startApMode();
    return;
  }

  if (now - lastWifiRetryMs >= kWifiRetryMs) {
    lastWifiRetryMs = now;
    Serial.println("Retrying WiFi connection");
    logSys("WLAN-Verbindung wird erneut versucht");
    WiFi.disconnect();
    beginWifiStation();
  }
}

bool webPortalIsApMode() { return apMode; }

bool webPortalIsWifiConnected() { return WiFi.status() == WL_CONNECTED; }

const NetworkSettings &webPortalNetworkSettings() { return networkSettings; }

void webPortalBegin(const NetworkSettings &defaults) {
  bootMs = millis();
  compileDefaults = defaults;
  networkConfigLoad(networkSettings, defaults);

  const bool hasStoredWifi = networkConfigHasStoredWifi();
  const bool hasCompileWifi = !isCompileDefaultSsid(defaults.wifiSsid);

  if (!hasStoredWifi && !hasCompileWifi) {
    if (!improvWifiShouldDeferNetwork()) {
      startApMode();
    }
  } else if (!hasStoredWifi && hasCompileWifi) {
    networkSettings.wifiSsid = defaults.wifiSsid;
    networkSettings.wifiPassword = defaults.wifiPassword;
    tryConnectWifi();
  } else {
    tryConnectWifi();
  }

  if (!improvWifiShouldDeferNetwork()) {
    startWebServer();
  }
}

static void webPortalEnsureStarted() {
  if (improvWifiShouldDeferNetwork()) {
    return;
  }

  const bool hasStoredWifi = networkConfigHasStoredWifi();
  const bool hasCompileWifi = !isCompileDefaultSsid(compileDefaults.wifiSsid);

  if (!hasStoredWifi && !hasCompileWifi && !apMode &&
      networkSettings.wifiSsid.isEmpty()) {
    startApMode();
  }

  startWebServer();
}

void webPortalLoop() {
  webPortalEnsureStarted();

  if (improvWifiShouldDeferNetwork()) {
    return;
  }

  if (apMode) {
    dnsServer.processNextRequest();
  }

  server.handleClient();
  tryConnectWifi();
}

void webPortalRequestMqttReconnect() { mqttReconnectRequested = true; }

void webPortalRequestMqttRepublish() { mqttRepublishRequested = true; }

bool webPortalConsumeMqttRepublishRequest() {
  if (!mqttRepublishRequested) {
    return false;
  }
  mqttRepublishRequested = false;
  return true;
}

void webPortalSetMqttConnected(bool connected) { mqttConnectedFlag = connected; }

bool webPortalConnectMqtt(PubSubClient &mqtt, const char *deviceId,
                          const char *statusTopic) {
  static uint32_t lastAttemptMs = 0;

  if (WiFi.status() != WL_CONNECTED) {
    mqttConnectedFlag = false;
    return false;
  }

  if (mqtt.connected()) {
    mqttConnectedFlag = true;
    mqttReconnectRequested = false;
    return true;
  }

  const uint32_t now = millis();
  if (!mqttReconnectRequested && now - lastAttemptMs < 5000) {
    return false;
  }
  lastAttemptMs = now;

  mqtt.setServer(networkSettings.mqttHost.c_str(), networkSettings.mqttPort);

  bool connected = false;
  if (!networkSettings.mqttUser.isEmpty()) {
    connected = mqtt.connect(deviceId, networkSettings.mqttUser.c_str(),
                             networkSettings.mqttPassword.c_str(), statusTopic,
                             1, true, "offline");
  } else {
    connected = mqtt.connect(deviceId, statusTopic, 1, true, "offline");
  }

  mqttConnectedFlag = connected;
  if (!connected) {
    logMqttf("Verbindung fehlgeschlagen, rc=%d", mqtt.state());
  } else {
    logMqttf("Verbunden mit %s:%u", networkSettings.mqttHost.c_str(),
             networkSettings.mqttPort);
  }
  return connected;
}
