#pragma once

#include <Arduino.h>

struct NetworkSettings {
  String wifiSsid;
  String wifiPassword;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser;
  String mqttPassword;
  bool wifiConfigured = false;
};

void networkConfigLoad(NetworkSettings &settings, const NetworkSettings &defaults);
void networkConfigSave(const NetworkSettings &settings);
bool networkConfigHasStoredWifi();
