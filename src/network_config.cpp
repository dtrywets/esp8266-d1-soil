#include "network_config.h"

#include "eeprom_store.h"

#include <EEPROM.h>
#include <cstring>

static constexpr uint32_t kMagic = 0xD1501111UL;
static constexpr uint16_t kOffset = 0;

struct NetworkStored {
  uint32_t magic = 0;
  bool wifiOk = false;
  char wifiSsid[33] = "";
  char wifiPass[65] = "";
  char mqttHost[64] = "";
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPass[65] = "";
};

static bool isPlaceholderWifiSsid(const String &ssid) {
  return ssid.isEmpty() || ssid == "your-wifi";
}

static void loadStored(NetworkStored &stored) {
  eepromStoreBegin();
  EEPROM.get(kOffset, stored);
}

static void saveStored(const NetworkStored &stored) {
  eepromStoreBegin();
  EEPROM.put(kOffset, stored);
  EEPROM.commit();
}

void networkConfigLoad(NetworkSettings &settings, const NetworkSettings &defaults) {
  NetworkStored stored;
  loadStored(stored);

  if (stored.magic != kMagic) {
    settings = defaults;
    settings.wifiConfigured = false;
    return;
  }

  settings.wifiConfigured = stored.wifiOk;
  settings.wifiSsid = stored.wifiSsid;
  settings.wifiPassword = stored.wifiPass;
  settings.mqttHost = stored.mqttHost;
  settings.mqttPort = stored.mqttPort;
  settings.mqttUser = stored.mqttUser;
  settings.mqttPassword = stored.mqttPass;

  if (settings.wifiSsid.isEmpty() && !isPlaceholderWifiSsid(defaults.wifiSsid)) {
    settings.wifiSsid = defaults.wifiSsid;
    settings.wifiPassword = defaults.wifiPassword;
  }

  if (settings.mqttHost.isEmpty()) {
    settings.mqttHost = defaults.mqttHost;
  }
}

void networkConfigSave(const NetworkSettings &settings) {
  NetworkStored stored;
  stored.magic = kMagic;
  stored.wifiOk = true;
  strncpy(stored.wifiSsid, settings.wifiSsid.c_str(), sizeof(stored.wifiSsid) - 1);
  strncpy(stored.wifiPass, settings.wifiPassword.c_str(), sizeof(stored.wifiPass) - 1);
  strncpy(stored.mqttHost, settings.mqttHost.c_str(), sizeof(stored.mqttHost) - 1);
  stored.mqttPort = settings.mqttPort;
  strncpy(stored.mqttUser, settings.mqttUser.c_str(), sizeof(stored.mqttUser) - 1);
  strncpy(stored.mqttPass, settings.mqttPassword.c_str(), sizeof(stored.mqttPass) - 1);
  saveStored(stored);
}

bool networkConfigHasStoredWifi() {
  NetworkStored stored;
  loadStored(stored);
  return stored.magic == kMagic && stored.wifiOk && stored.wifiSsid[0] != '\0';
}
