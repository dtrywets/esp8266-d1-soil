#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "project_config.h"
#include "platform_io.h"

#if defined(ESP8266)
#include "eeprom_store.h"
#elif defined(ESP32)
#include <nvs_flash.h>
#endif
#include "event_log.h"
#include "firmware_version.h"
#include "network_config.h"
#include "improv_wifi.h"
#include "ota_update.h"
#include "soil_sensor.h"
#include "web_portal.h"

#ifndef WIFI_SSID
#define WIFI_SSID "your-wifi"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "your-password"
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

#ifndef DEVICE_ID
#define DEVICE_ID "d1_soil_1"
#endif

#ifndef DEVICE_NAME
#define DEVICE_NAME "Bodenfeuchte 1"
#endif

#ifndef MQTT_BASE_TOPIC
#define MQTT_BASE_TOPIC "d1_soil/1"
#endif

#ifndef MQTT_DISCOVERY_PREFIX
#define MQTT_DISCOVERY_PREFIX "homeassistant"
#endif

static WiFiClient *wifiClientPtr = nullptr;
static PubSubClient *mqttPtr = nullptr;

static PubSubClient &mqttClient() {
  if (!mqttPtr) {
    wifiClientPtr = new WiFiClient();
    mqttPtr = new PubSubClient(*wifiClientPtr);
  }
  return *mqttPtr;
}

static bool mqttSessionReady = false;
static bool lastReadingPublished = false;

static String globalTopic(const char *suffix) {
  String topic = MQTT_BASE_TOPIC;
  topic += "/";
  topic += suffix;
  return topic;
}

static bool mqttPublish(const String &topic, const char *payload, bool retain = false) {
  logMqttOut(topic.c_str(), payload, retain);
  return mqttClient().publish(topic.c_str(), payload, retain);
}

static bool mqttPublish(const String &topic, const String &payload, bool retain = false) {
  return mqttPublish(topic, payload.c_str(), retain);
}

static void addDeviceBlock(JsonDocument &doc) {
  JsonObject device = doc["device"].to<JsonObject>();
  JsonArray ids = device["identifiers"].to<JsonArray>();
  ids.add(DEVICE_ID);
  device["name"] = DEVICE_NAME;
  device["manufacturer"] = "DIY";
  device["model"] = "ESP8266 D1 Soil Sensor";
}

static void publishHomeAssistantDiscovery() {
  if (!mqttClient().connected()) {
    return;
  }

  logMqtt("Veröffentliche Home-Assistant-Discovery");

  const SoilCalibration cal = soilSensorCalibration();
  const String label = cal.label;
  const String statusTopic = globalTopic("status");
  const String base = MQTT_BASE_TOPIC;

  {
    JsonDocument doc;
    doc["name"] = label;
    doc["unique_id"] = String(DEVICE_ID) + "_moisture";
    doc["state_topic"] = base + "/moisture";
    doc["unit_of_measurement"] = "%";
    doc["device_class"] = "moisture";
    doc["state_class"] = "measurement";
    doc["icon"] = "mdi:water-percent";
    doc["availability_topic"] = statusTopic;
    addDeviceBlock(doc);
    char payload[640];
    serializeJson(doc, payload);
    String topic = String(MQTT_DISCOVERY_PREFIX) + "/sensor/" + DEVICE_ID +
                   "_moisture/config";
    mqttPublish(topic, payload, true);
  }

  {
    JsonDocument doc;
    doc["name"] = label + " Roh-ADC";
    doc["unique_id"] = String(DEVICE_ID) + "_moisture_raw";
    doc["state_topic"] = base + "/moisture_raw";
    doc["entity_category"] = "diagnostic";
    doc["icon"] = "mdi:chip";
    doc["availability_topic"] = statusTopic;
    addDeviceBlock(doc);
    char payload[640];
    serializeJson(doc, payload);
    String topic = String(MQTT_DISCOVERY_PREFIX) + "/sensor/" + DEVICE_ID +
                   "_moisture_raw/config";
    mqttPublish(topic, payload, true);
  }

  struct CalEntity {
    const char *suffix;
    const char *nameSuffix;
    const char *topicKey;
  };
  const CalEntity calEntities[] = {
      {"cal_dry", "Kalibrierung trocken", "calibration/dry"},
      {"cal_wet", "Kalibrierung nass", "calibration/wet"},
  };

  for (const CalEntity &entity : calEntities) {
    JsonDocument doc;
    doc["name"] = label + " " + entity.nameSuffix;
    doc["unique_id"] = String(DEVICE_ID) + "_" + entity.suffix;
    doc["command_topic"] = base + "/" + entity.topicKey + "/set";
    doc["state_topic"] = base + "/" + entity.topicKey;
    doc["min"] = 0;
    doc["max"] = ADC_MAX_VALUE;
    doc["step"] = 1;
    doc["mode"] = "box";
    doc["availability_topic"] = statusTopic;
    addDeviceBlock(doc);
    char payload[640];
    serializeJson(doc, payload);
    String topic = String(MQTT_DISCOVERY_PREFIX) + "/number/" + DEVICE_ID + "_" +
                   entity.suffix + "/config";
    mqttPublish(topic, payload, true);
  }
}

