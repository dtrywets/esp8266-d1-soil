#pragma once

#include <Arduino.h>

#ifndef DEFAULT_DRY_ADC
#define DEFAULT_DRY_ADC 800
#endif

#ifndef DEFAULT_WET_ADC
#define DEFAULT_WET_ADC 300
#endif

#ifndef DEFAULT_SENSOR_LABEL
#define DEFAULT_SENSOR_LABEL "Bodenfeuchte"
#endif

struct SoilCalibration {
  uint16_t dryAdc = DEFAULT_DRY_ADC;
  uint16_t wetAdc = DEFAULT_WET_ADC;
  String label = DEFAULT_SENSOR_LABEL;
};

struct SoilReading {
  uint16_t rawAdc = 0;
  float moisturePercent = 0.0f;
  uint32_t measuredAtMs = 0;
  bool valid = false;
};

void soilCalLoad(SoilCalibration &cal, const SoilCalibration &defaults);
void soilCalSave(const SoilCalibration &cal);

void soilSensorBegin();
void soilSensorTask();
void soilSensorRequestMeasure();

SoilReading soilSensorMeasure();
const SoilReading &soilSensorLastReading();
uint32_t soilSensorLastMeasureMs();
void soilSensorSetCalibration(const SoilCalibration &cal);
const SoilCalibration &soilSensorCalibration();

float soilSensorSmoothedPercent();
