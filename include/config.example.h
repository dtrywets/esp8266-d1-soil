#pragma once

// Nach include/config.h kopieren. WLAN/MQTT alternativ über Web-Dashboard.

#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"

#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_PASSWORD ""

#define DEVICE_ID "d1_soil_1"
#define DEVICE_NAME "Bodenfeuchte 1"
#define MQTT_BASE_TOPIC "d1_soil/1"
#define WIFI_HOSTNAME "D1Soil1"
#define MQTT_DISCOVERY_PREFIX "homeassistant"

#define OTA_PASSWORD ""

#define SENSOR_ADC_PIN A0
#define SENSOR_POWER_PIN 14
#define SENSOR_POWER_ENABLED 1

#define MEASURE_INTERVAL_MS 30000
#define MEASURE_WARMUP_MS 100

#define DEFAULT_DRY_ADC 800
#define DEFAULT_WET_ADC 300

#define DEFAULT_SENSOR_LABEL "Bodenfeuchte"
