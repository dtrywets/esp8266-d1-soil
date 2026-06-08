#if defined(ESP8266)
#include <Arduino.h>
#include <Esp.h>
ADC_MODE(ADC_TOUT_3V3);
#endif

#include "project_config.h"
#include "soil_sensor.h"

#include "event_log.h"

#include <cstring>

#if defined(ESP8266)
#include "eeprom_store.h"
#include <EEPROM.h>
#else
#include <Preferences.h>
#endif

#ifndef SENSOR_ADC_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define SENSOR_ADC_PIN 4
#else
#define SENSOR_ADC_PIN A0
#endif
#endif

#ifndef SENSOR_POWER_PIN
#define SENSOR_POWER_PIN 14
#endif

#ifndef SENSOR_POWER_ENABLED
#define SENSOR_POWER_ENABLED 1
#endif

#ifndef MEASURE_WARMUP_MS
#define MEASURE_WARMUP_MS 100
#endif

#ifndef MEASURE_INTERVAL_MS
#define MEASURE_INTERVAL_MS 30000
#endif

#if defined(ESP8266)
static constexpr uint32_t kCalMagic = 0xD150CA11UL;
static constexpr uint16_t kCalOffset = 256;

struct CalStored {
  uint32_t magic = 0;
  uint16_t dryAdc = DEFAULT_DRY_ADC;
  uint16_t wetAdc = DEFAULT_WET_ADC;
  char label[49] = DEFAULT_SENSOR_LABEL;
};
#else
static Preferences calPreferences;
static const char *kCalNamespace = "d1_soil_cal";
#endif

static SoilCalibration calibration;
static SoilReading lastReading;
static uint32_t lastMeasureMs = 0;

static float smoothHistory[3] = {0.0f, 0.0f, 0.0f};
static uint8_t smoothCount = 0;
static uint8_t smoothIndex = 0;

static bool measureRequested = false;
static bool measuring = false;
static uint32_t measureStateMs = 0;
static uint8_t sampleIndex = 0;
static uint16_t samples[5] = {0, 0, 0, 0, 0};

static float moisturePercent(uint16_t raw, uint16_t dryAdc, uint16_t wetAdc) {
  if (dryAdc <= wetAdc) {
    return 0.0f;
  }
  const float pct =
      (static_cast<float>(dryAdc - raw) / static_cast<float>(dryAdc - wetAdc)) *
      100.0f;
  return constrain(pct, 0.0f, 100.0f);
}

static uint16_t median5(const uint16_t values[5]) {
  uint16_t sorted[5];
  for (uint8_t i = 0; i < 5; ++i) {
    sorted[i] = values[i];
  }
  for (uint8_t i = 1; i < 5; ++i) {
    const uint16_t key = sorted[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && sorted[static_cast<uint8_t>(j)] > key) {
      sorted[static_cast<uint8_t>(j + 1)] = sorted[static_cast<uint8_t>(j)];
      --j;
    }
    sorted[static_cast<uint8_t>(j + 1)] = key;
  }
  return sorted[2];
}

static void sensorPowerOn() {
#if SENSOR_POWER_ENABLED
  if (SENSOR_POWER_PIN >= 0) {
    digitalWrite(SENSOR_POWER_PIN, HIGH);
  }
#endif
}

static void sensorPowerOff() {
#if SENSOR_POWER_ENABLED
  if (SENSOR_POWER_PIN >= 0) {
    digitalWrite(SENSOR_POWER_PIN, LOW);
  }
#endif
}

static void finishMeasurement(uint16_t rawAdc) {
  lastReading.rawAdc = rawAdc;
  lastReading.moisturePercent =
      moisturePercent(rawAdc, calibration.dryAdc, calibration.wetAdc);
  lastReading.measuredAtMs = millis();
  lastReading.valid = true;
  lastMeasureMs = lastReading.measuredAtMs;

  smoothHistory[smoothIndex] = lastReading.moisturePercent;
  smoothIndex = (smoothIndex + 1) % 3;
  if (smoothCount < 3) {
    ++smoothCount;
  }

  logSysf("Bodenfeuchte: ADC=%u, %.1f %%", rawAdc, lastReading.moisturePercent);
}

void soilCalLoad(SoilCalibration &cal, const SoilCalibration &defaults) {
#if defined(ESP8266)
  CalStored stored;
  eepromStoreBegin();
  EEPROM.get(kCalOffset, stored);

  if (stored.magic != kCalMagic) {
    cal = defaults;
    return;
  }

  cal.dryAdc = stored.dryAdc;
  cal.wetAdc = stored.wetAdc;
  cal.label = stored.label;
#else
  if (!calPreferences.begin(kCalNamespace, true)) {
    calPreferences.end();
    calPreferences.begin(kCalNamespace, false);
  }

  cal.dryAdc =
      static_cast<uint16_t>(calPreferences.getUShort("dry_adc", defaults.dryAdc));
  cal.wetAdc =
      static_cast<uint16_t>(calPreferences.getUShort("wet_adc", defaults.wetAdc));
  cal.label = calPreferences.getString("label", defaults.label);
  calPreferences.end();
#endif

  if (cal.dryAdc <= cal.wetAdc) {
    cal = defaults;
  }
}

