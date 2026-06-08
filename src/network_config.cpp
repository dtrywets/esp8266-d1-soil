#include "network_config.h"

#if defined(ESP8266)

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

#else

#include <Preferences.h>

static Preferences networkPreferences;
static const char *kNamespace = "d1_soil_net";

#endif

static bool isPlaceholderWifiSsid(const String &ssid) {
  return ssid.isEmpty() || ssid == "your-wifi";
}

void networkConfigLoad(NetworkSettings &settings, const NetworkSettings &defaults) {
#if defined(ESP8266)
  NetworkStored stored;
  eepromStoreBegin();
  EEPROM.get(kOffset, stored);

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
#else
  if (!networkPreferences.begin(kNamespace, true)) {
    networkPreferences.end();
    networkPreferences.begin(kNamespace, false);
  }

  settings.wifiConfigured = networkPreferences.getBool("wifi_ok", false);
  settings.wifiSsid = networkPreferences.getString("wifi_ssid", "");
  settings.wifiPassword = networkPreferences.getString("wifi_pass", "");
  settings.mqttHost = networkPreferences.getString("mqtt_host", defaults.mqttHost);
  settings.mqttPort =
      static_cast<uint16_t>(networkPreferences.getUShort("mqtt_port", defaults.mqttPort));
  settings.mqttUser = networkPreferences.getString("mqtt_user", defaults.mqttUser);
  settings.mqttPassword =
      networkPreferences.getString("mqtt_pass", defaults.mqttPassword);
  networkPreferences.end();
#endif

  if (settings.wifiSsid.isEmpty() && !isPlaceholderWifiSsid(defaults.wifiSsid)) {
    settings.wifiSsid = defaults.wifiSsid;
    settings.wifiPassword = defaults.wifiPassword;
  }

  if (settings.mqttHost.isEmpty()) {
    settings.mqttHost = defaults.mqttHost;
  }
}

void networkConfigSave(const NetworkSettings &settings) {
#if defined(ESP8266)
  NetworkStored stored;
  stored.magic = kMagic;
  stored.wifiOk = true;
  strncpy(stored.wifiSsid, settings.wifiSsid.c_str(), sizeof(stored.wifiSsid) - 1);
  strncpy(stored.wifiPass, settings.wifiPassword.c_str(), sizeof(stored.wifiPass) - 1);
  strncpy(stored.mqttHost, settings.mqttHost.c_str(), sizeof(stored.mqttHost) - 1);
  stored.mqttPort = settings.mqttPort;
  strncpy(stored.mqttUser, settings.mqttUser.c_str(), sizeof(stored.mqttUser) - 1);
  strncpy(stored.mqttPass, settings.mqttPassword.c_str(), sizeof(stored.mqttPass) - 1);
  eepromStoreBegin();
  EEPROM.put(kOffset, stored);
  EEPROM.commit();
#else
  networkPreferences.begin(kNamespace, false);
  networkPreferences.putBool("wifi_ok", true);
  networkPreferences.putString("wifi_ssid", settings.wifiSsid);
  networkPreferences.putString("wifi_pass", settings.wifiPassword);
  networkPreferences.putString("mqtt_host", settings.mqttHost);
  networkPreferences.putUShort("mqtt_port", settings.mqttPort);
  networkPreferences.putString("mqtt_user", settings.mqttUser);
  networkPreferences.putString("mqtt_pass", settings.mqttPassword);
  networkPreferences.end();
#endif
}

bool networkConfigHasStoredWifi() {
#if defined(ESP8266)
  NetworkStored stored;
  eepromStoreBegin();
  EEPROM.get(kOffset, stored);
  return stored.magic == kMagic && stored.wifiOk && stored.wifiSsid[0] != '\0';
#else
  networkPreferences.begin(kNamespace, true);
  const bool configured =
      networkPreferences.getBool("wifi_ok", false) &&
      !networkPreferences.getString("wifi_ssid", "").isEmpty();
  networkPreferences.end();
  return configured;
#endif
}