static void publishSoilValues() {
  if (!mqttClient().connected()) {
    return;
  }

  const SoilReading reading = soilSensorLastReading();
  const SoilCalibration cal = soilSensorCalibration();

  if (reading.valid) {
    char moistureBuf[16];
    snprintf(moistureBuf, sizeof(moistureBuf), "%.1f", soilSensorSmoothedPercent());
    mqttPublish(globalTopic("moisture"), moistureBuf, true);
    mqttPublish(globalTopic("moisture_raw"), String(reading.rawAdc), false);
    lastReadingPublished = true;
  }

  mqttPublish(globalTopic("calibration/dry"), String(cal.dryAdc), true);
  mqttPublish(globalTopic("calibration/wet"), String(cal.wetAdc), true);
}

static void mqttCallback(char *topic, uint8_t *payload, unsigned int length) {
  String topicString(topic);
  String payloadString;
  payloadString.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    payloadString += static_cast<char>(payload[i]);
  }
  payloadString.trim();
  logMqttIn(topicString.c_str(), payloadString.c_str());

  if (topicString == globalTopic("measure/cmd")) {
    String cmd = payloadString;
    cmd.toUpperCase();
    if (cmd == "MEASURE") {
      soilSensorRequestMeasure();
    }
    return;
  }

  if (topicString == globalTopic("calibration/dry/set")) {
    SoilCalibration cal = soilSensorCalibration();
    cal.dryAdc = static_cast<uint16_t>(payloadString.toInt());
    if (cal.dryAdc > cal.wetAdc) {
      soilSensorSetCalibration(cal);
      webPortalRequestMqttRepublish();
    }
    return;
  }

  if (topicString == globalTopic("calibration/wet/set")) {
    SoilCalibration cal = soilSensorCalibration();
    cal.wetAdc = static_cast<uint16_t>(payloadString.toInt());
    if (cal.dryAdc > cal.wetAdc) {
      soilSensorSetCalibration(cal);
      webPortalRequestMqttRepublish();
    }
    return;
  }

  logMqttf("Kein Handler für Topic: %s", topicString.c_str());
}

static void subscribeMqttTopics() {
  const char *subs[] = {"measure/cmd", "calibration/dry/set",
                        "calibration/wet/set"};
  for (const char *sub : subs) {
    const String topic = globalTopic(sub);
    mqttClient().subscribe(topic.c_str());
    logMqttf("Subscribe: %s", topic.c_str());
  }
}

static void connectMqtt() {
  if (!webPortalIsWifiConnected()) {
    mqttSessionReady = false;
    webPortalSetMqttConnected(false);
    return;
  }

  mqttClient().setCallback(mqttCallback);
  mqttClient().setBufferSize(2048);

  if (mqttClient().connected()) {
    webPortalSetMqttConnected(true);
    if (!mqttSessionReady) {
      mqttPublish(globalTopic("status"), "online", true);
      subscribeMqttTopics();
      publishHomeAssistantDiscovery();
      publishSoilValues();
      logMqtt("MQTT-Session bereit");
      mqttSessionReady = true;
    }
    return;
  }

  mqttSessionReady = false;
  if (!webPortalConnectMqtt(mqttClient(), DEVICE_ID, globalTopic("status").c_str())) {
    return;
  }

  mqttPublish(globalTopic("status"), "online", true);
  subscribeMqttTopics();
  publishHomeAssistantDiscovery();
  publishSoilValues();
  logMqtt("MQTT verbunden und initialisiert");
  mqttSessionReady = true;
}

static void mqttLoop() {
  connectMqtt();

  if (webPortalConsumeMqttRepublishRequest()) {
    if (mqttClient().connected()) {
      publishHomeAssistantDiscovery();
      publishSoilValues();
    }
  }

  if (mqttClient().connected()) {
    mqttClient().loop();
  }

  const SoilReading reading = soilSensorLastReading();
  if (reading.valid && !lastReadingPublished && mqttClient().connected()) {
    publishSoilValues();
  }
}

static void startNormalOperation() {
  soilSensorBegin();

  NetworkSettings defaults;
  defaults.wifiSsid = WIFI_SSID;
  defaults.wifiPassword = WIFI_PASSWORD;
  defaults.mqttHost = MQTT_HOST;
  defaults.mqttPort = MQTT_PORT;
  defaults.mqttUser = MQTT_USER;
  defaults.mqttPassword = MQTT_PASSWORD;
  webPortalBegin(defaults);

  Serial.println();
  Serial.println("Bodenfeuchte-Sensor startet");
  logSysf("Firmware %s", FIRMWARE_VERSION_LABEL);
}

#if defined(ESP32)
static void platformEnsureNvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
}
#endif

void setup() {
  Serial.begin(115200);
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  {
    const uint32_t usbWaitUntil = millis() + 3000;
    while (!Serial && millis() < usbWaitUntil) {
      delay(10);
    }
  }
#else
  delay(50);
#endif

#if defined(ESP8266)
  eepromStoreBegin();
#elif defined(ESP32)
  platformEnsureNvs();
#endif

  improvWifiBegin();

  if (!improvWifiSerialQuiet()) {
    startNormalOperation();
  }
}

void loop() {
  improvWifiLoop();

  if (improvWifiSerialQuiet()) {
    return;
  }

  webPortalLoop();
  otaUpdateLoop();
  soilSensorTask();
  mqttLoop();
}
