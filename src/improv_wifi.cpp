#include "improv_wifi.h"

#include "event_log.h"
#include "firmware_version.h"
#include "network_config.h"
#include "platform_io.h"
#include "project_config.h"
#include "web_portal.h"

#include <ImprovWiFiLibrary.h>

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "D1Soil1"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "Bodenfeuchte 1"
#endif

#ifndef IMPROV_PROVISION_WINDOW_MS
#define IMPROV_PROVISION_WINDOW_MS 120000
#endif

static ImprovWiFi improvSerial(&Serial);
static bool improvStarted = false;
static bool improvNeedProvision = false;
static uint32_t improvBootMs = 0;
static uint32_t lastAnnounceMs = 0;

static ImprovTypes::ChipFamily improvChipFamily() {
#if defined(ESP8266)
  return ImprovTypes::ChipFamily::CF_ESP8266;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return ImprovTypes::ChipFamily::CF_ESP32_C3;
#else
  return ImprovTypes::ChipFamily::CF_ESP32;
#endif
}

bool improvWifiShouldDeferNetwork() {
  if (!improvNeedProvision) {
    return false;
  }
  return (millis() - improvBootMs) < IMPROV_PROVISION_WINDOW_MS;
}

bool improvWifiSerialQuiet() {
  return improvWifiShouldDeferNetwork();
}

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
  NetworkSettings settings = webPortalNetworkSettings();
  settings.wifiSsid = ssid;
  settings.wifiPassword = password;
  settings.wifiConfigured = true;
  networkConfigSave(settings);
  improvNeedProvision = false;
  logSysf("Improv: WLAN \"%s\" gespeichert, Neustart …", ssid);
  (void)password;
  delay(400);
  ESP.restart();
}

static void improvOnError(ImprovTypes::Error err) {
  logSysf("Improv-Fehler: %u", static_cast<unsigned>(err));
}

void improvWifiBegin() {
  improvBootMs = millis();
  improvNeedProvision = !networkConfigHasStoredWifi();

  improvSerial.setDeviceInfo(improvChipFamily(), "Bodenfeuchte Soil Sensor",
                             FIRMWARE_VERSION, DEVICE_NAME,
                             "http://{LOCAL_IPV4}/");
  improvSerial.setCustomConnectWiFi(improvCustomConnect);
  improvSerial.onImprovConnected(improvOnConnected);
  improvSerial.onImprovError(improvOnError);
  improvStarted = true;

  if (improvNeedProvision) {
    improvSerial.announceAuthorized();
    lastAnnounceMs = improvBootMs;
  }
}

void improvWifiLoop() {
  if (!improvStarted) {
    return;
  }

  improvSerial.handleSerial();

  if (!improvWifiShouldDeferNetwork()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastAnnounceMs >= 2000) {
    improvSerial.announceAuthorized();
    lastAnnounceMs = now;
  }
}