void soilCalSave(const SoilCalibration &cal) {
#if defined(ESP8266)
  CalStored stored;
  stored.magic = kCalMagic;
  stored.dryAdc = cal.dryAdc;
  stored.wetAdc = cal.wetAdc;
  strncpy(stored.label, cal.label.c_str(), sizeof(stored.label) - 1);
  eepromStoreBegin();
  EEPROM.put(kCalOffset, stored);
  EEPROM.commit();
#else
  calPreferences.begin(kCalNamespace, false);
  calPreferences.putUShort("dry_adc", cal.dryAdc);
  calPreferences.putUShort("wet_adc", cal.wetAdc);
  calPreferences.putString("label", cal.label);
  calPreferences.end();
#endif
}

static uint16_t readAdcMedian() {
  for (uint8_t i = 0; i < 2; ++i) {
    analogRead(SENSOR_ADC_PIN);
    delay(5);
  }

  uint16_t localSamples[5];
  for (uint8_t i = 0; i < 5; ++i) {
    localSamples[i] = static_cast<uint16_t>(analogRead(SENSOR_ADC_PIN));
    delay(10);
  }
  return median5(localSamples);
}

void soilSensorBegin() {
#if SENSOR_POWER_ENABLED
  if (SENSOR_POWER_PIN >= 0) {
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, LOW);
  }
#elif SENSOR_POWER_PIN >= 0
  pinMode(SENSOR_POWER_PIN, INPUT);
#endif

  SoilCalibration defaults;
  soilCalLoad(calibration, defaults);
  logSysf("Kalibrierung: trocken=%u, nass=%u, Label=%s", calibration.dryAdc,
          calibration.wetAdc, calibration.label.c_str());
}

SoilReading soilSensorMeasure() {
  sensorPowerOn();
  delay(MEASURE_WARMUP_MS);
  const uint16_t raw = readAdcMedian();
  sensorPowerOff();
  finishMeasurement(raw);
  return lastReading;
}

void soilSensorRequestMeasure() { measureRequested = true; }

void soilSensorTask() {
  const uint32_t now = millis();

  if (!measuring) {
    const bool intervalElapsed =
        lastMeasureMs == 0 || (now - lastMeasureMs >= MEASURE_INTERVAL_MS);
    if (!measureRequested && !intervalElapsed) {
      return;
    }
    measureRequested = false;
    measuring = true;
    sampleIndex = 0;
    sensorPowerOn();
    measureStateMs = now;
    return;
  }

  if (sampleIndex == 0 && now - measureStateMs < MEASURE_WARMUP_MS) {
    return;
  }

  if (sampleIndex == 0) {
    for (uint8_t i = 0; i < 2; ++i) {
      analogRead(SENSOR_ADC_PIN);
    }
    measureStateMs = now;
    ++sampleIndex;
    return;
  }

  if (sampleIndex <= 5) {
    if (now - measureStateMs < 10) {
      return;
    }
    samples[sampleIndex - 1] = static_cast<uint16_t>(analogRead(SENSOR_ADC_PIN));
    ++sampleIndex;
    measureStateMs = now;
    if (sampleIndex <= 5) {
      return;
    }
  }

  sensorPowerOff();
  finishMeasurement(median5(samples));
  measuring = false;
}

const SoilReading &soilSensorLastReading() { return lastReading; }

uint32_t soilSensorLastMeasureMs() { return lastMeasureMs; }

void soilSensorSetCalibration(const SoilCalibration &cal) {
  calibration = cal;
  if (calibration.dryAdc <= calibration.wetAdc) {
    calibration.dryAdc = DEFAULT_DRY_ADC;
    calibration.wetAdc = DEFAULT_WET_ADC;
  }
  soilCalSave(calibration);
  if (lastReading.valid) {
    lastReading.moisturePercent =
        moisturePercent(lastReading.rawAdc, calibration.dryAdc, calibration.wetAdc);
  }
}

const SoilCalibration &soilSensorCalibration() { return calibration; }

float soilSensorSmoothedPercent() {
  if (!lastReading.valid) {
    return 0.0f;
  }
  if (smoothCount == 0) {
    return lastReading.moisturePercent;
  }
  float sum = 0.0f;
  for (uint8_t i = 0; i < smoothCount; ++i) {
    sum += smoothHistory[i];
  }
  return sum / static_cast<float>(smoothCount);
}
