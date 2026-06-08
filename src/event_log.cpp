#include "event_log.h"
#include "improv_wifi.h"

#include <cstdarg>
#include <cstdio>

static void logPrefix(const char *channel) {
  const unsigned long sec = millis() / 1000UL;
  const unsigned long ms = millis() % 1000UL;
  Serial.printf("[%lu.%03lu] [%s] ", sec, ms, channel);
}

static bool loggingSuppressed() { return improvWifiSerialQuiet(); }

static void logV(const char *channel, const char *fmt, va_list args) {
  if (loggingSuppressed()) {
    return;
  }
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  logPrefix(channel);
  Serial.println(buffer);
}

void logWeb(const char *message) {
  if (loggingSuppressed()) {
    return;
  }
  logPrefix("WEB");
  Serial.println(message);
}

void logWebf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logV("WEB", fmt, args);
  va_end(args);
}

void logMqttIn(const char *topic, const char *payload) {
  if (loggingSuppressed()) {
    return;
  }
  logPrefix("MQTT");
  Serial.printf("<- %s = %s\n", topic, payload ? payload : "");
}

void logMqttOut(const char *topic, const char *payload, bool retain) {
  if (loggingSuppressed()) {
    return;
  }
  logPrefix("MQTT");
  Serial.printf("-> %s = %s%s\n", topic, payload ? payload : "",
                retain ? " (retain)" : "");
}

void logMqtt(const char *message) {
  if (loggingSuppressed()) {
    return;
  }
  logPrefix("MQTT");
  Serial.println(message);
}

void logMqttf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logV("MQTT", fmt, args);
  va_end(args);
}

void logSys(const char *message) {
  if (loggingSuppressed()) {
    return;
  }
  logPrefix("SYS");
  Serial.println(message);
}

void logSysf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  logV("SYS", fmt, args);
  va_end(args);
}
