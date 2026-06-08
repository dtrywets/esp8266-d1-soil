#pragma once

#include <Arduino.h>

#if defined(ESP8266)

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

using PlatformWebServer = ESP8266WebServer;

inline void platformWifiBeginStation() { WiFi.mode(WIFI_STA); }

inline void platformWifiNoSleep() { WiFi.setSleepMode(WIFI_NONE_SLEEP); }

inline void platformSetHostname(const char *hostname) { WiFi.hostname(hostname); }

inline String platformChipModel() { return "ESP8266"; }

inline uint32_t platformChipIdSuffix() {
  return static_cast<uint32_t>(ESP.getChipId() & 0xFFFF);
}

inline bool platformWifiScanIsOpen(uint8_t enc) {
  return enc == ENC_TYPE_NONE;
}

#elif defined(ESP32)

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

using PlatformWebServer = WebServer;

inline void platformWifiBeginStation() { WiFi.mode(WIFI_STA); }

inline void platformWifiNoSleep() { WiFi.setSleep(false); }

inline void platformSetHostname(const char *hostname) { WiFi.setHostname(hostname); }

inline String platformChipModel() { return ESP.getChipModel(); }

inline uint32_t platformChipIdSuffix() {
  return static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF);
}

inline bool platformWifiScanIsOpen(uint8_t enc) {
  return enc == WIFI_AUTH_OPEN;
}

#else
#error "Nur ESP8266 und ESP32 werden unterstützt."
#endif
