#include "ota_update.h"

#include "event_log.h"

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "D1Soil1"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

#ifndef ARDUINO_OTA_PORT
#define ARDUINO_OTA_PORT 8266
#endif

static bool otaUdpStarted = false;

static bool otaPasswordEnabled() { return strlen(OTA_PASSWORD) > 0; }

static void configureArduinoOtaCallbacks() {
  ArduinoOTA.onStart([]() { logSys("OTA-Update gestartet"); });
  ArduinoOTA.onEnd([]() { logSys("OTA-Update abgeschlossen, Neustart …"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    logSysf("OTA-Fortschritt: %u%%", (progress * 100U) / total);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    logSysf("OTA-Fehler [%u]", static_cast<unsigned>(error));
  });
}

void otaUpdateOnWifiConnected() {
  if (otaUdpStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  ArduinoOTA.setPort(ARDUINO_OTA_PORT);
  ArduinoOTA.setHostname(WIFI_HOSTNAME);
  configureArduinoOtaCallbacks();

  if (otaPasswordEnabled()) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
    logSysf("Arduino-OTA: UDP-Port %u (mit Passwort)",
            static_cast<unsigned>(ARDUINO_OTA_PORT));
  } else {
    logSysf("Arduino-OTA: UDP-Port %u (ohne Passwort — nur im LAN)",
            static_cast<unsigned>(ARDUINO_OTA_PORT));
  }

  ArduinoOTA.begin();
  otaUdpStarted = true;

  MDNS.addService("arduino", "tcp", ARDUINO_OTA_PORT);
  logSysf("Arduino-OTA aktiv: %s:%u/udp (pio run -e d1_mini_ota -t upload)",
          WiFi.localIP().toString().c_str(),
          static_cast<unsigned>(ARDUINO_OTA_PORT));
}

void otaUpdateLoop() {
  if (!otaUdpStarted) {
    return;
  }
  ArduinoOTA.handle();
}

bool otaUpdateIsActive() { return otaUdpStarted; }

uint16_t otaUpdatePort() { return ARDUINO_OTA_PORT; }
