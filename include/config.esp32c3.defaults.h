#pragma once

#define CONFIG_BOARD_ESP32C3 1

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"

#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""

#define DEVICE_ID "c3_soil_1"
#define DEVICE_NAME "Bodenfeuchte C3"
#define MQTT_BASE_TOPIC "c3_soil/1"
#define WIFI_HOSTNAME "C3Soil1"
#define MQTT_DISCOVERY_PREFIX "homeassistant"

#define OTA_PASSWORD ""

#define SENSOR_ADC_PIN 4
#define SENSOR_POWER_PIN 5
#define SENSOR_POWER_ENABLED 0

#define MEASURE_INTERVAL_MS 30000
#define MEASURE_WARMUP_MS 200

#define DEFAULT_SENSOR_LABEL "Bodenfeuchte"

#define AP_SSID_PREFIX "C3Soil-"
