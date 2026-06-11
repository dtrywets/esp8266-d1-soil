#include "improv_wifi.h"

#include "firmware_version.h"
#include "network_config.h"
#include "platform_io.h"
#include "project_config.h"

#include <ImprovWiFiLibrary.h>

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

static ImprovWiFi improvSerial(&Serial);
static bool improvStarted = false;
static uint32_t lastAnnounceMs = 0;

static bool needsWifiSetup() { return !networkConfigHasStoredWifi(); }

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
#if defined(ESP8266)
  if (needsWifiSetup()) {
    Serial.setDebugOutput(false);
  }
#endif

  improvSerial.setDeviceInfo(improvChipFamily(), IMPROV_FIRMWARE_NAME,
                             FIRMWARE_VERSION_LABEL, DEVICE_NAME,
                             "http://{LOCAL_IPV4}/");
  improvSerial.setCustomConnectWiFi(improvCustomConnect);
  improvSerial.onImprovConnected(improvOnConnected);
  improvSerial.onImprovError(improvOnError);
  improvStarted = true;

  if (needsWifiSetup()) {
    improvSerial.announceAuthorized();
    lastAnnounceMs = millis();
  }
}

void improvWifiLoop() {
  if (!improvStarted) {
    return;
  }

  improvSerial.handleSerial();

  if (!needsWifiSetup()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastAnnounceMs >= 500) {
    improvSerial.announceAuthorized();
    lastAnnounceMs = now;
  }
}
