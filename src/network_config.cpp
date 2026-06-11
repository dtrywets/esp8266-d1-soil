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

static const char *kNamespace = "d1_soil_net";

static bool openNetworkPrefs(Preferences &prefs) {
  // RW-Modus: legt Namespace an, wenn nach Flash-Löschen noch keiner existiert.
  return prefs.begin(kNamespace, false);
}

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
  Preferences prefs;
  if (!openNetworkPrefs(prefs)) {
    settings = defaults;
    settings.wifiConfigured = false;
    return;
  }

  settings.wifiConfigured = prefs.getBool("wifi_ok", false);
  settings.wifiSsid =
      prefs.isKey("wifi_ssid") ? prefs.getString("wifi_ssid", "") : "";
  settings.wifiPassword =
      prefs.isKey("wifi_pass") ? prefs.getString("wifi_pass", "") : "";
  settings.mqttHost =
      prefs.isKey("mqtt_host") ? prefs.getString("mqtt_host", defaults.mqttHost)
                               : defaults.mqttHost;
  settings.mqttPort = prefs.isKey("mqtt_port")
                          ? static_cast<uint16_t>(prefs.getUShort(
                                "mqtt_port", defaults.mqttPort))
                          : defaults.mqttPort;
  settings.mqttUser =
      prefs.isKey("mqtt_user") ? prefs.getString("mqtt_user", defaults.mqttUser)
                               : defaults.mqttUser;
  settings.mqttPassword =
      prefs.isKey("mqtt_pass")
          ? prefs.getString("mqtt_pass", defaults.mqttPassword)
          : defaults.mqttPassword;
  prefs.end();
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
  Preferences prefs;
  if (!openNetworkPrefs(prefs)) {
    return;
  }
  prefs.putBool("wifi_ok", true);
  prefs.putString("wifi_ssid", settings.wifiSsid);
  prefs.putString("wifi_pass", settings.wifiPassword);
  prefs.putString("mqtt_host", settings.mqttHost);
  prefs.putUShort("mqtt_port", settings.mqttPort);
  prefs.putString("mqtt_user", settings.mqttUser);
  prefs.putString("mqtt_pass", settings.mqttPassword);
  prefs.end();
#endif
}

bool networkConfigHasStoredWifi() {
#if defined(ESP8266)
  NetworkStored stored;
  eepromStoreBegin();
  EEPROM.get(kOffset, stored);
  return stored.magic == kMagic && stored.wifiOk && stored.wifiSsid[0] != '\0';
#else
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) {
    return false;
  }
  const String ssid = prefs.isKey("wifi_ssid") ? prefs.getString("wifi_ssid", "") : "";
  const bool configured =
      prefs.getBool("wifi_ok", false) && !ssid.isEmpty() && !isPlaceholderWifiSsid(ssid);
  if (prefs.getBool("wifi_ok", false) && !configured) {
    prefs.putBool("wifi_ok", false);
  }
  prefs.end();
  return configured;
#endif
}
