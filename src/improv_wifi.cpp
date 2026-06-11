#include "improv_wifi.h"

#include "firmware_version.h"
#include "network_config.h"
#include "platform_io.h"
#include "project_config.h"

#include <ImprovWiFiLibrary.h>

#if defined(ESP32)
#include <esp_log.h>
#endif

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "D1Soil1"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "Bodenfeuchte 1"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.10"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif

static ImprovWiFi *improvSerial = nullptr;
static bool improvStarted = false;
static uint32_t lastAnnounceMs = 0;

static ImprovWiFi &improvClient() {
  if (!improvSerial) {
    improvSerial = new ImprovWiFi(&Serial);
  }
  return *improvSerial;
}

// NVS nur einmal prüfen — nicht in jeder loop()-Iteration (sonst Serial-Flut + kein Improv).
static int8_t wifiSetupCached = -1;

static void refreshWifiSetupCache() {
  wifiSetupCached = networkConfigHasStoredWifi() ? 0 : 1;
}

static bool needsWifiSetup() {
  if (wifiSetupCached < 0) {
    refreshWifiSetupCache();
  }
  return wifiSetupCached == 1;
}

static ImprovTypes::ChipFamily improvChipFamily() {
#if defined(ESP8266)
  return ImprovTypes::ChipFamily::CF_ESP8266;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return ImprovTypes::ChipFamily::CF_ESP32_C3;
#else
  return ImprovTypes::ChipFamily::CF_ESP32;
#endif
}

bool improvWifiShouldDeferNetwork() { return needsWifiSetup(); }

bool improvWifiSerialQuiet() { return needsWifiSetup(); }

static bool improvCustomConnect(const char *ssid, const char *password) {
  WiFi.disconnect(true);
  delay(100);
  platformWifiBeginStation();
  platformWifiNoSleep();
  platformSetHostname(WIFI_HOSTNAME);
  WiFi.begin(ssid, password);

  for (uint8_t attempt = 0; attempt < 40; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    }
    delay(250);
  }
  return false;
}

static void improvOnConnected(const char *ssid, const char *password) {
  NetworkSettings settings;
  settings.wifiSsid = ssid;
  settings.wifiPassword = password;
  settings.mqttHost = MQTT_HOST;
  settings.mqttPort = MQTT_PORT;
  settings.mqttUser = MQTT_USER;
  settings.mqttPassword = MQTT_PASSWORD;
  settings.wifiConfigured = true;
  networkConfigSave(settings);
  (void)password;
  delay(400);
  ESP.restart();
}

static void improvOnError(ImprovTypes::Error err) {
  (void)err;
}

void improvWifiBegin() {
  refreshWifiSetupCache();

  if (needsWifiSetup()) {
#if defined(ESP8266)
    Serial.setDebugOutput(false);
#elif defined(ESP32)
    esp_log_level_set("*", ESP_LOG_NONE);
#endif
  }

  if (needsWifiSetup()) {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(50);
  }

  improvClient().setDeviceInfo(improvChipFamily(), IMPROV_FIRMWARE_NAME,
                               FIRMWARE_VERSION_LABEL, DEVICE_NAME,
                               "http://{LOCAL_IPV4}/");
  improvClient().setCustomConnectWiFi(improvCustomConnect);
  improvClient().onImprovConnected(improvOnConnected);
  improvClient().onImprovError(improvOnError);
  improvStarted = true;

  if (needsWifiSetup()) {
    improvClient().announceAuthorized();
    lastAnnounceMs = millis();
  }
}

void improvWifiLoop() {
  if (!improvStarted) {
    return;
  }

  improvClient().handleSerial();

  if (!needsWifiSetup()) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t announceIntervalMs = now < 15000 ? 100 : 250;
  if (now - lastAnnounceMs >= announceIntervalMs) {
    improvClient().announceAuthorized();
    lastAnnounceMs = now;
  }

  delay(1);
}
