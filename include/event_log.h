#pragma once

#include <Arduino.h>

void logWeb(const char *message);
void logWebf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void logMqttIn(const char *topic, const char *payload);
void logMqttOut(const char *topic, const char *payload, bool retain = false);
void logMqtt(const char *message);
void logMqttf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

void logSys(const char *message);
void logSysf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
